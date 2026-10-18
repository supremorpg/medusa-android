#include "../main.h"
#include "game.h"
#include "Camera.h"
#include "../net/netgame.h"
#include "../gui/gui.h"
#include "../vendor/armhook/patch.h"
#include "GTASAEngineApi.h"
#include "GTASAEngineNativeHookBindings.h"
#include "GTASAEnginePedInputBindings.h"
#include "SampInputApi.h"
#include "World.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

extern UI* pUI;
extern CGame* pGame;
extern CNetGame* pNetGame;

extern uint8_t byteCurPlayer;
extern uintptr_t dwCurPlayerActor;

PAD_KEYS LocalPlayerKeys;
PAD_KEYS RemotePlayerKeys[PLAYER_PED_SLOTS];

static uint32_t g_vehicleSelfTestUntilTick = 0;
static int16_t g_vehicleSelfTestSteering = 0;
static uint16_t g_vehicleSelfTestThrottle = 0;
static uint16_t g_vehicleSelfTestBrake = 0;
static uint32_t g_vehicleSelfTestLastLogTick = 0;
static CVehicleGTA* g_vehicleSelfTestTelemetryVehicle = nullptr;
static uint32_t g_vehicleSelfTestTelemetryModel = 0;
static CVector g_vehicleSelfTestStartPos;
static CVector g_vehicleSelfTestLastPos;
static float g_vehicleSelfTestMaxSpeed = 0.0f;
static float g_vehicleSelfTestMaxDistance2D = 0.0f;
static float g_vehicleSelfTestMinZ = 0.0f;
static float g_vehicleSelfTestMaxZ = 0.0f;
static uint32_t g_vehicleSelfTestSamples = 0;
static uint32_t g_vehicleSelfTestAccelHookCount = 0;
static uint32_t g_vehicleSelfTestBrakeHookCount = 0;
static uint32_t g_vehicleSelfTestSteerHookCount = 0;
static uint32_t g_vehicleSelfTestHandBrakeHookCount = 0;
static uint32_t g_vehicleSelfTestCarGunLRHookCount = 0;
static uint32_t g_vehicleSelfTestCarGunUDHookCount = 0;
static uint32_t g_vehicleSelfTestCarGunFireHookCount = 0;
static uint32_t g_vehicleSelfTestProcessCount = 0;

static constexpr int16_t VEHICLE_SELF_TEST_AIRCRAFT_FORWARD_PITCH = -90;
static constexpr int16_t VEHICLE_SELF_TEST_AIRCRAFT_BACK_PITCH = 90;
static constexpr uint16_t PAD_AXIS_POSITIVE = 0x0080;
static constexpr uint16_t PAD_AXIS_NEGATIVE = 0xFF80;

namespace InputApi = Xyron::Input;
using SourceButton = Xyron::Input::SourceButton;

static bool IsVehicleSelfTestInputActive();

static float VehicleSelfTestLength2D(const CVector& vec)
{
	return std::sqrt(vec.x * vec.x + vec.y * vec.y);
}

static void CopyVehicleSelfTestModelName(char (&dst)[22], CBaseModelInfo* modelInfo)
{
	dst[0] = '\0';
	if (!modelInfo) {
		return;
	}

	std::memcpy(dst, modelInfo->m_modelName, 21);
	dst[21] = '\0';
	for (char& ch : dst) {
		if (ch == '\0') {
			break;
		}
		const auto byte = static_cast<unsigned char>(ch);
		if (byte < 32 || byte > 126) {
			ch = '?';
		}
	}
}

static void DescribeVehicleSelfTestCollisionEntities(CVehicleGTA* pGtaVehicle, char (&out)[256])
{
	out[0] = '\0';
	if (!pGtaVehicle) {
		return;
	}

	const CVector vehiclePos = pGtaVehicle->GetPosition();
	size_t used = 0;
	for (size_t i = 0; i < 6; ++i) {
		CEntityGTA* entity = pGtaVehicle->m_apCollidedEntities[i];
		if (!entity) {
			continue;
		}

		char modelName[22];
		CopyVehicleSelfTestModelName(modelName, entity->GetModelInfo());
		const CVector entityPos = entity->GetPosition();
		const float distance = VehicleSelfTestLength2D(entityPos - vehiclePos);
		const int written = std::snprintf(
			out + used,
			sizeof(out) - used,
			"%s%zu:%u/%s/t%u/col%u/flags0x%08x/d%.1f/z%.2f",
			used ? "|" : "",
			i,
			static_cast<uint32_t>(entity->m_nModelIndex),
			modelName[0] ? modelName : "?",
			static_cast<uint32_t>(entity->GetType()),
			entity->m_bUsesCollision ? 1 : 0,
			entity->m_nFlags,
			distance,
			entityPos.z);
		if (written < 0) {
			break;
		}
		const auto appended = static_cast<size_t>(written);
		if (appended >= sizeof(out) - used) {
			out[sizeof(out) - 1] = '\0';
			break;
		}
		used += appended;
	}
}

static uint8_t RemoveDriverFromVehicleCollisionRecords(CVehicleGTA* pGtaVehicle, CPedGTA* pDriver)
{
	if (!pGtaVehicle || !pDriver) {
		return 0;
	}

	CEntityGTA* kept[6] = {};
	uint8_t keptCount = 0;
	uint8_t removed = 0;
	for (CEntityGTA* entity : pGtaVehicle->m_apCollidedEntities) {
		if (!entity) {
			continue;
		}
		if (entity == pDriver) {
			++removed;
			continue;
		}
		if (keptCount < 6) {
			kept[keptCount++] = entity;
		}
	}

	if (removed) {
		for (size_t i = 0; i < 6; ++i) {
			pGtaVehicle->m_apCollidedEntities[i] = i < keptCount ? kept[i] : nullptr;
		}
		pGtaVehicle->m_nNumEntitiesCollided = keptCount;
	}
	return removed;
}

static bool ShouldSuppressSeatedPedCollision(CVehicleGTA* pGtaVehicle)
{
	return pGtaVehicle != nullptr;
}

static void SuppressLocalDriverVehicleCollision(CVehicleGTA* pGtaVehicle)
{
	CPedGTA* localPed = GamePool_FindPlayerPed();
	CPedGTA* pDriver = pGtaVehicle ? pGtaVehicle->pDriver : nullptr;
	if (!pGtaVehicle || !pDriver || pDriver != localPed || !pDriver->IsInVehicle() || pDriver->pVehicle != pGtaVehicle) {
		return;
	}

	const bool oldPedUsesCollision = pDriver->m_bUsesCollision;
	const bool oldPedCollidable = pDriver->physicalFlags.bCollidable;
	const bool oldPedCanBeCollided = pDriver->physicalFlags.bCanBeCollidedWith;

	pGtaVehicle->m_pEntityIgnoredCollision = pDriver;
	pDriver->m_pEntityIgnoredCollision = pGtaVehicle;
	pDriver->bCollidedWithMyVehicle = false;
	pDriver->bPushedAlongByCar = false;
	pDriver->bKnockedUpIntoAir = false;
	pDriver->bStuckUnderCar = false;

	if (ShouldSuppressSeatedPedCollision(pGtaVehicle)) {
		pDriver->m_bUsesCollision = false;
		pDriver->physicalFlags.bCollidable = false;
		pDriver->physicalFlags.bCanBeCollidedWith = false;
	}

	const uint8_t removed = RemoveDriverFromVehicleCollisionRecords(pGtaVehicle, pDriver);
	static uint32_t s_lastPedCollisionFixLog = 0;
	const uint32_t now = GetTickCount();
	if (removed || oldPedUsesCollision != pDriver->m_bUsesCollision ||
		oldPedCollidable != pDriver->physicalFlags.bCollidable ||
		oldPedCanBeCollided != pDriver->physicalFlags.bCanBeCollidedWith) {
		if (now - s_lastPedCollisionFixLog > 5000) {
			s_lastPedCollisionFixLog = now;
			FLog("[VEH_PEDCOL64] suppress model=%u removed=%u pedCol=%u/%u/%u vehIgnored=%u pedIgnored=%u",
				 pGtaVehicle->GetModelId(),
				 removed,
				 pDriver->m_bUsesCollision ? 1 : 0,
				 pDriver->physicalFlags.bCollidable ? 1 : 0,
				 pDriver->physicalFlags.bCanBeCollidedWith ? 1 : 0,
				 pGtaVehicle->m_pEntityIgnoredCollision == pDriver ? 1 : 0,
				 pDriver->m_pEntityIgnoredCollision == pGtaVehicle ? 1 : 0);
		}
	}
}

static void RestoreLocalPedCollisionWhenOnFoot(CPedGTA* pPed)
{
	if (!pPed || pPed != GamePool_FindPlayerPed() || pPed->IsInVehicle()) {
		return;
	}

	const bool changed = !pPed->m_bUsesCollision ||
		!pPed->physicalFlags.bCollidable ||
		!pPed->physicalFlags.bCanBeCollidedWith ||
		pPed->m_pEntityIgnoredCollision != nullptr;

	pPed->m_bUsesCollision = true;
	pPed->physicalFlags.bCollidable = true;
	pPed->physicalFlags.bCanBeCollidedWith = true;
	pPed->m_pEntityIgnoredCollision = nullptr;

	static uint32_t s_lastPedCollisionRestoreLog = 0;
	const uint32_t now = GetTickCount();
	if (changed && now - s_lastPedCollisionRestoreLog > 1000) {
		s_lastPedCollisionRestoreLog = now;
		FLog("[VEH_PEDCOL64] restore-onfoot ped=%p", pPed);
	}
}

