#include "../main.h"
#include "../game/game.h"
#include "../game/SampInputApi.h"
#include "../game/SampStreamingApi.h"
#include "../game/World.h"
#include "../game/Camera.h"
#include "../game/GTASAEngineApi.h"
#include "../game/Models/ModelInfo.h"
#include "netgame.h"
#include "localplayer.h"
#include "nettransport.h"
#include "../gui/gui.h"
#include "../java/jniutil.h"
#include <cmath>

// voice
#include "../voice_new/MicroIcon.h"
#include "../voice_new/SpeakerList.h"
#include "game/Tasks/TaskTypes/TaskComplexEnterCarAsDriver.h"

extern UI* pUI;
extern CGame *pGame;
extern CNetGame *pNetGame;
extern CJavaWrapper *pJavaWrapper;
extern "C" void ApplyVehicleSelfTestInputKeys();
#if !VER_x32
extern "C" void RequestArm64WorldCollisionForPosition(const CVector& pos, uint8_t area, bool immediate);
extern "C" void XyronRememberLocalVehicleForRender64(CVehicleGTA* vehicle);
#endif

extern int iNetModeNormalOnFootSendRate;
extern int iNetModeNormalInCarSendRate;
extern int iNetModeFiringSendRate;
extern int iNetModeSendMultiplier;
extern bool bIsShowPassengerButt;

bool m_bWasInCar = false;

extern bool bUsedPlayerSlots[];

uint32_t dwEnterVehTimeElasped = -1;
bool bFirstSpawn = true;
bool g_bLockEnterVehicleWidget = false;
bool bIsShowPassengerButt = false;
bool bIsShowLockButt = false;
uint32_t count = 0;

namespace {
bool IsNativeDrivenVehicleCameraMode(uint32_t mode);

void EnsureLocalPedWorldCollision(CPlayerPed* playerPed)
{
	if (!playerPed || !playerPed->m_pPed) {
		return;
	}

	CPedGTA* ped = playerPed->m_pPed;
	const bool repaired = !ped->m_bUsesCollision
		|| !ped->physicalFlags.bCollidable
		|| !ped->physicalFlags.bCanBeCollidedWith
		|| ped->physicalFlags.bDisableSimpleCollision
		|| ped->physicalFlags.bSkipLineCol;

	ped->m_bUsesCollision = true;
	ped->physicalFlags.bCollidable = true;
	ped->physicalFlags.bCanBeCollidedWith = true;
	ped->physicalFlags.bDisableSimpleCollision = false;
	ped->physicalFlags.bSkipLineCol = false;
	ped->physicalFlags.bApplyGravity = true;
	(void)repaired;
}

bool IsFiniteGamePosition64(const CVector& pos)
{
	return std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z);
}

bool IsZeroishGamePosition64(const CVector& pos)
{
	return std::fabs(pos.x) < 0.025f &&
		std::fabs(pos.y) < 0.025f &&
		std::fabs(pos.z) < 0.025f;
}

bool IsReliableGamePosition64(const CVector& pos)
{
	return IsFiniteGamePosition64(pos) &&
		!IsZeroishGamePosition64(pos) &&
		pos.z > -1000.0f &&
		pos.z < 3000.0f;
}

CVector ResolveReliableLocalSyncPosition64(const CVector& primary,
                                           const CVector& secondary,
                                           const char* tag)
{
	static bool s_hasLastGoodPosition = false;
	static CVector s_lastGoodPosition{};
	static uint32_t s_lastGoodPositionTick = 0;
	static uint32_t s_lastPositionRepairLogTick = 0;

	const uint32_t now = GetTickCount();
	if (IsReliableGamePosition64(primary)) {
		s_hasLastGoodPosition = true;
		s_lastGoodPosition = primary;
		s_lastGoodPositionTick = now;
		return primary;
	}

	if (IsReliableGamePosition64(secondary)) {
		s_hasLastGoodPosition = true;
		s_lastGoodPosition = secondary;
		s_lastGoodPositionTick = now;
		if (now - s_lastPositionRepairLogTick > 1000) {
			s_lastPositionRepairLogTick = now;
			FLog("[POS_FIX64] tag=%s primary=%.3f %.3f %.3f secondary=%.3f %.3f %.3f source=secondary",
				 tag ? tag : "?",
				 primary.x,
				 primary.y,
				 primary.z,
				 secondary.x,
				 secondary.y,
				 secondary.z);
		}
		return secondary;
	}

	if (s_hasLastGoodPosition && now - s_lastGoodPositionTick < 15000) {
		if (now - s_lastPositionRepairLogTick > 1000) {
			s_lastPositionRepairLogTick = now;
			FLog("[POS_FIX64] tag=%s primary=%.3f %.3f %.3f secondary=%.3f %.3f %.3f source=last last=%.3f %.3f %.3f age=%u",
				 tag ? tag : "?",
				 primary.x,
				 primary.y,
				 primary.z,
				 secondary.x,
				 secondary.y,
				 secondary.z,
				 s_lastGoodPosition.x,
				 s_lastGoodPosition.y,
				 s_lastGoodPosition.z,
				 now - s_lastGoodPositionTick);
		}
		return s_lastGoodPosition;
	}

	return primary;
}

CVector GetReliablePedSyncPosition64(CPlayerPed* playerPed, const char* tag)
{
	if (!playerPed || !playerPed->m_pPed) {
		return CVector(0.0f, 0.0f, 0.0f);
	}

	CPedGTA* ped = playerPed->m_pPed;
	const CVector matrixPos = ped->m_matrix ? ped->m_matrix->GetPosition() : ped->GetPosition();
	CVector fallbackPos = ped->GetPosition();
	if (!IsReliableGamePosition64(fallbackPos) && playerPed->IsInVehicle()) {
		CVehicleGTA* vehicle = playerPed->GetGtaVehicle();
		if (vehicle) {
			fallbackPos = vehicle->m_matrix ? vehicle->m_matrix->GetPosition() : vehicle->GetPosition();
		}
	}
	return ResolveReliableLocalSyncPosition64(matrixPos, fallbackPos, tag);
}

CVector GetReliableVehicleSyncPosition64(CVehicleGTA* vehicle, CPlayerPed* playerPed, const char* tag)
{
	if (!vehicle) {
		return GetReliablePedSyncPosition64(playerPed, tag);
	}

	const CVector matrixPos = vehicle->m_matrix ? vehicle->m_matrix->GetPosition() : vehicle->GetPosition();
	const CVector fallbackPos = playerPed && playerPed->m_pPed ? playerPed->m_pPed->GetPosition() : vehicle->GetPosition();
	return ResolveReliableLocalSyncPosition64(matrixPos, fallbackPos, tag);
}

bool BuildAimSyncData(CPlayerPed* playerPed, AIM_SYNC_DATA* aimSync)
{
	if (!playerPed || !aimSync) {
		return false;
	}

	CAMERA_AIM* caAim = playerPed->GetCurrentAim();
	CWeapon* weapon = playerPed->GetCurrentWeaponSlot();
	if (!caAim || !weapon) {
		return false;
	}

	*aimSync = {};
	aimSync->byteCamMode = playerPed->GetCameraMode();
	aimSync->vecAimf.x = caAim->f1x;
	aimSync->vecAimf.y = caAim->f1y;
	aimSync->vecAimf.z = caAim->f1z;
	aimSync->vecAimPos.x = caAim->pos1x;
	aimSync->vecAimPos.y = caAim->pos1y;
	aimSync->vecAimPos.z = caAim->pos1z;
	aimSync->fAimZ = playerPed->GetAimZ();
	aimSync->aspect_ratio = GameGetAspectRatio() * 255.0;
	aimSync->byteCamExtZoom = static_cast<unsigned char>(playerPed->GetCameraExtendedZoom() * 63.0f) & 63;
	aimSync->byteWeaponState = weapon->dwState == 2
		? WEAPONSTATE_RELOADING
		: ((weapon->dwAmmoInClip > 1) ? WEAPONSTATE_FIRING : weapon->dwAmmoInClip);
	return true;
}

void RestoreLocalCameraAfterVehicleExit(CPlayerPed* playerPed, uint32_t now)
{
	if (!playerPed || !playerPed->m_pPed || playerPed->IsInVehicle()) {
		return;
	}

	CCamera& camera = GTASAEngineApi::Camera();
	uint32_t modeBefore = 0xFFFFFFFF;
	uintptr_t* targetBefore = camera.pTargetEntity;
	uintptr_t* attachedBefore = camera.pAttachedEntity;
	if (camera.m_nActiveCam < 3) {
		modeBefore = camera.GetActiveCamera().m_nMode;
	}

	const bool targetIsPed = targetBefore == reinterpret_cast<uintptr_t*>(playerPed->m_pPed);
	const bool attachedToSomething = attachedBefore != nullptr;
	const bool alreadyFollowPed =
		modeBefore == MODE_FOLLOWPED &&
		targetIsPed &&
		!attachedToSomething &&
		!camera.m_bPersistCamPos &&
		!camera.m_bPersistCamLookAt;
	if (alreadyFollowPed && !IsNativeDrivenVehicleCameraMode(modeBefore)) {
		return;
	}

	camera.Restore();
	camera.m_bLookingAtVector = false;
	camera.m_bPersistCamPos = false;
	camera.m_bPersistCamLookAt = false;
	camera.pAttachedEntity = nullptr;
	camera.TakeControl(playerPed->m_pPed, MODE_FOLLOWPED, eSwitchType::JUMPCUT, 0);
	playerPed->SetCameraMode(static_cast<uint8_t>(MODE_FOLLOWPED));
	CCamera::SetBehindPlayer();

	static uint32_t s_lastExitCameraLog = 0;
	if (now - s_lastExitCameraLog > 500) {
		s_lastExitCameraLog = now;
		const CVector pos = playerPed->m_pPed->GetPosition();
		FLog("[CAM_EXIT64] restore mode=%u ctrl=%d target=%p attached=%p ped=%p pos=%.2f %.2f %.2f",
			 modeBefore,
			 camera.WhoIsInControlOfTheCamera,
			 targetBefore,
			 attachedBefore,
			 playerPed->m_pPed,
			 pos.x,
			 pos.y,
			 pos.z);
	}
}

bool IsSampVehicleAliveForExitRepair(CVehicleGTA* vehicle, VEHICLEID* outVehicleId = nullptr)
{
	if (outVehicleId) {
		*outVehicleId = INVALID_VEHICLE_ID;
	}
	if (!vehicle || !pNetGame) {
		return false;
	}

	CVehiclePool* vehiclePool = pNetGame->GetVehiclePool();
	if (!vehiclePool) {
		return false;
	}

	const VEHICLEID vehicleId = vehiclePool->FindIDFromGtaPtr(vehicle);
	if (vehicleId == INVALID_VEHICLE_ID) {
		return false;
	}

	CVehicle* sampVehicle = vehiclePool->GetAt(vehicleId);
	if (!sampVehicle || sampVehicle->m_pVehicle != vehicle) {
		return false;
	}

	if (outVehicleId) {
		*outVehicleId = vehicleId;
	}
	return true;
}

