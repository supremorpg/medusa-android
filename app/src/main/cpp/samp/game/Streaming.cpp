//
// Created by x1y2z on 11.01.2023.
//

#include "Streaming.h"
#include "StreamingInfo.h"
#include "game/Models/ModelInfo.h"
#include "game/Animation/AnimManager.h"
#include "TxdStore.h"
#include "game.h"
#include "net/netgame.h"
#include "game/Collision/ColStore.h"
#include "game/Enums/eAreaCodes.h"
#include "IplStore.h"
#include "Camera.h"
#include "GTASAEngineApi.h"
#include "GTASAEngineStreamingBindings.h"
#include "Renderer.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <vector>
#include <iostream>

extern UI *pUI;

namespace {
int32 OpenImageArchiveWithFallback(const char* requestedName, const char** openedName, bool bNotPlayerImg)
{
    struct CandidateSet {
        const char* requested;
        const char* candidates[4];
    };

    static constexpr CandidateSet kFallbacks[] = {
        {"TEXDB\\SAMPCOL.IMG", {"texdb/SAMPCOL.img", "texdb/sampcol.img", "TEXDB/SAMPCOL.IMG", nullptr}},
        {"TEXDB\\SAMP.IMG", {"texdb/samp.img", "texdb/SAMP.img", "TEXDB/SAMP.IMG", nullptr}},
        {"TEXDB\\GTA3.IMG", {"texdb/gta3.img", "texdb/GTA3.img", "TEXDB/GTA3.IMG", nullptr}},
        {"TEXDB\\GTA_INT.IMG", {"texdb/gta_int.img", "texdb/GTA_INT.img", "TEXDB/GTA_INT.IMG", nullptr}},
    };

    int32 handle = CdStreamOpen(requestedName, bNotPlayerImg);
    if (handle >= 0) {
        FLog("[STREAM_FIX64] image open ok requested=%s handle=%d", requestedName, handle);
        if (openedName) {
            *openedName = requestedName;
        }
        return handle;
    }
    FLog("[STREAM_FIX64] image open miss requested=%s", requestedName);

    for (const auto& set : kFallbacks) {
        if (strcmp(set.requested, requestedName) != 0) {
            continue;
        }

        for (const char* candidate : set.candidates) {
            if (!candidate) {
                break;
            }
            handle = CdStreamOpen(candidate, bNotPlayerImg);
            if (handle >= 0) {
                FLog("[STREAM_FIX64] image fallback requested=%s candidate=%s handle=%d",
                     requestedName,
                     candidate,
                     handle);
                if (openedName) {
                    *openedName = candidate;
                }
                return handle;
            }
        }
        break;
    }

    if (openedName) {
        *openedName = requestedName;
    }
    FLog("[STREAM_FIX64] image open failed requested=%s", requestedName);
    return handle;
}

#if !VER_x32
constexpr size_t kArm64StreamingMemoryBudget = 384u * 1024u * 1024u;
constexpr size_t kArm64StreamingMemoryHardBudget = 448u * 1024u * 1024u;
std::atomic<int> g_xyronStreamingRenderDistance {80};
std::atomic<bool> g_xyronStreamingEffectsEnabled {true};

void RequestSceneObjectsAroundPlayer64Impl(const CVector& playerPos, const CVector& velocity)
{
    static uint32_t s_lastSceneRequestTick = 0;
    static uint32_t s_lastHeavySceneTick = 0;
    static uint32_t s_lastSceneLogTick = 0;
    static uint32_t s_lastStreamingEnableLogTick = 0;
    static uint32_t s_lastBlockingSceneLoadTick = 0;
    static CVector s_lastHeavyScenePos(0.0f, 0.0f, 0.0f);

    const uint32_t now = GetTickCount();
    const float speedSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    const uint32_t minSceneRequestIntervalMs =
        speedSq > 9.0f ? 450u :
        (speedSq > 1.0f ? 650u : 1200u);
    if (now - s_lastSceneRequestTick < minSceneRequestIntervalMs) {
        return;
    }
    s_lastSceneRequestTick = now;

    const int renderDistance = std::clamp(
        g_xyronStreamingRenderDistance.load(std::memory_order_relaxed),
        35,
        220
    );
    const bool effectsEnabled = g_xyronStreamingEffectsEnabled.load(std::memory_order_relaxed);

    GTASAEngineApi::ForceStreamingEnabled("obj-stream64");

    const float aheadScale = speedSq > 0.10f
        ? std::clamp(renderDistance * (speedSq > 9.0f ? 0.55f : 0.35f), 18.0f, 72.0f)
        : 0.0f;
    const CVector ahead(
        playerPos.x + velocity.x * aheadScale,
        playerPos.y + velocity.y * aheadScale,
        playerPos.z + velocity.z * 8.0f
    );
    const float movedX = playerPos.x - s_lastHeavyScenePos.x;
    const float movedY = playerPos.y - s_lastHeavyScenePos.y;
    const float movedZ = playerPos.z - s_lastHeavyScenePos.z;
    const float movedSq = movedX * movedX + movedY * movedY + movedZ * movedZ;
    const float movedHeavyThreshold = std::clamp(renderDistance * 0.22f, 14.0f, 32.0f);
    const bool stationaryNearLastHeavy =
        speedSq <= 0.10f &&
        movedSq <= (movedHeavyThreshold * movedHeavyThreshold);
    const uint32_t heavySceneIntervalMs =
        stationaryNearLastHeavy ? 15000u :
        (speedSq > 9.0f ? 1300u :
         (renderDistance <= 50 || !effectsEnabled ? 1800u :
          (renderDistance >= 120 ? 1100u : 1400u)));
    const bool movedOutsideHeavyAnchor = movedSq > (movedHeavyThreshold * movedHeavyThreshold);
    const uint32_t movedHeavyIntervalMs = speedSq > 9.0f ? 1200u : 2200u;
    const bool heavyScene = s_lastHeavySceneTick == 0 ||
                            now - s_lastHeavySceneTick >= heavySceneIntervalMs ||
                            (movedOutsideHeavyAnchor && now - s_lastHeavySceneTick >= movedHeavyIntervalMs);

    int requestCount = 0;
    auto requestAt = [&requestCount, heavyScene](const CVector& sample, bool allowLoadScene) {
        requestCount++;
        CVector streamSample(sample.x, sample.y, sample.z < 18.0f ? 18.0f : sample.z);

        GTASAEngineApi::WorldStreamRequest request{};
        request.areaCode = AREA_CODE_NORMAL_WORLD;
        request.streamingFlags = STREAMING_DEFAULT;
        request.addLods = false;
        request.loadScene = heavyScene && allowLoadScene;
        request.reason = "obj-stream64";
        GTASAEngineApi::PrepareWorldAtPoint(streamSample, request);
    };

    const float kNearStep = std::clamp(renderDistance * 0.55f, 36.0f, 72.0f);
    requestAt(playerPos, true);
    requestAt(CVector(playerPos.x + kNearStep, playerPos.y, playerPos.z), false);
    requestAt(CVector(playerPos.x - kNearStep, playerPos.y, playerPos.z), false);
    requestAt(CVector(playerPos.x, playerPos.y + kNearStep, playerPos.z), false);
    requestAt(CVector(playerPos.x, playerPos.y - kNearStep, playerPos.z), false);
    if (heavyScene) {
        const bool requestDiagonalRing = effectsEnabled || renderDistance >= 70 || speedSq > 4.0f;
        if (requestDiagonalRing) {
            requestAt(CVector(playerPos.x + kNearStep, playerPos.y + kNearStep, playerPos.z), false);
            requestAt(CVector(playerPos.x + kNearStep, playerPos.y - kNearStep, playerPos.z), false);
            requestAt(CVector(playerPos.x - kNearStep, playerPos.y + kNearStep, playerPos.z), false);
            requestAt(CVector(playerPos.x - kNearStep, playerPos.y - kNearStep, playerPos.z), false);
        }

        if (renderDistance >= 55 || speedSq > 9.0f) {
            const float kFarCardinalStep = std::clamp(renderDistance * 2.4f, 120.0f, 340.0f);
            const float kFarDiagonalStep = std::clamp(renderDistance * 1.7f, 90.0f, 240.0f);
            requestAt(CVector(playerPos.x + kFarCardinalStep, playerPos.y, playerPos.z), false);
            requestAt(CVector(playerPos.x - kFarCardinalStep, playerPos.y, playerPos.z), false);
            requestAt(CVector(playerPos.x, playerPos.y + kFarCardinalStep, playerPos.z), false);
            requestAt(CVector(playerPos.x, playerPos.y - kFarCardinalStep, playerPos.z), false);
            if (requestDiagonalRing) {
                requestAt(CVector(playerPos.x + kFarDiagonalStep, playerPos.y + kFarDiagonalStep, playerPos.z), false);
                requestAt(CVector(playerPos.x + kFarDiagonalStep, playerPos.y - kFarDiagonalStep, playerPos.z), false);
                requestAt(CVector(playerPos.x - kFarDiagonalStep, playerPos.y + kFarDiagonalStep, playerPos.z), false);
                requestAt(CVector(playerPos.x - kFarDiagonalStep, playerPos.y - kFarDiagonalStep, playerPos.z), false);
            }
        }
    }
    if (aheadScale > 0.0f) {
        requestAt(ahead, true);
    }

    const bool canSpendBlockingLoad =
        heavyScene &&
        speedSq > 9.0f &&
        now - s_lastBlockingSceneLoadTick >= 1800u;
    if (canSpendBlockingLoad) {
        s_lastBlockingSceneLoadTick = now;
        CStreaming::LoadAllRequestedModels(false);
    } else {
        CStreaming::LoadRequestedModels();
    }
    if (heavyScene) {
        s_lastHeavySceneTick = now;
        s_lastHeavyScenePos = playerPos;
    }
    GTASAEngineApi::ForceStreamingEnabled("obj-stream64-after-request");

    const bool importantSceneLog =
        heavyScene ||
        speedSq > 4.0f ||
        requestCount > 12 ||
        CStreaming::ms_disableStreaming;
    const uint32_t sceneLogInterval = importantSceneLog ? 6000u : 12000u;
    if (now - s_lastSceneLogTick >= sceneLogInterval) {
        s_lastSceneLogTick = now;
        FLog("[OBJ_STREAM64] scene-request pos=%.2f %.2f %.2f vel2=%.3f requested=%d heavy=%d dist=%d effects=%d moved2=%.2f streamingDisabled=%d",
             playerPos.x,
             playerPos.y,
             playerPos.z,
             speedSq,
             requestCount,
             heavyScene ? 1 : 0,
             renderDistance,
             effectsEnabled ? 1 : 0,
             movedSq,
             CStreaming::ms_disableStreaming ? 1 : 0);
    }
}
#endif
}