static void LogVehicleSelfTestCollisionEntityDetails(CVehicleGTA* pGtaVehicle)
{
	if (!pGtaVehicle) {
		return;
	}

	const CVector vehiclePos = pGtaVehicle->GetPosition();
	for (size_t i = 0; i < 6; ++i) {
		CEntityGTA* entity = pGtaVehicle->m_apCollidedEntities[i];
		if (!entity) {
			continue;
		}

		char modelName[22];
		CopyVehicleSelfTestModelName(modelName, entity->GetModelInfo());
		const CVector entityPos = entity->GetPosition();
		FLog("[VEH_COLL64] idx=%zu model=%u name=%s type=%u useCol=%u dist=%.1f z=%.2f driver=%u ignored=%u",
			 i,
			 static_cast<uint32_t>(entity->m_nModelIndex),
			 modelName[0] ? modelName : "?",
			 static_cast<uint32_t>(entity->GetType()),
			 entity->m_bUsesCollision ? 1 : 0,
			 VehicleSelfTestLength2D(entityPos - vehiclePos),
			 entityPos.z,
			 entity == pGtaVehicle->pDriver ? 1 : 0,
			 entity == pGtaVehicle->m_pEntityIgnoredCollision ? 1 : 0);
	}
}

static void PrepareVehicleForLocalDriver(CVehicleGTA* pGtaVehicle)
{
	if (!pGtaVehicle) {
		return;
	}

	const uint32_t oldPhysFlags = pGtaVehicle->m_nPhysicalFlags;
	const uint32_t oldVehicleUpperFlags = pGtaVehicle->m_nVehicleUpperFlags;
	const uint32_t oldVehicleLowerFlags = pGtaVehicle->m_nVehicleLowerFlags;
	pGtaVehicle->SetStatus(STATUS_PLAYER);
	pGtaVehicle->m_bUsesCollision = true;
	pGtaVehicle->physicalFlags.bCollidable = true;
	pGtaVehicle->physicalFlags.bCanBeCollidedWith = true;
	pGtaVehicle->physicalFlags.bApplyGravity = true;
	pGtaVehicle->physicalFlags.bDisableCollisionForce = false;
	pGtaVehicle->physicalFlags.bDisableMoveForce = false;
	pGtaVehicle->physicalFlags.bDontApplySpeed = false;
	pGtaVehicle->m_nVehicleFlags.bEngineOn = 1;
	pGtaVehicle->m_nVehicleFlags.bEngineBroken = 0;
	pGtaVehicle->m_nVehicleFlags.bComedyControls = 0;
	pGtaVehicle->m_nVehicleFlags.bIsBeingCarJacked = 0;
	pGtaVehicle->m_nVehicleFlags.bIsDrowning = 0;
	pGtaVehicle->m_nVehicleFlags.bParking = 0;
	pGtaVehicle->m_nVehicleFlags.bDriverLastFrame = 1;
	if (!InputApi::IsCompatibilityButtonDown(SourceButton::Handbrake)) {
		pGtaVehicle->m_nVehicleFlags.bIsHandbrakeOn = 0;
	}
	SuppressLocalDriverVehicleCollision(pGtaVehicle);

	if (oldPhysFlags != pGtaVehicle->m_nPhysicalFlags ||
		oldVehicleUpperFlags != pGtaVehicle->m_nVehicleUpperFlags ||
		oldVehicleLowerFlags != pGtaVehicle->m_nVehicleLowerFlags) {
		static uint32_t s_lastVehicleControlFixLog = 0;
		const uint32_t now = GetTickCount();
		if (now - s_lastVehicleControlFixLog > 2000) {
			s_lastVehicleControlFixLog = now;
			FLog("[VEH_CONTROL64] normalize model=%u phys=0x%08x->0x%08x upper=0x%08x->0x%08x lower=0x%08x->0x%08x hand=%u",
				 pGtaVehicle->GetModelId(),
				 oldPhysFlags,
				 pGtaVehicle->m_nPhysicalFlags,
				 oldVehicleUpperFlags,
				 pGtaVehicle->m_nVehicleUpperFlags,
				 oldVehicleLowerFlags,
				 pGtaVehicle->m_nVehicleLowerFlags,
				 pGtaVehicle->m_nVehicleFlags.bIsHandbrakeOn);
		}
	}
}

static void PrepareVehicleForSelfTest(CVehicleGTA* pGtaVehicle)
{
	PrepareVehicleForLocalDriver(pGtaVehicle);
	if (!pGtaVehicle) {
		return;
	}

	pGtaVehicle->m_nVehicleFlags.bEngineOn = 1;
	pGtaVehicle->m_nVehicleFlags.bEngineBroken = 0;
	pGtaVehicle->m_nVehicleFlags.bIsHandbrakeOn = 0;
}

static void ClearVehicleSelfTestTransientKeys()
{
	static constexpr SourceButton kTransientKeys[] = {
		SourceButton::Action,
		SourceButton::Crouch,
		SourceButton::Fire,
		SourceButton::SecondaryAttack,
		SourceButton::LookRight,
		SourceButton::Handbrake,
		SourceButton::LookLeft,
		SourceButton::Submission,
		SourceButton::Walk,
		SourceButton::AnalogUp,
		SourceButton::AnalogDown,
		SourceButton::AnalogLeft,
		SourceButton::AnalogRight,
	};

	InputApi::ClearCompatibilityButtons(kTransientKeys, sizeof(kTransientKeys) / sizeof(kTransientKeys[0]));
}

static void ApplyVehicleSelfTestPedalState(CVehicleGTA* pGtaVehicle)
{
	if (!pGtaVehicle) {
		return;
	}

	pGtaVehicle->m_nVehicleFlags.bIsHandbrakeOn = 0;
	if (g_vehicleSelfTestThrottle != 0 && g_vehicleSelfTestBrake == 0) {
		pGtaVehicle->m_fGasPedal = 1.0f;
		pGtaVehicle->m_fBreakPedal = 0.0f;
	} else if (g_vehicleSelfTestBrake != 0) {
		pGtaVehicle->m_fGasPedal = 0.0f;
		pGtaVehicle->m_fBreakPedal = 1.0f;
	} else {
		pGtaVehicle->m_fGasPedal = 0.0f;
		pGtaVehicle->m_fBreakPedal = 0.0f;
	}
}

static bool IsAircraftVehicle(CVehicleGTA* pGtaVehicle)
{
	const bool handlingAircraft = pGtaVehicle &&
		pGtaVehicle->m_pHandlingData &&
		(pGtaVehicle->IsRealHeli() || pGtaVehicle->IsRealPlane());
	return pGtaVehicle &&
		(pGtaVehicle->IsHeli() ||
		 pGtaVehicle->IsPlane() ||
		 pGtaVehicle->IsFakeAircraft() ||
		 pGtaVehicle->IsSubHeli() ||
		 pGtaVehicle->IsSubPlane() ||
		 pGtaVehicle->IsSubFakeAircraft() ||
		 handlingAircraft);
}

static CVehicleGTA* GetLocalSelfTestDriverVehicle()
{
	if (!IsVehicleSelfTestInputActive()) {
		return nullptr;
	}

	CPlayerPed* pPlayerPed = pGame ? pGame->FindPlayerPed() : nullptr;
	if (!pPlayerPed || !pPlayerPed->IsInVehicle() || pPlayerPed->IsAPassenger()) {
		return nullptr;
	}

	return pPlayerPed->GetGtaVehicle();
}

static int16_t GetVehicleSelfTestPitch(CVehicleGTA* pGtaVehicle)
{
	if (!IsAircraftVehicle(pGtaVehicle)) {
		return 0;
	}
	if (g_vehicleSelfTestThrottle != 0) {
		return VEHICLE_SELF_TEST_AIRCRAFT_FORWARD_PITCH;
	}
	if (g_vehicleSelfTestBrake != 0) {
		return VEHICLE_SELF_TEST_AIRCRAFT_BACK_PITCH;
	}
	return 0;
}

static void ResetVehicleSelfTestTelemetry(CVehicleGTA* pGtaVehicle)
{
	g_vehicleSelfTestTelemetryVehicle = pGtaVehicle;
	g_vehicleSelfTestTelemetryModel = pGtaVehicle ? pGtaVehicle->GetModelId() : 0;
	g_vehicleSelfTestStartPos = pGtaVehicle ? pGtaVehicle->GetPosition() : CVector(0.0f, 0.0f, 0.0f);
	g_vehicleSelfTestLastPos = g_vehicleSelfTestStartPos;
	g_vehicleSelfTestMaxSpeed = 0.0f;
	g_vehicleSelfTestMaxDistance2D = 0.0f;
	g_vehicleSelfTestMinZ = g_vehicleSelfTestStartPos.z;
	g_vehicleSelfTestMaxZ = g_vehicleSelfTestStartPos.z;
	g_vehicleSelfTestSamples = 0;
	g_vehicleSelfTestAccelHookCount = 0;
	g_vehicleSelfTestBrakeHookCount = 0;
	g_vehicleSelfTestSteerHookCount = 0;
	g_vehicleSelfTestHandBrakeHookCount = 0;
	g_vehicleSelfTestCarGunLRHookCount = 0;
	g_vehicleSelfTestCarGunUDHookCount = 0;
	g_vehicleSelfTestCarGunFireHookCount = 0;
	g_vehicleSelfTestProcessCount = 0;
}