bool RepairVehicleAfterLocalExit(CVehicleGTA* vehicle, CPlayerPed* playerPed, uint32_t now)
{
	VEHICLEID vehicleId = INVALID_VEHICLE_ID;
	if (!IsSampVehicleAliveForExitRepair(vehicle, &vehicleId)) {
		return false;
	}

	CPedGTA* localPed = playerPed ? playerPed->m_pPed : nullptr;
	const uint32_t oldFlags = vehicle->m_nFlags;
	const uint32_t oldPhysFlags = vehicle->m_nPhysicalFlags;
	const uint32_t oldUpperFlags = vehicle->m_nVehicleUpperFlags;
	const uint32_t oldLowerFlags = vehicle->m_nVehicleLowerFlags;
	const eEntityStatus oldStatus = vehicle->GetStatus();
	const bool oldVisible = vehicle->m_bIsVisible;
	const bool oldUsesCollision = vehicle->m_bUsesCollision;
	const bool oldRemoveFromWorld = vehicle->m_bRemoveFromWorld;
	const bool oldHasRw = vehicle->m_pRwObject != nullptr;

	if (localPed && !playerPed->IsInVehicle()) {
		if (vehicle->pDriver == localPed) {
			vehicle->pDriver = nullptr;
		}
		for (CPedGTA*& passenger : vehicle->m_apPassengers) {
			if (passenger == localPed) {
				passenger = nullptr;
				if (vehicle->m_nNumPassengers > 0) {
					--vehicle->m_nNumPassengers;
				}
			}
		}
		if (vehicle->m_pEntityIgnoredCollision == localPed) {
			vehicle->m_pEntityIgnoredCollision = nullptr;
		}
		localPed->m_pEntityIgnoredCollision = nullptr;
		localPed->m_bUsesCollision = true;
		localPed->physicalFlags.bCollidable = true;
		localPed->physicalFlags.bCanBeCollidedWith = true;
		localPed->physicalFlags.bDisableSimpleCollision = false;
		localPed->physicalFlags.bSkipLineCol = false;
	}

	if (localPed) {
		vehicle->SetInterior(static_cast<int>(localPed->m_nAreaCode), false);
	}
	vehicle->m_pEntityIgnoredCollision = nullptr;
	vehicle->m_bUsesCollision = true;
	vehicle->physicalFlags.bCollidable = true;
	vehicle->physicalFlags.bCanBeCollidedWith = true;
	vehicle->physicalFlags.bDisableSimpleCollision = false;
	vehicle->physicalFlags.bSkipLineCol = false;
	vehicle->physicalFlags.bApplyGravity = true;
	vehicle->physicalFlags.bDisableCollisionForce = false;
	vehicle->physicalFlags.bDisableMoveForce = false;
	vehicle->physicalFlags.bDontApplySpeed = false;
	vehicle->m_bIsVisible = true;
	vehicle->m_bRemoveFromWorld = false;
	vehicle->m_bDistanceFade = false;
	vehicle->m_nVehicleFlags.bFadeOut = false;
	vehicle->m_nVehicleFlags.bIsBeingCarJacked = false;
	vehicle->m_nVehicleFlags.bDriverLastFrame = false;
	vehicle->m_nVehicleFlags.bNeverUseSmallerRemovalRange = true;
	vehicle->m_nNumGettingIn = 0;
	vehicle->m_nGettingInFlags = 0;
	vehicle->m_nGettingOutFlags = 0;
	if (!vehicle->pDriver && vehicle->GetStatus() == STATUS_PLAYER) {
		vehicle->SetStatus(STATUS_ABANDONED);
	}

	const int modelId = static_cast<int>(vehicle->m_nModelIndex);
	if (!vehicle->m_pRwObject && modelId >= 0 && modelId < CModelInfo::NUM_MODEL_INFOS) {
		if (Xyron::Streaming::IsModelLoaded(modelId)) {
			vehicle->CreateRwObject();
		} else {
			Xyron::Streaming::RequestModel(modelId, STREAMING_DONTREMOVE_IN_LOADSCENE);
		}
	}
	if (vehicle->m_pRwObject) {
		vehicle->UpdateRW();
		vehicle->UpdateRwFrame();
	}
#if !VER_x32
	XyronRememberLocalVehicleForRender64(vehicle);
#endif

	static uint32_t s_lastExitVehicleRepairLog = 0;
	const bool changed = oldFlags != vehicle->m_nFlags ||
		oldPhysFlags != vehicle->m_nPhysicalFlags ||
		oldUpperFlags != vehicle->m_nVehicleUpperFlags ||
		oldLowerFlags != vehicle->m_nVehicleLowerFlags ||
		oldStatus != vehicle->GetStatus() ||
		oldVisible != vehicle->m_bIsVisible ||
		oldUsesCollision != vehicle->m_bUsesCollision ||
		oldRemoveFromWorld != vehicle->m_bRemoveFromWorld ||
		oldHasRw != (vehicle->m_pRwObject != nullptr);
	if (changed || now - s_lastExitVehicleRepairLog > 700) {
		s_lastExitVehicleRepairLog = now;
		const CVector pos = vehicle->GetPosition();
		FLog("[VEH_EXIT_REPAIR64] samp=%u model=%u changed=%u rw=%p vis=%u use=%u phys=0x%08x status=%u ignored=%p driver=%p pos=%.2f %.2f %.2f",
			 vehicleId,
			 vehicle->GetModelId(),
			 changed ? 1 : 0,
			 vehicle->m_pRwObject,
			 vehicle->m_bIsVisible ? 1 : 0,
			 vehicle->m_bUsesCollision ? 1 : 0,
			 vehicle->m_nPhysicalFlags,
			 static_cast<unsigned>(vehicle->GetStatus()),
			 vehicle->m_pEntityIgnoredCollision,
			 vehicle->pDriver,
			 pos.x,
			 pos.y,
			 pos.z);
	}

	return true;
}

bool IsNativeDrivenVehicleCameraMode(uint32_t mode)
{
	return mode == MODE_BEHINDCAR ||
		mode == MODE_CAM_ON_A_STRING ||
		mode == MODE_BEHINDBOAT ||
		mode == MODE_CAM_ON_TRAIN_ROOF ||
		mode == MODE_DW_HELI_CHASE;
}

void RestoreLocalCameraForDrivenVehicle(CPlayerPed* playerPed, uint32_t now)
{
	if (!playerPed || !playerPed->m_pPed || !playerPed->IsInVehicle() || playerPed->IsAPassenger()) {
		return;
	}

	CVehicleGTA* vehicle = playerPed->GetGtaVehicle();
	if (!vehicle) {
		return;
	}

	CCamera& camera = GTASAEngineApi::Camera();
	uint32_t modeBefore = 0xFFFFFFFF;
	uintptr_t* targetBefore = camera.pTargetEntity;
	if (camera.m_nActiveCam < 3) {
		modeBefore = camera.GetActiveCamera().m_nMode;
	}

	if (IsNativeDrivenVehicleCameraMode(modeBefore) &&
		targetBefore == reinterpret_cast<uintptr_t*>(vehicle) &&
		camera.WhoIsInControlOfTheCamera == 0) {
		return;
	}

	camera.Restore();
	playerPed->SetCameraMode(static_cast<uint8_t>(MODE_BEHINDCAR));
	CCamera::SetBehindPlayer();

	static uint32_t s_lastVehicleCameraLog = 0;
	if (now - s_lastVehicleCameraLog > 500) {
		s_lastVehicleCameraLog = now;
		const CVector pos = vehicle->GetPosition();
		FLog("[CAM_VEH64] restore model=%u mode=%u ctrl=%d target=%p vehicle=%p pos=%.2f %.2f %.2f",
			 vehicle->GetModelId(),
			 modeBefore,
			 camera.WhoIsInControlOfTheCamera,
			 targetBefore,
			 vehicle,
			 pos.x,
			 pos.y,
			 pos.z);
	}
}

void MaintainLocalVehicleExitCamera(CPlayerPed* playerPed, uint32_t now)
{
	static bool s_wasInVehicle = false;
	static CVehicleGTA* s_lastDrivenVehicle = nullptr;
	static uint32_t s_repairUntilTick = 0;
	static uint32_t s_lastRepairTick = 0;
	static uint32_t s_vehicleRepairUntilTick = 0;
	static uint32_t s_lastVehicleRepairTick = 0;

	const bool inVehicle = playerPed && playerPed->IsInVehicle();
	if (inVehicle) {
		CVehicleGTA* currentVehicle = playerPed->GetGtaVehicle();
		if (IsSampVehicleAliveForExitRepair(currentVehicle)) {
			s_lastDrivenVehicle = currentVehicle;
		}
		if (!s_wasInVehicle) {
			s_vehicleRepairUntilTick = now + 1800;
			s_lastVehicleRepairTick = 0;
		}
		s_wasInVehicle = true;
		s_repairUntilTick = 0;
		if (s_vehicleRepairUntilTick && now <= s_vehicleRepairUntilTick &&
			now - s_lastVehicleRepairTick >= 250) {
			s_lastVehicleRepairTick = now;
			RestoreLocalCameraForDrivenVehicle(playerPed, now);
		} else if (s_vehicleRepairUntilTick && now > s_vehicleRepairUntilTick) {
			s_vehicleRepairUntilTick = 0;
		}
		return;
	}

	if (s_wasInVehicle) {
		s_wasInVehicle = false;
		s_repairUntilTick = now + 3500;
		s_lastRepairTick = 0;
	}

	if (s_repairUntilTick && now <= s_repairUntilTick && now - s_lastRepairTick >= 160) {
		s_lastRepairTick = now;
		RepairVehicleAfterLocalExit(s_lastDrivenVehicle, playerPed, now);
		RestoreLocalCameraAfterVehicleExit(playerPed, now);
	} else if (s_repairUntilTick && now > s_repairUntilTick) {
		s_repairUntilTick = 0;
	}
}

#if !VER_x32
bool ProbeSpawnGround64(const CVector& pos, float* outGroundZ)
{
	const GTASAEngineApi::GroundProbeResult probe = GTASAEngineApi::ProbeGround(pos, 80.0f, 140.0f);
	if (probe.hit && probe.entityUsesCollision) {
		if (outGroundZ) {
			*outGroundZ = probe.point.z;
		}
		return true;
	}

	return false;
}

void WarmupSpawnWorld64(CPlayerPed* playerPed, const CVector& spawnPos)
{
	if (!pGame) {
		return;
	}

	const uint32_t startTick = GetTickCount();
	const int32 rawAreaCode = playerPed && playerPed->m_pPed ? playerPed->m_pPed->m_nAreaCode : CGame::currArea;
	const int32 areaCode = Xyron::Streaming::ResolveEffectiveWorldAreaCode(spawnPos, rawAreaCode);
	int sampleCount = 0;
	float beforeGroundZ = 0.0f;
	const bool beforeHit = ProbeSpawnGround64(spawnPos, &beforeGroundZ);

	Xyron::Streaming::ForceStreamingEnabled("spawn-warmup64");

	auto requestAt = [&](const CVector& sample, bool loadScene) {
		++sampleCount;
		Xyron::Streaming::WorldStreamRequest request{};
		request.areaCode = areaCode;
		request.streamingFlags = STREAMING_DEFAULT;
		request.loadScene = loadScene;
		request.refreshGame = true;
		request.reason = "spawn-warmup64";
		Xyron::Streaming::PrepareWorldAtPoint(sample, request);
	};

	auto requestCross = [&](float radius, bool loadScene) {
		const float diag = radius * 0.70710678f;
		requestAt(spawnPos, loadScene);
		requestAt(CVector(spawnPos.x + radius, spawnPos.y, spawnPos.z), false);
		requestAt(CVector(spawnPos.x - radius, spawnPos.y, spawnPos.z), false);
		requestAt(CVector(spawnPos.x, spawnPos.y + radius, spawnPos.z), false);
		requestAt(CVector(spawnPos.x, spawnPos.y - radius, spawnPos.z), false);
		requestAt(CVector(spawnPos.x + diag, spawnPos.y + diag, spawnPos.z), false);
		requestAt(CVector(spawnPos.x + diag, spawnPos.y - diag, spawnPos.z), false);
		requestAt(CVector(spawnPos.x - diag, spawnPos.y + diag, spawnPos.z), false);
		requestAt(CVector(spawnPos.x - diag, spawnPos.y - diag, spawnPos.z), false);
	};

	const CVector zeroVelocity(0.0f, 0.0f, 0.0f);
	Xyron::Streaming::RequestSceneObjectsAroundPlayer(spawnPos, zeroVelocity);
	const float radii[] = { 12.0f, 24.0f, 42.0f, 64.0f };
	for (int pass = 0; pass < 4; ++pass) {
		requestCross(radii[pass], pass == 0);
		Xyron::Streaming::LoadAllRequestedModels(false);
	}
	RequestArm64WorldCollisionForPosition(
		spawnPos,
		areaCode < 0 ? static_cast<uint8_t>(AREA_CODE_NORMAL_WORLD) : static_cast<uint8_t>(areaCode),
		true);

	float afterGroundZ = 0.0f;
	const bool afterHit = ProbeSpawnGround64(spawnPos, &afterGroundZ);
	FLog("[SPAWN_WORLD64] warmup pos=%.2f %.2f %.2f area=%d samples=%d beforeHit=%d beforeZ=%.2f afterHit=%d afterZ=%.2f elapsed=%u streamingDisabled=%d",
	     spawnPos.x,
	     spawnPos.y,
	     spawnPos.z,
	     areaCode,
	     sampleCount,
	     beforeHit ? 1 : 0,
	     beforeGroundZ,
	     afterHit ? 1 : 0,
	     afterGroundZ,
	     GetTickCount() - startTick,
	     Xyron::Streaming::IsStreamingDisabled() ? 1 : 0);
}
#endif
}

bool IsLongTimeDead()
{
    if(pGame->FindPlayerPed()->IsDead()){
        if ((GetTickCount()-count) > 5000) {
            return true;
        }
    }
    return false;
}

CLocalPlayer::CLocalPlayer()
{
    FLog("CLocalPlayer::CLocalPlayer()");
	ResetAllSyncAttributes();

	m_bInRCMode = false;

	m_surfData.bIsActive = false;
	m_surfData.pSurfInst = 0;
	m_surfData.bIsVehicle = false;
	m_surfData.vecOffsetPos = CVector {0.0f, 0.0f, 0.0f};

	m_statsData.dwLastMoney = 0;
	m_statsData.dwLastDrunkLevel = 0;

	m_pPlayerPed = pGame->FindPlayerPed();
	m_bIsActive = false;
	m_bIsWasted = false;
	
	m_iDisplayZoneTick = 0;
	m_dwLastSendTick = GetTickCount();
	m_dwLastSendSpecTick = GetTickCount();
	m_dwLastSendSyncTick = GetTickCount();
    count = GetTickCount();
	m_dwLastUpdateInCarData = GetTickCount();
	m_dwLastSendAimSyncTick = GetTickCount();
	m_dwLastStatsUpdateTick = GetTickCount();
	m_dwLastPerformStuffAnimTick = GetTickCount();
	m_dwLastUpdateOnFootData = GetTickCount();
	m_dwLastUpdateHudButtons = GetTickCount();
	m_bIsSpectating = false;
	m_byteSpectateType = SPECTATE_TYPE_NONE;
	m_SpectateID = 0xFFFFFFFF;
	m_bSpawnDialogShowed = false;

	for (int i = 0; i < 13; i++)
	{
		m_byteLastWeapon[i] = 0;
		m_dwLastAmmo[i] = 0;
	}

	m_byteTeam = NO_TEAM;
}

CLocalPlayer::~CLocalPlayer()
{

}
extern int g_iLagCompensationMode;
extern bool DriveBy;

