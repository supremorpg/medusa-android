#include "main.h"
#include "game/game.h"
#include "game/World.h"
#include "game/GTASAEngineApi.h"
#include "game/Camera.h"
#include "game/SampAppCommandApi.h"
#include "game/SampCollisionApi.h"
#include "game/SampInputApi.h"
#include "game/SampStreamingApi.h"
#include "net/netgame.h"
#include "gui/gui.h"
#include "jniutil.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <ctime>
#include <sys/stat.h>

extern CGame *pGame;
extern UI *pUI;
extern CNetGame *pNetGame;

extern "C" void WarmKnownArm64CollisionSlotsForPosition(const CVector& pos, uint8_t area);

extern bool OpenButton;
extern bool Mchat;

extern "C" void StartVehicleSelfTestInput(int durationMs, int steering, bool throttle, bool brake);

#if !VER_x32
namespace {
struct PendingMapScanTeleport64 {
	bool pending = false;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	int area = 0;
};

struct PendingMapScanProbe64 {
	bool pending = false;
	char zoneId[96] = {};
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	int area = 0;
};

std::mutex g_mapScanTeleportMutex64;
std::mutex g_mapScanProbeMutex64;
PendingMapScanTeleport64 g_pendingMapScanTeleport64;
PendingMapScanProbe64 g_pendingMapScanProbe64;

bool ApplyNativeMapScanTeleportNow64(float x, float y, float z, int area)
{
	const float targetX = static_cast<float>(x);
	const float targetY = static_cast<float>(y);
	const float targetZ = static_cast<float>(z);
	if (!std::isfinite(targetX) || !std::isfinite(targetY) || !std::isfinite(targetZ)) {
		FLog("[MAP_SCAN_TP64] invalid target=%.3f %.3f %.3f", targetX, targetY, targetZ);
		return false;
	}

	if (!pGame || !pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
		FLog("[MAP_SCAN_TP64] unavailable game=%p net=%p", pGame, pNetGame);
		return false;
	}

	CPlayerPed* playerPed = pGame->FindPlayerPed();
	if (!playerPed || !playerPed->m_pPed) {
		FLog("[MAP_SCAN_TP64] unavailable playerPed=%p", playerPed);
		return false;
	}

	const uint8_t safeArea = static_cast<uint8_t>(std::clamp(static_cast<int>(area), 0, 255));
	CVector target(targetX, targetY, targetZ);
	Xyron::Streaming::WorldStreamRequest streamRequest{};
	streamRequest.areaCode = safeArea;
	streamRequest.refreshGame = true;
	streamRequest.reason = "map-scan-teleport";
	Xyron::Streaming::PrepareWorldAtPoint(target, streamRequest);
	WarmKnownArm64CollisionSlotsForPosition(target, safeArea);
	Xyron::Streaming::PrepareWorldAtPoint(target, streamRequest);
	playerPed->SetInterior(safeArea, true);

	if (playerPed->IsInVehicle()) {
		playerPed->RemoveFromVehicleAndPutAt(targetX, targetY, targetZ);
	} else {
		playerPed->TeleportTo(targetX, targetY, targetZ);
	}

	if (playerPed->m_pPed) {
		playerPed->m_pPed->m_bHasContacted = false;
		playerPed->m_pPed->m_bHasHitWall = false;
		playerPed->m_pPed->m_bIsInSafePosition = false;
		playerPed->m_pPed->m_bUsesCollision = true;
		playerPed->m_pPed->physicalFlags.bCollidable = true;
		playerPed->m_pPed->physicalFlags.bCanBeCollidedWith = true;
		playerPed->m_pPed->physicalFlags.bDisableSimpleCollision = false;
		playerPed->m_pPed->physicalFlags.bSkipLineCol = false;
		playerPed->m_pPed->physicalFlags.bOnSolidSurface = false;
		playerPed->m_pPed->physicalFlags.bApplyGravity = true;
		playerPed->m_pPed->bIsStanding = false;
		playerPed->m_pPed->bWasStanding = false;
		playerPed->m_pPed->bIsInTheAir = true;
		playerPed->m_pPed->bIsLanding = false;
	}

	FLog("[MAP_SCAN_TP64] ok target=%.3f %.3f %.3f area=%u",
		 targetX,
		 targetY,
		 targetZ,
		 static_cast<unsigned>(safeArea));
	return true;
}

std::string EscapeMapProbeJsonString64(const char* value)
{
	std::string escaped;
	if (!value) {
		return escaped;
	}

	for (const unsigned char ch : std::string(value)) {
		switch (ch) {
			case '"':
				escaped += "\\\"";
				break;
			case '\\':
				escaped += "\\\\";
				break;
			case '\b':
				escaped += "\\b";
				break;
			case '\f':
				escaped += "\\f";
				break;
			case '\n':
				escaped += "\\n";
				break;
			case '\r':
				escaped += "\\r";
				break;
			case '\t':
				escaped += "\\t";
				break;
			default:
				if (ch < 0x20) {
					char control[8];
					std::snprintf(control, sizeof(control), "\\u%04x", static_cast<unsigned>(ch));
					escaped += control;
				} else {
					escaped.push_back(static_cast<char>(ch));
				}
				break;
		}
	}
	return escaped;
}

std::string BuildNativeMapScanProbeJson64(float x, float y, float z, int area)
{
	const float targetX = static_cast<float>(x);
	const float targetY = static_cast<float>(y);
	const float targetZ = static_cast<float>(z);
	const int gameState = pNetGame ? static_cast<int>(pNetGame->GetGameState()) : -1;
	if (!std::isfinite(targetX) || !std::isfinite(targetY) || !std::isfinite(targetZ)) {
		char invalidBuffer[384];
		std::snprintf(
				invalidBuffer,
				sizeof(invalidBuffer),
				"{\"status\":\"invalid_target\",\"nativeReady\":false,\"gameState\":%d,"
				"\"x\":%.3f,\"y\":%.3f,\"z\":%.3f}",
				gameState,
				targetX,
				targetY,
				targetZ);
		return invalidBuffer;
	}

	if (!pGame) {
		char unavailableBuffer[384];
		std::snprintf(
				unavailableBuffer,
				sizeof(unavailableBuffer),
				"{\"status\":\"game_unavailable\",\"nativeReady\":false,\"gameState\":%d,"
				"\"x\":%.3f,\"y\":%.3f,\"z\":%.3f}",
				gameState,
				targetX,
				targetY,
				targetZ);
		return unavailableBuffer;
	}
	if (!pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
		char notConnectedBuffer[384];
		std::snprintf(
				notConnectedBuffer,
				sizeof(notConnectedBuffer),
				"{\"status\":\"game_not_connected\",\"nativeReady\":false,\"gameState\":%d,"
				"\"x\":%.3f,\"y\":%.3f,\"z\":%.3f}",
				gameState,
				targetX,
				targetY,
				targetZ);
		return notConnectedBuffer;
	}

	const int safeAreaInt = std::clamp(static_cast<int>(area), 0, 255);
	const uint8_t safeArea = static_cast<uint8_t>(safeAreaInt);
	CVector streamTarget(targetX, targetY, targetZ < 18.0f ? 18.0f : targetZ);

	auto warmProbeTarget = [&]() {
		Xyron::Streaming::WorldStreamRequest streamRequest{};
		streamRequest.areaCode = safeAreaInt;
		streamRequest.refreshGame = true;
		streamRequest.reason = "map-scan-probe";
		Xyron::Streaming::PrepareWorldAtPoint(streamTarget, streamRequest);
		Xyron::Streaming::PrepareWorldAtPoint(streamTarget, streamRequest);
	};

	warmProbeTarget();
	bool forcedFullPass = false;
	bool collisionLoaded = Xyron::Collision::HasCollisionLoaded(streamTarget, safeAreaInt);

	float groundZ = pGame->FindGroundZForCoord(targetX, targetY, targetZ + 500.0f);
	if (!std::isfinite(groundZ)) {
		groundZ = -9999.0f;
	}

	const float originZ = std::max(targetZ + 220.0f, 900.0f);
	const float bottomZ = std::min(targetZ - 420.0f, -300.0f);
	CVector origin(targetX, targetY, originZ);
	CColPoint col{};
	CEntityGTA* entity = nullptr;
	CStoredCollPoly stored{};
	auto runVerticalProbe = [&]() {
		col = {};
		entity = nullptr;
		stored = {};
		return CWorld::ProcessVerticalLine(
				&origin,
				bottomZ,
				&col,
				&entity,
				true,
				true,
				false,
				true,
				true,
				false,
				&stored);
	};
	bool verticalHit = runVerticalProbe();

	float verticalZ = -9999.0f;
	float hitX = -9999.0f;
	float hitY = -9999.0f;
	int model = -1;
	int type = -1;
	int entityArea = -1;
	bool usesCollision = false;
	bool visible = false;
	bool hasRw = false;
	bool staticEntity = false;
	bool tempBuilding = false;
	bool procObject = false;
	if (verticalHit && std::isfinite(col.m_vecPoint.z)) {
		verticalZ = col.m_vecPoint.z;
		hitX = col.m_vecPoint.x;
		hitY = col.m_vecPoint.y;
	}
	if (entity) {
		model = static_cast<int>(entity->m_nModelIndex);
		type = static_cast<int>(entity->GetType());
		entityArea = static_cast<int>(entity->m_nAreaCode);
		usesCollision = entity->m_bUsesCollision;
		visible = entity->m_bIsVisible;
		hasRw = entity->m_pRwObject != nullptr;
		staticEntity = entity->m_bIsStatic;
		tempBuilding = entity->m_bIsTempBuilding;
		procObject = entity->m_bIsProcObject;
	}

	const float dz = verticalHit && groundZ > -9000.0f ? verticalZ - groundZ : -9999.0f;
	const bool usableCollision = verticalHit && (!entity || usesCollision);
	const char* status = usableCollision ? "ok" : "missing_floor_or_collision";
	if (verticalHit && entity && !usesCollision) {
		status = "hit_entity_without_collision_flag";
	} else if (verticalHit && entity && !visible && !hasRw) {
		status = "hit_invisible_or_unrendered_entity";
	} else if (!collisionLoaded) {
		status = verticalHit ? "hit_but_collision_slot_not_loaded" : "collision_slot_not_loaded";
	}

	char buffer[2048];
	std::snprintf(
			buffer,
			sizeof(buffer),
			"{\"status\":\"%s\",\"nativeReady\":true,\"gameState\":%d,"
			"\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"area\":%d,"
			"\"collisionLoaded\":%s,\"forcedFullPass\":%s,\"verticalHit\":%s,"
			"\"verticalZ\":%.3f,\"groundZ\":%.3f,\"dz\":%.3f,"
			"\"hitX\":%.3f,\"hitY\":%.3f,\"originZ\":%.3f,\"bottomZ\":%.3f,"
			"\"model\":%d,\"type\":%d,\"entityArea\":%d,"
			"\"usesCollision\":%s,\"visible\":%s,\"hasRw\":%s,"
			"\"staticEntity\":%s,\"tempBuilding\":%s,\"procObject\":%s}",
			status,
			gameState,
			targetX,
			targetY,
			targetZ,
			static_cast<int>(safeArea),
			collisionLoaded ? "true" : "false",
			forcedFullPass ? "true" : "false",
			verticalHit ? "true" : "false",
			verticalZ,
			groundZ,
			dz,
			hitX,
			hitY,
			originZ,
			bottomZ,
			model,
			type,
			entityArea,
			usesCollision ? "true" : "false",
			visible ? "true" : "false",
			hasRw ? "true" : "false",
			staticEntity ? "true" : "false",
			tempBuilding ? "true" : "false",
			procObject ? "true" : "false");

	FLog("[MAP_PROBE64] status=%s target=%.3f %.3f %.3f area=%d colLoaded=%d forced=%d hit=%d z=%.3f ground=%.3f model=%d type=%d uses=%d visible=%d hasRw=%d",
		 status,
		 targetX,
		 targetY,
		 targetZ,
		 safeAreaInt,
		 collisionLoaded ? 1 : 0,
		 forcedFullPass ? 1 : 0,
		 verticalHit ? 1 : 0,
		 verticalZ,
		 groundZ,
		 model,
		 type,
		 usesCollision ? 1 : 0,
		 visible ? 1 : 0,
		 hasRw ? 1 : 0);
	return buffer;
}

bool AppendQueuedMapScanProbeResult64(const char* zoneId, const std::string& nativeJson)
{
	if (!g_pszStorage || !g_pszStorage[0]) {
		FLog("[MAP_PROBE64] cannot append result: storage unavailable");
		return false;
	}

	char dir[1024];
	std::snprintf(dir, sizeof(dir), "%sSAMP", g_pszStorage);
	mkdir(dir, 0775);
	std::snprintf(dir, sizeof(dir), "%sSAMP/xyron_monitor", g_pszStorage);
	mkdir(dir, 0775);

	char path[1200];
	std::snprintf(path, sizeof(path), "%sSAMP/xyron_monitor/map_probe_results.jsonl", g_pszStorage);
	std::FILE* file = std::fopen(path, "ab");
	if (!file) {
		FLog("[MAP_PROBE64] cannot append result path=%s", path);
		return false;
	}

	std::string payload = nativeJson;
	if (payload.empty() || payload[0] != '{') {
		payload = "{\"status\":\"invalid_native_json\"}";
	}

	const long long wallTimeMs = static_cast<long long>(std::time(nullptr)) * 1000LL;
	const std::string line =
			std::string("{\"wallTimeMs\":") + std::to_string(wallTimeMs) +
			",\"zoneId\":\"" + EscapeMapProbeJsonString64(zoneId ? zoneId : "manual") +
			"\",\"native\":" + payload + "}\n";
	const size_t written = std::fwrite(line.data(), 1, line.size(), file);
	std::fclose(file);

	if (written != line.size()) {
		FLog("[MAP_PROBE64] partial result write path=%s written=%zu expected=%zu",
			 path,
			 written,
			 line.size());
		return false;
	}
	return true;
}
}