extern "C" void XyronApplyStreamingRuntimeProfile(int renderDistance, bool effectsEnabled)
{
#if !VER_x32
    g_xyronStreamingRenderDistance.store(std::clamp(renderDistance, 35, 220), std::memory_order_relaxed);
    g_xyronStreamingEffectsEnabled.store(effectsEnabled, std::memory_order_relaxed);
#else
    (void)renderDistance;
    (void)effectsEnabled;
#endif
}

bool CStreaming::TryLoadModel(int modelId) {
    FLog("TryLoadModel %d", modelId);
    if(!CStreaming::GetInfo(modelId).IsLoaded()) {
        FLog("TryLoadModel 1");
        CStreaming::RequestModel(modelId, STREAMING_GAME_REQUIRED | STREAMING_KEEP_IN_MEMORY);
        FLog("TryLoadModel 11");
        CStreaming::LoadAllRequestedModels(false);
        FLog("TryLoadModel 2");
        uint32 count = 0;
        while (!CStreaming::GetInfo(modelId).IsLoaded()) {
            count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (count > 30) {
                pUI->chat()->addDebugMessage("{ff0000} Error loading model %d", modelId);
                return false;
            }
        }
        FLog("TryLoadModel 3");
    }
    FLog("TryLoadModel 4");
    return true;
}

// Finishes loading all channels. (So both channels will be `IDLE` after it returns)
// Blocking. (Calls `CdStreamSync`)
void CStreaming::FlushChannels()
{
    GTASAEngineApi::FlushStreamingChannels();
}

void CStreaming::ClearFlagForAll(uint32 streamingFlag) {
    for (auto i = 0; i < RESOURCE_ID_TOTAL; i++) {
        GetInfo(i).ClearFlags(streamingFlag);
    }
}

bool CStreaming::AreAnimsUsedByRequestedModels(int32 animModelId) {
    for (auto info = ms_pStartRequestedList->GetNext(); info != ms_pEndRequestedList; info = info->GetNext()) {
        const auto modelId = GetModelFromInfo(info);
        if (IsModelDFF(modelId) && CModelInfo::GetModelInfo(modelId)->GetAnimFileIndex() == animModelId)
            return true;
    }

    for (auto & channel : ms_channel) {
        for (const auto& modelId : channel.modelIds) {
            if (modelId != MODEL_INVALID && IsModelDFF(modelId) &&
                CModelInfo::GetModelInfo(modelId)->GetAnimFileIndex() == animModelId
                    ) {
                return true;
            }
        }
    }

    return false;
}

void CStreaming::RemoveTxdModel(int32 modelId) {
    RemoveModel(TXDToModelId(modelId));
}