static void SampleVehicleSelfTestTelemetry(CVehicleGTA* pGtaVehicle)
{
	if (!pGtaVehicle) {
		return;
	}
	if (g_vehicleSelfTestTelemetryVehicle != pGtaVehicle ||
		g_vehicleSelfTestTelemetryModel != pGtaVehicle->GetModelId()) {
		ResetVehicleSelfTestTelemetry(pGtaVehicle);
	}

	const CVector pos = pGtaVehicle->GetPosition();
	const CVector speed = pGtaVehicle->GetMoveSpeed();
	const float speed2D = VehicleSelfTestLength2D(speed);
	const float dist2D = VehicleSelfTestLength2D(pos - g_vehicleSelfTestStartPos);
	if (speed2D > g_vehicleSelfTestMaxSpeed) {
		g_vehicleSelfTestMaxSpeed = speed2D;
	}
	if (dist2D > g_vehicleSelfTestMaxDistance2D) {
		g_vehicleSelfTestMaxDistance2D = dist2D;
	}
	if (pos.z < g_vehicleSelfTestMinZ) {
		g_vehicleSelfTestMinZ = pos.z;
	}
	if (pos.z > g_vehicleSelfTestMaxZ) {
		g_vehicleSelfTestMaxZ = pos.z;
	}
	g_vehicleSelfTestLastPos = pos;
	++g_vehicleSelfTestSamples;
}

static void LogVehicleSelfTestTelemetrySummary(const char* reason, CPlayerPed* pPlayerPed)
{
	CVehicleGTA* pGtaVehicle = pPlayerPed ? pPlayerPed->GetGtaVehicle() : nullptr;
	if (pGtaVehicle) {
		SampleVehicleSelfTestTelemetry(pGtaVehicle);
	}

	const bool inVehicle = pPlayerPed && pPlayerPed->IsInVehicle() && !pPlayerPed->IsAPassenger();
	const bool ejected = !inVehicle || (pGtaVehicle && g_vehicleSelfTestTelemetryVehicle && pGtaVehicle != g_vehicleSelfTestTelemetryVehicle);
	const bool moved = g_vehicleSelfTestMaxDistance2D >= 2.0f;
	const bool fell = g_vehicleSelfTestMinZ < (g_vehicleSelfTestStartPos.z - 5.0f);
	CVehicleGTA* loggedVehicle = g_vehicleSelfTestTelemetryVehicle ? g_vehicleSelfTestTelemetryVehicle : pGtaVehicle;
	const bool groundVehicle = loggedVehicle && !IsAircraftVehicle(loggedVehicle);
	const bool zDrift = groundVehicle &&
		(((g_vehicleSelfTestMaxZ > g_vehicleSelfTestStartPos.z + 6.0f) &&
		  (g_vehicleSelfTestMaxSpeed < 0.06f)) ||
		 (g_vehicleSelfTestMaxZ > g_vehicleSelfTestStartPos.z + 20.0f));
	const char* status = ejected ? "EJECT" : (fell ? "FALL" : (zDrift ? "Z_DRIFT" : (moved ? "PASS" : "NO_MOVE")));
	FLog("[VEH_MATRIX64] summary reason=%s status=%s model=%u inVeh=%u samples=%u dist2d=%.2f maxSpeed=%.3f z=%.2f..%.2f start=%.2f %.2f %.2f last=%.2f %.2f %.2f hooks={accel:%u,brake:%u,steer:%u,hand:%u,gunLR:%u,gunUD:%u,gunFire:%u,process:%u} pedals={gas:%.2f,brake:%.2f,steer:%.3f,hand:%u} flags={engine:%u,broken:%u,disableMove:%u,dontSpeed:%u}",
		 reason ? reason : "?",
		 status,
		 loggedVehicle ? loggedVehicle->GetModelId() : g_vehicleSelfTestTelemetryModel,
		 inVehicle ? 1 : 0,
		 g_vehicleSelfTestSamples,
		 g_vehicleSelfTestMaxDistance2D,
		 g_vehicleSelfTestMaxSpeed,
		 g_vehicleSelfTestMinZ,
		 g_vehicleSelfTestMaxZ,
		 g_vehicleSelfTestStartPos.x,
		 g_vehicleSelfTestStartPos.y,
		 g_vehicleSelfTestStartPos.z,
		 g_vehicleSelfTestLastPos.x,
		 g_vehicleSelfTestLastPos.y,
		 g_vehicleSelfTestLastPos.z,
		 g_vehicleSelfTestAccelHookCount,
		 g_vehicleSelfTestBrakeHookCount,
		 g_vehicleSelfTestSteerHookCount,
		 g_vehicleSelfTestHandBrakeHookCount,
		 g_vehicleSelfTestCarGunLRHookCount,
		 g_vehicleSelfTestCarGunUDHookCount,
		 g_vehicleSelfTestCarGunFireHookCount,
		 g_vehicleSelfTestProcessCount,
		 loggedVehicle ? loggedVehicle->m_fGasPedal : 0.0f,
		 loggedVehicle ? loggedVehicle->m_fBreakPedal : 0.0f,
		 loggedVehicle ? loggedVehicle->m_fSteerAngle : 0.0f,
		 loggedVehicle ? loggedVehicle->m_nVehicleFlags.bIsHandbrakeOn : 0,
		 loggedVehicle ? loggedVehicle->m_nVehicleFlags.bEngineOn : 0,
		 loggedVehicle ? loggedVehicle->m_nVehicleFlags.bEngineBroken : 0,
		 loggedVehicle ? loggedVehicle->physicalFlags.bDisableMoveForce : 0,
		 loggedVehicle ? loggedVehicle->physicalFlags.bDontApplySpeed : 0);
	tHandlingData* handling = loggedVehicle ? loggedVehicle->m_pHandlingData : nullptr;
	char collisionEntities[256];
	DescribeVehicleSelfTestCollisionEntities(loggedVehicle, collisionEntities);
	FLog("[VEH_MATRIX64] detail model=%u gtaStatus=%u type=%u usesCol=%u phys=0x%08x collided=%u contact=%u mass=%.1f move=%.3f %.3f %.3f handling={id:%d,traction:%.2f,engineAcc:%.2f,maxVel:%.2f,drive:%u,gears:%u,current:%.2f} collEnt=%s",
		 loggedVehicle ? loggedVehicle->GetModelId() : g_vehicleSelfTestTelemetryModel,
		 loggedVehicle ? static_cast<uint32_t>(loggedVehicle->GetStatus()) : 0,
		 loggedVehicle ? static_cast<uint32_t>(loggedVehicle->GetType()) : 0,
		 loggedVehicle && loggedVehicle->m_bUsesCollision ? 1 : 0,
		 loggedVehicle ? loggedVehicle->m_nPhysicalFlags : 0,
		 loggedVehicle ? loggedVehicle->m_nNumEntitiesCollided : 0,
		 loggedVehicle ? static_cast<uint32_t>(loggedVehicle->m_nContactSurface) : 0,
		 loggedVehicle ? loggedVehicle->m_fMass : 0.0f,
		 loggedVehicle ? loggedVehicle->m_vecMoveSpeed.x : 0.0f,
		 loggedVehicle ? loggedVehicle->m_vecMoveSpeed.y : 0.0f,
		 loggedVehicle ? loggedVehicle->m_vecMoveSpeed.z : 0.0f,
		 handling ? handling->m_nVehicleId : -1,
		 handling ? handling->m_fTractionMultiplier : 0.0f,
		 handling ? handling->m_transmissionData.m_fEngineAcceleration : 0.0f,
		 handling ? handling->m_transmissionData.m_fMaxVelocity : 0.0f,
		 handling ? static_cast<uint32_t>(handling->m_transmissionData.m_nDriveType) : 0,
		 handling ? static_cast<uint32_t>(handling->m_transmissionData.m_nNumberOfGears) : 0,
		 handling ? handling->m_transmissionData.m_fCurrentSpeed : 0.0f,
		 collisionEntities[0] ? collisionEntities : "-");
	LogVehicleSelfTestCollisionEntityDetails(loggedVehicle);
}

static bool IsVehicleSelfTestInputActive()
{
	if (g_vehicleSelfTestUntilTick == 0) {
		return false;
	}
	const uint32_t now = GetTickCount();
	if (now < g_vehicleSelfTestUntilTick) {
		return true;
	}
	LogVehicleSelfTestTelemetrySummary("input-end", pGame ? pGame->FindPlayerPed() : nullptr);
	g_vehicleSelfTestUntilTick = 0;
	g_vehicleSelfTestSteering = 0;
	g_vehicleSelfTestThrottle = 0;
	g_vehicleSelfTestBrake = 0;
	InputApi::ClearCompatibilityAnalog();
	InputApi::ClearCompatibilityButton(SourceButton::Sprint);
	InputApi::ClearCompatibilityButton(SourceButton::Jump);
	ClearVehicleSelfTestTransientKeys();
	FLog("[VEH_TEST64] input-end");
	return false;
}

static void PrepareCurrentVehicleForSelfTest(CPlayerPed* pPlayerPed)
{
	if (!pPlayerPed || !pPlayerPed->IsInVehicle() || pPlayerPed->IsAPassenger()) {
		return;
	}

	CVehicleGTA* pGtaVehicle = pPlayerPed->GetGtaVehicle();
	if (!pGtaVehicle) {
		return;
	}

	PrepareVehicleForSelfTest(pGtaVehicle);
}