extern "C" void ProcessQueuedMapScanTeleport64()
{
	PendingMapScanTeleport64 job;
	{
		std::lock_guard<std::mutex> lock(g_mapScanTeleportMutex64);
		if (!g_pendingMapScanTeleport64.pending) {
			return;
		}
		job = g_pendingMapScanTeleport64;
		g_pendingMapScanTeleport64.pending = false;
	}

	const bool ok = ApplyNativeMapScanTeleportNow64(job.x, job.y, job.z, job.area);
	FLog("[MAP_SCAN_TP64] queued-executed target=%.3f %.3f %.3f area=%d ok=%d",
		 job.x,
		 job.y,
		 job.z,
		 job.area,
		 ok ? 1 : 0);
}

extern "C" void ProcessQueuedMapScanProbe64()
{
	PendingMapScanProbe64 job;
	{
		std::lock_guard<std::mutex> lock(g_mapScanProbeMutex64);
		if (!g_pendingMapScanProbe64.pending) {
			return;
		}
		job = g_pendingMapScanProbe64;
		g_pendingMapScanProbe64.pending = false;
	}

	const std::string nativeJson = BuildNativeMapScanProbeJson64(job.x, job.y, job.z, job.area);
	const bool appended = AppendQueuedMapScanProbeResult64(job.zoneId, nativeJson);
	FLog("[MAP_PROBE64] queued-executed zone=%s target=%.3f %.3f %.3f area=%d appended=%d",
		 job.zoneId[0] ? job.zoneId : "manual",
		 job.x,
		 job.y,
		 job.z,
		 job.area,
		 appended ? 1 : 0);
}
#endif