void CStreaming::RemoveModelIfNoRefs(int32 modelId) {
    CStreamingInfo& streamingInfo = GetInfo(modelId);
    if(streamingInfo.IsLoaded() && !CModelInfo::GetModelInfo(modelId)->m_nRefCount) {
        RemoveModel(modelId);

        streamingInfo.ClearAllFlags();
    }
}

void CStreaming::SetModelIsDeletable(int32 modelId) {
    GTASAEngineApi::SetModelIsDeletable(modelId);
}

void CStreaming::RemoveModel(int32 modelId) {
    if(modelId == MODEL_MALE01)
        return;

    CStreamingInfo& streamingInfo = GetInfo(modelId);
    if (streamingInfo.m_nLoadState == LOADSTATE_NOT_LOADED)
        return;

    if (streamingInfo.IsLoaded()) {
        switch (GetModelType(modelId)) {
            case eModelType::DFF: {
                CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
                modelInfo->DeleteRwObject();
                break;
            }
            case eModelType::TXD: {
                CTxdStore::RemoveTxd(ModelIdToTXD(modelId));
                break;
            }
            case eModelType::COL: {
                CColStore::RemoveCol(ModelIdToCOL(modelId));
                break;
            }
            case eModelType::IPL: {
                CIplStore::RemoveIpl(ModelIdToIPL(modelId));
                break;
            }
            case eModelType::DAT: {
              //  ThePaths.UnLoadPathFindData(ModelIdToDAT(modelId));
                break;
            }
            case eModelType::IFP: {
                CAnimManager::RemoveAnimBlock(ModelIdToIFP(modelId));
                break;
            }
            case eModelType::SCM: {
             //   CTheScripts::StreamedScripts.RemoveStreamedScriptFromMemory(ModelIdToSCM(modelId));
                break;
            }
        }
        ms_memoryUsed -= STREAMING_SECTOR_SIZE * streamingInfo.GetCdSize();
    }

    if (streamingInfo.InList()) {
        if (streamingInfo.IsRequested()) {
            ms_numModelsRequested--;
            if (streamingInfo.IsPriorityRequest()) {
                streamingInfo.ClearFlags(STREAMING_PRIORITY_REQUEST);
                ms_numPriorityRequests--;
            }
        }
        streamingInfo.RemoveFromList();
    } else if (streamingInfo.IsBeingRead()) {
        for (auto& ch : ms_channel) {
            for (auto& mId : ch.modelIds) {
                if (mId == modelId) {
                    mId = MODEL_INVALID;
                }
            }
        }
    }

    if (streamingInfo.IsLoadingFinishing()) {
        switch (GetModelType(modelId)) {
            case eModelType::DFF:
                RpClumpGtaCancelStream();
                break;
            case eModelType::TXD:
                CTxdStore::RemoveTxd(ModelIdToTXD(modelId));
                break;
            case eModelType::COL:
                CColStore::RemoveCol(ModelIdToCOL(modelId));
                break;
            case eModelType::IPL:
                CIplStore::RemoveIpl(ModelIdToIPL(modelId));
                break;
            case eModelType::IFP:
                CAnimManager::RemoveAnimBlock(ModelIdToIFP(modelId));
                break;
            case eModelType::SCM:
             //   CTheScripts::StreamedScripts.RemoveStreamedScriptFromMemory(ModelIdToSCM(modelId));
                break;
        }
    }

    streamingInfo.m_nLoadState = LOADSTATE_NOT_LOADED;
}

void CStreaming::RemoveBigBuildings() {
    /*for (auto i = GetBuildingPool()->GetSize() - 1; i >= 0; i--) {
        CBuilding* building = GetBuildingPool()->GetAt(i);
        if (building && building->m_bIsBIGBuilding && !building->m_bImBeingRendered) {
            building->DeleteRwObject();
            if (!CModelInfo::GetModelInfo(building->m_nModelIndex)->m_nRefCount)
                RemoveModel(building->m_nModelIndex);
        }
    }*/
}

void CStreaming::RemoveBuildingsNotInArea(eAreaCodes areaCode) {
    GTASAEngineApi::RemoveBuildingsNotInArea(static_cast<int32>(areaCode));
}

void CStreaming::InjectHooks() {

    GTASAEngineApi::BindStreamingGlobals();

    GTASAEngineApi::InstallStreamingHooksNative(
        reinterpret_cast<uintptr_t>(&CStreaming::InitImageList),
        reinterpret_cast<uintptr_t>(&CStreaming::MakeSpaceFor),
#if !VER_x32
        reinterpret_cast<uintptr_t>(&CStreaming::DeleteAllRwObjects)
#else
        0
#endif
    );
#if !VER_x32
    FLog("[STREAM_GUARD64] DeleteAllRwObjects guard installed");
#endif
    //CHook::Redirect("_ZN10CStreaming22LoadAllRequestedModelsEb", &CStreaming::LoadAllRequestedModels);
}

int CStreaming::AddImageToList(char const* pFileName, bool bNotPlayerImg) {
    // find a free slot
    std::int32_t fileIndex = 0;
    for (; fileIndex < TOTAL_IMG_ARCHIVES; fileIndex++) {
        if (!ms_files[fileIndex].m_szName[0])
            break;
    }

    if (fileIndex >= TOTAL_IMG_ARCHIVES) {
        assert("AddImageToList");
    }

    // free slot found, load the IMG file
    const char* openedName = pFileName;
    ms_files[fileIndex].m_StreamHandle = OpenImageArchiveWithFallback(pFileName, &openedName, bNotPlayerImg);
    strcpy(ms_files[fileIndex].m_szName, openedName);
    ms_files[fileIndex].m_bNotPlayerImg = bNotPlayerImg;
    return fileIndex;
}

void CStreaming::InitImageList() {
    for (auto & ms_file : ms_files) {
        ms_file.m_szName[0] = 0;
        ms_file.m_StreamHandle = 0;
    }

#if VER_SAMP
    CStreaming::AddImageToList("TEXDB\\SAMPCOL.IMG", true);
    CStreaming::AddImageToList("TEXDB\\GTA3.IMG", true);
    CStreaming::AddImageToList("TEXDB\\GTA_INT.IMG", true);
    CStreaming::AddImageToList("TEXDB\\SAMP.IMG", true);
#else
    CStreaming::AddImageToList("TEXDB\\GTA3.IMG", true);
    CStreaming::AddImageToList("TEXDB\\GTA_INT.IMG", true);
    CStreaming::AddImageToList("TEXDB\\SAMP.IMG", true);
    CStreaming::AddImageToList("TEXDB\\SAMPCOL.IMG", true);
#endif
}