extern "C" void ApplyVehicleSelfTestInputKeys()
{
	if (!IsVehicleSelfTestInputActive()) {
		return;
	}

	CPlayerPed* pPlayerPed = pGame ? pGame->FindPlayerPed() : nullptr;
	PrepareCurrentVehicleForSelfTest(pPlayerPed);
	CVehicleGTA* pGtaVehicle = pPlayerPed ? pPlayerPed->GetGtaVehicle() : nullptr;
	ApplyVehicleSelfTestPedalState(pGtaVehicle);
	const int16_t pitch = GetVehicleSelfTestPitch(pGtaVehicle);

	InputApi::SetCompatibilityAnalog(static_cast<uint16_t>(g_vehicleSelfTestSteering), static_cast<uint16_t>(pitch));
	ClearVehicleSelfTestTransientKeys();
	InputApi::SetCompatibilityButton(SourceButton::Sprint, g_vehicleSelfTestThrottle != 0);
	InputApi::SetCompatibilityButton(SourceButton::Jump, g_vehicleSelfTestBrake != 0);
	InputApi::ClearCompatibilityButton(SourceButton::Handbrake);

	SampleVehicleSelfTestTelemetry(pGtaVehicle);

	const uint32_t now = GetTickCount();
	if (now - g_vehicleSelfTestLastLogTick > 1000) {
		g_vehicleSelfTestLastLogTick = now;
		const CVector pos = pGtaVehicle ? pGtaVehicle->GetPosition() : CVector(0.0f, 0.0f, 0.0f);
		const CVector speed = pGtaVehicle ? pGtaVehicle->GetMoveSpeed() : CVector(0.0f, 0.0f, 0.0f);
		FLog("[VEH_TEST64] input-active inVeh=%u passenger=%u model=%u throttle=%u brake=%u steer=%d pitch=%d engine=%u hand=%u gas=%.2f brakePed=%.2f speed=%.3f %.3f %.3f pos=%.2f %.2f %.2f",
			 pPlayerPed && pPlayerPed->IsInVehicle() ? 1 : 0,
			 pPlayerPed && pPlayerPed->IsAPassenger() ? 1 : 0,
			 pGtaVehicle ? pGtaVehicle->GetModelId() : 0,
			 g_vehicleSelfTestThrottle,
			 g_vehicleSelfTestBrake,
			 g_vehicleSelfTestSteering,
			 pitch,
			 pGtaVehicle ? pGtaVehicle->m_nVehicleFlags.bEngineOn : 0,
			 pGtaVehicle ? pGtaVehicle->m_nVehicleFlags.bIsHandbrakeOn : 0,
			 pGtaVehicle ? pGtaVehicle->m_fGasPedal : 0.0f,
			 pGtaVehicle ? pGtaVehicle->m_fBreakPedal : 0.0f,
			 speed.x,
			 speed.y,
			 speed.z,
			 pos.x,
			 pos.y,
			 pos.z);
	}
}

extern "C" void StartVehicleSelfTestInput(int durationMs, int steering, bool throttle, bool brake)
{
	int safeDurationMs = durationMs;
	if (safeDurationMs < 250) {
		safeDurationMs = 250;
	}
	if (safeDurationMs > 15000) {
		safeDurationMs = 15000;
	}

	int safeSteering = steering;
	if (safeSteering < -128) {
		safeSteering = -128;
	}
	if (safeSteering > 127) {
		safeSteering = 127;
	}

	g_vehicleSelfTestUntilTick = GetTickCount() + static_cast<uint32_t>(safeDurationMs);
	g_vehicleSelfTestSteering = static_cast<int16_t>(safeSteering);
	g_vehicleSelfTestThrottle = throttle ? 0xFF : 0x00;
	g_vehicleSelfTestBrake = brake ? 0xFF : 0x00;
	g_vehicleSelfTestLastLogTick = 0;

	CPlayerPed* pPlayerPed = pGame ? pGame->FindPlayerPed() : nullptr;
	PrepareCurrentVehicleForSelfTest(pPlayerPed);
	CVehicleGTA* pGtaVehicle = pPlayerPed ? pPlayerPed->GetGtaVehicle() : nullptr;
	ClearVehicleSelfTestTransientKeys();
	ApplyVehicleSelfTestPedalState(pGtaVehicle);
	if (pGtaVehicle && (g_vehicleSelfTestTelemetryVehicle != pGtaVehicle ||
		g_vehicleSelfTestTelemetryModel != pGtaVehicle->GetModelId())) {
		ResetVehicleSelfTestTelemetry(pGtaVehicle);
	}
	SampleVehicleSelfTestTelemetry(pGtaVehicle);
	FLog("[VEH_TEST64] input-start duration=%d steer=%d throttle=%u brake=%u inVeh=%u model=%u",
		 safeDurationMs,
		 g_vehicleSelfTestSteering,
		 g_vehicleSelfTestThrottle,
		 g_vehicleSelfTestBrake,
		 pPlayerPed && pPlayerPed->IsInVehicle() ? 1 : 0,
		 pGtaVehicle ? pGtaVehicle->GetModelId() : 0);
}

uint16_t(*CPad__GetPedWalkLeftRight)(uintptr_t thiz);
uint16_t CPad__GetPedWalkLeftRight_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		// Remote player
		uint16_t dwResult = RemotePlayerKeys[byteCurPlayer].wKeyLR;
		if ((dwResult == 0xFF80 || dwResult == 0x80) &&
			RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_WALK])
		{
			dwResult = 0x20;
		}
		return dwResult;
	}
	else
	{
		// Local player
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return static_cast<uint16_t>(Xyron::Input::SourceAnalogLeftRight());
		}
		const uint16_t leftRight = CPad__GetPedWalkLeftRight(thiz);
		InputApi::SetCompatibilityAnalogLeftRight(leftRight);
		return leftRight;
	}
}

uint16_t(*CPad__GetPedWalkUpDown)(uintptr_t thiz);
uint16_t CPad__GetPedWalkUpDown_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		// Remote player
		uint16_t dwResult = RemotePlayerKeys[byteCurPlayer].wKeyUD;
		if ((dwResult == 0xFF80 || dwResult == 0x80) &&
			RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_WALK])
		{
			dwResult = 0x20;
		}
		return dwResult;
	}
	else
	{
		// Local player
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return static_cast<uint16_t>(Xyron::Input::SourceAnalogUpDown());
		}
		const uint16_t upDown = CPad__GetPedWalkUpDown(thiz);
		InputApi::SetCompatibilityAnalogUpDown(upDown);
		return upDown;
	}
}

uint32_t(*CPad__GetSprint)(uintptr_t thiz, uint32_t unk);
uint32_t CPad__GetSprint_hook(uintptr_t thiz, uint32_t unk)
{
	if (CWorld::PlayerInFocus)
	{
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_SPRINT];
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			const bool sprint = Xyron::Input::IsSourceSprintPressed() || Xyron::Input::IsSourceAcceleratePressed();
			return sprint ? 1 : 0;
		}
		const bool sprint = CPad__GetSprint(thiz, unk) != 0;
		InputApi::SetCompatibilityButton(SourceButton::Sprint, sprint);
		return sprint ? 1 : 0;
	}
}

uint32_t(*CPad__JumpJustDown)(uintptr_t thiz);
uint32_t CPad__JumpJustDown_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		if (!RemotePlayerKeys[byteCurPlayer].bIgnoreJump &&
			RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_JUMP] &&
			!RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_HANDBRAKE])
		{
			RemotePlayerKeys[byteCurPlayer].bIgnoreJump = true;
			return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_JUMP];
		}

		return 0;
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			const bool jumpJustPressed = Xyron::Input::ConsumeJumpJustPressed();
			return jumpJustPressed ? 1 : 0;
		}
		const bool jumpJustDown = CPad__JumpJustDown(thiz) != 0;
		InputApi::SetCompatibilityButton(SourceButton::Jump, jumpJustDown);
		return jumpJustDown ? 1 : 0;
	}
}

uint32_t(*CPad__GetJump)(uintptr_t thiz);
uint32_t CPad__GetJump_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		if (RemotePlayerKeys[byteCurPlayer].bIgnoreJump) return 0;
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_JUMP];
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return Xyron::Input::IsSourceJumpPressed() ? 1 : 0;
		}
		const uint32_t jumpHeld = CPad__GetJump ? CPad__GetJump(thiz) : CPad__JumpJustDown(thiz);
		InputApi::SetCompatibilityButton(SourceButton::Jump, jumpHeld != 0);
		return jumpHeld;
	}
}

uint32_t(*CPad__GetAutoClimb)(uintptr_t thiz);
uint32_t CPad__GetAutoClimb_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_JUMP];
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return Xyron::Input::IsSourceJumpPressed() ? 1 : 0;
		}
		const bool autoClimb = CPad__GetAutoClimb(thiz) != 0;
		InputApi::SetCompatibilityButton(SourceButton::Jump, autoClimb);
		return autoClimb ? 1 : 0;
	}
}

uint32_t(*CPad__GetAbortClimb)(uintptr_t thiz);
uint32_t CPad__GetAbortClimb_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_SECONDARY_ATTACK];
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return Xyron::Input::IsSourceSecondaryPressed() ? 1 : 0;
		}
		const bool abortClimb = CPad__GetAutoClimb(thiz) != 0;
		InputApi::SetCompatibilityButton(SourceButton::SecondaryAttack, abortClimb);
		return abortClimb ? 1 : 0;
	}
}

uint32_t(*CPad__DiveJustDown)();
uint32_t CPad__DiveJustDown_hook()
{
	if (CWorld::PlayerInFocus)
	{
		// remote player
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_FIRE];
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return Xyron::Input::IsSourceFirePressed() ? 1 : 0;
		}
		const bool dive = CPad__DiveJustDown() != 0;
		InputApi::SetCompatibilityButton(SourceButton::Fire, dive);
		return dive ? 1 : 0;
	}
}

uint32_t(*CPad__SwimJumpJustDown)(uintptr_t thiz);
uint32_t CPad__SwimJumpJustDown_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_JUMP];
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			const bool jumpJustPressed = Xyron::Input::ConsumeJumpJustPressed();
			return jumpJustPressed ? 1 : 0;
		}
		const bool swimJump = CPad__SwimJumpJustDown(thiz) != 0;
		InputApi::SetCompatibilityButton(SourceButton::Jump, swimJump);
		return swimJump ? 1 : 0;
	}
}