bool CLocalPlayer::Process()
{
	if (m_bIsActive && m_pPlayerPed != nullptr)
	{
		EnsureLocalPedWorldCollision(m_pPlayerPed);

		// local player is dead
		if (!m_bIsWasted && m_pPlayerPed->GetActionTrigger() == ACTION_DEATH || m_pPlayerPed->IsDead())
		{
			ToggleSpectating(false);
			if(m_pPlayerPed->GetDanceStyle() != -1) m_pPlayerPed->StopDancing();
			if(m_pPlayerPed->IsCellphoneEnabled()) m_pPlayerPed->ToggleCellphone(0);
			if(m_pPlayerPed->IsPissing()) m_pPlayerPed->StopPissing();
			if(m_pPlayerPed->GetStuff() != eStuffType::STUFF_TYPE_NONE) m_pPlayerPed->DropStuff();
			m_pPlayerPed->TogglePlayerControllable(true);
			if (m_bInRCMode)
			{
				m_bInRCMode = false;
				m_pPlayerPed->m_pPed->Add();
			}

			if (m_pPlayerPed->IsInVehicle() && !m_pPlayerPed->IsAPassenger())
			{
				SendInCarFullSyncData(); // for explosion
				m_LastVehicle = pNetGame->GetVehiclePool()->FindIDFromGtaPtr(m_pPlayerPed->GetGtaVehicle());
			}
			m_pPlayerPed->ExtinguishFire();
            m_pPlayerPed->SetHealth(0.0f);
            m_pPlayerPed->SetDead();
			SendWastedNotification();
			m_bIsActive = false;
			m_bIsWasted = true;
			pGame->EnableZoneNames(false);
			return true;
		}

		uint16_t wKeys, lrAnalog, udAnalog;
		wKeys = m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog, false);

		uint32_t dwThisTick = GetTickCount();
		MaintainLocalVehicleExitCamera(m_pPlayerPed, dwThisTick);
		const bool secondaryAttackDown = Xyron::Input::IsSourceSecondaryPressed();
		const bool fireDown = Xyron::Input::IsSourceFirePressed();

		if(m_pPlayerPed->GetDanceStyle() != -1) 
		{
			m_pPlayerPed->ProcessDancing();
			if(secondaryAttackDown || m_pPlayerPed->IsInVehicle()
				|| m_pPlayerPed->IsInJetpackMode())
			{ 
				m_pPlayerPed->StopDancing();
			}
		}

		// HANDLE I GOT MY HANDS UP BUT DON'T WANT TO ANYMORE
		if(m_pPlayerPed->HasHandsUp())
		{ 
			if(secondaryAttackDown || m_pPlayerPed->IsInVehicle()
				|| m_pPlayerPed->IsInJetpackMode())
			{ 
				m_pPlayerPed->TogglePlayerControllable(true);
			}
		}

		// HANDLE PHONE LOCAL PED
		if(m_pPlayerPed->IsCellphoneEnabled()) 
		{
			if(secondaryAttackDown || m_pPlayerPed->IsInVehicle()
				|| m_pPlayerPed->IsInJetpackMode())
			{ 
				m_pPlayerPed->ToggleCellphone(0);
			}
		}

		// HANDLE URINATING LOCAL PED
		if(m_pPlayerPed->IsPissing())
		{ 
			if(secondaryAttackDown || m_pPlayerPed->IsInVehicle()
				|| m_pPlayerPed->IsInJetpackMode())
			{ 
				m_pPlayerPed->StopPissing();
			} 
		}

		// HANDLE STUFF LOCAL PED
		if(m_pPlayerPed->GetStuff() != eStuffType::STUFF_TYPE_NONE)
		{
			if((dwThisTick - m_dwLastPerformStuffAnimTick) > 3500)
				m_bPerformingStuffAnim = false;

			if(!m_bPerformingStuffAnim && fireDown) 
			{
				if(m_pPlayerPed->ApplyStuff())
				{
					m_dwLastPerformStuffAnimTick = dwThisTick;
					m_bPerformingStuffAnim = true;
				}
			}

			if(secondaryAttackDown || m_pPlayerPed->IsInVehicle()
				|| m_pPlayerPed->IsInJetpackMode())
			{
				m_pPlayerPed->DropStuff();
			}
		}

		// HANDLE DRUNK
		m_pPlayerPed->ProcessDrunk();

		if (dwEnterVehTimeElasped != -1 &&
			(dwThisTick - dwEnterVehTimeElasped) > 5000 &&
			!m_pPlayerPed->IsInVehicle())
		{
			CCamera::SetBehindPlayer();
			dwEnterVehTimeElasped = -1;
		}
		if (dwThisTick >= m_iDisplayZoneTick) {
			pGame->EnableZoneNames(pNetGame->m_pNetSet->bZoneNames);
		}
		pGame->UpdateCheckpoints();
		if ((dwThisTick - m_dwLastStatsUpdateTick) > 1000) {
			SendStatsUpdate();
			m_dwLastStatsUpdateTick = dwThisTick;
		}
		UpdateSurfing();
		CheckWeapons();
		uint8_t byteInterior = pGame->GetActiveInterior();
		if (byteInterior != m_byteCurInterior) {
			UpdateRemoteInterior(byteInterior);
		}
		UpdateCameraTarget();
		// PLAYER DATA UPDATES
		if (m_bIsSpectating) {
			ProcessSpectating();
			m_bPassengerDriveByMode = false;
		}
		// DRIVER CONDITIONS
		else if (m_pPlayerPed->IsInVehicle() && !m_pPlayerPed->IsAPassenger())
		{
            MaybeSendExitVehicle();

			g_bLockEnterVehicleWidget = false;

            CVehicleGTA* pGtaVehicle = m_pPlayerPed->GetGtaVehicle();
			m_nLastVehicle = pNetGame->GetVehiclePool()->FindIDFromGtaPtr(pGtaVehicle);

			//m_pPlayerPed->RemoveWeaponWhenEnteringVehicle();
			MoveHeadWithCamera();
			ProcessInCarWorldBounds();

			if ((dwThisTick - m_dwLastSendAimSyncTick) > 500)
			{
				m_dwLastSendAimSyncTick = dwThisTick;
				SendAimSyncData();
			}

			CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
			CVehicle *pVehicle = nullptr;

			if (pVehiclePool) {
				m_CurrentVehicle = pVehiclePool->FindIDFromGtaPtr(m_pPlayerPed->GetGtaVehicle());
			}

			pVehicle = pVehiclePool->GetAt(m_CurrentVehicle);
			if (pVehicle && !m_bInRCMode && pVehicle->IsRCVehicle())
			{
				m_pPlayerPed->m_pPed->Remove();
				m_bInRCMode = true;
			}

			if (m_bInRCMode && !pVehicle)
			{
				m_pPlayerPed->SetHealth(0.0f);
				m_pPlayerPed->SetDead();
			}
			
			if (m_bInRCMode && pVehicle && pVehicle->GetHealth() == 0.0f)
			{
				m_pPlayerPed->SetHealth(0.0f);
				m_pPlayerPed->SetDead();
			}

			if ((dwThisTick - m_dwLastSendTick) > GetOptimumInCarSendRate())
			{
				m_dwLastSendTick = GetTickCount();
				SendInCarFullSyncData();
				UpdateVehicleDamage(m_CurrentVehicle);
			}

			m_bPassengerDriveByMode = false;
		}
		// ONFOOT CONDITIONS
		else if (m_pPlayerPed->GetActionTrigger() == ACTION_NORMAL || m_pPlayerPed->GetActionTrigger() == ACTION_SCOPE)
		{
			if ((dwThisTick - m_dwLastSendTick) > GetOptimumOnFootSendRate())
			{
				m_dwLastSendTick = GetTickCount();
                g_bLockEnterVehicleWidget = true;
                if(m_bWasInCar)
                {
                    m_bWasInCar = false;
                }

                MoveHeadWithCamera();

                if (m_bInRCMode)
                {
                    m_bInRCMode = false;
                    m_pPlayerPed->m_pPed->Add();
                }

                HandlePassengerEntry();
                ProcessOnFootWorldBounds();

                if (m_CurrentVehicle != 0xFFFF)
                {
                    m_LastVehicle = m_CurrentVehicle;
                    m_CurrentVehicle = 0xFFFF;
                }

                ProcessSurfing();
                MaybeSendEnterVehicle();
				SendOnFootFullSyncData();
			}

            if((dwThisTick - m_dwLastSendTick) < 1000)
            {
                if(IS_TARGETING(m_pPlayerPed->m_pPed) && IS_FIRING(m_pPlayerPed->m_pPed))
                {
                    if(g_iLagCompensationMode == 2)
                    {
                        if((dwThisTick - m_dwLastSendAimSyncTick) > iNetModeFiringSendRate)
                        {
                            SendAimSyncData();
                            m_dwLastSendAimSyncTick = dwThisTick;
                        }
                    }
                    else if((dwThisTick - m_dwLastSendAimSyncTick) > 500)
                    {
                        SendAimSyncData();
                        m_dwLastSendAimSyncTick = dwThisTick;
                    }
                }
                else
                {
                    if((dwThisTick - m_dwLastSendAimSyncTick) > 1000)
                    {
                        SendAimSyncData();
                        m_dwLastSendAimSyncTick = dwThisTick;
                    }
                }
            }

			m_bPassengerDriveByMode = false;
		}
		// PASSENGER CONDITIONS
		else if (m_pPlayerPed->GetActionTrigger() == ACTION_INCAR && m_pPlayerPed->IsAPassenger())
		{
            MaybeSendExitVehicle();
			g_bLockEnterVehicleWidget = false;

            CVehicleGTA* pGtaVehicle = m_pPlayerPed->GetGtaVehicle();
			m_nLastVehicle = pNetGame->GetVehiclePool()->FindIDFromGtaPtr(pGtaVehicle);

			MoveHeadWithCamera();

			if (m_bInRCMode)
			{
				m_bInRCMode = false;
				m_pPlayerPed->m_pPed->Add();
			}

			//GTA_CONTROLSET *controls = GameGetInternalKeys();

			if(!m_bPassengerDriveByMode &&
				Xyron::Input::IsPassengerDriveByInputPressed() &&
				m_pPlayerPed->CanStartPassengerDriveByMode()) {
				if(m_pPlayerPed->StartPassengerDriveByMode()) {
					m_bPassengerDriveByMode = true;
				}
			}

			if (dwThisTick - m_dwLastSendTick > GetOptimumInCarSendRate())
			{
				m_dwLastSendTick = GetTickCount();
				SendPassengerFullSyncData();
			}
		}

		if (pJavaWrapper && (dwThisTick - m_dwLastUpdateHudButtons) > 100)
		{
			m_dwLastUpdateHudButtons = GetTickCount();
			CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();

			auto hideHudVehicleButtons = []() {
				if (bIsShowPassengerButt) {
					pJavaWrapper->togglePassengerButton(false);
				}
				if (bIsShowLockButt) {
					pJavaWrapper->toggleLockButton(false);
				}
			};

			if (!m_pPlayerPed->lToggle || m_pPlayerPed->iSpecialAction == SPECIAL_ACTION_CARRY)
			{
				hideHudVehicleButtons();
			}
			else if (!m_pPlayerPed->IsInVehicle())
			{
				if (pVehiclePool)
				{
					VEHICLEID closestVehicleID = pVehiclePool->FindNearestToLocalPlayerPed();
					CVehicle* pVehicle = pVehiclePool->GetAt(closestVehicleID);
					if (pVehicle && pVehicle->m_pVehicle &&
						pVehicle->m_pVehicle->GetDistanceFromLocalPlayerPed() < 4.0f &&
						!pVehicle->IsATrailer())
					{
						if (!pVehicle->m_bIsLocked)
						{
							if (!bIsShowPassengerButt) {
								pJavaWrapper->togglePassengerButton(true);
							}
						}
						else if (bIsShowPassengerButt)
						{
							pJavaWrapper->togglePassengerButton(false);
						}

						if (!bIsShowLockButt) {
							pJavaWrapper->toggleLockButton(true);
						}
					}
					else
					{
						hideHudVehicleButtons();
					}
				}
			}
			else
			{
				if (!bIsShowPassengerButt) {
					pJavaWrapper->togglePassengerButton(true);
				}
				if (m_pPlayerPed->IsAPassenger()) {
					if (bIsShowLockButt) {
						pJavaWrapper->toggleLockButton(false);
					}
				}
				else if (!bIsShowLockButt) {
					pJavaWrapper->toggleLockButton(true);
				}
			}
		}
	}

	// HANDLE !IsActive spectating
	if (m_bIsSpectating && !m_bIsActive)
	{
		if (m_bSpawnDialogShowed)
		{
			m_bSpawnDialogShowed = false;
			if(pUI) pUI->spawn()->setVisible(false);
		}

		ProcessSpectating();
		return true;
	}

	// HANDLE NEEDS TO RESPAWN AFTER DEATH
	if (m_bIsWasted
		&& m_pPlayerPed->GetActionTrigger() != ACTION_WASTED
		&& m_pPlayerPed->GetActionTrigger() != ACTION_DEATH)
	{
        m_pPlayerPed->FlushAttach();

		//if (sub_100AC660(_this->m_pPlayerPed))
		//	sub_100AC670(_this->m_pPlayerPed, 0);

		if (IsClearedToSpawn() && pNetGame->GetGameState() == GAMESTATE_CONNECTED)
		{
			if (m_pPlayerPed->GetHealth() > 0.0f) {
				Spawn();
			}
		}
		else
		{
			m_bIsWasted = false;
			HandleClassSelection();
		}

		return true;
	}

	if (m_pPlayerPed->GetActionTrigger() != ACTION_WASTED &&
		m_pPlayerPed->GetActionTrigger() != ACTION_DEATH &&
		pNetGame->GetGameState() == GAMESTATE_CONNECTED &&
		!m_bIsActive &&
		!m_bIsSpectating)
	{
		ProcessClassSelection();
	}

	return true;
}

void CLocalPlayer::ProcessClassSelection()
{ 
	if (!m_bSpawnDialogShowed)
	{
		if (pUI) pUI->spawn()->setVisible(false);
		RequestClass(m_iSelectedClass);
		m_bSpawnDialogShowed = true;
		FLog("[SPAWN_FLOW64] auto-class-request class=%d", m_iSelectedClass);
	}
}

