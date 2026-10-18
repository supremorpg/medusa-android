#include <jni.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "../main.h"
#include "../settings.h"
#include "../game/game.h"
#include "../game/util.h"
#include "../game/SampStreamingApi.h"
#include "../game/Models/ModelInfo.h"
#include "../game/Renderer.h"
#include "../game/World.h"
#include "../net/netgame.h"
#include "../perf/PerformanceProfiler.h"
#include "../voice_new/Plugin.h"

extern "C" JNIEXPORT void JNICALL
Java_com_raiferoleplay_game_game_SAMP_onInputEnd(JNIEnv *env, jobject thiz, jbyteArray str);

extern "C" JNIEXPORT void JNICALL
Java_com_raiferoleplay_game_game_ui_AttachEdit_Exit(JNIEnv *env, jobject thiz);

extern "C" JNIEXPORT void JNICALL
Java_com_raiferoleplay_game_game_ui_AttachEdit_AttachClick(
        JNIEnv *env,
        jobject thiz,
        jint buttonType,
        jboolean buttonId);

extern "C" JNIEXPORT void JNICALL
Java_com_raiferoleplay_game_game_ui_AttachEdit_Save(JNIEnv *env, jobject thiz);

extern "C" JNIEXPORT void JNICALL
Java_com_raiferoleplay_game_game_ui_dialog_DialogManager_sendDialogResponse(
        JNIEnv *env,
        jobject thiz,
        jint buttonId,
        jint dialogId,
        jint listItem,
        jbyteArray input);

namespace {
    std::atomic<int> g_nativeHudFps {0};
    int g_nativeOverlayState = 0;
    bool g_nativeVoipEnabled = true;
    bool g_allowNextNativePauseMenu = true;

    const char *ReadJString(JNIEnv *env, jstring value) {
        if (!value) {
            return nullptr;
        }
        return env->GetStringUTFChars(value, nullptr);
    }

    void ReleaseJString(JNIEnv *env, jstring value, const char *chars) {
        if (value && chars) {
            env->ReleaseStringUTFChars(value, chars);
        }
    }

    jstring NewRuntimeMonitorStatus(
            JNIEnv *env,
            const char *reason,
            int gameState,
            int nativeFps) {
        char buffer[256];
        std::snprintf(
                buffer,
                sizeof(buffer),
                "{\"nativeReady\":false,\"reason\":\"%s\",\"gameState\":%d,\"nativeFps\":%d}",
                reason ? reason : "unknown",
                gameState,
                nativeFps);
        return env->NewStringUTF(buffer);
    }

    int GetCurrentNativeHudFps() {
        int fps = g_nativeHudFps.load(std::memory_order_relaxed);
        if (fps <= 0) {
            Xyron::Perf::Snapshot snapshot = Xyron::Perf::GetSnapshot();
            fps = static_cast<int>(snapshot.fps);
        }
        return std::clamp(fps, 0, 240);
    }

    bool IsReliableRuntimePosition(const CVector& pos) {
        return std::isfinite(pos.x) &&
               std::isfinite(pos.y) &&
               std::isfinite(pos.z) &&
               !(std::fabs(pos.x) < 0.025f &&
                 std::fabs(pos.y) < 0.025f &&
                 std::fabs(pos.z) < 0.025f) &&
               pos.z > -1000.0f &&
               pos.z < 3000.0f;
    }

    bool IsRuntimeWeaponSupported(int weaponId) {
        return weaponId == 0 ||
               (weaponId >= 1 && weaponId <= 18) ||
               (weaponId >= 22 && weaponId <= 46);
    }