uint32_t(*CPad__DuckJustDown)(uintptr_t thiz, int unk);
uint32_t CPad__DuckJustDown_hook(uintptr_t thiz, int unk)
{
	if (CWorld::PlayerInFocus)
	{
		return 0;
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return 0;
		}
		return CPad__DuckJustDown(thiz, unk);
	}
}

uint32_t(*CPad__MeleeAttackJustDown)(uintptr_t thiz);
uint32_t CPad__MeleeAttackJustDown_hook(uintptr_t thiz)
{
	/*
		0 - �� ����
		1 - ������� ���� (���)
		2 - ������� ���� (��� + F)
	*/

	if (CWorld::PlayerInFocus)
	{
		if (RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_HANDBRAKE] &&
			RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_SECONDARY_ATTACK])
			return 2;

		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_FIRE];
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			const uint32_t dwResult = Xyron::Input::IsSourceFirePressed() ? (Xyron::Input::IsSourceSecondaryPressed() ? 2 : 1) : 0;
			return dwResult;
		}
		uint32_t dwResult = CPad__MeleeAttackJustDown(thiz);
		if (Xyron::Input::IsSourceFirePressed()) {
			dwResult = Xyron::Input::IsSourceSecondaryPressed() ? 2 : 1;
		}
		InputApi::SetCompatibilityButton(SourceButton::Fire, dwResult != 0);
		InputApi::SetCompatibilityButton(SourceButton::SecondaryAttack, Xyron::Input::IsSourceSecondaryPressed());

		return dwResult;
	}
}

uint32_t(*CPad__GetBlock)(uintptr_t thiz);
uint32_t CPad__GetBlock_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		if (RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_JUMP] &&
			RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_HANDBRAKE])
			return 1;

		return 0;
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return 0;
		}
		return CPad__GetBlock(thiz);
	}
}

int16_t(*CPad__GetSteeringLeftRight)(uintptr_t thiz);
int16_t CPad__GetSteeringLeftRight_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		// remote player
		return (int16_t)RemotePlayerKeys[byteCurPlayer].wKeyLR;
	}
	else
	{
		// local player
		CPlayerPed* pPlayerPed = pGame ? pGame->FindPlayerPed() : nullptr;
		if (IsVehicleSelfTestInputActive() &&
			pPlayerPed &&
			pPlayerPed->IsInVehicle() &&
			!pPlayerPed->IsAPassenger())
		{
			++g_vehicleSelfTestSteerHookCount;
			ApplyVehicleSelfTestInputKeys();
			return g_vehicleSelfTestSteering;
		}
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return Xyron::Input::SourceAnalogLeftRight();
		}
		const uint16_t leftRight = CPad__GetSteeringLeftRight(thiz);
		InputApi::SetCompatibilityAnalogLeftRight(leftRight);
		return leftRight;
	}
}

uint16_t(*CPad__GetSteeringUpDown)(uintptr_t thiz);
uint16_t CPad__GetSteeringUpDown_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		// remote player
		return RemotePlayerKeys[byteCurPlayer].wKeyUD;
	}
	else
	{
		// local player
		CPlayerPed* pPlayerPed = pGame ? pGame->FindPlayerPed() : nullptr;
		if (IsVehicleSelfTestInputActive() &&
			pPlayerPed &&
			pPlayerPed->IsInVehicle() &&
			!pPlayerPed->IsAPassenger())
		{
			++g_vehicleSelfTestSteerHookCount;
			ApplyVehicleSelfTestInputKeys();
			const uint16_t upDown = static_cast<uint16_t>(GetVehicleSelfTestPitch(pPlayerPed->GetGtaVehicle()));
			InputApi::SetCompatibilityAnalogUpDown(upDown);
			return upDown;
		}
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return static_cast<uint16_t>(Xyron::Input::SourceAnalogUpDown());
		}
		const uint16_t upDown = CPad__GetSteeringUpDown(thiz);
		InputApi::SetCompatibilityAnalogUpDown(upDown);
		return upDown;
	}
}

uint16_t(*CPad__GetAccelerate)(uintptr_t thiz);
uint16_t CPad__GetAccelerate_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		// remote player
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_SPRINT] ? 0xFF : 0x00;
	}
	else
	{
		// local player
		CPlayerPed* pPlayerPed = pGame ? pGame->FindPlayerPed() : nullptr;
		if (pPlayerPed)
		{
			if (!pPlayerPed->IsInVehicle() || pPlayerPed->IsAPassenger())
				return 0;
		}
		if (IsVehicleSelfTestInputActive())
		{
			++g_vehicleSelfTestAccelHookCount;
			ApplyVehicleSelfTestInputKeys();
			return g_vehicleSelfTestThrottle;
		}

		if (Xyron::Input::IsSourceInputOverrideEnabled())
		{
			uint16_t wAccelerate = Xyron::Input::IsSourceAcceleratePressed() ? 0xFF : 0x00;
			if (wAccelerate != 0 && pPlayerPed)
			{
				CVehicleGTA* pGtaVehicle = pPlayerPed->GetGtaVehicle();
				if (pGtaVehicle && !pGtaVehicle->m_nVehicleFlags.bEngineOn)
				{
					pGtaVehicle->m_nVehicleFlags.bEngineOn = 1;
				}
			}
			return wAccelerate;
		}

		// local player
		uint16_t wAccelerate = CPad__GetAccelerate(thiz);
		InputApi::SetCompatibilityButton(SourceButton::Sprint, wAccelerate != 0);
		if (wAccelerate != 0 && pPlayerPed)
		{
			CVehicleGTA* pGtaVehicle = pPlayerPed->GetGtaVehicle();
			if (pGtaVehicle && !pGtaVehicle->m_nVehicleFlags.bEngineOn)
			{
				pGtaVehicle->m_nVehicleFlags.bEngineOn = 1;
				static uint32_t s_lastThrottleEngineFixLog = 0;
				const uint32_t now = GetTickCount();
				if (now - s_lastThrottleEngineFixLog > 1000) {
					s_lastThrottleEngineFixLog = now;
					FLog("[VEH_INPUT64] local throttle forced engine on");
				}
			}
		}

		return wAccelerate;
	}
}

uint16_t(*CPad__GetBrake)(uintptr_t thiz);
uint16_t CPad__GetBrake_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		// remote player
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_JUMP] ? 0xFF : 0x00;
	}
	else
	{
        CPlayerPed* pPlayerPed = pGame ? pGame->FindPlayerPed() : nullptr;
        if (pPlayerPed)
        {
            if (!pPlayerPed->IsInVehicle() || pPlayerPed->IsAPassenger())
                return 0;
        }
        if (IsVehicleSelfTestInputActive())
        {
            ++g_vehicleSelfTestBrakeHookCount;
            ApplyVehicleSelfTestInputKeys();
            return g_vehicleSelfTestBrake;
        }

		if (Xyron::Input::IsSourceInputOverrideEnabled())
		{
			uint16_t wBrake = Xyron::Input::IsSourceBrakePressed() ? 0xFF : 0x00;
			if (wBrake != 0 && pPlayerPed)
			{
				CVehicleGTA* pGtaVehicle = pPlayerPed->GetGtaVehicle();
				if (pGtaVehicle && !pGtaVehicle->m_nVehicleFlags.bEngineOn)
				{
					pGtaVehicle->m_nVehicleFlags.bEngineOn = 1;
				}
			}
			return wBrake;
		}

		// local player
		uint16_t wBrake = CPad__GetBrake(thiz);
		InputApi::SetCompatibilityButton(SourceButton::Jump, wBrake != 0);
        if (wBrake != 0 && pPlayerPed)
        {
            CVehicleGTA* pGtaVehicle = pPlayerPed->GetGtaVehicle();
            if (pGtaVehicle && !pGtaVehicle->m_nVehicleFlags.bEngineOn)
            {
                pGtaVehicle->m_nVehicleFlags.bEngineOn = 1;
                static uint32_t s_lastBrakeEngineFixLog = 0;
                const uint32_t now = GetTickCount();
                if (now - s_lastBrakeEngineFixLog > 1000) {
                    s_lastBrakeEngineFixLog = now;
                    FLog("[VEH_INPUT64] local brake forced engine on");
                }
            }
        }
		return wBrake;
	}
}

uint32_t(*CPad__GetHandBrake)(uintptr_t thiz);
uint32_t CPad__GetHandBrake_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		// remote player
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_HANDBRAKE] ? 0xFF : 0x00;
	}
	else
	{
		// local player
		CPlayerPed* pPlayerPed = pGame ? pGame->FindPlayerPed() : nullptr;
		if (IsVehicleSelfTestInputActive() &&
			pPlayerPed &&
			pPlayerPed->IsInVehicle() &&
			!pPlayerPed->IsAPassenger())
		{
			++g_vehicleSelfTestHandBrakeHookCount;
			ApplyVehicleSelfTestInputKeys();
			return 0;
		}
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			uint32_t handBrake = Xyron::Input::IsSourceHandbrakePressed() ? 0xFF : 0x00;
			return handBrake;
		}
		uint32_t handBrake = CPad__GetHandBrake(thiz);
		InputApi::SetCompatibilityButton(SourceButton::Handbrake, handBrake != 0);
		return handBrake;
	}
}

uint32_t(*CPad__GetHorn)(uintptr_t thiz);
uint32_t CPad__GetHorn_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		// remote player
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_CROUCH];
	}
	else
	{
		// local player
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return Xyron::Input::IsSourceHornPressed() ? 1 : 0;
		}
		uint32_t horn = CPad__GetHorn(thiz);
		//Log("horn: %d", horn);
		InputApi::SetCompatibilityButton(SourceButton::Crouch, horn != 0);
		return horn;
	}
}