void CLocalPlayer::ResetAllSyncAttributes()
{
	m_bHasSpawnInfo = false;
	m_bWaitingForSpawnRequestReply = false;
	m_iSelectedClass = 0;
	m_byteCurInterior = 0;
	m_LastVehicle = 0xFFFF;
	m_bInRCMode = false;

	memset(&m_SpawnInfo, 0, sizeof(PLAYER_SPAWN_INFO));
	memset(&m_ofSync, 0, sizeof(ONFOOT_SYNC_DATA));
	memset(&m_icSync, 0, sizeof(INCAR_SYNC_DATA));
	memset(&m_TrailerData, 0, sizeof(TRAILER_SYNC_DATA));
	memset(&m_psSync, 0, sizeof(PASSENGER_SYNC_DATA));
	memset(&m_aimSync, 0, sizeof(AIM_SYNC_DATA));

	m_dwAnimation = 0;
	m_dwLastWeaponsUpdateTick = GetTickCount();
	m_byteCurrentWeapon = 0;

	/* voice */
	m_iVCState = VOICE_CHANNEL_STATE_CLOSED;
	m_dwVCOpenRequestTick = GetTickCount();

	m_CurrentVehicle = INVALID_VEHICLE_ID;
	m_LastVehicle = INVALID_VEHICLE_ID;
	m_nLastVehicle = INVALID_VEHICLE_ID;
	m_bWasInCar = false;
}
// 0.3.7 ( ศใํ๎๐ F4-class-reselect)
void CLocalPlayer::ToggleSpectating(bool bToggle)
{
	if (m_bIsSpectating && !bToggle) {
		Spawn();
	}

	m_bIsSpectating = bToggle;
	m_byteSpectateType = SPECTATE_TYPE_NONE;
	m_bSpectateProcessed = false;
	m_SpectateID = 0xFFFFFFFF;
}

void CLocalPlayer::ProcessSpectating()
{
	RakNet::BitStream bsSpectatorSync;
	SPECTATOR_SYNC_DATA spSync;
	RwMatrix matPos;

	uint16_t lrAnalog, udAnalog;
	uint16_t wKeys = m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog);
    CCamera& TheCamera = GTASAEngineApi::Camera();
    matPos = TheCamera.GetMatrix().ToRwMatrix();

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();

	if (!pPlayerPool || !pVehiclePool) return;

	spSync.vecPos.x = matPos.pos.x;
	spSync.vecPos.y = matPos.pos.y;
	spSync.vecPos.z = matPos.pos.z;
	spSync.lrAnalog = lrAnalog;
	spSync.udAnalog = udAnalog;
	spSync.wKeys = wKeys;

	if ((GetTickCount() - m_dwLastSendSpecTick) > GetOptimumOnFootSendRate())
	{
		m_dwLastSendSpecTick = GetTickCount();
		bsSpectatorSync.Write((uint8_t)ID_SPECTATOR_SYNC);
		bsSpectatorSync.Write((char*)&spSync, sizeof(SPECTATOR_SYNC_DATA));
		NetTransport::Send(pNetGame->GetRakClient(), &bsSpectatorSync, HIGH_PRIORITY, UNRELIABLE, 0, "spectator-sync");

		if ((GetTickCount() - m_dwLastSendAimSyncTick) > (GetOptimumOnFootSendRate() * 2))
		{
			m_dwLastSendAimSyncTick = GetTickCount();
			SendAimSyncData();
		}
	}

	pGame->DisplayHUD(false);

	m_pPlayerPed->SetHealth(100.0f);
	GetPlayerPed()->TeleportTo(spSync.vecPos.x, spSync.vecPos.y, spSync.vecPos.z + 20.0f);

	// handle spectate player left the server
	if (m_byteSpectateType == SPECTATE_TYPE_PLAYER &&
		!pPlayerPool->GetSlotState(m_SpectateID))
	{
		m_byteSpectateType = SPECTATE_TYPE_NONE;
		m_bSpectateProcessed = false;
	}

	// handle spectate player is no longer active (ie Died)
	if (m_byteSpectateType == SPECTATE_TYPE_PLAYER &&
		pPlayerPool->GetSlotState(m_SpectateID) &&
		(!pPlayerPool->GetAt(m_SpectateID)->IsActive() ||
			pPlayerPool->GetAt(m_SpectateID)->GetState() == PLAYER_STATE_WASTED))
	{
		m_byteSpectateType = SPECTATE_TYPE_NONE;
		m_bSpectateProcessed = false;
	}

	if (m_bSpectateProcessed) return;

	if (m_byteSpectateType == SPECTATE_TYPE_NONE)
	{
		GetPlayerPed()->RemoveFromVehicleAndPutAt(0.0f, 0.0f, 10.0f);
        CCamera::SetPosition(50.0f, 50.0f, 50.0f, 0.0f, 0.0f, 0.0f);
        CCamera::LookAtPoint(60.0f, 60.0f, 50.0f, 2);
		m_bSpectateProcessed = true;
	}
	else if (m_byteSpectateType == SPECTATE_TYPE_PLAYER)
	{
		uint32_t dwGTAId = 0;
		CPlayerPed* pPlayerPed = 0;

		if (pPlayerPool->GetSlotState(m_SpectateID))
		{
			pPlayerPed = pPlayerPool->GetAt(m_SpectateID)->GetPlayerPed();
			if (pPlayerPed)
			{
				dwGTAId = pPlayerPed->m_dwGTAId;
                CCamera& TheCamera = GTASAEngineApi::Camera();

                TheCamera.TakeControl(pPlayerPed->m_pPed, static_cast<eCamMode>(m_byteSpectateMode), eSwitchType::JUMPCUT, 1);
				//pGame->GetCamera()->AttachToEntity(m_pPlayerPed);
				m_bSpectateProcessed = true;
			}
		}
	}
	else if (m_byteSpectateType == SPECTATE_TYPE_VEHICLE)
	{
		CVehicle* pVehicle = nullptr;
		uint32_t dwGTAId = 0;
		
		pVehicle = pVehiclePool->GetAt((VEHICLEID)m_SpectateID);
		if (pVehicle)
		{
			dwGTAId = pVehicle->m_dwGTAId;
            CCamera &TheCamera = GTASAEngineApi::Camera();

            TheCamera.TakeControl(pVehicle->m_pVehicle, static_cast<eCamMode>(m_byteSpectateMode), eSwitchType::JUMPCUT, 1);
			//pGame->GetCamera()->AttachToEntity(pVehicle);
			m_bSpectateProcessed = true;
		}
	}
}

bool CLocalPlayer::Spawn()
{
	if (!m_bHasSpawnInfo) {
		return false;
	}

	if (m_bSpawnDialogShowed == true)
	{
		m_bSpawnDialogShowed = false;
		if (pUI) pUI->spawn()->setVisible(false);
	}
    CCamera& TheCamera = GTASAEngineApi::Camera();
    TheCamera.Restore();
    CCamera::SetBehindPlayer();

    CVector spawnPos = m_SpawnInfo.vecPos;
#if !VER_x32
	WarmupSpawnWorld64(m_pPlayerPed, spawnPos);
#else
	pGame->RefreshStreamingAt(spawnPos.x, spawnPos.y);
#endif

	if (!bFirstSpawn) {
		m_pPlayerPed->SetInitialState();
	}
	else {
		bFirstSpawn = false;
	}

	if (m_pPlayerPed->IsCuffed()) {
	//	m_pPlayerPed->ToggleCuffed(false);
	}

	m_pPlayerPed->RestartIfWastedAt(&spawnPos, m_SpawnInfo.fRotation);
	m_pPlayerPed->SetModelIndex(m_SpawnInfo.iSkin);
	m_pPlayerPed->ClearWeapons();
	m_pPlayerPed->ResetDamageEntity();

	//ApplySpecialAction(0);

	//_this->field_2DE = 0;

	if (m_SpawnInfo.iSpawnWeapons[2] != -1) {
		m_pPlayerPed->GiveWeapon(m_SpawnInfo.iSpawnWeapons[2],
			m_SpawnInfo.iSpawnWeaponsAmmo[2]);
	}

	if (m_SpawnInfo.iSpawnWeapons[1] != -1) {
		m_pPlayerPed->GiveWeapon(m_SpawnInfo.iSpawnWeapons[1],
			m_SpawnInfo.iSpawnWeaponsAmmo[1]);
	}

	if (m_SpawnInfo.iSpawnWeapons[0] != -1) {
		m_pPlayerPed->GiveWeapon(m_SpawnInfo.iSpawnWeapons[0],
			m_SpawnInfo.iSpawnWeaponsAmmo[0]);
	}

	pGame->DisableTrainTraffic();

	m_pPlayerPed->TeleportTo(spawnPos.x,
		spawnPos.y, spawnPos.z + 0.5f);

	m_pPlayerPed->SetTargetRotation(m_SpawnInfo.fRotation);
    CCamera::SetBehindPlayer();

	m_bIsWasted = false;
	m_bIsActive = true;
	m_bWaitingForSpawnRequestReply = false;
	m_surfData.bIsActive = false;

	pGame->DisplayHUD(true);
	m_pPlayerPed->TogglePlayerControllable(true);

	if (pJavaWrapper) {
		pJavaWrapper->ShowLogo(true);
		pJavaWrapper->ShowHud();
	}

	SpeakerList::Show();
	MicroIcon::Show();

	RakNet::BitStream bsSendSpawn;
	NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_Spawn, &bsSendSpawn, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "spawn");

	m_iDisplayZoneTick = GetTickCount() + 1000;

	return true;
}
// 0.3.7
void CLocalPlayer::HandleClassSelection()
{
	m_bClearedToSpawn = false;
	if (m_pPlayerPed) {
		m_pPlayerPed->SetInitialState();
		m_pPlayerPed->SetHealth(100.0f);
		m_pPlayerPed->TogglePlayerControllable(false);
	}
}
// 0.3.7
void CLocalPlayer::SendWastedNotification()
{
	uint8_t byteDeathReason;
	PLAYERID WhoWasResponsible = INVALID_PLAYER_ID;
	RakNet::BitStream bsPlayerDeath;
	byteDeathReason = m_pPlayerPed->FindDeathReasonAndResponsiblePlayer(&WhoWasResponsible);
	bsPlayerDeath.Write(byteDeathReason);
	bsPlayerDeath.Write(WhoWasResponsible);
    FLog("SendWastedNotification %d %d", byteDeathReason, WhoWasResponsible);
	NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_Death, &bsPlayerDeath, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "death");
}
// 0.3.7 (ํๅ๒ extKeys ่ this->field_104 = 1)
void CLocalPlayer::SendOnFootFullSyncData()
{
	RakNet::BitStream bsPlayerSync;
	RwMatrix matPlayer;
	CVector vecMoveSpeed;
	uint16_t lrAnalog, udAnalog;
	uint8_t exKeys = m_pPlayerPed->GetAdditionalKeys();
	uint16_t wKeys = m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog);

	ONFOOT_SYNC_DATA ofSync{};

    matPlayer = m_pPlayerPed->m_pPed->GetMatrix().ToRwMatrix();
	const CVector reliablePos = GetReliablePedSyncPosition64(m_pPlayerPed, "onfoot");
	matPlayer.pos.x = reliablePos.x;
	matPlayer.pos.y = reliablePos.y;
	matPlayer.pos.z = reliablePos.z;
    vecMoveSpeed = m_pPlayerPed->m_pPed->GetMoveSpeed();
	
	ofSync.lrAnalog = lrAnalog;
	ofSync.udAnalog = udAnalog;
	ofSync.wKeys = wKeys;
	ofSync.vecPos = reliablePos;

	ofSync.quat.SetFromMatrix(&matPlayer);
	ofSync.quat.Normalize();

	if (FloatOffset(ofSync.quat.w, m_ofSync.quat.w) < 0.00001 &&
		FloatOffset(ofSync.quat.x, m_ofSync.quat.x) < 0.00001 &&
		FloatOffset(ofSync.quat.y, m_ofSync.quat.y) < 0.00001 &&
		FloatOffset(ofSync.quat.z, m_ofSync.quat.z) < 0.00001)
	{
		ofSync.quat.Set(m_ofSync.quat);
	}

	ofSync.byteHealth = (uint8_t)m_pPlayerPed->GetHealth();
	ofSync.byteArmour = (uint8_t)m_pPlayerPed->GetArmour();

	ofSync.byteCurrentWeapon = (exKeys << 6) | ofSync.byteCurrentWeapon & 0x3F;
	ofSync.byteCurrentWeapon ^= (ofSync.byteCurrentWeapon ^ m_pPlayerPed->GetCurrentWeapon()) & 0x3F;
	ofSync.byteSpecialAction = GetSpecialAction();
	ofSync.vecMoveSpeed.x = vecMoveSpeed.x;
	ofSync.vecMoveSpeed.y = vecMoveSpeed.y;
	ofSync.vecMoveSpeed.z = vecMoveSpeed.z;

    ofSync.vecSurfOffsets.x = 0.0f;
    ofSync.vecSurfOffsets.y = 0.0f;
    ofSync.vecSurfOffsets.z = 0.0f;
    ofSync.wSurfID = 0;
	if(m_surfData.bIsActive){
		if(m_surfData.bIsVehicle && m_surfData.dwSurfVehID != INVALID_VEHICLE_ID){
			CVehicle* pVeh = (CVehicle*)m_surfData.pSurfInst;
			ofSync.vecSurfOffsets.x = m_surfData.vecOffsetPos.x;
			ofSync.vecSurfOffsets.y = m_surfData.vecOffsetPos.y;
			ofSync.vecSurfOffsets.z = m_surfData.vecOffsetPos.z;
			ofSync.wSurfID = m_surfData.dwSurfVehID;
		}
	}

	ofSync.dwAnimation = 0;
	//_this->field_104 = 1;

	if ((GetTickCount() - m_dwLastUpdateOnFootData) > 500 || memcmp(&m_ofSync, &ofSync, sizeof(ONFOOT_SYNC_DATA)))
	{
		m_dwLastUpdateOnFootData = GetTickCount();

		bsPlayerSync.Write((uint8_t)ID_PLAYER_SYNC);
		bsPlayerSync.Write((char*)&ofSync, sizeof(ONFOOT_SYNC_DATA));
		NetTransport::Send(pNetGame->GetRakClient(), &bsPlayerSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 1, "onfoot-sync");
        m_ofSync = ofSync;
	}
}