    CVector ResolveRuntimeMonitorPosition(
            const CVector& primary,
            const CVector& secondary,
            const char* tag) {
        static bool s_hasLastGoodPosition = false;
        static CVector s_lastGoodPosition{};
        static uint32_t s_lastGoodPositionTick = 0;
        static uint32_t s_lastPositionFixLogTick = 0;

        const uint32_t now = GetTickCount();
        if (IsReliableRuntimePosition(primary)) {
            s_hasLastGoodPosition = true;
            s_lastGoodPosition = primary;
            s_lastGoodPositionTick = now;
            return primary;
        }

        if (IsReliableRuntimePosition(secondary)) {
            s_hasLastGoodPosition = true;
            s_lastGoodPosition = secondary;
            s_lastGoodPositionTick = now;
            if (now - s_lastPositionFixLogTick > 1200) {
                s_lastPositionFixLogTick = now;
                FLog("[MON_POS_FIX64] tag=%s primary=%.3f %.3f %.3f secondary=%.3f %.3f %.3f",
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
            if (now - s_lastPositionFixLogTick > 1200) {
                s_lastPositionFixLogTick = now;
                FLog("[MON_POS_FIX64] tag=%s primary=%.3f %.3f %.3f source=last last=%.3f %.3f %.3f age=%u",
                     tag ? tag : "?",
                     primary.x,
                     primary.y,
                     primary.z,
                     s_lastGoodPosition.x,
                     s_lastGoodPosition.y,
                     s_lastGoodPosition.z,
                     now - s_lastGoodPositionTick);
            }
            return s_lastGoodPosition;
        }

        return primary;
    }

    struct RuntimeCollisionProbe {
        bool hit = false;
        int modelId = -1;
        int type = -1;
        int colSlot = -1;
        int surfaceA = -1;
        int surfaceB = -1;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float distance = 0.0f;
        float depth = 0.0f;
        bool usesCollision = false;
        bool visible = false;
        bool hasRw = false;
        bool hasCol = false;
        bool hasColData = false;
        bool breakable = false;
        char modelName[22] = {};
    };

    static char ProbeLowerAscii(char value) {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
    }

    static bool ProbeNameContains(const char* haystack, const char* needle) {
        if (!haystack || !needle || !needle[0]) {
            return false;
        }
        for (int i = 0; haystack[i]; ++i) {
            int h = i;
            int n = 0;
            while (needle[n] && haystack[h] &&
                   ProbeLowerAscii(haystack[h]) == ProbeLowerAscii(needle[n])) {
                ++h;
                ++n;
            }
            if (!needle[n]) {
                return true;
            }
        }
        return false;
    }

    static void CopyProbeModelName(char (&dst)[22], CBaseModelInfo* modelInfo) {
        if (!modelInfo) {
            std::snprintf(dst, sizeof(dst), "?");
            return;
        }
        int out = 0;
        for (; out < 21; ++out) {
            const char value = modelInfo->m_modelName[out];
            if (!value) {
                break;
            }
            dst[out] = (value == '"' || value == '\\' || static_cast<unsigned char>(value) < 32)
                    ? '_'
                    : value;
        }
        dst[out] = '\0';
        if (out == 0) {
            std::snprintf(dst, sizeof(dst), "?");
        }
    }

    static bool IsBreakableProbeModel(int modelId) {
        switch (modelId) {
            case 1223: // lampost_coast
            case 1262: // MTraffic4
            case 1263: // MTraffic3
            case 1283: // MTraffic1
            case 1284: // MTraffic2
            case 1295: // doublestreetlght1
            case 1306: // tlgraphpolegen
            case 1307: // telgrphpoleall
            case 1308: // telgrphpole02
            case 1315: // trafficlight1
            case 1350: // cj_traffic_light4
            case 1351: // cj_traffic_light5
            case 1352: // cj_traffic_light3
            case 1411: // DYN_MESH_1
            case 1412: // DYN_MESH_2
            case 1413: // DYN_MESH_3
            case 1447: // DYN_MESH_4
            case 1468: // DYN_MESH_05
            case 1226: // lamppost3
            case 1290: // lamppost2
            case 1294: // mlamppost
            case 1297: // lamppost1
            case 3665: // AirYelrm_LAS
            case 3666: // Airuntest_las
            case 5032: // las_runsigns
            case 5044: // las_runsignsx
                return true;
            default:
                return modelId >= 4995 && modelId <= 5000;
        }
    }

    static bool IsBreakableProbeName(const char* name) {
        return ProbeNameContains(name, "lamp") ||
               ProbeNameContains(name, "light") ||
               ProbeNameContains(name, "pole") ||
               ProbeNameContains(name, "post") ||
               ProbeNameContains(name, "sign") ||
               ProbeNameContains(name, "signal") ||
               ProbeNameContains(name, "fence") ||
               ProbeNameContains(name, "barrier") ||
               ProbeNameContains(name, "mtraffic") ||
               ProbeNameContains(name, "trafficlight") ||
               ProbeNameContains(name, "traffic_light") ||
               ProbeNameContains(name, "telgrph") ||
               ProbeNameContains(name, "dyn_mesh") ||
               ProbeNameContains(name, "airyelrm") ||
               ProbeNameContains(name, "airuntest") ||
               ProbeNameContains(name, "runsign");
    }

    static bool NormalizeProbeDirection2D(CVector& direction) {
        direction.z = 0.0f;
        const float mag2 = direction.x * direction.x + direction.y * direction.y;
        if (!std::isfinite(mag2) || mag2 < 0.0001f) {
            return false;
        }
        const float invMag = 1.0f / std::sqrt(mag2);
        direction.x *= invMag;
        direction.y *= invMag;
        return true;
    }

    static float ProbeDistance3D(const CVector& a, const CVector& b) {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float dz = b.z - a.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    static float ProbeDistancePointToSegment2D(const CVector& point, const CVector& start, const CVector& end) {
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float len2 = dx * dx + dy * dy;
        if (len2 < 0.0001f) {
            const float px = point.x - start.x;
            const float py = point.y - start.y;
            return std::sqrt(px * px + py * py);
        }
        float t = ((point.x - start.x) * dx + (point.y - start.y) * dy) / len2;
        t = std::clamp(t, 0.0f, 1.0f);
        const float cx = start.x + dx * t;
        const float cy = start.y + dy * t;
        const float px = point.x - cx;
        const float py = point.y - cy;
        return std::sqrt(px * px + py * py);
    }

    static bool FillProbeFromEntity(
            RuntimeCollisionProbe& probe,
            CEntityGTA* hitEntity,
            const CColPoint* col,
            const CVector& start) {
        if (!hitEntity) {
            return false;
        }

        probe.hit = true;
        probe.modelId = static_cast<int>(hitEntity->m_nModelIndex);
        probe.type = static_cast<int>(hitEntity->GetType());
        probe.usesCollision = hitEntity->m_bUsesCollision;
        probe.visible = hitEntity->m_bIsVisible;
        probe.hasRw = hitEntity->m_pRwObject != nullptr;

        CBaseModelInfo* modelInfo =
                probe.modelId >= 0 && probe.modelId < CModelInfo::NUM_MODEL_INFOS
                        ? CModelInfo::GetModelInfo(probe.modelId)
                        : nullptr;
        CopyProbeModelName(probe.modelName, modelInfo);
        CColModel* colModel = modelInfo ? modelInfo->m_pColModel : nullptr;
        probe.hasCol = colModel != nullptr;
        probe.hasColData = colModel && colModel->m_pColData;
        probe.colSlot = colModel ? static_cast<int>(colModel->m_nColSlot) : -1;
        probe.breakable = IsBreakableProbeModel(probe.modelId) ||
                          IsBreakableProbeName(probe.modelName) ||
                          (modelInfo && (modelInfo->IsGlass() ||
                                         modelInfo->IsGarageDoor() ||
                                         modelInfo->IsBreakableStatuePart()));

        if (col) {
            probe.x = col->m_vecPoint.x;
            probe.y = col->m_vecPoint.y;
            probe.z = col->m_vecPoint.z;
            probe.surfaceA = static_cast<int>(col->m_nSurfaceTypeA);
            probe.surfaceB = static_cast<int>(col->m_nSurfaceTypeB);
            probe.depth = col->m_fDepth;
            probe.distance = ProbeDistance3D(start, col->m_vecPoint);
        } else {
            const CVector& pos = hitEntity->GetPosition();
            probe.x = pos.x;
            probe.y = pos.y;
            probe.z = pos.z;
            probe.distance = ProbeDistance3D(start, pos);
        }
        return true;
    }

    static RuntimeCollisionProbe RunLineProbe(const CVector& start, const CVector& end) {
        RuntimeCollisionProbe probe{};
        CColPoint col{};
        CEntityGTA* hitEntity = nullptr;
        const bool hit = CWorld::ProcessLineOfSight(
                &start,
                &end,
                &col,
                &hitEntity,
                true,
                false,
                false,
                true,
                true,
                false,
                false,
                false);
        if (hit) {
            FillProbeFromEntity(probe, hitEntity, &col, start);
        }
        return probe;
    }

    static RuntimeCollisionProbe RunVisiblePathProbe(const CVector& start, const CVector& end) {
        RuntimeCollisionProbe probe{};
        CEntityGTA* bestEntity = nullptr;
        float bestScore = 999999.0f;
        const int count = std::clamp(CRenderer::ms_nNoOfVisibleEntities, 0, static_cast<int>(MAX_VISIBLE_ENTITY_PTRS));
        for (int i = 0; i < count; ++i) {
            CEntityGTA* entity = CRenderer::ms_aVisibleEntityPtrs[i];
            if (!entity || entity->IsVehicle() || entity->IsPed()) {
                continue;
            }
            const int modelId = static_cast<int>(entity->m_nModelIndex);
            if (modelId < 0 || modelId >= CModelInfo::NUM_MODEL_INFOS) {
                continue;
            }
            CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
            CColModel* colModel = modelInfo ? modelInfo->m_pColModel : nullptr;
            if (!modelInfo || !colModel) {
                continue;
            }
            CVector center = entity->GetPosition();
            center.x += colModel->m_boundSphere.m_vecCenter.x;
            center.y += colModel->m_boundSphere.m_vecCenter.y;
            center.z += colModel->m_boundSphere.m_vecCenter.z;
            if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)) {
                continue;
            }
            float radius = colModel->GetBoundRadius();
            if (!std::isfinite(radius) || radius <= 0.05f) {
                radius = 1.0f;
            }
            radius = std::clamp(radius, 0.5f, 12.0f);
            const float horizontalDistance = ProbeDistancePointToSegment2D(center, start, end);
            const float verticalDistance = std::fabs(center.z - ((start.z + end.z) * 0.5f));
            const float allowance = radius + 1.35f;
            if (horizontalDistance > allowance || verticalDistance > radius + 4.0f) {
                continue;
            }
            char safeModelName[22] = {};
            CopyProbeModelName(safeModelName, modelInfo);
            const bool suspicious =
                    !entity->m_bUsesCollision ||
                    !colModel->m_pColData ||
                    IsBreakableProbeModel(modelId) ||
                    IsBreakableProbeName(safeModelName);
            if (!suspicious) {
                continue;
            }
            const float score = horizontalDistance - radius + verticalDistance * 0.12f;
            if (score < bestScore) {
                bestScore = score;
                bestEntity = entity;
            }
        }

        if (bestEntity) {
            FillProbeFromEntity(probe, bestEntity, nullptr, start);
            probe.distance = bestScore;
        }
        return probe;
    }
}

extern bool g_bHudMapEnabled;
extern CSettings* pSettings;
extern CGame* pGame;
extern CNetGame* pNetGame;

void ApplyFPSPatch(uint8_t fps);
extern "C" void XyronApplyNativeChatArea(
        int left,
        int top,
        int width,
        int height,
        int screenWidth,
        int screenHeight);
extern "C" void XyronApplyNativeMenuRuntimeSettings(int chatMaxMessages);
extern "C" void XyronApplyNativeRenderRuntimeSettings(
        int renderDistance,
        bool shadowsEnabled,
        bool effectsEnabled);

extern "C" void XyronNativeFrameTick() {
    using clock = std::chrono::steady_clock;

    static auto lastSample = clock::now();
    static int frameCount = 0;

    frameCount++;
    auto now = clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSample).count();
    if (elapsedMs < 500) {
        return;
    }