extern "C" {
	JNIEXPORT void JNICALL Java_com_xyron_game_main_ui_RadialMenu_sendCommand(JNIEnv* pEnv, jobject thiz, jbyteArray str)
	{
		jboolean isCopy = true;

		jbyte* pMsg = pEnv->GetByteArrayElements(str, &isCopy);
		jsize length = pEnv->GetArrayLength(str);

		std::string szStr((char*)pMsg, length);

		if (Xyron::AppCommand::TrySendAppCommandLine(szStr.c_str())) {
			pEnv->ReleaseByteArrayElements(str, pMsg, JNI_ABORT);
			return;
		}

		if(pNetGame) {
			pNetGame->SendChatCommand((char*)szStr.c_str());
		}

		pEnv->ReleaseByteArrayElements(str, pMsg, JNI_ABORT);
	}

	JNIEXPORT jboolean JNICALL Java_com_xyron_game_main_SAMP_isNativeGameConnected(JNIEnv* pEnv, jobject thiz)
	{
		return (pNetGame && pNetGame->GetGameState() == GAMESTATE_CONNECTED) ? JNI_TRUE : JNI_FALSE;
	}

	JNIEXPORT jboolean JNICALL Java_com_xyron_game_main_SAMP_isNativeLocalPlayerInVehicle(JNIEnv* pEnv, jobject thiz)
	{
		if (!pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
			return JNI_FALSE;
		}

		CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
		if (!pPlayerPool) {
			return JNI_FALSE;
		}

		CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
		CPlayerPed* pPlayerPed = pLocalPlayer ? pLocalPlayer->GetPlayerPed() : nullptr;
		if (!pPlayerPed || !pPlayerPed->m_pPed || pPlayerPed->IsDead()) {
			return JNI_FALSE;
		}

		return pPlayerPed->IsInVehicle() ? JNI_TRUE : JNI_FALSE;
	}

	JNIEXPORT jboolean JNICALL Java_com_xyron_game_main_SAMP_isNativePlayerSpawnReady(JNIEnv* pEnv, jobject thiz)
	{
		if (!pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
			return JNI_FALSE;
		}

		CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
		if (!pPlayerPool) {
			return JNI_FALSE;
		}

		CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
		CVector spawnPos;
		static uint32_t s_lastRuntimeSpawnRequestTick = 0;
		if (pLocalPlayer && !pLocalPlayer->m_bIsActive && !pLocalPlayer->m_bWaitingForSpawnRequestReply &&
			pLocalPlayer->TryGetSpawnPos(spawnPos)) {
			const uint32_t now = GetTickCount();
			if (now - s_lastRuntimeSpawnRequestTick >= 1200) {
				s_lastRuntimeSpawnRequestTick = now;
				FLog("[SPAWN_FLOW64] runtime-ready requesting spawn pos=%.2f %.2f %.2f",
					 spawnPos.x,
					 spawnPos.y,
					 spawnPos.z);
				pLocalPlayer->RequestSpawn();
				pLocalPlayer->m_bWaitingForSpawnRequestReply = true;
			}
		}

		if (!pLocalPlayer || !pLocalPlayer->m_bIsActive || pLocalPlayer->IsSpectating() ||
			pLocalPlayer->m_bWaitingForSpawnRequestReply) {
			return JNI_FALSE;
		}

		CPlayerPed* pPlayerPed = pLocalPlayer->GetPlayerPed();
		if (!pPlayerPed || pPlayerPed->IsDead()) {
			return JNI_FALSE;
		}

		return JNI_TRUE;
	}

	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_startVehicleSelfTest(
			JNIEnv* pEnv,
			jobject thiz,
			jint durationMs,
			jint steering,
			jboolean throttle,
			jboolean brake)
	{
		StartVehicleSelfTestInput(
				static_cast<int>(durationMs),
				static_cast<int>(steering),
				throttle == JNI_TRUE,
				brake == JNI_TRUE);
	}

	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_exitVehicleVisualSelfTest(JNIEnv* pEnv, jobject thiz)
	{
#if !VER_x32
		if (!pGame || !pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
			FLog("[VEH_EXIT64] unavailable game=%p net=%p", pGame, pNetGame);
			return;
		}

		CPlayerPed* playerPed = pGame->FindPlayerPed();
		if (!playerPed || !playerPed->m_pPed || !playerPed->IsInVehicle()) {
			FLog("[VEH_EXIT64] skip playerPed=%p inVeh=0", playerPed);
			return;
		}

		CVehicleGTA* vehicle = playerPed->GetGtaVehicle();
		CVector pos = vehicle ? vehicle->GetPosition() : playerPed->m_pPed->GetPosition();
		const uint32_t model = vehicle ? vehicle->GetModelId() : 0;
		playerPed->RemoveFromVehicleAndPutAt(pos.x + 4.0f, pos.y + 1.5f, pos.z + 1.0f);
		FLog("[VEH_EXIT64] detached model=%u vehicle=%p vehiclePos=%.2f %.2f %.2f",
			 model,
			 vehicle,
			 pos.x,
			 pos.y,
			 pos.z);
#else
		(void)pEnv;
		(void)thiz;
#endif
	}

	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_runWorldCollisionSelfTest(JNIEnv* pEnv, jobject thiz)
	{
#if !VER_x32
		if (!pGame || !pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
			FLog("[COL_SELFTEST64] unavailable game=%p net=%p", pGame, pNetGame);
			return;
		}

		CPlayerPed* playerPed = pGame->FindPlayerPed();
		if (!playerPed || !playerPed->m_pPed) {
			FLog("[COL_SELFTEST64] unavailable playerPed=%p", playerPed);
			return;
		}

		const CVector base = playerPed->m_pPed->GetPosition();
		struct ColSelfTestOffset {
			float x;
			float y;
		};
		static constexpr ColSelfTestOffset kOffsets[] = {
			{0.0f, 0.0f},
			{2.0f, 0.0f},
			{-2.0f, 0.0f},
			{0.0f, 2.0f},
			{0.0f, -2.0f},
			{6.0f, 0.0f},
			{-6.0f, 0.0f},
			{0.0f, 6.0f},
			{0.0f, -6.0f},
			{12.0f, 0.0f},
			{-12.0f, 0.0f},
			{0.0f, 12.0f},
			{0.0f, -12.0f},
		};

		int hits = 0;
		int misses = 0;
		FLog("[COL_SELFTEST64] start base=%.2f %.2f %.2f area=%d samples=%u",
			 base.x,
			 base.y,
			 base.z,
			 playerPed->m_pPed->m_nAreaCode,
			 static_cast<unsigned>(sizeof(kOffsets) / sizeof(kOffsets[0])));

		for (unsigned i = 0; i < sizeof(kOffsets) / sizeof(kOffsets[0]); ++i) {
			CVector origin(base.x + kOffsets[i].x, base.y + kOffsets[i].y, base.z + 8.0f);
			const float targetZ = base.z - 90.0f;
			CColPoint col{};
			CEntityGTA* entity = nullptr;
			CStoredCollPoly stored{};
			const bool hit = CWorld::ProcessVerticalLine(
				&origin,
				targetZ,
				&col,
				&entity,
				true,
				true,
				false,
				true,
				true,
				false,
				&stored);

			if (hit) {
				++hits;
			} else {
				++misses;
			}

			FLog("[COL_SELFTEST64] sample=%u pos=%.2f %.2f startZ=%.2f targetZ=%.2f hit=%d z=%.2f model=%d type=%d area=%d uses=%d",
				 i,
				 origin.x,
				 origin.y,
				 origin.z,
				 targetZ,
				 hit ? 1 : 0,
				 hit ? col.m_vecPoint.z : 0.0f,
				 entity ? entity->m_nModelIndex : -1,
				 entity ? static_cast<int>(entity->GetType()) : -1,
				 entity ? static_cast<int>(entity->m_nAreaCode) : -1,
				 entity && entity->m_bUsesCollision ? 1 : 0);
		}

		FLog("[COL_SELFTEST64] summary base=%.2f %.2f %.2f hits=%d misses=%d",
			 base.x,
			 base.y,
			 base.z,
			 hits,
			 misses);
#else
		FLog("[COL_SELFTEST64] skipped on 32-bit build");
#endif
	}

	JNIEXPORT jboolean JNICALL Java_com_xyron_game_main_SAMP_applyNativeMapScanTeleport(
			JNIEnv* pEnv,
			jobject thiz,
			jfloat x,
			jfloat y,
			jfloat z,
			jint area)
	{
#if !VER_x32
		(void)pEnv;
		(void)thiz;

		const float targetX = static_cast<float>(x);
		const float targetY = static_cast<float>(y);
		const float targetZ = static_cast<float>(z);
		if (!std::isfinite(targetX) || !std::isfinite(targetY) || !std::isfinite(targetZ)) {
			FLog("[MAP_SCAN_TP64] invalid target=%.3f %.3f %.3f", targetX, targetY, targetZ);
			return JNI_FALSE;
		}

		{
			std::lock_guard<std::mutex> lock(g_mapScanTeleportMutex64);
			g_pendingMapScanTeleport64.pending = true;
			g_pendingMapScanTeleport64.x = targetX;
			g_pendingMapScanTeleport64.y = targetY;
			g_pendingMapScanTeleport64.z = targetZ;
			g_pendingMapScanTeleport64.area = static_cast<int>(area);
		}

		FLog("[MAP_SCAN_TP64] queued target=%.3f %.3f %.3f area=%d",
			 targetX,
			 targetY,
			 targetZ,
			 static_cast<int>(area));
		return JNI_TRUE;
#else
		(void)pEnv;
		(void)thiz;
		(void)x;
		(void)y;
		(void)z;
		(void)area;
		FLog("[MAP_SCAN_TP64] skipped on 32-bit build");
		return JNI_FALSE;
#endif
	}

	JNIEXPORT jstring JNICALL Java_com_xyron_game_main_SAMP_runNativeMapScanProbe(
			JNIEnv* pEnv,
			jobject thiz,
			jfloat x,
			jfloat y,
			jfloat z,
			jint area)
	{
#if !VER_x32
		(void)thiz;
		const std::string nativeJson = BuildNativeMapScanProbeJson64(x, y, z, area);
		return pEnv->NewStringUTF(nativeJson.c_str());
#else
		(void)thiz;
		(void)x;
		(void)y;
		(void)z;
		(void)area;
		return pEnv->NewStringUTF("{\"status\":\"skipped_32bit\",\"nativeReady\":false}");
#endif
	}

	JNIEXPORT jboolean JNICALL Java_com_xyron_game_main_SAMP_queueNativeMapScanProbe(
			JNIEnv* pEnv,
			jobject thiz,
			jstring zoneId,
			jfloat x,
			jfloat y,
			jfloat z,
			jint area)
	{
#if !VER_x32
		(void)thiz;

		const float targetX = static_cast<float>(x);
		const float targetY = static_cast<float>(y);
		const float targetZ = static_cast<float>(z);
		if (!std::isfinite(targetX) || !std::isfinite(targetY) || !std::isfinite(targetZ)) {
			FLog("[MAP_PROBE64] invalid queued target=%.3f %.3f %.3f", targetX, targetY, targetZ);
			return JNI_FALSE;
		}

		const char* rawZoneId = zoneId ? pEnv->GetStringUTFChars(zoneId, nullptr) : nullptr;
		PendingMapScanProbe64 job;
		job.pending = true;
		job.x = targetX;
		job.y = targetY;
		job.z = targetZ;
		job.area = static_cast<int>(area);
		std::snprintf(job.zoneId, sizeof(job.zoneId), "%s", rawZoneId && rawZoneId[0] ? rawZoneId : "manual");
		if (rawZoneId) {
			pEnv->ReleaseStringUTFChars(zoneId, rawZoneId);
		}

		{
			std::lock_guard<std::mutex> lock(g_mapScanProbeMutex64);
			g_pendingMapScanProbe64 = job;
		}

		FLog("[MAP_PROBE64] queued zone=%s target=%.3f %.3f %.3f area=%d",
			 job.zoneId,
			 targetX,
			 targetY,
			 targetZ,
			 static_cast<int>(area));
		return JNI_TRUE;
#else
		(void)pEnv;
		(void)thiz;
		(void)zoneId;
		(void)x;
		(void)y;
		(void)z;
		(void)area;
		FLog("[MAP_PROBE64] queued probe skipped on 32-bit build");
		return JNI_FALSE;
#endif
	}
}