// Request a given model to be loaded.
// Can be called on an already requested model to add `PRIORITY_REQUEST` flag
void CStreaming::RequestModel(int32 modelId, int32 streamingFlags) {
#if !VER_x32
    // The local reimplementation skips TXD dependency handling. On arm64 that
    // can leave custom objects with missing/blue textures, so use the native
    // streamer just like LoadAllRequestedModels does.
    GTASAEngineApi::RequestModelNativeArm64(modelId, streamingFlags);
    return;
#endif

    CStreamingInfo& info = GetInfo(modelId);

    switch (info.m_nLoadState) {
        case eStreamingLoadState::LOADSTATE_NOT_LOADED:
            break;

        case eStreamingLoadState::LOADSTATE_REQUESTED: {
            // Model already requested, just set priority request flag if not set already
            if ((streamingFlags & STREAMING_PRIORITY_REQUEST) && !info.IsPriorityRequest())
            {
                ++ms_numPriorityRequests;
                info.SetFlags(STREAMING_PRIORITY_REQUEST);
            }
            break;
        }

        default: {
            streamingFlags &= ~STREAMING_PRIORITY_REQUEST; // Remove flag otherwise
            break;
        }
    }
    info.SetFlags(streamingFlags);

    // BUG: Possibly? If the model was requested once with `PRIORITY_REQUEST` set
    //      and later is requested without it `ms_numPriorityRequests` won't be decreased.

    switch (info.m_nLoadState) {
        case eStreamingLoadState::LOADSTATE_LOADED: {
            if (info.InList()) {
                info.RemoveFromList();
                if (IsModelDFF(modelId)) {
                    switch (CModelInfo::GetModelInfo(modelId)->GetModelType()) {
                        case MODEL_INFO_PED:
                        case MODEL_INFO_VEHICLE: {
                            return;
                        }
                    }
                }

                if (!info.IsMissionOrGameRequired())
                    info.AddToList(ms_startLoadedList);
            }
            break;
        }
        case eStreamingLoadState::LOADSTATE_READING:
        case eStreamingLoadState::LOADSTATE_REQUESTED:
        case eStreamingLoadState::LOADSTATE_FINISHING:
            break;

        case eStreamingLoadState::LOADSTATE_NOT_LOADED: {
            switch (GetModelType(modelId)) {
                case eModelType::TXD: { // Request parent (if any) TXD
                    info.m_nLoadState = LOADSTATE_LOADED;
                    return;
                }
                case eModelType::DFF: { // Request TXD and (if any) IFP
                    CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
                  //  RequestTxdModel(modelInfo->m_nTxdIndex, streamingFlags);

                    const int32 animFileIndex = modelInfo->GetAnimFileIndex();
                    if (animFileIndex != -1)
                        RequestModel(IFPToModelId(animFileIndex), STREAMING_KEEP_IN_MEMORY);
                    break;
                }
            }
            info.AddToList(ms_pStartRequestedList);

            ++ms_numModelsRequested;
            if (streamingFlags & STREAMING_PRIORITY_REQUEST)
                ++ms_numPriorityRequests;

            info.SetFlags(streamingFlags);
            info.m_nLoadState = LOADSTATE_REQUESTED;
            break;
        }
    }
}

void CStreaming::AddLodsToRequestList(const CVector* point, int32 streamingFlags) {
    if (!point) {
        return;
    }
    GTASAEngineApi::AddLodsToRequestList(*point, streamingFlags);
}

void CStreaming::AddModelsToRequestList(const CVector* point, int32 streamingFlags) {
    if (!point) {
        return;
    }
    GTASAEngineApi::AddModelsToRequestList(*point, streamingFlags);
}

#include "Textures/TextureDatabaseRuntime.h"
extern CNetGame *pNetGame;
void CStreaming::Update() {

    if (CTimer::GetIsPaused())
        return;

    if(!CStreaming::GetInfo(MODEL_MALE01).IsLoaded()) {
        RequestModel(MODEL_MALE01, STREAMING_KEEP_IN_MEMORY);
        CStreaming::LoadAllRequestedModels(false);
    }
    CModelInfo::GetModelInfo(MODEL_MALE01)->m_nRefCount = 999;

#if VER_x32
    if (CTimer::m_snTimeInMillisecondsNonClipped % 100 == 0) {
        RemoveLeastUsedModel(STREAMING_KEEP_IN_MEMORY);
    }
#else
    static uint32_t s_lastArm64CleanupTick = 0;
    const uint32_t now = GetTickCount();
    if (ms_memoryAvailable < kArm64StreamingMemoryBudget) {
        ms_memoryAvailable = kArm64StreamingMemoryBudget;
    } else if (ms_memoryAvailable > kArm64StreamingMemoryHardBudget) {
        ms_memoryAvailable = kArm64StreamingMemoryHardBudget;
    }

    const size_t softLimit = (ms_memoryAvailable * 7u) / 10u;
    const size_t hardLimit = (ms_memoryAvailable * 9u) / 10u;
    const bool overSoftLimit = ms_memoryUsed > softLimit;
    const bool overHardLimit = ms_memoryUsed > hardLimit;
    if ((overSoftLimit && now - s_lastArm64CleanupTick >= 350u) ||
        (overHardLimit && now - s_lastArm64CleanupTick >= 120u)) {
        s_lastArm64CleanupTick = now;
        const int maxRemovals = overHardLimit ? 8 : 3;
        for (int i = 0; i < maxRemovals && ms_memoryUsed > softLimit; ++i) {
            const size_t before = ms_memoryUsed;
            if (!RemoveLeastUsedModel(STREAMING_DONTREMOVE_IN_LOADSCENE) ||
                before == ms_memoryUsed) {
                break;
            }
        }
    }
#endif


    static double previousTime{};
    const double currentTimeInSeconds = CTimer::m_snTimeInMillisecondsNonClipped / 1000.0;
    const double deltaTime = currentTimeInSeconds - previousTime;
    previousTime = currentTimeInSeconds;
    const double clampedDeltaTime = std::min(0.1, deltaTime);
    TextureDatabaseRuntime::UpdateStreaming(clampedDeltaTime, true);

    CCamera& TheCamera = GTASAEngineApi::Camera();

    const auto& camPos = TheCamera.GetPosition();
    const bool multiplayerPlayerAnchoredStreaming =
        pNetGame && pNetGame->GetGameState() == GAMESTATE_CONNECTED;
    const float fCamDistanceToGroundZ = camPos.z - TheCamera.CalculateGroundHeight(eGroundHeightType::ENTITY_BB_BOTTOM);
    if (!multiplayerPlayerAnchoredStreaming && !ms_disableStreaming && !CRenderer::m_loadingPriority) {
        if (fCamDistanceToGroundZ >= 50.0f) {
            if (CGame::CanSeeOutSideFromCurrArea()) {
                AddLodsToRequestList(&camPos, 0);
            }
        }
        else if (CRenderer::ms_bRenderOutsideTunnels) {
            AddModelsToRequestList(&camPos, 0);
        }
    }

//    if (CTimer::GetFrameCounter() % 128 == 106) {
//        m_bBoatsNeeded = false;
//        if (camPos.z < 500.0f) {
//            m_bBoatsNeeded = ThePaths.IsWaterNodeNearby(camPos, 80.0f);
//        }
//    }
    if (!pNetGame || !pNetGame->GetPlayerPool() || !pNetGame->GetPlayerPool()->GetLocalPlayer())
        return;

    auto* localPlayerPed = pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed();
    if (!localPlayerPed || !localPlayerPed->m_pPed) {
        return;
    }

    auto pLocalPed = localPlayerPed->m_pPed;
    const CVector& playerPos = pLocalPed->GetPosition();
#if !VER_x32
    CStreaming::RequestSceneObjectsAroundPlayer64(playerPos, pLocalPed->GetMoveSpeed());
#endif
//    if (!ms_disableStreaming
//        && !CCutsceneMgr::IsCutsceneProcessing()
//        && CGame::CanSeeOutSideFromCurrArea()
//        && CReplay::Mode != MODE_PLAYBACK
//        && fCamDistanceToGroundZ < 50.0f
//            ) {
//        StreamVehiclesAndPeds_Always(playerPos);
//        if (!IsVeryBusy()) {
//            StreamVehiclesAndPeds();
//            StreamZoneModels(playerPos);
//        }
//    }
    LoadRequestedModels();

    if (pLocalPed->IsInVehicle()) {
        CVehicleGTA* remoteVehicle = pLocalPed->pVehicle;

        GTASAEngineApi::WorldStreamRequest playerRequest{};
        playerRequest.addModels = false;
        playerRequest.addLods = false;
        playerRequest.setCollisionRequired = false;
        playerRequest.requestCollision = false;
        playerRequest.loadCollision = false;
        playerRequest.ensureCollision = false;
        playerRequest.loadIpls = false;
        playerRequest.ensureIpls = false;
        playerRequest.loadSceneCollision = false;
        playerRequest.reason = "stream-update-player-in-vehicle";
        GTASAEngineApi::PrepareWorldAtPoint(playerPos, playerRequest);

        if (remoteVehicle) {
            const auto& removeVehiclePos = remoteVehicle->GetPosition();
            GTASAEngineApi::WorldStreamRequest vehicleRequest{};
            vehicleRequest.addModels = false;
            vehicleRequest.addLods = false;
            vehicleRequest.addCollisionNeeded = false;
            vehicleRequest.setCollisionRequired = false;
            vehicleRequest.requestCollision = false;
            vehicleRequest.addIpls = false;
            vehicleRequest.loadSceneCollision = false;
            vehicleRequest.reason = "stream-update-vehicle";
            GTASAEngineApi::PrepareWorldAtPoint(removeVehiclePos, vehicleRequest);
        }
    }
    else {
        GTASAEngineApi::WorldStreamRequest playerRequest{};
        playerRequest.addModels = false;
        playerRequest.addLods = false;
        playerRequest.setCollisionRequired = false;
        playerRequest.requestCollision = false;
        playerRequest.loadSceneCollision = false;
        playerRequest.reason = "stream-update-player";
        GTASAEngineApi::PrepareWorldAtPoint(playerPos, playerRequest);
    }

    if (ms_bEnableRequestListPurge) {
        PurgeRequestList();
    }
}