void CLocalPlayer::SendInCarFullSyncData()
{
	CPlayerPed* pPlayerPed = m_pPlayerPed;

	if (!pPlayerPed) return;

	INCAR_SYNC_DATA icSync;
	memset(&icSync, 0, sizeof(INCAR_SYNC_DATA));

	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
	if(!pVehiclePool) return;

	icSync.VehicleID = pVehiclePool->FindIDFromGtaPtr(m_pPlayerPed->GetGtaVehicle());
	if(icSync.VehicleID == INVALID_VEHICLE_ID) return;

	CVehicle *pVehicle = m_pPlayerPed->GetCurrentVehicle();
	if(!pVehicle) return;

	uint16_t lrAnalog, udAnalog;
	ApplyVehicleSelfTestInputKeys();
	uint8_t exKeys = m_pPlayerPed->GetAdditionalKeys();
	uint16_t wKeys = m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog);

	icSync.lrAnalog = lrAnalog;
	icSync.udAnalog = udAnalog;
	icSync.wKeys = wKeys;

	RwMatrix mat;
	CVector vecMoveSpeed;
    pVehicle->m_pVehicle->GetMatrix(&mat);
	const CVector reliableVehiclePos = GetReliableVehicleSyncPosition64(pVehicle->m_pVehicle, m_pPlayerPed, "incar");
	mat.pos.x = reliableVehiclePos.x;
	mat.pos.y = reliableVehiclePos.y;
	mat.pos.z = reliableVehiclePos.z;
    vecMoveSpeed = pVehicle->m_pVehicle->GetMoveSpeed();

    icSync.quat.SetFromMatrix(&mat);
    icSync.quat.Normalize();

    if (	FloatOffset(icSync.quat.w, m_icSync.quat.w) < 0.00001f
            &&	FloatOffset(icSync.quat.x, m_icSync.quat.x) < 0.00001f
            &&	FloatOffset(icSync.quat.y, m_icSync.quat.y) < 0.00001f
            &&	FloatOffset(icSync.quat.z, m_icSync.quat.z) < 0.00001f)
    {
        icSync.quat.Set(m_icSync.quat);
    }

    icSync.vecPos = reliableVehiclePos;

	icSync.vecMoveSpeed.x = vecMoveSpeed.x;
	icSync.vecMoveSpeed.y = vecMoveSpeed.y;
	icSync.vecMoveSpeed.z = vecMoveSpeed.z;

	icSync.fCarHealth = pVehicle->GetHealth();
	icSync.bytePlayerHealth = m_pPlayerPed->GetHealth();
	icSync.bytePlayerArmour = m_pPlayerPed->GetArmour();

	icSync.byteCurrentWeapon = (exKeys << 6) | icSync.byteCurrentWeapon & 0x3F;
	icSync.byteCurrentWeapon ^= (icSync.byteCurrentWeapon ^ m_pPlayerPed->GetCurrentWeapon()) & 0x3F;

	static uint32_t s_lastVehicleSyncDiagTick = 0;
	static uint32_t s_vehicleSyncDiagCount = 0;
	const uint32_t vehicleSyncDiagNow = GetTickCount();
	if ((wKeys || lrAnalog || udAnalog ||
		 std::fabs(icSync.vecMoveSpeed.x) > 0.01f ||
		 std::fabs(icSync.vecMoveSpeed.y) > 0.01f ||
		 std::fabs(icSync.vecMoveSpeed.z) > 0.01f) &&
		(s_vehicleSyncDiagCount < 80 || vehicleSyncDiagNow - s_lastVehicleSyncDiagTick > 2000)) {
		++s_vehicleSyncDiagCount;
		s_lastVehicleSyncDiagTick = vehicleSyncDiagNow;
		FLog("[VEH_SYNC64] id=%u model=%u keys=%u lr=%u ud=%u pos=%.2f %.2f %.2f speed=%.3f %.3f %.3f engine=%u",
			 icSync.VehicleID,
			 pVehicle->m_pVehicle ? pVehicle->m_pVehicle->GetModelId() : 0,
			 wKeys,
			 lrAnalog,
			 udAnalog,
			 icSync.vecPos.x,
			 icSync.vecPos.y,
			 icSync.vecPos.z,
			 icSync.vecMoveSpeed.x,
			 icSync.vecMoveSpeed.y,
			 icSync.vecMoveSpeed.z,
			 pVehicle->m_pVehicle ? pVehicle->m_pVehicle->m_nVehicleFlags.bEngineOn : 0);
	}

	icSync.TrailerID = INVALID_VEHICLE_ID;

	CVehicle *pTrailer = pVehicle->GetTrailer();
	if(pTrailer && pTrailer->m_pVehicle && pTrailer->GetTractor() == pVehicle)
	{
		pVehicle->SetTrailer(pTrailer);
		icSync.TrailerID = pVehiclePool->FindIDFromGtaPtr(pTrailer->m_pVehicle);
	}
	else pVehicle->SetTrailer(NULL);

	if (pVehicle->m_pVehicle->GetModelId() == TRAIN_PASSENGER_LOCO ||
		pVehicle->m_pVehicle->GetModelId() == TRAIN_FREIGHT_LOCO ||
		pVehicle->m_pVehicle->GetModelId() == TRAIN_TRAM)
	{
		icSync.fTrainSpeed = pVehicle->GetTrainSpeed();
	}
	else if(pVehicle->GetVehicleSubtype() == VEHICLE_SUBTYPE_BIKE ||
		pVehicle->GetVehicleSubtype() == VEHICLE_SUBTYPE_PUSHBIKE)
	{
		icSync.fTrainSpeed = pVehicle->GetBikeLean();
	}
	else if (pVehicle->m_pVehicle->GetModelId() == HYDRA)
	{
		icSync.fTrainSpeed = pVehicle->GetHydraThrusters();
	}
	else
	{
		icSync.fTrainSpeed = 0.0f;
	}

	icSync.byteSirenOn = 0;
	if(pVehicle->SirenEnabled()) icSync.byteSirenOn = 1;

	if(pVehicle->GetVehicleSubtype() == VEHICLE_SUBTYPE_PLANE)
	{
		if(pVehicle->IsLandingGearNotUp()) icSync.byteLandingGearState = 1;
		else icSync.byteLandingGearState = 0;
	}

    if(icSync.TrailerID != INVALID_VEHICLE_ID)
        SendTrailerData(icSync.TrailerID);

	//if (IsNeedSyncDataSend(&m_icSync, &icSync, sizeof(INCAR_SYNC_DATA)))
	if( (GetTickCount() - m_dwLastUpdateInCarData) > 500 || memcmp(&m_icSync, &icSync, sizeof(INCAR_SYNC_DATA)))
	{
		RakNet::BitStream bsVehicleSync;
		bsVehicleSync.Write((uint8_t) ID_VEHICLE_SYNC);
		bsVehicleSync.Write((char *) &icSync, sizeof(INCAR_SYNC_DATA));
		NetTransport::Send(pNetGame->GetRakClient(), &bsVehicleSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0, "vehicle-sync");

		memcpy(&m_icSync, &icSync, sizeof(INCAR_SYNC_DATA));
	}

	//if (pVehicle->HasTurret() || GetTickCount() - m_dwLastSendAimSyncTick > 1000)
	//{
	//	m_dwLastSendAimSyncTick = GetTickCount();
	//	SendAimSyncData();
	//}

}

void CLocalPlayer::SendTrailerData(VEHICLEID vehicleId)
{
	TRAILER_SYNC_DATA trSync;
	memset(&trSync, 0, sizeof(TRAILER_SYNC_DATA));

	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
	if(!pVehiclePool) return;
	
	CVehicle* pTrailer = pVehiclePool->GetAt(vehicleId);
	if(pTrailer)
	{
		RwMatrix matTrailer = pTrailer->m_pVehicle->GetMatrix().ToRwMatrix();
        CQuaternion syncQuat;
        syncQuat.SetFromMatrix(&matTrailer);
        trSync.quat = syncQuat;
		
		trSync.trailerId = vehicleId;

		trSync.vecPos.x = matTrailer.pos.x;
		trSync.vecPos.y = matTrailer.pos.y;
		trSync.vecPos.z = matTrailer.pos.z;

        trSync.vecMoveSpeed = pTrailer->m_pVehicle->GetMoveSpeed();
        trSync.vecTurnSpeed = pTrailer->m_pVehicle->GetTurnSpeed();

		//if(IsNeedSyncDataSend(&m_TrailerData, &trSync, sizeof(TRAILER_SYNC_DATA)))
		//{
			RakNet::BitStream bsTrailerSync;
			bsTrailerSync.Write((uint8_t)ID_TRAILER_SYNC);
			bsTrailerSync.Write((char*)&trSync, sizeof (TRAILER_SYNC_DATA));
			NetTransport::Send(pNetGame->GetRakClient(), &bsTrailerSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0, "trailer-sync");

			memcpy(&m_TrailerData, &trSync, sizeof(TRAILER_SYNC_DATA));
		//}
	}
}

void CLocalPlayer::SendPassengerFullSyncData()
{
	RakNet::BitStream bsData;
	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	uint16_t lrAnalog, udAnalog;
	uint8_t exKeys = m_pPlayerPed->GetAdditionalKeys();
	uint16_t wkeys = m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog);

	PASSENGER_SYNC_DATA psSync;
	memset(&psSync, 0, sizeof(PASSENGER_SYNC_DATA));

    CVehicleGTA* pGtaVehicle = m_pPlayerPed->GetGtaVehicle();
	psSync.VehicleID = pVehiclePool->FindIDFromGtaPtr(pGtaVehicle);
	if (psSync.VehicleID == INVALID_VEHICLE_ID) return;

	psSync.lrAnalog = lrAnalog;
	psSync.udAnalog = udAnalog;
	psSync.wKeys = wkeys;
	psSync.byteCurrentWeapon = (exKeys << 6) | psSync.byteCurrentWeapon & 0x3F;
	psSync.byteCurrentWeapon ^= (psSync.byteCurrentWeapon ^ m_pPlayerPed->GetCurrentWeapon()) & 0x3F;
	psSync.bytePlayerHealth = m_pPlayerPed->GetHealth();
	psSync.bytePlayerArmour = m_pPlayerPed->GetArmour();
	psSync.byteSeatFlags ^= (psSync.byteSeatFlags ^ m_pPlayerPed->GetVehicleSeatID()) & 0x3F;
	uint8_t byteUnk = psSync.byteSeatFlags & 0x7F;
	//if(m_pPlayerPed->IsCuffed()) byteUnk = psSync.byteSeatFlags | 0x80;
	psSync.byteSeatFlags = (byteUnk ^ (m_pPlayerPed->IsInPassengerDriveByMode() << 6)) & 0x40 ^ byteUnk;

    psSync.vecPos = GetReliablePedSyncPosition64(m_pPlayerPed, "passenger");

	if (IsNeedSyncDataSend(&m_psSync, &psSync, sizeof(PASSENGER_SYNC_DATA)))
	{
		bsData.Write((char)ID_PASSENGER_SYNC);
		bsData.Write((char*)& psSync, sizeof(PASSENGER_SYNC_DATA));
		NetTransport::Send(pNetGame->GetRakClient(), &bsData, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 1, "passenger-sync");
		memcpy(&m_psSync, &psSync, sizeof(PASSENGER_SYNC_DATA));
	}

	if(m_bPassengerDriveByMode)	SendAimSyncData();
}
// 0.3.7
void CLocalPlayer::SendAimSyncData()
{
	AIM_SYNC_DATA aimSync{};
	if (!BuildAimSyncData(m_pPlayerPed, &aimSync)) {
		return;
	}

	if ((GetTickCount() - m_dwLastSendSyncTick) > 500 || memcmp(&m_aimSync, &aimSync, sizeof(AIM_SYNC_DATA)))
	{
		m_dwLastSendSyncTick = GetTickCount();
		RakNet::BitStream bsAimSync;
		bsAimSync.Write((char)ID_AIM_SYNC);
		bsAimSync.Write((char*)&aimSync, sizeof(AIM_SYNC_DATA));
		NetTransport::Send(pNetGame->GetRakClient(), &bsAimSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 1, "aim-sync");
		memcpy(&m_aimSync, &aimSync, sizeof(AIM_SYNC_DATA));
	}
}

void CLocalPlayer::SendStatsUpdate()
{
	if(m_statsData.dwLastMoney != pGame->GetLocalMoney() ||
	   m_statsData.dwLastDrunkLevel != m_pPlayerPed->GetDrunkLevel())
	{
		m_statsData.dwLastMoney = pGame->GetLocalMoney();
		m_statsData.dwLastDrunkLevel = m_pPlayerPed->GetDrunkLevel();

		RakNet::BitStream bsStats;
		bsStats.Write((uint8_t)ID_STATS_UPDATE);
		bsStats.Write(m_statsData.dwLastMoney);
		bsStats.Write(m_statsData.dwLastDrunkLevel);
		NetTransport::Send(pNetGame->GetRakClient(), &bsStats, HIGH_PRIORITY, UNRELIABLE, 0, "stats-sync");
	}
}