extern "C" {
	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_sendCommandV(JNIEnv* pEnv, jobject thiz, jbyteArray str)
	{
		jboolean isCopy = true;

		jbyte* pMsg = pEnv->GetByteArrayElements(str, &isCopy);
		jsize length = pEnv->GetArrayLength(str);

		std::string szStr((char*)pMsg, length);

		if (Xyron::AppCommand::TrySendAppCommandLine(szStr.c_str())) {
			pEnv->ReleaseByteArrayElements(str, pMsg, JNI_ABORT);
			return;
		}

		if(pNetGame) {
			pNetGame->SendChatCommand((char*)szStr.c_str());
		}

		pEnv->ReleaseByteArrayElements(str, pMsg, JNI_ABORT);
	}

	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_requestAppInventory(JNIEnv* pEnv, jobject thiz)
	{
		(void)pEnv;
		(void)thiz;
		Xyron::AppCommand::SendInventoryRequest();
	}
}
extern "C"
{
	JNIEXPORT void JNICALL
	Java_com_xyron_game_main_SAMP_ClickEnterPassengerButton(JNIEnv *env, jobject thiz) 
	{
		Xyron::Input::PressVehicleAction();
	}
}
extern "C"
{
	JNIEXPORT void JNICALL
	Java_com_xyron_game_main_SAMP_ClickLockVehicleButton(JNIEnv *env, jobject thiz) 
	{
		Xyron::Input::PressVehicleLockAction();
	}
}