// Call `RemoveModel` on all models in the request list except
// those ones which have either `KEEP_IN_MEMORY` or `PRIORITY_REQUEST` flag(s) set.
void CStreaming::PurgeRequestList() {
    auto info = ms_pEndRequestedList->GetPrev();
    while (info != ms_pStartRequestedList) {
        auto prev = info->GetPrev();
        if (!info->IsRequiredToBeKept() && !info->IsPriorityRequest()) {
            RemoveModel(GetModelFromInfo(info));
        }
        info = prev;
    }
}

void CStreaming::DeleteAllRwObjects() {
#if !VER_x32
    static uint32_t s_lastSkipLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastSkipLogTick >= 5000) {
        s_lastSkipLogTick = now;
        FLog("[STREAM_GUARD64] DeleteAllRwObjects skipped to avoid arm64 RW pointer crash mem=%zu/%zu",
             CStreaming::ms_memoryUsed,
             CStreaming::ms_memoryAvailable);
    }
    return;
#else
    GTASAEngineApi::DeleteAllRwObjectsNative32();
#endif
}

void CStreaming::LoadRequestedModels() {
    GTASAEngineApi::LoadRequestedModels();
}

void CStreaming::LoadScene(const CVector& point) {
    GTASAEngineApi::LoadScene(point);
}

void CStreaming::LoadSceneCollision(const CVector& point) {
    GTASAEngineApi::LoadSceneCollision(point);
}

#if !VER_x32
void CStreaming::RequestSceneObjectsAroundPlayer64(const CVector& playerPos, const CVector& velocity) {
    RequestSceneObjectsAroundPlayer64Impl(playerPos, velocity);
}
#endif

// There are only 2 streaming channels within CStreaming::ms_channel. In this function,
// if your current channelIndex is zero then "1 - channelIndex" will give you the other
// streaming channel within CStreaming::ms_channel which is 1 (second streaming channel).
void CStreaming::LoadAllRequestedModels(bool bOnlyPriorityRequests) {
#if !VER_x32
    GTASAEngineApi::LoadAllRequestedModelsNativeArm64(bOnlyPriorityRequests);
    return;
#endif

   static bool m_bLoadingAllRequestedModels{};

    if (m_bLoadingAllRequestedModels) {
        return;
    }
    m_bLoadingAllRequestedModels = true;

    FlushChannels();

    auto numModelsToLoad = std::max(10, 2 * ms_numModelsRequested);
    int32 chIdx = 0;
    while (true) {
        const tStreamingChannel& ch1 = ms_channel[0];
        const tStreamingChannel& ch2 = ms_channel[1];

        if (IsRequestListEmpty()
            && ch1.IsIdle()
            && ch2.IsIdle()
            || numModelsToLoad <= 0
                ) {
            break;
        }

        if (ms_bLoadingBigModel) {
            chIdx = 0;
        }

        auto& currCh = ms_channel[chIdx];

        if (!currCh.IsIdle()) {
            // Finish loading whatever it was loading
            CdStreamSync(chIdx);
            currCh.iLoadingLevel = 100;
        }
        if (currCh.IsReading()) {
            ProcessLoadingChannel(chIdx);
            if (currCh.IsStarted())
                ProcessLoadingChannel(chIdx); // Finish loading big model
        }
        if (bOnlyPriorityRequests && ms_numPriorityRequests == 0)
            break;

        if (!ms_bLoadingBigModel) {
            const auto other = 1 - chIdx;
            if (ms_channel[other].IsIdle()) {
                RequestModelStream(other);
            }

            if (currCh.IsIdle() && !ms_bLoadingBigModel) {
                RequestModelStream(chIdx);
            }
        }

        if (ch1.IsIdle() && ch2.IsIdle())
            break;

        chIdx = 1 - chIdx; // Switch to other channel
        --numModelsToLoad;
    }
    FlushChannels();
    m_bLoadingAllRequestedModels = false;
}