void CLocalPlayer::CheckWeapons()
{
	if (m_pPlayerPed->IsInVehicle()) return;

	//uint8_t byteCurWep = m_pPlayerPed->GetCurrentWeapon();
	bool bMSend = false;

	for (int i = 0; i < 13; i++) {

		if (m_byteLastWeapon[i] != m_pPlayerPed->m_pPed->m_aWeapons[i].dwType ||
			m_dwLastAmmo[i] != m_pPlayerPed->m_pPed->m_aWeapons[i].dwAmmo)
		{
			m_byteLastWeapon[i] = m_pPlayerPed->m_pPed->m_aWeapons[i].dwType;
			m_dwLastAmmo[i] = m_pPlayerPed->m_pPed->m_aWeapons[i].dwAmmo;

			bMSend = true;
			break;
		}

	}
	if (bMSend) {
		RakNet::BitStream bsWeapons;
		bsWeapons.Write((BYTE) ID_WEAPONS_UPDATE);

		for (int i = 0; i < 13; i++) {

			bsWeapons.Write((uint8_t) i);
			bsWeapons.Write((uint8_t) m_byteLastWeapon[i]);
			bsWeapons.Write((uint16_t) m_dwLastAmmo[i]);
		}
		NetTransport::Send(pNetGame->GetRakClient(), &bsWeapons, HIGH_PRIORITY, RELIABLE, 0, "weapons-sync");
	}
}
// 0.3.7
void CLocalPlayer::UpdateRemoteInterior(uint8_t byteInterior)
{
	m_byteCurInterior = byteInterior;
	RakNet::BitStream bsUpdateInterior;
	bsUpdateInterior.Write(byteInterior);
	NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_SetInteriorId, &bsUpdateInterior, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "set-interior");
}

void CLocalPlayer::UpdateCameraTarget()
{

}

void CLocalPlayer::ProcessSurfing() {
	if(m_pPlayerPed && !m_pPlayerPed->IsDead() && !Xyron::Input::IsJumpDown()) {
		CEntityGTA* contactEntity = m_pPlayerPed->GetEntityUnderPlayer();
		if(contactEntity && pNetGame){
			CVehiclePool* vehiclePool = pNetGame->GetVehiclePool();
			CObjectPool* objectPool = pNetGame->GetObjectPool();
			VEHICLEID vehicleId = vehiclePool
				? vehiclePool->FindIDFromGtaPtr(reinterpret_cast<CVehicleGTA*>(contactEntity))
				: INVALID_VEHICLE_ID;
			if(vehicleId != INVALID_VEHICLE_ID){
				CVehicle* pVeh = vehiclePool->GetAt(vehicleId);
				if(pVeh && (pVeh->HasADriver() || pVeh->m_pVehicle->GetModelId() == 569 || pVeh->m_pVehicle->GetModelId() == 570)
						   && pVeh->m_pVehicle->GetDistanceFromLocalPlayerPed() < 30.0){
					/*bool onFootObject = ScriptCommand(&is_char_touching_vehicle, m_pPlayerPed->m_dwGTAId, pVeh->m_dwGTAId);
                    if(onFootObject){*/
					if(m_surfData.bIsActive){
						return;
					}
					memset(&m_surfData, 0, sizeof(m_surfData));
					m_surfData.vecOffsetPos = CVector{0.0f, 0.0f, 0.0f};
					m_surfData.dwSurfVehID = vehicleId;
					m_surfData.pSurfInst = (uintptr_t)pVeh;
					m_surfData.bIsVehicle = true;

					static RwMatrix matVeh;
                    matVeh = pVeh->m_pVehicle->GetMatrix().ToRwMatrix();
					static RwMatrix matPed;
                    matPed = m_pPlayerPed->m_pPed->GetMatrix().ToRwMatrix();
					static RwMatrix matOut;
					mat_invert(&matOut, &matVeh);
					ProjectMatrix(&m_surfData.vecOffsetPos, (CMatrix*)(&matOut), (CVector*)&matPed.pos);

					m_surfData.bIsActive = true;
					return;
					//}
				}
			}else if(objectPool){
				OBJECTID objectId = objectPool->FindIDFromGtaPtr(reinterpret_cast<CPhysical*>(contactEntity));
				if(objectId != INVALID_OBJECT_ID){
					CObject* pObject = objectPool->GetAt(objectId);
					if(pObject){
						//bool onFootObject = ScriptCommand(&is_char_touching_object, m_pPlayerPed->m_dwGTAId, pObject->m_dwGTAId);
						//if(onFootObject) {
						if(m_surfData.bIsActive){
							return;
						}
						memset(&m_surfData, 0, sizeof(m_surfData));
						m_surfData.bIsVehicle = false;
						m_surfData.pSurfInst = (uintptr_t) pObject;
						m_surfData.bIsActive = true;
						return;
						//}
					}
				}
			}
		}
	}

	m_surfData.bIsActive = false;
	m_surfData.dwSurfVehID = INVALID_VEHICLE_ID;
	m_surfData.pSurfInst = 0;
	m_surfData.vecOffsetPos = CVector{0.0f, 0.0f, 0.0f};

}
void CLocalPlayer::UpdateSurfing() {
	static RwMatrix surfInstMatrix;
	static RwMatrix surfPedMatrix;
	static CVector surfInstMoveSpeed;
	static CVector surfInstTurnSpeed;
	if(m_pPlayerPed) {
		if (Xyron::Input::IsJumpDown()) {
			return;
		}
		if (m_surfData.bIsActive) {
			if (m_surfData.bIsVehicle && m_surfData.pSurfInst) {
				CVehiclePool* vehiclePool = pNetGame ? pNetGame->GetVehiclePool() : nullptr;
				CVehicle *pVeh = vehiclePool && m_surfData.dwSurfVehID != INVALID_VEHICLE_ID
					? vehiclePool->GetAt(static_cast<VEHICLEID>(m_surfData.dwSurfVehID))
					: nullptr;
				if (!pVeh || pVeh != reinterpret_cast<CVehicle*>(m_surfData.pSurfInst) || !pVeh->m_pVehicle) {
					FLog("[SURF_FIX64] clearing stale vehicle surf id=%u inst=%p resolved=%p",
					     static_cast<unsigned>(m_surfData.dwSurfVehID),
					     reinterpret_cast<void*>(m_surfData.pSurfInst),
					     pVeh);
					m_surfData.bIsActive = false;
					m_surfData.dwSurfVehID = INVALID_VEHICLE_ID;
					m_surfData.pSurfInst = 0;
					m_surfData.vecOffsetPos = CVector{0.0f, 0.0f, 0.0f};
					return;
				}
                surfInstMatrix = pVeh->m_pVehicle->GetMatrix().ToRwMatrix();
                surfPedMatrix = m_pPlayerPed->m_pPed->GetMatrix().ToRwMatrix();
                surfInstMoveSpeed = pVeh->m_pVehicle->GetMoveSpeed();
                surfInstTurnSpeed = pVeh->m_pVehicle->GetTurnSpeed();

				uint16_t lrAnalog;
				uint16_t udAnalog;
				m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog);

				if (lrAnalog || udAnalog) {
					static RwMatrix matOut;
					mat_invert(&matOut, &surfInstMatrix);
					ProjectMatrix(&m_surfData.vecOffsetPos, (CMatrix*)&matOut,  (CVector*)&surfPedMatrix.pos);
				} else {
					ProjectMatrix( (CVector*)&surfPedMatrix.pos, (CMatrix*)&surfInstMatrix,  (CVector*)&m_surfData.vecOffsetPos);

					m_pPlayerPed->m_pPed->SetMatrix((CMatrix&)surfPedMatrix);
					CVector vecMoveSpeed;
                    vecMoveSpeed = m_pPlayerPed->m_pPed->GetMoveSpeed();
					m_pPlayerPed->m_pPed->SetVelocity(
							CVector{surfInstMoveSpeed.x, surfInstMoveSpeed.y, vecMoveSpeed.z});

					CVector vecTurnSpeed = m_pPlayerPed->m_pPed->GetTurnSpeed();
					m_pPlayerPed->m_pPed->SetTurnSpeed(
							CVector{vecTurnSpeed.x, vecTurnSpeed.y, surfInstTurnSpeed.z});
				}
			} else {
				CObject *pObject = (CObject *) m_surfData.pSurfInst;
				bool objectStillTracked = false;
				CObjectPool* objectPool = pNetGame ? pNetGame->GetObjectPool() : nullptr;
				if (objectPool && pObject) {
					for (OBJECTID objectId = 0; objectId < MAX_OBJECTS; ++objectId) {
						if (objectPool->GetAt(objectId) == pObject) {
							objectStillTracked = true;
							break;
						}
					}
				}

				if (!objectStillTracked || !pObject || !pObject->m_pEntity) {
					FLog("[SURF_FIX64] clearing stale object surf inst=%p tracked=%d",
					     reinterpret_cast<void*>(m_surfData.pSurfInst),
					     objectStillTracked ? 1 : 0);
					m_surfData.bIsActive = false;
					m_surfData.dwSurfVehID = INVALID_VEHICLE_ID;
					m_surfData.pSurfInst = 0;
					m_surfData.vecOffsetPos = CVector{0.0f, 0.0f, 0.0f};
					return;
				}

				if (pObject->m_byteMoving & 1) {
                    surfInstMatrix = pObject->m_pEntity->GetMatrix().ToRwMatrix();
                    surfPedMatrix = m_pPlayerPed->m_pPed->GetMatrix().ToRwMatrix();
                    surfInstMoveSpeed = pObject->m_pEntity->GetMoveSpeed();
                    surfInstTurnSpeed = pObject->m_pEntity->GetTurnSpeed();

					uint16_t lrAnalog;
					uint16_t udAnalog;
					m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog);

					if (lrAnalog || udAnalog) {
						static RwMatrix matOut;
						mat_invert(&matOut, &surfInstMatrix);
						ProjectMatrix(&m_surfData.vecOffsetPos, (CMatrix*)&matOut,  (CVector*)&surfPedMatrix.pos);
					} else {
						ProjectMatrix( (CVector*)&surfPedMatrix.pos, (CMatrix*)&surfInstMatrix,
									  &m_surfData.vecOffsetPos);

						m_pPlayerPed->m_pPed->SetMatrix((CMatrix&)surfPedMatrix);
						CVector vecMoveSpeed = m_pPlayerPed->m_pPed->GetMoveSpeed();
						m_pPlayerPed->m_pPed->SetVelocity(
								CVector{surfInstMoveSpeed.x, surfInstMoveSpeed.y, vecMoveSpeed.z});

						CVector vecTurnSpeed = m_pPlayerPed->m_pPed->GetTurnSpeed();
						m_pPlayerPed->m_pPed->SetTurnSpeed(
								CVector{vecTurnSpeed.x, vecTurnSpeed.y, surfInstTurnSpeed.z});
					}
				}

			}
		}

	}
}

void CLocalPlayer::MoveHeadWithCamera()
{

}

void CLocalPlayer::HandleSourceVehicleAction()
{
	if (!m_pPlayerPed) {
		return;
	}

	if (m_pPlayerPed->IsInVehicle()) {
		m_pPlayerPed->ExitCurrentVehicle();
		return;
	}

	if (!EnterVehicleAsDriver()) {
		Xyron::Input::RequestPassengerVehicleAction();
	}
}

bool CLocalPlayer::HandlePassengerEntry()
{
	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();

	if (!Xyron::Input::HasPendingPassengerVehicleAction()) return false;
	VEHICLEID ClosetVehicleID = pVehiclePool->FindNearestToLocalPlayerPed();
	CVehicle* pVehicle = pVehiclePool->GetAt(ClosetVehicleID);
	if (!pVehicle) return false;

	if (pVehicle->m_pVehicle->GetDistanceFromLocalPlayerPed() < 8.0f)
	{
		if (m_pPlayerPed->GetCurrentWeapon() == WEAPON_PARACHUTE) {
			m_pPlayerPed->SetArmedWeapon(0, false);
		}

		m_pPlayerPed->EnterVehicle(pVehicle->m_dwGTAId, true);
		SendEnterVehicleNotification(ClosetVehicleID, true);
		Xyron::Input::ClearPassengerVehicleActionRequest();
	}
	Xyron::Input::ClearPassengerVehicleActionRequest();
	return true;
}

bool CLocalPlayer::EnterVehicleAsDriver()
{
	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	VEHICLEID ClosetVehicleID = pVehiclePool->FindNearestToLocalPlayerPed();
	CVehicle* pVehicle = pVehiclePool->GetAt(ClosetVehicleID);
	if (!pVehicle) return false;

	if (pVehicle->m_pVehicle->GetDistanceFromLocalPlayerPed() < 8.0f)
	{
		if (m_pPlayerPed->GetCurrentWeapon() == WEAPON_PARACHUTE) {
			m_pPlayerPed->SetArmedWeapon(0, false);
		}

		m_pPlayerPed->EnterVehicle(pVehicle->m_dwGTAId, false);
		//SendEnterVehicleNotification(ClosetVehicleID, false);
		return true;
	}

	return false;
}

void CLocalPlayer::ProcessOnFootWorldBounds()
{
	if(pGame->GetActiveInterior() != 0) return; // can't enforce inside interior

	/*if(m_pPlayerPed->EnforceWorldBoundries(
			pNetGame->m_pNetSet->fWorldBounds[0], pNetGame->m_pNetSet->fWorldBounds[1],
			pNetGame->m_pNetSet->fWorldBounds[2], pNetGame->m_pNetSet->fWorldBounds[3]))
	{
		m_pPlayerPed->SetArmedWeapon(0, 0);
		pGame->DisplayGameText("Stay within the ~r~world boundries", 1000, 5);
	}*/
}