/*extern bool g_bLockEnterVehicleWidget;
extern bool g_bForceEnterVehicle;
uint32_t(*CPad__ExitVehicleJustDown)(uintptr_t thiz, int a2, uintptr_t vehicle, int a4, uintptr_t vec);
uint32_t CPad__ExitVehicleJustDown_hook(uintptr_t thiz, int a2, uintptr_t vehicle, int a4, uintptr_t vec)
{
	int result = CPad__ExitVehicleJustDown(thiz, a2, vehicle, a4, vec);

	if (g_bForceEnterVehicle)
	{
		g_bForceEnterVehicle = false;
		return true;
	}

	if (g_bLockEnterVehicleWidget) return false;

	return result;
}
*/

uint32_t(*CPad__ExitVehicleJustDown)(uintptr_t thiz, int a2, uintptr_t vehicle, int a4, uintptr_t vec);
uint32_t CPad__ExitVehicleJustDown_hook(uintptr_t thiz, int a2, uintptr_t vehicle, int a4, uintptr_t vec)
{
	const uint32_t now = GetTickCount();

	if (!Xyron::Input::CanProcessPassengerVehicleAction(now))
		return 0;

	if (pNetGame)
	{
		CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
		if (pPlayerPool)
		{
			CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
			if (pLocalPlayer) {
				if (pLocalPlayer->HandlePassengerEntry())
				{
					Xyron::Input::MarkPassengerVehicleActionHandled(now);
					return 0;
				}
			}
		}
	}

	return CPad__ExitVehicleJustDown(thiz, a2, vehicle, a4, vec);
}

uint32_t(*CPad__GetExitVehicle)(uintptr_t thiz);
uint32_t CPad__GetExitVehicle_hook(uintptr_t thiz)
{
    return 0;
}


/* Weapons */

bool (*CPad__GetEnterTargeting)(uintptr_t thiz);
bool CPad__GetEnterTargeting_hook(uintptr_t thiz)
{
    if (CWorld::PlayerInFocus)
    {
        return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_HANDBRAKE];
	}
    else
    {
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return Xyron::Input::IsSourceFirePressed();
		}
        uint8_t old = CWorld::PlayerInFocus;
        CWorld::PlayerInFocus = byteCurPlayer;
        uintptr_t result = CPad__GetEnterTargeting(thiz);
        InputApi::SetCompatibilityButton(SourceButton::Handbrake, result != 0);
        CWorld::PlayerInFocus = old;
        return result;
	}
}

bool bWeaponClicked = false;

extern "C" {
	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_changeGun(JNIEnv *pEnv, jobject thiz)
	{
		if (!pGame || !pGame->FindPlayerPed()) {
			return;
		}

		bWeaponClicked = !bWeaponClicked;
	}

	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_selectWeapon(JNIEnv *pEnv, jobject thiz, jint weaponId)
	{
		if (!pGame) {
			return;
		}

		CPlayerPed* pPlayerPed = pGame->FindPlayerPed();
		if (!pPlayerPed) {
			return;
		}

		if (weaponId < 0 || weaponId > 46) {
			weaponId = 0;
		}

		if (weaponId != 0) {
			CWeapon* pSlot = pPlayerPed->FindWeaponSlot(static_cast<uint8_t>(weaponId));
			if (!pSlot || pSlot->dwType != static_cast<eWeaponType>(weaponId)) {
				return;
			}
		}

		pPlayerPed->SetArmedWeapon(static_cast<uint8_t>(weaponId), false);
	}
}

uint32_t(*CPad__CycleWeaponRightJustDown)(uintptr_t thiz);
uint32_t CPad__CycleWeaponRightJustDown_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		return 0;
	}

	if (!bWeaponClicked)
	{
		return 0;
	}

	bWeaponClicked = false;
	return 1;
}

uint32_t(*CPad__CycleWeaponLeftJustDown)(uintptr_t thiz);
uint32_t CPad__CycleWeaponLeftJustDown_hook(uintptr_t thiz)
{
	if (CWorld::PlayerInFocus)
	{
		return 0;
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return 0;
		}
		return CPad__CycleWeaponLeftJustDown(thiz);
	}
}

bool (*CPad__GetWeapon)(uintptr_t thiz, CPedGTA* pPed);
bool CPad__GetWeapon_hook(uintptr_t thiz, CPedGTA* pPed)
{
	if (CWorld::PlayerInFocus)
	{
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_FIRE];
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return Xyron::Input::IsSourceFirePressed();
		}
		const bool weapon = CPad__GetWeapon(thiz, pPed) || Xyron::Input::IsSourceFirePressed();
		InputApi::SetCompatibilityButton(SourceButton::Fire, weapon);
		return weapon;
	}
}

uint32_t(*CCamera_IsTargetingActive)(uintptr_t thiz, CPedGTA* pPed);
uint32_t CCamera_IsTargetingActive_hook(uintptr_t thiz, CPedGTA* pPed)
{
    if (pPed != GamePool_FindPlayerPed())
    {
        return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_HANDBRAKE] ? 1 : 0;
    }
    else
    {
        uint32_t bIsTargeting = GTASAEngineApi::CameraIsTargetingActive();

        InputApi::SetCompatibilityButton(SourceButton::Handbrake, bIsTargeting != 0);
        return bIsTargeting;
    }
}

uint32_t(*CPad__GetDisplayVitalStats)(uint32_t thiz);
uint32_t CPad__GetDisplayVitalStats_hook(uint32_t thiz)
{
	uint32_t result = CPad__GetDisplayVitalStats(thiz);

	if (pUI) {
		if (result) pUI->playertablist()->show();
	}

	return 0;
}

uint32_t(*CPad__GetLookBehindForPed)(uint32_t thiz);
uint32_t CPad__GetLookBehindForPed_hook(uint32_t thiz)
{
	return CPad__GetLookBehindForPed(thiz);
}

int (*CPad__GetNitroFired)(uintptr_t thiz);
int CPad__GetNitroFired_hook(uintptr_t thiz)
{
    if(CWorld::PlayerInFocus)
    {
        if(RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_FIRE])
            return 1;
    }
    else
    {
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return Xyron::Input::IsSourceFirePressed();
		}
        const bool nitroFired = CPad__GetNitroFired(thiz) || Xyron::Input::IsSourceFirePressed();
        InputApi::SetCompatibilityButton(SourceButton::Fire, nitroFired);
        return nitroFired;
    }
}

uint32_t (*CPad__GetLookLeft)(uintptr_t thiz);
uint32_t CPad__GetLookLeft_hook(uintptr_t thiz)
{
    if(CWorld::PlayerInFocus)
    {
        if(RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_FIRE])
            return 1;
    }
    else
    {
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return 0;
		}
        const bool lookLeft = CPad__GetLookLeft(thiz) != 0;
        InputApi::SetCompatibilityButton(SourceButton::Fire, lookLeft);
        return lookLeft ? 1 : 0;
    }
}

uint32_t (*CPad__GetLookRight)(uintptr_t thiz);
uint32_t CPad__GetLookRight_hook(uintptr_t thiz)
{
    if(CWorld::PlayerInFocus)
    {
        if(RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_FIRE])
            return 1;
    }
    else
    {
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return 0;
		}
        const bool lookRight = CPad__GetLookRight(thiz) != 0;
        InputApi::SetCompatibilityButton(SourceButton::Fire, lookRight);
        return lookRight ? 1 : 0;
    }
}

uint16_t(*CPad__GetCarGunLeftRight)(uintptr_t thiz, bool a2, bool a3);
uint16_t CPad__GetCarGunLeftRight_hook(uintptr_t thiz, bool a2, bool a3)
{
	if (CWorld::PlayerInFocus)
	{
		// Remote player
		uint16_t dwResult = RemotePlayerKeys[byteCurPlayer].wKeyLR;
		if (RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_FIRE])
		{
			dwResult = PAD_AXIS_NEGATIVE;
		}
		return dwResult;
	}
	else
	{
		// Local player
		if (CVehicleGTA* pGtaVehicle = GetLocalSelfTestDriverVehicle())
		{
			++g_vehicleSelfTestCarGunLRHookCount;
			PrepareVehicleForSelfTest(pGtaVehicle);
			ApplyVehicleSelfTestInputKeys();
			return static_cast<uint16_t>(g_vehicleSelfTestSteering);
		}

		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return static_cast<uint16_t>(Xyron::Input::SourceAnalogLeftRight());
		}

		uint16_t dwResult = CPad__GetCarGunLeftRight(thiz, a2, a3);

		if ( dwResult == PAD_AXIS_POSITIVE )
		{
			InputApi::SetCompatibilityAnalogLeftRight(1);
			dwResult = PAD_AXIS_POSITIVE;
		}
		else if ( dwResult == PAD_AXIS_NEGATIVE )
		{
			InputApi::SetCompatibilityAnalogLeftRight(1);
			dwResult = PAD_AXIS_NEGATIVE;
		}
		else
		{
			InputApi::SetCompatibilityAnalogLeftRight(0);
		}

		return dwResult;
	}
}