int32 CStreaming::GetNextFileOnCd(uint32 streamLastPosn, bool bNotPriority) {
  //  ZoneScoped;

    uint32 nextRequestModelPos    = UINT32_MAX;
    uint32 firstRequestModelCdPos = UINT32_MAX;
    int32  firstRequestModelId    = MODEL_INVALID;
    int32  nextRequestModelId     = MODEL_INVALID;
    for (auto info = ms_pStartRequestedList->GetNext(); info != ms_pEndRequestedList; info = info->GetNext()) {
        const auto modelId = GetModelFromInfo(info);
        if (bNotPriority && ms_numPriorityRequests != 0 && !info->IsPriorityRequest())
            continue;

        // Additional conditions for some model types (DFF, TXD, IFP)
        switch (GetModelType(modelId)) {
            case eModelType::DFF: {
                CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);

//                // Make sure TXD will be loaded for this model
//                const auto txdModel = TXDToModelId(modelInfo->m_nTxdIndex);
//                if (!GetInfo(txdModel).IsLoadedOrBeingRead()) {
//                    RequestModel(txdModel, GetInfo(modelId).GetFlags()); // Request TXD for this DFF
//                    continue;
//                }

                // Check if it has an anim (IFP), if so, make sure it gets loaded
                if(modelInfo) {
                    const int32 animFileIndex = modelInfo->GetAnimFileIndex();
                    if (animFileIndex != -1) {
                        const int32 animModelId = IFPToModelId(animFileIndex);
                        if (!GetInfo(animModelId).IsLoadedOrBeingRead()) {
                            RequestModel(animModelId, STREAMING_KEEP_IN_MEMORY);
                            continue;
                        }
                    }
                }
                break;
            }
//            case eModelType::TXD: {
//                // Make sure parent is/will be loaded
//                TxdDef* texDictionary = CTxdStore::ms_pTxdPool->GetAt(ModelIdToTXD(modelId));
//                const int16 parentIndex = texDictionary->m_wParentIndex;
//                if (parentIndex != -1) {
//                    const int32 parentModelIdx = TXDToModelId(parentIndex);
//                    if (!GetInfo(parentModelIdx).IsLoadedOrBeingRead()) {
//                        RequestModel(parentModelIdx, STREAMING_KEEP_IN_MEMORY);
//                        continue;
//                    }
//                }
//                break;
//            }
//            case eModelType::IFP: {
////                if (CCutsceneMgr::IsCutsceneProcessing() || !GetInfo(MODEL_MALE01).IsLoaded()) {
////                    // Skip in this case
////                    continue;
////                }
//                break;
//            }
        }

        const uint32 modelCdPos = GetInfo(modelId).GetCdPosn();
        if (modelCdPos < firstRequestModelCdPos) {
            firstRequestModelCdPos = modelCdPos;
            firstRequestModelId = modelId;
        }

        if (modelCdPos < nextRequestModelPos && modelCdPos >= streamLastPosn) {
            nextRequestModelPos = modelCdPos;
            nextRequestModelId = modelId;
        }
    }

    const int32 nextModelId = nextRequestModelId == MODEL_INVALID ? firstRequestModelId : nextRequestModelId;
    if (nextModelId != MODEL_INVALID || ms_numPriorityRequests == 0)
        return nextModelId;

    ms_numPriorityRequests = 0;
    return MODEL_INVALID;
}