extern "C"
{
	JNIEXPORT void JNICALL
	Java_com_xyron_game_main_SAMP_setNativeActionButtonState(JNIEnv *env, jobject thiz, jint action, jboolean pressed)
	{
		switch (action)
		{
			case 1:
				if (pressed) {
					Xyron::Input::PressVehicleAction();
				}
				break;
			case 10:
				if (pressed) {
					Xyron::Input::PressCameraAction();
				}
				break;
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
				Xyron::Input::SetHudActionState(action, pressed == JNI_TRUE);
				break;
			default:
				break;
		}
	}
}

extern "C"
{
	JNIEXPORT void JNICALL
	Java_com_xyron_game_main_SAMP_setNativeAnalogState(JNIEnv *env, jobject thiz, jint leftRight, jint upDown)
	{
		Xyron::Input::SetHudAnalogState(leftRight, upDown);
	}
}

extern "C"
{
	JNIEXPORT void JNICALL
	Java_com_xyron_game_main_SAMP_addNativeCameraLookDelta(JNIEnv *env, jobject thiz, jfloat deltaX, jfloat deltaY, jint screenWidth, jint screenHeight)
	{
		Xyron::Input::AddCameraLookDelta(deltaX, deltaY, screenWidth, screenHeight);
	}
}

extern "C"
{
	JNIEXPORT void JNICALL
	Java_com_xyron_game_main_SAMP_resetNativeSourceControlState(JNIEnv *env, jobject thiz)
	{
		Xyron::Input::ResetHudControls();
	}
}