uint16_t(*CPad__GetCarGunUpDown)(uintptr_t thiz, bool a2, void *a3, float a4, bool a5);
uint16_t CPad__GetCarGunUpDown_hook(uintptr_t thiz, bool a2, void *a3, float a4, bool a5)
{
	if (CWorld::PlayerInFocus)
	{
		// Remote player
		uint16_t dwResult = RemotePlayerKeys[byteCurPlayer].wKeyUD;
		if (RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_FIRE])
		{
			dwResult = PAD_AXIS_NEGATIVE;
		}
		return dwResult;
	}
	else
	{
		// Local player
		if (CVehicleGTA* pGtaVehicle = GetLocalSelfTestDriverVehicle())
		{
			++g_vehicleSelfTestCarGunUDHookCount;
			PrepareVehicleForSelfTest(pGtaVehicle);
			ApplyVehicleSelfTestInputKeys();
			return static_cast<uint16_t>(GetVehicleSelfTestPitch(pGtaVehicle));
		}

		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return static_cast<uint16_t>(Xyron::Input::SourceAnalogUpDown());
		}

		uint16_t dwResult = CPad__GetCarGunUpDown(thiz, a2, a3, a4, a5);

		if ( dwResult == PAD_AXIS_POSITIVE )
		{
			InputApi::SetCompatibilityAnalogUpDown(1);
			dwResult = PAD_AXIS_POSITIVE;
		}
		else if ( dwResult == PAD_AXIS_NEGATIVE )
		{
			InputApi::SetCompatibilityAnalogUpDown(1);
			dwResult = PAD_AXIS_NEGATIVE;
		}
		else
		{
			InputApi::SetCompatibilityAnalogUpDown(0);
		}

		return dwResult;
	}
}

uint32_t (*CPad__GetCarGunFired)(uintptr_t thiz, bool a2, bool a3);
uint32_t CPad__GetCarGunFired_hook(uintptr_t thiz, bool a2, bool a3)
{
	if(CWorld::PlayerInFocus)
	{
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_FIRE];
	}
	else
	{
		if (CVehicleGTA* pGtaVehicle = GetLocalSelfTestDriverVehicle())
		{
			++g_vehicleSelfTestCarGunFireHookCount;
			PrepareVehicleForSelfTest(pGtaVehicle);
			ApplyVehicleSelfTestInputKeys();
			InputApi::ClearCompatibilityButton(SourceButton::Fire);
			return 0;
		}

		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return Xyron::Input::IsSourceFirePressed();
		}
		const bool carGunFired = CPad__GetCarGunFired(thiz, a2, a3) || Xyron::Input::IsSourceFirePressed();
		InputApi::SetCompatibilityButton(SourceButton::Fire, carGunFired);
		return carGunFired;
	}
}

bool (*CPad__GetTurretRight)(uintptr_t *thiz);
bool CPad__GetTurretRight_hook(uintptr_t *thiz)
{
	if(CWorld::PlayerInFocus)
	{
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_LOOK_RIGHT];
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return false;
		}
		const bool turretRight = CPad__GetTurretRight(thiz);
		InputApi::SetCompatibilityButton(SourceButton::LookRight, turretRight);
		return turretRight;
	}
}

bool (*CPad__GetTurretLeft)(uintptr_t *thiz);
bool CPad__GetTurretLeft_hook(uintptr_t *thiz)
{
	if(CWorld::PlayerInFocus)
	{
		return RemotePlayerKeys[byteCurPlayer].bKeys[ePadKeys::KEY_LOOK_LEFT];
	}
	else
	{
		if (Xyron::Input::IsSourceInputOverrideEnabled()) {
			return false;
		}
		const bool turretLeft = CPad__GetTurretLeft(thiz);
		InputApi::SetCompatibilityButton(SourceButton::LookLeft, turretLeft);
		return turretLeft;
	}
}

extern uint8_t byteInternalPlayer;
void AllVehicles__ProcessControl_hook(uintptr_t thiz)
{
    auto* pVehicle = (CVehicleGTA*)thiz;
    uintptr_t this_vtable = *(uintptr_t*)pVehicle;
    this_vtable = GTASAEngineApi::RebaseFromGta(this_vtable);

    uintptr_t call_addr = 0;

    switch(this_vtable)
    {
#if VER_x32
        // CAutomobile
		case 0x0066D678:
		call_addr = 0x00553E44;
		break;

		// CBoat
		case 0x0066DA20:
		call_addr = 0x0056BEC0;
		break;

		// CBike
		case 0x0066D7F0:
		call_addr = 0x00561A90;
		break;

		// CPlane
		case 0x0066DD84:
		call_addr = 0x00575CF8;
		break;

		// CHeli
		case 0x0066DB34:
		call_addr = 0x005712A8;
		break;

		// CBmx
		case 0x0066D908:
		call_addr = 0x00568B84;
		break;

		// CMonsterTruck
		case 0x0066DC5C:
		call_addr = 0x00574864;
		break;

		// CQuadBike
		case 0x0066DEAC:
		call_addr = 0x0057A2F0;
		break;

		// CTrain
		case 0x0066E0FC:
		call_addr = 0x0057D0A0;
		break;
#else
        // CAutomobile
        case 0x83BB50:
            call_addr = 0x67459C;
            break;

            // CBoat
        case 0x83C2A0:
            call_addr = 0x68DCE8;
            break;

            // CBike
        case 0x83BE40:
            call_addr = 0x682BC8;
            break;

            // CPlane
        case 0x83C968:
            call_addr = 0x6993B8;
            break;

            // CHeli
        case 0x83C4C8:
            call_addr = 0x693978;
            break;

            // CBmx
        case 0x83C070:
            call_addr = 0x693978;
            break;

            // CMonsterTruck
        case 0x83C718:
            call_addr = 0x698090;
            break;

            // CQuadBike
        case 0x83CBB8:
            call_addr = 0x69DB44;
            break;

            // CTrain
        case 0x83D058:
            call_addr = 0x6A0A20;
            break;
#endif
    }

    if (!call_addr) {
        static uint32_t s_lastUnknownVehicleProcessLog = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastUnknownVehicleProcessLog > 1000) {
            s_lastUnknownVehicleProcessLog = now;
            FLog("[VEH_PROC64] unknown-vtable vtable=0x%zx model=%u vehicle=%p",
                 static_cast<size_t>(this_vtable),
                 pVehicle ? pVehicle->GetModelId() : 0,
                 pVehicle);
        }
        return;
    }

    const bool localDrivenVehicle =
            pVehicle &&
            pVehicle->pDriver &&
            pVehicle->pDriver == GamePool_FindPlayerPed();
    if (localDrivenVehicle) {
        PrepareVehicleForLocalDriver(pVehicle);
    }

    const bool selfTestVehicle =
            IsVehicleSelfTestInputActive() &&
            localDrivenVehicle;
    if (selfTestVehicle) {
        ++g_vehicleSelfTestProcessCount;
        PrepareVehicleForSelfTest(pVehicle);
        SampleVehicleSelfTestTelemetry(pVehicle);
    }

    if(pVehicle && pVehicle->pDriver)
    {
        byteCurPlayer = FindPlayerNumFromPedPtr(pVehicle->pDriver);
    }

    if(pVehicle->pDriver && pVehicle->pDriver->m_nPedType == 0 &&
       pVehicle->pDriver != GamePool_FindPlayerPed() &&
            CWorld::PlayerInFocus  == 0) // CWorld::PlayerInFocus
    {
        CWorld::PlayerInFocus = 0;

        pVehicle->pDriver->m_nPedType = static_cast<ePedType>(4);
        GTASAEngineApi::ServiceVehicleAudioEntity(reinterpret_cast<uintptr_t>(&pVehicle->m_VehicleAudioEntity));
        pVehicle->pDriver->m_nPedType = static_cast<ePedType>(0);
    }
    else
    {
        GTASAEngineApi::ServiceVehicleAudioEntity(reinterpret_cast<uintptr_t>(&pVehicle->m_VehicleAudioEntity));
    }

    // Tyre burst fix
    if (pVehicle->pDriver)
    {
        if (pVehicle->m_nVehicleFlags.bTyresDontBurst)
        {
            pVehicle->m_nVehicleFlags.bTyresDontBurst = 0;
        }
        if(!pVehicle->m_nVehicleFlags.bCanBeDamaged) pVehicle->m_nVehicleFlags.bCanBeDamaged = true;
    }
    else
    {
        if (!pVehicle->m_nVehicleFlags.bTyresDontBurst)
        {
            pVehicle->m_nVehicleFlags.bTyresDontBurst = 1;
        }
        if (pVehicle->m_nVehicleFlags.bCanBeDamaged) pVehicle->m_nVehicleFlags.bCanBeDamaged = false;
    }

    GTASAEngineApi::ProcessVehicleControl(pVehicle, call_addr);
    if (localDrivenVehicle) {
        PrepareVehicleForLocalDriver(pVehicle);
    }
    if (selfTestVehicle) {
        PrepareVehicleForSelfTest(pVehicle);
        SampleVehicleSelfTestTelemetry(pVehicle);
    }
}

extern float * pfCameraExtZoom;
void (*CPed__ProcessControl)(uintptr_t thiz);
void CPed__ProcessControl_hook(uintptr_t thiz)
{
    dwCurPlayerActor = reinterpret_cast<uintptr_t>(thiz);
    byteCurPlayer = FindPlayerNumFromPedPtr(reinterpret_cast<CPedGTA *>(dwCurPlayerActor));
	RestoreLocalPedCollisionWhenOnFoot(reinterpret_cast<CPedGTA *>(dwCurPlayerActor));

    if (dwCurPlayerActor && (byteCurPlayer != 0))
    {
        // REMOTE PLAYER
        uint16_t byteSavedCameraMode;
        byteSavedCameraMode = *pbyteCameraMode;
        *pbyteCameraMode = GameGetPlayerCameraMode(byteCurPlayer);

        // aim switching
        GameStoreLocalPlayerAim();
        GameSetRemotePlayerAim(byteCurPlayer);

        GameStoreLocalPlayerCameraExtZoomAndAspect();
        GameSetRemotePlayerCameraExtZoomAndAspect(byteCurPlayer);


        uint16_t wSavedCameraMode2 = *wCameraMode2;
        *wCameraMode2 = GameGetPlayerCameraMode(byteCurPlayer);
        if (*wCameraMode2 == 4)
            *wCameraMode2 = 0;

        GTASAEngineApi::PatchRemotePedUpdatePosition(true);

        CWorld::PlayerInFocus = byteCurPlayer;
        // call original

        CPed__ProcessControl(thiz);

        // restore
        GTASAEngineApi::PatchRemotePedUpdatePosition(false);

        CWorld::PlayerInFocus = 0;
        *pbyteCameraMode = byteSavedCameraMode;

        GameSetLocalPlayerCameraExtZoomAndAspect();
        GameSetLocalPlayerAim();
        *wCameraMode2 = wSavedCameraMode2;
    }
    else
    {
        CPed__ProcessControl(thiz);
    }
}