// Starts reading at most 16 models at a time.
// Removes all unused (if not IsRequiredToBeKept()) IFP/TXDs models as well.
void CStreaming::RequestModelStream(int32 chIdx) {
    int32 modelId = GetNextFileOnCd(CdStreamGetLastPosn(), true);
    if (modelId == MODEL_INVALID)
        return;

    tStreamingChannel& ch = ms_channel[chIdx];
    size_t posn = 0;
    size_t nThisModelSizeInSectors = 0;
    CStreamingInfo* streamingInfo = &GetInfo(modelId);

    // Find first model that has to be loaded
    while (!streamingInfo->IsRequiredToBeKept()) {
        // In case of TXD/IFP's check if they're used at all, if not remove them.
        if (IsModelIFP(modelId)) {
            if (AreAnimsUsedByRequestedModels(ModelIdToIFP(modelId)))
                break;
        } else /*model is neither TXD or IFP*/ {
            break; // No checks needed
        }

        // TXD/IFP unused, so remove it, and go on to the next file

        RemoveModel(modelId);

        streamingInfo->GetCdPosnAndSize(posn, nThisModelSizeInSectors);    // Grab pos and size of this model
        modelId = GetNextFileOnCd(posn + nThisModelSizeInSectors, true);   // Find where the next file is after it
        if (modelId == MODEL_INVALID)
            return; // No more models...
        streamingInfo = &GetInfo(modelId); // Grab next file's info
    }

    // Grab cd pos and size for this model
    streamingInfo->GetCdPosnAndSize(posn, nThisModelSizeInSectors);

    // Check if it's big 0x40CCD5
    if (nThisModelSizeInSectors > ms_streamingBufferSize) {
        // A model is considered "big" if it doesn't fit into a single channel's buffer
        // In which case it has to be loaded entirely by channel 0.
        if (chIdx == 1 || !ms_channel[1].IsIdle())
            return;
        ms_bLoadingBigModel = true;
    }

    // Find all (but at most 16) consecutive models starting at `posn` and load them in one go
    uint32 nSectorsToRead = 0; // The # of sectors to be loaded beginning at `posn`

    bool isPreviousLargeishBigOrVeh = false;
    bool isPreviousModelPed = false;

    // 0x40CD10
    uint32 i = 0;
    for (; i < std::size(ch.modelIds); i++) {
        if (modelId == MODEL_INVALID) {
            break;
        }
        streamingInfo = &GetInfo(modelId);

        if (!streamingInfo->IsRequested())
            break; // Model not requested, so no need to load it.

        if (streamingInfo->GetCdSize())
            nThisModelSizeInSectors = streamingInfo->GetCdSize();

        const bool isThisModelLargeish = nThisModelSizeInSectors > 200;

        if (ms_numPriorityRequests && !streamingInfo->IsPriorityRequest())
            break; // There are priority requests, but this isn't one of them

        CBaseModelInfo* mi = CModelInfo::GetModelInfo(modelId);
        if (IsModelDFF(modelId)) {
            if (isPreviousModelPed && mi->GetModelType() == MODEL_INFO_PED)
                break; // Don't load two peds after each other

            if (isPreviousLargeishBigOrVeh && mi->GetModelType() == MODEL_INFO_VEHICLE)
                break; // Don't load two vehicles / big model + vehicle after each other

            // Check if TXD and/or IFP is loaded for this model.
            // If not we can't load the model yet.

            // Check TXD
//            if (!GetInfo(TXDToModelId(mi->m_nTxdIndex)).IsLoadedOrBeingRead())
//                break;

            // Check IFP (if any)
            const int32 animFileIndex = mi->GetAnimFileIndex();
            if (animFileIndex != -1) {
                if (!GetInfo(IFPToModelId(animFileIndex)).IsLoadedOrBeingRead())
                    break;
            }
        } else {
            if (isPreviousLargeishBigOrVeh && isThisModelLargeish)
                break; // Do not load a big model/car and a big model after each other
        }

        // At this point we've made sure the model can be loaded
        // so let's add it to the channel.

        // Set offset where the model's data begins at
        ch.modelStreamingBufferOffsets[i] = nSectorsToRead;

        // Set the corresponding modelId
        ch.modelIds[i] = modelId;

        // `i == 0` is a special case:
        // If the 0th model doesn't fit into the buffer it's a `big` one
        // so `ms_bLoadingBigModel` is set already (before the `for` loop).
        // But we still need to continue to set the appropriate states for the
        // model, thus we can't just `break` (which would also cause the loop below setting modelId slots `-1`'s to override the modelId)
        if (i > 0) {
            // Check if this model + all the previous fits into one channel's buffer
            if (nSectorsToRead + nThisModelSizeInSectors > ms_streamingBufferSize) {
                // No, so stop at the previous model, and ignore this one
                break;
            }
        }
        nSectorsToRead += nThisModelSizeInSectors;

        if (IsModelDFF(modelId)) {
            switch (mi->GetModelType()) {
                case ModelInfoType::MODEL_INFO_PED:
                    isPreviousModelPed = true;
                    break;
                case ModelInfoType::MODEL_INFO_VEHICLE:
                    isPreviousLargeishBigOrVeh = true; // I guess all vehicles are considered big?
                    break;
            }
        } else {
            if (isThisModelLargeish)
                isPreviousLargeishBigOrVeh = true;
        }

        // Modify the state of models
        {
            streamingInfo->m_nLoadState = LOADSTATE_READING; // Set as being read
            streamingInfo->RemoveFromList(); // Remove from it's current list (That is the requested list)
            ms_numModelsRequested--;
            if (streamingInfo->IsPriorityRequest()) {
                streamingInfo->ClearFlags(STREAMING_PRIORITY_REQUEST); // Remove priority request flag, as its not a request anymore.
                ms_numPriorityRequests--;
            }
        }

        modelId = streamingInfo->m_nNextIndexOnCd; // Continue onto the next one in the directory
    }

    // Set remaining modelId slots to `-1`
    for (auto j = i; j < std::size(ch.modelIds); j++) {
        ch.modelIds[j] = MODEL_INVALID;
    }

    CdStreamRead(chIdx, ms_pStreamingBuffer[chIdx], posn, nSectorsToRead); // Request models to be read
    ch.LoadStatus = eChannelState::READING;
    ch.iLoadingLevel = 0;
    ch.sectorCount = nSectorsToRead; // Set how many sectors to read
    ch.offsetAndHandle = posn;       // And from where to read
    ch.totalTries = 0;

    GTASAEngineApi::ClearModelStreamNotLoadedFlag();
}

// If the channel is done reading (`CdStreamGetStatus(chIdx)` == READING_SUCCESS) then loads all the read models
// using either `ConvertBufferToObject` or `FinishLoadingLargeFile` (in case of big models)
bool CStreaming::ProcessLoadingChannel(int32 chIdx) {
    tStreamingChannel& ch = ms_channel[chIdx];

   // Log("ProcessLoadingChannel");
    const eCdStreamStatus streamStatus = CdStreamGetStatus(chIdx);
    switch (streamStatus) {
        case eCdStreamStatus::READING_SUCCESS:
            break;

        case eCdStreamStatus::READING:
        case eCdStreamStatus::WAITING_TO_READ:
            return false; // Not ready yet.

        case eCdStreamStatus::READING_FAILURE: {
            // Retry

            ch.m_nCdStreamStatus = streamStatus;
            ch.LoadStatus = eChannelState::ERR;

            if (ms_channelError != -1)
                return false;

            ms_channelError = chIdx;
            RetryLoadFile(chIdx);

            return true;
        }
    }
  //  Log("ProcessLoadingChannel1");
    const bool isStarted = ch.IsStarted();
    ch.LoadStatus = eChannelState::IDLE;
    if (isStarted) {
        // It's a large model so finish loading it
        auto bufferOffset = ch.modelStreamingBufferOffsets[0];
        auto* pFileContents = reinterpret_cast<uint8*>(&ms_pStreamingBuffer[chIdx][STREAMING_SECTOR_SIZE * bufferOffset]);
        FinishLoadingLargeFile(pFileContents, ch.modelIds[0]);
        ch.modelIds[0] = MODEL_INVALID;
    } else {
        // Load models individually
        for (uint32 i = 0u; i < std::size(ch.modelIds); i++) {
            const int32 modelId = ch.modelIds[i];
            if (modelId == MODEL_INVALID)
                continue;

            CBaseModelInfo* baseModelInfo = CModelInfo::GetModelInfo(modelId);
            CStreamingInfo& info = GetInfo(modelId);

            if (!IsModelDFF(modelId)
                || baseModelInfo->GetModelType() != MODEL_INFO_VEHICLE /* It's a DFF, check if its a vehicle */
              //  || ms_vehiclesLoaded.CountMembers() < desiredNumVehiclesLoaded /* It's a vehicle, so lets check if we can load more */
              //  || RemoveLoadedVehicle() /* no, so try to remove one, and load this in its place */
                || info.IsMissionOrGameRequired() /* failed, lets check if its absolutely mission critical */
                    ) {
                if (!IsModelIPL(modelId)) {
                    MakeSpaceFor(info.GetCdSize() * STREAMING_SECTOR_SIZE); // IPL's dont require any memory themselves
                }
                const auto bufferOffsetInSectors = ch.modelStreamingBufferOffsets[i];
                auto* fileBuffer = reinterpret_cast <uint8*> (&ms_pStreamingBuffer[chIdx][STREAMING_SECTOR_SIZE * bufferOffsetInSectors]);

                // Actually load the model into memory
                ConvertBufferToObject(fileBuffer, modelId);

                if (info.IsLoadingFinishing()) {
                    ch.LoadStatus = eChannelState::STARTED;
                    ch.modelStreamingBufferOffsets[i] = bufferOffsetInSectors;
                    ch.modelIds[i] = modelId;
                    if (i == 0)
                        continue;
                }
                ch.modelIds[i] = MODEL_INVALID;
            } else {
                // I think at this point it's guaranteed to be a vehicle (thus its a DFF),
                // with `STREAMING_MISSION_REQUIRED` `STREAMING_GAME_REQUIRED` flags unset.

                const int32 modelTxdIdx = baseModelInfo->m_nTxdIndex;
                RemoveModel(modelId);

                if (info.IsMissionOrGameRequired()) {
                    // Re-request it.
                    // I think this code is unreachable, because
                    // if any of the 2 flags (above) is set this code is never reached.
                    RequestModel(modelId, info.GetFlags());
                } else if (!CTxdStore::GetNumRefs(modelTxdIdx))
                    RemoveTxdModel(modelTxdIdx); // Unload TXD, as it has no refs
            }
        }
    }

    if (ms_bLoadingBigModel) {
        if (!ch.IsStarted()) {
            ms_bLoadingBigModel = false;
            for (auto& id : ms_channel[1].modelIds) {
                id = MODEL_INVALID;
            }
        }
    }

    return true;
}