extern "C" {
	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_onClickButton(JNIEnv* pEnv, jobject thiz, jint action)
	{
		switch(action) 
		{
			case 1: // Y
			{
				Xyron::Input::PressSourceButton(Xyron::Input::SourceButton::Yes);
				break;
			}
			case 2: // F
			{
				Xyron::Input::PressSourceButton(Xyron::Input::SourceButton::SecondaryAttack);
				break;
			}
		}
	}		
}

extern "C"
{
	JNIEXPORT void JNICALL
	Java_com_xyron_game_main_SAMP_MostrarChat(JNIEnv *env, jobject thiz) 
	{
		if (pUI) {
			pUI->chat()->setVisible(true);
			Mchat = true;
		}
	}
}

extern "C"
{
	JNIEXPORT void JNICALL
	Java_com_xyron_game_main_SAMP_AbrirChatBotao(JNIEnv *env, jobject thiz)
	{
		if (pUI) {
			pUI->chat()->setVisible(true);
			if (pUI->keyboard()) {
				pUI->keyboard()->show(pUI->chat());
			}
			Mchat = true;
		}
	}

	JNIEXPORT void JNICALL
	Java_com_xyron_game_main_SAMP_OcultarChatBotao(JNIEnv *env, jobject thiz)
	{
		if (pUI) {
			pUI->chat()->setVisible(false);
			Mchat = false;
		}
	}

	JNIEXPORT void JNICALL
	Java_com_xyron_game_main_SAMP_togglePlayer(JNIEnv *env, jobject thiz, jint toggle)
	{
		if (pGame && pGame->FindPlayerPed()) {
			pGame->FindPlayerPed()->TogglePlayerControllable(toggle != 0);
		}
	}
}

/*
		if (Tab == 0) {
			if (pUI) { pUI->playertablist()->show(); Tab = 1; }
		}
		else {
			Tab = 0;
		} 
*/