uint32_t TaskUseGun(uintptr_t thiz, uintptr_t ped)
{
    dwCurPlayerActor = ped;
    byteCurPlayer = FindPlayerNumFromPedPtr(reinterpret_cast<CPedGTA *>(dwCurPlayerActor));

    uint32_t result = 0;

    if (dwCurPlayerActor &&
        (byteCurPlayer != 0)) // not local player and local player's keys set.
    {
        uint16_t byteSavedCameraMode = *pbyteCameraMode;
        *pbyteCameraMode = GameGetPlayerCameraMode(byteCurPlayer);

        uint16_t wSavedCameraMode2 = *wCameraMode2;
        *wCameraMode2 = GameGetPlayerCameraMode(byteCurPlayer);
        if (*wCameraMode2 == 4)* wCameraMode2 = 0;

        // save the camera zoom factor, apply the context
        GameStoreLocalPlayerCameraExtZoomAndAspect();
        GameSetRemotePlayerCameraExtZoomAndAspect(byteCurPlayer);

        // aim switching
        GameStoreLocalPlayerAim();
        GameSetRemotePlayerAim(byteCurPlayer);
        CWorld::PlayerInFocus = byteCurPlayer;

        result = GTASAEngineApi::CallTaskUseGun(thiz, ped);

        // restore the camera modes, internal id and local player's aim
        *pbyteCameraMode = byteSavedCameraMode;

        // remote the local player's camera zoom factor
        GameSetLocalPlayerCameraExtZoomAndAspect();

        CWorld::PlayerInFocus = 0;
        GameSetLocalPlayerAim();
        *wCameraMode2 = wSavedCameraMode2;
    }
    else
    {
        result = GTASAEngineApi::CallTaskUseGun(thiz, ped);
    }

    return result;
}

uint32_t CPad__TaskProcess(uintptr_t thiz, uintptr_t ped, int unk, int unk1)
{
    dwCurPlayerActor = ped;
    byteCurPlayer = FindPlayerNumFromPedPtr(reinterpret_cast<CPedGTA *>(dwCurPlayerActor));
    uint8_t old = CWorld::PlayerInFocus;
    CWorld::PlayerInFocus = byteCurPlayer;

    uint32_t result = GTASAEngineApi::CallPadTaskProcess(thiz, ped, unk, unk1);
    CWorld::PlayerInFocus = old;
    return result;
}

void HookCPad()
{
	InputApi::ResetCompatibilityPadState();

    GTASAEngineApi::InstallPedProcessControlHook(&CPed__ProcessControl_hook, &CPed__ProcessControl);
    GTASAEngineApi::InstallVehicleProcessControlHooks(&AllVehicles__ProcessControl_hook);
    GTASAEngineApi::InstallTaskUseGunHook(&TaskUseGun);
    GTASAEngineApi::InstallPadTaskProcessHook(&CPad__TaskProcess);

    const GTASAEngineApi::NativeInlineHook padInputHooks[] = {
        {"_ZN4CPad19GetPedWalkLeftRightEv", reinterpret_cast<uintptr_t>(&CPad__GetPedWalkLeftRight_hook), reinterpret_cast<void**>(&CPad__GetPedWalkLeftRight)},
        {"_ZN4CPad16GetPedWalkUpDownEv", reinterpret_cast<uintptr_t>(&CPad__GetPedWalkUpDown_hook), reinterpret_cast<void**>(&CPad__GetPedWalkUpDown)},
        {"_ZN4CPad9GetSprintEi", reinterpret_cast<uintptr_t>(&CPad__GetSprint_hook), reinterpret_cast<void**>(&CPad__GetSprint)},
        {"_ZN4CPad12JumpJustDownEv", reinterpret_cast<uintptr_t>(&CPad__JumpJustDown_hook), reinterpret_cast<void**>(&CPad__JumpJustDown)},
        {"_ZN4CPad7GetJumpEv", reinterpret_cast<uintptr_t>(&CPad__GetJump_hook), reinterpret_cast<void**>(&CPad__GetJump)},
        {"_ZN4CPad18GetCarGunLeftRightEbb", reinterpret_cast<uintptr_t>(&CPad__GetCarGunLeftRight_hook), reinterpret_cast<void**>(&CPad__GetCarGunLeftRight)},
        {"_ZN4CPad15GetCarGunUpDownEbP11CAutomobilefb", reinterpret_cast<uintptr_t>(&CPad__GetCarGunUpDown_hook), reinterpret_cast<void**>(&CPad__GetCarGunUpDown)},
        {"_ZN4CPad14GetCarGunFiredEbb", reinterpret_cast<uintptr_t>(&CPad__GetCarGunFired_hook), reinterpret_cast<void**>(&CPad__GetCarGunFired)},
        {"_ZN4CPad12GetAutoClimbEv", reinterpret_cast<uintptr_t>(&CPad__GetAutoClimb_hook), reinterpret_cast<void**>(&CPad__GetAutoClimb)},
        {"_ZN4CPad13GetAbortClimbEv", reinterpret_cast<uintptr_t>(&CPad__GetAbortClimb_hook), reinterpret_cast<void**>(&CPad__GetAbortClimb)},
        {"_ZN4CPad12DiveJustDownEv", reinterpret_cast<uintptr_t>(&CPad__DiveJustDown_hook), reinterpret_cast<void**>(&CPad__DiveJustDown)},
        {"_ZN4CPad16SwimJumpJustDownEv", reinterpret_cast<uintptr_t>(&CPad__SwimJumpJustDown_hook), reinterpret_cast<void**>(&CPad__SwimJumpJustDown)},
        {"_ZN4CPad19MeleeAttackJustDownEv", reinterpret_cast<uintptr_t>(&CPad__MeleeAttackJustDown_hook), reinterpret_cast<void**>(&CPad__MeleeAttackJustDown)},
        {"_ZN4CPad12DuckJustDownEP4CPed", reinterpret_cast<uintptr_t>(&CPad__DuckJustDown_hook), reinterpret_cast<void**>(&CPad__DuckJustDown)},
        {"_ZN4CPad8GetBlockEv", reinterpret_cast<uintptr_t>(&CPad__GetBlock_hook), reinterpret_cast<void**>(&CPad__GetBlock)},
        {"_ZN4CPad20GetSteeringLeftRightEv", reinterpret_cast<uintptr_t>(&CPad__GetSteeringLeftRight_hook), reinterpret_cast<void**>(&CPad__GetSteeringLeftRight)},
        {"_ZN4CPad17GetSteeringUpDownEv", reinterpret_cast<uintptr_t>(&CPad__GetSteeringUpDown_hook), reinterpret_cast<void**>(&CPad__GetSteeringUpDown)},
        {"_ZN4CPad13GetAccelerateEv", reinterpret_cast<uintptr_t>(&CPad__GetAccelerate_hook), reinterpret_cast<void**>(&CPad__GetAccelerate)},
        {"_ZN4CPad8GetBrakeEv", reinterpret_cast<uintptr_t>(&CPad__GetBrake_hook), reinterpret_cast<void**>(&CPad__GetBrake)},
        {"_ZN4CPad12GetHandBrakeEv", reinterpret_cast<uintptr_t>(&CPad__GetHandBrake_hook), reinterpret_cast<void**>(&CPad__GetHandBrake)},
        {"_ZN4CPad7GetHornEb", reinterpret_cast<uintptr_t>(&CPad__GetHorn_hook), reinterpret_cast<void**>(&CPad__GetHorn)},
        {"_ZN4CPad17GetEnterTargetingEv", reinterpret_cast<uintptr_t>(&CPad__GetEnterTargeting_hook), reinterpret_cast<void**>(&CPad__GetEnterTargeting)},
        {"_ZN4CPad9GetWeaponEP4CPedb", reinterpret_cast<uintptr_t>(&CPad__GetWeapon_hook), reinterpret_cast<void**>(&CPad__GetWeapon)},
        {"_ZN7CCamera17IsTargetingActiveEP10CPlayerPed", reinterpret_cast<uintptr_t>(&CCamera_IsTargetingActive_hook), reinterpret_cast<void**>(&CCamera_IsTargetingActive)},
        {"_ZN4CPad24CycleWeaponRightJustDownEv", reinterpret_cast<uintptr_t>(&CPad__CycleWeaponRightJustDown_hook), reinterpret_cast<void**>(&CPad__CycleWeaponRightJustDown)},
        {"_ZN4CPad13GetNitroFiredEv", reinterpret_cast<uintptr_t>(&CPad__GetNitroFired_hook), reinterpret_cast<void**>(&CPad__GetNitroFired)},
        {"_ZN4CPad13GetTurretLeftEv", reinterpret_cast<uintptr_t>(&CPad__GetTurretLeft_hook), reinterpret_cast<void**>(&CPad__GetTurretLeft)},
        {"_ZN4CPad14GetTurretRightEv", reinterpret_cast<uintptr_t>(&CPad__GetTurretRight_hook), reinterpret_cast<void**>(&CPad__GetTurretRight)},
    };
    GTASAEngineApi::InstallPadInputHooksNative(
        padInputHooks,
        sizeof(padInputHooks) / sizeof(padInputHooks[0]));
    //����ҧ������ͧ�ѹ��ѧ

    // WEAPON

    // nitro


}