void CLocalPlayer::ProcessInCarWorldBounds()
{
	/*CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
	if(pVehiclePool)
	{
		if(pGame->GetActiveInterior() != 0) return; // can't enforce inside interior

		VEHICLEID vehicleId = pVehiclePool->FindIDFromGtaPtr(m_pPlayerPed->GetGtaVehicle());
		if(vehicleId != INVALID_VEHICLE_ID)
		{
			CVehicle *pVehicle = pVehiclePool->GetAt(vehicleId);
			if(pVehicle)
			{
				if(pVehicle->EnforceWorldBoundries(
						pNetGame->m_pNetSet->fWorldBounds[0], pNetGame->m_pNetSet->fWorldBounds[1],
						pNetGame->m_pNetSet->fWorldBounds[2], pNetGame->m_pNetSet->fWorldBounds[3]))
				{
					pGame->DisplayGameText("Stay within the ~r~world boundries", 1000, 5);
				}
			}
		}
	}*/
}
// 0.3.7
bool CLocalPlayer::CompareOnFootSyncKeys(uint16_t wKeys, uint16_t lrAnalog, uint16_t udAnalog)
{
	return wKeys != m_ofSync.wKeys || udAnalog != m_ofSync.udAnalog || lrAnalog != m_ofSync.lrAnalog;
}
// 0.3.7
int CLocalPlayer::GetOptimumOnFootSendRate()
{
	if(!m_pPlayerPed) return 1000;

	if(pNetGame->m_bLanMode) return 15;
	else
	{
		int iNumPlayersInRange = 0;
		for(int i = 2; i < 120; i++)
			if(bUsedPlayerSlots[i]) iNumPlayersInRange++;

		return (iNetModeNormalOnFootSendRate + iNumPlayersInRange);
	}
}
// 0.3.7
int CLocalPlayer::GetOptimumInCarSendRate()
{
	if(!m_pPlayerPed) return 1000;

	if(pNetGame->m_bLanMode) return 15;
	else
	{
		int iNumPlayersInRange = 0;
		for(int i = 0; i < 120; i++)
			if(bUsedPlayerSlots[i]) iNumPlayersInRange++;

		return (iNetModeNormalInCarSendRate + iNumPlayersInRange);
	}
}

void CLocalPlayer::UpdateVehicleDamage(VEHICLEID vehicleID)
{

}
// 0.3.7
void CLocalPlayer::SendNextClass()
{
	RwMatrix matPlayer;

	if (!m_bSpawnDialogShowed) return;

	m_bClearedToSpawn = false;
    matPlayer = m_pPlayerPed->m_pPed->GetMatrix().ToRwMatrix();

	if (m_iSelectedClass == (pNetGame->m_pNetSet->iSpawnsAvailable - 1)) m_iSelectedClass = 0;
	else m_iSelectedClass++;

	pGame->PlaySound(1052, matPlayer.pos.x, matPlayer.pos.y, matPlayer.pos.z);
	RequestClass(m_iSelectedClass);
}
// 0.3.7
void CLocalPlayer::SendPrevClass()
{
	RwMatrix matPlayer;

	if (!m_bSpawnDialogShowed) return;

	m_bClearedToSpawn = false;
    matPlayer = m_pPlayerPed->m_pPed->GetMatrix().ToRwMatrix();

	if (m_iSelectedClass == 0) m_iSelectedClass = (pNetGame->m_pNetSet->iSpawnsAvailable - 1);
	else m_iSelectedClass--;

	pGame->PlaySound(1053, matPlayer.pos.x, matPlayer.pos.y, matPlayer.pos.z);
	RequestClass(m_iSelectedClass);
}
// 0.3.7
void CLocalPlayer::SendSpawn()
{
	if (!m_bSpawnDialogShowed) return;

	RequestSpawn();
	m_bWaitingForSpawnRequestReply = true;
}
// 0.3.7
void CLocalPlayer::RequestClass(int iClass)
{
	RakNet::BitStream bsClassRequest;
	bsClassRequest.Write(iClass);
	NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_RequestClass, &bsClassRequest, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "request-class");
}
// 0.3.7
void CLocalPlayer::RequestSpawn()
{
	RakNet::BitStream bsSpawnRequest;
	NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_RequestSpawn, &bsSpawnRequest, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "request-spawn");
}

void CLocalPlayer::ApplySpecialAction(uint8_t byteSpecialAction)
{
	if(!m_pPlayerPed) return;

	m_pPlayerPed->iSpecialAction = byteSpecialAction;

	if(byteSpecialAction != SPECIAL_ACTION_USECELLPHONE && m_pPlayerPed->IsCellphoneEnabled())
		m_pPlayerPed->ToggleCellphone(0);
	if(byteSpecialAction != SPECIAL_ACTION_USEJETPACK && m_pPlayerPed->IsInJetpackMode())
		m_pPlayerPed->StopJetpack();
	if(byteSpecialAction != SPECIAL_ACTION_HANDSUP && m_pPlayerPed->HasHandsUp())
		m_pPlayerPed->TogglePlayerControllable(true);
	if(m_pPlayerPed->GetDanceStyle() != -1)
	{
		if((byteSpecialAction != SPECIAL_ACTION_DANCE1 ||
			byteSpecialAction != SPECIAL_ACTION_DANCE2 ||
			byteSpecialAction != SPECIAL_ACTION_DANCE3 ||
			byteSpecialAction != SPECIAL_ACTION_DANCE4))
		{
			m_pPlayerPed->StopDancing();
		}
	}
	if(byteSpecialAction != SPECIAL_ACTION_PISSING && m_pPlayerPed->IsPissing())
		m_pPlayerPed->StopPissing();
	if(m_pPlayerPed->GetStuff() != eStuffType::STUFF_TYPE_NONE)
	{
		if(byteSpecialAction != SPECIAL_ACTION_DRINK_BEER ||
			byteSpecialAction != SPECIAL_ACTION_SMOKE_CIGGY ||
			byteSpecialAction != SPECIAL_ACTION_DRINK_WINE ||
			byteSpecialAction != SPECIAL_ACTION_DRINK_SPRUNK)
		{
			m_pPlayerPed->DropStuff();
		}
	}

	switch(byteSpecialAction)
	{
		default:
		case SPECIAL_ACTION_NONE:
			// ~
		break;

		case SPECIAL_ACTION_USECELLPHONE:
			m_pPlayerPed->ToggleCellphone(1);
		break;

		case SPECIAL_ACTION_STOPUSECELLPHONE:
			if(m_pPlayerPed->IsCellphoneEnabled()) m_pPlayerPed->ToggleCellphone(0);
		break;

		case SPECIAL_ACTION_USEJETPACK:
			if(!m_pPlayerPed->IsInJetpackMode()) m_pPlayerPed->StartJetpack();
		break;

		case SPECIAL_ACTION_HANDSUP:
			m_pPlayerPed->HandsUp();
		break;

		case SPECIAL_ACTION_PISSING:
			m_pPlayerPed->StartPissing();
		break;

		case SPECIAL_ACTION_DANCE1:
			m_pPlayerPed->StartDancing(1);
		break;

		case SPECIAL_ACTION_DANCE2:
			m_pPlayerPed->StartDancing(2);
		break;

		case SPECIAL_ACTION_DANCE3:
			m_pPlayerPed->StartDancing(3);
		break;

		case SPECIAL_ACTION_DANCE4:
			m_pPlayerPed->StartDancing(4);
		break;

		case SPECIAL_ACTION_DRINK_BEER:
			m_pPlayerPed->GiveStuff(eStuffType::STUFF_TYPE_BEER);
		break;

		case SPECIAL_ACTION_SMOKE_CIGGY:
			m_pPlayerPed->GiveStuff(eStuffType::STUFF_TYPE_CIGGI);
		break;

		case SPECIAL_ACTION_DRINK_WINE:
			m_pPlayerPed->GiveStuff(eStuffType::STUFF_TYPE_DYN_BEER);
		break;

		case SPECIAL_ACTION_DRINK_SPRUNK:
			m_pPlayerPed->GiveStuff(eStuffType::STUFF_TYPE_PINT_GLASS);
		break;
	}
}
// 0.3.7
void CLocalPlayer::SetSpawnInfo(PLAYER_SPAWN_INFO* pSpawnInfo)
{
	memcpy(&m_SpawnInfo, pSpawnInfo, sizeof(PLAYER_SPAWN_INFO));
	m_bHasSpawnInfo = true;
    FLog("[SPAWN_FIX64] SetSpawnInfo skin=%d pos=%.2f %.2f %.2f rot=%.2f",
         m_SpawnInfo.iSkin,
         m_SpawnInfo.vecPos.x,
         m_SpawnInfo.vecPos.y,
         m_SpawnInfo.vecPos.z,
         m_SpawnInfo.fRotation);
}

bool CLocalPlayer::TryGetSpawnPos(CVector& outPos) const
{
    if (!m_bHasSpawnInfo) {
        return false;
    }

    outPos = m_SpawnInfo.vecPos;
    return true;
}
// 0.3.7
void CLocalPlayer::HandleClassSelectionOutcome(bool bOutcome)
{
	if (bOutcome)
	{
		if (m_pPlayerPed)
		{
			m_pPlayerPed->ClearWeapons();
			m_pPlayerPed->SetModelIndex(m_SpawnInfo.iSkin);
		}

		m_bClearedToSpawn = true;
		if (!m_bIsActive && !m_bWaitingForSpawnRequestReply)
		{
			if (pUI) pUI->spawn()->setVisible(false);
			RequestSpawn();
			m_bWaitingForSpawnRequestReply = true;
			FLog("[SPAWN_FLOW64] class-ready auto-request-spawn");
		}
	}
}

uint8_t CLocalPlayer::GetSpecialAction()
{
	if(!m_pPlayerPed) return SPECIAL_ACTION_NONE;

	if(m_pPlayerPed->IsCrouching())
		return SPECIAL_ACTION_DUCK;

	if(m_pPlayerPed->IsEnteringVehicle() != 0)
		return SPECIAL_ACTION_ENTER_VEHICLE;

	if(m_pPlayerPed->IsExitingVehicle())
		return SPECIAL_ACTION_EXIT_VEHICLE;

	if(m_pPlayerPed->IsSitTask())
		return SPECIAL_ACTION_SITTING;

	if(m_pPlayerPed->IsInJetpackMode())
		return SPECIAL_ACTION_USEJETPACK;

	if(m_pPlayerPed->IsCuffed())
		return SPECIAL_ACTION_CUFFED;

	if(m_pPlayerPed->IsCarry())
		return SPECIAL_ACTION_CARRY;

	if(m_pPlayerPed->GetDanceStyle() != -1)
	{
		switch(m_pPlayerPed->GetDanceStyle())
		{
			case 0:
				return SPECIAL_ACTION_DANCE1;
				break;
			case 1:
				return SPECIAL_ACTION_DANCE2;
				break;
			case 2:
				return SPECIAL_ACTION_DANCE3;
				break;
			case 3:
				return SPECIAL_ACTION_DANCE4;
				break;
		}
	}

	if(m_pPlayerPed->HasHandsUp())
		return SPECIAL_ACTION_HANDSUP;

	if(m_pPlayerPed->IsCellphoneEnabled())
		return SPECIAL_ACTION_USECELLPHONE;

	if(m_pPlayerPed->IsPissing())
		return SPECIAL_ACTION_PISSING;

	if(m_pPlayerPed->GetStuff() == eStuffType::STUFF_TYPE_BEER)
		return SPECIAL_ACTION_DRINK_BEER;

	if(m_pPlayerPed->GetStuff() == eStuffType::STUFF_TYPE_DYN_BEER)
		return SPECIAL_ACTION_DRINK_WINE;

	if(m_pPlayerPed->GetStuff() == eStuffType::STUFF_TYPE_PINT_GLASS)
		return SPECIAL_ACTION_DRINK_SPRUNK;

	if(m_pPlayerPed->GetStuff() == eStuffType::STUFF_TYPE_CIGGI)
		return SPECIAL_ACTION_SMOKE_CIGGY;

	return SPECIAL_ACTION_NONE;
}
// 0.3.7
uint32_t CLocalPlayer::GetPlayerColorAsARGB()
{
	return (TranslateColorCodeToRGBA(pNetGame->GetPlayerPool()->GetLocalPlayerID()) >> 8) | 0xFF000000;
}
// 0.3.7
uint32_t CLocalPlayer::GetPlayerColorAsRGBA()
{
	return TranslateColorCodeToRGBA(pNetGame->GetPlayerPool()->GetLocalPlayerID());
}
// 0.3.7
bool CLocalPlayer::IsNeedSyncDataSend(const void* data1, const void* data2, size_t size)
{
	return NetTransport::ShouldSendDelta(m_dwLastSendSyncTick, data1, data2, size, 500);
}
// 0.3.7
void CLocalPlayer::SendEnterVehicleNotification(VEHICLEID VehicleID, bool bPassenger)
{
	uint8_t bytePassenger = 0;

	if(bPassenger)
		bytePassenger = 1;
	RakNet::BitStream bsSend;
	bsSend.Write(VehicleID);
	bsSend.Write(bytePassenger);
	NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_EnterVehicle, &bsSend, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "enter-vehicle");
}
// 0.3.7
void CLocalPlayer::SpectatePlayer(PLAYERID PlayerID)
{
	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();

	if (pPlayerPool && pPlayerPool->GetSlotState(PlayerID))
	{
		CRemotePlayer* pRemotePlayer = pPlayerPool->GetAt(PlayerID);
		if (pRemotePlayer->GetState() != PLAYER_STATE_NONE &&
			pRemotePlayer->GetState() != PLAYER_STATE_WASTED)
		{
			m_byteSpectateType = SPECTATE_TYPE_PLAYER;
			m_SpectateID = PlayerID;
			m_bSpectateProcessed = false;
		}
	}
}