bool CStreaming::ConvertBufferToObject(uint8* fileBuffer, int32 modelId) {
    return GTASAEngineApi::ConvertStreamingBufferToObject(fileBuffer, modelId);
}

// Finishes loading a big model by loading the second half of the file
// residing at `pFileBuffer`.
void CStreaming::FinishLoadingLargeFile(uint8* pFileBuffer, int32 modelId) {
    GTASAEngineApi::FinishLoadingLargeStreamingFile(pFileBuffer, modelId);
}

void CStreaming::RetryLoadFile(int32 chIdx) {
    GTASAEngineApi::RetryStreamingLoadFile(chIdx);
}

void CStreaming::MakeSpaceFor(size_t memoryToCleanInBytes) {
#if !VER_x32
    if (ms_memoryAvailable < kArm64StreamingMemoryBudget) {
        ms_memoryAvailable = kArm64StreamingMemoryBudget;
    } else if (ms_memoryAvailable > kArm64StreamingMemoryHardBudget) {
        ms_memoryAvailable = kArm64StreamingMemoryHardBudget;
    }
#endif

    auto lastmemused = ms_memoryUsed;
    while (ms_memoryUsed >= ms_memoryAvailable - memoryToCleanInBytes) {
        lastmemused = ms_memoryUsed;
        if (!RemoveLeastUsedModel(STREAMING_DONTREMOVE_IN_LOADSCENE) || lastmemused == ms_memoryUsed) {
          //  DeleteRwObjectsBehindCamera(ms_memoryAvailable - memoryToCleanInBytes);
            return;
        }
    }
}

//
void CStreaming::DeleteRwObjectsBehindCamera(size_t memoryToCleanInBytes) {
    GTASAEngineApi::DeleteRwObjectsBehindCamera(memoryToCleanInBytes);
}

// Function name is a little misleading, as it deletes the first entity it can.
bool CStreaming::DeleteLeastUsedEntityRwObject(bool bNotOnScreen, int32 streamingFlags) {
    return GTASAEngineApi::DeleteLeastUsedEntityRwObject(bNotOnScreen, streamingFlags);
}

// The name is misleading: It just removes the first model with no references.
bool CStreaming::RemoveLeastUsedModel(int32 streamingFlags) {

    auto streamingInfo = ms_pEndLoadedList->GetPrev();
    for (; streamingInfo != ms_startLoadedList; streamingInfo = streamingInfo->GetPrev()) {
        const auto modelId = GetModelFromInfo(streamingInfo);
        if (!streamingInfo->AreAnyFlagsSetOutOf(streamingFlags)) {
            switch (GetModelType(modelId)) {
                case eModelType::DFF: {
                    if (!CModelInfo::GetModelInfo(modelId)->m_nRefCount) {
                        RemoveModel(modelId);
                        return true;
                    }
                    break;
                }
//                case eModelType::TXD: {
//                    const auto txdId = ModelIdToTXD(modelId);
//                    if (!CTxdStore::GetNumRefs(txdId) && !AreTexturesUsedByRequestedModels(txdId)) {
//                        RemoveModel(modelId);
//                        return true;
//                    }
//                    break;
//                }
                case eModelType::IFP: {
                    const auto animBlockId = ModelIdToIFP(modelId);
                    if (!CAnimManager::GetNumRefsToAnimBlock(animBlockId) && !AreAnimsUsedByRequestedModels(animBlockId)) {
                        RemoveModel(modelId);
                        return true;
                    }
                    break;
                }
//                case eModelType::SCM: {
//                    const auto scmId = ModelIdToSCM(modelId);
//                    if (!CTheScripts::StreamedScripts.m_aScripts[scmId].m_nStatus) {
//                        RemoveModel(modelId);
//                        return true;
//                    }
//                    break;
//                }
            }
        }
    }

    CCamera& TheCamera = GTASAEngineApi::Camera();
    if (TheCamera.GetPosition().z - TheCamera.CalculateGroundHeight(eGroundHeightType::ENTITY_BB_BOTTOM) <= 50.0f){
        return DeleteLeastUsedEntityRwObject(false, streamingFlags);
    }
    return false;
}

bool CStreaming::HasVehicleUpgradeLoaded(int32 modelId) {
    return GTASAEngineApi::HasVehicleUpgradeLoaded(modelId);
//    if (!GetInfo(modelId).IsLoaded())
//        return false;
//
//    assert(modelId <= INT16_MAX);
//    const int16 otherUpgradeModelId = CVehicleModelInfo::ms_linkedUpgrades.FindOtherUpgrade((int16)modelId);
//    return otherUpgradeModelId == -1 || GetInfo(otherUpgradeModelId).IsLoaded();
}

CLink<CEntityGTA*>* CStreaming::AddEntity(CEntityGTA* entity) {
    switch (entity->GetType()) {
        case ENTITY_TYPE_PED:
        case ENTITY_TYPE_VEHICLE:
            return nullptr;
    }

    auto l = ms_rwObjectInstances.Insert(entity);
    if (!l) { // No more entries left
        // Okay, try deleting something now
        for (auto it = ms_rwObjectInstances.GetTailLink().prev; it != &ms_rwObjectInstances.GetHeadLink(); it = it->prev) {
            const auto e = it->data;

            if (!e->m_bImBeingRendered && !e->m_bStreamingDontDelete) {
                e->DeleteRwObject();
                break;
            }
        }

        // Try inserting again, should succeed now because we've deleted something (Ideally anyways)
        VERIFY(l = ms_rwObjectInstances.Insert(entity));
    }
    return l;
}


char *CStreaming::GetModelCDName(int32 index) {
    return GTASAEngineApi::GetModelCDName(index);
}