    int fps = static_cast<int>((frameCount * 1000 + elapsedMs / 2) / elapsedMs);
    g_nativeHudFps.store(std::clamp(fps, 0, 240), std::memory_order_relaxed);
    frameCount = 0;
    lastSample = now;
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_SAMP_requestReconnect(JNIEnv *, jobject) {
}

extern "C" JNIEXPORT jint JNICALL
Java_com_xyron_game_main_SAMP_getNativeHudFps(JNIEnv *, jobject) {
    return GetCurrentNativeHudFps();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_xyron_game_main_SAMP_getNativeRuntimeMonitorSnapshotJson(JNIEnv *env, jobject) {
    const int nativeFps = GetCurrentNativeHudFps();
    const int gameState = pNetGame ? pNetGame->GetGameState() : -1;

    if (!pNetGame) {
        return NewRuntimeMonitorStatus(env, "net_unavailable", gameState, nativeFps);
    }

    if (gameState != GAMESTATE_CONNECTED) {
        return NewRuntimeMonitorStatus(env, "not_connected", gameState, nativeFps);
    }

    if (!pGame) {
        return NewRuntimeMonitorStatus(env, "game_unavailable", gameState, nativeFps);
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    if (!playerPool) {
        return NewRuntimeMonitorStatus(env, "player_pool_unavailable", gameState, nativeFps);
    }

    CLocalPlayer* localPlayer = playerPool->GetLocalPlayer();
    if (!localPlayer || !localPlayer->m_bIsActive) {
        return NewRuntimeMonitorStatus(env, "local_player_inactive", gameState, nativeFps);
    }

    CPlayerPed* playerPed = localPlayer->GetPlayerPed();
    if (!playerPed || !playerPed->m_pPed) {
        return NewRuntimeMonitorStatus(env, "player_unavailable", gameState, nativeFps);
    }

    CPedGTA* ped = playerPed->m_pPed;
    CVehicleGTA* vehicle = playerPed->IsInVehicle() ? playerPed->GetGtaVehicle() : nullptr;
    CPhysical* physical = vehicle ? static_cast<CPhysical*>(vehicle) : static_cast<CPhysical*>(ped);
    CEntityGTA* entity = static_cast<CEntityGTA*>(physical);

    const CVector rawPos = physical->GetPosition();
    const CVector rawPedPos = ped->GetPosition();
    const CVector pos = ResolveRuntimeMonitorPosition(rawPos, rawPedPos, "entity");
    const CVector pedPos = ResolveRuntimeMonitorPosition(rawPedPos, pos, "ped");
    const CVector& speed = physical->GetMoveSpeed();
    const bool positionFinite = std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z);
    float groundZ = pGame->FindGroundZForCoord(pos.x, pos.y, pos.z + 3.0f);
    if (!std::isfinite(groundZ)) {
        groundZ = -9999.0f;
    }
    const float dz = positionFinite ? pos.z - groundZ : 0.0f;
    const float speed2 = speed.x * speed.x + speed.y * speed.y + speed.z * speed.z;

    static uintptr_t s_lastProbePhysical = 0;
    static CVector s_lastProbePos{};
    static uint32_t s_lastProbeTick = 0;
    static bool s_hasLastProbePos = false;

    RuntimeCollisionProbe sweptProbe{};
    RuntimeCollisionProbe forwardProbe{};
    RuntimeCollisionProbe visualProbe{};
    const uint32_t probeTick = GetTickCount();
    const uintptr_t physicalKey = reinterpret_cast<uintptr_t>(physical);
    if (positionFinite && physicalKey != 0) {
        const CVector currentPos(pos.x, pos.y, pos.z);
        const float probeHeight = vehicle ? 1.10f : 0.90f;
        const float minSwept2 = vehicle ? 0.20f : 0.035f;

        if (s_hasLastProbePos && s_lastProbePhysical == physicalKey) {
            const uint32_t elapsed = probeTick - s_lastProbeTick;
            const float dx = currentPos.x - s_lastProbePos.x;
            const float dy = currentPos.y - s_lastProbePos.y;
            const float dzMove = currentPos.z - s_lastProbePos.z;
            const float moved2 = dx * dx + dy * dy + dzMove * dzMove;
            if (elapsed >= 60 && elapsed <= 2500 && moved2 >= minSwept2 && moved2 <= 900.0f) {
                const CVector sweptStart(s_lastProbePos.x, s_lastProbePos.y, s_lastProbePos.z + probeHeight);
                const CVector sweptEnd(currentPos.x, currentPos.y, currentPos.z + probeHeight);
                sweptProbe = RunLineProbe(sweptStart, sweptEnd);
                visualProbe = RunVisiblePathProbe(sweptStart, sweptEnd);
            }
        }

        CVector direction(speed.x, speed.y, speed.z);
        if (!NormalizeProbeDirection2D(direction)) {
            direction = entity->GetForwardVector();
            NormalizeProbeDirection2D(direction);
        }
        if (std::isfinite(direction.x) && std::isfinite(direction.y) &&
            (direction.x * direction.x + direction.y * direction.y) > 0.5f) {
            const float forwardDistance = vehicle ? 8.0f : 3.0f;
            const CVector forwardStart(currentPos.x, currentPos.y, currentPos.z + probeHeight);
            const CVector forwardEnd(
                    currentPos.x + direction.x * forwardDistance,
                    currentPos.y + direction.y * forwardDistance,
                    currentPos.z + probeHeight);
            forwardProbe = RunLineProbe(forwardStart, forwardEnd);
            if (!visualProbe.hit) {
                visualProbe = RunVisiblePathProbe(forwardStart, forwardEnd);
            }
        }

        s_hasLastProbePos = true;
        s_lastProbePhysical = physicalKey;
        s_lastProbePos = currentPos;
        s_lastProbeTick = probeTick;
    } else {
        s_hasLastProbePos = false;
        s_lastProbePhysical = 0;
        s_lastProbeTick = 0;
    }

    CVector origin(pos.x, pos.y, pos.z + 8.0f);
    CColPoint col{};
    CEntityGTA* hitEntity = nullptr;
    CStoredCollPoly stored{};
    bool verticalHit = false;
    float verticalZ = -9999.0f;
    int verticalModel = -1;
    int verticalType = -1;
    if (positionFinite) {
        verticalHit = CWorld::ProcessVerticalLine(
                &origin,
                pos.z - 130.0f,
                &col,
                &hitEntity,
                true,
                true,
                false,
                true,
                true,
                false,
                &stored);
        if (verticalHit && std::isfinite(col.m_vecPoint.z)) {
            verticalZ = col.m_vecPoint.z;
        }
        if (hitEntity) {
            verticalModel = static_cast<int>(hitEntity->m_nModelIndex);
            verticalType = static_cast<int>(hitEntity->GetType());
        }
    }

    char buffer[6144];
    std::snprintf(
            buffer,
            sizeof(buffer),
            "{\"nativeReady\":true,\"gameState\":%d,\"nativeFps\":%d,"
            "\"streamMemUsed\":%zu,\"streamMemAvailable\":%zu,"
            "\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,"
            "\"pedX\":%.3f,\"pedY\":%.3f,\"pedZ\":%.3f,"
            "\"groundZ\":%.3f,\"dz\":%.3f,"
            "\"vx\":%.5f,\"vy\":%.5f,\"vz\":%.5f,\"speed2\":%.6f,"
            "\"area\":%d,\"inVehicle\":%s,\"vehicleModel\":%d,"
            "\"usesCollision\":%s,\"collidable\":%s,\"canBeCollidedWith\":%s,"
            "\"simpleCollision\":%s,\"applyGravity\":%s,"
            "\"onGround\":%s,\"inAir\":%s,\"positionFinite\":%s,"
            "\"verticalHit\":%s,\"verticalZ\":%.3f,\"verticalModel\":%d,\"verticalType\":%d,"
            "\"sweptHit\":%s,\"sweptModel\":%d,\"sweptType\":%d,\"sweptName\":\"%s\","
            "\"sweptX\":%.3f,\"sweptY\":%.3f,\"sweptZ\":%.3f,\"sweptDistance\":%.3f,"
            "\"sweptUsesCollision\":%s,\"sweptVisible\":%s,\"sweptHasRw\":%s,"
            "\"sweptHasCol\":%s,\"sweptHasColData\":%s,\"sweptColSlot\":%d,"
            "\"sweptBreakable\":%s,\"sweptSurfaceA\":%d,\"sweptSurfaceB\":%d,\"sweptDepth\":%.3f,"
            "\"forwardHit\":%s,\"forwardModel\":%d,\"forwardType\":%d,\"forwardName\":\"%s\","
            "\"forwardX\":%.3f,\"forwardY\":%.3f,\"forwardZ\":%.3f,\"forwardDistance\":%.3f,"
            "\"forwardUsesCollision\":%s,\"forwardVisible\":%s,\"forwardHasRw\":%s,"
            "\"forwardHasCol\":%s,\"forwardHasColData\":%s,\"forwardColSlot\":%d,"
            "\"forwardBreakable\":%s,\"forwardSurfaceA\":%d,\"forwardSurfaceB\":%d,\"forwardDepth\":%.3f,"
            "\"visualProbeHit\":%s,\"visualProbeModel\":%d,\"visualProbeType\":%d,\"visualProbeName\":\"%s\","
            "\"visualProbeX\":%.3f,\"visualProbeY\":%.3f,\"visualProbeZ\":%.3f,\"visualProbeDistance\":%.3f,"
            "\"visualProbeUsesCollision\":%s,\"visualProbeVisible\":%s,\"visualProbeHasRw\":%s,"
            "\"visualProbeHasCol\":%s,\"visualProbeHasColData\":%s,\"visualProbeColSlot\":%d,"
            "\"visualProbeBreakable\":%s}",
            gameState,
            nativeFps,
            Xyron::Streaming::MemoryUsed(),
            Xyron::Streaming::MemoryAvailable(),
            pos.x,
            pos.y,
            pos.z,
            pedPos.x,
            pedPos.y,
            pedPos.z,
            groundZ,
            dz,
            speed.x,
            speed.y,
            speed.z,
            speed2,
            static_cast<int>(ped->m_nAreaCode),
            vehicle ? "true" : "false",
            vehicle ? static_cast<int>(vehicle->m_nModelIndex) : -1,
            entity->m_bUsesCollision ? "true" : "false",
            physical->physicalFlags.bCollidable ? "true" : "false",
            physical->physicalFlags.bCanBeCollidedWith ? "true" : "false",
            physical->physicalFlags.bDisableSimpleCollision ? "false" : "true",
            physical->physicalFlags.bApplyGravity ? "true" : "false",
            playerPed->IsOnGround() ? "true" : "false",
            ped->bIsInTheAir ? "true" : "false",
            positionFinite ? "true" : "false",
            verticalHit ? "true" : "false",
            verticalZ,
            verticalModel,
            verticalType,
            sweptProbe.hit ? "true" : "false",
            sweptProbe.modelId,
            sweptProbe.type,
            sweptProbe.modelName,
            sweptProbe.x,
            sweptProbe.y,
            sweptProbe.z,
            sweptProbe.distance,
            sweptProbe.usesCollision ? "true" : "false",
            sweptProbe.visible ? "true" : "false",
            sweptProbe.hasRw ? "true" : "false",
            sweptProbe.hasCol ? "true" : "false",
            sweptProbe.hasColData ? "true" : "false",
            sweptProbe.colSlot,
            sweptProbe.breakable ? "true" : "false",
            sweptProbe.surfaceA,
            sweptProbe.surfaceB,
            sweptProbe.depth,
            forwardProbe.hit ? "true" : "false",
            forwardProbe.modelId,
            forwardProbe.type,
            forwardProbe.modelName,
            forwardProbe.x,
            forwardProbe.y,
            forwardProbe.z,
            forwardProbe.distance,
            forwardProbe.usesCollision ? "true" : "false",
            forwardProbe.visible ? "true" : "false",
            forwardProbe.hasRw ? "true" : "false",
            forwardProbe.hasCol ? "true" : "false",
            forwardProbe.hasColData ? "true" : "false",
            forwardProbe.colSlot,
            forwardProbe.breakable ? "true" : "false",
            forwardProbe.surfaceA,
            forwardProbe.surfaceB,
            forwardProbe.depth,
            visualProbe.hit ? "true" : "false",
            visualProbe.modelId,
            visualProbe.type,
            visualProbe.modelName,
            visualProbe.x,
            visualProbe.y,
            visualProbe.z,
            visualProbe.distance,
            visualProbe.usesCollision ? "true" : "false",
            visualProbe.visible ? "true" : "false",
            visualProbe.hasRw ? "true" : "false",
            visualProbe.hasCol ? "true" : "false",
            visualProbe.hasColData ? "true" : "false",
            visualProbe.colSlot,
            visualProbe.breakable ? "true" : "false");
    return env->NewStringUTF(buffer);
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_SAMP_setNativeVoipEnabled(JNIEnv *, jobject, jboolean enabled) {
    g_nativeVoipEnabled = (enabled == JNI_TRUE);
    if (pSettings && !pSettings->Get().bVoiceChatEnable) {
        g_nativeVoipEnabled = false;
        Plugin::SetInputRecordStatus(false);
        return;
    }
    Plugin::SetInputRecordStatus(g_nativeVoipEnabled);
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_SAMP_applyNativeVoiceChatLayout(JNIEnv *, jobject, jfloat posX, jfloat posY, jfloat sizeScale) {
    Plugin::SetVoiceButtonLayout(
            std::clamp(static_cast<float>(posX), 0.0f, 1.0f),
            std::clamp(static_cast<float>(posY), 0.0f, 1.0f),
            std::clamp(static_cast<float>(sizeScale), 0.65f, 1.85f));
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_SAMP_resetNativeVoiceChatLayout(JNIEnv *, jobject) {
    Plugin::ResetVoiceButtonLayout();
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_SAMP_setNativeRadarEnabled(JNIEnv *, jobject, jboolean enabled) {
    g_bHudMapEnabled = (enabled == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_SAMP_setHotReloadWeaponAudioActive(JNIEnv *, jobject, jint, jboolean) {
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_SAMP_applyNativeMenuSettings(JNIEnv *, jobject, jboolean androidKeyboardEnabled, jint chatMaxMessages, jboolean voiceChatEnabled) {
    const bool voiceMasterEnabled = (voiceChatEnabled == JNI_TRUE);
    if (!voiceMasterEnabled) {
        g_nativeVoipEnabled = false;
        Plugin::SetInputRecordStatus(false);
    }
    int safeChatMaxMessages = std::clamp(static_cast<int>(chatMaxMessages), 1, 30);
    if (!pSettings) {
        XyronApplyNativeMenuRuntimeSettings(safeChatMaxMessages);
        return;
    }

    pSettings->Get().iAndroidKeyboard = (androidKeyboardEnabled == JNI_TRUE);
    pSettings->Get().iChatMaxMessages = safeChatMaxMessages;
    pSettings->Get().bVoiceChatEnable = voiceMasterEnabled;
    XyronApplyNativeMenuRuntimeSettings(safeChatMaxMessages);
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_SAMP_applyNativeChatArea(
        JNIEnv *,
        jobject,
        jint left,
        jint top,
        jint width,
        jint height,
        jint screenWidth,
        jint screenHeight) {
    XyronApplyNativeChatArea(
            static_cast<int>(left),
            static_cast<int>(top),
            static_cast<int>(width),
            static_cast<int>(height),
            static_cast<int>(screenWidth),
            static_cast<int>(screenHeight));
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_SAMP_giveWeaponToLocalPlayer(JNIEnv *, jobject, jint weaponId, jint ammo) {
    const int safeWeapon = IsRuntimeWeaponSupported(static_cast<int>(weaponId))
                           ? static_cast<int>(weaponId)
                           : 0;
    const int safeAmmo = std::clamp(static_cast<int>(ammo), 0, 9999);

    CGame::PostToMainThread([safeWeapon, safeAmmo]() {
        if (!pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED || !pGame) {
            FLog("[WEAPON_TEST64] skip weapon=%d reason=not-ready", safeWeapon);
            return;
        }

        CPlayerPool* playerPool = pNetGame->GetPlayerPool();
        CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
        CPlayerPed* playerPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;
        if (!localPlayer || !localPlayer->m_bIsActive || localPlayer->IsSpectating() ||
            !playerPed || !playerPed->m_pPed || !GamePool_Ped_GetAt(playerPed->m_dwGTAId) ||
            !IsValidGamePed(playerPed->m_pPed)) {
            FLog("[WEAPON_TEST64] skip weapon=%d reason=invalid-ped", safeWeapon);
            return;
        }

        if (safeWeapon == 0) {
            playerPed->SetArmedWeapon(0, false);
            FLog("[WEAPON_TEST64] unarmed ok");
            return;
        }

        playerPed->GiveWeapon(safeWeapon, safeAmmo);
        CWeapon* slot = playerPed->FindWeaponSlot(static_cast<uint8_t>(safeWeapon));
        if (!slot || slot->dwType != static_cast<eWeaponType>(safeWeapon)) {
            FLog("[WEAPON_TEST64] give failed weapon=%d ammo=%d", safeWeapon, safeAmmo);
            return;
        }

        playerPed->SetArmedWeapon(static_cast<uint8_t>(safeWeapon), false);
        FLog("[WEAPON_TEST64] give ok weapon=%d ammo=%d", safeWeapon, safeAmmo);
    });
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_SAMP_setLocalPlayerSkin(JNIEnv *, jobject, jint) {
}

extern "C" JNIEXPORT jint JNICALL
Java_com_xyron_game_main_SAMP_applyHotReloadTexture(
        JNIEnv *env,
        jobject,
        jstring textureName,
        jstring targetGroup,
        jint,
        jstring stagedFilePath,
        jstring format,
        jint,
        jint) {
    const char *texture = ReadJString(env, textureName);
    const char *group = ReadJString(env, targetGroup);
    const char *path = ReadJString(env, stagedFilePath);
    const char *fmt = ReadJString(env, format);
    ReleaseJString(env, textureName, texture);
    ReleaseJString(env, targetGroup, group);
    ReleaseJString(env, stagedFilePath, path);
    ReleaseJString(env, format, fmt);
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_xyron_game_main_SAMP_applyHotReloadBitmap(
        JNIEnv *,
        jobject,
        jstring,
        jstring,
        jint,
        jbyteArray,
        jint,
        jint) {
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_xyron_game_main_SAMP_restoreHotReloadTexture(JNIEnv *, jobject, jstring, jstring, jint) {
    return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_SAMP_applyNativeRenderSettings(JNIEnv *, jobject, jint fpsLimit, jint renderDistance, jboolean shadowsEnabled, jboolean effectsEnabled) {
    int safeFpsLimit = std::clamp(static_cast<int>(fpsLimit), 30, 120);
    if (pSettings) {
        pSettings->Get().iFPSCount = safeFpsLimit;
    }
    if (g_libGTASA != 0) {
        ApplyFPSPatch(static_cast<uint8_t>(safeFpsLimit));
    }
    XyronApplyNativeRenderRuntimeSettings(
            static_cast<int>(renderDistance),
            shadowsEnabled == JNI_TRUE,
            effectsEnabled == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_ui_AttachEdit_Exit(JNIEnv *env, jobject thiz) {
    Java_com_raiferoleplay_game_game_ui_AttachEdit_Exit(env, thiz);
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_ui_AttachEdit_AttachClick(
        JNIEnv *env,
        jobject thiz,
        jint buttonType,
        jboolean buttonId) {
    Java_com_raiferoleplay_game_game_ui_AttachEdit_AttachClick(env, thiz, buttonType, buttonId);
}

extern "C" JNIEXPORT void JNICALL
Java_com_xyron_game_main_ui_AttachEdit_Save(JNIEnv *env, jobject thiz) {
    Java_com_raiferoleplay_game_game_ui_AttachEdit_Save(env, thiz);
}