void CLocalPlayer::SpectateVehicle(VEHICLEID VehicleID)
{
	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();

	if (pVehiclePool && pVehiclePool->GetSlotState(VehicleID))
	{
		m_byteSpectateType = SPECTATE_TYPE_VEHICLE;
		m_SpectateID = VehicleID;
		m_bSpectateProcessed = false;
	}
}
// 0.3.7
void CLocalPlayer::SetPlayerColor(uint32_t dwColor)
{
	SetRadarColor(pNetGame->GetPlayerPool()->GetLocalPlayerID(), dwColor);
}
// 0.3.7
void CLocalPlayer::SendExitVehicleNotification(VEHICLEID VehicleID)
{
	RakNet::BitStream bsSend;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (pVehicle)
	{
		if (!m_pPlayerPed->IsAPassenger())
			m_LastVehicle = VehicleID;

		if (pVehicle->IsATrainPart())
            CCamera::SetBehindPlayer();

		if (!pVehicle->IsRCVehicle())
		{
			bsSend.Write(VehicleID);
			NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_ExitVehicle, &bsSend, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "exit-vehicle");
		}
	}
}
// 0.3.7
void CLocalPlayer::SendTakeDamageEvent(PLAYERID PlayerID, float fDamageFactor, int weaponType, int pedPieceType)
{
	fDamageFactor *= 0.33f;

	RakNet::BitStream bsSend;
	bsSend.Write(true);
	bsSend.Write(PlayerID);
	bsSend.Write(fDamageFactor);
	bsSend.Write(weaponType);
	bsSend.Write(pedPieceType);

	NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_PlayerGiveTakeDamage, &bsSend, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "take-damage");
}
// 0.3.7
void CLocalPlayer::SendGiveDamageEvent(PLAYERID PlayerID, float fDamageFactor, int weaponType, int pedPieceType)
{
	fDamageFactor *= 0.33f;

	RakNet::BitStream bsSend;
	bsSend.Write(false);
	bsSend.Write(PlayerID);
	bsSend.Write(fDamageFactor);
	bsSend.Write(weaponType);
	bsSend.Write(pedPieceType);

	NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_PlayerGiveTakeDamage, &bsSend, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "give-damage");
}
// 0.3.7
void CLocalPlayer::SendGiveDamageActorEvent(PLAYERID ActorID, float fDamageFactor, int weaponType, int pedPieceType)
{
	RakNet::BitStream bsSend;
	bsSend.Write(false);
	bsSend.Write(ActorID);
	bsSend.Write(fDamageFactor);
	bsSend.Write(weaponType);
	bsSend.Write(pedPieceType);

	NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_GiveDamageActor, &bsSend, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "give-damage-actor");
}

void CLocalPlayer::GiveTakeDamage(bool bGiveOrTake, uint16_t wPlayerID, float damage_amount, uint32_t weapon_id, uint32_t bodypart)
{
	RakNet::BitStream bitStream;

	bitStream.Write((bool)bGiveOrTake);
	bitStream.Write((uint16_t)wPlayerID);
	bitStream.Write((float)damage_amount);
	bitStream.Write((uint32_t)weapon_id);
	bitStream.Write((uint32_t)bodypart);

	NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_PlayerGiveTakeDamage, &bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "give-take-damage");
}

void CLocalPlayer::GiveActorDamage(PLAYERID wPlayerID, float damage_amount, uint32_t weapon_id, uint32_t bodypart)
{
	RakNet::BitStream bitStream;
	bitStream.Write((uint16_t)wPlayerID);
	bitStream.Write((float)damage_amount);
	bitStream.Write((uint32_t)weapon_id);
	bitStream.Write((uint32_t)bodypart);

	int RPC_GiveActorDamage = 177;
	NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_GiveActorDamage, &bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "actor-damage");
}

uint32_t CLocalPlayer::GetCurrentAnimationIndexFlag()
{
	uint32_t dwAnim = 0;

	float fBlendData = 4.0f;

	int iAnimIdx = m_pPlayerPed->GetCurrentAnimationIndex();

	uint32_t hardcodedBlend = 0b00000100;	// 4
	hardcodedBlend <<= 16;

	uint32_t hardcodedFlags = 0;

	if (iAnimIdx)
	{
		hardcodedFlags = 0b00010001;
	}
	else
	{
		hardcodedFlags = 0b10000000;
		iAnimIdx = 1189;
	}

	hardcodedFlags <<= 24;

	uint16_t usAnimidx = (uint16_t)iAnimIdx;

	dwAnim = (uint32_t)usAnimidx;
	dwAnim |= hardcodedBlend;
	dwAnim |= hardcodedFlags;

	return dwAnim;
}

void CLocalPlayer::SendBulletSyncData(PLAYERID byteHitID, uint8_t byteHitType, CVector vecHitPos)
{
    if (!m_pPlayerPed) return;
    switch (byteHitType)
    {
        case BULLET_HIT_TYPE_NONE:
            break;
        case BULLET_HIT_TYPE_PLAYER:
            if (!pNetGame->GetPlayerPool()->GetSlotState((PLAYERID)byteHitID)) return;
            break;

    }
	uint8_t byteCurrWeapon = m_pPlayerPed->GetCurrentWeapon(), byteShotWeapon;
    //uint8_t byteCurrWeapon = m_pPlayerPed->GetCurrentWeapon(), byteShotWeapon;

    RwMatrix matPlayer;
    BULLET_SYNC blSync;

    matPlayer = m_pPlayerPed->m_pPed->GetMatrix().ToRwMatrix();

    blSync.hitId = byteHitID;
    blSync.hitType = byteHitType;

    if (byteHitType == BULLET_HIT_TYPE_PLAYER)
    {
        float fDistance = pNetGame->GetPlayerPool()->GetAt((PLAYERID)byteHitID)->GetPlayerPed()->m_pPed->GetDistanceFromLocalPlayerPed();
        if (byteCurrWeapon != 0 && fDistance < 1.0f)
            byteShotWeapon = 0;
        else
            byteShotWeapon = byteCurrWeapon;
    }
    else
    {
        byteShotWeapon = m_pPlayerPed->GetCurrentWeapon();
    }
    blSync.weapId = byteShotWeapon;

    blSync.hitPos[0] = vecHitPos.x;
    blSync.hitPos[1] = vecHitPos.y;
    blSync.hitPos[2] = vecHitPos.z;

    blSync.offsets[0] = 0.0f;
    blSync.offsets[1] = 0.0f;
    blSync.offsets[2] = 0.0f;

	FLog("SendBulletSync: %d, %d, %d, %f, %f, %f, %f, %f, %f", blSync.hitId, blSync.hitType, blSync.weapId,
		 blSync.hitPos[0], blSync.hitPos[1], blSync.hitPos[2], blSync.offsets[0], blSync.offsets[1], blSync.offsets[2]);

    RakNet::BitStream bsBulletSync;
    bsBulletSync.Write((uint8_t)ID_BULLET_SYNC);
    bsBulletSync.Write((const char*)& blSync, sizeof(BULLET_SYNC));
    NetTransport::Send(pNetGame->GetRakClient(), &bsBulletSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0, "bullet-sync");
}

int CLocalPlayer::GetOptimumUnoccupiedSendRate()
{
	if(!m_pPlayerPed) return 1000;

	if(m_pPlayerPed->GetGtaVehicle()) // ramming an unoccupied vehicle
		return GetOptimumInCarSendRate();
	else return GetOptimumOnFootSendRate(); // pushing an unoccupied vehicle
}

bool CLocalPlayer::ProcessUnoccupiedSync(VEHICLEID vehicleId, CVehicle *pVehicle)
{
	if((GetTickCount() - m_dwLastSendTick) > (unsigned int)GetOptimumUnoccupiedSendRate())
	//if(vehicleId <= 2000 && pVehicle)
	{
		CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
		CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
		if(!pPlayerPool || !pVehiclePool) return false;

        CVehicleGTA *pVehicleType = pVehicle->m_pVehicle;
		if(pVehicleType && m_pPlayerPed && !pVehicle->IsATrainPart() &&
		   !pVehicle->IsATrailer() && !pVehicle->GetTractor())
		{
			CPedGTA *pDriver = pVehicleType->pDriver;
			if(pDriver && pDriver->IsInVehicle() ||
			   pVehicle->m_pVehicle->GetDistanceFromLocalPlayerPed() > 90.0f /*||
			   pVehicle->m_pVehicle->IsStationary()*/)
			{
				return false;
			}

			float fDistance = 0.0f, fSmallest = 1000000.0f;
			PLAYERID iClosestPlayerId = 0;

			for(int i = 0; i < 7; i++)
			{
				CPedGTA *pPassenger = pVehicleType->m_apPassengers[i];
				if(pPassenger && pPassenger->m_nPedType == (ePedType)0)
				{
					if(pPassenger == m_pPlayerPed->m_pPed) goto sync;
					return false;
				}
			}

			for(PLAYERID i = 0; i < MAX_PLAYERS; i++)
			{
				CPlayerPed* pPlayerPed;

				if(i == pPlayerPool->GetLocalPlayerID())
					pPlayerPed = m_pPlayerPed;
				else
				{
					CRemotePlayer* pTmpPlayer = pPlayerPool->GetAt(i);
					if(pTmpPlayer) pPlayerPed = pTmpPlayer->GetPlayerPed();
				}

				if(pVehicle && pPlayerPed && pPlayerPed->m_pPed->IsAdded())
				{
					fDistance = pPlayerPed->GetDistanceFromVehicle(pVehicle);
					if(i)
					{
						if(fDistance < fSmallest)
						{
							fSmallest = fDistance;
							iClosestPlayerId = i;
						}
					}
					else fSmallest = fDistance;
				}
			}

			if(iClosestPlayerId == pPlayerPool->GetLocalPlayerID() && fSmallest <= 90.0f)
			{
				sync:
				SendUnoccupiedData(vehicleId, pVehicle);
                m_dwLastSendTick = GetTickCount();
				return true;
			}
		}
	}

	return false;
}

void CompressNormalVector(CVector *vecOut, CVector vecIn)
{
	vecOut->x = (short)(vecIn.x * 10000.0);
	vecOut->y = (short)(vecIn.y * 10000.0);
	vecOut->z = (short)(vecIn.z * 10000.0);
}

void CLocalPlayer::SendUnoccupiedData(VEHICLEID vehicleId, CVehicle *pVehicle)
{
	RakNet::BitStream bsUnoccupiedSync;
	CVector vecMoveSpeed, vecTurnSpeed;
	RwMatrix matVehicle;
	UNOCCUPIED_SYNC_DATA unSync;

    matVehicle = pVehicle->m_pVehicle->GetMatrix().ToRwMatrix();

	CompressNormalVector(&unSync.vecRoll, matVehicle.right);
	CompressNormalVector(&unSync.vecDirection, matVehicle.up);

    unSync.vehicleId = vehicleId;

	unSync.byteSeatId = m_pPlayerPed->GetVehicleSeatID();

    unSync.vecMoveSpeed = pVehicle->m_pVehicle->GetMoveSpeed();
    unSync.vecTurnSpeed = pVehicle->m_pVehicle->GetTurnSpeed();

	unSync.vecPos.x = matVehicle.pos.x;
	unSync.vecPos.y = matVehicle.pos.y;
	unSync.vecPos.z = matVehicle.pos.z;

	unSync.fCarHealth = pVehicle->GetHealth();

	if(IsNeedSyncDataSend(&m_UnoccupiedData, &unSync, sizeof(UNOCCUPIED_SYNC_DATA)))
	{
		bsUnoccupiedSync.Write((uint8_t)ID_UNOCCUPIED_SYNC);
		bsUnoccupiedSync.Write((char*)&unSync, sizeof(UNOCCUPIED_SYNC_DATA));
		NetTransport::Send(pNetGame->GetRakClient(), &bsUnoccupiedSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0, "unoccupied-sync");

		memcpy(&m_UnoccupiedData, &unSync, sizeof(UNOCCUPIED_SYNC_DATA));
	}
}

void CLocalPlayer::MaybeSendExitVehicle() {
    bool exitVehicleState = m_pPlayerPed->m_pPed->IsExitingVehicle();

    if(Xyron::Input::ConsumeExitVehicleTaskStarted(exitVehicleState)) {
        auto vehicleId = pNetGame->GetVehiclePool()->FindIDFromGtaPtr(m_pPlayerPed->m_pPed->pVehicle);

        if(vehicleId != INVALID_VEHICLE_ID) {
            RakNet::BitStream bsSend;

            bsSend.Write(vehicleId);
            NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_ExitVehicle, &bsSend, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, NULL, "maybe-exit-vehicle");
        }

    }
}

void CLocalPlayer::MaybeSendEnterVehicle() {
    CTaskComplexEnterCarAsDriver* task
            = static_cast<CTaskComplexEnterCarAsDriver*>(m_pPlayerPed->m_pPed->GetTaskManager().CTaskManager::FindActiveTaskByType(TASK_COMPLEX_ENTER_CAR_AS_DRIVER));

    bool enterVehicleState = task != nullptr;

    if(Xyron::Input::ConsumeDriverEnterVehicleTaskStarted(enterVehicleState)) {
        auto vehicleId = pNetGame->GetVehiclePool()->FindIDFromGtaPtr(task->GetTarget());

        if(vehicleId != INVALID_VEHICLE_ID)
            SendEnterVehicleNotification(vehicleId, false);
    }
}
