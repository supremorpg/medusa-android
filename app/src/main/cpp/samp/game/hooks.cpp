#include <GLES2/gl2.h>
#include "../main.h"
#include "../vendor/armhook/patch.h"
#include "game.h"
#include "../net/netgame.h"
#include "../net/localplayer.h"
#include "Widgets/TouchInterface.h"
#include "Widgets/WidgetGta.h"
#include "../gui/gui.h"
#include "Textures/TextureDatabase.h"
#include "Textures/TextureDatabaseEntry.h"
#include "Textures/TextureDatabaseRuntime.h"
#include "Scene.h"
#include "sprite2d.h"
#include "Entity/PlayerPedGta.h"
#include "Entity/Object.h"
#include "Entity/CPhysical.h"
#include "Pools.h"
#include "java/jniutil.h"
#include "game/Models/ModelInfo.h"
#include "MatrixLink.h"
#include "MatrixLinkList.h"
#include "game/Collision/Collision.h"
#include "TxdStore.h"
#include "util.h"
#include "util/CUtil.h"
#include "Coronas.h"
#include "SampStreamingApi.h"
#include "SampCollisionApi.h"
#include "References.h"
#include "VisibilityPlugins.h"
#include "game/Animation/AnimManager.h"
#include "SampFileLoaderApi.h"
#include "Renderer.h"
#include "World.h"
#include "GTASAEngineApi.h"
#include "GTASAEngineNativeHookBindings.h"
#include "GTASAEnginePhysicalRenderBindings.h"
#include "GTASAEngineTaskPedBindings.h"
#include "SampInputApi.h"
#include "Camera.h"
#include "MemoryMgr.h"
#include "pad.h"
#include "Core/PtrListSingleLink.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include "src/ui/SampUiController.h"

extern UI* pUI;
extern CGame* pGame;
extern CNetGame *pNetGame;
extern MaterialTextGenerator* pMaterialTextGenerator;
extern CJavaWrapper* pJavaWrapper;
void MainLoop();

#if !VER_x32
extern "C" void ProcessQueuedMapScanTeleport64();
extern "C" void ProcessQueuedMapScanProbe64();
#endif

#if !VER_x32
static constexpr bool kArm64DirectRenderFallbackEnabled = false;
static constexpr bool kArm64CollisionDeepDiagnosticsEnabled = false;
static constexpr bool kArm64CollisionRuntimeSummaryLogsEnabled = false;
#endif

static bool IsSampConnectedGameplay64()
{
    return pNetGame && pNetGame->GetGameState() == GAMESTATE_CONNECTED;
}

static bool IsSampRuntimeActive64()
{
    return pNetGame != nullptr;
}

static void RestoreWorld3DRenderState64(const char* stage)
{
#if !VER_x32
    DefinedState();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLBACK));
    glDisable(GL_SCISSOR_TEST);

    static uint32_t s_lastWorldStateLogTick = 0;
    const uint32_t now = GetTickCount();
    if (stage && now - s_lastWorldStateLogTick >= 2000u) {
        s_lastWorldStateLogTick = now;
        FLog("[RENDER_STATE64] restored 3D depth state stage=%s", stage);
    }
#else
    (void)stage;
#endif
}

#if !VER_x32
static void ClearWorldTextureRasterState64(const char* stage)
{
    if (!IsSampConnectedGameplay64()) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));
    glBindTexture(GL_TEXTURE_2D, 0);

    static uint32_t s_lastClearTextureLogTick = 0;
    const uint32_t now = GetTickCount();
    if (stage && now - s_lastClearTextureLogTick >= 2000u) {
        s_lastClearTextureLogTick = now;
        FLog("[RENDER_STATE64] cleared texture raster stage=%s", stage);
    }
}
#endif

static void SuppressNativeLoadingDuringGameplay64(const char* stage)
{
    if (!IsSampConnectedGameplay64() || !IsLocalPlayerHudReady()) {
        return;
    }

    static bool s_abiDiagnosticLogged = false;
    if (!s_abiDiagnosticLogged) {
        s_abiDiagnosticLogged = true;
        FLog("[LOAD_SUPPRESS64] native loading ABI offsets enabled only after connected/spawn-ready stage=%s base=%p",
             stage ? stage : "?", reinterpret_cast<void*>(g_libGTASA));
    }

    static uint32_t s_lastPriorityLogTick = 0;
    static uint32_t s_lastJavaHideTick = 0;
    const uint32_t now = GetTickCount();

    if (CRenderer::m_loadingPriority) {
        CRenderer::m_loadingPriority = false;
        if (now - s_lastPriorityLogTick >= 2000u) {
            s_lastPriorityLogTick = now;
            FLog("[LOAD_SUPPRESS64] cleared renderer loading priority state=%d stage=%s",
                 pNetGame ? pNetGame->GetGameState() : -1,
                 stage ? stage : "?");
        }
    }

#if !VER_x32
    // These offsets are intentionally isolated and unchanged; validate their
    // ABI at runtime before writing so unsupported game builds are untouched.
    static constexpr uintptr_t kLoadingBoolGlobals64[] = {
        0xC20ED8, // CLoadingScreen::m_bActive
        0xC20EF0, // CLoadingScreen::m_bFading
        0xC20EDA, // CLoadingScreen::m_bPaused
        0xC20EF1, // CLoadingScreen::m_bLegalScreen
        0xC20ED9, // CLoadingScreen::m_bWantToPause
        0xC20EDB, // CLoadingScreen::m_bForceShutdown
        0xC2105B, // CLoadingScreen::m_bReadyToDelete
        0xC20EF3, // CLoadingScreen::m_bFadeOutCurrSplashToBlack
        0xC20EF2, // CLoadingScreen::m_bFadeInNextSplashFromBlack
        0x972DE0, // bLoadingScene
        0xC9AE21  // CGenericGameStorage::ms_bLoading
    };
    for (uintptr_t offset : kLoadingBoolGlobals64) {
        *reinterpret_cast<uint8_t*>(g_libGTASA + offset) = 0;
    }
    *reinterpret_cast<float*>(g_libGTASA + 0xC20EF4) = 0.0f;
#endif

    if (pJavaWrapper && now - s_lastJavaHideTick >= 750u) {
        s_lastJavaHideTick = now;
        pJavaWrapper->HideLoadingScreen();
    }
}

static uint8_t NormalizeLocalExteriorArea64(CPlayerPed* localPed, const CVector& pos, const char* reason)
{
    if (!localPed || !localPed->m_pPed) {
        return static_cast<uint8_t>(AREA_CODE_NORMAL_WORLD);
    }

    const uint8_t rawArea = static_cast<uint8_t>(localPed->m_pPed->m_nAreaCode);
    const uint8_t effectiveArea = Supremo::Streaming::ResolveEffectiveWorldArea(pos, rawArea);
    if (rawArea == effectiveArea) {
        return effectiveArea;
    }

    localPed->m_pPed->SetInterior(static_cast<int>(effectiveArea), false);
    if (localPed->IsInVehicle()) {
        CVehicleGTA* vehicle = localPed->GetGtaVehicle();
        if (vehicle) {
            vehicle->SetInterior(static_cast<int>(effectiveArea), false);
        }
    }

    static uint32_t s_lastLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastLogTick >= 1200u) {
        s_lastLogTick = now;
        FLog("[AREA_FIX64] reason=%s raw=%u effective=%u pos=%.2f %.2f %.2f",
             reason ? reason : "?",
             static_cast<unsigned>(rawArea),
             static_cast<unsigned>(effectiveArea),
             pos.x,
             pos.y,
             pos.z);
    }

    return effectiveArea;
}

#if !VER_x32
struct TextureWarmupRequest64 {
    TextureDatabaseRuntime* database = nullptr;
    uint32 index = 0;
    uint32 queuedTick = 0;
};

static constexpr uint32_t kTextureWarmupQueueCapacity64 = 512;
static TextureWarmupRequest64 gTextureWarmupQueue64[kTextureWarmupQueueCapacity64];
static uint32_t gTextureWarmupRead64 = 0;
static uint32_t gTextureWarmupWrite64 = 0;
static uint32_t gTextureWarmupDropped64 = 0;

extern "C" void ResetTextureWarmupQueue64()
{
    gTextureWarmupRead64 = 0;
    gTextureWarmupWrite64 = 0;
    gTextureWarmupDropped64 = 0;
}

static bool IsWorldTextureDatabase64(const TextureDatabaseRuntime* database)
{
    if (!database || !database->name) {
        return false;
    }

    return !std::strcmp(database->name, "gta3") ||
           !std::strcmp(database->name, "gta_int") ||
           !std::strcmp(database->name, "txd") ||
           !std::strcmp(database->name, "mobile");
}

static uint32_t NextTextureWarmupSlot64(uint32_t slot)
{
    ++slot;
    return slot == kTextureWarmupQueueCapacity64 ? 0 : slot;
}

static bool IsTextureWarmupQueued64(TextureDatabaseRuntime* database, uint32 index)
{
    for (uint32_t cursor = gTextureWarmupRead64;
         cursor != gTextureWarmupWrite64;
         cursor = NextTextureWarmupSlot64(cursor)) {
        const TextureWarmupRequest64& request = gTextureWarmupQueue64[cursor];
        if (request.database == database && request.index == index) {
            return true;
        }
    }

    return false;
}

static void QueueTextureWarmup64(TextureDatabaseRuntime* database, int32 index)
{
    if (!IsSampConnectedGameplay64() ||
        index < 0 ||
        !IsWorldTextureDatabase64(database) ||
        !database->entries.dataPtr ||
        static_cast<uint32>(index) >= database->entries.numEntries) {
        return;
    }

    const uint32 textureIndex = static_cast<uint32>(index);
    if (IsTextureWarmupQueued64(database, textureIndex)) {
        return;
    }

    const uint32_t nextWrite = NextTextureWarmupSlot64(gTextureWarmupWrite64);
    if (nextWrite == gTextureWarmupRead64) {
        gTextureWarmupRead64 = NextTextureWarmupSlot64(gTextureWarmupRead64);
        ++gTextureWarmupDropped64;
    }

    gTextureWarmupQueue64[gTextureWarmupWrite64] = { database, textureIndex, GetTickCount() };
    gTextureWarmupWrite64 = nextWrite;
}

static bool PopTextureWarmup64(TextureWarmupRequest64* outRequest)
{
    if (gTextureWarmupRead64 == gTextureWarmupWrite64) {
        return false;
    }

    *outRequest = gTextureWarmupQueue64[gTextureWarmupRead64];
    gTextureWarmupRead64 = NextTextureWarmupSlot64(gTextureWarmupRead64);
    return true;
}

static void ProcessTextureWarmupQueue64()
{
    if (!IsSampConnectedGameplay64()) {
        return;
    }

    static uint32_t s_lastWarmupLogTick = 0;
    constexpr uint32_t kMaxRenderedMarksPerFrame = 36;
    constexpr uint32_t kMaxFullLoadsPerFrame = 3;
    uint32_t renderedMarks = 0;
    uint32_t fullLoads = 0;
    uint32_t skipped = 0;
    TextureWarmupRequest64 request{};

    while (renderedMarks < kMaxRenderedMarksPerFrame && PopTextureWarmup64(&request)) {
        TextureDatabaseRuntime* database = request.database;
        if (!IsWorldTextureDatabase64(database) ||
            !database->entries.dataPtr ||
            request.index >= database->entries.numEntries) {
            ++skipped;
            continue;
        }

        TextureDatabaseEntry& entry = database->entries.dataPtr[request.index];
        database->SetAsRendered(request.index);
        ++renderedMarks;

        const bool textureMissing = entry.instance == nullptr;
        const bool textureNotReady = entry.status < 2;
        if (fullLoads < kMaxFullLoadsPerFrame && (textureMissing || textureNotReady)) {
            database->LoadFullTexture(request.index);
            ++fullLoads;
        }
    }

    if (renderedMarks > 0 || fullLoads > 0) {
        TextureDatabaseRuntime::UpdateStreaming(0.016f, true);
    }

    const uint32_t now = GetTickCount();
    if ((renderedMarks > 0 || fullLoads > 0 || skipped > 0 || gTextureWarmupDropped64 > 0) &&
        now - s_lastWarmupLogTick >= 2000u) {
        s_lastWarmupLogTick = now;
        FLog("[TEX_WARM64] marked=%u full=%u skipped=%u dropped=%u queue=%u/%u",
             renderedMarks,
             fullLoads,
             skipped,
             gTextureWarmupDropped64,
             (gTextureWarmupWrite64 + kTextureWarmupQueueCapacity64 - gTextureWarmupRead64) %
                 kTextureWarmupQueueCapacity64,
             kTextureWarmupQueueCapacity64);
    }
}

static RwTexture* (*TextureDatabaseRuntime__GetRWTexture64)(TextureDatabaseRuntime* thiz, int32 index) = nullptr;
static bool gTextureDatabaseGetRWTextureGuard64 = false;

static bool ShouldForceFullWorldTexture64(TextureDatabaseRuntime* database, int32 index)
{
    if (!IsSampConnectedGameplay64() ||
        !IsWorldTextureDatabase64(database) ||
        !database->entries.dataPtr ||
        index < 0 ||
        static_cast<uint32>(index) >= database->entries.numEntries) {
        return false;
    }

    const TextureDatabaseEntry& entry = database->entries.dataPtr[index];
    return entry.instance == nullptr || entry.status < 2;
}

static void ForceFullWorldTexture64(TextureDatabaseRuntime* database, int32 index, const char* reason)
{
    if (!ShouldForceFullWorldTexture64(database, index)) {
        return;
    }

    TextureDatabaseEntry& entry = database->entries.dataPtr[index];
    database->SetAsRendered(static_cast<uint32>(index));
    database->LoadFullTexture(static_cast<uint32>(index));

    static uint32_t s_lastFullTextureLogTick = 0;
    static uint32_t s_fullTextureLogCount = 0;
    const uint32_t now = GetTickCount();
    if (s_fullTextureLogCount < 80 && now - s_lastFullTextureLogTick >= 40u) {
        s_lastFullTextureLogTick = now;
        ++s_fullTextureLogCount;
        FLog("[TEX_FULL64] forced db=%s index=%d name=%s status=%u size=%ux%u reason=%s",
             database->name ? database->name : "?",
             index,
             entry.name ? entry.name : "?",
             static_cast<unsigned>(entry.status),
             static_cast<unsigned>(entry.width),
             static_cast<unsigned>(entry.height),
             reason ? reason : "?");
    }
}

static RwTexture* TextureDatabaseRuntime__GetRWTexture64_hook(TextureDatabaseRuntime* thiz, int32 index)
{
    if (!gTextureDatabaseGetRWTextureGuard64 && ShouldForceFullWorldTexture64(thiz, index)) {
        gTextureDatabaseGetRWTextureGuard64 = true;
        ForceFullWorldTexture64(thiz, index, "get-rw-texture");
        TextureDatabaseRuntime::UpdateStreaming(0.016f, true);
        gTextureDatabaseGetRWTextureGuard64 = false;
    }

    return TextureDatabaseRuntime__GetRWTexture64
               ? TextureDatabaseRuntime__GetRWTexture64(thiz, index)
               : nullptr;
}

static void InstallTextureRuntimeStreaming64Hooks()
{
    static bool s_installed = false;
    if (s_installed) {
        return;
    }

    s_installed = true;
    FLog("[TEX_FULL64] TextureDatabaseRuntime::GetRWTexture hook disabled");
}
#endif

uint8_t byteInternalPlayer = 0;
CPedGTA* dwCurPlayerActor = 0;
uint8_t byteCurPlayer = 0;

extern "C" uintptr_t get_lib()
{
    return g_libGTASA;
}
// 0.3.7
PLAYERID FindPlayerIDFromGtaPtr(CEntityGTA* pEntity)
{
    if (pEntity == nullptr) return INVALID_PLAYER_ID;

    CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
    CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();

    PLAYERID PlayerID = pPlayerPool->FindRemotePlayerIDFromGtaPtr((CPedGTA*)pEntity);
    if (PlayerID != INVALID_PLAYER_ID) return PlayerID;

    VEHICLEID VehicleID = pVehiclePool->FindIDFromGtaPtr((CVehicleGTA*)pEntity);
    if (VehicleID != INVALID_VEHICLE_ID)
    {
        for (PLAYERID i = 0; i < MAX_PLAYERS; i++)
        {
            CRemotePlayer* pRemotePlayer = pPlayerPool->GetAt(i);
            if (pRemotePlayer && pRemotePlayer->CurrentVehicleID() == VehicleID) {
                return i;
            }
        }
    }

    return INVALID_PLAYER_ID;
}
// 0.3.7
PLAYERID FindActorIDFromGtaPtr(CPedGTA* pPed)
{
    if (pPed) {
        return pNetGame->GetActorPool()->FindIDFromGtaPtr(pPed);
    }

    return INVALID_PLAYER_ID;
}

#if !VER_x32
static CVehicleGTA* s_lastLocalRenderVehicle64 = nullptr;
static uint32_t s_lastLocalRenderVehicleSeenTick64 = 0;
static CVehicleGTA* s_lastQueuedVehicle64 = nullptr;
static uint32_t s_lastVehicleQueueTick64 = 0;
static CVehicleGTA* s_lastMainPassRenderedVehicle64 = nullptr;
static uint32_t s_lastMainPassRenderTick64 = 0;
static std::atomic<uint32_t> s_forceVehicleRenderRefreshUntilTick64 {0};

static bool RendererListContainsEntity64(CEntityGTA* const* list, int32 count, int32 capacity, CEntityGTA* entity)
{
    if (!list || !entity || count <= 0) {
        return false;
    }

    const int32 scanCount = std::min(count, capacity);
    for (int32 i = 0; i < scanCount; ++i) {
        if (list[i] == entity) {
            return true;
        }
    }
    return false;
}

static bool IsEntityQueuedForRender64(CEntityGTA* entity)
{
    return RendererListContainsEntity64(CRenderer::ms_aVisibleEntityPtrs,
                                        CRenderer::ms_nNoOfVisibleEntities,
                                        static_cast<int32>(MAX_VISIBLE_ENTITY_PTRS),
                                        entity) ||
           RendererListContainsEntity64(CRenderer::ms_aVisibleLodPtrs,
                                        CRenderer::ms_nNoOfVisibleLods,
                                        static_cast<int32>(MAX_VISIBLE_LOD_PTRS),
                                        entity) ||
           RendererListContainsEntity64(CRenderer::ms_aVisibleSuperLodPtrs,
                                        CRenderer::ms_nNoOfVisibleSuperLods,
                                        static_cast<int32>(MAX_VISIBLE_SUPERLOD_PTRS),
                                        entity) ||
           RendererListContainsEntity64(CRenderer::ms_aInVisibleEntityPtrs,
                                        CRenderer::ms_nNoOfInVisibleEntities,
                                        static_cast<int32>(MAX_INVISIBLE_ENTITY_PTRS),
                                        entity);
}

static bool IsSampVehicleAlive64(CVehicleGTA* vehicle, VEHICLEID* outVehicleId = nullptr)
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
    if (vehicleId == INVALID_VEHICLE_ID || vehicleId >= MAX_VEHICLES) {
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

static float DistanceSquared64(const CVector& a, const CVector& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static CVehicleGTA* GetTrackedLocalVehicleForRender64(bool* outInVehicle = nullptr,
                                                      VEHICLEID* outVehicleId = nullptr)
{
    if (!pGame) {
        return nullptr;
    }

    if (outInVehicle) {
        *outInVehicle = false;
    }
    if (outVehicleId) {
        *outVehicleId = INVALID_VEHICLE_ID;
    }

    const uint32_t now = GetTickCount();
    CPlayerPed* playerPed = pGame->FindPlayerPed();
    if (playerPed && playerPed->m_pPed && playerPed->IsInVehicle()) {
        CVehicleGTA* currentVehicle = playerPed->GetGtaVehicle();
        VEHICLEID vehicleId = INVALID_VEHICLE_ID;
        if (IsSampVehicleAlive64(currentVehicle, &vehicleId)) {
            s_lastLocalRenderVehicle64 = currentVehicle;
            s_lastLocalRenderVehicleSeenTick64 = now;
            if (outInVehicle) {
                *outInVehicle = true;
            }
            if (outVehicleId) {
                *outVehicleId = vehicleId;
            }
            return currentVehicle;
        }
    }

    if (!s_lastLocalRenderVehicle64 || now - s_lastLocalRenderVehicleSeenTick64 > 30000) {
        s_lastLocalRenderVehicle64 = nullptr;
        return nullptr;
    }

    VEHICLEID vehicleId = INVALID_VEHICLE_ID;
    if (!IsSampVehicleAlive64(s_lastLocalRenderVehicle64, &vehicleId)) {
        s_lastLocalRenderVehicle64 = nullptr;
        return nullptr;
    }

    if (playerPed && playerPed->m_pPed) {
        const CVector pedPos = playerPed->m_pPed->GetPosition();
        const CVector vehiclePos = s_lastLocalRenderVehicle64->GetPosition();
        if (DistanceSquared64(pedPos, vehiclePos) > 260.0f * 260.0f) {
            s_lastLocalRenderVehicle64 = nullptr;
            return nullptr;
        }
    }

    if (outVehicleId) {
        *outVehicleId = vehicleId;
    }
    return s_lastLocalRenderVehicle64;
}

static void KeepLocalOccupantRenderable64(CVehicleGTA* vehicle)
{
    if (!vehicle || !pGame) {
        return;
    }

    CPlayerPed* playerPed = pGame->FindPlayerPed();
    if (!playerPed || !playerPed->m_pPed || playerPed->m_pPed->pVehicle != vehicle) {
        return;
    }

    CPedGTA* localPed = playerPed->m_pPed;
    localPed->bDontRender = false;
    localPed->bRenderPedInCar = true;
    localPed->m_bIsVisible = true;
}

static bool PrepareVehicleForRender64(CVehicleGTA* vehicle)
{
    if (!vehicle ||
        vehicle->m_nModelIndex >= CModelInfo::NUM_MODEL_INFOS ||
        !Supremo::Streaming::IsModelLoaded(vehicle->m_nModelIndex)) {
        return false;
    }

    if (!vehicle->m_pRwObject) {
        vehicle->CreateRwObject();
    }
    if (!vehicle->m_pRwObject) {
        return false;
    }

    vehicle->m_bIsVisible = true;
    vehicle->m_bRemoveFromWorld = false;
    vehicle->m_bStreamingDontDelete = true;
    vehicle->m_bDontStream = false;
    vehicle->m_bUsesCollision = true;
    vehicle->physicalFlags.bCollidable = true;
    vehicle->physicalFlags.bCanBeCollidedWith = true;
    vehicle->physicalFlags.bDisableSimpleCollision = false;
    vehicle->physicalFlags.bSkipLineCol = false;
    vehicle->physicalFlags.bDisableCollisionForce = false;
    vehicle->physicalFlags.bDisableMoveForce = false;
    vehicle->physicalFlags.bDontApplySpeed = false;
    vehicle->m_bDistanceFade = false;
    vehicle->m_nVehicleFlags.bFadeOut = false;
    vehicle->m_nVehicleFlags.bNeverUseSmallerRemovalRange = true;
    vehicle->UpdateRW();
    vehicle->UpdateRwFrame();
    KeepLocalOccupantRenderable64(vehicle);

    return true;
}

static void PreRenderVehicleForQueue64(CVehicleGTA* vehicle)
{
    if (!vehicle || !vehicle->m_pRwObject) {
        return;
    }

    CVisibilityPlugins::SetupVehicleVariables(vehicle->m_pRwClump);
    ((void (*)(CEntityGTA*))(*(uintptr_t*)(*(uintptr_t*)(vehicle) + 0x48 * 2)))(vehicle);
}

static bool AddVisibleEntityForRender64(CEntityGTA* entity)
{
    if (!entity) {
        return false;
    }
    if (IsEntityQueuedForRender64(entity)) {
        return true;
    }
    if (CRenderer::ms_nNoOfVisibleEntities < 0) {
        CRenderer::ms_nNoOfVisibleEntities = 0;
    }
    if (CRenderer::ms_nNoOfVisibleEntities >= static_cast<int32>(MAX_VISIBLE_ENTITY_PTRS)) {
        return false;
    }

    CRenderer::ms_aVisibleEntityPtrs[CRenderer::ms_nNoOfVisibleEntities++] = entity;
    return true;
}

static bool IsLocalVehicleRenderRefreshActive64(uint32_t now = GetTickCount())
{
    const uint32_t until = s_forceVehicleRenderRefreshUntilTick64.load(std::memory_order_acquire);
    return until != 0 && static_cast<int32_t>(until - now) > 0;
}

extern "C" void SupremoRememberLocalVehicleForRender64(CVehicleGTA* vehicle)
{
    VEHICLEID vehicleId = INVALID_VEHICLE_ID;
    if (!IsSampVehicleAlive64(vehicle, &vehicleId)) {
        return;
    }

    const uint32_t now = GetTickCount();
    s_lastLocalRenderVehicle64 = vehicle;
    s_lastLocalRenderVehicleSeenTick64 = now;

    if (!PrepareVehicleForRender64(vehicle)) {
        return;
    }

    CEntityGTA* entity = reinterpret_cast<CEntityGTA*>(vehicle);
    const bool queued = AddVisibleEntityForRender64(entity);
    if (queued) {
        s_lastQueuedVehicle64 = vehicle;
        s_lastVehicleQueueTick64 = now;
    }

    static uint32_t s_lastRememberVehicleLogTick = 0;
    if (now - s_lastRememberVehicleLogTick >= 700) {
        s_lastRememberVehicleLogTick = now;
        const CVector pos = vehicle->GetPosition();
        FLog("[VEH_REMEMBER64] samp=%u model=%u queued=%u rw=%p vis=%u use=%u phys=0x%08x area=%u pos=%.2f %.2f %.2f",
             static_cast<unsigned>(vehicleId),
             static_cast<unsigned>(vehicle->m_nModelIndex),
             queued ? 1 : 0,
             vehicle->m_pRwObject,
             vehicle->m_bIsVisible ? 1 : 0,
             vehicle->m_bUsesCollision ? 1 : 0,
             vehicle->m_nPhysicalFlags,
             static_cast<unsigned>(vehicle->m_nAreaCode),
             pos.x,
             pos.y,
             pos.z);
    }
}

static void EnsureLocalVehicleQueuedForRender64(const char* stage)
{
    bool inVehicle = false;
    VEHICLEID vehicleId = INVALID_VEHICLE_ID;
    CVehicleGTA* vehicle = GetTrackedLocalVehicleForRender64(&inVehicle, &vehicleId);
    if (!vehicle || !PrepareVehicleForRender64(vehicle)) {
        return;
    }

    CEntityGTA* entity = reinterpret_cast<CEntityGTA*>(vehicle);
    const bool alreadyQueued = IsEntityQueuedForRender64(entity);
    if (!alreadyQueued) {
        PreRenderVehicleForQueue64(vehicle);
    }
    const bool queued = AddVisibleEntityForRender64(entity);
    if (queued) {
        s_lastQueuedVehicle64 = vehicle;
        s_lastVehicleQueueTick64 = GetTickCount();
    }

    static uint32_t s_lastVehicleQueueLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastVehicleQueueLogTick >= 700) {
        s_lastVehicleQueueLogTick = now;
        const CVector pos = vehicle->GetPosition();
        const uint32_t driverState = vehicle->pDriver ? static_cast<uint32_t>(vehicle->pDriver->m_nPedState) : 0xFFFFFFFFu;
        FLog("[VEH_QUEUE64] stage=%s samp=%u model=%u rw=%p queued=%u already=%u inVeh=%u count=%d visible=%u fade=%u driver=%p driverState=%u pos=%.2f %.2f %.2f",
             stage ? stage : "?",
             static_cast<unsigned>(vehicleId),
             static_cast<unsigned>(vehicle->m_nModelIndex),
             vehicle->m_pRwObject,
             queued ? 1 : 0,
             alreadyQueued ? 1 : 0,
             inVehicle ? 1 : 0,
             CRenderer::ms_nNoOfVisibleEntities,
             vehicle->m_bIsVisible ? 1 : 0,
             vehicle->m_nVehicleFlags.bFadeOut ? 1 : 0,
             vehicle->pDriver,
             driverState,
             pos.x,
             pos.y,
             pos.z);
    }
}

static void RenderTrackedLocalVehicleMainPass64(const char* stage)
{
    bool inVehicle = false;
    VEHICLEID vehicleId = INVALID_VEHICLE_ID;
    CVehicleGTA* vehicle = GetTrackedLocalVehicleForRender64(&inVehicle, &vehicleId);
    if (!vehicle || !PrepareVehicleForRender64(vehicle)) {
        return;
    }

    DefinedState();
    RenderEntity(reinterpret_cast<CEntityGTA*>(vehicle));
    vehicle->RenderDriverAndPassengers();

    s_lastMainPassRenderedVehicle64 = vehicle;
    s_lastMainPassRenderTick64 = GetTickCount();

    static uint32_t s_lastVehicleMainPassLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastVehicleMainPassLogTick >= 700) {
        s_lastVehicleMainPassLogTick = now;
        const CVector pos = vehicle->GetPosition();
        const uint32_t driverState = vehicle->pDriver ? static_cast<uint32_t>(vehicle->pDriver->m_nPedState) : 0xFFFFFFFFu;
        FLog("[VEH_RENDER64] stage=%s samp=%u model=%u rw=%p main=1 inVeh=%u visible=%u fade=%u driver=%p driverState=%u pos=%.2f %.2f %.2f",
             stage ? stage : "?",
             static_cast<unsigned>(vehicleId),
             static_cast<unsigned>(vehicle->m_nModelIndex),
             vehicle->m_pRwObject,
             inVehicle ? 1 : 0,
             vehicle->m_bIsVisible ? 1 : 0,
             vehicle->m_nVehicleFlags.bFadeOut ? 1 : 0,
             vehicle->pDriver,
             driverState,
             pos.x,
             pos.y,
             pos.z);
    }
}

static void RenderLocalVehicleBodyFallback64()
{
    bool inVehicle = false;
    VEHICLEID vehicleId = INVALID_VEHICLE_ID;
    CVehicleGTA* vehicle = GetTrackedLocalVehicleForRender64(&inVehicle, &vehicleId);
    if (!vehicle || !PrepareVehicleForRender64(vehicle)) {
        return;
    }

    const uint32_t now = GetTickCount();
    const bool queuedThisFrame = vehicle == s_lastQueuedVehicle64 && now - s_lastVehicleQueueTick64 < 250;
    const bool renderedMainPass = vehicle == s_lastMainPassRenderedVehicle64 && now - s_lastMainPassRenderTick64 < 250;
    CEntityGTA* entity = reinterpret_cast<CEntityGTA*>(vehicle);
    const bool queued = IsEntityQueuedForRender64(entity) || queuedThisFrame || renderedMainPass;
    if (!queued) {
        DefinedState();
        RenderEntity(entity);
        vehicle->RenderDriverAndPassengers();
    }

    static uint32_t s_lastLocalVehicleRenderLogTick = 0;
    if (now - s_lastLocalVehicleRenderLogTick >= 1000) {
        s_lastLocalVehicleRenderLogTick = now;
        CCamera& camera = GTASAEngineApi::Camera();
        const uint8_t activeIndex = camera.m_nActiveCam < 3 ? camera.m_nActiveCam : 0;
        CCam& activeCam = camera.m_aCams[activeIndex];
        const CVector pos = vehicle->GetPosition();
        FLog("[VEH_RENDER64] local samp=%u model=%u rw=%p queued=%u manual=%u main=%u inVeh=%u visible=%u mode=%u goto=%u ctrl=%d pos=%.2f %.2f %.2f cam=%.2f %.2f %.2f",
             static_cast<unsigned>(vehicleId),
             static_cast<unsigned>(vehicle->m_nModelIndex),
             vehicle->m_pRwObject,
             queued ? 1 : 0,
             queued ? 0 : 1,
             renderedMainPass ? 1 : 0,
             inVehicle ? 1 : 0,
             vehicle->m_bIsVisible ? 1 : 0,
             static_cast<unsigned>(activeCam.m_nMode),
             static_cast<unsigned>(camera.m_nModeToGoTo),
             camera.WhoIsInControlOfTheCamera,
             pos.x,
             pos.y,
             pos.z,
             activeCam.Source.x,
             activeCam.Source.y,
             activeCam.Source.z);
    }
}

static void RepairNearbyVehiclesForRender64(const char* stage)
{
    if (!pNetGame || !pGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
        return;
    }

    static uint32_t s_lastRepairTick = 0;
    static uint32_t s_lastRepairLogTick = 0;
    const uint32_t now = GetTickCount();
    const bool forcedRefresh = IsLocalVehicleRenderRefreshActive64(now);
    const uint32_t repairInterval = forcedRefresh ? 60u : 350u;
    if (now - s_lastRepairTick < repairInterval) {
        return;
    }
    s_lastRepairTick = now;

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
    CPlayerPed* localPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;
    if (!localPed || !localPed->m_pPed) {
        return;
    }

    CVehiclePool* vehiclePool = pNetGame->GetVehiclePool();
    if (!vehiclePool) {
        return;
    }

    const CVector playerPos = localPed->m_pPed->GetPosition();
    const uint8_t playerArea = NormalizeLocalExteriorArea64(localPed, playerPos, "vehicle-render-repair");
    constexpr float kRepairRadius = 125.0f;
    constexpr float kAreaRepairRadius = 85.0f;
    constexpr float kQueueKickRadius = 70.0f;
    const float repairRadius2 = kRepairRadius * kRepairRadius;
    const float areaRepairRadius2 = kAreaRepairRadius * kAreaRepairRadius;
    const float queueKickRadius2 = kQueueKickRadius * kQueueKickRadius;

    int scanned = 0;
    int near = 0;
    int requested = 0;
    int repaired = 0;
    int areaFixed = 0;
    int queued = 0;

    for (VEHICLEID id = 0; id < MAX_VEHICLES; ++id) {
        CVehicle* sampVehicle = vehiclePool->GetAt(id);
        if (!sampVehicle || !sampVehicle->m_pVehicle) {
            continue;
        }

        ++scanned;
        CVehicleGTA* vehicle = sampVehicle->m_pVehicle;
        const CVector pos = vehicle->GetPosition();
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) {
            continue;
        }

        const float dist2 = DistanceSquared64(playerPos, pos);
        if (dist2 > repairRadius2) {
            continue;
        }
        ++near;

        uint8_t vehicleArea = static_cast<uint8_t>(vehicle->m_nAreaCode);
        if (vehicleArea != playerArea &&
            dist2 <= areaRepairRadius2) {
            vehicle->SetInterior(static_cast<int>(playerArea), false);
            vehicleArea = playerArea;
            ++areaFixed;
        }

        if (vehicleArea != playerArea) {
            continue;
        }

        const int modelId = static_cast<int>(vehicle->m_nModelIndex);
        if (modelId < 0 || modelId >= CModelInfo::NUM_MODEL_INFOS) {
            continue;
        }

        const bool needsRepair =
            !vehicle->m_pRwObject ||
            !vehicle->m_bIsVisible ||
            vehicle->m_bRemoveFromWorld ||
            !vehicle->m_bStreamingDontDelete ||
            vehicle->m_bDontStream ||
            !vehicle->m_bUsesCollision ||
            !vehicle->physicalFlags.bCollidable ||
            !vehicle->physicalFlags.bCanBeCollidedWith ||
            vehicle->physicalFlags.bDisableSimpleCollision ||
            vehicle->m_nVehicleFlags.bFadeOut;
        if (!needsRepair) {
            CEntityGTA* entity = reinterpret_cast<CEntityGTA*>(vehicle);
            if (dist2 <= queueKickRadius2 &&
                vehicle->m_pRwObject &&
                vehicle->m_bIsVisible &&
                !IsEntityQueuedForRender64(entity)) {
                PreRenderVehicleForQueue64(vehicle);
                if (AddVisibleEntityForRender64(entity)) {
                    ++queued;
                }
            }
            continue;
        }

        Supremo::Streaming::MarkModelDontRemoveInLoadScene(modelId);
        if (!Supremo::Streaming::IsModelLoaded(modelId)) {
            // First try a fast synchronous load (TryLoadModel handles priority internally)
            const bool fastLoaded = Supremo::Streaming::TryLoadModel(modelId);
            if (!fastLoaded) {
                // Fallback: queue a priority request and process the load queue now
                Supremo::Streaming::RequestModel(modelId,
                    STREAMING_DONTREMOVE_IN_LOADSCENE | STREAMING_PRIORITY_REQUEST);
                Supremo::Streaming::LoadAllRequestedModels(true);
            }
            ++requested;
            // Even after a forced load attempt, re-check before trying to prepare
            if (!Supremo::Streaming::IsModelLoaded(modelId)) {
                continue;
            }
        }

        if (!PrepareVehicleForRender64(vehicle)) {
            continue;
        }

        ++repaired;
        CEntityGTA* entity = reinterpret_cast<CEntityGTA*>(vehicle);
        if (!IsEntityQueuedForRender64(entity)) {
            PreRenderVehicleForQueue64(vehicle);
        }
        if (AddVisibleEntityForRender64(entity)) {
            ++queued;
        }
    }

    if ((requested || repaired || areaFixed) && now - s_lastRepairLogTick >= 900) {
        s_lastRepairLogTick = now;
        FLog("[VEH_INTERIOR_REPAIR64] stage=%s scanned=%d near=%d req=%d repaired=%d queued=%d areaFixed=%d player=%.2f %.2f %.2f area=%u",
             stage ? stage : "?",
             scanned,
             near,
             requested,
             repaired,
             queued,
             areaFixed,
             playerPos.x,
             playerPos.y,
             playerPos.z,
             static_cast<unsigned>(playerArea));
    }
}
#endif

extern "C" void SupremoRequestVehicleRenderRefresh64()
{
#if !VER_x32
    const uint32_t now = GetTickCount();
    s_forceVehicleRenderRefreshUntilTick64.store(now + 4000u, std::memory_order_release);

    bool inVehicle = false;
    VEHICLEID vehicleId = INVALID_VEHICLE_ID;
    CVehicleGTA* vehicle = GetTrackedLocalVehicleForRender64(&inVehicle, &vehicleId);
    int modelId = -1;
    bool modelLoaded = false;
    bool queued = false;

    if (vehicle) {
        modelId = static_cast<int>(vehicle->m_nModelIndex);
        if (modelId >= 0 && modelId < CModelInfo::NUM_MODEL_INFOS) {
            Supremo::Streaming::MarkModelDontRemoveInLoadScene(modelId);
            if (!Supremo::Streaming::IsModelLoaded(modelId)) {
                Supremo::Streaming::RequestModel(modelId, STREAMING_DONTREMOVE_IN_LOADSCENE);
            }
            modelLoaded = Supremo::Streaming::IsModelLoaded(modelId);
        }

        if (PrepareVehicleForRender64(vehicle)) {
            CEntityGTA* entity = reinterpret_cast<CEntityGTA*>(vehicle);
            if (!IsEntityQueuedForRender64(entity)) {
                PreRenderVehicleForQueue64(vehicle);
            }
            queued = AddVisibleEntityForRender64(entity);
            if (queued) {
                s_lastQueuedVehicle64 = vehicle;
                s_lastVehicleQueueTick64 = now;
            }
        }
    }

    FLog("[VEH_RENDER_REFRESH64] requested until=%u vehicle=%p samp=%u model=%d loaded=%u queued=%u inVeh=%u",
         s_forceVehicleRenderRefreshUntilTick64.load(std::memory_order_relaxed),
         vehicle,
         static_cast<unsigned>(vehicleId),
         modelId,
         modelLoaded ? 1 : 0,
         queued ? 1 : 0,
         inVehicle ? 1 : 0);
#endif
}

/* =============================================================================== */

void RenderEffects() {
//	RenderEffects();
#if !VER_x32
    SuppressNativeLoadingDuringGameplay64("render-effects-begin");
    RestoreWorld3DRenderState64("render-effects-begin");
#endif
    using NativeEffectStage = GTASAEngineApi::NativeRenderEffectStage;
    GTASAEngineApi::RenderNativeEffectStage(NativeEffectStage::Stage0);
    GTASAEngineApi::RenderNativeEffectStage(NativeEffectStage::Stage1);
//    CRopes::Render();
//    CGlass::Render();
    GTASAEngineApi::RenderNativeEffectStage(NativeEffectStage::Stage2);
    CVisibilityPlugins::RenderReallyDrawLastObjects();
    CCoronas::Render();

    CCamera& TheCamera = GTASAEngineApi::Camera();
    GTASAEngineApi::RenderFx(GTASAEngineApi::FxManager(), TheCamera.m_pRwCamera, false);

    GTASAEngineApi::RenderNativeEffectStage(NativeEffectStage::Stage3);
    GTASAEngineApi::RenderNativeEffectStage(NativeEffectStage::Stage4);
    GTASAEngineApi::RenderNativeEffectStage(NativeEffectStage::Stage5);
    //   CClouds::VolumetricCloudsRender();
////    if (CHeli::NumberOfSearchLights || CTheScripts::NumberOfScriptSearchLights) {
////        CHeli::Pre_SearchLightCone();
////        CHeli::RenderAllHeliSearchLights();
////        CTheScripts::RenderAllSearchLights();
////        CHeli::Post_SearchLightCone();
////    }
    GTASAEngineApi::RenderNativeEffectStage(NativeEffectStage::Stage6);
////    if (CReplay::Mode != MODE_PLAYBACK && !CPad::GetPad(0)->DisablePlayerControls) {
////        FindPlayerPed()->DrawTriangleForMouseRecruitPed();
////    }
    GTASAEngineApi::RenderNativeEffectStage(NativeEffectStage::Stage7);
//    //CVehicleRecording::Render();
    GTASAEngineApi::RenderNativeEffectStage(NativeEffectStage::Stage8);
//    //CRenderer::RenderFirstPersonVehicle();
#if !VER_x32
    if (kArm64DirectRenderFallbackEnabled) {
        RenderLocalVehicleBodyFallback64();
    }
#endif
    GTASAEngineApi::RenderNativeEffectStage(NativeEffectStage::Stage9);

    //DebugModules::Render3D();
}

void (*Render2dStuffNative)();

void Render2dStuff_hook()
{
    // Keep the native pass authoritative for loading, connecting, chat and
    // radar. Gameplay HUD visibility is controlled by the existing HUD state;
    // readiness must not suppress the entire engine 2D pass.
    if (Render2dStuffNative) {
        Render2dStuffNative();
    }

#if !VER_x32
    // The installed hook is the active 2D path; drain queued texture marks
    // only after the native pass has established the render context.
    ProcessTextureWarmupQueue64();
#endif

    if (pNetGame)
    {
        CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();
        if (pTextDrawPool) pTextDrawPool->Draw();
    }

    if (pUI && pUI->renderer()) {
        pUI->render();
    }

#if !VER_x32
    RestoreWorld3DRenderState64("render2d-hook-after-ui");
#endif
}

void Render2dStuff()
{
    SuppressNativeLoadingDuringGameplay64("render2d-begin");
#if !VER_x32
    ProcessTextureWarmupQueue64();
#endif

    if (GTASAEngineApi::IsAltRenderTarget()) {
        GTASAEngineApi::FlushAltRenderTarget();
    }

    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, RWRSTATE(rwRENDERSTATENARENDERSTATE));
    RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLNONE));

    if (!SampUiController::IsEnabled()) {
        GTASAEngineApi::DrawNativeHud();
        GTASAEngineApi::DrawNativeTouchInterface(false);
    }

    if (!IsSampRuntimeActive64() && !SampUiController::IsEnabled()) {
        GTASAEngineApi::SetEmuGamma(true);
        GTASAEngineApi::DisplayMessages(true);
        GTASAEngineApi::RenderFontBuffer();
        GTASAEngineApi::SetEmuGamma(false);
    } else {
        GTASAEngineApi::SetEmuGamma(false);
    }

    SuppressNativeLoadingDuringGameplay64("render2d-end");
#if !VER_x32
    RestoreWorld3DRenderState64("render2d-end-next-frame");
#endif
}

/* =============================================================================== */

void (*CHud_DrawRadar)();
void CHud_DrawRadar_hook()
{
    if (SampUiController::ShouldSuppressNativeRadar()) {
        return;
    }

    CHud_DrawRadar();
}

/* =============================================================================== */

int (*CRadar__SetCoordBlip)(int r0, float X, float Y, float Z, int r4, int r5, char* name);
int CRadar__SetCoordBlip_hook(int r0, float X, float Y, float Z, int r4, int r5, char* name)
{
    if(pNetGame && !strncmp(name, "CODEWAY", 7))
    {
        float fFindZ = CWorld::FindGroundZForCoord(X, Y) + 1.5f;

        if(pNetGame->GetGameState() != GAMESTATE_CONNECTED) return 0;

        RakNet::BitStream bsSend;
        bsSend.Write(X);
        bsSend.Write(Y);
        bsSend.Write(fFindZ);
        pNetGame->GetRakClient()->RPC(&RPC_MapMarker, &bsSend, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
    }

    return CRadar__SetCoordBlip(r0, X, Y, Z, r4, r5, name);
}

/* =============================================================================== */

void(*CRadar_DrawRadarGangOverlay)(uint32_t unk);
void CRadar_DrawRadarGangOverlay_hook(uint32_t unk)
{
    if (pNetGame)
    {
        CGangZonePool *pGangZonePool = pNetGame->GetGangZonePool();
        if (pGangZonePool) {
            pGangZonePool->Draw(unk);
        }
    }
}

#if !VER_x32
void (*CRadar__StreamRadarSections)(int x, int y);
void CRadar__StreamRadarSections_hook(int x, int y)
{
    TextureDatabaseRuntime* gta3 = TextureDatabaseRuntime::GetDatabase("gta3");
    const bool clippedGta3Etc = gta3 &&
                                gta3->loadedFormat == TextureDatabaseFormat::DF_ETC &&
                                gta3->entries.numEntries < 13239;

    if (clippedGta3Etc) {
        static bool logged = false;
        if (!logged) {
            FLog("[RADAR64] skip StreamRadarSections while gta3 ETC is clipped entries=%u",
                 gta3->entries.numEntries);
            logged = true;
        }
        return;
    }

    CRadar__StreamRadarSections(x, y);
}

static bool gRadar64HooksInstalled = false;

static void InstallRadar64Hooks()
{
    if (gRadar64HooksInstalled) {
        return;
    }

    // The arm64 gta3 ETC thumb table is shorter than gta3.txt. Keep radar
    // streaming away from the clipped tail until those missing thumbs fallback.
    GTASAEngineApi::InstallRadarStreamSectionsHook(&CRadar__StreamRadarSections_hook, &CRadar__StreamRadarSections);
    gRadar64HooksInstalled = true;
    FLog("[RADAR64] StreamRadarSections hook installed");
}
#endif

/* =============================================================================== */

typedef struct {
    float x;
    float y;
    float z;
    float w;
} stFileObjectRotation;
VALIDATE_SIZE(stFileObjectRotation, 0x10);

typedef struct {
    CVector     vecPosObject;
    stFileObjectRotation m_qRotation;
    int32       wModelIndex;
    union {
        struct { // CFileObjectInstanceType
            uint32 m_nAreaCode : 8;
            uint32 m_bRedundantStream : 1;
            uint32 m_bDontStream : 1; // Merely assumed, no countercheck possible.
            uint32 m_bUnderwater : 1;
            uint32 m_bTunnel : 1;
            uint32 m_bTunnelTransition : 1;
            uint32 m_nReserved : 19;
        };
        uint32 m_nInstanceType;
    };
    int32 m_nLodInstanceIndex; // -1 - without LOD model
} stLoadObjectInstance;
VALIDATE_SIZE(stLoadObjectInstance, (VER_x32 ? 0x28 : 0x28));

extern int iBuildingToRemoveCount;
extern REMOVEBUILDING_DATA BuildingToRemove[1000];

int (*CFileLoader__LoadObjectInstance)(stLoadObjectInstance *thiz);
int CFileLoader__LoadObjectInstance_hook(stLoadObjectInstance *thiz) {
    if (thiz) {
        if (iBuildingToRemoveCount >= 1) {
            for (int i = 0; i < iBuildingToRemoveCount; i++)
            {
                float fDistance = GetDistance(BuildingToRemove[i].vecPos, thiz->vecPosObject);
                if (fDistance <= BuildingToRemove[i].fRange) {
                    if (BuildingToRemove[i].dwModel == -1 || thiz->wModelIndex == (uint16_t) BuildingToRemove[i].dwModel) {
                        thiz->wModelIndex = 19300;
                        //thiz->vecPosObject = 0.0f;
                        break;
                    }
                }
            }
        }
    }

    return CFileLoader__LoadObjectInstance(thiz);
}

extern int iBuildingToRemoveCount;
extern std::list<REMOVE_BUILDING_DATA> RemoveBuildingData;
void (*CEntity_Render)(CEntityGTA* pEntity);
int g_iLastRenderedObject;

#if !VER_x32
static bool ContainsAsciiNoCaseBounded64(const char* text, size_t maxLen, const char* needle)
{
    if (!text || !needle || !needle[0]) {
        return false;
    }

    size_t needleLen = 0;
    while (needle[needleLen]) {
        ++needleLen;
    }

    if (needleLen == 0 || needleLen > maxLen) {
        return false;
    }

    for (size_t i = 0; i + needleLen <= maxLen && text[i]; ++i) {
        bool match = true;
        for (size_t j = 0; j < needleLen; ++j) {
            char a = text[i + j];
            char b = needle[j];
            if (!a) {
                match = false;
                break;
            }
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
            if (a != b) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }

    return false;
}

static const RpMaterial* FindLoadingTextureMaterial64(const CEntityGTA* entity, int* materialIndex)
{
    if (materialIndex) {
        *materialIndex = -1;
    }
    if (!entity || !entity->m_pRwObject || entity->m_pRwObject->type != rpATOMIC || !entity->m_pRwAtomic) {
        return nullptr;
    }

    RpGeometry* geometry = entity->m_pRwAtomic->geometry;
    if (!geometry || !geometry->matList.materials || geometry->matList.numMaterials <= 0) {
        return nullptr;
    }

    const int materialCount = std::min<int>(geometry->matList.numMaterials, 64);
    for (int i = 0; i < materialCount; ++i) {
        const RpMaterial* material = geometry->matList.materials[i];
        if (!material || !material->texture) {
            continue;
        }

        const RwTexture* texture = material->texture;
        if (ContainsAsciiNoCaseBounded64(texture->name, sizeof(texture->name), "loading") ||
            ContainsAsciiNoCaseBounded64(texture->mask, sizeof(texture->mask), "loading")) {
            if (materialIndex) {
                *materialIndex = i;
            }
            return material;
        }
    }

    return nullptr;
}

static bool IsSuspiciousRenderTextureName64(const char* textureName)
{
    return ContainsAsciiNoCaseBounded64(textureName, 32, "loading") ||
           ContainsAsciiNoCaseBounded64(textureName, 32, "runway") ||
           ContainsAsciiNoCaseBounded64(textureName, 32, "rnway") ||
           ContainsAsciiNoCaseBounded64(textureName, 32, "ap_run") ||
           ContainsAsciiNoCaseBounded64(textureName, 32, "sign") ||
           ContainsAsciiNoCaseBounded64(textureName, 32, "corrugated");
}

static bool GeometryHasSuspiciousRenderTexture64(const RpGeometry* geometry)
{
    if (!geometry || !geometry->matList.materials || geometry->matList.numMaterials <= 0) {
        return false;
    }

    const int materialCount = std::min<int>(geometry->matList.numMaterials, 64);
    for (int i = 0; i < materialCount; ++i) {
        const RpMaterial* material = geometry->matList.materials[i];
        const RwTexture* texture = material ? material->texture : nullptr;
        if (texture &&
            (IsSuspiciousRenderTextureName64(texture->name) ||
             IsSuspiciousRenderTextureName64(texture->mask))) {
            return true;
        }
    }

    return false;
}

static bool IsLodNameFast64(const char* modelName)
{
    return ContainsAsciiNoCaseBounded64(modelName, 21, "lod");
}

static bool ShouldSuppressNearbyLodRender64(CEntityGTA* entity)
{
    if (!entity || !pGame || !pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
        return false;
    }

    CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(entity->m_nModelIndex);
    if (!modelInfo || !IsLodNameFast64(modelInfo->m_modelName)) {
        return false;
    }

    CPlayerPed* playerPed = pGame->FindPlayerPed();
    if (!playerPed || !playerPed->m_pPed) {
        return false;
    }

    const CVector entityPos = entity->GetPosition();
    const CVector playerPos = playerPed->m_pPed->GetPosition();
    const float playerDist2 = DistanceSquared64(entityPos, playerPos);

    CCamera& camera = GTASAEngineApi::Camera();
    const uint8_t activeIndex = camera.m_nActiveCam < 3 ? camera.m_nActiveCam : 0;
    const CVector cameraPos = camera.m_aCams[activeIndex].Source;
    const float cameraDist2 = DistanceSquared64(entityPos, cameraPos);
    constexpr float kSuppressNearbyLodRadius = 360.0f;
    if (playerDist2 > kSuppressNearbyLodRadius * kSuppressNearbyLodRadius &&
        cameraDist2 > kSuppressNearbyLodRadius * kSuppressNearbyLodRadius) {
        return false;
    }

    static uint32_t s_lastLodSkipLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastLodSkipLogTick >= 1200u) {
        s_lastLodSkipLogTick = now;
        FLog("[LOD_CULL64] renderSkip model=%d name=%s pos=%.2f %.2f %.2f playerD2=%.1f cameraD2=%.1f",
             static_cast<int>(entity->m_nModelIndex),
             modelInfo->m_modelName,
             entityPos.x,
             entityPos.y,
             entityPos.z,
             playerDist2,
             cameraDist2);
    }

    return true;
}

static bool ShouldSuppressBrokenAirportRender64(CEntityGTA* entity)
{
    if (!entity || !pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
        return false;
    }

    const int modelId = static_cast<int>(entity->m_nModelIndex);
    CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(entity->m_nModelIndex);
    const char* modelName = modelInfo ? modelInfo->m_modelName : "?";
    const bool isBrokenRunwaySign =
        modelId == 3666 && ContainsAsciiNoCaseBounded64(modelName, 21, "airuntest");
    const bool isBrokenAirportTerminalLod =
        modelId == 5012 && ContainsAsciiNoCaseBounded64(modelName, 21, "lodairprterm");

    if (!isBrokenRunwaySign && !isBrokenAirportTerminalLod) {
        return false;
    }

    static uint32_t s_lastBrokenAirportSkipLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastBrokenAirportSkipLogTick >= 1200u) {
        s_lastBrokenAirportSkipLogTick = now;
        const CVector pos = entity->GetPosition();
        FLog("[AIRPORT_RENDER_FIX64] skipped broken airport render model=%d name=%s pos=%.2f %.2f %.2f",
             modelId,
             modelName,
             pos.x,
             pos.y,
             pos.z);
    }

    return true;
}

static void LogLoadingTextureEntity64(const CEntityGTA* entity, const RpMaterial* material, int materialIndex)
{
    static uint32_t s_lastLogTick = 0;
    static uint32_t s_logCount = 0;
    const uint32_t now = GetTickCount();
    if (s_logCount >= 24 || now - s_lastLogTick < 350u) {
        return;
    }

    s_lastLogTick = now;
    ++s_logCount;

    const int modelId = entity ? static_cast<int>(entity->m_nModelIndex) : -1;
    const CBaseModelInfo* modelInfo = entity ? CModelInfo::GetModelInfo(entity->m_nModelIndex) : nullptr;
    const char* modelName = modelInfo ? modelInfo->m_modelName : "?";
    const RwTexture* texture = material ? material->texture : nullptr;
    const char* textureName = texture ? texture->name : "?";
    const char* textureMask = texture ? texture->mask : "?";
    const CVector pos = entity ? entity->GetPosition() : CVector{};

    FLog("[TEX_MISS64] loading-texture entity model=%d name=%s mat=%d tex=%s mask=%s pos=%.2f %.2f %.2f",
         modelId,
         modelName,
         materialIndex,
         textureName,
         textureMask,
         pos.x,
         pos.y,
         pos.z);
}

static void LogSuspiciousRenderEntity64(const CEntityGTA* entity)
{
    if (!entity || !entity->m_pRwObject) {
        return;
    }

    const int modelId = static_cast<int>(entity->m_nModelIndex);
    const CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(entity->m_nModelIndex);
    const char* modelName = modelInfo ? modelInfo->m_modelName : "?";
    const bool isMaterialTextRange = modelId >= 19300 && modelId <= 19599;
    const bool isAirportOrSign =
        ContainsAsciiNoCaseBounded64(modelName, 21, "air") ||
        ContainsAsciiNoCaseBounded64(modelName, 21, "sign") ||
        ContainsAsciiNoCaseBounded64(modelName, 21, "bill") ||
        ContainsAsciiNoCaseBounded64(modelName, 21, "rnway") ||
        ContainsAsciiNoCaseBounded64(modelName, 21, "runway");

    const RpMaterial* mats[8]{};
    int materialCount = 0;
    bool hasSuspiciousTexture = false;
    if (entity->m_pRwObject->type == rpATOMIC && entity->m_pRwAtomic && entity->m_pRwAtomic->geometry) {
        RpMaterialList& matList = entity->m_pRwAtomic->geometry->matList;
        materialCount = matList.numMaterials;
        if (matList.materials) {
            const int maxProbeMaterialCount = static_cast<int>(sizeof(mats) / sizeof(mats[0]));
            const int probeMaterialCount = materialCount < maxProbeMaterialCount ? materialCount : maxProbeMaterialCount;
            for (int i = 0; i < probeMaterialCount; ++i) {
                mats[i] = matList.materials[i];
            }
        }
        hasSuspiciousTexture = GeometryHasSuspiciousRenderTexture64(entity->m_pRwAtomic->geometry);
    }

    if (!isMaterialTextRange && !isAirportOrSign && !hasSuspiciousTexture) {
        return;
    }

    const char* tex0 = (mats[0] && mats[0]->texture) ? mats[0]->texture->name : "-";
    const char* tex1 = (mats[1] && mats[1]->texture) ? mats[1]->texture->name : "-";
    const char* tex2 = (mats[2] && mats[2]->texture) ? mats[2]->texture->name : "-";
    const char* tex3 = (mats[3] && mats[3]->texture) ? mats[3]->texture->name : "-";
    const char* tex4 = (mats[4] && mats[4]->texture) ? mats[4]->texture->name : "-";
    const char* tex5 = (mats[5] && mats[5]->texture) ? mats[5]->texture->name : "-";
    const char* tex6 = (mats[6] && mats[6]->texture) ? mats[6]->texture->name : "-";
    const char* tex7 = (mats[7] && mats[7]->texture) ? mats[7]->texture->name : "-";

    static int s_loggedModels[128]{};
    static int s_loggedModelCount = 0;
    for (int i = 0; i < s_loggedModelCount; ++i) {
        if (s_loggedModels[i] == modelId) {
            return;
        }
    }
    if (s_loggedModelCount < static_cast<int>(sizeof(s_loggedModels) / sizeof(s_loggedModels[0]))) {
        s_loggedModels[s_loggedModelCount++] = modelId;
    }

    static uint32_t s_lastProbeTick = 0;
    static uint32_t s_probeCount = 0;
    const uint32_t now = GetTickCount();
    if (s_probeCount >= 96 || now - s_lastProbeTick < 20u) {
        return;
    }
    s_lastProbeTick = now;
    ++s_probeCount;

    const CVector pos = entity->GetPosition();
    FLog("[RENDER_PROBE64] model=%d name=%s type=%u mats=%d tex={%s|%s|%s|%s|%s|%s|%s|%s} flags=0x%08X vis=%u pos=%.2f %.2f %.2f",
         modelId,
         modelName,
         entity->m_pRwObject ? static_cast<unsigned>(entity->m_pRwObject->type) : 0u,
         materialCount,
         tex0,
         tex1,
         tex2,
         tex3,
         tex4,
         tex5,
         tex6,
         tex7,
         entity->m_nFlags,
         entity->m_bIsVisible ? 1u : 0u,
         pos.x,
         pos.y,
         pos.z);
}
#endif

void CEntity_Render_hook(CEntityGTA* pEntity)
{
    if(iBuildingToRemoveCount > 1)
    {
        if(pEntity && !GTASAEngineApi::IsBasePlaceable(reinterpret_cast<CPhysical*>(pEntity)) && !pNetGame->GetObjectPool()->GetObjectFromGtaPtr(pEntity))
        {
            for (auto &entry : RemoveBuildingData)
            {
                float fDistance = GetDistance(entry.vecPos, pEntity->GetMatrix().m_pos);
                if(fDistance <= entry.fRange)
                {
                    if(pEntity->GetModelId() == entry.usModelIndex)
                    {
                        pEntity->m_bUsesCollision = 0;
                        pEntity->m_bCollisionProcessed = 0;
                        return;
                    }
                }
            }
        }
    }

#if !VER_x32
    LogSuspiciousRenderEntity64(pEntity);

    if (ShouldSuppressBrokenAirportRender64(pEntity)) {
        return;
    }

    int loadingMaterialIndex = -1;
    const RpMaterial* loadingMaterial = FindLoadingTextureMaterial64(pEntity, &loadingMaterialIndex);
    if (loadingMaterial) {
        LogLoadingTextureEntity64(pEntity, loadingMaterial, loadingMaterialIndex);
        return;
    }

    if (ShouldSuppressNearbyLodRender64(pEntity)) {
        return;
    }
#endif

    g_iLastRenderedObject = pEntity->GetModelId();
    CEntity_Render(pEntity);
}

/* =============================================================================== */

/* =============================================================================== */

void (*CObject_Render)(CObjectGta* thiz);
void CObject_Render_hook(CObjectGta* thiz)
{
    CObjectGta *object = thiz;
    if(object != 0)
    {
        if(pNetGame)
        {
            CObject *pObject = pNetGame->GetObjectPool()->FindObjectFromGtaPtr(object);
            if(pObject && pObject->m_pEntity)
            {
                if (pObject->HasVisiblePlaceholderMaterialText()) {
                    static uint32_t s_lastPlaceholderSkipLogTick = 0;
                    const uint32_t now = GetTickCount();
                    if (now - s_lastPlaceholderSkipLogTick >= 2000u) {
                        s_lastPlaceholderSkipLogTick = now;
                        FLog("[OBJ_PLACEHOLDER64] skipped Loading material-text object model=%d",
                             object->GetModelId());
                    }
                    return;
                }

                RwObject* rwObject = (RwObject*)pObject->m_pEntity->m_pRwObject;
                if(rwObject)
                {
                    // SetObjectMaterial
                    if(pObject->m_bHasMaterial && object->m_pRwAtomic && object->m_pRwAtomic->geometry)
                    {
                        RpGeometryForAllMaterials(object->m_pRwAtomic->geometry, ObjectMaterialCallBack, (void*)pObject);
                    }
                    // SetObjectMaterialText
                    if(pObject->m_bHasMaterialText)
                    {
                        RwFrameForAllObjects((RwFrame*)rwObject->parent, (RwObject *(*)(RwObject *, void *))ObjectMaterialTextCallBack, pObject);
                    }
                }


            }
        }

        CObject_Render(object);
    }

}

#if !VER_x32
using CObjectFromDummyCtorFn = void (*)(void* thiz, void* dummyObject);
static CObjectFromDummyCtorFn CObjectFromDummyCtor = nullptr;

static void CObjectFromDummyCtor_hook(void* thiz, void* dummyObject)
{
    if (!thiz) {
        static uint32_t s_nullCtorLogCount = 0;
        if (s_nullCtorLogCount < 20) {
            ++s_nullCtorLogCount;
            auto* pool = GetObjectPoolGta();
            const int used = pool ? static_cast<int>(pool->GetNoOfUsedSpaces()) : -1;
            const int capacity = pool ? pool->m_nSize : -1;
            FLog("[OBJ_POOL64] skipped null CObject(CDummyObject*) dummy=%p used=%d/%d",
                 dummyObject,
                 used,
                 capacity);
        }
        return;
    }

    CObjectFromDummyCtor(thiz, dummyObject);
}
#endif

/* =============================================================================== */

/* =============================================================================== */

bool NotifyEnterVehicle(CVehicleGTA *_pVehicle)
{
    if(!pNetGame) {
        return false;
    }

    CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
    if(!pVehiclePool) {
        return false;
    }

    CVehicle *pVehicle = nullptr;
    VEHICLEID VehicleID = pVehiclePool->FindIDFromGtaPtr(_pVehicle);

    if(VehicleID <= 0 || VehicleID >= MAX_VEHICLES) {
        return false;
    }

    if(!pVehiclePool->GetSlotState(VehicleID)) {
        return false;
    }

    pVehicle = pVehiclePool->GetAt(VehicleID);
    if(!pVehicle) {
        return false;
    }

    CLocalPlayer *pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();

    if(pLocalPlayer) {
        FLog("Vehicle ID: %d", VehicleID);
        pLocalPlayer->SendEnterVehicleNotification(VehicleID, false);
    }

    return true;
}

int (*TaskEnterVehicle)(uintptr_t a1, uintptr_t a2);
int TaskEnterVehicleHook(uintptr_t a1, uintptr_t a2)
{
    if(!NotifyEnterVehicle((CVehicleGTA*)a1)) {
        return false;
    }

    uintptr_t pTask = GTASAEngineApi::CreateTask();
    GTASAEngineApi::ConstructEnterCarAsDriverTask(pTask, a1);
    GTASAEngineApi::SetTaskManagerTask(a2, pTask, 3, 0);

    return true;
}

void (*CTaskComplexLeaveCar)(uintptr_t** thiz, CVehicleGTA* pVehicle, int iTargetDoor, int iDelayTime, bool bSensibleLeaveCar, bool bForceGetOut);
void CTaskComplexLeaveCar_hook(uintptr_t** thiz, CVehicleGTA* pVehicle, int iTargetDoor, int iDelayTime, bool bSensibleLeaveCar, bool bForceGetOut)
{
    uintptr_t dwRetAddr = 0;
    __asm__ volatile ("mov %0, lr" : "=r" (dwRetAddr));
    dwRetAddr -= g_libGTASA;

    if (dwRetAddr == 0x409A42+1 || dwRetAddr == 0x40A818+1)
    {
        if (pNetGame)
        {
            if ((CVehicleGTA*)GamePool_FindPlayerPed()->pVehicle == pVehicle)
            {
                CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
                VEHICLEID VehicleID = pVehiclePool->FindIDFromGtaPtr((CVehicleGTA*)GamePool_FindPlayerPed()->pVehicle);
                if (VehicleID != INVALID_VEHICLE_ID)
                {
                    CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
                    CLocalPlayer* pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();
                    if (pVehicle && pLocalPlayer)
                    {
                        if (pVehicle->IsATrainPart())
                        {
                            RwMatrix mat = pVehicle->m_pVehicle->GetMatrix().ToRwMatrix();
                            pLocalPlayer->GetPlayerPed()->RemoveFromVehicleAndPutAt(mat.pos.x + 2.5f, mat.pos.y + 2.5f, mat.pos.z);
                        }
                        else
                        {
                            pLocalPlayer->SendExitVehicleNotification(VehicleID);
                        }
                    }
                }
            }
        }
    }

    (*CTaskComplexLeaveCar)(thiz, pVehicle, iTargetDoor, iDelayTime, bSensibleLeaveCar, bForceGetOut);
}

/* =============================================================================== */

uint32_t CRadar__GetRadarTraceColor(uint32_t color, uint8_t bright, uint8_t friendly)
{
    return TranslateColorCodeToRGBA(color);
}

#if VER_x32
uint32_t CHudColours__GetIntColour(uint32 colour_id)
{
	return TranslateColorCodeToRGBA(colour_id);
}
#else
uint32_t CHudColours__GetIntColour(uintptr* thiz, uint8 colour_id)
{
    return TranslateColorCodeToRGBA(colour_id);
}
#endif

/* =============================================================================== */

void (*AND_TouchEvent)(int type, int num, int posX, int posY);
static void DispatchOriginalAndroidTouchEvent(int type, int num, int posX, int posY)
{
    if (AND_TouchEvent) {
        AND_TouchEvent(type, num, posX, posY);
    }
}

void AND_TouchEvent_hook(int type, int num, int posX, int posY)
{
    if (g_bDirectTouchConsumed) {
        g_bDirectTouchConsumed = false;
        DispatchOriginalAndroidTouchEvent(1, 0, 0, 0);
        return;
    }

    // imgui
    //bool bRet = pUI->OnTouchEvent(type, num, posX, posY);

    if (!AND_TouchEvent) {
        return;
    }

    if (!pGame) {
        DispatchOriginalAndroidTouchEvent(type, num, posX, posY);
        return;
    }

    if (pGame->IsGamePaused())
    {
        DispatchOriginalAndroidTouchEvent(type, num, posX, posY);
        return;
    }

    // Java's direct bridge dispatches the widget tree and selectable textdraws.
    // Once it has been observed, this hook forwards only the remaining stock
    // game input so chat/textdraw events cannot be delivered twice.
    if (!g_bNativeImGuiTouchBridgeSeen && pUI != nullptr)
    {
        switch (type)
        {
            case 2: // push
                pUI->touchEvent(ImVec2(posX, posY), TouchType::push);
                break;

            case 3: // move
                pUI->touchEvent(ImVec2(posX, posY), TouchType::move);
                break;

            case 1: // pop
                pUI->touchEvent(ImVec2(posX, posY), TouchType::pop);
                break;
        }

        if (pUI->keyboard()->visible() || pUI->dialog()->visible() || SampUiController::WantsInputCapture()) {
            DispatchOriginalAndroidTouchEvent(1, 0, 0, 0);
            return;
        }
        else
        {
            if (pNetGame && pNetGame->GetTextDrawPool())
            {
                if (!pNetGame->GetTextDrawPool()->onTouchEvent(type, num, posX, posY)) {
                    DispatchOriginalAndroidTouchEvent(1, 0, 0, 0);
                    return;
                }
            }
        }
    }

    if (pGame->IsGameInputEnabled())
        DispatchOriginalAndroidTouchEvent(type, num, posX, posY);
    else
        DispatchOriginalAndroidTouchEvent(1, 0, 0, 0);
}

/* =============================================================================== */

/* =============================================================================== */

/* =============================================================================== */

uint32_t (*CPed__GetWeaponSkill)(CPedGTA *thiz);
uint32_t CPed__GetWeaponSkill_hook(CPedGTA *thiz)
{
    bool bWeaponSkillStored = false;

    dwCurPlayerActor = thiz;
    byteInternalPlayer = CWorld::PlayerInFocus;
    byteCurPlayer = FindPlayerNumFromPedPtr(dwCurPlayerActor);

    if(dwCurPlayerActor && byteCurPlayer != 0 && CWorld::PlayerInFocus == 0)
    {
        GameStoreLocalPlayerSkills();
        GameSetRemotePlayerSkills(byteCurPlayer);
        bWeaponSkillStored = true;
    }

    uint32_t result = GTASAEngineApi::GetPedWeaponSkill(thiz, thiz->m_aWeapons[thiz->m_nActiveWeaponSlot].dwType);

    if(bWeaponSkillStored)
    {
        GameSetLocalPlayerSkills();
        bWeaponSkillStored = false;
    }

    return result;
}

/* =============================================================================== */

extern CPlayerPed* g_pCurrentFiredPed;
extern BULLET_DATA* g_pCurrentBulletData;

extern int g_iLagCompensationMode;

void SendBulletSync(CVector* vecOrigin, CVector* a2, CColPoint *colPoint, CEntityGTA** ppEntity)
{
    CMatrix mat1, mat2;

    static BULLET_DATA bulletData;
    memset(&bulletData, 0, sizeof(BULLET_DATA));

    bulletData.vecOrigin.x = vecOrigin->x;
    bulletData.vecOrigin.y = vecOrigin->y;
    bulletData.vecOrigin.z = vecOrigin->z;

    bulletData.vecPos.x = colPoint->m_vecPoint.x;
    bulletData.vecPos.y = colPoint->m_vecPoint.y;
    bulletData.vecPos.z = colPoint->m_vecPoint.z;

    if (ppEntity)
    {
        CEntityGTA* pEntity = *ppEntity;
        if (pEntity)
        {
            if (g_iLagCompensationMode != 0)
            {
                bulletData.vecOffset.x = colPoint->m_vecPoint.x - pEntity->m_matrix->m_pos.x;
                bulletData.vecOffset.y = colPoint->m_vecPoint.y - pEntity->m_matrix->m_pos.y;
                bulletData.vecOffset.z = colPoint->m_vecPoint.z - pEntity->m_matrix->m_pos.z;
            }
            else
            {
                memset(&mat1, 0, sizeof(CMatrix));
                memset(&mat2, 0, sizeof(CMatrix));
                // RwMatrixOrthoNormalize
                auto entMat = pEntity->GetMatrix().ToRwMatrix();
                RwMatrixOrthoNormalize(reinterpret_cast<RwMatrix *>(&mat2), &entMat);
                // RwMatrixInvert
                Invert(mat1, mat2);

                ProjectMatrix(&bulletData.vecOffset, &mat1, &colPoint->m_vecPoint);
            }

            bulletData.pEntity = pEntity;
        }
        else bulletData.vecOffset = 0;
    }

    pGame->FindPlayerPed()->ProcessBulletData(&bulletData);
}

extern bool g_customFire;
/* 0.3.7 */
uint32_t(*CWeapon__FireInstantHit)(CWeapon* thiz, CPedGTA* pFiringEntity, CVector* vecOrigin, CVector* muzzlePosn, CEntityGTA* targetEntity,
        CVector* target, CVector* originForDriveBy, bool arg6, bool muzzle);
uint32_t CWeapon__FireInstantHit_hook(CWeapon* thiz, CPedGTA* pFiringEntity, CVector* vecOrigin, CVector* muzzlePosn, CEntityGTA* targetEntity,
        CVector* target, CVector* originForDriveBy, bool arg6, bool muzzle)
{
    if (pNetGame && pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->m_pPed)		// CWeapon::Fire
    {
        if(pFiringEntity != GamePool_FindPlayerPed())
            return muzzle;

        if(pNetGame)
        {
            pNetGame->GetPlayerPool()->ApplyCollisionChecking();
        }

        if(pGame)
        {
            CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
            if(pPlayerPed)
                pPlayerPed->FireInstant();
        }

        if(pNetGame)
        {
            pNetGame->GetPlayerPool()->ResetCollisionChecking();
        }

        return muzzle;
    }

    return CWeapon__FireInstantHit(thiz, pFiringEntity, vecOrigin, muzzlePosn, targetEntity,
            target, originForDriveBy, arg6, muzzle);
}

bool g_bForceWorldProcessLineOfSight = false;
uint32_t (*CWeapon__ProcessLineOfSight)(CVector *vecOrigin, CVector *vecEnd, CVector *vecPos, CPedGTA **ppEntity, CWeapon *pWeaponSlot, CPedGTA **ppEntity2, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7);
uint32_t CWeapon__ProcessLineOfSight_hook(CVector *vecOrigin, CVector *vecEnd, CVector *vecPos, CPedGTA **ppEntity, CWeapon *pWeaponSlot, CPedGTA **ppEntity2, bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7)
{
    uintptr_t dwRetAddr = 0;
    GET_LR(dwRetAddr);

    FLog("dwRetAddr CWeapon__ProcessLineOfSight_hook 0x%llx", dwRetAddr);
#if VER_x32
    if(dwRetAddr >= 0x005DC178 && dwRetAddr <= 0x005DD684)
		g_bForceWorldProcessLineOfSight = true;
#else
    if(dwRetAddr >= 0x701494 && dwRetAddr <= 0x702B18)
        g_bForceWorldProcessLineOfSight = true;
#endif

    return CWeapon__ProcessLineOfSight(vecOrigin, vecEnd, vecPos, ppEntity, pWeaponSlot, ppEntity2, b1, b2, b3, b4, b5, b6, b7);
}

uint32_t(*CWorld__ProcessLineOfSight)(CVector*, CVector*, CColPoint *colPoint, CEntityGTA**, bool, bool, bool, bool, bool, bool, bool, bool);
uint32_t CWorld__ProcessLineOfSight_hook(CVector* vecOrigin, CVector* vecEnd, CColPoint *colPoint, CEntityGTA** ppEntity,
        bool b1, bool b2, bool b3, bool b4, bool b5, bool b6, bool b7, bool b8)
{
    uintptr_t dwRetAddr = 0;
    GET_LR(dwRetAddr);

    if(dwRetAddr == (VER_x32 ? 0x005dd0b0 + 1 : 0x70253C) || g_bForceWorldProcessLineOfSight)
    {
        g_bForceWorldProcessLineOfSight = false;
        //LOGI("CWorld_ProcessLineOfSight iLagCompensationMode: %d", g_iLagCompensationMode);
        static CVector vecPosPlusOffset;

        if (g_iLagCompensationMode != 2)
        {
            if (g_pCurrentFiredPed != pGame->FindPlayerPed())
            {
                if (g_pCurrentBulletData && g_pCurrentBulletData->pEntity)
                {
                    if (!GTASAEngineApi::IsBasePlaceable(reinterpret_cast<CPhysical*>(g_pCurrentBulletData->pEntity)))
                    {
                        if (g_iLagCompensationMode)
                        {
                            vecPosPlusOffset.x = g_pCurrentBulletData->pEntity->GetPosition().x + g_pCurrentBulletData->vecOffset.x;
                            vecPosPlusOffset.y = g_pCurrentBulletData->pEntity->GetPosition().y + g_pCurrentBulletData->vecOffset.y;
                            vecPosPlusOffset.z = g_pCurrentBulletData->pEntity->GetPosition().z + g_pCurrentBulletData->vecOffset.z;
                        }
                        else
                        {
                            //FLog("vecPosPlusOffset %f %f %f", vecPosPlusOffset.x, vecPosPlusOffset.y, vecPosPlusOffset.z);
                            //FLog("pEntity->GetMatrix().m_up %f %f %f", g_pCurrentBulletData->pEntity->GetMatrix().m_up.x, g_pCurrentBulletData->pEntity->GetMatrix().m_up.y, g_pCurrentBulletData->pEntity->GetMatrix().m_up.z);
                            //FLog("g_pCurrentBulletData->vecOffset %f %f %f", g_pCurrentBulletData->vecOffset.x, g_pCurrentBulletData->vecOffset.y, g_pCurrentBulletData->vecOffset.z);
                            ProjectMatrix((CVector*)&vecPosPlusOffset, &g_pCurrentBulletData->pEntity->GetMatrix(), &g_pCurrentBulletData->vecOffset);
                            //vecPosPlusOffset.x = pEntity->GetMatrix().m_up.x * g_pCurrentBulletData->vecOffset.z + pEntity->GetMatrix().m_forward.x * g_pCurrentBulletData->vecOffset.y + pEntity->GetMatrix().m_right.x * g_pCurrentBulletData->vecOffset.x + pEntity->GetMatrix().m_pos.x;
                            //vecPosPlusOffset.y = pEntity->GetMatrix().m_up.y * g_pCurrentBulletData->vecOffset.z + pEntity->GetMatrix().m_forward.y * g_pCurrentBulletData->vecOffset.y + pEntity->GetMatrix().m_right.y * g_pCurrentBulletData->vecOffset.x + pEntity->GetMatrix().m_pos.y;
                            //vecPosPlusOffset.z = pEntity->GetMatrix().m_up.z * g_pCurrentBulletData->vecOffset.z + pEntity->GetMatrix().m_forward.z * g_pCurrentBulletData->vecOffset.y + pEntity->GetMatrix().m_right.z * g_pCurrentBulletData->vecOffset.x + pEntity->GetMatrix().m_pos.z;
                        }

                        vecEnd->x = vecPosPlusOffset.x - vecOrigin->x + vecPosPlusOffset.x;
                        vecEnd->y = vecPosPlusOffset.y - vecOrigin->y + vecPosPlusOffset.y;
                        vecEnd->z = vecPosPlusOffset.z - vecOrigin->z + vecPosPlusOffset.z;
                    }
                }
            }
        }

        uint32_t result = CWorld__ProcessLineOfSight(vecOrigin, vecEnd, colPoint, ppEntity, b1, b2, b3, b4, b5, b6, b7, b8);

        if (g_iLagCompensationMode == 2)
        {
            if (g_pCurrentFiredPed == pGame->FindPlayerPed()) {
                SendBulletSync(vecOrigin, vecEnd, colPoint, ppEntity);
            }
            return result;
        }

        if (g_pCurrentFiredPed)
        {
            if (g_pCurrentFiredPed != pGame->FindPlayerPed())
            {
                if (g_pCurrentBulletData)
                {
                    if (g_pCurrentBulletData->pEntity == nullptr)
                    {
                        CPedGTA* pLocalPed = GamePool_FindPlayerPed();
                        if (*ppEntity == GamePool_FindPlayerPed() ||
                                pLocalPed->IsInVehicle() && *ppEntity == pLocalPed->pVehicle)
                        {
                            result = 0;
                            *ppEntity = nullptr;
                            colPoint->m_vecPoint.x = 0.0f;
                            colPoint->m_vecPoint.y = 0.0f;
                            colPoint->m_vecPoint.z = 0.0f;
                            return result;
                        }
                    }
                }
            }
            else {
                SendBulletSync(vecOrigin, vecEnd, colPoint, ppEntity);
            }
        }

        return result;
    }

    return CWorld__ProcessLineOfSight(vecOrigin, vecEnd, colPoint, ppEntity, b1, b2, b3, b4, b5, b6, b7, b8);
}
// 0.3.7
uint32_t(*CWeapon__FireSniper)(CWeapon* thiz, CPedGTA* pFiringEntity, CEntityGTA* victim, CVector* target);
uint32_t CWeapon__FireSniper_hook(CWeapon* thiz, CPedGTA* pFiringEntity, CEntityGTA* victim, CVector* target)
{
    if (pFiringEntity == GamePool_FindPlayerPed())
    {
        if (pGame)
        {
            CPlayerPed* pPlayerPed = pGame->FindPlayerPed();
            if (pPlayerPed) {
                pPlayerPed->FireInstant();
            }
        }
    }

    return true;
}
// 0.3.7
bool(*CBulletInfo_AddBullet)(CEntityGTA* creator, int weaponType, CVector pos, CVector velocity);
bool CBulletInfo_AddBullet_hook(CEntityGTA* creator, int weaponType, CVector pos, CVector velocity)
{
    velocity.x *= 50.0f;
    velocity.y *= 50.0f;
    velocity.z *= 50.0f;

    CBulletInfo_AddBullet(creator, weaponType, pos, velocity);

    // CBulletInfo::Update
    CHook::CallFunction<void>("_ZN11CBulletInfo6UpdateEv");
    return true;
}

#pragma pack(push, 1)
struct CPedDamageResponseCalculator
{
    CPedGTA* m_pDamager;
    float m_fDamageFactor;
    int m_pedPieceType;
    int m_weaponType;
};
#pragma pack(pop)
// 0.3.7
bool ComputeDamageResponse(CPedDamageResponseCalculator* calculator, CPedGTA* pPed)
{
    CPedGTA* pGamePed = GamePool_FindPlayerPed();
    bool isLocalPed = false;

    if (!pNetGame) return false;

    CPedGTA* pDamager = calculator->m_pDamager;
    if (pDamager != pGamePed && IsValidGamePed(pDamager)) /* CCivilianPed */
        return true;

    if (pPed == pGamePed) {
        isLocalPed = true;
    }
    else if (pDamager != pGamePed) {
        return false;
    }

    CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
    CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
    PLAYERID PlayerID;

    if (isLocalPed)
    {
        PlayerID = FindPlayerIDFromGtaPtr(pDamager);
        pLocalPlayer->SendTakeDamageEvent(PlayerID,
                calculator->m_fDamageFactor,
                calculator->m_weaponType,
                calculator->m_pedPieceType);
    }
    else
    {
        PlayerID = FindPlayerIDFromGtaPtr(pPed);
        if (PlayerID != INVALID_PLAYER_ID)
        {
            pLocalPlayer->SendGiveDamageEvent(PlayerID,
                    calculator->m_fDamageFactor,
                    calculator->m_weaponType,
                    calculator->m_pedPieceType);
            if (pPlayerPool->GetAt(PlayerID)->IsNPC())
                return true;
        }
        else
        {
            PLAYERID ActorID = FindActorIDFromGtaPtr(pPed);
            if (ActorID != INVALID_PLAYER_ID) {
                pLocalPlayer->SendGiveDamageEvent(ActorID,
                        calculator->m_fDamageFactor,
                        calculator->m_weaponType,
                        calculator->m_pedPieceType);
                return true;
            }
        }
    }


    // :check_friendly_fire
    if (!pNetGame->m_pNetSet->bFriendlyFire)
        return false;
    uint8_t byteTeam = pPlayerPool->GetLocalPlayer()->m_byteTeam;
    if (byteTeam == NO_TEAM ||
            PlayerID == INVALID_PLAYER_ID ||
            pPlayerPool->GetAt(PlayerID)->m_byteTeam != byteTeam) {
        return false;
    }

    return true;
}

// 0.3.7
void (*CPedDamageResponseCalculator__ComputeDamageResponse)(CPedDamageResponseCalculator* thiz, CPedGTA* pPed, uintptr_t* a3, uint32_t a4);
void CPedDamageResponseCalculator__ComputeDamageResponse_hook(CPedDamageResponseCalculator* thiz, CPedGTA* pPed, uintptr_t *a3, uint32_t a4)
{
    if (thiz == nullptr || pPed == nullptr || a3 == nullptr) return;

    if (ComputeDamageResponse(thiz, pPed))
        return;

    CPedDamageResponseCalculator__ComputeDamageResponse(thiz, pPed, a3, a4);
}

void (*CRenderer_RenderEverythingBarRoads)();
static void LogWatchedWorldMatricesAroundPlayerABI(const CVector& pos);
#if !VER_x32
static constexpr bool kArm64ManualWorldRenderEnabled = false;
static void RenderNearbyStaticWorldEntities64(bool roadsOnly);
static void CullNearbyLodWorldEntities64();
static void LogRendererLists64(const char* stage);
static void SanitizeRendererLists64(const char* stage);
static void RepairWorldRwObjectsAroundPlayer64(const CVector& pos, uint8_t playerArea, bool renderPass);
#endif
void (*CRenderer_PreRender)();
void CRenderer_PreRender_hook()
{
    // Forward directly to the stock renderer.  The previous ARM64 wrapper
    // compacted renderer lists and rewrote vehicle/world RW state around the
    // player during every render pass, which could produce missing geometry
    // and a second inconsistent world after a camera gesture.
    if (CRenderer_PreRender) {
        CRenderer_PreRender();
    }
}

void CRenderer_RenderEverythingBarRoads_hook()
{
    // Do not add a second world-render pass after the native renderer.
    if (CRenderer_RenderEverythingBarRoads) {
        CRenderer_RenderEverythingBarRoads();
    }
}

void (*CRenderer_RenderRoads)();
void CRenderer_RenderRoads_hook()
{
#if !VER_x32
    SuppressNativeLoadingDuringGameplay64("roads-begin");
    RestoreWorld3DRenderState64("roads-before-native");
    if (kArm64ManualWorldRenderEnabled) {
        CullNearbyLodWorldEntities64();
    }
#endif

    CRenderer_RenderRoads();
#if !VER_x32
    RestoreWorld3DRenderState64("roads-after-native");
    if (kArm64ManualWorldRenderEnabled) {
        RenderNearbyStaticWorldEntities64(true);
    }
    LogRendererLists64("roads");
#endif
    if (pGame && pGame->FindPlayerPed() && pGame->FindPlayerPed()->m_pPed) {
        LogWatchedWorldMatricesAroundPlayerABI(pGame->FindPlayerPed()->m_pPed->GetPosition());
    }
}

#include "CFPSFix.h"
#include "RQ_Commands.h"
#include "Pickups.h"
#include "TimeCycle.h"
#include "game/Pipelines/CustomCar/CustomCarEnvMapPipeline.h"
#include "game/Pipelines/CustomBuilding/CustomBuildingDNPipeline.h"
#include "COcclusion.h"
#include "game/Widgets/WidgetGta.h"

CFPSFix g_fps;

void (*ANDRunThread)(void* a1);
void ANDRunThread_hook(void* a1)
{
    g_fps.PushThread(gettid());

    ANDRunThread(a1);
}

static constexpr float ar43 = 4.0f/3.0f;
float *ms_fAspectRatio;
void (*DrawCrosshair)(uintptr_t* thiz);
void DrawCrosshair_hook(uintptr_t* thiz)
{
    if (SampUiController::ShouldSuppressNativeCrosshair()) {
        return;
    }

    float save1 = CCamera::m_f3rdPersonCHairMultX;
    CCamera::m_f3rdPersonCHairMultX = 0.530f - (*ms_fAspectRatio - ar43) * 0.01125f;

    float save2 = CCamera::m_f3rdPersonCHairMultY;
    CCamera::m_f3rdPersonCHairMultY = 0.400f + (*ms_fAspectRatio - ar43) * 0.03600f;

    DrawCrosshair(thiz);

    CCamera::m_f3rdPersonCHairMultX = save1;
    CCamera::m_f3rdPersonCHairMultY = save2;
}

CVector& (*FindPlayerSpeed)(int a1);
CVector& FindPlayerSpeed_hook(int a1)
{
    uintptr_t dwRetAddr = 0;
    __asm__ volatile ("mov %0, lr":"=r" (dwRetAddr));
    dwRetAddr -= g_libGTASA;

    if(dwRetAddr == 0x43E1F6 + 1)
    {
        if(pNetGame)
        {
            CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
            if(pPlayerPed &&
                    pPlayerPed->IsInVehicle() &&
                    pPlayerPed->IsAPassenger())
            {
                CVector vec = CVector(-1.0f);
                return vec;
            }
        }
    }

    return FindPlayerSpeed(a1);
}

int (*RwFrameAddChild)(int a1, int a2);
int RwFrameAddChild_hook(int a1, int a2)
{
    if(a1 == 0 || a2 == 0) return 0;
    return RwFrameAddChild(a1, a2);
}

int iLastTouchedWidgetId = -1;

int iLastReleasedWidgetId = -1;

int (*CTouchInterface__IsReleased)(int iWidgetId, int iUnk, int iEnableWidget);
int CTouchInterface__IsReleased_hook(int iWidgetId, int iUnk, int iEnableWidget)
{
    uintptr_t dwRetAddr = 0;
    __asm__ volatile ("mov %0, lr" : "=r" (dwRetAddr));
    dwRetAddr -= g_libGTASA;

    int iReleased = CTouchInterface__IsReleased(iWidgetId, iUnk, iEnableWidget);
    if(iReleased && iEnableWidget)
    {
        iLastReleasedWidgetId = iWidgetId;
    }

    return iReleased;
}

int (*CTextureDatabaseRuntime__GetEntry)(uintptr_t thiz, const char* a2, bool* a3);
int CTextureDatabaseRuntime__GetEntry_hook(uintptr_t thiz, const char* a2, bool* a3)
{
    if (!thiz)
    {
        return -1;
    }

    int index = -1;
    if (CTextureDatabaseRuntime__GetEntry) {
        index = CTextureDatabaseRuntime__GetEntry(thiz, a2, a3);
    }

#if !VER_x32
    QueueTextureWarmup64(reinterpret_cast<TextureDatabaseRuntime*>(thiz), index);
#endif
    return index;
}

uintptr_t (*CTxdStore__TxdStoreFindCB)(const char *a1);
uintptr_t CTxdStore__TxdStoreFindCB_hook(const char *a1)
{
#if !VER_x32
    if (!a1) {
        return 0;
    }

    if (CTxdStore__TxdStoreFindCB) {
        uintptr_t originalTexture = CTxdStore__TxdStoreFindCB(a1);
        if (originalTexture) {
            return originalTexture;
        }
    }

    static const char* texdbs[] = { "samp", "gta_int", "gta3", "txd", "mobile", "player", "menu" };
    for (const char* texdb : texdbs) {
        TextureDatabaseRuntime* db = TextureDatabaseRuntime::GetDatabase(texdb);
        if (!db) {
            continue;
        }

        TextureDatabaseRuntime::Register(db);
        RwTexture* texture = TextureDatabaseRuntime::GetTexture(a1);
        TextureDatabaseRuntime::Unregister(db);
        if (texture) {
#if !VER_x32
            if (CTextureDatabaseRuntime__GetEntry) {
                bool entryFound = false;
                const int index = CTextureDatabaseRuntime__GetEntry(
                    reinterpret_cast<uintptr_t>(db),
                    a1,
                    &entryFound);
                QueueTextureWarmup64(db, index);
            }
#endif
            return reinterpret_cast<uintptr_t>(texture);
        }
    }

    return 0;
#else
    static const char* texdbs[] = { "samp", "gta_int", "gta3" };
    return GTASAEngineApi::FindTextureInNativeTexDictionaries(a1, texdbs, sizeof(texdbs) / sizeof(texdbs[0]));
#endif
}

#if !VER_x32
static bool gTextureLookup64HookInstalled = false;

static void InstallTextureLookup64Hooks()
{
    if (gTextureLookup64HookInstalled) {
        return;
    }

    CHook::InlineHook("_ZN9CTxdStore14TxdStoreFindCBEPKc",
                      &CTxdStore__TxdStoreFindCB_hook,
                      &CTxdStore__TxdStoreFindCB);
    gTextureLookup64HookInstalled = true;
    FLog("[TEXLOOKUP64] CTxdStore::TxdStoreFindCB hook installed");
}
#endif

int (*CCustomRoadsignMgr_RenderRoadsignAtomic)(int a1, int a2);
int CCustomRoadsignMgr_RenderRoadsignAtomic_hook(int a1, int a2)
{
    if (a1 && CCustomRoadsignMgr_RenderRoadsignAtomic) {
        const int result = CCustomRoadsignMgr_RenderRoadsignAtomic(a1, a2);
#if !VER_x32
        RestoreWorld3DRenderState64("roadsign-after-native");
#endif
        return result;
    }

    return 0;
}

int (*_RwTextureDestroy)(int a1);
int _RwTextureDestroy_hook(int a1)
{
    int result; // r0

    if ( (unsigned int)(a1 + 1) >= 2 )
        result = _RwTextureDestroy(a1);
    else
        result = 0;
    return result;
}

int (*CPed_UpdatePosition)(CPedGTA* a1);
int CPed_UpdatePosition_hook(CPedGTA* a1)
{
    int result; // r0

    if ( GamePool_FindPlayerPed() == a1 )
        result = CPed_UpdatePosition(a1);
    return result;
}

void (*CCamera__Process)(uintptr_t thiz);
void CCamera__Process_hook(uintptr_t thiz)
{
    //if(pGame->GetCamera())
    //pGame->GetCamera()->Update();

    CCamera__Process(thiz);
}

extern CJavaWrapper* pJavaWrapper;
void (*MainMenuScreen__OnExit)();
void MainMenuScreen__OnExit_hook()
{
    pGame->bIsGameExiting = true;

    pNetGame->GetRakClient()->Disconnect(0);

    pJavaWrapper->exitGame();
}

void (*rqVertexBufferSelect)(unsigned int **result);
void rqVertexBufferSelect_hook(unsigned int **result)
{
    uint32_t buffer = *(uint32_t *)*result;
    *result += 4;
    if ( buffer )
    {
        glBindBuffer(34962, *(uint32_t *)(buffer + 8));
        GTASAEngineApi::ResetRqVertexBufferState();
    }
    else
    {
        glBindBuffer(34962, 0);
    }
}

uintptr_t* (*rpMaterialListDeinitialize)(RpMaterialList* matList);
uintptr_t* rpMaterialListDeinitialize_hook(RpMaterialList* matList)
{
    if(!matList || !matList->materials)
        return nullptr;

    return rpMaterialListDeinitialize(matList);
}

void (*rqVertexBufferDelete)(unsigned int **result);
void rqVertexBufferDelete_hook(unsigned int **result)
{
    uint32_t* buffer = *(uint32_t **)*result;
    *result += 4;
    glDeleteBuffers(1, reinterpret_cast<const GLuint *>(buffer + 2));
    buffer[2] = 0;
    if ( buffer )
        (*(void (**)(uint32_t *))(*buffer + 4))(buffer);
}

#if !VER_x32
static bool IsLikelyArm64UserPointer(uintptr_t value)
{
    return value >= 0x0000006000000000ULL && value < 0x0000008000000000ULL;
}

void RQCommand_VertexBufferDeleteGuard64(char*& qData)
{
    if (!qData) {
        return;
    }

    const uintptr_t bufferPtr = *reinterpret_cast<uintptr_t*>(qData);
    qData += sizeof(uintptr_t);
    if (!IsLikelyArm64UserPointer(bufferPtr)) {
        static uint32_t s_lastInvalidDeleteLogTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastInvalidDeleteLogTick >= 1000) {
            s_lastInvalidDeleteLogTick = now;
            FLog("[RQ_GUARD64] skip vertex-buffer delete invalid ptr=0x%llX",
                 static_cast<unsigned long long>(bufferPtr));
        }
        return;
    }

    auto* bufferBytes = reinterpret_cast<uint8_t*>(bufferPtr);
    GLuint glBuffer = *reinterpret_cast<GLuint*>(bufferBytes + 8);
    if (glBuffer != 0 && glBuffer < 0x01000000u) {
        glDeleteBuffers(1, &glBuffer);
    }
    *reinterpret_cast<GLuint*>(bufferBytes + 8) = 0;
}
#endif

void rotate_ped_if_local(unsigned int *a1, unsigned int *a2)
{
    if ( GamePool_FindPlayerPed() == (CPedGTA*)a2 )
        *(uint32_t *)(a2 + 0x560) = *a1;
}

void (*player_control_zelda)(unsigned int *a2, unsigned int *a3);
void player_control_zelda_hook(unsigned int *a2, unsigned int *a3)
{
    rotate_ped_if_local(a2, a3);
}

// 006778B0
int (*rxOpenGLDefaultAllInOneRenderCB)(RwResEntry* resEntry, uintptr_t object, uint8_t type, uint32_t flags);
int rxOpenGLDefaultAllInOneRenderCB_hook(RwResEntry* resEntry, uintptr_t object, uint8_t type, uint32_t flags)
{
    if(!resEntry || !flags || !rxOpenGLDefaultAllInOneRenderCB)
        return 0;

#if !VER_x32
    ClearWorldTextureRasterState64("all-in-one-before");
#endif
    const int result = rxOpenGLDefaultAllInOneRenderCB(resEntry, object, type, flags);
#if !VER_x32
    ClearWorldTextureRasterState64("all-in-one-after");
#endif
    return result;
}

// 00677CB4
int (*CCustomBuildingDNPipeline__CustomPipeRenderCB)(RwResEntry* resEntry, uintptr_t object, uint8_t type, uint32_t flags);
int CCustomBuildingDNPipeline__CustomPipeRenderCB_hook(RwResEntry* resEntry, uintptr_t object, uint8_t type, uint32_t flags)
{
    if(!resEntry || !flags || !CCustomBuildingDNPipeline__CustomPipeRenderCB)
        return 0;

#if !VER_x32
    ClearWorldTextureRasterState64("building-pipe-before");
#endif
    const int result = CCustomBuildingDNPipeline__CustomPipeRenderCB(resEntry, object, type, flags);
#if !VER_x32
    ClearWorldTextureRasterState64("building-pipe-after");
#endif
    return result;
}

int (*EmuShader_Select)(uintptr_t *result);
int EmuShader_Select_hook(uintptr_t *result)
{
    int result1;
    if ( *result >= 0x1000 )
        return EmuShader_Select(result);
    return 0;
}

float float_4DD9E8;
float ms_fTimeStep;
float fMagic = 50.0f / 30.0f;
void (*CTaskSimpleUseGun__SetMoveAnim)(uintptr_t *thiz, uintptr_t *a2);
void CTaskSimpleUseGun__SetMoveAnim_hook(uintptr_t *thiz, uintptr_t *a2)
{
    ms_fTimeStep = GTASAEngineApi::NativeTimeStep();
    float_4DD9E8 = GTASAEngineApi::NativeRendererTimeStep();
    float_4DD9E8 = (fMagic) * (0.1f / ms_fTimeStep);
    CTaskSimpleUseGun__SetMoveAnim(thiz, a2);
}

int (*CAnimManager_UncompressAnimation)(int result);
int CAnimManager_UncompressAnimation_hook(int result)
{
    if ( result )
        return CAnimManager_UncompressAnimation(result);
    return 0;
}

void readVehiclesAudioSettings();

void (*CVehicleModelInfo__SetupCommonData)();

void CVehicleModelInfo__SetupCommonData_hook() {
    CVehicleModelInfo__SetupCommonData();
    readVehiclesAudioSettings();
}

extern VehicleAudioPropertiesStruct VehicleAudioProperties[20000];
static uintptr_t addr_veh_audio = (uintptr_t) &VehicleAudioProperties[0];
static constexpr int kVehicleAudioBaseModelId = 400;
static constexpr int kVehicleAudioSlotCount = 20000;

void (*CAEVehicleAudioEntity__GetVehicleAudioSettings)(uintptr_t thiz, int16_t a2, int a3);

void CAEVehicleAudioEntity__GetVehicleAudioSettings_hook(uintptr_t dest, int16_t a2, int ID) {
    (void)a2;

    if (!dest) {
        return;
    }

    const int audioSlot = ID - kVehicleAudioBaseModelId;
    if (audioSlot < 0 || audioSlot >= kVehicleAudioSlotCount) {
        static int s_invalidVehicleAudioRequests = 0;
        if (s_invalidVehicleAudioRequests < 8) {
            ++s_invalidVehicleAudioRequests;
            FLog("[VEH_AUDIO64] invalid-request model=%d", ID);
        }
        memset(reinterpret_cast<void*>(dest), 0, sizeof(VehicleAudioPropertiesStruct));
        return;
    }

    memcpy(reinterpret_cast<void*>(dest), &VehicleAudioProperties[audioSlot], sizeof(VehicleAudioPropertiesStruct));
}

void (*CRadar_ClearBlip)(uint32_t a2);
void CRadar_ClearBlip_hook(uint32_t a2)
{
    uintptr_t dwRetAddr = 0;
    GET_LR(dwRetAddr);

    //LOGI("[CRadar::ClearBlip]: %d called from 0x%X", (uint16_t)a2, dwRetAddr);

    if ( (uint16_t)a2 > 249 )
    {
        LOGI("[CRadar::ClearBlip]: Invalid blip ID (%d) called from 0x%X", (uint16_t)a2, dwRetAddr);
    }
    else
    {
        CRadar_ClearBlip(a2);
    }
}

/* =============================================================================== */

void InstallHuaweiCrashFixHooks()
{
    using NativePltSlot = GTASAEngineApi::NativePltSlot;
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::RqVertexBufferSelect, (uintptr_t)rqVertexBufferSelect_hook, (uintptr_t*)&rqVertexBufferSelect);
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::RqVertexBufferDelete, (uintptr_t)rqVertexBufferDelete_hook, (uintptr_t*)&rqVertexBufferDelete);
}

void InstallCrashFixHooks()
{
    using NativePltSlot = GTASAEngineApi::NativePltSlot;
    // some crashfixes
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::CustomRoadsignRenderAtomic, (uintptr_t)CCustomRoadsignMgr_RenderRoadsignAtomic_hook, (uintptr_t*)&CCustomRoadsignMgr_RenderRoadsignAtomic);
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::RwTextureDestroy, (uintptr_t)_RwTextureDestroy_hook, (uintptr_t*)&_RwTextureDestroy);
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::PedUpdatePosition, (uintptr_t)CPed_UpdatePosition_hook, (uintptr_t*)&CPed_UpdatePosition);
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::RwFrameAddChild, (uintptr_t)RwFrameAddChild_hook, (uintptr_t*)&RwFrameAddChild);
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::TextureDatabaseRuntimeGetEntry, (uintptr_t)CTextureDatabaseRuntime__GetEntry_hook, (uintptr_t*)&CTextureDatabaseRuntime__GetEntry);
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::RpMaterialListDeinitialize, (uintptr_t)rpMaterialListDeinitialize_hook, (uintptr_t*)&rpMaterialListDeinitialize);
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::AnimManagerUncompressAnimation, (uintptr_t)CAnimManager_UncompressAnimation_hook, (uintptr_t*)&CAnimManager_UncompressAnimation);
}

void InstallWeaponFireHooks()
{
}

void InstallSAMPHooks()
{
    using NativePltSlot = GTASAEngineApi::NativePltSlot;
    // samp main loop
    // imgui
    // splashscreen
    // gangzones
    // radar
    // removebuilding
    // obj material
    // textdraw models
    // enter vehicle as driver
    // radar color
    // exit vehicle
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::TaskComplexLeaveCarPrimary, (uintptr_t)CTaskComplexLeaveCar_hook, (uintptr_t*)&CTaskComplexLeaveCar);
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::TaskComplexLeaveCarSecondary, (uintptr_t)CTaskComplexLeaveCar_hook, (uintptr_t*)&CTaskComplexLeaveCar);
    // attach obj to ped
    // game pause
    // aim
    // Crosshair Fix

    // fix radar in passenger
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::FindPlayerSpeed, (uintptr_t)FindPlayerSpeed_hook, (uintptr_t*)&FindPlayerSpeed);

#if !VER_x32
    InstallRadar64Hooks();
#endif

    // fix texture loading
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::TxdStoreFindCb, (uintptr_t)CTxdStore__TxdStoreFindCB_hook, (uintptr_t*)&CTxdStore__TxdStoreFindCB);

    // interpolate camera fix
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::CameraProcess, (uintptr_t)CCamera__Process_hook, (uintptr_t*)&CCamera__Process);

    // for surfing

    // hueta ne rabotaet no pust budet (tipo ne kak v 1.08)
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::VehicleModelSetupCommonData, (uintptr_t)CVehicleModelInfo__SetupCommonData_hook, (uintptr_t*)&CVehicleModelInfo__SetupCommonData);
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::VehicleAudioSettings, (uintptr_t)CAEVehicleAudioEntity__GetVehicleAudioSettings_hook, (uintptr_t*)&CAEVehicleAudioEntity__GetVehicleAudioSettings);

    GTASAEngineApi::InstallNativePltHook(NativePltSlot::RadarClearBlip, (uintptr_t)CRadar_ClearBlip_hook, (uintptr_t*)&CRadar_ClearBlip);

    // skills
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::PedGetWeaponSkill, (uintptr_t)CPed__GetWeaponSkill_hook, (uintptr_t*)&CPed__GetWeaponSkill);

    //InstallHuaweiCrashFixHooks();
    InstallCrashFixHooks();
    InstallWeaponFireHooks();
    HookCPad();
}

void ReadSettingFile();
void ApplyFPSPatch(uint8_t fps);
void (*NvUtilInit)();
void NvUtilInit_hook()
{
    FLog("NvUtilInit");

    NvUtilInit();

#if !VER_x32
    const char* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    FLog("[GLCAP64] vendor=%s renderer=%s version=%s dxt=%d s3tc=%d atc=%d pvrtc=%d etc1=%d etc2=%d astc=%d",
         glVendor ? glVendor : "(null)",
         glRenderer ? glRenderer : "(null)",
         glVersion ? glVersion : "(null)",
         glExtensions && strstr(glExtensions, "GL_EXT_texture_compression_dxt1"),
         glExtensions && strstr(glExtensions, "GL_EXT_texture_compression_s3tc"),
         glExtensions && strstr(glExtensions, "GL_AMD_compressed_ATC_texture"),
         glExtensions && strstr(glExtensions, "GL_IMG_texture_compression_pvrtc"),
         glExtensions && strstr(glExtensions, "GL_OES_compressed_ETC1_RGB8_texture"),
         glExtensions && strstr(glExtensions, "GL_OES_compressed_ETC2_RGB8_texture"),
         glExtensions && strstr(glExtensions, "GL_KHR_texture_compression_astc_ldr"));
#endif

    g_pszStorage = GTASAEngineApi::StorageRootBuffer();

    ReadSettingFile();

    ApplyFPSPatch(120);
}

struct stFile
{
    int isFileExist;
    FILE *f;
};

char lastFile[123];

static bool MatchesAtOffset(const char* value, size_t offset, const char* token)
{
    if (!value || !token) {
        return false;
    }

    const size_t valueLen = strlen(value);
    const size_t tokenLen = strlen(token);
    if (valueLen < offset + tokenLen) {
        return false;
    }

    return !strncmp(value + offset, token, tokenLen);
}

static void NormalizeGamePath(char* out, size_t outSize, const char* in)
{
    if (!out || outSize == 0) {
        return;
    }

    size_t i = 0;
    for (; in && in[i] && i + 1 < outSize; i++) {
        char ch = in[i] == '\\' ? '/' : in[i];
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 'a' + 'A');
        }
        out[i] = ch;
    }
    out[i] = '\0';
}

static void NormalizeFilesystemPath(char* out, size_t outSize, const char* in, bool toLower)
{
    if (!out || outSize == 0) {
        return;
    }

    size_t i = 0;
    for (; in && in[i] && i + 1 < outSize; i++) {
        char ch = in[i] == '\\' ? '/' : in[i];
        if (toLower && ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
        out[i] = ch;
    }
    out[i] = '\0';
}

static void BuildStoragePath(char* out, size_t outSize, const char* relativePath, bool lowercasePath)
{
    if (!out || outSize == 0) {
        return;
    }

    out[0] = '\0';
    if (!relativePath || !g_pszStorage) {
        return;
    }

    char normalized[255]{};
    NormalizeFilesystemPath(normalized, sizeof(normalized), relativePath, lowercasePath);

    const char* rel = normalized;
    while (*rel == '/') {
        ++rel;
    }

    snprintf(out, outSize, "%s%s", g_pszStorage, rel);
}

stFile* NvFOpen(const char* r0, const char* r1, int r2, int r3)
{
    (void)r0;
    (void)r2;
    (void)r3;

    if (!r1) {
        FLog("NVFOpen hook | Error: null path");
        return nullptr;
    }

    snprintf(lastFile, sizeof(lastFile), "%s", r1);

    char path[512]{};
    memset(path, 0, sizeof(path));

    snprintf(path, sizeof(path), "%s%s", g_pszStorage ? g_pszStorage : "", r1);
    char normalizedPath[256]{};
    NormalizeGamePath(normalizedPath, sizeof(normalizedPath), r1);

    // ----------------------------
    if (MatchesAtOffset(r1, 12, "mainV1.scm"))
    {
        snprintf(path, sizeof(path), "%sSAMP/main.scm", g_pszStorage ? g_pszStorage : "");
        FLog("Loading %s", path);
    }
    // ----------------------------
    if (MatchesAtOffset(r1, 12, "SCRIPTV1.IMG"))
    {
        snprintf(path, sizeof(path), "%sSAMP/script.img", g_pszStorage ? g_pszStorage : "");
        FLog("Loading script.img..");
    }
    // ----------------------------
    if(!strncmp(r1, "DATA/PEDS.IDE", 13))
    {
        snprintf(path, sizeof(path), "%sSAMP/peds.ide", g_pszStorage ? g_pszStorage : "");
        FLog("Loading peds.ide..");
    }
    // ----------------------------
    if(!strncmp(r1, "DATA/VEHICLES.IDE", 17))
    {
        snprintf(path, sizeof(path), "%sSAMP/vehicles.ide", g_pszStorage ? g_pszStorage : "");
        FLog("Loading vehicles.ide..");
    }

    if (!strncmp(r1, "DATA/GTA.DAT", 12))
    {
        snprintf(path, sizeof(path), "%sSAMP/gta.dat", g_pszStorage ? g_pszStorage : "");
        FLog("Loading gta.dat..");
    }

    if (!strncmp(r1, "DATA/HANDLING.CFG", 17))
    {
        snprintf(path, sizeof(path), "%sSAMP/handling.cfg", g_pszStorage ? g_pszStorage : "");
        FLog("Loading handling.cfg..");
    }

    if (!strncmp(r1, "DATA/WEAPON.DAT", 15))
    {
        snprintf(path, sizeof(path), "%sSAMP/weapon.dat", g_pszStorage ? g_pszStorage : "");
        FLog("Loading weapon.dat..");
    }

    if (!strncmp(r1, "DATA/FONTS.DAT", 15))
    {
        snprintf(path, sizeof(path), "%sdata/fonts.dat", g_pszStorage ? g_pszStorage : "");
        FLog("Loading weapon.dat..");
    }

    if (!strncmp(r1, "DATA/PEDSTATS.DAT", 15))
    {
        snprintf(path, sizeof(path), "%sdata/pedstats.dat", g_pszStorage ? g_pszStorage : "");
        FLog("Loading weapon.dat..");
    }

    if (!strncmp(r1, "DATA/TIMECYC.DAT", 15))
    {
        snprintf(path, sizeof(path), "%sdata/timecyc.dat", g_pszStorage ? g_pszStorage : "");
        FLog("Loading weapon.dat..");
    }

    if (!strncmp(r1, "DATA/POPCYCLE.DAT", 15))
    {
        snprintf(path, sizeof(path), "%sdata/popcycle.dat", g_pszStorage ? g_pszStorage : "");
        FLog("Loading weapon.dat..");
    }

    if (!strcmp(normalizedPath, "SAMP/SAMP.IDE"))
    {
        snprintf(path, sizeof(path), "%sSAMP/SAMP.ide", g_pszStorage ? g_pszStorage : "");
        FLog("Loading SAMP.ide..");
    }

    if (!strcmp(normalizedPath, "SAMP/VEHICLES.IDE"))
    {
        snprintf(path, sizeof(path), "%sSAMP/vehicles.ide", g_pszStorage ? g_pszStorage : "");
        FLog("Loading SAMP vehicles.ide..");
    }

    if (!strcmp(normalizedPath, "SAMP/PEDS.IDE"))
    {
        snprintf(path, sizeof(path), "%sSAMP/peds.ide", g_pszStorage ? g_pszStorage : "");
        FLog("Loading SAMP peds.ide..");
    }

#if VER_x32
    auto *st = (stFile*)malloc(8);
#else
    auto *st = (stFile*)malloc(0x10);
#endif
    st->isFileExist = false;

    char candidates[12][512]{};
    int candidateCount = 0;

    auto appendCandidate = [&](const char* candidatePath) {
        if (!candidatePath || !candidatePath[0] || candidateCount >= static_cast<int>(ARRAY_SIZE(candidates))) {
            return;
        }

        for (int i = 0; i < candidateCount; ++i) {
            if (!strcmp(candidates[i], candidatePath)) {
                return;
            }
        }

        snprintf(candidates[candidateCount], sizeof(candidates[candidateCount]), "%s", candidatePath);
        candidateCount++;
    };

    appendCandidate(path);

    char slashNormalizedPath[512]{};
    NormalizeFilesystemPath(slashNormalizedPath, sizeof(slashNormalizedPath), path, false);
    appendCandidate(slashNormalizedPath);

    char lowerStoragePath[512]{};
    BuildStoragePath(lowerStoragePath, sizeof(lowerStoragePath), r1, true);
    appendCandidate(lowerStoragePath);

    if (r1 && r1[0] == '/') {
        char absoluteNormalized[512]{};
        NormalizeFilesystemPath(absoluteNormalized, sizeof(absoluteNormalized), r1, false);
        if (!strncmp(absoluteNormalized, "/storage/", 9)) {
            appendCandidate(absoluteNormalized);
        } else {
            char externalStoragePath[512]{};
            snprintf(externalStoragePath, sizeof(externalStoragePath), "/storage/emulated/0%s", absoluteNormalized);
            appendCandidate(externalStoragePath);
        }

        char absoluteLower[512]{};
        NormalizeFilesystemPath(absoluteLower, sizeof(absoluteLower), r1, true);
        if (!strncmp(absoluteLower, "/storage/", 9)) {
            appendCandidate(absoluteLower);
        } else {
            char externalStorageLowerPath[512]{};
            snprintf(externalStorageLowerPath, sizeof(externalStorageLowerPath), "/storage/emulated/0%s", absoluteLower);
            appendCandidate(externalStorageLowerPath);
        }
    }

    FILE *f = nullptr;
    const char* openedPath = nullptr;
    for (int i = 0; i < candidateCount; ++i) {
        f = fopen(candidates[i], "rb");
        if (f) {
            openedPath = candidates[i];
            break;
        }
    }

    if(f)
    {
        if (openedPath && strcmp(openedPath, path)) {
            FLog("[FILE_FIX64] NvFOpen fallback: req=%s base=%s opened=%s",
                 r1 ? r1 : "<null>",
                 path,
                 openedPath);
        }

        st->isFileExist = true;
        st->f = f;
        return st;
    }
    else
    {
        FLog("NVFOpen hook | Error: file not found (%s)", path);
        free(st);
        return nullptr;
    }
}

bool g_bPlaySAMP = false;

void MainMenu_OnStartSAMP()
{
    if(g_bPlaySAMP) return;

    //InitInMenu();
    pGame->StartGame();

    // StartGameScreen::OnNewGameCheck()
    GTASAEngineApi::RunNewGameCheck();

    g_bPlaySAMP = true;
}

unsigned int (*MainMenuScreen__Update)(uintptr_t thiz, float a2);
unsigned int MainMenuScreen__Update_hook(uintptr_t thiz, float a2)
{
    unsigned int ret = MainMenuScreen__Update(thiz, a2);
    MainMenu_OnStartSAMP();
    return ret;
}

void (*StartGameScreen__OnNewGameCheck)();
void StartGameScreen__OnNewGameCheck_hook()
{
    // отключить кнопку начать игру
    if(g_bPlaySAMP)
        return;

    StartGameScreen__OnNewGameCheck();
}

void (*CTaskSimpleUseGun__RemoveStanceAnims)(uintptr* thiz, void* ped, float a3);
void CTaskSimpleUseGun__RemoveStanceAnims_hook(uintptr* thiz, void* ped, float a3)
{
    if(!thiz)
        return;

    uintptr* m_pAnim = (uintptr*)(thiz + 0x2c);
    if(m_pAnim) {
        if (!((uintptr *)(m_pAnim + 0x14)))
            return;
    }
    CTaskSimpleUseGun__RemoveStanceAnims(thiz, ped, a3);
}

static bool IsSafeVerticalLineColModel64(const CColModel* colModel)
{
    if (!colModel || !colModel->m_bHasCollisionVolumes || !colModel->m_pColData) {
        return false;
    }

    const CCollisionData* data = colModel->m_pColData;
    const uint32_t volumeCount =
        static_cast<uint32_t>(data->m_nNumSpheres) +
        static_cast<uint32_t>(data->m_nNumBoxes) +
        static_cast<uint32_t>(data->m_nNumLines) +
        static_cast<uint32_t>(data->m_nNumTriangles);
    if (volumeCount == 0) {
        return false;
    }

    if (data->m_nNumSpheres && !data->m_pSpheres) {
        return false;
    }
    if (data->m_nNumBoxes && !data->m_pBoxes) {
        return false;
    }
    if (data->m_nNumLines) {
        if (data->bUsesDisks) {
            if (!data->m_pDisks) {
                return false;
            }
        } else if (!data->m_pLines) {
            return false;
        }
    }
    if (data->m_nNumTriangles && (!data->m_pVertices || !data->m_pTriangles)) {
        return false;
    }

    return true;
}

bool (*CCollision__ProcessVerticalLine)(const CColLine* line,
                                        const CMatrix* transform,
                                        CColModel* colModel,
                                        CColPoint* colPoint,
                                        float* maxTouchDistance,
                                        bool doSeeThroughCheck,
                                        bool doShootThroughCheck,
                                        CStoredCollPoly* collPoly);
bool CCollision__ProcessVerticalLine_hook(const CColLine* line,
                                          const CMatrix* transform,
                                          CColModel* colModel,
                                          CColPoint* colPoint,
                                          float* maxTouchDistance,
                                          bool doSeeThroughCheck,
                                          bool doShootThroughCheck,
                                          CStoredCollPoly* collPoly)
{
    if (!line || !transform || !colPoint || !maxTouchDistance || !CCollision__ProcessVerticalLine ||
        !IsSafeVerticalLineColModel64(colModel)) {
        return false;
    }

    const float beforeMax = *maxTouchDistance;
    const float dxLine = line->m_vecStart.x - line->m_vecEnd.x;
    const float dyLine = line->m_vecStart.y - line->m_vecEnd.y;
    const float dzLine = line->m_vecStart.z - line->m_vecEnd.z;
    const float lineLength = fabsf(dzLine);
    const bool adaptVerticalMax =
        !VER_x32 &&
        fabsf(dxLine) <= 0.001f &&
        fabsf(dyLine) <= 0.001f &&
        lineLength > 1.001f &&
        beforeMax > 0.0f &&
        beforeMax <= 1.0001f;
    if (adaptVerticalMax) {
        *maxTouchDistance = beforeMax * lineLength;
    }
    bool hit = CCollision__ProcessVerticalLine(
            line,
            transform,
            colModel,
            colPoint,
            maxTouchDistance,
            doSeeThroughCheck,
            doShootThroughCheck,
            collPoly
    );
    const float nativeAfterMax = maxTouchDistance ? *maxTouchDistance : 0.0f;
    if (adaptVerticalMax) {
        if (hit && colPoint && std::isfinite(colPoint->m_vecPoint.z)) {
            const float denom = line->m_vecEnd.z - line->m_vecStart.z;
            float fraction = fabsf(denom) > 0.001f
                ? (colPoint->m_vecPoint.z - line->m_vecStart.z) / denom
                : nativeAfterMax;
            if (fraction < 0.0f) {
                hit = false;
                fraction = beforeMax;
            }
            if (fraction > beforeMax) {
                hit = false;
                fraction = beforeMax;
            }
            *maxTouchDistance = fraction;
        } else {
            *maxTouchDistance = beforeMax;
        }
    }
    if (!VER_x32 &&
        line &&
        colModel->m_nColSlot == 157 &&
        colModel->m_pColData &&
        colModel->m_pColData->m_nNumTriangles == 506) {
        static uint32_t s_lastWorldCollisionCallLogTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastWorldCollisionCallLogTick >= 300) {
            s_lastWorldCollisionCallLogTick = now;
            FLog("[COL_CCALL64] slot=%u tris=%u hit=%d start=%.2f %.2f %.2f end=%.2f %.2f %.2f max=%.3f->%.3f z=%.2f see=%d shoot=%d",
                 static_cast<unsigned>(colModel->m_nColSlot),
                 static_cast<unsigned>(colModel->m_pColData->m_nNumTriangles),
                 hit ? 1 : 0,
                 line->m_vecStart.x,
                 line->m_vecStart.y,
                 line->m_vecStart.z,
                 line->m_vecEnd.x,
                 line->m_vecEnd.y,
                 line->m_vecEnd.z,
                 beforeMax,
                 maxTouchDistance ? *maxTouchDistance : 0.0f,
                 (hit && colPoint) ? colPoint->m_vecPoint.z : 0.0f,
                 doSeeThroughCheck ? 1 : 0,
                 doShootThroughCheck ? 1 : 0);
        }
    }
    return hit;
}

static bool IsTempVehicleComponentModel64(int modelId)
{
    return modelId >= 374 && modelId <= 383;
}

static bool IsDynamicBreakableWorldPropModel64(int modelId)
{
    switch (modelId) {
        case 1223: // lampost_coast
        case 1226: // lamppost3
        case 1262: // MTraffic4
        case 1263: // MTraffic3
        case 1283: // MTraffic1
        case 1284: // MTraffic2
        case 1290: // lamppost2
        case 1294: // mlamppost
        case 1295: // doublestreetlght1
        case 1297: // lamppost1
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
            return true;
        default:
            return false;
    }
}

static bool IsUnstableBreakableWorldPropModel64(int modelId)
{
    if (IsDynamicBreakableWorldPropModel64(modelId)) {
        return true;
    }

    switch (modelId) {
        case 1226: // lamppost3
        case 1290: // lamppost2
        case 1294: // mlamppost
        case 1297: // lamppost1
        case 3665: // airyelrm_LAS
        case 3666: // airuntest_las
        case 5032: // las_runsigns_LAS
        case 5044: // las_runsignsx_LAS
            return true;
        default:
            return modelId >= 4995 && modelId <= 5000; // airport signage variants
    }
}

static void SuppressTempVehicleComponentCollision64(CEntityGTA* entity)
{
    if (!entity || !IsTempVehicleComponentModel64(entity->m_nModelIndex)) {
        return;
    }

    entity->m_bUsesCollision = false;
    entity->m_bCollisionProcessed = false;
    entity->m_bHasContacted = false;
    entity->m_bIsStuck = false;
    entity->m_bIsInSafePosition = false;
    entity->m_bIsVisible = false;
    entity->m_bDontStream = true;
}

static void PreserveBreakableWorldPropCollision64(CEntityGTA* entity)
{
    if (!entity) {
        return;
    }

    if (IsDynamicBreakableWorldPropModel64(entity->m_nModelIndex)) {
        return;
    }

    if (entity->m_bRemoveFromWorld || entity->m_bRenderDamaged) {
        return;
    }

    if (entity->IsPhysical()) {
        CPhysical* physical = static_cast<CPhysical*>(entity);
        if (physical->physicalFlags.bBroken ||
            physical->physicalFlags.bDestroyed ||
            entity->GetStatus() == STATUS_WRECKED) {
            return;
        }
    }

    if (entity->IsObject()) {
        CObjectGta* object = static_cast<CObjectGta*>(entity);
        if (object->m_nObjectType == OBJECT_TEMPORARY ||
            object->objectFlags.bIsBroken ||
            object->objectFlags.bDamaged ||
            object->objectFlags.bIsExploded ||
            object->objectFlags.bDoNotRender) {
            return;
        }
    }

    const bool keepVisible = entity->m_bIsVisible || entity->m_pRwObject != nullptr;
    entity->SetCollisionChecking(true);
    entity->m_bCollisionProcessed = false;
    entity->m_bHasContacted = false;
    entity->m_bIsStuck = false;
    entity->m_bIsInSafePosition = false;
    entity->m_bIsStaticWaitingForCollision = false;
    entity->m_bDontStream = false;
    entity->m_bIsVisible = keepVisible;
    if (entity->IsBuilding() || entity->IsDummy()) {
        entity->m_bIsStatic = true;
    }
}

#if !VER_x32
static void CopyModelName64(char (&dst)[22], CBaseModelInfo* modelInfo);
static bool IsUnstableBreakableWorldProp64(int modelId, CBaseModelInfo* modelInfo);

bool (*CWorld__ProcessVerticalLineNative)(CVector* origin,
                                          float targetZ,
                                          CColPoint* outColPoint,
                                          CEntityGTA** outEntity,
                                          bool buildings,
                                          bool vehicles,
                                          bool peds,
                                          bool objects,
                                          bool dummies,
                                          bool doSeeThroughCheck,
                                          CStoredCollPoly* storedPoly);

static constexpr bool kArm64VerticalGapFillEnabled = false;
static constexpr bool kArm64VerticalEntityFallbackEnabled = false;
static constexpr bool kArm64SyntheticWorldCollisionEnabled = false;
static constexpr bool kArm64SyntheticPropCollisionEnabled = true;
static constexpr bool kArm64NativeGroundHolePatchEnabled = false;
static constexpr bool kArm64SupplementalPhysicalCollisionEnabled = true;
static constexpr bool kArm64SupplementalPhysicalCollisionVehiclesEnabled = false;
static constexpr bool kArm64ElevatedAnchorAssistEnabled = false;

static bool TryFillNearbyVerticalCollisionGap64(CVector* origin,
                                                float targetZ,
                                                CColPoint* outColPoint,
                                                CEntityGTA** outEntity,
                                                bool buildings,
                                                bool vehicles,
                                                bool peds,
                                                bool objects,
                                                bool dummies,
                                                bool doSeeThroughCheck,
                                                CStoredCollPoly* storedPoly)
{
    if (!origin || !outColPoint || !outEntity || !CWorld__ProcessVerticalLineNative ||
        origin->z <= targetZ ||
        origin->z - targetZ < 1.5f ||
        origin->z - targetZ > 180.0f ||
        !std::isfinite(origin->x) ||
        !std::isfinite(origin->y) ||
        !std::isfinite(origin->z) ||
        !std::isfinite(targetZ)) {
        return false;
    }

    if (!pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
        return false;
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
    CPlayerPed* localPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;
    if (!localPed || !localPed->m_pPed || localPed->IsInVehicle()) {
        return false;
    }

    const CVector playerPos = localPed->m_pPed->GetPosition();
    const float dxPlayer = origin->x - playerPos.x;
    const float dyPlayer = origin->y - playerPos.y;
    const bool lowWorldGapProbe =
        playerPos.z >= -130.0f &&
        origin->z < -20.0f &&
        targetZ < -20.0f;
    if (dxPlayer * dxPlayer + dyPlayer * dyPlayer > 18.0f * 18.0f ||
        (!lowWorldGapProbe && playerPos.z < -20.0f) ||
        playerPos.z > 3000.0f) {
        return false;
    }

    struct SampleOffset64 {
        float x;
        float y;
    };

    static constexpr SampleOffset64 kOffsets[] = {
        {0.45f, 0.0f}, {-0.45f, 0.0f}, {0.0f, 0.45f}, {0.0f, -0.45f},
        {0.90f, 0.0f}, {-0.90f, 0.0f}, {0.0f, 0.90f}, {0.0f, -0.90f},
        {1.35f, 0.0f}, {-1.35f, 0.0f}, {0.0f, 1.35f}, {0.0f, -1.35f},
        {0.90f, 0.90f}, {0.90f, -0.90f}, {-0.90f, 0.90f}, {-0.90f, -0.90f},
        {1.80f, 0.0f}, {-1.80f, 0.0f}, {0.0f, 1.80f}, {0.0f, -1.80f},
        {2.20f, 0.0f}, {-2.20f, 0.0f}, {0.0f, 2.20f}, {0.0f, -2.20f},
        {1.80f, 1.80f}, {1.80f, -1.80f}, {-1.80f, 1.80f}, {-1.80f, -1.80f},
    };

    bool found = false;
    float bestScore = 1000000.0f;
    CColPoint bestCol{};
    CEntityGTA* bestEntity = nullptr;
    CStoredCollPoly bestStored{};
    SampleOffset64 bestOffset{};

    for (const SampleOffset64& offset : kOffsets) {
        CVector sample(origin->x + offset.x, origin->y + offset.y, origin->z);
        CColPoint sampleCol{};
        CEntityGTA* sampleEntity = nullptr;
        CStoredCollPoly sampleStored{};
        const bool hit = CWorld__ProcessVerticalLineNative(
            &sample,
            targetZ,
            &sampleCol,
            &sampleEntity,
            buildings,
            vehicles,
            peds,
            objects,
            dummies,
            doSeeThroughCheck,
            &sampleStored);

        if (!hit || !sampleEntity || !sampleEntity->m_bUsesCollision) {
            continue;
        }

        const eEntityType type = sampleEntity->GetType();
        if (type != ENTITY_TYPE_BUILDING &&
            type != ENTITY_TYPE_DUMMY) {
            continue;
        }

        const float groundToPed = playerPos.z - sampleCol.m_vecPoint.z;
        if (groundToPed < -2.25f || groundToPed > 4.50f) {
            continue;
        }

        const float dist2 = offset.x * offset.x + offset.y * offset.y;
        const float score = fabsf(groundToPed - 1.0f) * 3.0f + sqrtf(dist2);
        if (!found || score < bestScore) {
            found = true;
            bestScore = score;
            bestCol = sampleCol;
            bestEntity = sampleEntity;
            bestStored = sampleStored;
            bestOffset = offset;
        }
    }

    if (!found || !bestEntity) {
        return false;
    }

    *outEntity = bestEntity;
    *outColPoint = bestCol;
    outColPoint->m_vecPoint.x = origin->x;
    outColPoint->m_vecPoint.y = origin->y;
    if (storedPoly) {
        *storedPoly = bestStored;
    }

    static uint32_t s_lastGapFillLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastGapFillLogTick >= 450) {
        s_lastGapFillLogTick = now;
        char modelName[22]{};
        CopyModelName64(modelName, CModelInfo::GetModelInfo(bestEntity->m_nModelIndex));
        FLog("[COL_VLINE64] filled-gap origin=%.2f %.2f %.2f target=%.2f borrowed=%.2f %.2f z=%.2f model=%d,%s type=%u player=%.2f %.2f %.2f",
             origin->x,
             origin->y,
             origin->z,
             targetZ,
             origin->x + bestOffset.x,
             origin->y + bestOffset.y,
             outColPoint->m_vecPoint.z,
             bestEntity->m_nModelIndex,
             modelName[0] ? modelName : "?",
             static_cast<unsigned>(bestEntity->GetType()),
             playerPos.x,
             playerPos.y,
             playerPos.z);
    }

    return true;
}

struct VerticalEntityFallbackResult64 {
    CColPoint col{};
    CStoredCollPoly stored{};
    CEntityGTA* entity = nullptr;
    float z = -100000.0f;
    int scanned = 0;
    int candidates = 0;
    int areaSkipped = 0;
    int rangeSkipped = 0;
    int disabledCollision = 0;
    int badModel = 0;
    int missingColModel = 0;
    int missingColData = 0;
    int boundsSkipped = 0;
    int directMiss = 0;
    int rejectedZ = 0;
    int directHits = 0;
};

static bool IsEntityAreaActiveForVerticalFallback64(uint8_t entityArea, uint8_t playerArea)
{
    return entityArea == static_cast<uint8_t>(AREA_CODE_NORMAL_WORLD) ||
           entityArea == playerArea ||
           entityArea == static_cast<uint8_t>(AREA_CODE_13);
}

static bool IsCriticalNearbySurfaceModel64(CBaseModelInfo* modelInfo);
static bool IsKnownInteriorShellSurfaceModel64(CBaseModelInfo* modelInfo);
static bool IsLodWorldModel64(CBaseModelInfo* modelInfo);
static bool EnsureSyntheticSurfaceColData64(CEntityGTA* entity,
                                            CBaseModelInfo* modelInfo,
                                            CColModel* colModel,
                                            const CVector& sample,
                                            const char* reason);
static bool EnsureSupplementalSurfaceColBox64(CEntityGTA* entity,
                                              CBaseModelInfo* modelInfo,
                                              CColModel* colModel,
                                              const CVector& sample,
                                              const char* reason,
                                              float* outSurfaceZ = nullptr);
static bool TryProcessSupplementalColDataSurface64(CEntityGTA* entity,
                                                   CBaseModelInfo* modelInfo,
                                                   CColModel* colModel,
                                                   CVector* origin,
                                                   float targetZ,
                                                   VerticalEntityFallbackResult64& result);
static bool TryPatchNativeGroundCollisionHole64(CEntityGTA* entity,
                                                CBaseModelInfo* modelInfo,
                                                CColModel* colModel,
                                                CVector* origin,
                                                float targetZ,
                                                VerticalEntityFallbackResult64& result,
                                                const char* reason);
static float GetSyntheticSurfaceBoundPad64(CBaseModelInfo* modelInfo);
static bool IsTargetedHighSurfaceModel64(CBaseModelInfo* modelInfo);
static bool ShouldUseSupplementalSurfaceColBox64(CBaseModelInfo* modelInfo);
static float GetTargetedHighSurfacePad64(CBaseModelInfo* modelInfo);
static void GetStableSyntheticSurfaceBounds64(CColModel* colModel,
                                              int32 modelId,
                                              CVector& localMin,
                                              CVector& localMax);
static void ComputeEntityLocalBoundsWorld64(CEntityGTA* entity,
                                            const CVector& localMin,
                                            const CVector& localMax,
                                            float pad,
                                            CVector& worldMin,
                                            CVector& worldMax);
static void LogSyntheticSurfaceSkip64(CEntityGTA* entity,
                                      CBaseModelInfo* modelInfo,
                                      CColModel* colModel,
                                      const CVector& sample,
                                      const char* reason,
                                      const char* skipReason,
                                      const CVector& worldMin,
                                      const CVector& worldMax,
                                      float pad);
static float ClampFloat64(float value, float minValue, float maxValue);
static float RetainSyntheticSurfaceLocalZ64(CColModel* colModel,
                                            int32 modelId,
                                            float localX,
                                            float localY,
                                            float candidateLocalZ);

static CVector TransformLocalColPoint64(const CMatrix& matrix, const CVector& local)
{
    return CVector(
        matrix.m_pos.x + matrix.m_right.x * local.x + matrix.m_forward.x * local.y + matrix.m_up.x * local.z,
        matrix.m_pos.y + matrix.m_right.y * local.x + matrix.m_forward.y * local.y + matrix.m_up.y * local.z,
        matrix.m_pos.z + matrix.m_right.z * local.x + matrix.m_forward.z * local.y + matrix.m_up.z * local.z);
}

static CVector TransformWorldToLocalColPoint64(const CMatrix& matrix, const CVector& world)
{
    const CVector d(world.x - matrix.m_pos.x, world.y - matrix.m_pos.y, world.z - matrix.m_pos.z);
    return CVector(
        d.x * matrix.m_right.x + d.y * matrix.m_right.y + d.z * matrix.m_right.z,
        d.x * matrix.m_forward.x + d.y * matrix.m_forward.y + d.z * matrix.m_forward.z,
        d.x * matrix.m_up.x + d.y * matrix.m_up.y + d.z * matrix.m_up.z);
}

static void ExpandBoundsWithPoint64(CVector& min, CVector& max, const CVector& point)
{
    if (point.x < min.x) min.x = point.x;
    if (point.y < min.y) min.y = point.y;
    if (point.z < min.z) min.z = point.z;
    if (point.x > max.x) max.x = point.x;
    if (point.y > max.y) max.y = point.y;
    if (point.z > max.z) max.z = point.z;
}

static void SortLocalBounds64(CVector& localMin, CVector& localMax)
{
    if (localMin.x > localMax.x) std::swap(localMin.x, localMax.x);
    if (localMin.y > localMax.y) std::swap(localMin.y, localMax.y);
    if (localMin.z > localMax.z) std::swap(localMin.z, localMax.z);
}

static void ComputeEntityLocalBoundsWorld64(CEntityGTA* entity,
                                            const CVector& localMinIn,
                                            const CVector& localMaxIn,
                                            float pad,
                                            CVector& worldMin,
                                            CVector& worldMax)
{
    if (!entity) {
        worldMin = CVector(0.0f, 0.0f, 0.0f);
        worldMax = CVector(0.0f, 0.0f, 0.0f);
        return;
    }

    CVector localMin = localMinIn;
    CVector localMax = localMaxIn;
    SortLocalBounds64(localMin, localMax);

    CMatrix& matrix = entity->GetMatrix();
    worldMin = TransformLocalColPoint64(matrix, CVector(localMin.x, localMin.y, localMin.z));
    worldMax = worldMin;

    ExpandBoundsWithPoint64(worldMin, worldMax, TransformLocalColPoint64(matrix, CVector(localMax.x, localMin.y, localMin.z)));
    ExpandBoundsWithPoint64(worldMin, worldMax, TransformLocalColPoint64(matrix, CVector(localMin.x, localMax.y, localMin.z)));
    ExpandBoundsWithPoint64(worldMin, worldMax, TransformLocalColPoint64(matrix, CVector(localMax.x, localMax.y, localMin.z)));
    ExpandBoundsWithPoint64(worldMin, worldMax, TransformLocalColPoint64(matrix, CVector(localMin.x, localMin.y, localMax.z)));
    ExpandBoundsWithPoint64(worldMin, worldMax, TransformLocalColPoint64(matrix, CVector(localMax.x, localMin.y, localMax.z)));
    ExpandBoundsWithPoint64(worldMin, worldMax, TransformLocalColPoint64(matrix, CVector(localMin.x, localMax.y, localMax.z)));
    ExpandBoundsWithPoint64(worldMin, worldMax, TransformLocalColPoint64(matrix, CVector(localMax.x, localMax.y, localMax.z)));

    worldMin.x -= pad;
    worldMin.y -= pad;
    worldMin.z -= pad;
    worldMax.x += pad;
    worldMax.y += pad;
    worldMax.z += pad;
}

static void ComputeEntityColWorldBounds64(CEntityGTA* entity,
                                          CColModel* colModel,
                                          float pad,
                                          CVector& worldMin,
                                          CVector& worldMax)
{
    if (!entity || !colModel) {
        worldMin = CVector(0.0f, 0.0f, 0.0f);
        worldMax = CVector(0.0f, 0.0f, 0.0f);
        return;
    }

    const CVector localMin = colModel->m_boundBox.m_vecMin;
    const CVector localMax = colModel->m_boundBox.m_vecMax;
    ComputeEntityLocalBoundsWorld64(entity, localMin, localMax, pad, worldMin, worldMax);
}

static bool TryProcessMissingColDataSurface64(CEntityGTA* entity,
                                              CBaseModelInfo* modelInfo,
                                              CColModel* colModel,
                                              CVector* origin,
                                              float targetZ,
                                              VerticalEntityFallbackResult64& result)
{
    if (!kArm64SyntheticWorldCollisionEnabled) {
        return false;
    }

    if (!entity || !modelInfo || !colModel || !origin || origin->z <= targetZ ||
        !IsCriticalNearbySurfaceModel64(modelInfo)) {
        return false;
    }

    CVector worldMin{};
    CVector worldMax{};
    CVector localMin{};
    CVector localMax{};
    GetStableSyntheticSurfaceBounds64(colModel, entity->m_nModelIndex, localMin, localMax);
    const float boundPad = GetSyntheticSurfaceBoundPad64(modelInfo);
    ComputeEntityLocalBoundsWorld64(entity, localMin, localMax, boundPad, worldMin, worldMax);
    if (origin->x < worldMin.x || origin->x > worldMax.x ||
        origin->y < worldMin.y || origin->y > worldMax.y ||
        targetZ > worldMax.z + 180.0f ||
        origin->z < worldMin.z - 180.0f ||
        origin->z > worldMax.z + 180.0f) {
        ++result.boundsSkipped;
        LogSyntheticSurfaceSkip64(
            entity,
            modelInfo,
            colModel,
            *origin,
            "vertical",
            "bounds",
            worldMin,
            worldMax,
            boundPad);
        return false;
    }

    float surfaceZ = CWorld::FindGroundZForCoord(origin->x, origin->y);
    bool nativeSurface = std::isfinite(surfaceZ) &&
                         surfaceZ >= targetZ - 0.35f &&
                         surfaceZ <= origin->z + 1.50f &&
                         surfaceZ >= worldMin.z - 8.0f &&
                         surfaceZ <= worldMax.z + 8.0f;
    CMatrix& matrix = entity->GetMatrix();
    CVector localOriginSurface = TransformWorldToLocalColPoint64(
        matrix,
        CVector(origin->x, origin->y, origin->z - 1.0f));
    if (!nativeSurface && IsKnownInteriorShellSurfaceModel64(modelInfo)) {
        localOriginSurface.x = ClampFloat64(localOriginSurface.x, localMin.x, localMax.x);
        localOriginSurface.y = ClampFloat64(localOriginSurface.y, localMin.y, localMax.y);
        localOriginSurface.z = ClampFloat64(0.0f, localMin.z, localMax.z);
        const CVector worldSurface = TransformLocalColPoint64(matrix, localOriginSurface);
        surfaceZ = worldSurface.z;
    } else if (!nativeSurface) {
        const float retainedLocalZ = ClampFloat64(
            RetainSyntheticSurfaceLocalZ64(
                colModel,
                entity->m_nModelIndex,
                localOriginSurface.x,
                localOriginSurface.y,
                ClampFloat64(localOriginSurface.z, localMin.z - 180.0f, localMax.z + 180.0f)),
            localMin.z - 180.0f,
            localMax.z + 180.0f);
        localOriginSurface.x = ClampFloat64(localOriginSurface.x, localMin.x, localMax.x);
        localOriginSurface.y = ClampFloat64(localOriginSurface.y, localMin.y, localMax.y);
        localOriginSurface.z = retainedLocalZ;
        const CVector worldSurface = TransformLocalColPoint64(matrix, localOriginSurface);
        surfaceZ = worldSurface.z;
    }

    if (!std::isfinite(surfaceZ) ||
        surfaceZ < targetZ - 0.35f ||
        surfaceZ > origin->z + 1.50f ||
        surfaceZ < worldMin.z - 180.0f ||
        surfaceZ > worldMax.z + 180.0f) {
        ++result.rejectedZ;
        return false;
    }

    if (!IsKnownInteriorShellSurfaceModel64(modelInfo)) {
        EnsureSyntheticSurfaceColData64(entity, modelInfo, colModel, CVector(origin->x, origin->y, surfaceZ + 1.0f), "vertical");
    }

    CColPoint col{};
    col.m_vecPoint = CVector(origin->x, origin->y, surfaceZ);
    col.m_vecNormal = CVector(0.0f, 0.0f, 1.0f);
    col.m_nSurfaceTypeA = SURFACE_TARMAC;
    col.m_nSurfaceTypeB = SURFACE_TARMAC;
    col.m_nLightingA.value = 0xFF;
    col.m_nLightingB.value = 0xFF;
    col.m_fDepth = origin->z - surfaceZ;

    ++result.directHits;
    if (!result.entity || surfaceZ > result.z) {
        result.entity = entity;
        result.col = col;
        result.z = surfaceZ;
    }

    static uint32_t s_lastNoDataSurfaceLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastNoDataSurfaceLogTick >= 550) {
        s_lastNoDataSurfaceLogTick = now;
        char modelName[22]{};
        CopyModelName64(modelName, modelInfo);
        FLog("[COL_VENTITY64] no-data-surface fixed origin=%.2f %.2f %.2f target=%.2f z=%.2f native=%d model=%d,%s world={%.2f %.2f %.2f -> %.2f %.2f %.2f}",
             origin->x,
             origin->y,
             origin->z,
             targetZ,
             surfaceZ,
             nativeSurface ? 1 : 0,
             entity->m_nModelIndex,
             modelName[0] ? modelName : "?",
             worldMin.x,
             worldMin.y,
             worldMin.z,
             worldMax.x,
             worldMax.y,
             worldMax.z);
    }

    return true;
}

static bool ProbeEntityVerticalFallback64(CEntityGTA* entity,
                                          CVector* origin,
                                          float targetZ,
                                          VerticalEntityFallbackResult64& result)
{
    if (!entity || !origin || !CCollision__ProcessVerticalLine ||
        origin->z <= targetZ) {
        return false;
    }

    if (!entity->m_bUsesCollision) {
        ++result.disabledCollision;
        return false;
    }

    const int modelId = entity->m_nModelIndex;
    if (!Supremo::Streaming::IsValidResourceId(modelId)) {
        ++result.badModel;
        return false;
    }
    if (IsTempVehicleComponentModel64(modelId)) {
        SuppressTempVehicleComponentCollision64(entity);
        ++result.disabledCollision;
        return false;
    }

    CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
    if (!modelInfo) {
        ++result.badModel;
        return false;
    }
    if (IsUnstableBreakableWorldProp64(modelId, modelInfo)) {
        PreserveBreakableWorldPropCollision64(entity);
        ++result.disabledCollision;
        return false;
    }
    if (IsLodWorldModel64(modelInfo)) {
        ++result.disabledCollision;
        return false;
    }

    CColModel* colModel = modelInfo->m_pColModel;
    if (!colModel) {
        ++result.missingColModel;
        return false;
    }
    if (!colModel->m_pColData) {
        ++result.missingColData;
        TryProcessMissingColDataSurface64(entity, modelInfo, colModel, origin, targetZ, result);
        return false;
    }

    const float probeStartZ = origin->z + 2.5f;
    CVector worldMin{};
    CVector worldMax{};
    ComputeEntityColWorldBounds64(entity, colModel, 4.0f, worldMin, worldMax);

    if (origin->x < worldMin.x || origin->x > worldMax.x ||
        origin->y < worldMin.y || origin->y > worldMax.y ||
        probeStartZ < worldMin.z || targetZ > worldMax.z) {
        if (TryPatchNativeGroundCollisionHole64(
                entity,
                modelInfo,
                colModel,
                origin,
                targetZ,
                result,
                "bounds")) {
            return true;
        }
        if (ShouldUseSupplementalSurfaceColBox64(modelInfo) &&
            TryProcessSupplementalColDataSurface64(entity, modelInfo, colModel, origin, targetZ, result)) {
            return true;
        }
        ++result.boundsSkipped;
        return false;
    }

    CColLine line{};
    line.m_vecStart = CVector(origin->x, origin->y, probeStartZ);
    line.m_vecEnd = CVector(origin->x, origin->y, targetZ);
    line.m_fStartSize = 0.0f;
    line.m_fEndSize = 0.0f;

    CColPoint col{};
    CStoredCollPoly stored{};
    float maxTouchDistance = probeStartZ - targetZ;
    CMatrix& matrix = entity->GetMatrix();
    const bool hit = CCollision__ProcessVerticalLine(
        &line,
        &matrix,
        colModel,
        &col,
        &maxTouchDistance,
        false,
        false,
        &stored);

    if (!hit || !std::isfinite(col.m_vecPoint.z)) {
        ++result.directMiss;
        if (TryPatchNativeGroundCollisionHole64(
                entity,
                modelInfo,
                colModel,
                origin,
                targetZ,
                result,
                "direct-miss")) {
            return true;
        }
        TryProcessSupplementalColDataSurface64(entity, modelInfo, colModel, origin, targetZ, result);
        return false;
    }
    const bool targetedHighSurface = IsTargetedHighSurfaceModel64(modelInfo);
    if (targetedHighSurface &&
        col.m_vecPoint.z > origin->z + 0.75f &&
        TryProcessSupplementalColDataSurface64(entity, modelInfo, colModel, origin, targetZ, result)) {
        return true;
    }

    const float upwardTolerance = targetedHighSurface ? 3.25f : 1.25f;
    if (col.m_vecPoint.z > origin->z + upwardTolerance ||
        col.m_vecPoint.z < targetZ - 0.25f) {
        ++result.rejectedZ;
        if (TryPatchNativeGroundCollisionHole64(
                entity,
                modelInfo,
                colModel,
                origin,
                targetZ,
                result,
                "rejected-z")) {
            return true;
        }
        TryProcessSupplementalColDataSurface64(entity, modelInfo, colModel, origin, targetZ, result);
        return false;
    }

    ++result.directHits;
    if (!result.entity || col.m_vecPoint.z > result.z) {
        result.entity = entity;
        result.col = col;
        result.stored = stored;
        result.z = col.m_vecPoint.z;
    }

    return true;
}

template <typename TPool>
static void ProbeWorldPoolVerticalFallback64(TPool* pool,
                                             CVector* origin,
                                             float targetZ,
                                             uint8_t playerArea,
                                             VerticalEntityFallbackResult64& result)
{
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0) {
        return;
    }

    for (int32 i = 0; i < pool->m_nSize; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity) {
            continue;
        }

        ++result.scanned;
        if (!IsEntityAreaActiveForVerticalFallback64(static_cast<uint8_t>(entity->m_nAreaCode), playerArea)) {
            ++result.areaSkipped;
            continue;
        }

        const int modelId = entity->m_nModelIndex;
        if (!Supremo::Streaming::IsValidResourceId(modelId)) {
            ++result.badModel;
            continue;
        }
        if (IsTempVehicleComponentModel64(modelId)) {
            SuppressTempVehicleComponentCollision64(entity);
            ++result.disabledCollision;
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
        if (!modelInfo) {
            ++result.badModel;
            continue;
        }
        if (IsUnstableBreakableWorldProp64(modelId, modelInfo)) {
            PreserveBreakableWorldPropCollision64(entity);
            ++result.disabledCollision;
            continue;
        }
        if (IsLodWorldModel64(modelInfo)) {
            ++result.disabledCollision;
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        const float dx = entityPos.x - origin->x;
        const float dy = entityPos.y - origin->y;
        const float rangeLimit =
            (IsTargetedHighSurfaceModel64(modelInfo) || IsCriticalNearbySurfaceModel64(modelInfo))
                ? 420.0f
                : 230.0f;
        if (dx * dx + dy * dy > rangeLimit * rangeLimit) {
            ++result.rangeSkipped;
            continue;
        }

        ++result.candidates;
        ProbeEntityVerticalFallback64(entity, origin, targetZ, result);
    }
}

template <typename TPool>
static void LogVerticalHoleCandidatesPool64(const char* poolName,
                                            TPool* pool,
                                            CVector* origin,
                                            float targetZ,
                                            uint8_t playerArea,
                                            int& logged,
                                            int maxLogs)
{
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || !origin || logged >= maxLogs) {
        return;
    }

    for (int32 i = 0; i < pool->m_nSize && logged < maxLogs; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity ||
            !IsEntityAreaActiveForVerticalFallback64(static_cast<uint8_t>(entity->m_nAreaCode), playerArea)) {
            continue;
        }

        const int modelId = entity->m_nModelIndex;
        if (!Supremo::Streaming::IsValidResourceId(modelId) || IsTempVehicleComponentModel64(modelId)) {
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
        if (!modelInfo ||
            IsUnstableBreakableWorldProp64(modelId, modelInfo) ||
            IsLodWorldModel64(modelInfo)) {
            continue;
        }

        CColModel* colModel = modelInfo->m_pColModel;
        if (!colModel) {
            continue;
        }

        const bool criticalSurface = IsCriticalNearbySurfaceModel64(modelInfo);
        const float pad = criticalSurface ? GetSyntheticSurfaceBoundPad64(modelInfo) : 24.0f;
        CVector localMin{};
        CVector localMax{};
        GetStableSyntheticSurfaceBounds64(colModel, modelId, localMin, localMax);
        CVector worldMin{};
        CVector worldMax{};
        ComputeEntityLocalBoundsWorld64(entity, localMin, localMax, pad, worldMin, worldMax);

        if (origin->x < worldMin.x || origin->x > worldMax.x ||
            origin->y < worldMin.y || origin->y > worldMax.y) {
            continue;
        }

        CMatrix& matrix = entity->GetMatrix();
        const CVector localSample = TransformWorldToLocalColPoint64(matrix, *origin);
        const bool zPlausible =
            (origin->z >= worldMin.z - 24.0f && targetZ <= worldMax.z + 24.0f) ||
            (localSample.z >= localMin.z - 220.0f && localSample.z <= localMax.z + 220.0f);
        if (!zPlausible) {
            continue;
        }

        CCollisionData* data = colModel->m_pColData;
        char modelName[22]{};
        CopyModelName64(modelName, modelInfo);
        const CVector entityPos = entity->GetPosition();
        FLog("[COL_HOLE64] pool=%s slot=%d origin=%.2f %.2f %.2f target=%.2f model=%d,%s colSlot=%u data=%d tris=%u boxes=%u flags=0x%02X road=%d critical=%d localSample=%.2f %.2f %.2f local={%.2f %.2f %.2f -> %.2f %.2f %.2f} world={%.2f %.2f %.2f -> %.2f %.2f %.2f} ent=%.2f %.2f %.2f",
             poolName ? poolName : "?",
             i,
             origin->x,
             origin->y,
             origin->z,
             targetZ,
             modelId,
             modelName[0] ? modelName : "?",
             static_cast<unsigned>(colModel->m_nColSlot),
             data ? 1 : 0,
             data ? data->m_nNumTriangles : 0,
             data ? data->m_nNumBoxes : 0,
             static_cast<unsigned>(colModel->m_nFlags),
             modelInfo->bIsRoad ? 1 : 0,
             criticalSurface ? 1 : 0,
             localSample.x,
             localSample.y,
             localSample.z,
             localMin.x,
             localMin.y,
             localMin.z,
             localMax.x,
             localMax.y,
             localMax.z,
             worldMin.x,
             worldMin.y,
             worldMin.z,
             worldMax.x,
             worldMax.y,
             worldMax.z,
             entityPos.x,
             entityPos.y,
             entityPos.z);
        ++logged;
    }
}

static void LogVerticalHoleCandidates64(CVector* origin,
                                        float targetZ,
                                        uint8_t playerArea)
{
    static uint32_t s_lastHoleLogTick = 0;
    const uint32_t now = GetTickCount();
    if (!origin || now - s_lastHoleLogTick < 700) {
        return;
    }
    s_lastHoleLogTick = now;

    int logged = 0;
    constexpr int kMaxHoleLogs = 10;
    LogVerticalHoleCandidatesPool64("B", GetBuildingPool(), origin, targetZ, playerArea, logged, kMaxHoleLogs);
    LogVerticalHoleCandidatesPool64("O", GetObjectPoolGta(), origin, targetZ, playerArea, logged, kMaxHoleLogs);
    LogVerticalHoleCandidatesPool64("D", GetDummyPool(), origin, targetZ, playerArea, logged, kMaxHoleLogs);
}

static bool TryProcessVerticalLineFromLoadedEntities64(CVector* origin,
                                                       float targetZ,
                                                       CColPoint* outColPoint,
                                                       CEntityGTA** outEntity,
                                                       bool buildings,
                                                       bool objects,
                                                       bool dummies,
                                                       CStoredCollPoly* storedPoly,
                                                       const char* mode)
{
    if (!kArm64VerticalEntityFallbackEnabled ||
        !origin ||
        !outColPoint ||
        !outEntity ||
        origin->z <= targetZ ||
        origin->z - targetZ > 260.0f ||
        !std::isfinite(origin->x) ||
        !std::isfinite(origin->y) ||
        !std::isfinite(origin->z) ||
        !std::isfinite(targetZ) ||
        !pNetGame ||
        pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
        return false;
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
    CPlayerPed* localPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;
    if (!localPed || !localPed->m_pPed) {
        return false;
    }

    const CVector playerPos = localPed->m_pPed->GetPosition();
    const float dxPlayer = origin->x - playerPos.x;
    const float dyPlayer = origin->y - playerPos.y;
    if (dxPlayer * dxPlayer + dyPlayer * dyPlayer > 190.0f * 190.0f ||
        playerPos.z < -250.0f ||
        playerPos.z > 3000.0f) {
        return false;
    }

    VerticalEntityFallbackResult64 result{};
    const uint8_t playerArea = NormalizeLocalExteriorArea64(localPed, playerPos, "vertical-fallback");
    if (buildings) {
        ProbeWorldPoolVerticalFallback64(GetBuildingPool(), origin, targetZ, playerArea, result);
    }
    if (objects) {
        ProbeWorldPoolVerticalFallback64(GetObjectPoolGta(), origin, targetZ, playerArea, result);
    }
    if (dummies) {
        ProbeWorldPoolVerticalFallback64(GetDummyPool(), origin, targetZ, playerArea, result);
    }

    if (!result.entity) {
        if (kArm64VerticalGapFillEnabled &&
            TryFillNearbyVerticalCollisionGap64(
                origin,
                targetZ,
                outColPoint,
                outEntity,
                buildings,
                false,
                false,
                objects,
                dummies,
                false,
                storedPoly)) {
            return true;
        }

        static uint32_t s_lastEntityFallbackMissLogTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastEntityFallbackMissLogTick >= 2400) {
            s_lastEntityFallbackMissLogTick = now;
            FLog("[COL_VENTITY64] %s no-entity origin=%.2f %.2f %.2f target=%.2f scanned=%d candidates=%d skip={area:%d,range:%d,disabled:%d,bad:%d,noCol:%d,noData:%d,bounds:%d,miss:%d,z:%d,hit:%d} flags={B:%d,O:%d,D:%d} player=%.2f %.2f %.2f area=%u",
                 mode ? mode : "probe",
                 origin->x,
                 origin->y,
                 origin->z,
                 targetZ,
                 result.scanned,
                 result.candidates,
                 result.areaSkipped,
                 result.rangeSkipped,
                 result.disabledCollision,
                 result.badModel,
                 result.missingColModel,
                 result.missingColData,
                 result.boundsSkipped,
                 result.directMiss,
                 result.rejectedZ,
                 result.directHits,
                 buildings ? 1 : 0,
                 objects ? 1 : 0,
                 dummies ? 1 : 0,
                 playerPos.x,
                 playerPos.y,
                 playerPos.z,
                 static_cast<unsigned>(playerArea));
        }
        LogVerticalHoleCandidates64(origin, targetZ, playerArea);
        return false;
    }

    *outEntity = result.entity;
    *outColPoint = result.col;
    outColPoint->m_vecPoint.x = origin->x;
    outColPoint->m_vecPoint.y = origin->y;
    if (storedPoly) {
        *storedPoly = result.stored;
    }

    static uint32_t s_lastEntityFallbackLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastEntityFallbackLogTick >= 350) {
        s_lastEntityFallbackLogTick = now;
        char modelName[22]{};
        CopyModelName64(modelName, CModelInfo::GetModelInfo(result.entity->m_nModelIndex));
        FLog("[COL_VENTITY64] %s fixed origin=%.2f %.2f %.2f target=%.2f z=%.2f model=%d,%s type=%u area=%u scanned=%d candidates=%d player=%.2f %.2f %.2f",
             mode ? mode : "probe",
             origin->x,
             origin->y,
             origin->z,
             targetZ,
             outColPoint->m_vecPoint.z,
             result.entity->m_nModelIndex,
             modelName[0] ? modelName : "?",
             static_cast<unsigned>(result.entity->GetType()),
             static_cast<unsigned>(result.entity->m_nAreaCode),
             result.scanned,
             result.candidates,
             playerPos.x,
             playerPos.y,
             playerPos.z);
    }

    return true;
}

extern "C" bool ProbeArm64SpawnSurfaceForPosition(const CVector& pos,
                                                  uint8_t area,
                                                  float* outSurfaceZ,
                                                  int32* outModelId)
{
    if (!outSurfaceZ ||
        !std::isfinite(pos.x) ||
        !std::isfinite(pos.y) ||
        !std::isfinite(pos.z)) {
        return false;
    }

    VerticalEntityFallbackResult64 result{};
    CVector origin(pos.x, pos.y, pos.z + 2.25f);
    const float targetZ = pos.z - 6.0f;
    ProbeWorldPoolVerticalFallback64(GetBuildingPool(), &origin, targetZ, area, result);
    ProbeWorldPoolVerticalFallback64(GetObjectPoolGta(), &origin, targetZ, area, result);
    ProbeWorldPoolVerticalFallback64(GetDummyPool(), &origin, targetZ, area, result);

    if (!result.entity ||
        !std::isfinite(result.z) ||
        result.z < pos.z - 1.25f ||
        result.z > pos.z + 3.25f) {
        return false;
    }

    *outSurfaceZ = result.z;
    if (outModelId) {
        *outModelId = result.entity->m_nModelIndex;
    }
    return true;
}

static bool TryOverrideLowNativeVerticalHit64(CVector* origin,
                                              float targetZ,
                                              CColPoint* outColPoint,
                                              CEntityGTA** outEntity,
                                              bool buildings,
                                              bool objects,
                                              bool dummies,
                                              CStoredCollPoly* storedPoly)
{
    if (!origin || !outColPoint || !outEntity ||
        origin->z <= targetZ ||
        !std::isfinite(origin->z) ||
        !std::isfinite(outColPoint->m_vecPoint.z)) {
        return false;
    }

    const float nativeZ = outColPoint->m_vecPoint.z;
    if (origin->z - nativeZ < 4.0f) {
        return false;
    }

    CColPoint candidateCol{};
    CEntityGTA* candidateEntity = nullptr;
    CStoredCollPoly candidateStored{};
    if (!TryProcessVerticalLineFromLoadedEntities64(
            origin,
            targetZ,
            &candidateCol,
            &candidateEntity,
            buildings,
            objects,
            dummies,
            &candidateStored,
            "native-low")) {
        return false;
    }

    if (!candidateEntity ||
        !std::isfinite(candidateCol.m_vecPoint.z) ||
        candidateCol.m_vecPoint.z <= nativeZ + 1.25f ||
        candidateCol.m_vecPoint.z > origin->z + 1.25f ||
        candidateCol.m_vecPoint.z < targetZ - 0.25f) {
        return false;
    }

    CEntityGTA* nativeEntity = *outEntity;
    *outColPoint = candidateCol;
    *outEntity = candidateEntity;
    if (storedPoly) {
        *storedPoly = candidateStored;
    }

    static uint32_t s_lastLowOverrideLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastLowOverrideLogTick >= 350) {
        s_lastLowOverrideLogTick = now;
        char candidateName[22]{};
        char nativeName[22]{};
        CopyModelName64(candidateName, CModelInfo::GetModelInfo(candidateEntity->m_nModelIndex));
        if (nativeEntity) {
            CopyModelName64(nativeName, CModelInfo::GetModelInfo(nativeEntity->m_nModelIndex));
        }
        FLog("[COL_VENTITY64] native-low override origin=%.2f %.2f %.2f target=%.2f native=%.2f,%d,%s high=%.2f,%d,%s flags={B:%d,O:%d,D:%d}",
             origin->x,
             origin->y,
             origin->z,
             targetZ,
             nativeZ,
             nativeEntity ? nativeEntity->m_nModelIndex : -1,
             nativeName[0] ? nativeName : "?",
             candidateCol.m_vecPoint.z,
             candidateEntity->m_nModelIndex,
             candidateName[0] ? candidateName : "?",
             buildings ? 1 : 0,
             objects ? 1 : 0,
             dummies ? 1 : 0);
    }

    return true;
}

bool CWorld__ProcessVerticalLine_hook(CVector* origin,
                                      float targetZ,
                                      CColPoint* outColPoint,
                                      CEntityGTA** outEntity,
                                      bool buildings,
                                      bool vehicles,
                                      bool peds,
                                      bool objects,
                                      bool dummies,
                                      bool doSeeThroughCheck,
                                      CStoredCollPoly* storedPoly)
{
    const bool hit = CWorld__ProcessVerticalLineNative(
        origin,
        targetZ,
        outColPoint,
        outEntity,
        buildings,
        vehicles,
        peds,
        objects,
        dummies,
        doSeeThroughCheck,
        storedPoly);

    if (hit) {
        if (kArm64VerticalEntityFallbackEnabled) {
            TryOverrideLowNativeVerticalHit64(
                origin,
                targetZ,
                outColPoint,
                outEntity,
                buildings,
                objects,
                dummies,
                storedPoly);
        }
        return true;
    }

    if (TryProcessVerticalLineFromLoadedEntities64(
            origin,
            targetZ,
            outColPoint,
            outEntity,
            buildings,
            objects,
            dummies,
            storedPoly,
            "native-miss")) {
        return true;
    }

    if (!kArm64VerticalGapFillEnabled) {
        return false;
    }

    return TryFillNearbyVerticalCollisionGap64(
        origin,
        targetZ,
        outColPoint,
        outEntity,
        buildings,
        vehicles,
        peds,
        objects,
        dummies,
        doSeeThroughCheck,
        storedPoly);
}

bool (*CPhysical__CheckCollisionNative)(CPhysical* physical);

static CPlayerPed* GetConnectedLocalPlayerPed64()
{
    if (!pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
        return nullptr;
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
    if (!localPlayer || !localPlayer->m_bIsActive) {
        return nullptr;
    }

    return localPlayer->GetPlayerPed();
}

static bool IsPhysicalWaterActive64(const CPhysical* physical)
{
    return physical &&
           (physical->physicalFlags.bSubmergedInWater ||
            physical->physicalFlags.bTouchingWater);
}

static bool IsLocalPedWaterActive64(CPlayerPed* localPed)
{
    if (!localPed || !localPed->m_pPed) {
        return false;
    }

    CPedGTA* ped = localPed->m_pPed;
    if (IsPhysicalWaterActive64(reinterpret_cast<const CPhysical*>(ped))) {
        return true;
    }

    if (ped->m_pPlayerData) {
        if (ped->m_pPlayerData->m_nWaterCoverPerc > 0) {
            return true;
        }
    }

    if (localPed->IsInVehicle()) {
        CVehicleGTA* vehicle = localPed->GetGtaVehicle();
        if (IsPhysicalWaterActive64(reinterpret_cast<const CPhysical*>(vehicle))) {
            return true;
        }
    }

    return false;
}

static bool IsLocalPedOrVehiclePhysical64(CPlayerPed* localPed, const CPhysical* physical)
{
    if (!localPed || !localPed->m_pPed || !physical) {
        return false;
    }

    if (physical == reinterpret_cast<const CPhysical*>(localPed->m_pPed)) {
        return true;
    }

    if (localPed->IsInVehicle()) {
        CVehicleGTA* vehicle = localPed->GetGtaVehicle();
        if (vehicle && physical == reinterpret_cast<const CPhysical*>(vehicle)) {
            return true;
        }
    }

    return false;
}

static bool IsLocalPedJumpPhysical64(CPlayerPed* localPed, const CPhysical* physical)
{
    return localPed &&
           localPed->m_pPed &&
           physical == reinterpret_cast<const CPhysical*>(localPed->m_pPed) &&
           !localPed->IsInVehicle();
}

static void TraceLocalPedJumpPhysics64(const char* tag, CPlayerPed* localPed, CPhysical* physical, bool force)
{
    if (!localPed || !localPed->m_pPed || !physical) {
        return;
    }

    CPedGTA* ped = localPed->m_pPed;
    const bool jumpDown = Supremo::Input::IsJumpDown();
    const bool waterActive = IsLocalPedWaterActive64(localPed);
    const bool downwardPull = physical->m_vecMoveSpeed.z < -0.035f;
    const bool blocked =
        ped->bMidriffBlockedForJump ||
        ped->bHeadStuckInCollision ||
        ped->bStuckUnderCar ||
        ped->bCheckColAboveHead ||
        ped->physicalFlags.bSkipLineCol;

    if (!force && !jumpDown && !waterActive && !downwardPull && !blocked) {
        return;
    }

    static uint32_t s_lastInputPhysicsTraceTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastInputPhysicsTraceTick < 850) {
        return;
    }
    s_lastInputPhysicsTraceTick = now;

    const CVector pos = physical->GetPosition();
    const unsigned waterCover = ped->m_pPlayerData
        ? static_cast<unsigned>(ped->m_pPlayerData->m_nWaterCoverPerc)
        : 0;

    FLog("[INPUT_PHYS64] tag=%s jump=%d water=%d down=%d inVeh=%d pos=%.2f %.2f %.2f vel=%.3f %.3f %.3f standing=%d/%d blockers={mid:%d,head:%d,car:%d,above:%d,skip:%d} cover=%u flags={sub:%d,touch:%d,solid:%d}",
         tag ? tag : "?",
         jumpDown ? 1 : 0,
         waterActive ? 1 : 0,
         downwardPull ? 1 : 0,
         localPed->IsInVehicle() ? 1 : 0,
         pos.x,
         pos.y,
         pos.z,
         physical->m_vecMoveSpeed.x,
         physical->m_vecMoveSpeed.y,
         physical->m_vecMoveSpeed.z,
         ped->bIsStanding ? 1 : 0,
         ped->bWasStanding ? 1 : 0,
         ped->bMidriffBlockedForJump ? 1 : 0,
         ped->bHeadStuckInCollision ? 1 : 0,
         ped->bStuckUnderCar ? 1 : 0,
         ped->bCheckColAboveHead ? 1 : 0,
         ped->physicalFlags.bSkipLineCol ? 1 : 0,
         waterCover,
         physical->physicalFlags.bSubmergedInWater ? 1 : 0,
         physical->physicalFlags.bTouchingWater ? 1 : 0,
         physical->physicalFlags.bOnSolidSurface ? 1 : 0);
}

static void TraceVehicleRampPhysics64(const char* mode, CPhysical* physical)
{
    if (!physical || !physical->IsVehicle()) {
        return;
    }

    const bool verticalImpulse =
        physical->m_vecMoveSpeed.z > 0.080f ||
        physical->m_vecMoveSpeed.z < -0.120f;
    if (!verticalImpulse) {
        return;
    }

    static uint32_t s_lastVehicleRampTraceTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastVehicleRampTraceTick < 1200) {
        return;
    }
    s_lastVehicleRampTraceTick = now;

    const CVector pos = physical->GetPosition();
    FLog("[INPUT_PHYS64] tag=vehicle-ramp mode=%s pos=%.2f %.2f %.2f vel=%.3f %.3f %.3f flags={water:%d,use:%d,col:%d,simple:%d,stationary:%d}",
         mode ? mode : "?",
         pos.x,
         pos.y,
         pos.z,
         physical->m_vecMoveSpeed.x,
         physical->m_vecMoveSpeed.y,
         physical->m_vecMoveSpeed.z,
         IsPhysicalWaterActive64(physical) ? 1 : 0,
         physical->m_bUsesCollision ? 1 : 0,
         physical->physicalFlags.bCollidable ? 1 : 0,
         physical->physicalFlags.bDisableSimpleCollision ? 1 : 0,
         physical->physicalFlags.bProcessCollisionEvenIfStationary ? 1 : 0);
}

static bool IsLocalPedJumpInputActive64(CPlayerPed* localPed, CPhysical* physical)
{
    const bool active =
        IsLocalPedJumpPhysical64(localPed, physical) &&
        Supremo::Input::IsJumpDown();

    if (active) {
        TraceLocalPedJumpPhysics64("jump-input", localPed, physical, false);
    }

    return active;
}

static void RelaxLocalPedJumpBlockers64(CPlayerPed* localPed, CPhysical* physical)
{
    if (!IsLocalPedJumpPhysical64(localPed, physical)) {
        return;
    }

    CPedGTA* ped = localPed->m_pPed;
    ped->bMidriffBlockedForJump = false;
    ped->bHeadStuckInCollision = false;
    ped->bStuckUnderCar = false;
    ped->bCheckColAboveHead = false;
    ped->physicalFlags.bSkipLineCol = false;
}

static bool StabilizeLocalPedWaterJump64(CPlayerPed* localPed, CPhysical* physical)
{
    if (!IsLocalPedJumpPhysical64(localPed, physical) ||
        !IsLocalPedWaterActive64(localPed)) {
        return false;
    }

    CPedGTA* ped = localPed->m_pPed;
    ped->physicalFlags.bOnSolidSurface = false;
    ped->bIsStanding = false;
    ped->bWasStanding = false;
    ped->bMidriffBlockedForJump = false;
    ped->bHeadStuckInCollision = false;
    ped->bStuckUnderCar = false;

    const bool jumpDown = Supremo::Input::IsJumpDown();
    if (jumpDown || physical->m_vecMoveSpeed.z < -0.035f) {
        TraceLocalPedJumpPhysics64("water-jump", localPed, physical, true);
    }

    if (jumpDown && physical->m_vecMoveSpeed.z < -0.035f) {
        physical->m_vecMoveSpeed.z = -0.004f;
    }

    return true;
}

static CPhysical* GetGamePlayerSupplementalCollisionPhysical64()
{
    CPlayerPed* gamePlayer = pGame ? pGame->FindPlayerPed() : nullptr;
    CPedGTA* ped = gamePlayer ? gamePlayer->m_pPed : nullptr;
    if (!ped) {
        return nullptr;
    }

    if (ped->IsInVehicle() && !ped->IsAPassenger()) {
        return ped->pVehicle ? reinterpret_cast<CPhysical*>(ped->pVehicle) : nullptr;
    }

    return reinterpret_cast<CPhysical*>(ped);
}

static CPhysical* GetLocalSupplementalCollisionPhysical64()
{
    if (pNetGame && pNetGame->GetGameState() == GAMESTATE_CONNECTED) {
        CPlayerPool* playerPool = pNetGame->GetPlayerPool();
        CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
        CPlayerPed* localPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;
        if (localPed && localPed->m_pPed) {
            if (localPed->IsInVehicle() && !localPed->IsAPassenger()) {
                CVehicleGTA* vehicle = localPed->GetGtaVehicle();
                if (vehicle) {
                    return reinterpret_cast<CPhysical*>(vehicle);
                }
            }

            return reinterpret_cast<CPhysical*>(localPed->m_pPed);
        }
    }

    return GetGamePlayerSupplementalCollisionPhysical64();
}

static bool TryApplyArm64SupplementalPhysicalCollision64(CPhysical* physical, const char* mode)
{
    CPhysical* expectedPhysical = GetLocalSupplementalCollisionPhysical64();
    CPlayerPed* localPed = GetConnectedLocalPlayerPed64();
    const bool fromGameProcess =
        mode &&
        std::strncmp(mode, "game-process", sizeof("game-process") - 1) == 0;
    if (!physical ||
        physical != expectedPhysical ||
        physical->m_bRemoveFromWorld ||
        (!physical->IsPed() && !physical->IsVehicle())) {
        if (fromGameProcess) {
            static uint32_t s_lastEarlySkipLogTick = 0;
            const uint32_t now = GetTickCount();
            if (now - s_lastEarlySkipLogTick >= 700) {
                s_lastEarlySkipLogTick = now;
                const CVector pos = physical ? physical->GetPosition() : CVector{};
                const int type = physical ? static_cast<int>(physical->m_nType) : -1;
                FLog("[COL_PHYS64] early-skip mode=%s physical=%p expected=%p type=%d pos=%.2f %.2f %.2f rem=%d use=%d ped=%d veh=%d net=%d state=%d",
                     mode ? mode : "?",
                     physical,
                     expectedPhysical,
                     type,
                     pos.x,
                     pos.y,
                     pos.z,
                     physical ? (physical->m_bRemoveFromWorld ? 1 : 0) : -1,
                     physical ? (physical->m_bUsesCollision ? 1 : 0) : -1,
                     physical ? (physical->IsPed() ? 1 : 0) : -1,
                     physical ? (physical->IsVehicle() ? 1 : 0) : -1,
                     pNetGame ? 1 : 0,
                     pNetGame ? static_cast<int>(pNetGame->GetGameState()) : -1);
            }
        }
        return false;
    }

    if (localPed && IsLocalPedOrVehiclePhysical64(localPed, physical)) {
        if (IsLocalPedWaterActive64(localPed)) {
            return false;
        }
        if (IsLocalPedJumpInputActive64(localPed, physical)) {
            RelaxLocalPedJumpBlockers64(localPed, physical);
            return false;
        }
    }

    static bool s_inSupplementalPhysicalCollision = false;
    if (s_inSupplementalPhysicalCollision) {
        return false;
    }

    const bool vehicle = physical->IsVehicle();
    TraceVehicleRampPhysics64(mode, physical);
    if (vehicle && !kArm64SupplementalPhysicalCollisionVehiclesEnabled) {
        return false;
    }
    if (IsPhysicalWaterActive64(physical)) {
        return false;
    }

    const CVector pos = physical->GetPosition();
    if (!std::isfinite(pos.x) ||
        !std::isfinite(pos.y) ||
        !std::isfinite(pos.z) ||
        pos.z < -250.0f ||
        pos.z > 3000.0f) {
        return false;
    }

    const bool wasUsingCollision = physical->m_bUsesCollision;
    physical->m_bUsesCollision = true;
    physical->physicalFlags.bCollidable = true;
    physical->physicalFlags.bCanBeCollidedWith = true;
    physical->physicalFlags.bDisableSimpleCollision = false;
    physical->physicalFlags.bProcessCollisionEvenIfStationary = true;

    if (vehicle && !kArm64SupplementalPhysicalCollisionVehiclesEnabled) {
        static uint32_t s_lastVehicleSupplementalDisabledLogTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastVehicleSupplementalDisabledLogTick >= 1200) {
            s_lastVehicleSupplementalDisabledLogTick = now;
            FLog("[COL_PHYS64] skip vehicle-supplemental-disabled mode=%s pos=%.2f %.2f %.2f vel=%.3f %.3f %.3f",
                 mode ? mode : "?",
                 pos.x,
                 pos.y,
                 pos.z,
                 physical->m_vecMoveSpeed.x,
                 physical->m_vecMoveSpeed.y,
                 physical->m_vecMoveSpeed.z);
        }
        return false;
    }
    if (!kArm64SupplementalPhysicalCollisionEnabled) {
        static uint32_t s_lastSupplementalDisabledLogTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastSupplementalDisabledLogTick >= 900) {
            s_lastSupplementalDisabledLogTick = now;
            FLog("[COL_PHYS64] skip supplemental-disabled mode=%s phys=%d pos=%.2f %.2f %.2f vel=%.3f %.3f %.3f",
                 mode ? mode : "?",
                 vehicle ? 2 : 1,
                 pos.x,
                 pos.y,
                 pos.z,
                 physical->m_vecMoveSpeed.x,
                 physical->m_vecMoveSpeed.y,
                 physical->m_vecMoveSpeed.z);
        }
        return false;
    }

    s_inSupplementalPhysicalCollision = true;

    CVector origin(pos.x, pos.y, pos.z + (vehicle ? 3.75f : 2.25f));
    const float targetZ = pos.z - (vehicle ? 12.50f : 18.00f);
    CColPoint col{};
    CEntityGTA* entity = nullptr;
    CStoredCollPoly stored{};
    bool hit = CWorld::ProcessVerticalLine(
        &origin,
        targetZ,
        &col,
        &entity,
        true,
        false,
        false,
        true,
        true,
        false,
        &stored);
    if (!hit || !entity || !entity->m_bUsesCollision) {
        hit = TryProcessVerticalLineFromLoadedEntities64(
            &origin,
            targetZ,
            &col,
            &entity,
            true,
            true,
            true,
            &stored,
            mode ? mode : "phys-check");
    }

    s_inSupplementalPhysicalCollision = false;

    if (!hit || !entity || !std::isfinite(col.m_vecPoint.z)) {
        if (fromGameProcess) {
            static uint32_t s_lastNoHitLogTick = 0;
            const uint32_t now = GetTickCount();
            if (now - s_lastNoHitLogTick >= 900) {
                s_lastNoHitLogTick = now;
                FLog("[COL_PHYS64] no-real-surface mode=%s phys=%d pos=%.2f %.2f %.2f target=%.2f vel=%.3f %.3f %.3f hit=%d entity=%p wasUse=%d",
                     mode ? mode : "?",
                     vehicle ? 2 : 1,
                     pos.x,
                     pos.y,
                     pos.z,
                     targetZ,
                     physical->m_vecMoveSpeed.x,
                     physical->m_vecMoveSpeed.y,
                     physical->m_vecMoveSpeed.z,
                     hit ? 1 : 0,
                     entity,
                     wasUsingCollision ? 1 : 0);
            }
        }
        return false;
    }

    CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(entity->m_nModelIndex);
    const bool targetedSurface = IsTargetedHighSurfaceModel64(modelInfo);
    const bool criticalSurface = IsCriticalNearbySurfaceModel64(modelInfo);
    if (!targetedSurface && !criticalSurface) {
        return false;
    }

    const float surfaceDelta = pos.z - col.m_vecPoint.z;
    const float maxSurfaceDelta = vehicle ? 12.25f : 10.75f;
    const float ceilingRejectDelta = vehicle ? -1.25f : -0.35f;
    if (surfaceDelta < ceilingRejectDelta) {
        static uint32_t s_lastAboveSurfaceSkipLogTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastAboveSurfaceSkipLogTick >= 700) {
            s_lastAboveSurfaceSkipLogTick = now;
            char modelName[22]{};
            CopyModelName64(modelName, modelInfo);
            FLog("[COL_PHYS64] skip-above mode=%s phys=%d pos=%.2f %.2f %.2f surf=%.2f dz=%.2f model=%d,%s vel=%.3f %.3f %.3f",
                 mode ? mode : "?",
                 vehicle ? 2 : 1,
                 pos.x,
                 pos.y,
                 pos.z,
                 col.m_vecPoint.z,
                 surfaceDelta,
                 entity->m_nModelIndex,
                 modelName[0] ? modelName : "?",
                 physical->m_vecMoveSpeed.x,
                 physical->m_vecMoveSpeed.y,
                 physical->m_vecMoveSpeed.z);
        }
        return false;
    }
    const float minSurfaceDelta = ceilingRejectDelta;
    if (surfaceDelta < minSurfaceDelta ||
        surfaceDelta > maxSurfaceDelta ||
        col.m_vecNormal.z < 0.35f) {
        if (!vehicle && surfaceDelta > maxSurfaceDelta) {
            static uint32_t s_lastPedAirSkipLogTick = 0;
            const uint32_t now = GetTickCount();
            if (now - s_lastPedAirSkipLogTick >= 700) {
                s_lastPedAirSkipLogTick = now;
                char modelName[22]{};
                CopyModelName64(modelName, modelInfo);
                FLog("[COL_PHYS64] skip ped-air mode=%s pos=%.2f %.2f %.2f surf=%.2f dz=%.2f max=%.2f model=%d,%s vel=%.3f %.3f %.3f",
                     mode ? mode : "?",
                     pos.x,
                     pos.y,
                     pos.z,
                     col.m_vecPoint.z,
                     surfaceDelta,
                     maxSurfaceDelta,
                     entity->m_nModelIndex,
                     modelName[0] ? modelName : "?",
                     physical->m_vecMoveSpeed.x,
                     physical->m_vecMoveSpeed.y,
                     physical->m_vecMoveSpeed.z);
            }
        }
        return false;
    }

    if (vehicle) {
        const bool penetratingLoadedSurface = surfaceDelta < -0.35f;
        const bool fallingFast = physical->m_vecMoveSpeed.z < -0.18f;
        const bool fallingTowardSurface =
            surfaceDelta > 3.75f &&
            physical->m_vecMoveSpeed.z < -0.08f;
        if (!penetratingLoadedSurface && !fallingFast && !fallingTowardSurface) {
            static uint32_t s_lastVehicleSkipLogTick = 0;
            const uint32_t now = GetTickCount();
            if (now - s_lastVehicleSkipLogTick >= 1200) {
                s_lastVehicleSkipLogTick = now;
                char modelName[22]{};
                CopyModelName64(modelName, modelInfo);
                FLog("[COL_PHYS64] skip vehicle-rest mode=%s pos=%.2f %.2f %.2f surf=%.2f dz=%.2f model=%d,%s vel=%.3f %.3f %.3f",
                     mode ? mode : "?",
                     pos.x,
                     pos.y,
                     pos.z,
                     col.m_vecPoint.z,
                     surfaceDelta,
                     entity->m_nModelIndex,
                     modelName[0] ? modelName : "?",
                     physical->m_vecMoveSpeed.x,
                     physical->m_vecMoveSpeed.y,
                     physical->m_vecMoveSpeed.z);
            }
            return false;
        }
    }
    else {
        const bool penetratingLoadedSurface = surfaceDelta < -0.05f;
        const bool lowGroundGap =
            surfaceDelta >= -0.05f &&
            surfaceDelta <= 1.80f;
        const bool fallingOntoSurface =
            physical->m_vecMoveSpeed.z < -0.16f &&
            surfaceDelta > 1.80f &&
            surfaceDelta <= 6.0f;
        const bool recoveringDeepGap =
            physical->m_vecMoveSpeed.z < -0.08f &&
            surfaceDelta > 2.75f &&
            surfaceDelta <= maxSurfaceDelta;
        const bool ascendingOrJumping = physical->m_vecMoveSpeed.z > 0.015f;
        if (ascendingOrJumping ||
            (lowGroundGap && !penetratingLoadedSurface) ||
            (!penetratingLoadedSurface && !fallingOntoSurface && !recoveringDeepGap)) {
            static uint32_t s_lastPedRestSkipLogTick = 0;
            const uint32_t now = GetTickCount();
            if (now - s_lastPedRestSkipLogTick >= 900) {
                s_lastPedRestSkipLogTick = now;
                char modelName[22]{};
                CopyModelName64(modelName, modelInfo);
                FLog("[COL_PHYS64] skip ped-rest/jump mode=%s pos=%.2f %.2f %.2f surf=%.2f dz=%.2f model=%d,%s vel=%.3f %.3f %.3f asc=%d low=%d",
                     mode ? mode : "?",
                     pos.x,
                     pos.y,
                     pos.z,
                     col.m_vecPoint.z,
                     surfaceDelta,
                     entity->m_nModelIndex,
                     modelName[0] ? modelName : "?",
                     physical->m_vecMoveSpeed.x,
                     physical->m_vecMoveSpeed.y,
                     physical->m_vecMoveSpeed.z,
                     ascendingOrJumping ? 1 : 0,
                     lowGroundGap ? 1 : 0);
            }
            return false;
        }
    }

    if (surfaceDelta < 0.0f) {
        const float penetrationDepth = ClampFloat64(-surfaceDelta + 0.18f, 0.20f, vehicle ? 8.75f : 5.00f);
        if (!std::isfinite(col.m_fDepth) || col.m_fDepth < penetrationDepth) {
            col.m_fDepth = penetrationDepth;
        }
    }

    float damageIntensity = -1.0f;
    const bool applied = GTASAEngineApi::ApplyPhysicalCollision(physical, entity, col, damageIntensity);
    if (!applied) {
        static uint32_t s_lastApplyFailLogTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastApplyFailLogTick >= 700) {
            s_lastApplyFailLogTick = now;
            char modelName[22]{};
            CopyModelName64(modelName, modelInfo);
            FLog("[COL_PHYS64] apply-fail mode=%s phys=%d pos=%.2f %.2f %.2f surf=%.2f dz=%.2f model=%d,%s normal=%.2f %.2f %.2f vel=%.3f %.3f %.3f",
                 mode ? mode : "?",
                 vehicle ? 2 : 1,
                 pos.x,
                 pos.y,
                 pos.z,
                 col.m_vecPoint.z,
                 surfaceDelta,
                 entity->m_nModelIndex,
                 modelName[0] ? modelName : "?",
                 col.m_vecNormal.x,
                 col.m_vecNormal.y,
                 col.m_vecNormal.z,
                 physical->m_vecMoveSpeed.x,
                 physical->m_vecMoveSpeed.y,
                 physical->m_vecMoveSpeed.z);
        }
        return false;
    }

    physical->m_bHasContacted = true;
    physical->m_bHasHitWall = true;
    physical->m_bIsInSafePosition = true;
    physical->m_nContactSurface = col.m_nSurfaceTypeB;
    if (col.m_vecNormal.z > 0.55f) {
        physical->physicalFlags.bOnSolidSurface = true;
        GTASAEngineApi::ApplyPhysicalFriction(physical, vehicle ? 0.85f : 1.00f, col);
        if (physical->IsPed()) {
            CPedGTA* ped = reinterpret_cast<CPedGTA*>(physical);
            ped->bIsStanding = true;
            ped->bWasStanding = true;
            ped->bIsInTheAir = false;
            ped->bIsLanding = false;
        }
    }

    static uint32_t s_lastPhysCollisionLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastPhysCollisionLogTick >= 300) {
        s_lastPhysCollisionLogTick = now;
        char modelName[22]{};
        CopyModelName64(modelName, modelInfo);
        FLog("[COL_PHYS64] applied mode=%s phys=%d pos=%.2f %.2f %.2f surf=%.2f dz=%.2f model=%d,%s normal=%.2f %.2f %.2f damage=%.3f vel=%.3f %.3f %.3f",
             mode ? mode : "?",
             vehicle ? 2 : 1,
             pos.x,
             pos.y,
             pos.z,
             col.m_vecPoint.z,
             surfaceDelta,
             entity->m_nModelIndex,
             modelName[0] ? modelName : "?",
             col.m_vecNormal.x,
             col.m_vecNormal.y,
             col.m_vecNormal.z,
             damageIntensity,
             physical->m_vecMoveSpeed.x,
             physical->m_vecMoveSpeed.y,
             physical->m_vecMoveSpeed.z);
    }

    return true;
}

bool CPhysical__CheckCollision_hook(CPhysical* physical)
{
    const bool nativeHit = CPhysical__CheckCollisionNative
        ? CPhysical__CheckCollisionNative(physical)
        : false;

    CPlayerPed* localPed = GetConnectedLocalPlayerPed64();
    if (localPed && IsLocalPedOrVehiclePhysical64(localPed, physical)) {
        if (StabilizeLocalPedWaterJump64(localPed, physical)) {
            return false;
        }
        if (IsLocalPedJumpInputActive64(localPed, physical)) {
            RelaxLocalPedJumpBlockers64(localPed, physical);
            if (physical->m_vecMoveSpeed.z > -0.045f) {
                return false;
            }
        }
    }
    if (nativeHit) {
        return true;
    }

    if (physical && physical->IsPed()) {
        const float speedSq =
            physical->m_vecMoveSpeed.x * physical->m_vecMoveSpeed.x +
            physical->m_vecMoveSpeed.y * physical->m_vecMoveSpeed.y +
            physical->m_vecMoveSpeed.z * physical->m_vecMoveSpeed.z;
        const bool restingOrNormalStep =
            (physical->physicalFlags.bOnSolidSurface &&
             physical->m_vecMoveSpeed.z > -0.08f &&
             physical->m_vecMoveSpeed.z < 0.08f) ||
            (speedSq < 0.010f && std::fabs(physical->m_vecMoveSpeed.z) < 0.040f);
        if (restingOrNormalStep) {
            return false;
        }
    }

    const bool supplementalHit = TryApplyArm64SupplementalPhysicalCollision64(physical, "phys-check");
    return supplementalHit;
}
#endif

int(*CUpsideDownCarCheck__IsCarUpsideDown)(int, int);
int CUpsideDownCarCheck__IsCarUpsideDown_hook(int a1, int a2)
{
    /* Passengers leave the vehicle out of fear if it overturns */

//	if (*(uintptr_t*)(a2 + 20))
//	{
//		return CUpsideDownCarCheck__IsCarUpsideDown(a1, a2);
//	}
    return 0;
}

int (*CTaskSimpleGetUp__ProcessPed)(uintptr_t* thiz, CPedGTA* ped);
int CTaskSimpleGetUp__ProcessPed_hook(uintptr_t* thiz, CPedGTA* ped)
{
    //return false;
    if(!ped)return 0;
    int res = 0;
    try {
        res = CTaskSimpleGetUp__ProcessPed(thiz, ped);
    }
    catch(...) {
        return 0;
    }

    return res;
}

int64 getmip()
{
    return 1;
}

uint64_t* RQCommand_rqSetAlphaTest(uint64_t *result)
{
    *result += 8;
    return result;
}

int64 GetInputType(void)
{
    return 0LL;
}

int(*CAnimBlendNode__FindKeyFrame)(int, float, int, int);
int CAnimBlendNode__FindKeyFrame_hook(int a1, float a2, int a3, int a4)
{
    if (*(uintptr_t*)(a1 + 16))
    {
        return CAnimBlendNode__FindKeyFrame(a1, a2, a3, a4);
    }
    else return 0;
}

RwFrame* CClumpModelInfo_GetFrameFromId_Post(RwFrame* pFrameResult, RpClump* pClump, int id)
{
    if (pFrameResult)
        return pFrameResult;

    uintptr_t calledFrom = 0;
    __asm__ volatile ("mov %0, lr" : "=r" (calledFrom));
    calledFrom -= g_libGTASA;

    if (calledFrom == 0x00515708                // CVehicle::SetWindowOpenFlag
            || calledFrom == 0x00515730             // CVehicle::ClearWindowOpenFlag
            || calledFrom == 0x00338698             // CVehicleModelInfo::GetOriginalCompPosition
            || calledFrom == 0x00338B2C)            // CVehicleModelInfo::CreateInstance
        return nullptr;

    for (uint i = 2; i < 40; i++)
    {
        RwFrame* pNewFrameResult = nullptr;
        uint     uiNewId = id + (i / 2) * ((i & 1) ? -1 : 1);

        pNewFrameResult = GTASAEngineApi::GetFrameFromId(pClump, i);

        if (pNewFrameResult)
        {
            return pNewFrameResult;
        }
    }

    return nullptr;
}
RwFrame* (*CClumpModelInfo_GetFrameFromId)(RpClump*, int);
RwFrame* CClumpModelInfo_GetFrameFromId_hook(RpClump* a1, int a2)
{
    return CClumpModelInfo_GetFrameFromId_Post(CClumpModelInfo_GetFrameFromId(a1, a2), a1, a2);
}

void (*FxEmitterBP_c__Render)(uintptr_t* a1, int a2, int a3, float a4, char a5);
void FxEmitterBP_c__Render_hook(uintptr_t* a1, int a2, int a3, float a4, char a5)
{
    if(!a1 || !a2) return;
    uintptr_t* temp = *((uintptr_t**)a1 + 3);
    if (!temp)
    {
        return;
    }
    FxEmitterBP_c__Render(a1, a2, a3, a4, a5);
}

bool (*RwResourcesFreeResEntry)(void* entry);
bool RwResourcesFreeResEntry_hook(void* entry)
{
    bool result;
    if (entry) result = RwResourcesFreeResEntry(entry);
    else result = false;
    return result;
}

static uint32_t dwRLEDecompressSourceSize = 0;
static const uint8_t* gRLEDecompressSourceBase = nullptr;
static uint32_t gRLEDecompressSourceCapacity = 0;

int (*OS_FileRead)(OSFile a1, void *buffer, int numBytes);
int OS_FileRead_hook(OSFile a1, void *buffer, int numBytes)
{
    if (!a1 || !buffer || numBytes <= 0) {
        dwRLEDecompressSourceSize = 0;
        gRLEDecompressSourceBase = nullptr;
        gRLEDecompressSourceCapacity = 0;
        static uint32_t s_invalidFileReadLogCount = 0;
        if (s_invalidFileReadLogCount < 32) {
            ++s_invalidFileReadLogCount;
            FLog("OS_FileRead invalid read rejected: file=%p buffer=%p bytes=%d", a1, buffer, numBytes);
        }
        return 1;
    }

    const uint32_t requestedBytes = static_cast<uint32_t>(numBytes);
    dwRLEDecompressSourceSize = requestedBytes;
    gRLEDecompressSourceBase = static_cast<const uint8_t*>(buffer);
    gRLEDecompressSourceCapacity = requestedBytes;

    const int result = OS_FileRead(a1, buffer, numBytes);
    if (result != 0) {
        static uint32_t s_fileReadErrorLogCount = 0;
        if (s_fileReadErrorLogCount < 16) {
            ++s_fileReadErrorLogCount;
            FLog("[RLE_FIX64] OS_FileRead result=%d requested=%d buffer=%p", result, numBytes, buffer);
        }
    }
    return result;
}

void (*RLEDecompress)(uint8_t* pDest, size_t uiDestSize, uint8_t const* pSrc, size_t uiSegSize, uint32_t uiEscape);
void RLEDecompress_hook(uint8_t* pDest, size_t uiDestSize, const uint8_t* pSrc, size_t uiSegSize, uint32_t uiEscape) {
    if (!pDest || !pSrc || uiDestSize == 0 || uiSegSize == 0) {
        dwRLEDecompressSourceSize = 0;
        return;
    }

    uint32_t sourceSize = 0;
    if (gRLEDecompressSourceBase) {
        const uintptr_t srcAddr = reinterpret_cast<uintptr_t>(pSrc);
        const uintptr_t baseAddr = reinterpret_cast<uintptr_t>(gRLEDecompressSourceBase);
        if (srcAddr >= baseAddr) {
            const uintptr_t offset = srcAddr - baseAddr;
            if (offset < gRLEDecompressSourceCapacity) {
                sourceSize = gRLEDecompressSourceCapacity - static_cast<uint32_t>(offset);
            }
        }
    }
    if (sourceSize == 0) {
        sourceSize = dwRLEDecompressSourceSize;
    }
    dwRLEDecompressSourceSize = 0;
    if (sourceSize == 0) {
        static uint32_t s_rleMissingLogCount = 0;
        if (s_rleMissingLogCount < 32) {
            ++s_rleMissingLogCount;
            FLog("[RLE_FIX64] missing source size dest=%zu seg=%zu escape=%u src=%p base=%p cap=%u",
                 uiDestSize,
                 uiSegSize,
                 uiEscape,
                 pSrc,
                 gRLEDecompressSourceBase,
                 gRLEDecompressSourceCapacity);
        }
        return;
    }

    const uint8_t* src = pSrc;
    uint8_t* dst = pDest;
    const uint8_t* const endSrc = pSrc + sourceSize;
    uint8_t* const endDst = pDest + uiDestSize;
    const uint8_t escape = static_cast<uint8_t>(uiEscape);
    bool truncated = false;

    while (dst < endDst && src < endSrc) {
        if (*src == escape) {
            if (src + 2 + uiSegSize > endSrc) {
                truncated = true;
                break;
            }

            uint8_t repeats = src[1];
            const uint8_t* segment = src + 2;
            while (repeats--) {
                const size_t destRemaining = static_cast<size_t>(endDst - dst);
                if (destRemaining == 0) {
                    break;
                }
                const size_t copySize = destRemaining < uiSegSize ? destRemaining : uiSegSize;
                memcpy(dst, segment, copySize);
                dst += copySize;
            }
            if (truncated) {
                break;
            }
            src += 2 + uiSegSize;
            continue;
        }

        const size_t destRemaining = static_cast<size_t>(endDst - dst);
        const size_t copySize = destRemaining < uiSegSize ? destRemaining : uiSegSize;
        if (src + copySize > endSrc) {
            truncated = true;
            break;
        }
        memcpy(dst, src, copySize);
        dst += copySize;
        src += uiSegSize;
    }

    static uint32_t s_rleTruncateLogCount = 0;
    if (truncated && s_rleTruncateLogCount < 16) {
        ++s_rleTruncateLogCount;
        FLog("[RLE_FIX64] truncated src=%u dest=%zu seg=%zu wrote=%zu read=%zu escape=%u",
             sourceSize,
             uiDestSize,
             uiSegSize,
             static_cast<size_t>(dst - pDest),
             static_cast<size_t>(src - pSrc),
             uiEscape);
    }
    return;

    if (!pDest || !pSrc || uiDestSize == 0 || uiSegSize == 0) {
        // Обработка некорректных входных данных или размеров
        // Здесь можно сгенерировать исключение или вернуть код ошибки
        return;
    }

    const uint8_t* pTempSrc = pSrc;
    const uint8_t* const pEndOfDest = pDest + uiDestSize;
    const uint8_t* const pEndOfSrc = pSrc + dwRLEDecompressSourceSize; // Предполагается, что dwRLEDecompressSourceSize определено правильно

    try {
        while (pDest < pEndOfDest && pTempSrc < pEndOfSrc) {
            if (*pTempSrc == uiEscape) {
                if (pTempSrc + 1 >= pEndOfSrc || pTempSrc[1] == 0 || pTempSrc + 2 + uiSegSize > pEndOfSrc) {
                    // Обработка ошибки, неверное значение ucCurSeg или недостаточно данных в исходном буфере
                    throw std::runtime_error("rled error 1");
                }

                uint8_t ucCurSeg = pTempSrc[1];
                while (ucCurSeg--) {
                    if (pDest + uiSegSize > pEndOfDest) {
                        // Обработка ошибки, недостаточно места в целевом буфере
                        throw std::runtime_error("rled error 2");
                    }
                    memcpy(pDest, pTempSrc + 2, uiSegSize);
                    pDest += uiSegSize;
                }
                pTempSrc += 2 + uiSegSize;
            } else {
                if (pDest + uiSegSize > pEndOfDest || pTempSrc + uiSegSize > pEndOfSrc) {
                    // Обработка ошибки, недостаточно данных в исходном буфере или недостаточно места в целевом буфере
                    throw std::runtime_error("rled error 3");
                }
                memcpy(pDest, pTempSrc, uiSegSize);
                pDest += uiSegSize;
                pTempSrc += uiSegSize;
            }
        }

        dwRLEDecompressSourceSize = 0;
    } catch (const std::exception& e) {
        FLog("%s", e.what());
    }
}

void (*CGame_Process)();
static void MaintainWorldObjectStreaming64();
static void MaintainCollisionResidency64();
static void LogStaticWorldPoolsAroundPlayer64(const CVector& pos);
#if !VER_x32
static void RequestMissingNearbyWorldCollision64(const CVector& pos, uint8_t playerArea, uint32_t now, bool immediate);
static void SuppressNearbyTempVehicleComponents64(const CVector& pos);
#endif

#if !VER_x32
static constexpr uint32_t kArm64StreamingMemoryBudget = 384u * 1024u * 1024u;
static constexpr uint32_t kArm64StreamingMemoryHardBudget = 448u * 1024u * 1024u;

static void EnsureArm64StreamingMemoryBudget(const char* reason)
{
    static bool s_loggedOnce = false;
    static uint32_t s_lastLogTick = 0;

    const Supremo::Streaming::StreamingMemoryBudgetResult budget =
        Supremo::Streaming::ApplyMemoryBudget(
            kArm64StreamingMemoryBudget,
            kArm64StreamingMemoryHardBudget);

    const uint32_t now = GetTickCount();
    if (budget.changed || !s_loggedOnce || now - s_lastLogTick >= 10000) {
        s_loggedOnce = true;
        s_lastLogTick = now;
        FLog("[STREAM_MEM64] reason=%s wrapper=%zu->%zu native=%u->%u used=%zu",
             reason ? reason : "?",
             budget.wrapperBefore,
             budget.wrapperAfter,
             budget.nativeBefore,
             budget.nativeAfter,
             budget.memoryUsed);
    }
}
#endif

static void ForceSpawnCollisionDebugHook()
{
    MaintainCollisionResidency64();
}

struct WorldPoolDiag64 {
    int used = 0;
    int near = 0;
    int noModelInfo = 0;
    int notLoaded = 0;
    int noEntityRw = 0;
    int noModelRw = 0;
    int invisible = 0;
    int noCollision = 0;
};

struct WorldEntitySample64 {
    const char* poolName = nullptr;
    int slot = -1;
    int modelId = -1;
    float dist2 = 0.0f;
    CVector pos{};
    bool loaded = false;
    bool entityRw = false;
    bool modelRw = false;
    bool visible = false;
    bool hasCollision = false;
    bool entityCollision = false;
    int refCount = 0;
    int txdIndex = -1;
    uint8_t area = 0;
    uint32_t flags = 0;
    char modelName[22]{};
};

static bool IsLikelyRoadModel64(CBaseModelInfo* modelInfo);
static bool IsLikelyGroundSurfaceModel64(CBaseModelInfo* modelInfo);

static bool IsWatchedWorldModel64(int modelId)
{
    switch (modelId) {
        case 1656: // Esc_step
        case 1698: // Esc_step8
        case 3438: // ballyring01_lvs
        case 8393: // ballys01_lvs
        case 8485: // ballysbase_lvs
        case 8697: // lodlys01_lvs
        case 8622: // VegasEroad131
        case 8979: // vgsEesc02
        case 8980: // vgsEesc01
        case 4809: // LAroads_05_LAs
        case 4810: // hillpalos04_LAs
        case 4825: // griffithoblas
        case 4828: // lasairprt5
        case 4833: // airpurtder_las
        case 4834: // airoad1d_LAS
        case 4835: // airoad1b_LAS
        case 4838: // airpurtderfr_las
            return true;
        default:
            return false;
    }
}

static bool IsCoreBallysCollisionModel64(int modelId)
{
    switch (modelId) {
        case 3438: // ballyring01_lvs
        case 8393: // ballys01_lvs
        case 8485: // ballysbase_lvs
        case 8622: // VegasEroad131
        case 8979: // vgsEesc02
        case 8980: // vgsEesc01
            return true;
        default:
            return false;
    }
}

static bool IsBallysRequiredColDataModel64(int modelId)
{
    switch (modelId) {
        case 8393: // ballys01_lvs
        case 8485: // ballysbase_lvs
        case 8622: // VegasEroad131
            return true;
        default:
            return false;
    }
}

static void CopyModelName64(char (&dst)[22], CBaseModelInfo* modelInfo)
{
    dst[0] = '\0';
    if (!modelInfo) {
        return;
    }

    memcpy(dst, modelInfo->m_modelName, 21);
    dst[21] = '\0';
}

static const char* CurrentAbiLabel()
{
    return VER_x32 ? "32" : "64";
}

static float DistanceSquared3D(const CVector& a, const CVector& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

#if !VER_x32
template <typename TPool>
static int SuppressNearbyTempVehicleComponentsPool64(TPool* pool, const CVector& playerPos, float radius)
{
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0) {
        return 0;
    }

    const float radius2 = radius * radius;
    int suppressed = 0;
    for (int32 i = 0; i < pool->m_nSize; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity) {
            continue;
        }

        const int modelId = entity->m_nModelIndex;
        CBaseModelInfo* modelInfo =
            (modelId >= 0 && modelId < CModelInfo::NUM_MODEL_INFOS)
                ? CModelInfo::GetModelInfo(modelId)
                : nullptr;
        const bool tempVehiclePart = IsTempVehicleComponentModel64(modelId);
        const bool unstableProp = IsUnstableBreakableWorldProp64(modelId, modelInfo);
        if (!tempVehiclePart && !unstableProp) {
            continue;
        }

        if (DistanceSquared3D(entity->GetPosition(), playerPos) > radius2) {
            continue;
        }

        const bool wasCollidable = entity->m_bUsesCollision;
        const bool wasVisible = entity->m_bIsVisible;
        if (tempVehiclePart) {
            SuppressTempVehicleComponentCollision64(entity);
        } else {
            PreserveBreakableWorldPropCollision64(entity);
        }
        if (wasCollidable || wasVisible) {
            ++suppressed;
        }
    }

    return suppressed;
}

static void SuppressNearbyTempVehicleComponents64(const CVector& pos)
{
    static uint32_t s_lastSuppressTick = 0;
    static uint32_t s_lastSuppressLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastSuppressTick < 250) {
        return;
    }
    s_lastSuppressTick = now;

    int suppressed = 0;
    suppressed += SuppressNearbyTempVehicleComponentsPool64(GetBuildingPool(), pos, 260.0f);
    suppressed += SuppressNearbyTempVehicleComponentsPool64(GetObjectPoolGta(), pos, 220.0f);
    suppressed += SuppressNearbyTempVehicleComponentsPool64(GetDummyPool(), pos, 220.0f);
    if (suppressed > 0 && now - s_lastSuppressLogTick >= 1000) {
        s_lastSuppressLogTick = now;
        FLog("[TEMP_VEH_PART64] suppressed=%d pos=%.2f %.2f %.2f", suppressed, pos.x, pos.y, pos.z);
    }
}
#endif

static void LogEntityLayoutOnceABI(CEntityGTA* entity)
{
    static bool s_logged = false;
    if (s_logged || !entity) {
        return;
    }
    s_logged = true;

    const uintptr_t base = reinterpret_cast<uintptr_t>(entity);
    FLog("[ENTITY_LAYOUT%s] sizeof=%zu place=%zu matrix=%zu rw=%zu flags=%zu model=%zu refs=%zu stream=%zu scan=%zu ipl=%zu area=%zu lod=%zu",
         CurrentAbiLabel(),
         sizeof(CEntityGTA),
         static_cast<size_t>(reinterpret_cast<uintptr_t>(&entity->m_placement) - base),
         static_cast<size_t>(reinterpret_cast<uintptr_t>(&entity->m_matrix) - base),
         static_cast<size_t>(reinterpret_cast<uintptr_t>(&entity->m_pRwObject) - base),
         static_cast<size_t>(reinterpret_cast<uintptr_t>(&entity->m_nFlags) - base),
         static_cast<size_t>(reinterpret_cast<uintptr_t>(&entity->m_nModelIndex) - base),
         static_cast<size_t>(reinterpret_cast<uintptr_t>(&entity->m_pReferences) - base),
         static_cast<size_t>(reinterpret_cast<uintptr_t>(&entity->m_pStreamingLink) - base),
         static_cast<size_t>(reinterpret_cast<uintptr_t>(&entity->m_nScanCode) - base),
         static_cast<size_t>(reinterpret_cast<uintptr_t>(&entity->m_nIplIndex) - base),
         static_cast<size_t>(reinterpret_cast<uintptr_t>(&entity->m_nAreaCode) - base),
         static_cast<size_t>(reinterpret_cast<uintptr_t>(&entity->m_nLodIndex) - base));
}

static void LogWatchedWorldEntityMatrixABI(const char* poolName, int slot, CEntityGTA* entity, CBaseModelInfo* modelInfo, float dist)
{
    if (!entity) {
        return;
    }

    LogEntityLayoutOnceABI(entity);

    char modelName[22]{};
    CopyModelName64(modelName, modelInfo);

    const CVector entityPos = entity->GetPosition();
    const CVector placePos = entity->m_placement.m_vPosn;
    const float heading = entity->m_placement.m_fHeading;
    const bool hasMatrix = entity->m_matrix != nullptr;
    const CVector matRight = hasMatrix ? entity->m_matrix->GetRight() : CVector(0.0f, 0.0f, 0.0f);
    const CVector matForward = hasMatrix ? entity->m_matrix->GetForward() : CVector(0.0f, 0.0f, 0.0f);
    const CVector matUp = hasMatrix ? entity->m_matrix->GetUp() : CVector(0.0f, 0.0f, 0.0f);
    const CVector matPos = hasMatrix ? entity->m_matrix->GetPosition() : placePos;

    RwMatrix* rwMatrix = entity->GetModellingMatrix();
    const bool hasRwMatrix = rwMatrix != nullptr;
    const CVector rwRight = hasRwMatrix ? CVector(rwMatrix->right.x, rwMatrix->right.y, rwMatrix->right.z) : CVector(0.0f, 0.0f, 0.0f);
    const CVector rwUp = hasRwMatrix ? CVector(rwMatrix->up.x, rwMatrix->up.y, rwMatrix->up.z) : CVector(0.0f, 0.0f, 0.0f);
    const CVector rwAt = hasRwMatrix ? CVector(rwMatrix->at.x, rwMatrix->at.y, rwMatrix->at.z) : CVector(0.0f, 0.0f, 0.0f);
    const CVector rwPos = hasRwMatrix ? CVector(rwMatrix->pos.x, rwMatrix->pos.y, rwMatrix->pos.z) : CVector(0.0f, 0.0f, 0.0f);
    const float rwPosDelta = hasRwMatrix ? sqrtf(DistanceSquared3D(entityPos, rwPos)) : -1.0f;

    FLog("[WORLD_MATRIX%s] pool=%s slot=%d model=%d name=%s dist=%.2f area=%u type=%d vis=%d rw=%d mat=%d pos=%.3f %.3f %.3f place=%.3f %.3f %.3f head=%.5f matPos=%.3f %.3f %.3f rwPos=%.3f %.3f %.3f rwDelta=%.4f flags=0x%08X",
         CurrentAbiLabel(),
         poolName ? poolName : "?",
         slot,
         entity->m_nModelIndex,
         modelName[0] ? modelName : "?",
         dist,
         static_cast<unsigned>(entity->m_nAreaCode),
         static_cast<int>(entity->GetType()),
         entity->m_bIsVisible ? 1 : 0,
         entity->m_pRwObject ? 1 : 0,
         hasMatrix ? 1 : 0,
         entityPos.x, entityPos.y, entityPos.z,
         placePos.x, placePos.y, placePos.z,
         heading,
         matPos.x, matPos.y, matPos.z,
         rwPos.x, rwPos.y, rwPos.z,
         rwPosDelta,
         entity->m_nFlags);
    FLog("[WORLD_AXIS%s] pool=%s slot=%d model=%d name=%s mat=%d rw=%d matR=%.3f %.3f %.3f matF=%.3f %.3f %.3f matU=%.3f %.3f %.3f rwR=%.3f %.3f %.3f rwUp=%.3f %.3f %.3f rwAt=%.3f %.3f %.3f",
         CurrentAbiLabel(),
         poolName ? poolName : "?",
         slot,
         entity->m_nModelIndex,
         modelName[0] ? modelName : "?",
         hasMatrix ? 1 : 0,
         entity->m_pRwObject ? 1 : 0,
         matRight.x, matRight.y, matRight.z,
         matForward.x, matForward.y, matForward.z,
         matUp.x, matUp.y, matUp.z,
         rwRight.x, rwRight.y, rwRight.z,
         rwUp.x, rwUp.y, rwUp.z,
         rwAt.x, rwAt.y, rwAt.z);
}

template <typename TPool>
static void LogWatchedWorldMatricesPoolABI(
    const char* poolName,
    TPool* pool,
    const CVector& playerPos,
    float radius,
    int& logged)
{
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0 || logged >= 14) {
        return;
    }

    const float radius2 = radius * radius;
    for (int32 i = 0; i < pool->m_nSize && logged < 14; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity || !IsWatchedWorldModel64(entity->m_nModelIndex)) {
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        const float dist2 = DistanceSquared3D(entityPos, playerPos);
        if (dist2 > radius2) {
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(entity->m_nModelIndex);
        LogWatchedWorldEntityMatrixABI(poolName, i, entity, modelInfo, sqrtf(dist2));
        ++logged;
    }
}

static void LogWatchedWorldMatricesAroundPlayerABI(const CVector& pos)
{
    static uint32_t s_lastMatrixLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastMatrixLogTick < 4500) {
        return;
    }
    s_lastMatrixLogTick = now;

    int logged = 0;
    LogWatchedWorldMatricesPoolABI("B", GetBuildingPool(), pos, 230.0f, logged);
    LogWatchedWorldMatricesPoolABI("O", GetObjectPoolGta(), pos, 180.0f, logged);
    LogWatchedWorldMatricesPoolABI("D", GetDummyPool(), pos, 180.0f, logged);
}

static void StoreWorldSample64(WorldEntitySample64 samples[], int& sampleCount, const WorldEntitySample64& sample)
{
    constexpr int kMaxSamples = 8;
    if (sampleCount < kMaxSamples) {
        samples[sampleCount++] = sample;
        return;
    }

    int farthest = 0;
    for (int i = 1; i < kMaxSamples; ++i) {
        if (samples[i].dist2 > samples[farthest].dist2) {
            farthest = i;
        }
    }

    if (sample.dist2 < samples[farthest].dist2) {
        samples[farthest] = sample;
    }
}

static bool IsLodWorldModel64(CBaseModelInfo* modelInfo);

template <typename TPool>
static void LogWatchedWorldEntities64(const char* poolName, TPool* pool, const CVector& playerPos, float radius)
{
#if !VER_x32
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0) {
        return;
    }

    const float radius2 = radius * radius;
    int logged = 0;
    for (int32 i = 0; i < pool->m_nSize && logged < 18; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity || !IsWatchedWorldModel64(entity->m_nModelIndex)) {
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        const float dx = entityPos.x - playerPos.x;
        const float dy = entityPos.y - playerPos.y;
        const float dz = entityPos.z - playerPos.z;
        const float dist2 = dx * dx + dy * dy + dz * dz;
        if (dist2 > radius2) {
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(entity->m_nModelIndex);
        char modelName[22]{};
        CopyModelName64(modelName, modelInfo);

        CColModel* colModel = modelInfo ? modelInfo->m_pColModel : nullptr;
        CCollisionData* colData = colModel ? colModel->m_pColData : nullptr;
        const CVector colMin = colModel ? colModel->m_boundBox.m_vecMin : CVector(0.0f, 0.0f, 0.0f);
        const CVector colMax = colModel ? colModel->m_boundBox.m_vecMax : CVector(0.0f, 0.0f, 0.0f);
        const CVector worldMin(entityPos.x + colMin.x, entityPos.y + colMin.y, entityPos.z + colMin.z);
        const CVector worldMax(entityPos.x + colMax.x, entityPos.y + colMax.y, entityPos.z + colMax.z);

        FLog("[WORLD_WATCH64] pool=%s slot=%d model=%d name=%s txd=%d ref=%d dist=%.2f pos=%.2f %.2f %.2f area=%u type=%d road=%d lod=%d rw=%d mrw=%d loaded=%d vis=%d flags=0x%08X",
             poolName ? poolName : "?",
             i,
             entity->m_nModelIndex,
             modelName[0] ? modelName : "?",
             modelInfo ? modelInfo->m_nTxdIndex : -1,
             modelInfo ? modelInfo->m_nRefCount : 0,
             sqrtf(dist2),
             entityPos.x, entityPos.y, entityPos.z,
              static_cast<unsigned>(entity->m_nAreaCode),
              static_cast<int>(entity->GetType()),
              IsLikelyRoadModel64(modelInfo) ? 1 : 0,
              IsLodWorldModel64(modelInfo) ? 1 : 0,
              entity->m_pRwObject ? 1 : 0,
              modelInfo && modelInfo->m_pRwObject ? 1 : 0,
              Supremo::Streaming::IsModelLoaded(entity->m_nModelIndex) ? 1 : 0,
              entity->m_bIsVisible ? 1 : 0,
              entity->m_nFlags);
        FLog("[WORLD_BBOX64] pool=%s slot=%d model=%d name=%s col=%d colSlot=%u colFlags=0x%02X colVol=%d colActive=%d data=%d ownCol=%d road=%d miFlags=0x%04X sph=%u box=%u line=%u tri=%u shTri=%d local={%.2f %.2f %.2f -> %.2f %.2f %.2f} world={%.2f %.2f %.2f -> %.2f %.2f %.2f}",
             poolName ? poolName : "?",
             i,
             entity->m_nModelIndex,
             modelName[0] ? modelName : "?",
              colModel ? 1 : 0,
              colModel ? static_cast<unsigned>(colModel->m_nColSlot) : 0,
              colModel ? static_cast<unsigned>(colModel->m_nFlags) : 0,
              colModel && colModel->m_bHasCollisionVolumes ? 1 : 0,
              colModel && colModel->m_bIsActive ? 1 : 0,
              colData ? 1 : 0,
              modelInfo && modelInfo->bDoWeOwnTheColModel ? 1 : 0,
              modelInfo && modelInfo->bIsRoad ? 1 : 0,
              modelInfo ? static_cast<unsigned>(modelInfo->m_nFlags) : 0,
              colData ? static_cast<unsigned>(colData->m_nNumSpheres) : 0,
              colData ? static_cast<unsigned>(colData->m_nNumBoxes) : 0,
              colData ? static_cast<unsigned>(colData->m_nNumLines) : 0,
              colData ? static_cast<unsigned>(colData->m_nNumTriangles) : 0,
              colData && colData->bHasShadowInfo ? 1 : 0,
              colMin.x, colMin.y, colMin.z,
              colMax.x, colMax.y, colMax.z,
              worldMin.x, worldMin.y, worldMin.z,
              worldMax.x, worldMax.y, worldMax.z);
        ++logged;
    }
#else
    (void)poolName;
    (void)pool;
    (void)playerPos;
    (void)radius;
#endif
}

template <typename TPool>
static void ScanWorldEntityPool64(
    const char* poolName,
    TPool* pool,
    const CVector& playerPos,
    float radius,
    WorldPoolDiag64& diag,
    WorldEntitySample64 samples[],
    int& sampleCount)
{
#if !VER_x32
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0) {
        return;
    }
    const float radius2 = radius * radius;
    for (int32 i = 0; i < pool->m_nSize; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        diag.used++;

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity) {
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        const float dx = entityPos.x - playerPos.x;
        const float dy = entityPos.y - playerPos.y;
        const float dz = entityPos.z - playerPos.z;
        const float dist2 = dx * dx + dy * dy + dz * dz;
        if (dist2 > radius2) {
            continue;
        }

        diag.near++;

        const int modelId = entity->m_nModelIndex;
        if (IsTempVehicleComponentModel64(modelId)) {
            SuppressTempVehicleComponentCollision64(entity);
            diag.noCollision++;
            continue;
        }
        CBaseModelInfo* modelInfo = nullptr;
        if (modelId >= 0 && modelId < CModelInfo::NUM_MODEL_INFOS) {
            modelInfo = CModelInfo::GetModelInfo(modelId);
        }
        if (IsUnstableBreakableWorldProp64(modelId, modelInfo)) {
            PreserveBreakableWorldPropCollision64(entity);
        }

        const bool hasModelInfo = modelInfo != nullptr;
        const bool loaded = Supremo::Streaming::IsValidResourceId(modelId) && Supremo::Streaming::IsModelLoaded(modelId);
        const bool entityRw = entity->m_pRwObject != nullptr;
        const bool modelRw = modelInfo && modelInfo->m_pRwObject;
        const bool hasCollision = modelInfo && modelInfo->m_pColModel;

        if (!hasModelInfo) {
            diag.noModelInfo++;
        }
        if (!loaded) {
            diag.notLoaded++;
        }
        if (!entityRw) {
            diag.noEntityRw++;
        }
        if (!modelRw) {
            diag.noModelRw++;
        }
        if (!entity->m_bIsVisible) {
            diag.invisible++;
        }
        if (!hasCollision) {
            diag.noCollision++;
        }

        WorldEntitySample64 sample{};
        sample.poolName = poolName;
        sample.slot = i;
        sample.modelId = modelId;
        sample.dist2 = dist2;
        sample.pos = entityPos;
        sample.loaded = loaded;
        sample.entityRw = entityRw;
        sample.modelRw = modelRw;
        sample.visible = entity->m_bIsVisible;
        sample.hasCollision = hasCollision;
        sample.entityCollision = entity->m_bUsesCollision;
        sample.refCount = modelInfo ? modelInfo->m_nRefCount : 0;
        sample.txdIndex = modelInfo ? modelInfo->m_nTxdIndex : -1;
        sample.area = entity->m_nAreaCode;
        sample.flags = entity->m_nFlags;
        CopyModelName64(sample.modelName, modelInfo);
        StoreWorldSample64(samples, sampleCount, sample);
    }
#endif
}

static void LogStaticWorldPoolsAroundPlayer64(const CVector& pos)
{
#if !VER_x32
    static uint32_t s_lastWorldDiagTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastWorldDiagTick < 3500) {
        return;
    }
    s_lastWorldDiagTick = now;

    constexpr float kDiagRadius = 140.0f;
    WorldPoolDiag64 buildingDiag{};
    WorldPoolDiag64 objectDiag{};
    WorldPoolDiag64 dummyDiag{};
    WorldEntitySample64 samples[8]{};
    int sampleCount = 0;

    ScanWorldEntityPool64("B", GetBuildingPool(), pos, kDiagRadius, buildingDiag, samples, sampleCount);
    ScanWorldEntityPool64("O", GetObjectPoolGta(), pos, kDiagRadius, objectDiag, samples, sampleCount);
    ScanWorldEntityPool64("D", GetDummyPool(), pos, kDiagRadius, dummyDiag, samples, sampleCount);

    FLog("[WORLD_DIAG64] pos=%.2f %.2f %.2f r=%.0f B{used:%d near:%d rw0:%d load0:%d mi0:%d mrw0:%d invis:%d col0:%d} O{used:%d near:%d rw0:%d load0:%d mi0:%d mrw0:%d invis:%d col0:%d} D{used:%d near:%d rw0:%d load0:%d mi0:%d mrw0:%d invis:%d col0:%d} streamingDisabled=%d",
         pos.x, pos.y, pos.z, kDiagRadius,
         buildingDiag.used, buildingDiag.near, buildingDiag.noEntityRw, buildingDiag.notLoaded, buildingDiag.noModelInfo, buildingDiag.noModelRw, buildingDiag.invisible, buildingDiag.noCollision,
         objectDiag.used, objectDiag.near, objectDiag.noEntityRw, objectDiag.notLoaded, objectDiag.noModelInfo, objectDiag.noModelRw, objectDiag.invisible, objectDiag.noCollision,
         dummyDiag.used, dummyDiag.near, dummyDiag.noEntityRw, dummyDiag.notLoaded, dummyDiag.noModelInfo, dummyDiag.noModelRw, dummyDiag.invisible, dummyDiag.noCollision,
         Supremo::Streaming::IsStreamingDisabled() ? 1 : 0);

    for (int i = 0; i < sampleCount; ++i) {
        const WorldEntitySample64& sample = samples[i];
        FLog("[WORLD_NEAR64] pool=%s slot=%d model=%d name=%s txd=%d ref=%d dist=%.2f pos=%.2f %.2f %.2f area=%u rw=%d mrw=%d loaded=%d vis=%d colModel=%d useCol=%d flags=0x%08X",
             sample.poolName ? sample.poolName : "?",
             sample.slot,
             sample.modelId,
             sample.modelName[0] ? sample.modelName : "?",
             sample.txdIndex,
             sample.refCount,
             sqrtf(sample.dist2),
             sample.pos.x, sample.pos.y, sample.pos.z,
             static_cast<unsigned>(sample.area),
             sample.entityRw ? 1 : 0,
             sample.modelRw ? 1 : 0,
             sample.loaded ? 1 : 0,
              sample.visible ? 1 : 0,
              sample.hasCollision ? 1 : 0,
              sample.entityCollision ? 1 : 0,
              sample.flags);
    }

    LogWatchedWorldEntities64("B", GetBuildingPool(), pos, 230.0f);
    LogWatchedWorldEntities64("O", GetObjectPoolGta(), pos, 180.0f);
    LogWatchedWorldEntities64("D", GetDummyPool(), pos, 180.0f);
#endif
}

#if !VER_x32
struct WorldForceRenderStats64 {
    int scanned = 0;
    int rendered = 0;
    int skippedArea = 0;
    int skippedState = 0;
};

static bool ShouldRestoreWorldEntityVisibility64(CEntityGTA* entity, CBaseModelInfo* modelInfo);

static bool ModelNameContains64(const char* modelName, const char* needle)
{
    if (!modelName || !needle || !*needle) {
        return false;
    }

    char safeName[22]{};
    memcpy(safeName, modelName, 21);
    safeName[21] = '\0';

    for (int i = 0; safeName[i]; ++i) {
        int j = 0;
        while (needle[j] && safeName[i + j]) {
            char a = safeName[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z') {
                a = static_cast<char>(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = static_cast<char>(b - 'A' + 'a');
            }
            if (a != b) {
                break;
            }
            ++j;
        }
        if (!needle[j]) {
            return true;
        }
    }

    return false;
}

static bool ModelNameStartsWith64(const char* modelName, const char* prefix)
{
    if (!modelName || !prefix || !*prefix) {
        return false;
    }

    for (int i = 0; prefix[i]; ++i) {
        char a = modelName[i];
        char b = prefix[i];
        if (!a) {
            return false;
        }
        if (a >= 'A' && a <= 'Z') {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }

    return true;
}

static bool IsLodWorldModel64(CBaseModelInfo* modelInfo)
{
    if (!modelInfo) {
        return false;
    }

    const char* name = modelInfo->m_modelName;
    return ModelNameStartsWith64(name, "lod") ||
           ModelNameContains64(name, "_lod") ||
           ModelNameContains64(name, "lod_") ||
           ModelNameContains64(name, "lodbit");
}

static bool IsLikelyNonSurfacePropName64(const char* name)
{
    return ModelNameContains64(name, "sign") ||
           ModelNameContains64(name, "signal") ||
           ModelNameContains64(name, "light") ||
           ModelNameContains64(name, "lamp") ||
           ModelNameContains64(name, "wire") ||
           ModelNameContains64(name, "pole") ||
           ModelNameContains64(name, "pylon") ||
           ModelNameContains64(name, "tree") ||
           ModelNameContains64(name, "bush") ||
           ModelNameContains64(name, "veg_") ||
           ModelNameContains64(name, "_veg") ||
           ModelNameContains64(name, "vege") ||
           ModelNameContains64(name, "windmill");
}

static bool IsUnstableBreakableWorldPropName64(const char* name)
{
    return ModelNameContains64(name, "sign") ||
           ModelNameContains64(name, "signal") ||
           ModelNameContains64(name, "light") ||
           ModelNameContains64(name, "lamp") ||
           ModelNameContains64(name, "pole") ||
           ModelNameContains64(name, "pylon") ||
           ModelNameContains64(name, "mtraffic") ||
           ModelNameContains64(name, "trafficlight") ||
           ModelNameContains64(name, "traffic_light") ||
           ModelNameContains64(name, "telgrph") ||
           ModelNameContains64(name, "dyn_mesh") ||
           ModelNameContains64(name, "airyelrm") ||
           ModelNameContains64(name, "airuntest") ||
           ModelNameContains64(name, "runsigns");
}

static bool IsUnstableBreakableWorldProp64(int modelId, CBaseModelInfo* modelInfo)
{
    return IsUnstableBreakableWorldPropModel64(modelId) ||
           (modelInfo && IsUnstableBreakableWorldPropName64(modelInfo->m_modelName));
}

static bool IsLikelyVegetationPropName64(const char* name)
{
    if (ModelNameContains64(name, "seaweed") ||
        ModelNameContains64(name, "starfish") ||
        ModelNameContains64(name, "fish") ||
        ModelNameContains64(name, "fire")) {
        return false;
    }

    return ModelNameContains64(name, "tree") ||
           ModelNameContains64(name, "palm") ||
           ModelNameContains64(name, "pine") ||
           ModelNameContains64(name, "fir") ||
           ModelNameContains64(name, "elm") ||
           ModelNameContains64(name, "cedar") ||
           ModelNameContains64(name, "redwood") ||
           ModelNameContains64(name, "bush") ||
           ModelNameContains64(name, "scrub") ||
           ModelNameContains64(name, "scrb") ||
           ModelNameContains64(name, "cactus") ||
           ModelNameContains64(name, "hedge") ||
           ModelNameContains64(name, "veg_") ||
           ModelNameContains64(name, "_veg") ||
           ModelNameContains64(name, "vege") ||
           ModelNameContains64(name, "plant");
}

static bool IsLikelyPhysicalPropModel64(CBaseModelInfo* modelInfo)
{
    if (!modelInfo ||
        IsLodWorldModel64(modelInfo) ||
        IsLikelyRoadModel64(modelInfo) ||
        IsLikelyGroundSurfaceModel64(modelInfo) ||
        IsKnownInteriorShellSurfaceModel64(modelInfo)) {
        return false;
    }

    const char* name = modelInfo->m_modelName;
    if (ModelNameContains64(name, "neon") ||
        ModelNameContains64(name, "corona") ||
        ModelNameContains64(name, "seaweed") ||
        ModelNameContains64(name, "starfish") ||
        ModelNameContains64(name, "fish") ||
        IsUnstableBreakableWorldPropName64(name)) {
        return false;
    }

    return IsLikelyVegetationPropName64(name) ||
           ModelNameContains64(name, "lamppost") ||
           ModelNameContains64(name, "lampost") ||
           ModelNameContains64(name, "streetlamp") ||
           ModelNameContains64(name, "mlamppost") ||
           ModelNameContains64(name, "telepole") ||
           ModelNameContains64(name, "telgrph") ||
           ModelNameContains64(name, "pole") ||
           ModelNameContains64(name, "pylon") ||
           ModelNameContains64(name, "fence") ||
           ModelNameContains64(name, "gate") ||
           ModelNameContains64(name, "rail") ||
           ModelNameContains64(name, "bollard") ||
           ModelNameContains64(name, "hydrant") ||
           ModelNameContains64(name, "bench") ||
           ModelNameContains64(name, "table") ||
           ModelNameContains64(name, "chair") ||
           ModelNameContains64(name, "seat") ||
           ModelNameContains64(name, "stool") ||
           ModelNameContains64(name, "sofa") ||
           ModelNameContains64(name, "couch") ||
           ModelNameContains64(name, "crate") ||
           ModelNameContains64(name, "barrel") ||
           ModelNameContains64(name, "dump") ||
           ModelNameContains64(name, "hay") ||
           ModelNameContains64(name, "wood") ||
           ModelNameContains64(name, "panel") ||
           ModelNameContains64(name, "mesh") ||
           ModelNameContains64(name, "door") ||
           ModelNameContains64(name, "diner") ||
           ModelNameContains64(name, "trukstp") ||
           ModelNameContains64(name, "counter") ||
           ModelNameContains64(name, "vendor");
}

static bool ShouldRepairRenderableWorldProp64(CBaseModelInfo* modelInfo)
{
    if (!modelInfo || IsLodWorldModel64(modelInfo)) {
        return false;
    }

    const char* name = modelInfo->m_modelName;
    if (IsUnstableBreakableWorldPropName64(name)) {
        return false;
    }

    return IsLikelyPhysicalPropModel64(modelInfo) ||
           IsLikelyVegetationPropName64(name) ||
           ModelNameContains64(name, "airport") ||
           ModelNameContains64(name, "airprt") ||
           ModelNameContains64(name, "airpurt") ||
           ModelNameContains64(name, "lasair") ||
           ModelNameContains64(name, "runway") ||
           ModelNameContains64(name, "rnway") ||
           ModelNameContains64(name, "apron") ||
           ModelNameContains64(name, "hangar") ||
           ModelNameContains64(name, "baggage") ||
           ModelNameContains64(name, "jetty") ||
           ModelNameContains64(name, "esc_step") ||
           ModelNameContains64(name, "pipe") ||
           ModelNameContains64(name, "roofbit") ||
           ModelNameContains64(name, "newstand") ||
           ModelNameContains64(name, "sprunk") ||
           ModelNameContains64(name, "traffic");
}

static bool IsLikelyRoadModel64(CBaseModelInfo* modelInfo)
{
    if (!modelInfo) {
        return false;
    }

    const char* name = modelInfo->m_modelName;
    if (IsLikelyNonSurfacePropName64(name) ||
        IsUnstableBreakableWorldPropName64(name)) {
        return false;
    }

    return ModelNameContains64(name, "road") ||
           ModelNameContains64(name, "hiway") ||
           ModelNameContains64(name, "freeway") ||
           ModelNameContains64(name, "bridge") ||
           ModelNameContains64(name, "rnway") ||
           ModelNameContains64(name, "runway") ||
           ModelNameContains64(name, "airun") ||
           ModelNameContains64(name, "airport") ||
           ModelNameContains64(name, "airprt") ||
           ModelNameContains64(name, "airpurt") ||
           ModelNameContains64(name, "airoad") ||
           ModelNameContains64(name, "apron") ||
           ModelNameContains64(name, "taxi");
}

static bool IsLikelyGroundSurfaceModel64(CBaseModelInfo* modelInfo)
{
    if (!modelInfo) {
        return false;
    }

    const char* name = modelInfo->m_modelName;
    if (IsLikelyNonSurfacePropName64(name) ||
        IsUnstableBreakableWorldPropName64(name)) {
        return false;
    }

    const bool knownCuntwTerrain =
        ModelNameContains64(name, "dirtover") ||
        ModelNameContains64(name, "rockover") ||
        ModelNameContains64(name, "xxxxxxtra") ||
        ModelNameContains64(name, "xxxtra") ||
        ModelNameContains64(name, "xxxover") ||
        ModelNameContains64(name, "xxxe") ||
        ModelNameContains64(name, "xxxxxxxxa") ||
        ModelNameContains64(name, "xxxc01") ||
        ModelNameContains64(name, "xxxd") ||
        ModelNameContains64(name, "xxxzc") ||
        ModelNameContains64(name, "xxxza") ||
        ModelNameContains64(name, "xxovr2") ||
        ModelNameContains64(name, "xxcliff") ||
        ModelNameContains64(name, "xoverelaya");

    return ModelNameContains64(name, "land") ||
           ModelNameContains64(name, "grnd") ||
           ModelNameContains64(name, "ground") ||
           ModelNameContains64(name, "terrain") ||
           ModelNameContains64(name, "hillpalos") ||
           ModelNameContains64(name, "clifftest") ||
           ModelNameContains64(name, "lasgrif") ||
           ModelNameContains64(name, "griffith") ||
           ModelNameContains64(name, "grifovrhang") ||
           ModelNameContains64(name, "coast") ||
           ModelNameContains64(name, "beach") ||
           ModelNameContains64(name, "floor") ||
           ModelNameContains64(name, "tarmac") ||
           ModelNameContains64(name, "asph") ||
           ModelNameContains64(name, "dock") ||
           ModelNameContains64(name, "pier") ||
           ModelNameContains64(name, "quay") ||
           ModelNameContains64(name, "wharf") ||
           ModelNameContains64(name, "tunnel") ||
           knownCuntwTerrain;
}

static bool IsKnownInteriorShellSurfaceModel64(CBaseModelInfo* modelInfo)
{
    if (!modelInfo || IsLodWorldModel64(modelInfo)) {
        return false;
    }

    const char* name = modelInfo->m_modelName;
    return ModelNameContains64(name, "int_7_11") ||
           ModelNameContains64(name, "711_c") ||
           ModelNameContains64(name, "711_d");
}

static bool IsEntityAreaActiveForPlayer64(uint8_t entityArea, uint8_t playerArea)
{
    if (entityArea == static_cast<uint8_t>(AREA_CODE_NORMAL_WORLD) ||
        entityArea == playerArea) {
        return true;
    }

#if !VER_x32
    // GTA SA uses interior 13 for objects that should stream regardless of the player's area.
    return entityArea == static_cast<uint8_t>(AREA_CODE_13);
#else
    return false;
#endif
}

static void RenderStaticWorldEntity64(CEntityGTA* entity, bool road)
{
    if (!entity || !entity->m_pRwObject) {
        return;
    }

    const uintptr_t vtable = *reinterpret_cast<uintptr_t*>(entity);
    if (vtable) {
        reinterpret_cast<void (*)(CEntityGTA*)>(*reinterpret_cast<uintptr_t*>(vtable + 0x90))(entity);
    }

    if (road) {
        GTASAEngineApi::RenderStaticRoadEntity(entity);
    } else {
        GTASAEngineApi::RenderStaticNonRoadEntity(entity);
    }
}

template <typename TPool>
static void RenderStaticWorldPool64(
    TPool* pool,
    const CVector& playerPos,
    float radius,
    uint8_t playerArea,
    bool roadsOnly,
    int maxRender,
    WorldForceRenderStats64& stats)
{
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0 || stats.rendered >= maxRender) {
        return;
    }

    const float radius2 = radius * radius;
    for (int32 i = 0; i < pool->m_nSize && stats.rendered < maxRender; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity) {
            continue;
        }

        ++stats.scanned;

        const uint8_t entityArea = static_cast<uint8_t>(entity->m_nAreaCode);
        if (!IsEntityAreaActiveForPlayer64(entityArea, playerArea)) {
            ++stats.skippedArea;
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        const float dx = entityPos.x - playerPos.x;
        const float dy = entityPos.y - playerPos.y;
        const float dz = entityPos.z - playerPos.z;
        if (dx * dx + dy * dy + dz * dz > radius2) {
            continue;
        }

        const int modelId = entity->m_nModelIndex;
        if (!Supremo::Streaming::IsValidResourceId(modelId) || !entity->m_pRwObject) {
            ++stats.skippedState;
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
        if (!modelInfo || !modelInfo->m_pRwObject || !Supremo::Streaming::IsModelLoaded(modelId)) {
            ++stats.skippedState;
            continue;
        }

        if (IsLodWorldModel64(modelInfo)) {
            ++stats.skippedState;
            continue;
        }

        const bool isRoad = IsLikelyRoadModel64(modelInfo);
        if (isRoad != roadsOnly) {
            continue;
        }

        if (!entity->m_bIsVisible && ShouldRestoreWorldEntityVisibility64(entity, modelInfo)) {
            entity->m_bIsVisible = true;
        }

        if (!entity->m_bIsVisible) {
            ++stats.skippedState;
            continue;
        }

        RenderStaticWorldEntity64(entity, roadsOnly);
        ++stats.rendered;
    }
}

template <typename TPool>
static int CullNearbyLodWorldPool64(TPool* pool, const CVector& playerPos, const CVector& cameraPos, float radius)
{
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0) {
        return 0;
    }

    const float radius2 = radius * radius;
    int hidden = 0;
    for (int32 i = 0; i < pool->m_nSize; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity || !entity->m_bIsVisible) {
            continue;
        }

        const int modelId = entity->m_nModelIndex;
        if (!Supremo::Streaming::IsValidResourceId(modelId)) {
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
        if (!IsLodWorldModel64(modelInfo)) {
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        const float playerDx = entityPos.x - playerPos.x;
        const float playerDy = entityPos.y - playerPos.y;
        const float playerDz = entityPos.z - playerPos.z;
        const float cameraDx = entityPos.x - cameraPos.x;
        const float cameraDy = entityPos.y - cameraPos.y;
        const float cameraDz = entityPos.z - cameraPos.z;
        const float playerDist2 = playerDx * playerDx + playerDy * playerDy + playerDz * playerDz;
        const float cameraDist2 = cameraDx * cameraDx + cameraDy * cameraDy + cameraDz * cameraDz;
        if (playerDist2 > radius2 && cameraDist2 > radius2) {
            continue;
        }

        entity->m_bIsVisible = false;
        ++hidden;
    }

    return hidden;
}

template <typename TPool>
static int CullNearbyInactiveAreaWorldPool64(
    TPool* pool,
    const CVector& playerPos,
    const CVector& cameraPos,
    float radius,
    uint8_t playerArea)
{
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0) {
        return 0;
    }

    const float radius2 = radius * radius;
    int hidden = 0;
    for (int32 i = 0; i < pool->m_nSize; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity || !entity->m_bIsVisible) {
            continue;
        }

        const uint8_t entityArea = static_cast<uint8_t>(entity->m_nAreaCode);
        if (IsEntityAreaActiveForPlayer64(entityArea, playerArea)) {
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        const float playerDx = entityPos.x - playerPos.x;
        const float playerDy = entityPos.y - playerPos.y;
        const float playerDz = entityPos.z - playerPos.z;
        const float cameraDx = entityPos.x - cameraPos.x;
        const float cameraDy = entityPos.y - cameraPos.y;
        const float cameraDz = entityPos.z - cameraPos.z;
        const float playerDist2 = playerDx * playerDx + playerDy * playerDy + playerDz * playerDz;
        const float cameraDist2 = cameraDx * cameraDx + cameraDy * cameraDy + cameraDz * cameraDz;
        if (playerDist2 > radius2 && cameraDist2 > radius2) {
            continue;
        }

        entity->m_bIsVisible = false;
        ++hidden;
    }

    return hidden;
}

static void CullNearbyLodWorldEntities64()
{
    if (!pGame || !pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
        return;
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
    CPlayerPed* localPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;
    if (!localPlayer || !localPed || !localPed->m_pPed) {
        return;
    }

    const CVector playerPos = localPed->m_pPed->GetPosition();
    const uint8_t playerArea = NormalizeLocalExteriorArea64(localPed, playerPos, "lod-cull");
    CCamera& camera = GTASAEngineApi::Camera();
    const uint8_t activeIndex = camera.m_nActiveCam < 3 ? camera.m_nActiveCam : 0;
    const CVector cameraPos = camera.m_aCams[activeIndex].Source;
    constexpr float kCullLodRadius = 230.0f;

    const int hiddenBuildings = CullNearbyLodWorldPool64(GetBuildingPool(), playerPos, cameraPos, kCullLodRadius);
    const int hiddenObjects = CullNearbyLodWorldPool64(GetObjectPoolGta(), playerPos, cameraPos, kCullLodRadius);
    const int hiddenDummies = CullNearbyLodWorldPool64(GetDummyPool(), playerPos, cameraPos, kCullLodRadius);
    const int areaHiddenBuildings = CullNearbyInactiveAreaWorldPool64(GetBuildingPool(), playerPos, cameraPos, kCullLodRadius, playerArea);
    const int areaHiddenObjects = CullNearbyInactiveAreaWorldPool64(GetObjectPoolGta(), playerPos, cameraPos, kCullLodRadius, playerArea);
    const int areaHiddenDummies = CullNearbyInactiveAreaWorldPool64(GetDummyPool(), playerPos, cameraPos, kCullLodRadius, playerArea);

    static uint32_t s_lastLodCullLogTick = 0;
    const uint32_t now = GetTickCount();
    const int totalHidden = hiddenBuildings + hiddenObjects + hiddenDummies +
                            areaHiddenBuildings + areaHiddenObjects + areaHiddenDummies;
    if (totalHidden > 0 && now - s_lastLodCullLogTick >= 3500) {
        s_lastLodCullLogTick = now;
        FLog("[LOD_CULL64] lodHidden B=%d O=%d D=%d areaHidden B=%d O=%d D=%d player=%.2f %.2f %.2f area=%u cam=%.2f %.2f %.2f r=%.0f",
             hiddenBuildings,
             hiddenObjects,
             hiddenDummies,
             areaHiddenBuildings,
             areaHiddenObjects,
             areaHiddenDummies,
             playerPos.x, playerPos.y, playerPos.z,
             static_cast<unsigned>(playerArea),
             cameraPos.x, cameraPos.y, cameraPos.z,
             kCullLodRadius);
    }
}

static void RenderNearbyStaticWorldEntities64(bool roadsOnly)
{
    if (!pGame || !pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
        return;
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
    CPlayerPed* localPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;
    if (!localPlayer || (!localPlayer->m_bIsActive && !localPlayer->IsClearedToSpawn()) || !localPed || !localPed->m_pPed) {
        return;
    }

    const float forceRenderRadius = roadsOnly ? 300.0f : 260.0f;
    CPool<CObjectGta, CCutsceneObject>* objectPool = GetObjectPoolGta();
    const int maxForcedBuildings = roadsOnly ? 384 : 0;
    const int maxForcedObjects = objectPool && objectPool->m_nSize > 0
                                 ? objectPool->m_nSize
                                 : (roadsOnly ? 512 : 3000);
    WorldForceRenderStats64 buildingStats{};
    WorldForceRenderStats64 objectStats{};
    const CVector pos = localPed->m_pPed->GetPosition();
    const uint8_t area = NormalizeLocalExteriorArea64(localPed, pos, roadsOnly ? "render-roads" : "render-world");

    RenderStaticWorldPool64(GetBuildingPool(), pos, forceRenderRadius, area, roadsOnly, maxForcedBuildings, buildingStats);
    RenderStaticWorldPool64(objectPool, pos, forceRenderRadius, area, roadsOnly, maxForcedObjects, objectStats);

    WorldForceRenderStats64 stats{};
    stats.scanned = buildingStats.scanned + objectStats.scanned;
    stats.rendered = buildingStats.rendered + objectStats.rendered;
    stats.skippedArea = buildingStats.skippedArea + objectStats.skippedArea;
    stats.skippedState = buildingStats.skippedState + objectStats.skippedState;

    static uint32_t s_lastLogTick[2] = {};
    const uint32_t now = GetTickCount();
    const int logIndex = roadsOnly ? 1 : 0;
    if (now - s_lastLogTick[logIndex] >= 3500) {
        s_lastLogTick[logIndex] = now;
        CCamera& camera = GTASAEngineApi::Camera();
        const uint8_t activeIndex = camera.m_nActiveCam < 3 ? camera.m_nActiveCam : 0;
        CCam& activeCam = camera.m_aCams[activeIndex];

        FLog("[WORLD_RENDER64] pass=%s rendered=%d scanned=%d areaSkip=%d stateSkip=%d B=%d/%d O=%d/%d pos=%.2f %.2f %.2f area=%u camSrc=%.2f %.2f %.2f camFront=%.3f %.3f %.3f fixedSrc=%.2f %.2f %.2f mode=%u goto=%u ctrl=%d",
             roadsOnly ? "roads" : "nonroads",
             stats.rendered,
             stats.scanned,
             stats.skippedArea,
             stats.skippedState,
             buildingStats.rendered,
             maxForcedBuildings,
             objectStats.rendered,
             maxForcedObjects,
             pos.x, pos.y, pos.z,
             static_cast<unsigned>(area),
             activeCam.Source.x, activeCam.Source.y, activeCam.Source.z,
             activeCam.Front.x, activeCam.Front.y, activeCam.Front.z,
             camera.m_vecFixedModeSource.x, camera.m_vecFixedModeSource.y, camera.m_vecFixedModeSource.z,
             static_cast<unsigned>(activeCam.m_nMode),
             static_cast<unsigned>(camera.m_nModeToGoTo),
             camera.WhoIsInControlOfTheCamera);
    }
}

static void LogRendererLists64(const char* stage)
{
    static uint32_t s_lastLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastLogTick < 3500) {
        return;
    }
    s_lastLogTick = now;

    GTASAEngineApi::RendererListDebugState rendererDebug{};
    if (!GTASAEngineApi::GetRendererListDebugState(rendererDebug)) {
        return;
    }

    FLog("[RENDER_LIST64] stage=%s native={vis:%d,lod:%d,super:%d,inv:%d} wrapper={vis:%d,lod:%d,super:%d,inv:%d} got={visList:%p,visCount:%p,lodList:%p,lodCount:%p} wrapperAddr={visList:%p,visCount:%p,lodList:%p,lodCount:%p} outside=%d priority=%d",
         stage ? stage : "?",
         rendererDebug.nativeVisible,
         rendererDebug.nativeLods,
         rendererDebug.nativeSuperLods,
         rendererDebug.nativeInvisible,
         CRenderer::ms_nNoOfVisibleEntities,
         CRenderer::ms_nNoOfVisibleLods,
         CRenderer::ms_nNoOfVisibleSuperLods,
         CRenderer::ms_nNoOfInVisibleEntities,
         reinterpret_cast<void*>(rendererDebug.visibleListPtr),
         reinterpret_cast<void*>(rendererDebug.visibleCountPtr),
         reinterpret_cast<void*>(rendererDebug.lodListPtr),
         reinterpret_cast<void*>(rendererDebug.lodCountPtr),
         reinterpret_cast<void*>(CRenderer::ms_aVisibleEntityPtrs),
         reinterpret_cast<void*>(&CRenderer::ms_nNoOfVisibleEntities),
         reinterpret_cast<void*>(CRenderer::ms_aVisibleLodPtrs),
         reinterpret_cast<void*>(&CRenderer::ms_nNoOfVisibleLods),
         CRenderer::ms_bRenderOutsideTunnels ? 1 : 0,
         CRenderer::m_loadingPriority ? 1 : 0);
}

template <class TPool>
static bool IsRendererEntityFromPool64(TPool* pool, const CEntityGTA* entity, int* outSlot = nullptr)
{
    if (outSlot) {
        *outSlot = -1;
    }
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0 || !entity) {
        return false;
    }

    const uintptr_t ptr = reinterpret_cast<uintptr_t>(entity);
    const uintptr_t start = reinterpret_cast<uintptr_t>(pool->m_pObjects);
    size_t stride = sizeof(typename TPool::widest_type);

    if (GetVehiclePoolGta() && reinterpret_cast<const void*>(pool) == reinterpret_cast<const void*>(GetVehiclePoolGta())) {
        static size_t s_vehicleNativeStride = 0;
        if (s_vehicleNativeStride == 0) {
            for (int i = 1; i < pool->m_nSize; ++i) {
                CVehicleGTA* v = GTASAEngineApi::GetVehicleFromPool(i);
                if (v) {
                    uintptr_t v_ptr = reinterpret_cast<uintptr_t>(v);
                    if (v_ptr > start) {
                        s_vehicleNativeStride = (v_ptr - start) / i;
                        FLog("[POOL_FIX64] Dynamically detected vehicle pool native stride from slot %d: 0x%zX (%zu bytes)", i, s_vehicleNativeStride, s_vehicleNativeStride);
                        break;
                    }
                }
            }
        }
        if (s_vehicleNativeStride != 0) {
            stride = s_vehicleNativeStride;
        }
    } else if (GetPedPoolGta() && reinterpret_cast<const void*>(pool) == reinterpret_cast<const void*>(GetPedPoolGta())) {
        static size_t s_pedNativeStride = 0;
        if (s_pedNativeStride == 0) {
            for (int i = 1; i < pool->m_nSize; ++i) {
                CPedGTA* p = GTASAEngineApi::GetPedFromPool(i);
                if (p) {
                    uintptr_t p_ptr = reinterpret_cast<uintptr_t>(p);
                    if (p_ptr > start) {
                        s_pedNativeStride = (p_ptr - start) / i;
                        FLog("[POOL_FIX64] Dynamically detected ped pool native stride from slot %d: 0x%zX (%zu bytes)", i, s_pedNativeStride, s_pedNativeStride);
                        break;
                    }
                }
            }
        }
        if (s_pedNativeStride != 0) {
            stride = s_pedNativeStride;
        }
    }

    const uintptr_t end = start + static_cast<uintptr_t>(stride) * static_cast<uintptr_t>(pool->m_nSize);
    if (ptr < start || ptr >= end) {
        return false;
    }

    const uintptr_t offset = ptr - start;
    if (stride == 0 || (offset % stride) != 0) {
        return false;
    }

    const int slot = static_cast<int>(offset / stride);
    if (slot < 0 || slot >= pool->m_nSize || pool->m_byteMap[slot].bEmpty) {
        return false;
    }

    if (outSlot) {
        *outSlot = slot;
    }
    return true;
}

static bool IsRendererEntityPoolBacked64(const CEntityGTA* entity, char* outPool, size_t outPoolSize, int* outSlot)
{
    // Bypass explícito para entidades gerenciadas pelo SA-MP
    if (pNetGame) {
        // 1. Veículos do SA-MP
        if (pNetGame->GetVehiclePool()) {
            CVehicleGTA* gtaVeh = const_cast<CVehicleGTA*>(reinterpret_cast<const CVehicleGTA*>(entity));
            VEHICLEID sampVehId = pNetGame->GetVehiclePool()->FindIDFromGtaPtr(gtaVeh);
            if (sampVehId != INVALID_VEHICLE_ID) {
                if (outPool && outPoolSize > 0) {
                    outPool[0] = 'V';
                    if (outPoolSize > 1) outPool[1] = '\0';
                }
                if (outSlot) {
                    *outSlot = sampVehId;
                }
                return true;
            }
        }

        // 2. Players do SA-MP
        if (pNetGame->GetPlayerPool()) {
            CPlayerPool* playerPool = pNetGame->GetPlayerPool();
            CPedGTA* gtaPed = const_cast<CPedGTA*>(reinterpret_cast<const CPedGTA*>(entity));

            // Player local
            if (playerPool->GetLocalPlayer() && playerPool->GetLocalPlayer()->GetPlayerPed() &&
                playerPool->GetLocalPlayer()->GetPlayerPed()->m_pPed == gtaPed) {
                if (outPool && outPoolSize > 0) {
                    outPool[0] = 'P';
                    if (outPoolSize > 1) outPool[1] = '\0';
                }
                if (outSlot) {
                    *outSlot = 999;
                }
                return true;
            }

            // Player remoto
            PLAYERID playerId = playerPool->FindRemotePlayerIDFromGtaPtr(gtaPed);
            if (playerId != INVALID_PLAYER_ID) {
                if (outPool && outPoolSize > 0) {
                    outPool[0] = 'P';
                    if (outPoolSize > 1) outPool[1] = '\0';
                }
                if (outSlot) {
                    *outSlot = playerId;
                }
                return true;
            }
        }

        // 3. Actors do SA-MP
        if (pNetGame->GetActorPool()) {
            CPedGTA* gtaPed = const_cast<CPedGTA*>(reinterpret_cast<const CPedGTA*>(entity));
            PLAYERID actorId = pNetGame->GetActorPool()->FindIDFromGtaPtr(gtaPed);
            if (actorId != INVALID_PLAYER_ID) {
                if (outPool && outPoolSize > 0) {
                    outPool[0] = 'P';
                    if (outPoolSize > 1) outPool[1] = '\0';
                }
                if (outSlot) {
                    *outSlot = actorId;
                }
                return true;
            }
        }
    }

    int slot = -1;
    const char* poolName = nullptr;
    if (IsRendererEntityFromPool64(GetBuildingPool(), entity, &slot)) {
        poolName = "B";
    } else if (IsRendererEntityFromPool64(GetObjectPoolGta(), entity, &slot)) {
        poolName = "O";
    } else if (IsRendererEntityFromPool64(GetDummyPool(), entity, &slot)) {
        poolName = "D";
    } else if (IsRendererEntityFromPool64(GetPedPoolGta(), entity, &slot)) {
        poolName = "P";
    } else if (IsRendererEntityFromPool64(GetVehiclePoolGta(), entity, &slot)) {
        poolName = "V";
    }

    if (!poolName) {
        return false;
    }

    if (outPool && outPoolSize > 0) {
        outPool[0] = poolName[0];
        if (outPoolSize > 1) {
            outPool[1] = '\0';
        }
    }
    if (outSlot) {
        *outSlot = slot;
    }
    return true;
}

static bool IsRendererRwPointerPlausible64(const void* rwObject)
{
    const uintptr_t ptr = reinterpret_cast<uintptr_t>(rwObject);
    return ptr >= 0x10000ULL && (ptr & 0x7ULL) == 0;
}

static bool IsRendererEntitySafe64(CEntityGTA* entity,
                                   const char* listName,
                                   int index,
                                   char* outPool,
                                   size_t outPoolSize,
                                   int* outSlot,
                                   const char** outReason)
{
    if (outReason) {
        *outReason = "ok";
    }
    if (!entity) {
        if (outReason) *outReason = "null";
        return false;
    }

    if (!IsRendererEntityPoolBacked64(entity, outPool, outPoolSize, outSlot)) {
        if (outReason) *outReason = "pool";
        return false;
    }

    if (entity->m_bRemoveFromWorld) {
        if (outReason) *outReason = "remove";
        return false;
    }

    const int modelId = static_cast<int>(entity->m_nModelIndex);
    if (modelId < 0 || modelId >= CModelInfo::NUM_MODEL_INFOS) {
        if (outReason) *outReason = "model";
        return false;
    }

    CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
    if (!modelInfo) {
        if (outReason) *outReason = "model-info";
        return false;
    }

    if (!Supremo::Streaming::IsModelLoaded(modelId)) {
        if (outReason) *outReason = "stream";
        return false;
    }

    if (!entity->m_pRwObject) {
        if (outReason) *outReason = "rw-null";
        return false;
    }

    if (!IsRendererRwPointerPlausible64(entity->m_pRwObject)) {
        if (outReason) *outReason = "rw-align";
        return false;
    }

    (void)listName;
    (void)index;
    return true;
}

template <size_t Capacity>
static int32_t SanitizeRendererList64(CEntityGTA** list,
                                      int32_t count,
                                      const char* listName,
                                      int& removed,
                                      int& clamped)
{
    if (!list || count <= 0) {
        return 0;
    }

    int32_t scanCount = count;
    if (scanCount > static_cast<int32_t>(Capacity)) {
        clamped += scanCount - static_cast<int32_t>(Capacity);
        scanCount = static_cast<int32_t>(Capacity);
    }

    int32_t out = 0;
    uint32_t firstBadModel = 0;
    uintptr_t firstBadEntity = 0;
    uintptr_t firstBadRw = 0;
    int firstBadIndex = -1;
    int firstBadSlot = -1;
    char firstBadPool[2]{};
    const char* firstBadReason = nullptr;

    for (int32_t i = 0; i < scanCount; ++i) {
        CEntityGTA* entity = list[i];
        char poolName[2]{};
        int slot = -1;
        const char* reason = nullptr;
        if (IsRendererEntitySafe64(entity, listName, i, poolName, sizeof(poolName), &slot, &reason)) {
            list[out++] = entity;
            continue;
        }

        ++removed;
        if (!firstBadReason) {
            firstBadReason = reason ? reason : "?";
            firstBadIndex = i;
            firstBadSlot = slot;
            firstBadPool[0] = poolName[0] ? poolName[0] : '?';
            firstBadPool[1] = '\0';
            firstBadEntity = reinterpret_cast<uintptr_t>(entity);
            if (entity && slot >= 0) {
                firstBadModel = static_cast<uint32_t>(entity->m_nModelIndex);
                firstBadRw = reinterpret_cast<uintptr_t>(entity->m_pRwObject);
            }
        }
    }

    for (int32_t i = out; i < scanCount; ++i) {
        list[i] = nullptr;
    }

    if (firstBadReason) {
        static uint32_t s_lastRendererSanitizeDetailTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastRendererSanitizeDetailTick >= 900) {
            s_lastRendererSanitizeDetailTick = now;
            FLog("[RENDER_GUARD64] drop list=%s idx=%d reason=%s pool=%s slot=%d entity=%p rw=%p model=%u before=%d after=%d",
                 listName ? listName : "?",
                 firstBadIndex,
                 firstBadReason,
                 firstBadPool[0] ? firstBadPool : "?",
                 firstBadSlot,
                 reinterpret_cast<void*>(firstBadEntity),
                 reinterpret_cast<void*>(firstBadRw),
                 firstBadModel,
                 count,
                 out);
        }
    }

    return out;
}

template <size_t Capacity>
static void ClearRendererList64(CEntityGTA** list, int32_t& count, int& removed)
{
    if (!list || count <= 0) {
        count = 0;
        return;
    }

    const int32_t scanCount = std::min(count, static_cast<int32_t>(Capacity));
    for (int32_t i = 0; i < scanCount; ++i) {
        list[i] = nullptr;
    }

    removed += count;
    count = 0;
}

static void SanitizeRendererLists64(const char* stage)
{
    int removed = 0;
    int clamped = 0;

    CRenderer::ms_nNoOfVisibleEntities = SanitizeRendererList64<MAX_VISIBLE_ENTITY_PTRS>(
        CRenderer::ms_aVisibleEntityPtrs,
        CRenderer::ms_nNoOfVisibleEntities,
        "vis",
        removed,
        clamped);
    CRenderer::ms_nNoOfVisibleLods = SanitizeRendererList64<MAX_VISIBLE_LOD_PTRS>(
        CRenderer::ms_aVisibleLodPtrs,
        CRenderer::ms_nNoOfVisibleLods,
        "lod",
        removed,
        clamped);
    CRenderer::ms_nNoOfVisibleSuperLods = SanitizeRendererList64<MAX_VISIBLE_SUPERLOD_PTRS>(
        CRenderer::ms_aVisibleSuperLodPtrs,
        CRenderer::ms_nNoOfVisibleSuperLods,
        "super",
        removed,
        clamped);
    CRenderer::ms_nNoOfInVisibleEntities = SanitizeRendererList64<MAX_INVISIBLE_ENTITY_PTRS>(
        CRenderer::ms_aInVisibleEntityPtrs,
        CRenderer::ms_nNoOfInVisibleEntities,
        "inv",
        removed,
        clamped);

    if (IsSampConnectedGameplay64()) {
        ClearRendererList64<MAX_VISIBLE_LOD_PTRS>(
            CRenderer::ms_aVisibleLodPtrs,
            CRenderer::ms_nNoOfVisibleLods,
            removed);
        ClearRendererList64<MAX_VISIBLE_SUPERLOD_PTRS>(
            CRenderer::ms_aVisibleSuperLodPtrs,
            CRenderer::ms_nNoOfVisibleSuperLods,
            removed);
    }

    if (removed > 0 || clamped > 0) {
        static uint32_t s_lastRendererGuardLogTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastRendererGuardLogTick >= 700) {
            s_lastRendererGuardLogTick = now;
            FLog("[RENDER_GUARD64] stage=%s removed=%d clamped=%d counts={vis:%d,lod:%d,super:%d,inv:%d}",
                 stage ? stage : "?",
                 removed,
                 clamped,
                 CRenderer::ms_nNoOfVisibleEntities,
                 CRenderer::ms_nNoOfVisibleLods,
                 CRenderer::ms_nNoOfVisibleSuperLods,
                 CRenderer::ms_nNoOfInVisibleEntities);
        }
    }
}
#endif

struct NearbyAreaRequestStats64 {
    uint8_t areas[8]{};
    int areaCount = 0;
    int scanned = 0;
    int near = 0;
};

static void AddNearbyAreaRequest64(NearbyAreaRequestStats64& stats, uint8_t area)
{
#if !VER_x32
    if (area >= 32) {
        return;
    }

    for (int i = 0; i < stats.areaCount; ++i) {
        if (stats.areas[i] == area) {
            return;
        }
    }

    if (stats.areaCount < 8) {
        stats.areas[stats.areaCount++] = area;
    }
#endif
}

template <typename TPool>
static void CollectNearbyCollisionAreas64(
    TPool* pool,
    const CVector& sample,
    float radius,
    NearbyAreaRequestStats64& stats)
{
#if !VER_x32
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0 || stats.areaCount >= 8) {
        return;
    }

    const float radius2 = radius * radius;
    for (int32 i = 0; i < pool->m_nSize && stats.areaCount < 8; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity) {
            continue;
        }

        ++stats.scanned;
        const CVector entityPos = entity->GetPosition();
        const float dx = entityPos.x - sample.x;
        const float dy = entityPos.y - sample.y;
        const float dz = entityPos.z - sample.z;
        if (dx * dx + dy * dy + dz * dz > radius2) {
            continue;
        }

        ++stats.near;
        AddNearbyAreaRequest64(stats, static_cast<uint8_t>(entity->m_nAreaCode));
    }
#endif
}

static void RequestNearbyWorldCollisionAreas64(const CVector& sample, uint8_t playerArea)
{
#if !VER_x32
    static uint32_t s_lastLogTick = 0;

    NearbyAreaRequestStats64 stats{};
    AddNearbyAreaRequest64(stats, static_cast<uint8_t>(AREA_CODE_NORMAL_WORLD));
    AddNearbyAreaRequest64(stats, playerArea);
    CollectNearbyCollisionAreas64(GetBuildingPool(), sample, 160.0f, stats);
    CollectNearbyCollisionAreas64(GetObjectPoolGta(), sample, 160.0f, stats);
    CollectNearbyCollisionAreas64(GetDummyPool(), sample, 160.0f, stats);
    for (int i = 0; i < stats.areaCount; ++i) {
        Supremo::Streaming::WorldStreamRequest streamRequest{};
        streamRequest.areaCode = static_cast<int32>(stats.areas[i]);
        streamRequest.streamingFlags = STREAMING_DEFAULT;
        streamRequest.addModels = false;
        streamRequest.addLods = false;
        streamRequest.setCollisionRequired = false;
        streamRequest.addIpls = false;
        streamRequest.loadIpls = false;
        streamRequest.ensureIpls = false;
        streamRequest.loadSceneCollision = false;
        streamRequest.reason = "nearby-area-collision";
        Supremo::Streaming::PrepareWorldAtPoint(sample, streamRequest);
    }

    const uint32_t now = GetTickCount();
    if (stats.areaCount > 2 && now - s_lastLogTick >= 2500) {
        s_lastLogTick = now;
        FLog("[COL_AREAS64] pos=%.2f %.2f %.2f playerArea=%u areas=%u,%u,%u,%u,%u,%u,%u,%u scanned=%d near=%d",
             sample.x,
             sample.y,
             sample.z,
             static_cast<unsigned>(playerArea),
             stats.areaCount > 0 ? static_cast<unsigned>(stats.areas[0]) : 255u,
             stats.areaCount > 1 ? static_cast<unsigned>(stats.areas[1]) : 255u,
             stats.areaCount > 2 ? static_cast<unsigned>(stats.areas[2]) : 255u,
             stats.areaCount > 3 ? static_cast<unsigned>(stats.areas[3]) : 255u,
             stats.areaCount > 4 ? static_cast<unsigned>(stats.areas[4]) : 255u,
             stats.areaCount > 5 ? static_cast<unsigned>(stats.areas[5]) : 255u,
             stats.areaCount > 6 ? static_cast<unsigned>(stats.areas[6]) : 255u,
             stats.areaCount > 7 ? static_cast<unsigned>(stats.areas[7]) : 255u,
             stats.scanned,
             stats.near);
    }
#endif
}

static void NormalizeNearbyWorldObjectAreas64(const CVector& playerPos, uint8_t playerArea)
{
#if !VER_x32
    if (playerArea != static_cast<uint8_t>(AREA_CODE_NORMAL_WORLD)) {
        return;
    }

    CPool<CObjectGta, CCutsceneObject>* objectPool = GetObjectPoolGta();
    if (!objectPool || !objectPool->m_pObjects || !objectPool->m_byteMap || objectPool->m_nSize <= 0) {
        return;
    }

    constexpr float kRadius = 95.0f;
    const float radius2 = kRadius * kRadius;
    int scanned = 0;
    int changed = 0;
    int firstModel = -1;
    uint8_t firstArea = 0;
    CVector firstPos(0.0f, 0.0f, 0.0f);
    char firstName[22]{};

    for (int32 i = 0; i < objectPool->m_nSize; ++i) {
        if (objectPool->m_byteMap[i].bEmpty) {
            continue;
        }

        CEntityGTA* entity = static_cast<CEntityGTA*>(
            static_cast<CPool<CObjectGta, CCutsceneObject>::base_type*>(&objectPool->m_pObjects[i]));
        if (!entity || !entity->IsObject()) {
            continue;
        }

        const uint8_t entityArea = static_cast<uint8_t>(entity->m_nAreaCode);
        if (entityArea == static_cast<uint8_t>(AREA_CODE_NORMAL_WORLD)) {
            continue;
        }
        if (entityArea != static_cast<uint8_t>(AREA_CODE_13)) {
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        const float dx = entityPos.x - playerPos.x;
        const float dy = entityPos.y - playerPos.y;
        const float dz = entityPos.z - playerPos.z;
        if (dx * dx + dy * dy + dz * dz > radius2) {
            continue;
        }

        ++scanned;
        const int modelId = entity->m_nModelIndex;
        if (!Supremo::Streaming::IsValidResourceId(modelId)) {
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
        if (!modelInfo || !modelInfo->m_pColModel) {
            continue;
        }

        if (changed == 0) {
            firstModel = modelId;
            firstArea = entityArea;
            firstPos = entityPos;
            CopyModelName64(firstName, modelInfo);
        }

        entity->m_nAreaCode = AREA_CODE_NORMAL_WORLD;
        ++changed;
    }

    if (changed > 0) {
        static uint32_t s_lastLogTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastLogTick >= 2500) {
            s_lastLogTick = now;
            FLog("[COL_AREA_FIX64] changed=%d scanned=%d first=model:%d,%s area:%u pos=%.2f %.2f %.2f player=%.2f %.2f %.2f",
                 changed,
                 scanned,
                 firstModel,
                 firstName[0] ? firstName : "?",
                 static_cast<unsigned>(firstArea),
                 firstPos.x,
                 firstPos.y,
                 firstPos.z,
                 playerPos.x,
                 playerPos.y,
                 playerPos.z);
        }
    }
#else
    (void)playerPos;
    (void)playerArea;
#endif
}

struct WorldRepairStats64 {
    int near = 0;
    int requested = 0;
    int protectedEntities = 0;
    int visualCandidates = 0;
    int rwAttempts = 0;
    int rwRestored = 0;
    int failed = 0;
    int unhidden = 0;
    int skippedArea = 0;
    int loggedCandidates = 0;
};

static bool IsWorldEntityAreaActive64(CEntityGTA* entity, uint8_t playerArea)
{
#if !VER_x32
    if (!entity) {
        return false;
    }

    const uint8_t entityArea = static_cast<uint8_t>(entity->m_nAreaCode);
    return IsEntityAreaActiveForPlayer64(entityArea, playerArea);
#else
    (void)entity;
    (void)playerArea;
    return true;
#endif
}

static bool ShouldRepairWorldVisualEntity64(CEntityGTA* entity, CBaseModelInfo* modelInfo)
{
#if !VER_x32
    if (!entity || !modelInfo || IsLodWorldModel64(modelInfo)) {
        return false;
    }

    const int modelId = entity->m_nModelIndex;
    if (IsUnstableBreakableWorldProp64(modelId, modelInfo)) {
        return false;
    }

    const bool collisionBackedVisual =
        entity->m_bUsesCollision ||
        entity->m_bIsStaticWaitingForCollision ||
        modelInfo->m_pColModel != nullptr;

    return collisionBackedVisual ||
           ShouldRepairRenderableWorldProp64(modelInfo) ||
           IsLikelyRoadModel64(modelInfo) ||
           IsLikelyGroundSurfaceModel64(modelInfo) ||
           IsCriticalNearbySurfaceModel64(modelInfo) ||
           IsTargetedHighSurfaceModel64(modelInfo);
#else
    (void)entity;
    (void)modelInfo;
    return false;
#endif
}

static bool ShouldRestoreWorldEntityVisibility64(CEntityGTA* entity, CBaseModelInfo* modelInfo)
{
#if !VER_x32
    if (!entity || !modelInfo) {
        return false;
    }

    if (IsLodWorldModel64(modelInfo)) {
        return false;
    }

    return entity->m_bUsesCollision ||
           modelInfo->m_pColModel != nullptr ||
           IsLikelyRoadModel64(modelInfo) ||
           entity->IsBuilding() ||
           ShouldRepairWorldVisualEntity64(entity, modelInfo);
#else
    (void)entity;
    (void)modelInfo;
    return false;
#endif
}

template <typename TPool>
static void CollectWorldRwRepairCandidates64(
    const char* poolName,
    TPool* pool,
    const CVector& playerPos,
    float radius,
    uint8_t playerArea,
    CEntityGTA* candidates[],
    int& candidateCount,
    int maxCandidates,
    WorldRepairStats64& stats,
    bool logCandidates)
{
#if !VER_x32
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0) {
        return;
    }

    const float radius2 = radius * radius;
    for (int32 i = 0; i < pool->m_nSize; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity) {
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        const float dx = entityPos.x - playerPos.x;
        const float dy = entityPos.y - playerPos.y;
        const float dz = entityPos.z - playerPos.z;
        if (dx * dx + dy * dy + dz * dz > radius2) {
            continue;
        }

        ++stats.near;

        if (!IsWorldEntityAreaActive64(entity, playerArea)) {
            ++stats.skippedArea;
            continue;
        }

        const int modelId = entity->m_nModelIndex;
        if (!Supremo::Streaming::IsValidResourceId(modelId)) {
            continue;
        }
        if (IsTempVehicleComponentModel64(modelId)) {
            SuppressTempVehicleComponentCollision64(entity);
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
        if (!modelInfo) {
            continue;
        }

        if (IsUnstableBreakableWorldProp64(modelId, modelInfo)) {
            const bool modelLoaded = Supremo::Streaming::IsModelLoaded(modelId);
            const bool modelHasRw = modelInfo->m_pRwObject != nullptr;
            const bool entityHasRw = entity->m_pRwObject != nullptr;
            PreserveBreakableWorldPropCollision64(entity);
            if (entityHasRw && !entity->m_bIsVisible && ShouldRestoreWorldEntityVisibility64(entity, modelInfo)) {
                entity->m_bIsVisible = true;
                ++stats.unhidden;
            }
            if (!modelLoaded || !modelHasRw) {
                Supremo::Streaming::RequestModel(modelId, STREAMING_DEFAULT);
                ++stats.requested;
            }
            continue;
        }

        if (IsLodWorldModel64(modelInfo)) {
            continue;
        }

        const bool criticalBallysModel = IsCoreBallysCollisionModel64(modelId);
        const bool repairVisualEntity = ShouldRepairWorldVisualEntity64(entity, modelInfo);
        if (!criticalBallysModel && !repairVisualEntity) {
            continue;
        }

        const bool modelLoaded = Supremo::Streaming::IsModelLoaded(modelId);
        const bool modelHasRw = modelInfo->m_pRwObject != nullptr;
        const bool entityHasRw = entity->m_pRwObject != nullptr;

        if (criticalBallysModel) {
            Supremo::Streaming::MarkModelDontRemoveInLoadScene(modelId);
            entity->m_bDontStream = false;
            if (!entity->m_bIsVisible && ShouldRestoreWorldEntityVisibility64(entity, modelInfo)) {
                entity->m_bIsVisible = true;
                ++stats.unhidden;
            }

            if (!entity->m_bStreamingDontDelete) {
                entity->m_bStreamingDontDelete = true;
                ++stats.protectedEntities;
            }

            if (modelLoaded && modelHasRw && entityHasRw) {
                continue;
            }

            if (!modelLoaded || !modelHasRw) {
                Supremo::Streaming::RequestModel(modelId, STREAMING_DONTREMOVE_IN_LOADSCENE);
                ++stats.requested;
            }

            if (!entityHasRw && candidateCount < maxCandidates) {
                candidates[candidateCount++] = entity;
            }
            continue;
        }

        if (entityHasRw && !entity->m_bIsVisible && ShouldRestoreWorldEntityVisibility64(entity, modelInfo)) {
            entity->m_bIsVisible = true;
            ++stats.unhidden;
        }

        if (!modelLoaded || !modelHasRw) {
            Supremo::Streaming::RequestModel(modelId, STREAMING_DEFAULT);
            ++stats.requested;
        }

        if (!entityHasRw && candidateCount < maxCandidates) {
            candidates[candidateCount++] = entity;
            ++stats.visualCandidates;
            if (logCandidates && stats.loggedCandidates < 4) {
                char modelName[22]{};
                CopyModelName64(modelName, modelInfo);
                const CVector entityPos = entity->GetPosition();
                FLog("[WORLD_RW_CAND64] pool=%s slot=%d model=%d,%s pos=%.2f %.2f %.2f vis=%d flags=0x%08X protect=%d",
                     poolName ? poolName : "?",
                     i,
                     modelId,
                     modelName[0] ? modelName : "?",
                     entityPos.x,
                     entityPos.y,
                     entityPos.z,
                     entity->m_bIsVisible ? 1 : 0,
                     entity->m_nFlags,
                     entity->m_bStreamingDontDelete ? 1 : 0);
                ++stats.loggedCandidates;
            }
        }
    }
#endif
}

static void RepairWorldRwObjectsAroundPlayer64(const CVector& pos, uint8_t playerArea, bool renderPass)
{
#if !VER_x32
    static uint32_t s_lastRepairTick[2] = {};
    static uint32_t s_lastRepairLogTick[2] = {};
    const int passIndex = renderPass ? 1 : 0;

    const uint32_t now = GetTickCount();
    if (now - s_lastRepairTick[passIndex] < (renderPass ? 250u : 700u)) {
        return;
    }
    s_lastRepairTick[passIndex] = now;

    constexpr float kRepairRadius = 180.0f;
    constexpr int kMaxCandidates = 64;
    CEntityGTA* candidates[kMaxCandidates]{};
    int candidateCount = 0;
    WorldRepairStats64 stats{};
    const bool logCandidates =
        kArm64CollisionDeepDiagnosticsEnabled &&
        now - s_lastRepairLogTick[passIndex] >= 1800;

    CollectWorldRwRepairCandidates64("B", GetBuildingPool(), pos, kRepairRadius, playerArea, candidates, candidateCount, kMaxCandidates, stats, logCandidates);
    CollectWorldRwRepairCandidates64("O", GetObjectPoolGta(), pos, kRepairRadius, playerArea, candidates, candidateCount, kMaxCandidates, stats, logCandidates);

    if (stats.requested > 0) {
        Supremo::Streaming::LoadAllRequestedModels(false);
    }

    for (int i = 0; i < candidateCount; ++i) {
        CEntityGTA* entity = candidates[i];
        if (!entity || entity->m_pRwObject) {
            continue;
        }

        const int modelId = entity->m_nModelIndex;
        if (!Supremo::Streaming::IsValidResourceId(modelId) || !Supremo::Streaming::IsModelLoaded(modelId)) {
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
        if (!modelInfo || !modelInfo->m_pRwObject) {
            continue;
        }

        ++stats.rwAttempts;
        entity->CreateRwObject();
        entity->UpdateRW();
        entity->UpdateRwFrame();
        if (entity->m_pRwObject) {
            if (!entity->m_bIsVisible && ShouldRestoreWorldEntityVisibility64(entity, modelInfo)) {
                entity->m_bIsVisible = true;
                ++stats.unhidden;
            }
            ++stats.rwRestored;
        } else {
            ++stats.failed;
        }
    }

    if (kArm64CollisionRuntimeSummaryLogsEnabled &&
        (stats.requested || stats.visualCandidates || stats.rwRestored || stats.failed || stats.protectedEntities || stats.unhidden) &&
        now - s_lastRepairLogTick[passIndex] >= 1800) {
        s_lastRepairLogTick[passIndex] = now;
        FLog("[WORLD_REPAIR64] pass=%s pos=%.2f %.2f %.2f area=%u near=%d req=%d protect=%d unhide=%d areaSkip=%d candidates=%d visual=%d rwTry=%d rwOk=%d fail=%d mem=%zu/%zu",
             renderPass ? "pre" : "maintain",
             pos.x,
             pos.y,
             pos.z,
             static_cast<unsigned>(playerArea),
             stats.near,
             stats.requested,
             stats.protectedEntities,
             stats.unhidden,
             stats.skippedArea,
             candidateCount,
             stats.visualCandidates,
             stats.rwAttempts,
             stats.rwRestored,
             stats.failed,
             Supremo::Streaming::MemoryUsed(),
             Supremo::Streaming::MemoryAvailable());
    }
#endif
}

static void MaintainWorldObjectStreaming64()
{
#if !VER_x32
    if (!pGame || !pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
        return;
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
    CPlayerPed* localPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;
    if (!localPlayer || (!localPlayer->m_bIsActive && !localPlayer->IsClearedToSpawn()) || !localPed || !localPed->m_pPed) {
        return;
    }

    static uint32_t s_lastMaintainTick = 0;
    static uint32_t s_lastSceneRequestTick = 0;
    const uint32_t now = GetTickCount();
    const CVector playerPos = localPed->m_pPed->GetPosition();
    const CVector playerVel = localPed->m_pPed->GetMoveSpeed();
    const float speedSq = playerVel.x * playerVel.x + playerVel.y * playerVel.y + playerVel.z * playerVel.z;
    const uint32_t maintainIntervalMs = speedSq > 9.0f ? 420u : 650u;
    if (now - s_lastMaintainTick < maintainIntervalMs) {
        return;
    }
    s_lastMaintainTick = now;

    Supremo::Streaming::EnableStreamingIfDisabled();
    EnsureArm64StreamingMemoryBudget("maintain-world");
    CRenderer::ms_bRenderOutsideTunnels = true;
    CRenderer::m_loadingPriority = false;

    if (now - s_lastSceneRequestTick >= 900u || speedSq > 9.0f) {
        s_lastSceneRequestTick = now;
        Supremo::Streaming::RequestSceneObjectsAroundPlayer(playerPos, playerVel);
    }

    const uint8_t playerArea = NormalizeLocalExteriorArea64(localPed, playerPos, "maintain-world");
    SuppressNearbyTempVehicleComponents64(playerPos);
    RequestNearbyWorldCollisionAreas64(playerPos, playerArea);
    RequestMissingNearbyWorldCollision64(playerPos, playerArea, now, false);
    NormalizeNearbyWorldObjectAreas64(playerPos, playerArea);

    RepairWorldRwObjectsAroundPlayer64(
        playerPos,
        playerArea,
        false);

    if (kArm64CollisionDeepDiagnosticsEnabled) {
        LogStaticWorldPoolsAroundPlayer64(playerPos);
    }
#endif
}

#if !VER_x32
struct GroundProbe64 {
    bool hit = false;
    float z = 0.0f;
    int modelId = -1;
    uint8_t area = 0;
    uint8_t entityType = ENTITY_TYPE_NOTHING;
    bool usesCollision = false;
    char modelName[22]{};
};

static float Distance2DSquared64(const CVector& a, const CVector& b);

static GroundProbe64 ProbeGroundLine64(const CVector& pos, float startZ, float endZ)
{
    GroundProbe64 probe{};
    CColPoint col{};
    CEntityGTA* entity = nullptr;
    CVector start(pos.x, pos.y, startZ);
    CVector end(pos.x, pos.y, endZ);

    probe.hit = CWorld::ProcessLineOfSight(
        &start,
        &end,
        &col,
        &entity,
        true,
        false,
        false,
        true,
        true,
        false,
        false,
        false
    );

    if (!probe.hit) {
        return probe;
    }

    probe.z = col.m_vecPoint.z;
    if (entity) {
        probe.modelId = entity->m_nModelIndex;
        probe.area = static_cast<uint8_t>(entity->m_nAreaCode);
        probe.entityType = static_cast<uint8_t>(entity->GetType());
        probe.usesCollision = entity->m_bUsesCollision;
        CopyModelName64(probe.modelName, CModelInfo::GetModelInfo(probe.modelId));
    }
    return probe;
}

static GroundProbe64 ProbeVerticalLine64(const CVector& pos, float startZ, float endZ)
{
    GroundProbe64 probe{};
    CColPoint col{};
    CEntityGTA* entity = nullptr;
    CStoredCollPoly storedPoly{};
    CVector start(pos.x, pos.y, startZ);

    probe.hit = CWorld::ProcessVerticalLine(
        &start,
        endZ,
        &col,
        &entity,
        true,
        false,
        false,
        true,
        true,
        false,
        &storedPoly
    );

    if (!probe.hit) {
        return probe;
    }

    probe.z = col.m_vecPoint.z;
    if (entity) {
        probe.modelId = entity->m_nModelIndex;
        probe.area = static_cast<uint8_t>(entity->m_nAreaCode);
        probe.entityType = static_cast<uint8_t>(entity->GetType());
        probe.usesCollision = entity->m_bUsesCollision;
        CopyModelName64(probe.modelName, CModelInfo::GetModelInfo(probe.modelId));
    }
    return probe;
}

static bool IsInsideEntityColBounds64(CEntityGTA* entity, CColModel* colModel, const CVector& sample, float pad)
{
    if (!entity || !colModel) {
        return false;
    }

    const CVector entityPos = entity->GetPosition();
    const CVector min(
        entityPos.x + colModel->m_boundBox.m_vecMin.x - pad,
        entityPos.y + colModel->m_boundBox.m_vecMin.y - pad,
        entityPos.z + colModel->m_boundBox.m_vecMin.z - pad);
    const CVector max(
        entityPos.x + colModel->m_boundBox.m_vecMax.x + pad,
        entityPos.y + colModel->m_boundBox.m_vecMax.y + pad,
        entityPos.z + colModel->m_boundBox.m_vecMax.z + pad);

    return sample.x >= min.x && sample.x <= max.x &&
           sample.y >= min.y && sample.y <= max.y &&
           sample.z >= min.z && sample.z <= max.z;
}

static bool ProbeEntityVertical64(CEntityGTA* entity,
                                  CColModel* colModel,
                                  const CVector& sample,
                                  float startZ,
                                  float endZ,
                                  CColPoint& outCol,
                                  float& outMaxTouch)
{
    if (!entity || !colModel || !colModel->m_pColData || !CCollision__ProcessVerticalLine ||
        startZ <= endZ) {
        return false;
    }

    CColLine line{};
    line.m_vecStart = CVector(sample.x, sample.y, startZ);
    line.m_vecEnd = CVector(sample.x, sample.y, endZ);
    line.m_fStartSize = 0.0f;
    line.m_fEndSize = 0.0f;

    CStoredCollPoly stored{};
    outCol = {};
    outMaxTouch = startZ - endZ;
    CMatrix& matrix = entity->GetMatrix();
    return CCollision__ProcessVerticalLine(
        &line,
        &matrix,
        colModel,
        &outCol,
        &outMaxTouch,
        false,
        false,
        &stored);
}

template <typename TPool>
static void LogEntityVerticalProbePool64(const char* poolName,
                                         TPool* pool,
                                         const CVector& sample,
                                         float radius,
                                         int& logged)
{
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0 || logged >= 24) {
        return;
    }

    const float radius2 = radius * radius;
    for (int32 i = 0; i < pool->m_nSize && logged < 24; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity || !IsCoreBallysCollisionModel64(entity->m_nModelIndex)) {
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        if (DistanceSquared3D(entityPos, sample) > radius2) {
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(entity->m_nModelIndex);
        CColModel* colModel = modelInfo ? modelInfo->m_pColModel : nullptr;
        CCollisionData* colData = colModel ? colModel->m_pColData : nullptr;
        if (!colModel || !colData) {
            continue;
        }

        const bool inBounds = IsInsideEntityColBounds64(entity, colModel, sample, 0.35f);
        if (!inBounds && Distance2DSquared64(entityPos, sample) > 90.0f * 90.0f) {
            continue;
        }

        CColPoint entityCol{};
        float entityMaxTouch = 0.0f;
        const bool entityHit = ProbeEntityVertical64(
            entity,
            colModel,
            sample,
            27.0f,
            -35.0f,
            entityCol,
            entityMaxTouch);

        char modelName[22]{};
        CopyModelName64(modelName, modelInfo);

        const CVector colMin = colModel->m_boundBox.m_vecMin;
        const CVector colMax = colModel->m_boundBox.m_vecMax;
        const CVector worldMin(entityPos.x + colMin.x, entityPos.y + colMin.y, entityPos.z + colMin.z);
        const CVector worldMax(entityPos.x + colMax.x, entityPos.y + colMax.y, entityPos.z + colMax.z);

        FLog("[COL_ENTITY64] sample=%.2f %.2f pool=%s slot=%d model=%d,%s inBox=%d direct={hit:%d,z:%.2f,max:%.2f,surf:%u} tris=%u boxes=%u lines=%u uses=%d flags=0x%08X colFlags=0x%02X world={%.2f %.2f %.2f -> %.2f %.2f %.2f}",
             sample.x,
             sample.y,
             poolName ? poolName : "?",
             i,
             entity->m_nModelIndex,
             modelName[0] ? modelName : "?",
             inBounds ? 1 : 0,
             entityHit ? 1 : 0,
             entityHit ? entityCol.m_vecPoint.z : 0.0f,
             entityMaxTouch,
             entityHit ? static_cast<unsigned>(entityCol.m_nSurfaceTypeB) : 0,
             static_cast<unsigned>(colData->m_nNumTriangles),
             static_cast<unsigned>(colData->m_nNumBoxes),
             static_cast<unsigned>(colData->m_nNumLines),
             entity->m_bUsesCollision ? 1 : 0,
             entity->m_nFlags,
             static_cast<unsigned>(colModel->m_nFlags),
             worldMin.x,
             worldMin.y,
             worldMin.z,
             worldMax.x,
             worldMax.y,
             worldMax.z);
        ++logged;
    }
}

static float ClampFloat64(float value, float minValue, float maxValue)
{
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static CVector GetEntityColWorldCenter64(CEntityGTA* entity, CColModel* colModel)
{
    if (!entity || !colModel) {
        return CVector(0.0f, 0.0f, 0.0f);
    }

    const CVector entityPos = entity->GetPosition();
    return CVector(
        entityPos.x + (colModel->m_boundBox.m_vecMin.x + colModel->m_boundBox.m_vecMax.x) * 0.5f,
        entityPos.y + (colModel->m_boundBox.m_vecMin.y + colModel->m_boundBox.m_vecMax.y) * 0.5f,
        entityPos.z + (colModel->m_boundBox.m_vecMin.z + colModel->m_boundBox.m_vecMax.z) * 0.5f);
}

static CVector ClampPointToEntityColBounds64(CEntityGTA* entity, CColModel* colModel, const CVector& point)
{
    if (!entity || !colModel) {
        return point;
    }

    const CVector entityPos = entity->GetPosition();
    const CVector worldMin(
        entityPos.x + colModel->m_boundBox.m_vecMin.x,
        entityPos.y + colModel->m_boundBox.m_vecMin.y,
        entityPos.z + colModel->m_boundBox.m_vecMin.z);
    const CVector worldMax(
        entityPos.x + colModel->m_boundBox.m_vecMax.x,
        entityPos.y + colModel->m_boundBox.m_vecMax.y,
        entityPos.z + colModel->m_boundBox.m_vecMax.z);

    return CVector(
        ClampFloat64(point.x, worldMin.x, worldMax.x),
        ClampFloat64(point.y, worldMin.y, worldMax.y),
        ClampFloat64(point.z, worldMin.z, worldMax.z));
}

static void RequestCollisionSample64(const CVector& sample, uint8_t area)
{
    CVector streamSample(sample.x, sample.y, sample.z);
    const float nativeAtSample = CWorld::FindGroundZForCoord(sample.x, sample.y);
    if (std::isfinite(nativeAtSample) &&
        sample.z < 5.0f &&
        nativeAtSample > sample.z + 8.0f &&
        nativeAtSample < 3000.0f) {
        streamSample.z = nativeAtSample + 2.0f;
    } else if (streamSample.z < 18.0f) {
        streamSample.z = 18.0f;
    }

    Supremo::Streaming::WorldStreamRequest streamRequest{};
    streamRequest.areaCode = static_cast<int32>(area);
    streamRequest.streamingFlags = STREAMING_DEFAULT;
    streamRequest.reason = "collision-sample";
    Supremo::Streaming::PrepareWorldAtPoint(streamSample, streamRequest);
}

static void RetainWatchedColSlot64(int32 slot, int32 modelId, const char* modelName, const CVector& pos)
{
    static bool s_retainedColSlots64[256]{};
    Supremo::Collision::ColSlotInfo slotInfo{};
    if (slot <= 0 || slot >= 256 || !Supremo::Collision::ReadSlotInfo(slot, slotInfo) || slotInfo.empty) {
        return;
    }

    const uint16 beforeRef = slotInfo.refCount;
    const bool firstRetain = !s_retainedColSlots64[slot];
    if (firstRetain) {
        Supremo::Collision::AddRef(slot);
        s_retainedColSlots64[slot] = true;
    }

    const int32 colModelId = Supremo::Streaming::ColModelIdFromSlot(slot);
    if (Supremo::Streaming::IsValidResourceId(colModelId)) {
        Supremo::Streaming::MarkModelDontRemoveInLoadScene(colModelId);
        Supremo::Streaming::RequestModel(colModelId, STREAMING_DONTREMOVE_IN_LOADSCENE);
    }

    if (!firstRetain) {
        return;
    }

    Supremo::Collision::ColSlotInfo afterInfo{};
    if (!Supremo::Collision::ReadSlotInfo(slot, afterInfo) || afterInfo.empty) {
        afterInfo = slotInfo;
    }

    FLog("[COL_RETAIN64] slot=%d fields={0x%08X,0x%08X,0x%08X,0x%08X,0x%04X} model=%d,%s pos=%.2f %.2f %.2f ref=%u->%u active=%d required=%d colModelId=%d",
         slot,
         slotInfo.field10,
         slotInfo.field14,
         slotInfo.field18,
         slotInfo.field1C,
         static_cast<unsigned>(slotInfo.field20),
         modelId,
         modelName && modelName[0] ? modelName : "?",
         pos.x,
         pos.y,
         pos.z,
         static_cast<unsigned>(beforeRef),
         static_cast<unsigned>(afterInfo.refCount),
         afterInfo.active ? 1 : 0,
         afterInfo.required ? 1 : 0,
         colModelId);
}

static void LogColSlot64(const char* label, int32 slot)
{
    Supremo::Collision::ColSlotInfo slotInfo{};
    if (!Supremo::Collision::ReadSlotInfo(slot, slotInfo)) {
        FLog("[COL_SLOT64] label=%s slot=%d size=%d valid=0 onlyBB=%d",
             label ? label : "?",
             slot,
             slotInfo.poolSize,
             Supremo::Collision::OnlyBoundingBoxes() ? 1 : 0);
        return;
    }

    uint8_t loadState = 0;
    uint8_t imgId = 0;
    uint32_t cdPosn = 0;
    uint32_t cdSize = 0;
    int16_t nextOnCd = -1;
    const int32 colModelId = Supremo::Streaming::ColModelIdFromSlot(slot);
    if (Supremo::Streaming::IsValidResourceId(colModelId)) {
        const Supremo::Streaming::ModelStreamDiagnostic streamInfo =
            Supremo::Streaming::ReadModelDiagnostic(colModelId);
        loadState = streamInfo.loadState;
        imgId = streamInfo.imgId;
        cdPosn = streamInfo.cdPosn;
        cdSize = streamInfo.cdSize;
        nextOnCd = streamInfo.nextIndexOnCd;
    }

    FLog("[COL_SLOT64] label=%s slot=%d empty=%d onlyBB=%d area={%.2f %.2f %.2f %.2f} fields={0x%08X,0x%08X,0x%08X,0x%08X,0x%04X} models=%d-%d ref=%u active=%d required=%d proc=%d interior=%d stream={id:%d,state:%u,img:%u,pos:%u,size:%u,next:%d}",
         label ? label : "?",
         slot,
         slotInfo.empty ? 1 : 0,
         Supremo::Collision::OnlyBoundingBoxes() ? 1 : 0,
         slotInfo.empty ? 0.0f : slotInfo.areaLeft,
         slotInfo.empty ? 0.0f : slotInfo.areaBottom,
         slotInfo.empty ? 0.0f : slotInfo.areaRight,
         slotInfo.empty ? 0.0f : slotInfo.areaTop,
         slotInfo.empty ? 0u : slotInfo.field10,
         slotInfo.empty ? 0u : slotInfo.field14,
         slotInfo.empty ? 0u : slotInfo.field18,
         slotInfo.empty ? 0u : slotInfo.field1C,
         slotInfo.empty ? 0u : static_cast<unsigned>(slotInfo.field20),
         slotInfo.empty ? -1 : slotInfo.modelIdStart,
         slotInfo.empty ? -1 : slotInfo.modelIdEnd,
         slotInfo.empty ? 0u : static_cast<unsigned>(slotInfo.refCount),
         !slotInfo.empty && slotInfo.active ? 1 : 0,
         !slotInfo.empty && slotInfo.required ? 1 : 0,
         !slotInfo.empty && slotInfo.procedural ? 1 : 0,
         !slotInfo.empty && slotInfo.interior ? 1 : 0,
         colModelId,
         static_cast<unsigned>(loadState),
         static_cast<unsigned>(imgId),
         cdPosn,
         cdSize,
          static_cast<int>(nextOnCd));
}

struct AirportColSlots64 {
    bool initialized = false;
    int32 las1 = -1;
    int32 las2 = -1;
    int32 las3 = -1;
    int32 las4 = -1;
    int32 las5 = -1;
    int32 laxref = -1;
    int32 laxrefdocks = -1;
    int32 lawn1 = -1;
    int32 lawn3 = -1;
    int32 law1 = -1;
    int32 law2 = -1;
    int32 law3 = -1;
    int32 law4 = -1;
    int32 lae1 = -1;
    int32 lae2 = -1;
    int32 lae3 = -1;
    int32 lae4 = -1;
    int32 lae2_4 = -1;
    int32 lae2_5 = -1;
    int32 lan1 = -1;
    int32 lan2 = -1;
    int32 lan3 = -1;
    int32 lan2_1 = -1;
    int32 lan2_2 = -1;
    int32 lan2_3 = -1;
    int32 law2_1 = -1;
    int32 law2_2 = -1;
    int32 sfse1 = -1;
    int32 sfse2 = -1;
    int32 sfse3 = -1;
    int32 sfse5 = -1;
    int32 sfse6 = -1;
    int32 sfse8 = -1;
    int32 sfe5 = -1;
    int32 sfe6 = -1;
    int32 countryw7 = -1;
    int32 countryw8 = -1;
    int32 vegasw1 = -1;
    int32 vegasw2 = -1;
    int32 vegasw3 = -1;
    int32 vegasw4 = -1;
    int32 vegasw5 = -1;
    int32 vegasw6 = -1;
    int32 vegasw7 = -1;
    int32 vegasw8 = -1;
    int32 vegasw9 = -1;
};

static int32 FindColSlotByStreamingCd64(uint32_t cdPosn, uint32_t cdSize)
{
    const int32 poolSize = Supremo::Collision::PoolSize();
    if (poolSize <= 0) {
        return -1;
    }

    const int32 maxSlot = std::min<int32>(poolSize, 256);
    for (int32 slot = 1; slot < maxSlot; ++slot) {
        Supremo::Collision::ColSlotInfo slotInfo{};
        if (!Supremo::Collision::ReadSlotInfo(slot, slotInfo) || slotInfo.empty) {
            continue;
        }

        const int32 colModelId = Supremo::Streaming::ColModelIdFromSlot(slot);
        if (!Supremo::Streaming::IsValidResourceId(colModelId)) {
            continue;
        }

        const Supremo::Streaming::ModelStreamDiagnostic streamInfo =
            Supremo::Streaming::ReadModelDiagnostic(colModelId);
        if (streamInfo.cdPosn == cdPosn && streamInfo.cdSize == cdSize) {
            return slot;
        }
    }

    return -1;
}

static bool IsValidCachedColSlot64(int32 slot)
{
    return slot > 0 && slot < 256 && Supremo::Collision::IsValidSlot(slot);
}

static int32 ResolveColSlotByStreamingCd64(uint32_t cdPosn, uint32_t cdSize, int32 fallbackSlot)
{
    const int32 resolved = FindColSlotByStreamingCd64(cdPosn, cdSize);
    if (resolved > 0) {
        return resolved;
    }
    return IsValidCachedColSlot64(fallbackSlot) ? fallbackSlot : resolved;
}

static AirportColSlots64& GetAirportColSlots64()
{
    static AirportColSlots64 s_slots{};
    if (!s_slots.initialized ||
        !IsValidCachedColSlot64(s_slots.las1) ||
        !IsValidCachedColSlot64(s_slots.las2) ||
        !IsValidCachedColSlot64(s_slots.las3) ||
        !IsValidCachedColSlot64(s_slots.lawn1) ||
        !IsValidCachedColSlot64(s_slots.law1) ||
        !IsValidCachedColSlot64(s_slots.lae1) ||
        !IsValidCachedColSlot64(s_slots.lan2_1) ||
        !IsValidCachedColSlot64(s_slots.lan2_2) ||
        !IsValidCachedColSlot64(s_slots.lan2_3) ||
        !IsValidCachedColSlot64(s_slots.countryw7) ||
        !IsValidCachedColSlot64(s_slots.countryw8) ||
        !IsValidCachedColSlot64(s_slots.vegasw1) ||
        !IsValidCachedColSlot64(s_slots.vegasw3) ||
        !IsValidCachedColSlot64(s_slots.vegasw9)) {
        s_slots.initialized = true;
        s_slots.las1 = ResolveColSlotByStreamingCd64(116734, 17, 174);
        s_slots.las2 = ResolveColSlotByStreamingCd64(116751, 90, 175);
        s_slots.las3 = ResolveColSlotByStreamingCd64(116841, 17, 176);
        s_slots.las4 = ResolveColSlotByStreamingCd64(116858, 38, 177);
        s_slots.las5 = ResolveColSlotByStreamingCd64(116896, 47, 178);
        s_slots.laxref = Supremo::Collision::FindSlot("laxref");
        s_slots.laxrefdocks = Supremo::Collision::FindSlot("laxrefdocks");
        s_slots.lawn1 = FindColSlotByStreamingCd64(131864, 34);
        s_slots.lawn3 = FindColSlotByStreamingCd64(131937, 40);
        s_slots.law1 = FindColSlotByStreamingCd64(134177, 55);
        s_slots.law2 = FindColSlotByStreamingCd64(134232, 26);
        s_slots.law3 = FindColSlotByStreamingCd64(134258, 39);
        s_slots.law4 = FindColSlotByStreamingCd64(134297, 29);
        s_slots.lae1 = FindColSlotByStreamingCd64(125426, 66);
        s_slots.lae2 = FindColSlotByStreamingCd64(125492, 33);
        s_slots.lae3 = FindColSlotByStreamingCd64(125525, 51);
        s_slots.lae4 = FindColSlotByStreamingCd64(125576, 38);
        s_slots.lae2_4 = FindColSlotByStreamingCd64(122734, 68);
        s_slots.lae2_5 = FindColSlotByStreamingCd64(122802, 64);
        s_slots.lan1 = FindColSlotByStreamingCd64(127551, 44);
        s_slots.lan2 = FindColSlotByStreamingCd64(127595, 52);
        s_slots.lan3 = FindColSlotByStreamingCd64(127647, 64);
        s_slots.lan2_1 = FindColSlotByStreamingCd64(129447, 57);
        s_slots.lan2_2 = FindColSlotByStreamingCd64(129504, 66);
        s_slots.lan2_3 = FindColSlotByStreamingCd64(129570, 23);
        s_slots.law2_1 = FindColSlotByStreamingCd64(136418, 30);
        s_slots.law2_2 = FindColSlotByStreamingCd64(136448, 51);
        s_slots.sfse1 = FindColSlotByStreamingCd64(43806, 78);
        s_slots.sfse2 = FindColSlotByStreamingCd64(44248, 46);
        s_slots.sfse3 = FindColSlotByStreamingCd64(44294, 69);
        s_slots.sfse5 = FindColSlotByStreamingCd64(44366, 32);
        s_slots.sfse6 = FindColSlotByStreamingCd64(44398, 32);
        s_slots.sfse8 = FindColSlotByStreamingCd64(44455, 38);
        s_slots.sfe5 = FindColSlotByStreamingCd64(49888, 27);
        s_slots.sfe6 = FindColSlotByStreamingCd64(49915, 41);
        s_slots.countryw7 = FindColSlotByStreamingCd64(28583, 45);
        s_slots.countryw8 = FindColSlotByStreamingCd64(28628, 18);
        s_slots.vegasw1 = FindColSlotByStreamingCd64(76048, 41);
        s_slots.vegasw2 = FindColSlotByStreamingCd64(76089, 43);
        s_slots.vegasw3 = FindColSlotByStreamingCd64(76132, 55);
        s_slots.vegasw4 = FindColSlotByStreamingCd64(76187, 61);
        s_slots.vegasw5 = FindColSlotByStreamingCd64(76248, 29);
        s_slots.vegasw6 = FindColSlotByStreamingCd64(76277, 36);
        s_slots.vegasw7 = FindColSlotByStreamingCd64(76313, 34);
        s_slots.vegasw8 = FindColSlotByStreamingCd64(76347, 41);
        s_slots.vegasw9 = FindColSlotByStreamingCd64(76388, 35);
    }
    return s_slots;
}

static const char* GetKnownColSlotName64(int32 slot)
{
    AirportColSlots64& slots = GetAirportColSlots64();
    if (slot == slots.las1) return "las_1";
    if (slot == slots.las2) return "las_2";
    if (slot == slots.las3) return "las_3";
    if (slot == slots.las4) return "las_4";
    if (slot == slots.las5) return "las_5";
    if (slot == slots.lawn1) return "lawn_1";
    if (slot == slots.lawn3) return "lawn_3";
    if (slot == slots.law1) return "law_1";
    if (slot == slots.law2) return "law_2";
    if (slot == slots.law3) return "law_3";
    if (slot == slots.law4) return "law_4";
    if (slot == slots.lae1) return "lae_1";
    if (slot == slots.lae2) return "lae_2";
    if (slot == slots.lae3) return "lae_3";
    if (slot == slots.lae4) return "lae_4";
    if (slot == slots.lae2_4) return "lae2_4";
    if (slot == slots.lae2_5) return "lae2_5";
    if (slot == slots.lan1) return "lan_1";
    if (slot == slots.lan2) return "lan_2";
    if (slot == slots.lan3) return "lan_3";
    if (slot == slots.lan2_1) return "lan2_1";
    if (slot == slots.lan2_2) return "lan2_2";
    if (slot == slots.lan2_3) return "lan2_3";
    if (slot == slots.law2_1) return "law2_1";
    if (slot == slots.law2_2) return "law2_2";
    if (slot == slots.sfse1) return "sfse_1";
    if (slot == slots.sfse2) return "sfse_2";
    if (slot == slots.sfse3) return "sfse_3";
    if (slot == slots.sfse5) return "sfse_5";
    if (slot == slots.sfse6) return "sfse_6";
    if (slot == slots.sfse8) return "sfse_8";
    if (slot == slots.sfe5) return "sfe_5";
    if (slot == slots.sfe6) return "sfe_6";
    if (slot == slots.countryw7) return "countryw_7";
    if (slot == slots.countryw8) return "countryw_8";
    if (slot == slots.vegasw1) return "vegasw_1";
    if (slot == slots.vegasw2) return "vegasw_2";
    if (slot == slots.vegasw3) return "vegasw_3";
    if (slot == slots.vegasw4) return "vegasw_4";
    if (slot == slots.vegasw5) return "vegasw_5";
    if (slot == slots.vegasw6) return "vegasw_6";
    if (slot == slots.vegasw7) return "vegasw_7";
    if (slot == slots.vegasw8) return "vegasw_8";
    if (slot == slots.vegasw9) return "vegasw_9";
    return nullptr;
}

static bool IsVegasWestKnownColSlot64(int32 slot)
{
    AirportColSlots64& slots = GetAirportColSlots64();
    return slot == slots.vegasw1 ||
           slot == slots.vegasw2 ||
           slot == slots.vegasw3 ||
           slot == slots.vegasw4 ||
           slot == slots.vegasw5 ||
           slot == slots.vegasw6 ||
           slot == slots.vegasw7 ||
           slot == slots.vegasw8 ||
           slot == slots.vegasw9;
}

static void IncludeKnownModelIndexesForColSlot64(int32 slot)
{
    AirportColSlots64& slots = GetAirportColSlots64();
    const int32* modelIds = nullptr;
    int count = 0;

    static const int32 kCountryw7Models[] = {
        17088, // cuntwland13b
        17137, // cuntwland72b
        17170, // cuntwroad13
        17172, // cuntwroad14
        17462, // xxxxxxxxa
        17468  // xxcliffx
    };
    static const int32 kLas1Models[] = {
        4822, 4841, 4844, 4847, 4862, 4864, 4867, 4890, 4990,
        4995, 4997, 4998, 5001, 5002, 5003, 5004, 5005, 5031,
        5038, 5044, 5070, 5071, 5072, 5073, 5089
    };
    static const int32 kLas2Models[] = {
        4806, // BTOLAND8_LAS
        4807, // LAroads_20gh_LAs
        4808, // LAroadss_30_LAs
        4817, // TRNTRK7_LAS
        4818, // TRNTRK8_LAS
        4819, // TRNTRK5_LAS
        4820, // BTOLAND1_LAS
        4821, // BTOLAND2_LAS
        4827, // LAroads_20ghi_LAs
        4829, // lasairprt4
        4830, // lasairprt3
        4831, // airpurt2_las
        4832, // airtwer_Las
        4837, // LApedhusrea_LAs
        4846, // LAcityped1_LAs
        4848, // sanpedbeaut
        4849, // snpdmshfnc3_LAS
        4850, // snpedshpblk07
        4853, // traincano_LAS
        4857, // snpedmtsp1_LAS
        4858, // snpedland1_LAS
        4859, // snpedland2_LAS
        4860, // unionstwar_LAS
        4861, // snpedhuair2_LAS
        4868, // LAroads_23_LAs
        4869, // lasrnway8_LAS
        4872, // LAroads_042e_LAs
        4873, // unionstwarc2_LAS
        4874, // Helipad1_las
        4876, // hillpalos08_LAs
        4881, // uninstps_LAS01
        4882, // lasbrid1_LAS
        4886, // gngspwnhus1_LAS
        4891, // billboard_LAS
        4892, // kbsgarage2_LAS
        4894, // dwntwnbit1b_LAS
        4895, // lstrud_LAS
        4981, // snpedteew1_LAS
        4982, // snpedteew3_LAS
        4983, // snpedteew1vv_LAS
        4984, // snpedteew3gt_LAS
        4991, // lasairprterm1_LAS
        4992, // airplants_LAS
        4993, // airplnt2_LAS
        4994, // airbillb_LAS
        4996, // airsinage2_LAS
        4999, // airsinage6_LAS
        5000, // airsinage5_LAS
        5006, // airprtwlkto2_LAS
        5007, // lasrunwall3_LAS
        5009, // lasrnway7_LAS
        5013, // LAroakt1_30_LAs
        5016, // snpdPESS1_LAS
        5017, // lastripx1_LAS
        5020, // mul_LAS
        5024, // snpedtee_LAS
        5025, // snpedtedc_LAS
        5026, // lstrudct1_LAS
        5028, // obcity1ct1_LAS
        5033, // unmainstat_LAS
        5034, // lasairprtcut4
        5036, // BTOLAND1ct_LAS
        5040, // unionliq_LAS
        5042, // bombshop_LAs
        5043, // bombdoor_LAs
        5052, // BTOROAD1vb_LAS
        5056, // modLAS
        5059, // lanitewin3_LAS
        5060, // crlsafhus_LAS
        5061, // lascarl
        5064, // TRNTRK5z_LAS
        5066, // mondoshave_LAS
        5069, // ctscene1_las
        5074, // sjmctfnce5_las
        5077, // sjmctfnce8_las
        5078, // ctscene2_las
        5080, // sjmbarct2_LAS
        5083, // alphbrk1_las
        5084, // alphbrk2_las
        5086, // alphbrk3_las
        5087, // alphbrk4_las
        5088  // alphbrk5_las
    };
    static const int32 kLas3Models[] = {
        4814, // clifftest09
        4815, // clifftestgrnd2
        4816, // ROCKLIFF1_LAS
        4839, // bchcostrd3_LAS
        4840, // bchcostrd4_LAS
        4842, // Beach1_LAs0fg
        4843, // Beach1_LAs0fhy
        4855, // lasundrairprt1
        4856, // lasundrairprt3
        4863, // airtun1_LAS
        4865, // lasrnway2_LAS
        4866, // lasrnway1_LAS
        4871, // airpurt2bx_las
        4883, // bchcostair_LAS
        5030, // lasrunwall1ct_LAS
        5032, // las_runsigns_LAS
        5046, // bchcostrd4fuk_LAS
        5068, // airctsjm1_las
        5075, // sjmctfnce6_las
        5076  // sjmctfnce7_las
    };
    static const int32 kLas4Models[] = {
        4809, // LAroads_05_LAs
        4810, // hillpalos04_LAs
        4811, // clifftest02
        4812, // clifftest05
        4813, // clifftest07
        4824, // lasgrifsteps2
        4825, // griffithoblas
        4826, // grifftop2
        4845, // hillpalos02_LAs
        4851, // hillpalos01_LAs
        4875, // hillpalos06_LAs
        4877, // dwntwnbit4_LAS
        4888, // dwntwnbit3_LAS
        4896, // clifftest12
        4897, // Beach1a1_LAs
        4898, // clifftestgrnd
        4986, // odfwer_LAS
        4987, // LODfftop03
        4988, // lasbillbrd1_las
        5021, // LAroadsbrk_05_LAs
        5023, // grifovrhang2_LAS
        5057, // lanitewin1_LAS
        5062, // hillpawfnce_LAs
        5081, // rdcrashbar1_LAs
        5082  // rdcrashbar2_LAs
    };
    static const int32 kLas5Models[] = {
        4823, // lasgrifroad
        4828, // lasairprt5
        4833, // airpurtder_las
        4834, // airoad1d_LAS
        4835, // airoad1b_LAS
        4836, // LAroadsx_04_LAs
        4838, // airpurtderfr_las
        4852, // hillpalos03_LAs
        4854, // lasundrairprt2
        4870, // airpurt2ax_las
        4878, // obcity1_LAS
        4879, // hillpaloswal1_LAs
        4880, // dwntwnbit2_LAS
        4884, // lastranentun1_LAS
        4885, // lastranentun4_LAS
        4887, // dwntwnbit1_LAS
        4889, // dwntwnbit2b_LAS
        4985, // Cylinder03
        5051, // airobarsjm_LAS
        5058, // lanitewin2_LAS
        5079  // sjmbarct1_LAS
    };
    static const int32 kLawn1Models[] = {
        5794, // road_lawn06
        5808, // road_lawn39
        5809, // lawngrndaa
        5859, // road_lawn24
        5994, // road_lawn26
        5995  // road_lawn04
    };
    static const int32 kLaw1Models[] = {
        6054, // lawroads_law02
        6114, // lawroads_law08
        6127, // lawroads_law21
        6129  // lawroads_law23
    };
    static const int32 kLaw2Models[] = {
        6122, // lawroads_law16
        6233, // canal_floor
        6236  // canal_floor3
    };
    static const int32 kLaw3Models[] = {
        6094, // bevgrnd03b_law
        6234  // canal_floor2
    };
    static const int32 kLaw4Models[] = {
        6040, // wilshire7_law
        6120, // lawroads_law14
        6213, // venlaw_grnd
        6217  // law_vengrnd
    };
    static const int32 kLae1Models[] = {
        5391, // laeroad01
        5472, // frecrsbrid_LAE
        5497, // laeroad32
        5510, // laeroad45
        5528  // laeroadct43
    };
    static const int32 kLae4Models[] = {
        5469, // laeRoads11Tr
        5504  // laeroad39
    };
    static const int32 kLae2_4Models[] = {
        17513, // lae2_ground04
        17597, // Lae2_roads03
        17610, // Lae2_roads15
        17612, // Lae2_roads88
        17614, // Lae2_landHUB02
        17620, // Lae2_landHUB01
        17621, // Lae2_roads17
        17639, // Lae2_roads31
        17656, // Lae2_roads50
        17920, // Lae2_roads49
        17972, // grnd_alpha2
        17978  // grnd_alpha6
    };
    static const int32 kLae2_5Models[] = {
        17625, // Lae2_roads21
        17627  // Lae2_roads23
    };
    static const int32 kLan2_1Models[] = {
        4556, // sky4plaz1_LAn
        4557, // road10_LAn2
        4558, // LacmEntr1_LAn
        4559, // LacmaBase1_LAn
        4560, // LacmCanop1_LAn
        4564, // LAskyscrap2_LAn
        4567, // road07_LAn2
        4573, // stolenbuilds12
        4574, // stolenbuilds13
        4576, // lan2newbuild1
        4587, // skyscrapn203
        4589, // road15_LAn2
        4590, // grasspatchlan2
        4591, // lan2shit03
        4592, // lan2shit04
        4593, // lan2buildblk01
        4594, // lan2buildblk02
        4598, // crprkblok2_LAN2
        4599, // csp2GM_LAN2
        4602, // LAskyscrap4_LAn
        4603, // sky4plaz2_LAn
        4604, // build4plaz_LAn2
        4605, // skyscrapn203_gls
        4636, // cparkgmaumk_LAN
        4639, // paypark_lan02
        4640, // paypark_lan03
        4641, // paypark_lan04
        4643, // LAplaza2b_LAn2
        4646, // road13_LAn2
        4653, // Freeway7_LAn2
        4654, // road09_LAn2
        4656, // Freeway1_LAn2
        4658, // Freeway2_LAn2
        4682, // LAdtbuild3_LAn2
        4684, // LAalley1_LAn2
        4691, // csp3GM_LAN2
        4692, // Freeway9_LAn2
        4695, // Freeway11_LAn2
        4697, // crprkblok1_LAN2
        4700, // cpark01_LAN2
        4701, // cpark02_LAN2
        4702, // cpark03_LAN2
        4710, // road08_LAn2
        4714, // Lacmaalphas1_LAn
        4717, // LTSLAsky3_LAn2
        4718, // gm_build4_LAn2
        4722, // LTSLAsky3b_LAn2
        4729, // billbrdlan2_01
        4730, // billbrdlan2_03
        4731, // billbrdlan2_05
        4732, // billbrdlan2_06
        4739, // LTSLAbuild1_LAn2
        4744, // LTSLAbuild5_LAn2
        4747, // LTSLAsky8_LAn2
        4748, // LTSLAbuild7_LAn2
        4749, // LTSLAbuild8_LAn2
        4750  // LTSLAbuild9_LAn2
    };
    static const int32 kLan2_2Models[] = {
        4550, // LibrTow1_LAn
        4551, // LAriverSec2_LAn
        4552, // amubloksun1_LAn
        4553, // road12_LAn2
        4554, // LibBase1_LAn
        4555, // figfree4_LAn
        4562, // LAplaza2_LAn
        4563, // LAskyscrap1_LAn
        4565, // bunksteps1_LAn
        4568, // ground01_LAn2
        4570, // stolenbuilds08
        4571, // stolenbuilds09
        4572, // stolenbuilds11
        4575, // fireescapes1_lan2
        4584, // halgroundlan2
        4586, // skyscrapn201
        4588, // roofshitlan2
        4595, // cpark05_LAN2
        4601, // LAn2_gm1
        4638, // paypark_lan01
        4644, // road06_LAn2
        4645, // road14_LAn2
        4647, // road11_LAn2
        4648, // road05_LAn2
        4650, // road02_LAn2
        4652, // road04_LAn2
        4662, // Freeway4_LAn2
        4664, // Freeway5_LAn2
        4679, // Freeway8_LAn2
        4681, // LAdtbuild6_LAn2
        4683, // LAdtbuild2_LAn2
        4685, // LAalley2_LAn2
        4708, // LAdtbuild1_LAn2
        4711, // amublokalpha_LAn2
        4712, // LibPlaza1_LAn
        4715, // LTSLAsky1_LAn2
        4716, // LTSLAsky2_LAn2
        4720, // LTSLAsky1b_LAn
        4721, // LTSLAsky2b_LAn2
        4723, // LTSLAsky4_LAn2
        4724, // librarywall_lan2
        4725, // LTSLAsky6_LAn2
        4726, // libtwrhelipd_LAn2
        4727, // libtwrhelipda_LAn2
        4733, // billbrdlan2_07
        4735, // billbrdlan2_09
        4736, // billbrdlan2_10
        4737, // fireescapes3_lan2
        4738, // fireescapes2_lan2
        4740, // LTSLAbuild2_LAn2
        4741, // LTSLAbuild3_LAn2
        4745, // LTSLAbuild6_LAn2
        4746, // LTSLAsky7_LAn2
        4751, // LTSLAbuild10_LAn2
        4752  // LTSLAbuild11_LAn2
    };
    static const int32 kLan2_3Models[] = {
        4569, // stolenbuilds05
        4585, // towerlan2
        4596, // cspGM_LAN2
        4597, // crprkblok4_LAN2
        4600, // LAdtbuild10_LAn
        4637, // cpark_muck_lan2
        4642, // paypark_lan
        4649, // road01_LAn2
        4651, // road03_LAn2
        4660, // Freeway3_LAn2
        4666, // Freeway6_LAn2
        4690, // skyscrapn202
        4694, // Freeway10_LAn2
        4703, // cpark04_LAN2
        4734, // billbrdlan2_08
        4742, // LTSLAbuild4_LAn2
        4743  // LTSLAsky5_LAn2
    };
    static const int32 kLaw2_1Models[] = {
        6329, // Roads27_LAw2
        6333, // Roads25_LAw2
        6389  // SanClift01_LAw2
    };
    static const int32 kLaw2_2Models[] = {
        6290, // RailTunn02_LAw2
        6488, // countclub02_LAw2
        6507  // Roads09_LAw2
    };
    static const int32 kSfse1Models[] = {
        10767, // Airport_11_SFSe
        10788, // aircarpark_11_SFSe
        10819, // airprtgnd_05_SFSe
        11367, // airprtgnd_ct_SFSe
        11383, // jjct02
        11385  // ctscene2_sfse
    };
    static const int32 kSfse2Models[] = {
        10817, // airprtgnd_03_SFSe
        10841  // drydock1_SFSe01
    };
    static const int32 kSfse3Models[] = {
        10779, // aircarpark_06_SFSe
        10783, // aircarpark_03_SFSe
        10816  // airprtgnd_01_SFSe
    };
    static const int32 kSfse5Models[] = {
        10761 // Airport_08_SFSe
    };
    static const int32 kSfse6Models[] = {
        10755, // Airport_02_SFSe
        10756, // Airport_03_SFSe
        10758, // Airport_05_SFSe
        10760, // Airport_07_SFSe
        10762  // Airport_09_SFSe
    };
    static const int32 kSfse8Models[] = {
        11283 // Airport_14B_SFSe
    };
    static const int32 kSfe5Models[] = {
        10013, // vicstuff_sfe17
        10017, // bigvic_a1
        10018, // tunnel_sfe
        10020, // vicstuff_sfe22
        10053, // fishwarf20_sfe
        10054, // fishwarf24_sfe
        10055, // fishwarf21_sfe
        10076, // road13_sfe
        10084, // fishwarf13_sfe
        10086, // aprtmnts03_sfe
        10113, // road19_sfe
        10114, // road20_sfe
        10151  // bigvicgrnd_sfe
    };
    static const int32 kSfe6Models[] = {
        9930, // nicepark_sfe
        9931, // church_sfe
        9950, // pier2_sfe
        9954, // pier69_sfe3
        9958, // submarr_sfe
        10011, // carspaces_sfe14
        10060, // aprtmnts01_sfe
        10061, // aprtmntrailgs01_SFe
        10062, // aprtmntrailgs03_SFe
        10063, // aprtmnts02_sfe
        10064, // aprtmntrailgs02_SFe
        10066, // road02_sfe
        10074, // road12_sfe
        10080, // fishwarf10_sfe
        10083, // backalleys1_sfe
        10087, // landsl01_sfe
        10101, // vicstuff_sfe67
        10115, // road21_sfe
        10116, // road22_sfe
        10120, // road26_sfe
        10123, // road29_sfe
        10147, // nitelites_sfe15
        10233, // carspaces_sfe15
        10273, // churchgr_sfe
        10274, // churchgr2_sfe
        10308  // yet_another_sfe2
    };
    static const int32 kVegasw1Models[] = {
        7437, // vegasNroad25
        7462  // vegasNland11
    };
    static const int32 kVegasw3Models[] = {
        7429, // vegasNroad04
        7431, // vegasNroad06
        7435, // vegasNroad15
        7467, // vegasNland16
        7474, // vegasNland23
        7482, // vegasNroad49
        7484, // vegasNroad51
        7550  // vegasNroad21
    };
    static const int32 kVegasw9Models[] = {
        7468 // vegasNland17
    };
    static const int32 kVegasw7Models[] = {
        7483, // vegasNroad50
        7517  // vgnwreland1
    };

    if (slot == slots.countryw7) {
        modelIds = kCountryw7Models;
        count = static_cast<int>(sizeof(kCountryw7Models) / sizeof(kCountryw7Models[0]));
    } else if (slot == slots.las1) {
        modelIds = kLas1Models;
        count = static_cast<int>(sizeof(kLas1Models) / sizeof(kLas1Models[0]));
    } else if (slot == slots.las2) {
        modelIds = kLas2Models;
        count = static_cast<int>(sizeof(kLas2Models) / sizeof(kLas2Models[0]));
    } else if (slot == slots.las3) {
        modelIds = kLas3Models;
        count = static_cast<int>(sizeof(kLas3Models) / sizeof(kLas3Models[0]));
    } else if (slot == slots.las4) {
        modelIds = kLas4Models;
        count = static_cast<int>(sizeof(kLas4Models) / sizeof(kLas4Models[0]));
    } else if (slot == slots.las5) {
        modelIds = kLas5Models;
        count = static_cast<int>(sizeof(kLas5Models) / sizeof(kLas5Models[0]));
    } else if (slot == slots.lawn1) {
        modelIds = kLawn1Models;
        count = static_cast<int>(sizeof(kLawn1Models) / sizeof(kLawn1Models[0]));
    } else if (slot == slots.law1) {
        modelIds = kLaw1Models;
        count = static_cast<int>(sizeof(kLaw1Models) / sizeof(kLaw1Models[0]));
    } else if (slot == slots.law2) {
        modelIds = kLaw2Models;
        count = static_cast<int>(sizeof(kLaw2Models) / sizeof(kLaw2Models[0]));
    } else if (slot == slots.law3) {
        modelIds = kLaw3Models;
        count = static_cast<int>(sizeof(kLaw3Models) / sizeof(kLaw3Models[0]));
    } else if (slot == slots.law4) {
        modelIds = kLaw4Models;
        count = static_cast<int>(sizeof(kLaw4Models) / sizeof(kLaw4Models[0]));
    } else if (slot == slots.lae1) {
        modelIds = kLae1Models;
        count = static_cast<int>(sizeof(kLae1Models) / sizeof(kLae1Models[0]));
    } else if (slot == slots.lae4) {
        modelIds = kLae4Models;
        count = static_cast<int>(sizeof(kLae4Models) / sizeof(kLae4Models[0]));
    } else if (slot == slots.lae2_4) {
        modelIds = kLae2_4Models;
        count = static_cast<int>(sizeof(kLae2_4Models) / sizeof(kLae2_4Models[0]));
    } else if (slot == slots.lae2_5) {
        modelIds = kLae2_5Models;
        count = static_cast<int>(sizeof(kLae2_5Models) / sizeof(kLae2_5Models[0]));
    } else if (slot == slots.lan2_1) {
        modelIds = kLan2_1Models;
        count = static_cast<int>(sizeof(kLan2_1Models) / sizeof(kLan2_1Models[0]));
    } else if (slot == slots.lan2_2) {
        modelIds = kLan2_2Models;
        count = static_cast<int>(sizeof(kLan2_2Models) / sizeof(kLan2_2Models[0]));
    } else if (slot == slots.lan2_3) {
        modelIds = kLan2_3Models;
        count = static_cast<int>(sizeof(kLan2_3Models) / sizeof(kLan2_3Models[0]));
    } else if (slot == slots.law2_1) {
        modelIds = kLaw2_1Models;
        count = static_cast<int>(sizeof(kLaw2_1Models) / sizeof(kLaw2_1Models[0]));
    } else if (slot == slots.law2_2) {
        modelIds = kLaw2_2Models;
        count = static_cast<int>(sizeof(kLaw2_2Models) / sizeof(kLaw2_2Models[0]));
    } else if (slot == slots.sfse1) {
        modelIds = kSfse1Models;
        count = static_cast<int>(sizeof(kSfse1Models) / sizeof(kSfse1Models[0]));
    } else if (slot == slots.sfse2) {
        modelIds = kSfse2Models;
        count = static_cast<int>(sizeof(kSfse2Models) / sizeof(kSfse2Models[0]));
    } else if (slot == slots.sfse3) {
        modelIds = kSfse3Models;
        count = static_cast<int>(sizeof(kSfse3Models) / sizeof(kSfse3Models[0]));
    } else if (slot == slots.sfse5) {
        modelIds = kSfse5Models;
        count = static_cast<int>(sizeof(kSfse5Models) / sizeof(kSfse5Models[0]));
    } else if (slot == slots.sfse6) {
        modelIds = kSfse6Models;
        count = static_cast<int>(sizeof(kSfse6Models) / sizeof(kSfse6Models[0]));
    } else if (slot == slots.sfse8) {
        modelIds = kSfse8Models;
        count = static_cast<int>(sizeof(kSfse8Models) / sizeof(kSfse8Models[0]));
    } else if (slot == slots.sfe5) {
        modelIds = kSfe5Models;
        count = static_cast<int>(sizeof(kSfe5Models) / sizeof(kSfe5Models[0]));
    } else if (slot == slots.sfe6) {
        modelIds = kSfe6Models;
        count = static_cast<int>(sizeof(kSfe6Models) / sizeof(kSfe6Models[0]));
    } else if (slot == slots.vegasw1) {
        modelIds = kVegasw1Models;
        count = static_cast<int>(sizeof(kVegasw1Models) / sizeof(kVegasw1Models[0]));
    } else if (slot == slots.vegasw3) {
        modelIds = kVegasw3Models;
        count = static_cast<int>(sizeof(kVegasw3Models) / sizeof(kVegasw3Models[0]));
    } else if (slot == slots.vegasw7) {
        modelIds = kVegasw7Models;
        count = static_cast<int>(sizeof(kVegasw7Models) / sizeof(kVegasw7Models[0]));
    } else if (slot == slots.vegasw9) {
        modelIds = kVegasw9Models;
        count = static_cast<int>(sizeof(kVegasw9Models) / sizeof(kVegasw9Models[0]));
    }

    for (int i = 0; i < count; ++i) {
        const int32 modelId = modelIds[i];
        if (Supremo::Streaming::IsValidResourceId(modelId)) {
            Supremo::Collision::IncludeModelIndex(slot, modelId);
        }
    }
}

static void LogCollisionStoreSlots64(const CVector& pos, uint32_t now)
{
    static uint32_t s_lastColSlotLogTick = 0;
    static const CVector kBallysSpawn64(1958.3783f, 1343.1572f, 15.3746f);
    static const CVector kVegasWestColProbe64(1637.391f, 2155.0f, 17.82f);

    const bool nearBallys = Distance2DSquared64(pos, kBallysSpawn64) <= 125.0f * 125.0f;
    const bool nearVegasWest = Distance2DSquared64(pos, kVegasWestColProbe64) <= 180.0f * 180.0f;
    if ((!nearBallys && !nearVegasWest) ||
        now - s_lastColSlotLogTick < 5000) {
        return;
    }
    s_lastColSlotLogTick = now;

    const int32 vegasS = Supremo::Collision::FindSlot("vegasS");
    const int32 vegasE = Supremo::Collision::FindSlot("vegasE");
    const int32 vegasW = Supremo::Collision::FindSlot("vegasW");
    const int32 vegasN = Supremo::Collision::FindSlot("vegasN");
    const int32 vegaxref = Supremo::Collision::FindSlot("vegaxref");
    const int32 ballys01 = Supremo::Collision::FindSlot("ballys01");
    const int32 vgseroads = Supremo::Collision::FindSlot("vgseroads");

    const bool loadedPlayer = Supremo::Collision::HasCollisionLoaded(pos, AREA_CODE_NORMAL_WORLD);
    FLog("[COL_SLOT_FIND64] pos=%.2f %.2f %.2f onlyBB=%d loaded=%d vegas={S:%d,E:%d,W:%d,N:%d,xref:%d} names={ballys01:%d,vgseroads:%d}",
         pos.x,
         pos.y,
         pos.z,
         Supremo::Collision::OnlyBoundingBoxes() ? 1 : 0,
         loadedPlayer ? 1 : 0,
         vegasS,
         vegasE,
         vegasW,
         vegasN,
         vegaxref,
         ballys01,
         vgseroads);

    LogColSlot64("vegasS", vegasS);
    LogColSlot64("vegasE", vegasE);
    LogColSlot64("vegaxref", vegaxref);
    LogColSlot64("fixed153", 153);
    LogColSlot64("fixed154", 154);
    LogColSlot64("fixed165", 165);
    if (nearVegasWest) {
        AirportColSlots64& slots = GetAirportColSlots64();
        LogColSlot64("vegasw_1", slots.vegasw1);
        LogColSlot64("vegasw_2", slots.vegasw2);
        LogColSlot64("vegasw_3", slots.vegasw3);
        LogColSlot64("vegasw_4", slots.vegasw4);
        LogColSlot64("vegasw_5", slots.vegasw5);
        LogColSlot64("vegasw_6", slots.vegasw6);
        LogColSlot64("vegasw_7", slots.vegasw7);
        LogColSlot64("vegasw_8", slots.vegasw8);
        LogColSlot64("vegasw_9", slots.vegasw9);
    }
}

static void LogAirportColSlots64(const CVector& pos, uint32_t now)
{
    static uint32_t s_lastAirportColSlotLogTick = 0;
    static const CVector kAirportSpawn64(1744.0f, -2494.0f, 14.0f);

    if (Distance2DSquared64(pos, kAirportSpawn64) > 380.0f * 380.0f ||
        now - s_lastAirportColSlotLogTick < 5000) {
        return;
    }
    s_lastAirportColSlotLogTick = now;

    AirportColSlots64& slots = GetAirportColSlots64();
    FLog("[COL_SLOT_FIND64] pos=%.2f %.2f %.2f onlyBB=%d airport={las_1:%d,las_2:%d,las_3:%d,las_4:%d,las_5:%d,laxref:%d,laxrefdocks:%d}",
         pos.x,
         pos.y,
         pos.z,
         Supremo::Collision::OnlyBoundingBoxes() ? 1 : 0,
         slots.las1,
         slots.las2,
         slots.las3,
         slots.las4,
         slots.las5,
         slots.laxref,
         slots.laxrefdocks);

    LogColSlot64("las_1", slots.las1);
    LogColSlot64("las_2", slots.las2);
    LogColSlot64("las_3", slots.las3);
    LogColSlot64("las_4", slots.las4);
    LogColSlot64("las_5", slots.las5);
}

static void RequestEntityCollisionSamples64(CEntityGTA* entity,
                                            CColModel* colModel,
                                            const CVector& playerPos,
                                            uint8_t playerArea)
{
    if (!entity || !colModel) {
        return;
    }

    const uint8_t entityArea = static_cast<uint8_t>(entity->m_nAreaCode);
    const CVector entityPos = entity->GetPosition();
    const CVector center = GetEntityColWorldCenter64(entity, colModel);
    const CVector playerInside = ClampPointToEntityColBounds64(
        entity,
        colModel,
        CVector(playerPos.x, playerPos.y, center.z));

    RequestCollisionSample64(entityPos, entityArea);
    RequestCollisionSample64(center, entityArea);
    RequestCollisionSample64(playerInside, entityArea);
    if (playerArea != entityArea) {
        RequestCollisionSample64(entityPos, playerArea);
        RequestCollisionSample64(center, playerArea);
        RequestCollisionSample64(playerInside, playerArea);
    }
}

struct NearbyWorldCollisionStats64 {
    int scanned = 0;
    int near = 0;
    int skippedArea = 0;
    int skippedLod = 0;
    int badModel = 0;
    int noModelInfo = 0;
    int noColModel = 0;
    int skippedBounds = 0;
    int skippedTempVehicleParts = 0;
    int skippedUnstableProps = 0;
    int keptModel = 0;
    int retainedSlots = 0;
    int alreadyData = 0;
    int missingData = 0;
    int requested = 0;
    int loadedAfter = 0;
    int protectedEntities = 0;
    int unhidden = 0;
    int limited = 0;
    int reloadPending = 0;
    int reloadRequested = 0;
    int reloadThrottled = 0;
    int hintedSlots = 0;
    int remappedSlots = 0;
    int mismatchedData = 0;
    int synthesizedData = 0;
    bool reloadSlots[256]{};
};

struct NearbyWorldCollisionLog64 {
    const char* poolName = nullptr;
    int slot = -1;
    int modelId = -1;
    uint8_t entityArea = 0;
    uint8_t colSlot = 0;
    uint8_t colFlags = 0;
    bool beforeData = false;
    bool road = false;
    bool ownCol = false;
    uint16_t modelFlags = 0;
    int16_t colModelStart = -1;
    int16_t colModelEnd = -1;
    uint16_t colRefCount = 0;
    uint8_t colLoadState = 0;
    uint8_t expectedColSlot = 0;
    uint8_t expectedColLoadState = 0;
    uint8_t expectedColImgId = 0;
    uint32_t expectedColCdPosn = 0;
    uint32_t expectedColCdSize = 0;
    int16_t expectedColNextOnCd = -1;
    bool colActive = false;
    bool colRequired = false;
    CColModel* colModel = nullptr;
    CVector entityPos{};
    CVector worldMin{};
    CVector worldMax{};
    char modelName[22]{};
    char colSlotName[19]{};
};

static int32 GetExpectedAirportColSlot64(int modelId, CBaseModelInfo* modelInfo)
{
    if (!modelInfo) {
        return -1;
    }

    AirportColSlots64& slots = GetAirportColSlots64();
    const char* name = modelInfo->m_modelName;
    switch (modelId) {
        case 7437: // vegasNroad25
        case 7462: // vegasNland11
            return slots.vegasw1;
        case 7429: // vegasNroad04
        case 7431: // vegasNroad06
        case 7435: // vegasNroad15
        case 7467: // vegasNland16
        case 7474: // vegasNland23
        case 7482: // vegasNroad49
        case 7484: // vegasNroad51
        case 7550: // vegasNroad21
            return slots.vegasw3;
        case 7468: // vegasNland17
            return slots.vegasw9;
        case 7483: // vegasNroad50
        case 7517: // vgnwreland1
            return slots.vegasw7;
        case 5794: // road_lawn06
        case 5808: // road_lawn39
        case 5809: // lawngrndaa
        case 5859: // road_lawn24
        case 5994: // road_lawn26
        case 5995: // road_lawn04
            return slots.lawn1;
        case 6054: // lawroads_law02
        case 6114: // lawroads_law08
        case 6127: // lawroads_law21
        case 6129: // lawroads_law23
            return slots.law1;
        case 6122: // lawroads_law16
        case 6233: // canal_floor
        case 6236: // canal_floor3
            return slots.law2;
        case 6094: // bevgrnd03b_law
        case 6234: // canal_floor2
            return slots.law3;
        case 6040: // wilshire7_law
        case 6120: // lawroads_law14
        case 6213: // venlaw_grnd
        case 6217: // law_vengrnd
            return slots.law4;
        case 5391: // laeroad01
        case 5472: // frecrsbrid_LAE
        case 5497: // laeroad32
        case 5510: // laeroad45
        case 5528: // laeroadct43
            return slots.lae1;
        case 5469: // laeRoads11Tr
        case 5504: // laeroad39
            return slots.lae4;
        case 17513: // lae2_ground04
        case 17597: // Lae2_roads03
        case 17610: // Lae2_roads15
        case 17612: // Lae2_roads88
        case 17614: // Lae2_landHUB02
        case 17620: // Lae2_landHUB01
        case 17621: // Lae2_roads17
        case 17639: // Lae2_roads31
        case 17656: // Lae2_roads50
        case 17920: // Lae2_roads49
        case 17972: // grnd_alpha2
        case 17978: // grnd_alpha6
            return slots.lae2_4;
        case 17625: // Lae2_roads21
        case 17627: // Lae2_roads23
            return slots.lae2_5;
        case 4556: // sky4plaz1_LAn
        case 4557: // road10_LAn2
        case 4558: // LacmEntr1_LAn
        case 4559: // LacmaBase1_LAn
        case 4560: // LacmCanop1_LAn
        case 4564: // LAskyscrap2_LAn
        case 4567: // road07_LAn2
        case 4573: // stolenbuilds12
        case 4574: // stolenbuilds13
        case 4576: // lan2newbuild1
        case 4587: // skyscrapn203
        case 4589: // road15_LAn2
        case 4590: // grasspatchlan2
        case 4591: // lan2shit03
        case 4592: // lan2shit04
        case 4593: // lan2buildblk01
        case 4594: // lan2buildblk02
        case 4598: // crprkblok2_LAN2
        case 4599: // csp2GM_LAN2
        case 4602: // LAskyscrap4_LAn
        case 4603: // sky4plaz2_LAn
        case 4604: // build4plaz_LAn2
        case 4605: // skyscrapn203_gls
        case 4636: // cparkgmaumk_LAN
        case 4639: // paypark_lan02
        case 4640: // paypark_lan03
        case 4641: // paypark_lan04
        case 4643: // LAplaza2b_LAn2
        case 4646: // road13_LAn2
        case 4653: // Freeway7_LAn2
        case 4654: // road09_LAn2
        case 4656: // Freeway1_LAn2
        case 4658: // Freeway2_LAn2
        case 4682: // LAdtbuild3_LAn2
        case 4684: // LAalley1_LAn2
        case 4691: // csp3GM_LAN2
        case 4692: // Freeway9_LAn2
        case 4695: // Freeway11_LAn2
        case 4697: // crprkblok1_LAN2
        case 4700: // cpark01_LAN2
        case 4701: // cpark02_LAN2
        case 4702: // cpark03_LAN2
        case 4710: // road08_LAn2
        case 4714: // Lacmaalphas1_LAn
        case 4717: // LTSLAsky3_LAn2
        case 4718: // gm_build4_LAn2
        case 4722: // LTSLAsky3b_LAn2
        case 4729: // billbrdlan2_01
        case 4730: // billbrdlan2_03
        case 4731: // billbrdlan2_05
        case 4732: // billbrdlan2_06
        case 4739: // LTSLAbuild1_LAn2
        case 4744: // LTSLAbuild5_LAn2
        case 4747: // LTSLAsky8_LAn2
        case 4748: // LTSLAbuild7_LAn2
        case 4749: // LTSLAbuild8_LAn2
        case 4750: // LTSLAbuild9_LAn2
            return slots.lan2_1;
        case 4550: // LibrTow1_LAn
        case 4551: // LAriverSec2_LAn
        case 4552: // amubloksun1_LAn
        case 4553: // road12_LAn2
        case 4554: // LibBase1_LAn
        case 4555: // figfree4_LAn
        case 4562: // LAplaza2_LAn
        case 4563: // LAskyscrap1_LAn
        case 4565: // bunksteps1_LAn
        case 4568: // ground01_LAn2
        case 4570: // stolenbuilds08
        case 4571: // stolenbuilds09
        case 4572: // stolenbuilds11
        case 4575: // fireescapes1_lan2
        case 4584: // halgroundlan2
        case 4586: // skyscrapn201
        case 4588: // roofshitlan2
        case 4595: // cpark05_LAN2
        case 4601: // LAn2_gm1
        case 4638: // paypark_lan01
        case 4644: // road06_LAn2
        case 4645: // road14_LAn2
        case 4647: // road11_LAn2
        case 4648: // road05_LAn2
        case 4650: // road02_LAn2
        case 4652: // road04_LAn2
        case 4662: // Freeway4_LAn2
        case 4664: // Freeway5_LAn2
        case 4679: // Freeway8_LAn2
        case 4681: // LAdtbuild6_LAn2
        case 4683: // LAdtbuild2_LAn2
        case 4685: // LAalley2_LAn2
        case 4708: // LAdtbuild1_LAn2
        case 4711: // amublokalpha_LAn2
        case 4712: // LibPlaza1_LAn
        case 4715: // LTSLAsky1_LAn2
        case 4716: // LTSLAsky2_LAn2
        case 4720: // LTSLAsky1b_LAn
        case 4721: // LTSLAsky2b_LAn2
        case 4723: // LTSLAsky4_LAn2
        case 4724: // librarywall_lan2
        case 4725: // LTSLAsky6_LAn2
        case 4726: // libtwrhelipd_LAn2
        case 4727: // libtwrhelipda_LAn2
        case 4733: // billbrdlan2_07
        case 4735: // billbrdlan2_09
        case 4736: // billbrdlan2_10
        case 4737: // fireescapes3_lan2
        case 4738: // fireescapes2_lan2
        case 4740: // LTSLAbuild2_LAn2
        case 4741: // LTSLAbuild3_LAn2
        case 4745: // LTSLAbuild6_LAn2
        case 4746: // LTSLAsky7_LAn2
        case 4751: // LTSLAbuild10_LAn2
        case 4752: // LTSLAbuild11_LAn2
            return slots.lan2_2;
        case 4569: // stolenbuilds05
        case 4585: // towerlan2
        case 4596: // cspGM_LAN2
        case 4597: // crprkblok4_LAN2
        case 4600: // LAdtbuild10_LAn
        case 4637: // cpark_muck_lan2
        case 4642: // paypark_lan
        case 4649: // road01_LAn2
        case 4651: // road03_LAn2
        case 4660: // Freeway3_LAn2
        case 4666: // Freeway6_LAn2
        case 4690: // skyscrapn202
        case 4694: // Freeway10_LAn2
        case 4703: // cpark04_LAN2
        case 4734: // billbrdlan2_08
        case 4742: // LTSLAbuild4_LAn2
        case 4743: // LTSLAsky5_LAn2
            return slots.lan2_3;
        case 6329: // Roads27_LAw2
        case 6333: // Roads25_LAw2
        case 6389: // SanClift01_LAw2
            return slots.law2_1;
        case 6290: // RailTunn02_LAw2
        case 6488: // countclub02_LAw2
        case 6507: // Roads09_LAw2
            return slots.law2_2;
        case 10767: // Airport_11_SFSe
        case 10788: // aircarpark_11_SFSe
        case 10819: // airprtgnd_05_SFSe
        case 11367: // airprtgnd_ct_SFSe
        case 11383: // jjct02
        case 11385: // ctscene2_sfse
            return slots.sfse1;
        case 10817: // airprtgnd_03_SFSe
        case 10841: // drydock1_SFSe01
            return slots.sfse2;
        case 10779: // aircarpark_06_SFSe
        case 10783: // aircarpark_03_SFSe
        case 10816: // airprtgnd_01_SFSe
            return slots.sfse3;
        case 10761: // Airport_08_SFSe
            return slots.sfse5;
        case 10755: // Airport_02_SFSe
        case 10756: // Airport_03_SFSe
        case 10758: // Airport_05_SFSe
        case 10760: // Airport_07_SFSe
        case 10762: // Airport_09_SFSe
            return slots.sfse6;
        case 11283: // Airport_14B_SFSe
            return slots.sfse8;
        case 10013: // vicstuff_sfe17
        case 10017: // bigvic_a1
        case 10018: // tunnel_sfe
        case 10020: // vicstuff_sfe22
        case 10053: // fishwarf20_sfe
        case 10054: // fishwarf24_sfe
        case 10055: // fishwarf21_sfe
        case 10076: // road13_sfe
        case 10084: // fishwarf13_sfe
        case 10086: // aprtmnts03_sfe
        case 10113: // road19_sfe
        case 10114: // road20_sfe
        case 10151: // bigvicgrnd_sfe
            return slots.sfe5;
        case 9930: // nicepark_sfe
        case 9931: // church_sfe
        case 9950: // pier2_sfe
        case 9954: // pier69_sfe3
        case 9958: // submarr_sfe
        case 10011: // carspaces_sfe14
        case 10060: // aprtmnts01_sfe
        case 10061: // aprtmntrailgs01_SFe
        case 10062: // aprtmntrailgs03_SFe
        case 10063: // aprtmnts02_sfe
        case 10064: // aprtmntrailgs02_SFe
        case 10066: // road02_sfe
        case 10074: // road12_sfe
        case 10080: // fishwarf10_sfe
        case 10083: // backalleys1_sfe
        case 10087: // landsl01_sfe
        case 10101: // vicstuff_sfe67
        case 10115: // road21_sfe
        case 10116: // road22_sfe
        case 10120: // road26_sfe
        case 10123: // road29_sfe
        case 10147: // nitelites_sfe15
        case 10233: // carspaces_sfe15
        case 10273: // churchgr_sfe
        case 10274: // churchgr2_sfe
        case 10308: // yet_another_sfe2
            return slots.sfe6;
        case 17088: // cuntwland13b
        case 17137: // cuntwland72b
        case 17170: // cuntwroad13
        case 17172: // cuntwroad14
        case 17462: // xxxxxxxxa
        case 17468: // xxcliffx
            return slots.countryw7;
        case 4822: // NWCSTRD1_LAS
        case 4841: // bchcostrd1_LAS
        case 4844: // Beach1_LAs04
        case 4847: // Beach1_LAs0gj
        case 4862: // airtun2_LAS
        case 4864: // airtun3_LAS
        case 4867: // lasrnway3_LAS
        case 4890: // lasairprterm2_LAS
        case 4990: // airprtwlkto1_LAS
        case 4995: // airsinage_LAS
        case 4997: // airsinage3_LAS
        case 4998: // airsinage4_LAS
        case 5001: // lasrunwall2_LAS
        case 5002: // lasrnway4_LAS
        case 5003: // lasrnway5_LAS
        case 5004: // lasrnway6_LAS
        case 5005: // lasrunwall1_LAS
        case 5031: // snpedteairt_LAS
        case 5038: // airtun2ct_LAS
        case 5044: // las_runsignsx_LAS
        case 5070: // sjmctfnce1_las
        case 5071: // sjmctfnce2_las
        case 5072: // sjmctfnce3_las
        case 5073: // sjmctfnce4_las
        case 5089: // alphbrk6_las
            return slots.las1;
        case 4806: // BTOLAND8_LAS
        case 4807: // LAroads_20gh_LAs
        case 4808: // LAroadss_30_LAs
        case 4817: // TRNTRK7_LAS
        case 4818: // TRNTRK8_LAS
        case 4819: // TRNTRK5_LAS
        case 4820: // BTOLAND1_LAS
        case 4821: // BTOLAND2_LAS
        case 4827: // LAroads_20ghi_LAs
        case 4829: // lasairprt4
        case 4830: // lasairprt3
        case 4831: // airpurt2_las
        case 4832: // airtwer_Las
        case 4837: // LApedhusrea_LAs
        case 4846: // LAcityped1_LAs
        case 4848: // sanpedbeaut
        case 4849: // snpdmshfnc3_LAS
        case 4850: // snpedshpblk07
        case 4853: // traincano_LAS
        case 4857: // snpedmtsp1_LAS
        case 4858: // snpedland1_LAS
        case 4859: // snpedland2_LAS
        case 4860: // unionstwar_LAS
        case 4861: // snpedhuair2_LAS
        case 4868: // LAroads_23_LAs
        case 4869: // lasrnway8_LAS
        case 4872: // LAroads_042e_LAs
        case 4873: // unionstwarc2_LAS
        case 4874: // Helipad1_las
        case 4876: // hillpalos08_LAs
        case 4881: // uninstps_LAS01
        case 4882: // lasbrid1_LAS
        case 4886: // gngspwnhus1_LAS
        case 4891: // billboard_LAS
        case 4892: // kbsgarage2_LAS
        case 4894: // dwntwnbit1b_LAS
        case 4895: // lstrud_LAS
        case 4981: // snpedteew1_LAS
        case 4982: // snpedteew3_LAS
        case 4983: // snpedteew1vv_LAS
        case 4984: // snpedteew3gt_LAS
        case 4991: // lasairprterm1_LAS
        case 4992: // airplants_LAS
        case 4993: // airplnt2_LAS
        case 4994: // airbillb_LAS
        case 4996: // airsinage2_LAS
        case 4999: // airsinage6_LAS
        case 5000: // airsinage5_LAS
        case 5006: // airprtwlkto2_LAS
        case 5007: // lasrunwall3_LAS
        case 5009: // lasrnway7_LAS
        case 5013: // LAroakt1_30_LAs
        case 5016: // snpdPESS1_LAS
        case 5017: // lastripx1_LAS
        case 5020: // mul_LAS
        case 5024: // snpedtee_LAS
        case 5025: // snpedtedc_LAS
        case 5026: // lstrudct1_LAS
        case 5028: // obcity1ct1_LAS
        case 5033: // unmainstat_LAS
        case 5034: // lasairprtcut4
        case 5036: // BTOLAND1ct_LAS
        case 5040: // unionliq_LAS
        case 5042: // bombshop_LAs
        case 5043: // bombdoor_LAs
        case 5052: // BTOROAD1vb_LAS
        case 5056: // modLAS
        case 5059: // lanitewin3_LAS
        case 5060: // crlsafhus_LAS
        case 5061: // lascarl
        case 5064: // TRNTRK5z_LAS
        case 5066: // mondoshave_LAS
        case 5069: // ctscene1_las
        case 5074: // sjmctfnce5_las
        case 5077: // sjmctfnce8_las
        case 5078: // ctscene2_las
        case 5080: // sjmbarct2_LAS
        case 5083: // alphbrk1_las
        case 5084: // alphbrk2_las
        case 5086: // alphbrk3_las
        case 5087: // alphbrk4_las
        case 5088: // alphbrk5_las
            return slots.las2;
        case 4809: // LAroads_05_LAs
        case 4810: // hillpalos04_LAs
        case 4811: // clifftest02
        case 4812: // clifftest05
        case 4813: // clifftest07
        case 4824: // lasgrifsteps2
        case 4825: // griffithoblas
        case 4826: // grifftop2
        case 4845: // hillpalos02_LAs
        case 4851: // hillpalos01_LAs
        case 4875: // hillpalos06_LAs
        case 4877: // dwntwnbit4_LAS
        case 4888: // dwntwnbit3_LAS
        case 4896: // clifftest12
        case 4897: // Beach1a1_LAs
        case 4898: // clifftestgrnd
        case 4986: // odfwer_LAS
        case 4987: // LODfftop03
        case 4988: // lasbillbrd1_las
        case 5021: // LAroadsbrk_05_LAs
        case 5023: // grifovrhang2_LAS
        case 5057: // lanitewin1_LAS
        case 5062: // hillpawfnce_LAs
        case 5081: // rdcrashbar1_LAs
        case 5082: // rdcrashbar2_LAs
            return slots.las4;
        case 4823: // lasgrifroad
        case 4828: // lasairprt5
        case 4833: // airpurtder_las
        case 4834: // airoad1d_LAS
        case 4835: // airoad1b_LAS
        case 4836: // LAroadsx_04_LAs
        case 4838: // airpurtderfr_las
        case 4852: // hillpalos03_LAs
        case 4854: // lasundrairprt2
        case 4870: // airpurt2ax_las
        case 4878: // obcity1_LAS
        case 4879: // hillpaloswal1_LAs
        case 4880: // dwntwnbit2_LAS
        case 4884: // lastranentun1_LAS
        case 4885: // lastranentun4_LAS
        case 4887: // dwntwnbit1_LAS
        case 4889: // dwntwnbit2b_LAS
        case 4985: // Cylinder03
        case 5051: // airobarsjm_LAS
        case 5058: // lanitewin2_LAS
        case 5079: // sjmbarct1_LAS
            return slots.las5;
        case 4814: // clifftest09
        case 4815: // clifftestgrnd2
        case 4816: // ROCKLIFF1_LAS
        case 4839: // bchcostrd3_LAS
        case 4840: // bchcostrd4_LAS
        case 4842: // Beach1_LAs0fg
        case 4843: // Beach1_LAs0fhy
        case 4855: // lasundrairprt1
        case 4856: // lasundrairprt3
        case 4863: // airtun1_LAS
        case 4865: // lasrnway2_LAS
        case 4866: // lasrnway1_LAS
        case 4871: // airpurt2bx_las
        case 4883: // bchcostair_LAS
        case 5030: // lasrunwall1ct_LAS
        case 5032: // las_runsigns_LAS
        case 5046: // bchcostrd4fuk_LAS
        case 5068: // airctsjm1_las
        case 5075: // sjmctfnce6_las
        case 5076: // sjmctfnce7_las
            return slots.las3;
        default:
            break;
    }

    if (ModelNameContains64(name, "xxxxxxxxa") ||
        ModelNameContains64(name, "xxcliffx")) {
        return slots.countryw7;
    }

    if (ModelNameContains64(name, "road_lawn24") ||
        ModelNameContains64(name, "road_lawn26") ||
        ModelNameContains64(name, "road_lawn04") ||
        ModelNameContains64(name, "road_lawn06") ||
        ModelNameContains64(name, "road_lawn39") ||
        ModelNameContains64(name, "lawngrndaa")) {
        return slots.lawn1;
    }
    if (ModelNameContains64(name, "lawroads_law08") ||
        ModelNameContains64(name, "lawroads_law21") ||
        ModelNameContains64(name, "lawroads_law23") ||
        ModelNameContains64(name, "lawroads_law02")) {
        return slots.law1;
    }
    if (ModelNameContains64(name, "frecrsbrid_LAE") ||
        ModelNameContains64(name, "laeroad45") ||
        ModelNameContains64(name, "laeroad01") ||
        ModelNameContains64(name, "laeroad32") ||
        ModelNameContains64(name, "laeroadct43")) {
        return slots.lae1;
    }

    if (ModelNameContains64(name, "las_runsignsx")) {
        return slots.las1;
    }

    if (ModelNameContains64(name, "lasrnway2") ||
        ModelNameContains64(name, "lasrnway1") ||
        ModelNameContains64(name, "lasrunwall1ct") ||
        ModelNameContains64(name, "las_runsigns")) {
        return slots.las3;
    }

    if (ModelNameContains64(name, "lasrnway3") ||
        ModelNameContains64(name, "lasairprterm2") ||
        ModelNameContains64(name, "airprtwlkto1") ||
        ModelNameContains64(name, "lasrunwall2") ||
        ModelNameContains64(name, "lasrunwall1")) {
        return slots.las1;
    }

    if (ModelNameContains64(name, "lasairprt4") ||
        ModelNameContains64(name, "lasrnway4") ||
        ModelNameContains64(name, "lasrnway5") ||
        ModelNameContains64(name, "lasrnway6") ||
        ModelNameContains64(name, "lasrnway8") ||
        ModelNameContains64(name, "lasairprterm1") ||
        ModelNameContains64(name, "airprtwlkto2") ||
        ModelNameContains64(name, "lasrunwall3") ||
        ModelNameContains64(name, "lasrnway7") ||
        ModelNameContains64(name, "lasairprtcut4")) {
        return slots.las2;
    }

    if (ModelNameContains64(name, "laroads_05") ||
        ModelNameContains64(name, "hillpalos04") ||
        ModelNameContains64(name, "clifftest02") ||
        ModelNameContains64(name, "clifftest05") ||
        ModelNameContains64(name, "clifftest07") ||
        ModelNameContains64(name, "lasgrifsteps2") ||
        ModelNameContains64(name, "griffithoblas") ||
        ModelNameContains64(name, "grifftop2") ||
        ModelNameContains64(name, "hillpalos02") ||
        ModelNameContains64(name, "hillpalos01") ||
        ModelNameContains64(name, "hillpalos06") ||
        ModelNameContains64(name, "laroadsbrk_05") ||
        ModelNameContains64(name, "grifovrhang2") ||
        ModelNameContains64(name, "hillpawfnce")) {
        return slots.las4;
    }

    if (ModelNameContains64(name, "lasgrifroad") ||
        ModelNameContains64(name, "lasairprt5") ||
        ModelNameContains64(name, "airpurtder") ||
        ModelNameContains64(name, "airoad1d") ||
        ModelNameContains64(name, "airoad1b") ||
        ModelNameContains64(name, "laroadsx_04") ||
        ModelNameContains64(name, "hillpalos03") ||
        ModelNameContains64(name, "lasundrairprt2") ||
        ModelNameContains64(name, "airpurt2ax") ||
        ModelNameContains64(name, "hillpaloswal")) {
        return slots.las5;
    }

    return -1;
}

static void MarkNearbyColSlotReload64(NearbyWorldCollisionStats64& stats, int32 slot)
{
    if (slot <= 0 || slot >= 256 || stats.reloadSlots[slot]) {
        return;
    }

    stats.reloadSlots[slot] = true;
    ++stats.reloadPending;
}

static void RemoveStreamingModelNative64(int32 modelId)
{
    if (!Supremo::Streaming::IsValidResourceId(modelId)) {
        return;
    }

    Supremo::Streaming::RemoveModelFromNativeStreaming(modelId);
}

static void ReloadPendingNearbyColSlots64(NearbyWorldCollisionStats64& stats,
                                          const CVector& pos,
                                          uint32_t now,
                                          bool immediate)
{
    if (stats.reloadPending <= 0) {
        return;
    }

    static uint32_t s_lastColSlotReloadTick64[256]{};
    const int32 poolSize = Supremo::Collision::PoolSize();
    if (poolSize <= 0) {
        return;
    }

    for (int32 slot = 1; slot < poolSize && slot < 256; ++slot) {
        if (!stats.reloadSlots[slot]) {
            continue;
        }

        if (!immediate && s_lastColSlotReloadTick64[slot] != 0 &&
            now - s_lastColSlotReloadTick64[slot] < 5500) {
            ++stats.reloadThrottled;
            continue;
        }

        Supremo::Collision::ColSlotInfo slotInfo{};
        if (!Supremo::Collision::ReadSlotInfo(slot, slotInfo) || slotInfo.empty) {
            continue;
        }

        const int32 colModelId = Supremo::Streaming::ColModelIdFromSlot(slot);
        if (!Supremo::Streaming::IsValidResourceId(colModelId)) {
            continue;
        }

        const Supremo::Streaming::ModelStreamDiagnostic beforeInfo =
            Supremo::Streaming::ReadModelDiagnostic(colModelId);
        const uint8_t beforeState = beforeInfo.loadState;
        const uint8_t beforeFlags = beforeInfo.flags;
        const uint8_t beforeImg = beforeInfo.imgId;
        const uint32_t beforeCdPosn = beforeInfo.cdPosn;
        const uint32_t beforeCdSize = beforeInfo.cdSize;
        const int16_t beforeNextOnCd = beforeInfo.nextIndexOnCd;
        const bool beforeLoaded = beforeInfo.loaded;
        const bool beforeActive = slotInfo.active;
        const bool beforeRequired = slotInfo.required;
        const uint16_t beforeRef = slotInfo.refCount;
        const int16_t beforeStart = slotInfo.modelIdStart;
        const int16_t beforeEnd = slotInfo.modelIdEnd;
        const char* knownSlotName = GetKnownColSlotName64(slot);

        Supremo::Streaming::MarkModelDontRemoveInLoadScene(colModelId);
        Supremo::Streaming::RequestModel(colModelId, STREAMING_DONTREMOVE_IN_LOADSCENE);
        IncludeKnownModelIndexesForColSlot64(slot);
        Supremo::Streaming::WorldStreamRequest streamRequest{};
        streamRequest.areaCode = AREA_CODE_NORMAL_WORLD;
        streamRequest.streamingFlags = STREAMING_DONTREMOVE_IN_LOADSCENE;
        streamRequest.addModels = false;
        streamRequest.addLods = false;
        streamRequest.addIpls = false;
        streamRequest.loadIpls = false;
        streamRequest.ensureIpls = false;
        streamRequest.loadSceneCollision = false;
        streamRequest.reason = "col-slot-reload";
        Supremo::Streaming::PrepareWorldAtPoint(pos, streamRequest);
        if (knownSlotName) {
            const bool needsLoad = !beforeLoaded || !beforeActive || beforeState == LOADSTATE_NOT_LOADED;
            if (needsLoad) {
                Supremo::Collision::LoadCol(slot, knownSlotName);
            }
            Supremo::Streaming::PrepareWorldAtPoint(pos, streamRequest);
            FLog("[COL_SLOT_LOAD64] mode=%s slot=%d name=%s colModelId=%d nonDestructive=1 needsLoad=%d pos=%.2f %.2f %.2f",
                 immediate ? "spawn" : "runtime",
                 slot,
                 knownSlotName,
                 colModelId,
                 needsLoad ? 1 : 0,
                 pos.x,
                 pos.y,
                 pos.z);
        }
        ++stats.reloadRequested;
        s_lastColSlotReloadTick64[slot] = now;
        const Supremo::Streaming::ModelStreamDiagnostic afterInfo =
            Supremo::Streaming::ReadModelDiagnostic(colModelId);

        FLog("[COL_SLOT_RELOAD64] mode=%s slot=%d,%s colModelId=%d nonDestructive=1 state=%u->%u flags=0x%02X active=%d required=%d ref=%u range=%d-%d cd={img:%u,pos:%u,size:%u,next:%d} pos=%.2f %.2f %.2f",
             immediate ? "spawn" : "runtime",
             slot,
             slotInfo.name[0] ? slotInfo.name : "?",
             colModelId,
             static_cast<unsigned>(beforeState),
             static_cast<unsigned>(afterInfo.loadState),
             static_cast<unsigned>(beforeFlags),
             beforeActive ? 1 : 0,
             beforeRequired ? 1 : 0,
             static_cast<unsigned>(beforeRef),
             beforeStart,
             beforeEnd,
             static_cast<unsigned>(beforeImg),
             beforeCdPosn,
             beforeCdSize,
             static_cast<int>(beforeNextOnCd),
             pos.x,
             pos.y,
             pos.z);
    }
}

static void WarmupActualNearbyCollision64(const CVector& pos,
                                          uint8_t playerArea,
                                          const NearbyWorldCollisionStats64& stats,
                                          uint32_t now,
                                          bool immediate)
{
    if (stats.requested <= 0 &&
        stats.keptModel <= 0 &&
        stats.retainedSlots <= 0 &&
        stats.reloadRequested <= 0 &&
        stats.missingData <= 0) {
        return;
    }

    static uint32_t s_lastLoadAllCollisionTick64 = 0;
    static uint32_t s_lastWarmLogTick64 = 0;

    const int32 areaCode = playerArea == AREA_CODE_NORMAL_WORLD
                               ? AREA_CODE_NORMAL_WORLD
                               : static_cast<int32>(playerArea);
    CVector sample(pos.x, pos.y, pos.z < 18.0f ? 18.0f : pos.z);

    Supremo::Streaming::WorldStreamRequest streamRequest{};
    streamRequest.areaCode = areaCode;
    streamRequest.streamingFlags = STREAMING_DEFAULT;
    streamRequest.reason = immediate ? "nearby-collision-spawn" : "nearby-collision-runtime";
    Supremo::Streaming::PrepareWorldAtPoint(sample, streamRequest);
    Supremo::Streaming::LoadAllRequestedModels(false);

    const bool needsFullColPass = stats.reloadRequested > 0 ||
                                  (stats.missingData > 0 && stats.alreadyData == 0);
    bool didLoadAllCollision = false;
    if (needsFullColPass &&
        (immediate ||
         s_lastLoadAllCollisionTick64 == 0 ||
         now - s_lastLoadAllCollisionTick64 >= 4500)) {
        s_lastLoadAllCollisionTick64 = now;
        Supremo::Collision::LoadAllCollision();
        didLoadAllCollision = true;
    }

    Supremo::Streaming::PrepareWorldAtPoint(sample, streamRequest);
    Supremo::Streaming::LoadAllRequestedModels(false);
    Supremo::Streaming::PrepareWorldAtPoint(sample, streamRequest);

    const bool warmLogImportant =
        didLoadAllCollision ||
        stats.requested > 0 ||
        stats.missingData > 0 ||
        stats.reloadRequested > 0 ||
        stats.reloadPending > 0;
    const uint32_t warmLogInterval = warmLogImportant ? 2200u : 12000u;
    if (immediate || didLoadAllCollision || now - s_lastWarmLogTick64 >= warmLogInterval) {
        s_lastWarmLogTick64 = now;
        FLog("[COL_REAL_WARM64] mode=%s pos=%.2f %.2f %.2f sample=%.2f %.2f %.2f area=%u req=%d missing=%d ok=%d reload=%d loadAll=%d onlyBB=%d mem=%zu/%zu",
             immediate ? "spawn" : "runtime",
             pos.x,
             pos.y,
             pos.z,
             sample.x,
             sample.y,
             sample.z,
             static_cast<unsigned>(playerArea),
             stats.requested,
             stats.missingData,
             stats.alreadyData,
             stats.reloadRequested,
             didLoadAllCollision ? 1 : 0,
             Supremo::Collision::OnlyBoundingBoxes() ? 1 : 0,
             Supremo::Streaming::MemoryUsed(),
             Supremo::Streaming::MemoryAvailable());
    }
}

static bool IsCriticalNearbySurfaceModel64(CBaseModelInfo* modelInfo)
{
    if (!modelInfo) {
        return false;
    }

    if (IsLodWorldModel64(modelInfo) ||
        IsLikelyNonSurfacePropName64(modelInfo->m_modelName) ||
        IsUnstableBreakableWorldPropName64(modelInfo->m_modelName)) {
        return false;
    }

    if (IsKnownInteriorShellSurfaceModel64(modelInfo)) {
        return true;
    }

    return modelInfo->bIsRoad ||
           IsLikelyRoadModel64(modelInfo) ||
           IsLikelyGroundSurfaceModel64(modelInfo) ||
           ModelNameContains64(modelInfo->m_modelName, "lasrnway") ||
           ModelNameContains64(modelInfo->m_modelName, "run") ||
           ModelNameContains64(modelInfo->m_modelName, "taxi");
}

static bool IsTargetedHighSurfaceModel64(CBaseModelInfo* modelInfo)
{
    if (!modelInfo ||
        IsLodWorldModel64(modelInfo) ||
        IsLikelyNonSurfacePropName64(modelInfo->m_modelName) ||
        IsUnstableBreakableWorldPropName64(modelInfo->m_modelName)) {
        return false;
    }

    const char* name = modelInfo->m_modelName;
    return ModelNameContains64(name, "cuntwroad56") ||
           ModelNameContains64(name, "roads13_lan") ||
           ModelNameContains64(name, "ladocks2_las2") ||
           ModelNameContains64(name, "cs_roadbridge01") ||
           ModelNameContains64(name, "southtunnel_04_sfs") ||
           ModelNameContains64(name, "lawroads_law16") ||
           ModelNameContains64(name, "gaz_pier1") ||
           ModelNameContains64(name, "gaz_pier2") ||
           ModelNameContains64(name, "canal_floor3");
}

static float GetTargetedHighSurfacePad64(CBaseModelInfo* modelInfo)
{
    if (!IsTargetedHighSurfaceModel64(modelInfo)) {
        return 0.0f;
    }

    const char* name = modelInfo->m_modelName;
    if (ModelNameContains64(name, "cs_roadbridge01")) {
        return 240.0f;
    }
    if (ModelNameContains64(name, "southtunnel_04_sfs")) {
        return 180.0f;
    }
    if (ModelNameContains64(name, "lawroads_law16") ||
        ModelNameContains64(name, "gaz_pier1") ||
        ModelNameContains64(name, "gaz_pier2") ||
        ModelNameContains64(name, "canal_floor3")) {
        return 150.0f;
    }
    return 180.0f;
}

static float GetTargetedHighSurfacePatchRadius64(CBaseModelInfo* modelInfo)
{
    if (!IsTargetedHighSurfaceModel64(modelInfo)) {
        return 0.0f;
    }

    const char* name = modelInfo->m_modelName;
    if (ModelNameContains64(name, "ladocks2_las2")) {
        return 72.0f;
    }
    if (ModelNameContains64(name, "cs_roadbridge01") ||
        ModelNameContains64(name, "southtunnel_04_sfs")) {
        return 64.0f;
    }
    if (ModelNameContains64(name, "lawroads_law16") ||
        ModelNameContains64(name, "gaz_pier1") ||
        ModelNameContains64(name, "gaz_pier2") ||
        ModelNameContains64(name, "canal_floor3")) {
        return 56.0f;
    }
    return 64.0f;
}

static bool ShouldUseSupplementalSurfaceColBox64(CBaseModelInfo* modelInfo)
{
    return IsTargetedHighSurfaceModel64(modelInfo) ||
           IsCriticalNearbySurfaceModel64(modelInfo);
}

static float GetSupplementalSurfacePatchRadius64(CBaseModelInfo* modelInfo)
{
    const float targetedPatchRadius = GetTargetedHighSurfacePatchRadius64(modelInfo);
    if (targetedPatchRadius > 0.0f) {
        return targetedPatchRadius;
    }
    if (IsLikelyGroundSurfaceModel64(modelInfo)) {
        return 48.0f;
    }
    if (modelInfo && (modelInfo->bIsRoad || IsLikelyRoadModel64(modelInfo))) {
        return 36.0f;
    }
    return 32.0f;
}

static float GetSyntheticSurfaceBoundPad64(CBaseModelInfo* modelInfo)
{
    if (!modelInfo) {
        return 4.0f;
    }

    const char* name = modelInfo->m_modelName;
    if (IsLikelyNonSurfacePropName64(name)) {
        return 4.0f;
    }

    const float targetedPad = GetTargetedHighSurfacePad64(modelInfo);
    if (targetedPad > 0.0f) {
        return targetedPad;
    }

    if (ModelNameContains64(name, "tunnel") ||
        ModelNameContains64(name, "bridge") ||
        ModelNameContains64(name, "landbit")) {
        return 180.0f;
    }
    if (modelInfo->bIsRoad || IsLikelyRoadModel64(modelInfo)) {
        return 96.0f;
    }
    if (IsLikelyGroundSurfaceModel64(modelInfo)) {
        return 96.0f;
    }

    return 4.0f;
}

struct SyntheticSurfaceBounds64 {
    CColModel* colModel = nullptr;
    int32 modelId = -1;
    CVector localMin{};
    CVector localMax{};
    uint32_t lastTick = 0;
};

static void GetStableSyntheticSurfaceBounds64(CColModel* colModel,
                                              int32 modelId,
                                              CVector& localMin,
                                              CVector& localMax)
{
    localMin = colModel ? colModel->m_boundBox.m_vecMin : CVector(0.0f, 0.0f, 0.0f);
    localMax = colModel ? colModel->m_boundBox.m_vecMax : CVector(0.0f, 0.0f, 0.0f);
    SortLocalBounds64(localMin, localMax);
    if (!colModel) {
        return;
    }

    static SyntheticSurfaceBounds64 s_bounds[192]{};
    const uint32_t now = GetTickCount();
    int freeIndex = -1;
    int oldestIndex = 0;
    uint32_t oldestTick = 0xFFFFFFFFu;

    for (int i = 0; i < static_cast<int>(sizeof(s_bounds) / sizeof(s_bounds[0])); ++i) {
        SyntheticSurfaceBounds64& cached = s_bounds[i];
        if (cached.colModel == colModel && cached.modelId == modelId) {
            localMin = cached.localMin;
            localMax = cached.localMax;
            cached.lastTick = now;
            return;
        }

        if (!cached.colModel && freeIndex < 0) {
            freeIndex = i;
        }
        if (cached.lastTick < oldestTick) {
            oldestTick = cached.lastTick;
            oldestIndex = i;
        }
    }

    SyntheticSurfaceBounds64& cached = s_bounds[freeIndex >= 0 ? freeIndex : oldestIndex];
    cached.colModel = colModel;
    cached.modelId = modelId;
    cached.localMin = localMin;
    cached.localMax = localMax;
    cached.lastTick = now;
}

static bool ShouldPatchSyntheticSurfaceFootprintAxis64(CBaseModelInfo* modelInfo,
                                                       float originalAxisSize,
                                                       bool outsideAxis)
{
    if (!modelInfo) {
        return false;
    }

    const char* name = modelInfo->m_modelName;
    if (IsTargetedHighSurfaceModel64(modelInfo)) {
        return outsideAxis || originalAxisSize > 650.0f;
    }

    if (ModelNameContains64(name, "tunnel") ||
        ModelNameContains64(name, "bridge") ||
        ModelNameContains64(name, "landbit")) {
        return outsideAxis || originalAxisSize > 650.0f;
    }

    if (originalAxisSize > 650.0f) {
        return true;
    }

    return outsideAxis && originalAxisSize < 180.0f;
}

static void LogSyntheticSurfaceSkip64(CEntityGTA* entity,
                                      CBaseModelInfo* modelInfo,
                                      CColModel* colModel,
                                      const CVector& sample,
                                      const char* reason,
                                      const char* skipReason,
                                      const CVector& worldMin,
                                      const CVector& worldMax,
                                      float pad)
{
    if (!entity || !modelInfo || !colModel ||
        !IsCriticalNearbySurfaceModel64(modelInfo)) {
        return;
    }

    static uint32_t s_lastSkipLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastSkipLogTick < 650) {
        return;
    }
    s_lastSkipLogTick = now;

    char modelName[22]{};
    CopyModelName64(modelName, modelInfo);
    CMatrix& matrix = entity->GetMatrix();
    const CVector localSample = TransformWorldToLocalColPoint64(matrix, sample);
    CVector localMin{};
    CVector localMax{};
    GetStableSyntheticSurfaceBounds64(colModel, entity->m_nModelIndex, localMin, localMax);
    const CVector entityPos = entity->GetPosition();

    FLog("[COL_SYNTH_SKIP64] reason=%s skip=%s model=%d,%s colSlot=%u data=%d flags=0x%02X road=%d pad=%.1f localSample=%.2f %.2f %.2f local={%.2f %.2f %.2f -> %.2f %.2f %.2f} world={%.2f %.2f %.2f -> %.2f %.2f %.2f} sample=%.2f %.2f %.2f ent=%.2f %.2f %.2f",
         reason ? reason : "?",
         skipReason ? skipReason : "?",
         entity->m_nModelIndex,
         modelName[0] ? modelName : "?",
         static_cast<unsigned>(colModel->m_nColSlot),
         colModel->m_pColData ? 1 : 0,
         static_cast<unsigned>(colModel->m_nFlags),
         modelInfo->bIsRoad ? 1 : 0,
         pad,
         localSample.x,
         localSample.y,
         localSample.z,
         localMin.x,
         localMin.y,
         localMin.z,
         localMax.x,
         localMax.y,
         localMax.z,
         worldMin.x,
         worldMin.y,
         worldMin.z,
         worldMax.x,
         worldMax.y,
         worldMax.z,
         sample.x,
         sample.y,
         sample.z,
         entityPos.x,
         entityPos.y,
         entityPos.z);
}

struct SyntheticSurfaceAnchor64 {
    CColModel* colModel = nullptr;
    int32 modelId = -1;
    float localX = 0.0f;
    float localY = 0.0f;
    float localZ = 0.0f;
    uint32_t lastTick = 0;
};

static float RetainSyntheticSurfaceLocalZ64(CColModel* colModel,
                                            int32 modelId,
                                            float localX,
                                            float localY,
                                            float candidateLocalZ)
{
    static SyntheticSurfaceAnchor64 s_anchors[96]{};
    const uint32_t now = GetTickCount();
    int freeIndex = -1;
    int oldestIndex = 0;
    uint32_t oldestTick = 0xFFFFFFFFu;
    constexpr float kCellRadius = 48.0f;
    constexpr float kCellRadius2 = kCellRadius * kCellRadius;

    for (int i = 0; i < static_cast<int>(sizeof(s_anchors) / sizeof(s_anchors[0])); ++i) {
        SyntheticSurfaceAnchor64& anchor = s_anchors[i];
        const float dx = anchor.localX - localX;
        const float dy = anchor.localY - localY;
        if (anchor.colModel == colModel &&
            anchor.modelId == modelId &&
            dx * dx + dy * dy <= kCellRadius2) {
            if (candidateLocalZ > anchor.localZ) {
                anchor.localZ = candidateLocalZ;
            }
            anchor.localX = localX;
            anchor.localY = localY;
            anchor.lastTick = now;
            return anchor.localZ;
        }

        if (!anchor.colModel && freeIndex < 0) {
            freeIndex = i;
        }
        if (anchor.lastTick < oldestTick) {
            oldestTick = anchor.lastTick;
            oldestIndex = i;
        }
    }

    SyntheticSurfaceAnchor64& anchor = s_anchors[freeIndex >= 0 ? freeIndex : oldestIndex];
    anchor.colModel = colModel;
    anchor.modelId = modelId;
    anchor.localX = localX;
    anchor.localY = localY;
    anchor.localZ = candidateLocalZ;
    anchor.lastTick = now;
    return anchor.localZ;
}

static void RefreshWorldEntityCollisionRegistration64(CEntityGTA* entity,
                                                      CBaseModelInfo* modelInfo,
                                                      const char* reason)
{
    if (!entity || !modelInfo ||
        entity->IsPed() ||
        entity->IsVehicle() ||
        entity->m_bRemoveFromWorld) {
        return;
    }

    struct RefreshedEntity64 {
        CEntityGTA* entity = nullptr;
        int32 modelId = -1;
        uint32_t lastTick = 0;
    };

    static RefreshedEntity64 s_refreshed[192]{};
    const uint32_t now = GetTickCount();
    int freeIndex = -1;
    int oldestIndex = 0;
    uint32_t oldestTick = 0xFFFFFFFFu;

    for (int i = 0; i < static_cast<int>(sizeof(s_refreshed) / sizeof(s_refreshed[0])); ++i) {
        RefreshedEntity64& refreshed = s_refreshed[i];
        if (refreshed.entity == entity && refreshed.modelId == entity->m_nModelIndex) {
            if (now - refreshed.lastTick < 1200) {
                return;
            }
            refreshed.lastTick = now;
            goto do_refresh;
        }

        if (!refreshed.entity && freeIndex < 0) {
            freeIndex = i;
        }
        if (refreshed.lastTick < oldestTick) {
            oldestTick = refreshed.lastTick;
            oldestIndex = i;
        }
    }

    {
        RefreshedEntity64& refreshed = s_refreshed[freeIndex >= 0 ? freeIndex : oldestIndex];
        refreshed.entity = entity;
        refreshed.modelId = entity->m_nModelIndex;
        refreshed.lastTick = now;
    }

do_refresh:
    if (IsUnstableBreakableWorldProp64(entity->m_nModelIndex, modelInfo)) {
        PreserveBreakableWorldPropCollision64(entity);
        return;
    }

    entity->SetCollisionChecking(true);
    entity->m_bIsStaticWaitingForCollision = false;

    char modelName[22]{};
    CopyModelName64(modelName, modelInfo);
    FLog("[COL_REFRESH64] light flag refresh reason=%s model=%d,%s pos=%.2f %.2f %.2f flags=0x%08X",
         reason ? reason : "?",
         entity->m_nModelIndex,
         modelName[0] ? modelName : "?",
         entity->GetPosition().x,
         entity->GetPosition().y,
         entity->GetPosition().z,
         entity->m_nFlags);
}

static bool IsTreeLikeSyntheticPropName64(const char* name)
{
    if (ModelNameContains64(name, "fire")) {
        return false;
    }

    return ModelNameContains64(name, "tree") ||
           ModelNameContains64(name, "palm") ||
           ModelNameContains64(name, "pine") ||
           ModelNameContains64(name, "fir") ||
           ModelNameContains64(name, "elm") ||
           ModelNameContains64(name, "cedar") ||
           ModelNameContains64(name, "redwood");
}

static bool IsBushLikeSyntheticPropName64(const char* name)
{
    return ModelNameContains64(name, "bush") ||
           ModelNameContains64(name, "scrub") ||
           ModelNameContains64(name, "scrb") ||
           ModelNameContains64(name, "combush") ||
           ModelNameContains64(name, "cactus") ||
           ModelNameContains64(name, "hedge") ||
           ModelNameContains64(name, "plant");
}

static eSurfaceType GetSyntheticPropSurfaceType64(const char* name)
{
    if (ModelNameContains64(name, "fence")) return SURFACE_METAL_CHAIN_FENCE;
    if (ModelNameContains64(name, "gate")) return SURFACE_METAL_GATE;
    if (ModelNameContains64(name, "hydrant")) return SURFACE_FIRE_HYDRANT;
    if (ModelNameContains64(name, "lamp")) return SURFACE_LAMP_POST;
    if (ModelNameContains64(name, "pole") || ModelNameContains64(name, "pylon")) return SURFACE_SCAFFOLD_POLE;
    if (ModelNameContains64(name, "bench")) return SURFACE_WOOD_BENCH;
    if (ModelNameContains64(name, "wood") ||
        ModelNameContains64(name, "table") ||
        ModelNameContains64(name, "chair") ||
        ModelNameContains64(name, "seat") ||
        ModelNameContains64(name, "stool") ||
        ModelNameContains64(name, "sofa") ||
        ModelNameContains64(name, "couch") ||
        ModelNameContains64(name, "crate")) return SURFACE_WOOD_SOLID;
    if (ModelNameContains64(name, "barrel")) return SURFACE_METAL_BARREL;
    if (ModelNameContains64(name, "dump")) return SURFACE_METAL_DUMPSTER;
    if (IsLikelyVegetationPropName64(name)) return SURFACE_VEGETATION;
    return SURFACE_DEFAULT;
}

static bool BuildSyntheticPropBox64(CBaseModelInfo* modelInfo,
                                    const CVector& localMin,
                                    const CVector& localMax,
                                    CVector& boxMin,
                                    CVector& boxMax,
                                    eSurfaceType& material)
{
    if (!modelInfo) {
        return false;
    }

    const char* name = modelInfo->m_modelName;
    const float width = localMax.x - localMin.x;
    const float length = localMax.y - localMin.y;
    const float height = localMax.z - localMin.z;
    if (!std::isfinite(width) || !std::isfinite(length) || !std::isfinite(height) ||
        width < 0.05f || length < 0.05f || height < 0.05f ||
        width > 85.0f || length > 85.0f || height > 140.0f) {
        return false;
    }

    const float cx = (localMin.x + localMax.x) * 0.5f;
    const float cy = (localMin.y + localMax.y) * 0.5f;
    material = GetSyntheticPropSurfaceType64(name);

    if (IsTreeLikeSyntheticPropName64(name)) {
        const float maxXY = std::max(width, length);
        const float radius = ClampFloat64(maxXY * 0.11f, 0.24f, 1.35f);
        boxMin = CVector(cx - radius, cy - radius, localMin.z);
        boxMax = CVector(cx + radius, cy + radius, localMax.z);
        return true;
    }

    if (IsBushLikeSyntheticPropName64(name)) {
        const float maxXY = std::max(width, length);
        const float radius = ClampFloat64(maxXY * 0.34f, 0.45f, 2.85f);
        const float bushHeight = ClampFloat64(height * 0.55f, 0.65f, 2.75f);
        boxMin = CVector(cx - radius, cy - radius, localMin.z);
        boxMax = CVector(cx + radius, cy + radius, localMin.z + bushHeight);
        return true;
    }

    if (IsLikelyVegetationPropName64(name)) {
        const float maxXY = std::max(width, length);
        if (height > 4.0f) {
            const float radius = ClampFloat64(maxXY * 0.09f, 0.24f, 1.45f);
            boxMin = CVector(cx - radius, cy - radius, localMin.z);
            boxMax = CVector(cx + radius, cy + radius, localMax.z);
        } else {
            const float radius = ClampFloat64(maxXY * 0.30f, 0.40f, 2.50f);
            const float lowHeight = ClampFloat64(height * 0.65f, 0.45f, 2.20f);
            boxMin = CVector(cx - radius, cy - radius, localMin.z);
            boxMax = CVector(cx + radius, cy + radius, localMin.z + lowHeight);
        }
        return true;
    }

    if (ModelNameContains64(name, "lamp") ||
        ModelNameContains64(name, "pole") ||
        ModelNameContains64(name, "pylon") ||
        ModelNameContains64(name, "traffic")) {
        const float maxXY = std::max(width, length);
        const float radius = ClampFloat64(maxXY * 0.22f, 0.16f, 0.80f);
        boxMin = CVector(cx - radius, cy - radius, localMin.z);
        boxMax = CVector(cx + radius, cy + radius, localMax.z);
        return true;
    }

    boxMin = localMin;
    boxMax = localMax;
    const float minThickness = 0.20f;
    if (boxMax.x - boxMin.x < minThickness) {
        boxMin.x = cx - minThickness * 0.5f;
        boxMax.x = cx + minThickness * 0.5f;
    }
    if (boxMax.y - boxMin.y < minThickness) {
        boxMin.y = cy - minThickness * 0.5f;
        boxMax.y = cy + minThickness * 0.5f;
    }
    if (boxMax.z - boxMin.z < minThickness) {
        const float cz = (localMin.z + localMax.z) * 0.5f;
        boxMin.z = cz - minThickness * 0.5f;
        boxMax.z = cz + minThickness * 0.5f;
    }
    return true;
}

static bool EnsureSyntheticPropColData64(CEntityGTA* entity,
                                         CBaseModelInfo* modelInfo,
                                         CColModel* colModel,
                                         const CVector& sample,
                                         const char* reason)
{
    if (!kArm64SyntheticPropCollisionEnabled) {
        return false;
    }

    if (!entity || !modelInfo || !colModel || colModel->m_pColData ||
        !IsLikelyPhysicalPropModel64(modelInfo) ||
        !std::isfinite(sample.x) ||
        !std::isfinite(sample.y) ||
        !std::isfinite(sample.z)) {
        return false;
    }

    CVector localMin{};
    CVector localMax{};
    GetStableSyntheticSurfaceBounds64(colModel, entity->m_nModelIndex, localMin, localMax);

    CVector boxMin{};
    CVector boxMax{};
    eSurfaceType material = SURFACE_DEFAULT;
    if (!BuildSyntheticPropBox64(modelInfo, localMin, localMax, boxMin, boxMax, material)) {
        return false;
    }

    CVector worldMin{};
    CVector worldMax{};
    ComputeEntityLocalBoundsWorld64(entity, localMin, localMax, 6.0f, worldMin, worldMax);
    if (sample.x < worldMin.x || sample.x > worldMax.x ||
        sample.y < worldMin.y || sample.y > worldMax.y ||
        sample.z < worldMin.z - 80.0f || sample.z > worldMax.z + 80.0f) {
        LogSyntheticSurfaceSkip64(entity, modelInfo, colModel, sample, reason, "prop-bounds", worldMin, worldMax, 6.0f);
        return false;
    }

    colModel->AllocateData(0, 1, 0, 0, 0, false);
    CCollisionData* colData = colModel->m_pColData;
    if (!colData || !colData->m_pBoxes) {
        LogSyntheticSurfaceSkip64(entity, modelInfo, colModel, sample, reason, "prop-alloc", worldMin, worldMax, 6.0f);
        return false;
    }

    colData->bUsesDisks = false;
    colData->bHasFaceGroups = false;
    colData->bHasShadowInfo = false;
    colData->m_nNumShadowTriangles = 0;
    colData->m_nNumShadowVertices = 0;
    colData->m_pShadowVertices = nullptr;
    colData->m_pShadowTriangles = nullptr;
    colData->m_pTrianglePlanes = nullptr;
    colData->m_modelSec = nullptr;

    CColBox& box = colData->m_pBoxes[0];
    box.m_vecMin = boxMin;
    box.m_vecMax = boxMax;
    box.m_Surface.m_nMaterial = material;
    box.m_Surface.m_nPiece = 0;
    box.m_Surface.m_nLighting = tColLighting(0xFF);
    box.m_Surface.m_nLight = 0xFF;

    colModel->m_bHasCollisionVolumes = true;
    colModel->m_bIsSingleColDataAlloc = true;
    colModel->m_bIsActive = true;
    colModel->m_boundBox.m_vecMin = localMin;
    colModel->m_boundBox.m_vecMax = localMax;
    colModel->m_boundSphere.m_vecCenter = (localMin + localMax) / 2.0f;
    const CVector radiusVec = localMax - colModel->m_boundSphere.m_vecCenter;
    colModel->m_boundSphere.m_fRadius = sqrtf(radiusVec.x * radiusVec.x +
                                              radiusVec.y * radiusVec.y +
                                              radiusVec.z * radiusVec.z);
    entity->m_bUsesCollision = true;
    RefreshWorldEntityCollisionRegistration64(entity, modelInfo, reason);

    char modelName[22]{};
    CopyModelName64(modelName, modelInfo);
    RetainWatchedColSlot64(colModel->m_nColSlot, entity->m_nModelIndex, modelName, sample);
    static uint32_t s_lastPropBuildLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastPropBuildLogTick >= 2500) {
        s_lastPropBuildLogTick = now;
        FLog("[COL_PROP64] built prop-box reason=%s model=%d,%s colSlot=%u material=%u local={%.2f %.2f %.2f -> %.2f %.2f %.2f} box={%.2f %.2f %.2f -> %.2f %.2f %.2f} world={%.2f %.2f %.2f -> %.2f %.2f %.2f} sample=%.2f %.2f %.2f",
             reason ? reason : "?",
             entity->m_nModelIndex,
             modelName[0] ? modelName : "?",
             static_cast<unsigned>(colModel->m_nColSlot),
             static_cast<unsigned>(material),
             localMin.x,
             localMin.y,
             localMin.z,
             localMax.x,
             localMax.y,
             localMax.z,
             boxMin.x,
             boxMin.y,
             boxMin.z,
             boxMax.x,
             boxMax.y,
             boxMax.z,
             worldMin.x,
             worldMin.y,
             worldMin.z,
             worldMax.x,
             worldMax.y,
             worldMax.z,
             sample.x,
             sample.y,
             sample.z);
    }

    return true;
}

void (*CShadows__StoreShadowForPole)(CEntityGTA* entity,
                                     float offsetX,
                                     float offsetY,
                                     float offsetZ,
                                     float poleHeight,
                                     float poleWidth,
                                     uint32_t localId);

static bool ShouldSkipUnsafePoleShadow64(CEntityGTA* entity)
{
#if !VER_x32
    if (!entity ||
        entity->m_bRemoveFromWorld ||
        entity->IsPed() ||
        entity->IsVehicle()) {
        return true;
    }

    const int32 modelId = entity->m_nModelIndex;
    if (modelId < 0 || modelId >= CModelInfo::NUM_MODEL_INFOS) {
        return true;
    }

    CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
    if (!modelInfo) {
        return true;
    }

    const char* name = modelInfo->m_modelName;
    const bool poleLike =
        ModelNameContains64(name, "lamp") ||
        ModelNameContains64(name, "lite") ||
        ModelNameContains64(name, "light") ||
        ModelNameContains64(name, "pole") ||
        ModelNameContains64(name, "pylon") ||
        ModelNameContains64(name, "telegraph") ||
        ModelNameContains64(name, "telgrph") ||
        ModelNameContains64(name, "nitelite") ||
        ModelNameContains64(name, "nitlit");
    if (!poleLike && !pNetGame) {
        return false;
    }

    CColModel* colModel = modelInfo->m_pColModel;
    CCollisionData* colData = colModel ? colModel->m_pColData : nullptr;
    const bool hasUsableShadowMesh =
        colData &&
        colData->bHasShadowInfo &&
        colData->m_nNumShadowTriangles > 0 &&
        colData->m_nNumShadowVertices > 0 &&
        colData->m_pShadowTriangles &&
        colData->m_pShadowVertices;

    if (!pNetGame && entity->m_pRwObject && hasUsableShadowMesh) {
        return false;
    }

    entity->m_bDontCastShadowsOn = true;
    modelInfo->bDontCastShadowsOn = true;

    static uint32_t s_lastLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastLogTick > 1500) {
        char modelName[22]{};
        CopyModelName64(modelName, modelInfo);
        const CVector pos = entity->GetPosition();
        FLog("[SHADOW_GUARD64] skip unsafe pole shadow model=%d,%s pos=%.2f %.2f %.2f rw=%d col=%d data=%d sh=%d/%d flags=0x%08X",
             modelId,
             modelName[0] ? modelName : "?",
             pos.x,
             pos.y,
             pos.z,
             entity->m_pRwObject ? 1 : 0,
             colModel ? 1 : 0,
             colData ? 1 : 0,
             colData ? static_cast<int>(colData->m_nNumShadowTriangles) : 0,
             colData ? static_cast<int>(colData->m_nNumShadowVertices) : 0,
             entity->m_nFlags);
        s_lastLogTick = now;
    }

    return true;
#else
    (void)entity;
    return false;
#endif
}

void CShadows__StoreShadowForPole_hook(CEntityGTA* entity,
                                       float offsetX,
                                       float offsetY,
                                       float offsetZ,
                                       float poleHeight,
                                       float poleWidth,
                                       uint32_t localId)
{
#if !VER_x32
    if (ShouldSkipUnsafePoleShadow64(entity)) {
        return;
    }
#endif

    if (CShadows__StoreShadowForPole) {
        CShadows__StoreShadowForPole(entity, offsetX, offsetY, offsetZ, poleHeight, poleWidth, localId);
    }
}

static bool EnsureSyntheticSurfaceColData64(CEntityGTA* entity,
                                            CBaseModelInfo* modelInfo,
                                            CColModel* colModel,
                                            const CVector& sample,
                                            const char* reason)
{
    if (!kArm64SyntheticWorldCollisionEnabled) {
        return false;
    }

    if (!entity || !modelInfo || !colModel || colModel->m_pColData ||
        !IsCriticalNearbySurfaceModel64(modelInfo) ||
        !std::isfinite(sample.x) ||
        !std::isfinite(sample.y) ||
        !std::isfinite(sample.z)) {
        return false;
    }

    CVector worldMin{};
    CVector worldMax{};
    CVector localMin{};
    CVector localMax{};
    GetStableSyntheticSurfaceBounds64(colModel, entity->m_nModelIndex, localMin, localMax);
    const float boundPad = GetSyntheticSurfaceBoundPad64(modelInfo);
    ComputeEntityLocalBoundsWorld64(entity, localMin, localMax, boundPad, worldMin, worldMax);
    const bool allowLooseZ = IsLikelyRoadModel64(modelInfo) || IsLikelyGroundSurfaceModel64(modelInfo);
    if (sample.x < worldMin.x || sample.x > worldMax.x ||
        sample.y < worldMin.y || sample.y > worldMax.y) {
        LogSyntheticSurfaceSkip64(entity, modelInfo, colModel, sample, reason, "xy", worldMin, worldMax, boundPad);
        return false;
    }
    if ((sample.z < worldMin.z - 12.0f || sample.z > worldMax.z + 12.0f) &&
        (!allowLooseZ || sample.z < worldMin.z - 180.0f || sample.z > worldMax.z + 180.0f)) {
        LogSyntheticSurfaceSkip64(entity, modelInfo, colModel, sample, reason, "z", worldMin, worldMax, boundPad);
        return false;
    }

    CMatrix& matrix = entity->GetMatrix();
    const CVector localSampleSurface = TransformWorldToLocalColPoint64(
        matrix,
        CVector(sample.x, sample.y, sample.z - 1.0f));

    CVector footprintMin = localMin;
    CVector footprintMax = localMax;
    const float originalWidth = localMax.x - localMin.x;
    const float originalLength = localMax.y - localMin.y;
    const bool outsideLocalX =
        localSampleSurface.x < localMin.x || localSampleSurface.x > localMax.x;
    const bool outsideLocalY =
        localSampleSurface.y < localMin.y || localSampleSurface.y > localMax.y;
    const bool patchAxisX = allowLooseZ && boundPad > 4.0f &&
                            ShouldPatchSyntheticSurfaceFootprintAxis64(modelInfo, originalWidth, outsideLocalX);
    const bool patchAxisY = allowLooseZ && boundPad > 4.0f &&
                            ShouldPatchSyntheticSurfaceFootprintAxis64(modelInfo, originalLength, outsideLocalY);
    if (patchAxisX || patchAxisY) {
        const float targetedPatchRadius = GetTargetedHighSurfacePatchRadius64(modelInfo);
        const float patchRadius = targetedPatchRadius > 0.0f
            ? targetedPatchRadius
            : ((ModelNameContains64(modelInfo->m_modelName, "tunnel") ||
                ModelNameContains64(modelInfo->m_modelName, "bridge") ||
                ModelNameContains64(modelInfo->m_modelName, "landbit"))
                   ? 48.0f
                   : 36.0f);
        if (patchAxisX) {
            footprintMin.x = ClampFloat64(localSampleSurface.x - patchRadius, localMin.x - boundPad, localMax.x + boundPad);
            footprintMax.x = ClampFloat64(localSampleSurface.x + patchRadius, localMin.x - boundPad, localMax.x + boundPad);
        }
        if (patchAxisY) {
            footprintMin.y = ClampFloat64(localSampleSurface.y - patchRadius, localMin.y - boundPad, localMax.y + boundPad);
            footprintMax.y = ClampFloat64(localSampleSurface.y + patchRadius, localMin.y - boundPad, localMax.y + boundPad);
        }
        if (footprintMin.x > footprintMax.x) std::swap(footprintMin.x, footprintMax.x);
        if (footprintMin.y > footprintMax.y) std::swap(footprintMin.y, footprintMax.y);
    }

    const float width = footprintMax.x - footprintMin.x;
    const float length = footprintMax.y - footprintMin.y;
    const float height = localMax.z - localMin.z;
    if (width < 1.0f || length < 1.0f || height < 0.05f ||
        width > 650.0f || length > 650.0f || height > 180.0f) {
        LogSyntheticSurfaceSkip64(entity, modelInfo, colModel, sample, reason, "size", worldMin, worldMax, boundPad);
        return false;
    }

    constexpr int kGrid = 5;
    const float stepX = width / static_cast<float>(kGrid - 1);
    const float stepY = length / static_cast<float>(kGrid - 1);
    CVector meshLocalMin(100000.0f, 100000.0f, 100000.0f);
    CVector meshLocalMax(-100000.0f, -100000.0f, -100000.0f);
    const float looseLocalMinZ = allowLooseZ ? localMin.z - 180.0f : localMin.z;
    const float looseLocalMaxZ = allowLooseZ ? localMax.z + 180.0f : localMax.z;
    float candidateSurfaceLocalZ = ClampFloat64(localSampleSurface.z, looseLocalMinZ, looseLocalMaxZ);
    const float nativeGroundAtSample = CWorld::FindGroundZForCoord(sample.x, sample.y);
    if (std::isfinite(nativeGroundAtSample) &&
        nativeGroundAtSample >= worldMin.z - 4.0f &&
        nativeGroundAtSample <= worldMax.z + 4.0f &&
        nativeGroundAtSample <= sample.z + 1.50f &&
        nativeGroundAtSample >= sample.z - 7.50f) {
        const CVector nativeLocal = TransformWorldToLocalColPoint64(
            matrix,
            CVector(sample.x, sample.y, nativeGroundAtSample));
        candidateSurfaceLocalZ = ClampFloat64(nativeLocal.z, looseLocalMinZ, looseLocalMaxZ);
    }
    const float retainedSurfaceLocalZ = ClampFloat64(
        RetainSyntheticSurfaceLocalZ64(
            colModel,
            entity->m_nModelIndex,
            localSampleSurface.x,
            localSampleSurface.y,
            candidateSurfaceLocalZ),
        looseLocalMinZ,
        looseLocalMaxZ);
    ExpandBoundsWithPoint64(meshLocalMin, meshLocalMax, CVector(footprintMin.x, footprintMin.y, retainedSurfaceLocalZ));
    ExpandBoundsWithPoint64(meshLocalMin, meshLocalMax, CVector(footprintMax.x, footprintMax.y, retainedSurfaceLocalZ));

    for (int y = 0; y < kGrid; ++y) {
        for (int x = 0; x < kGrid; ++x) {
            const float lx = (x == kGrid - 1) ? footprintMax.x : footprintMin.x + stepX * static_cast<float>(x);
            const float ly = (y == kGrid - 1) ? footprintMax.y : footprintMin.y + stepY * static_cast<float>(y);
            const CVector worldSample = TransformLocalColPoint64(matrix, CVector(lx, ly, 0.0f));
            float groundZ = CWorld::FindGroundZForCoord(worldSample.x, worldSample.y);
            CVector local(lx, ly, retainedSurfaceLocalZ);
            if (std::isfinite(groundZ) &&
                groundZ >= worldMin.z - 6.0f &&
                groundZ <= worldMax.z + 6.0f) {
                CVector nativeLocal = TransformWorldToLocalColPoint64(
                    matrix,
                    CVector(worldSample.x, worldSample.y, groundZ));
                if (fabsf(nativeLocal.z - retainedSurfaceLocalZ) <= 8.0f) {
                    local = nativeLocal;
                }
            }
            local.x = lx;
            local.y = ly;
            local.z = ClampFloat64(local.z, localMin.z - 1.0f, localMax.z + 1.0f);
            ExpandBoundsWithPoint64(meshLocalMin, meshLocalMax, local);
        }
    }

    const float surfaceTopLocalZ = ClampFloat64(
        retainedSurfaceLocalZ,
        looseLocalMinZ,
        looseLocalMaxZ);
    const bool targetedSurface = IsTargetedHighSurfaceModel64(modelInfo);
    const float boxBottomPad = targetedSurface ? 8.00f : 4.50f;
    const float boxTopPad = targetedSurface ? 1.50f : 0.85f;
    CVector boxMin(footprintMin.x, footprintMin.y, surfaceTopLocalZ - boxBottomPad);
    CVector boxMax(footprintMax.x, footprintMax.y, surfaceTopLocalZ + boxTopPad);
    CVector expandedLocalMin = localMin;
    CVector expandedLocalMax = localMax;
    ExpandBoundsWithPoint64(expandedLocalMin, expandedLocalMax, boxMin);
    ExpandBoundsWithPoint64(expandedLocalMin, expandedLocalMax, boxMax);

    colModel->AllocateData(0, 1, 0, 0, 0, false);
    CCollisionData* colData = colModel->m_pColData;
    if (!colData) {
        LogSyntheticSurfaceSkip64(entity, modelInfo, colModel, sample, reason, "alloc", worldMin, worldMax, boundPad);
        return false;
    }

    colData->bUsesDisks = false;
    colData->bHasFaceGroups = false;
    colData->bHasShadowInfo = false;
    colData->m_nNumShadowTriangles = 0;
    colData->m_nNumShadowVertices = 0;
    colData->m_pShadowVertices = nullptr;
    colData->m_pShadowTriangles = nullptr;
    colData->m_pTrianglePlanes = nullptr;
    colData->m_modelSec = nullptr;

    CColBox& box = colData->m_pBoxes[0];
    box.m_vecMin = boxMin;
    box.m_vecMax = boxMax;
    box.m_Surface.m_nMaterial = SURFACE_TARMAC;
    box.m_Surface.m_nPiece = 0;
    box.m_Surface.m_nLighting = tColLighting(0xFF);
    box.m_Surface.m_nLight = 0xFF;

    colModel->m_bHasCollisionVolumes = true;
    colModel->m_bIsSingleColDataAlloc = true;
    colModel->m_bIsActive = true;
    colModel->m_boundBox.m_vecMin = expandedLocalMin;
    colModel->m_boundBox.m_vecMax = expandedLocalMax;
    colModel->m_boundSphere.m_vecCenter = (expandedLocalMin + expandedLocalMax) / 2.0f;
    const CVector radiusVec = expandedLocalMax - colModel->m_boundSphere.m_vecCenter;
    colModel->m_boundSphere.m_fRadius = sqrtf(radiusVec.x * radiusVec.x +
                                              radiusVec.y * radiusVec.y +
                                              radiusVec.z * radiusVec.z);
    entity->m_bUsesCollision = true;
    if (targetedSurface || patchAxisX || patchAxisY) {
        RefreshWorldEntityCollisionRegistration64(entity, modelInfo, reason);
    }

    char modelName[22]{};
    CopyModelName64(modelName, modelInfo);
    RetainWatchedColSlot64(colModel->m_nColSlot, entity->m_nModelIndex, modelName, sample);
    FLog("[COL_SYNTH64] built surface-box reason=%s model=%d,%s colSlot=%u road=%d anchor=%.2f local={%.2f %.2f %.2f -> %.2f %.2f %.2f} mesh={%.2f %.2f %.2f -> %.2f %.2f %.2f} box={%.2f %.2f %.2f -> %.2f %.2f %.2f} world={%.2f %.2f %.2f -> %.2f %.2f %.2f} sample=%.2f %.2f %.2f",
         reason ? reason : "?",
         entity->m_nModelIndex,
         modelName[0] ? modelName : "?",
         static_cast<unsigned>(colModel->m_nColSlot),
         modelInfo->bIsRoad ? 1 : 0,
         retainedSurfaceLocalZ,
         localMin.x,
         localMin.y,
         localMin.z,
         localMax.x,
         localMax.y,
         localMax.z,
         meshLocalMin.x,
         meshLocalMin.y,
         meshLocalMin.z,
         meshLocalMax.x,
         meshLocalMax.y,
         meshLocalMax.z,
         boxMin.x,
         boxMin.y,
         boxMin.z,
         boxMax.x,
         boxMax.y,
         boxMax.z,
         worldMin.x,
         worldMin.y,
         worldMin.z,
         worldMax.x,
         worldMax.y,
         worldMax.z,
         sample.x,
         sample.y,
         sample.z);

    return true;
}

static bool EnsureMultipleAllocColData64(CColModel* colModel, int32 modelId, const char* modelName)
{
    if (!colModel || !colModel->m_pColData) {
        return false;
    }
    if (!colModel->m_bIsSingleColDataAlloc) {
        return true;
    }

    CCollisionData* oldData = colModel->m_pColData;
    CCollisionData* newData = new CCollisionData();
    if (!newData) {
        return false;
    }

    newData->m_modelSec = nullptr;
    newData->Copy(*oldData);
    newData->m_modelSec = nullptr;
    CCollision::RemoveTrianglePlanes(oldData);
    CMemoryMgr::Free(oldData);

    colModel->m_pColData = newData;
    colModel->m_bIsSingleColDataAlloc = false;

    FLog("[COL_SUPP64] converted single-alloc data model=%d,%s boxes=%u tris=%u",
         modelId,
         modelName && modelName[0] ? modelName : "?",
         static_cast<unsigned>(newData->m_nNumBoxes),
         static_cast<unsigned>(newData->m_nNumTriangles));
    return true;
}

static bool IsDuplicateSupplementalBox64(CCollisionData* data,
                                         const CVector& boxMin,
                                         const CVector& boxMax)
{
    if (!data || !data->m_pBoxes) {
        return false;
    }

    const CVector center = (boxMin + boxMax) / 2.0f;
    for (uint16 i = 0; i < data->m_nNumBoxes; ++i) {
        const CColBox& box = data->m_pBoxes[i];
        const CVector existingCenter = (box.m_vecMin + box.m_vecMax) / 2.0f;
        const bool overlapsX = boxMax.x >= box.m_vecMin.x && boxMin.x <= box.m_vecMax.x;
        const bool overlapsY = boxMax.y >= box.m_vecMin.y && boxMin.y <= box.m_vecMax.y;
        if (overlapsX &&
            overlapsY &&
            fabsf(existingCenter.x - center.x) <= 16.0f &&
            fabsf(existingCenter.y - center.y) <= 16.0f &&
            fabsf(existingCenter.z - center.z) <= 5.0f) {
            return true;
        }
    }
    return false;
}

static bool AppendSupplementalColBox64(CColModel* colModel,
                                       CBaseModelInfo* modelInfo,
                                       int32 modelId,
                                       const CVector& boxMin,
                                       const CVector& boxMax,
                                       const CVector& expandedLocalMin,
                                       const CVector& expandedLocalMax,
                                       const char* reason)
{
    if (!colModel || !modelInfo || !colModel->m_pColData) {
        return false;
    }

    char modelName[22]{};
    CopyModelName64(modelName, modelInfo);
    if (!EnsureMultipleAllocColData64(colModel, modelId, modelName)) {
        return false;
    }

    CCollisionData* data = colModel->m_pColData;
    if (!data || data->m_nNumBoxes >= 240) {
        return false;
    }
    if (IsDuplicateSupplementalBox64(data, boxMin, boxMax)) {
        return true;
    }

    const uint16 oldBoxCount = data->m_nNumBoxes;
    CColBox* boxes = static_cast<CColBox*>(
        CMemoryMgr::Malloc(static_cast<unsigned int>((oldBoxCount + 1) * sizeof(CColBox))));
    if (!boxes) {
        return false;
    }

    for (uint16 i = 0; i < oldBoxCount; ++i) {
        boxes[i] = data->m_pBoxes[i];
    }

    CColBox& box = boxes[oldBoxCount];
    box.m_vecMin = boxMin;
    box.m_vecMax = boxMax;
    box.m_Surface.m_nMaterial = SURFACE_TARMAC;
    box.m_Surface.m_nPiece = 0;
    box.m_Surface.m_nLighting = tColLighting(0xFF);
    box.m_Surface.m_nLight = 0xFF;

    CMemoryMgr::Free(data->m_pBoxes);
    data->m_pBoxes = boxes;
    data->m_nNumBoxes = oldBoxCount + 1;
    data->bHasFaceGroups = false;

    colModel->m_bHasCollisionVolumes = true;
    colModel->m_bIsActive = true;
    colModel->m_boundBox.m_vecMin = expandedLocalMin;
    colModel->m_boundBox.m_vecMax = expandedLocalMax;
    colModel->m_boundSphere.m_vecCenter = (expandedLocalMin + expandedLocalMax) / 2.0f;
    const CVector radiusVec = expandedLocalMax - colModel->m_boundSphere.m_vecCenter;
    colModel->m_boundSphere.m_fRadius = sqrtf(radiusVec.x * radiusVec.x +
                                              radiusVec.y * radiusVec.y +
                                              radiusVec.z * radiusVec.z);

    FLog("[COL_SUPP64] appended surface-box reason=%s model=%d,%s boxes=%u box={%.2f %.2f %.2f -> %.2f %.2f %.2f} bounds={%.2f %.2f %.2f -> %.2f %.2f %.2f}",
         reason ? reason : "?",
         modelId,
         modelName[0] ? modelName : "?",
         static_cast<unsigned>(data->m_nNumBoxes),
         boxMin.x,
         boxMin.y,
         boxMin.z,
         boxMax.x,
         boxMax.y,
         boxMax.z,
         expandedLocalMin.x,
         expandedLocalMin.y,
         expandedLocalMin.z,
         expandedLocalMax.x,
         expandedLocalMax.y,
         expandedLocalMax.z);

    return true;
}

static bool TryPatchNativeGroundCollisionHole64(CEntityGTA* entity,
                                                CBaseModelInfo* modelInfo,
                                                CColModel* colModel,
                                                CVector* origin,
                                                float targetZ,
                                                VerticalEntityFallbackResult64& result,
                                                const char* reason)
{
    if (!kArm64NativeGroundHolePatchEnabled ||
        !entity ||
        !modelInfo ||
        !colModel ||
        !colModel->m_pColData ||
        !origin ||
        origin->z <= targetZ ||
        !ShouldUseSupplementalSurfaceColBox64(modelInfo)) {
        return false;
    }

    const bool surfaceLike =
        modelInfo->bIsRoad ||
        IsLikelyRoadModel64(modelInfo) ||
        IsLikelyGroundSurfaceModel64(modelInfo) ||
        IsTargetedHighSurfaceModel64(modelInfo);
    if (!surfaceLike) {
        return false;
    }

    const float nativeGroundZ = CWorld::FindGroundZForCoord(origin->x, origin->y);
    if (!std::isfinite(nativeGroundZ) ||
        nativeGroundZ < targetZ - 0.35f ||
        nativeGroundZ > origin->z + 1.75f ||
        nativeGroundZ < -100.0f ||
        nativeGroundZ > 3000.0f) {
        return false;
    }

    CVector localMin{};
    CVector localMax{};
    GetStableSyntheticSurfaceBounds64(colModel, entity->m_nModelIndex, localMin, localMax);
    const float basePad = GetSyntheticSurfaceBoundPad64(modelInfo);
    const float xyPad = ClampFloat64(basePad, 6.0f, IsTargetedHighSurfaceModel64(modelInfo) ? 32.0f : 18.0f);
    CVector worldMin{};
    CVector worldMax{};
    ComputeEntityLocalBoundsWorld64(entity, localMin, localMax, xyPad, worldMin, worldMax);
    if (origin->x < worldMin.x || origin->x > worldMax.x ||
        origin->y < worldMin.y || origin->y > worldMax.y ||
        nativeGroundZ < worldMin.z - 16.0f ||
        nativeGroundZ > worldMax.z + 16.0f) {
        return false;
    }

    CMatrix& matrix = entity->GetMatrix();
    CVector localGround = TransformWorldToLocalColPoint64(
        matrix,
        CVector(origin->x, origin->y, nativeGroundZ));
    if (localGround.x < localMin.x - xyPad ||
        localGround.x > localMax.x + xyPad ||
        localGround.y < localMin.y - xyPad ||
        localGround.y > localMax.y + xyPad) {
        return false;
    }

    const float zPad = IsTargetedHighSurfaceModel64(modelInfo) ? 10.0f : 6.0f;
    if (localGround.z < localMin.z - zPad ||
        localGround.z > localMax.z + zPad) {
        return false;
    }

    const float patchRadius = IsTargetedHighSurfaceModel64(modelInfo) ? 8.0f : 6.0f;
    const float minX = localMin.x - xyPad;
    const float maxX = localMax.x + xyPad;
    const float minY = localMin.y - xyPad;
    const float maxY = localMax.y + xyPad;
    CVector boxMin(
        ClampFloat64(localGround.x - patchRadius, minX, maxX),
        ClampFloat64(localGround.y - patchRadius, minY, maxY),
        localGround.z - 1.40f);
    CVector boxMax(
        ClampFloat64(localGround.x + patchRadius, minX, maxX),
        ClampFloat64(localGround.y + patchRadius, minY, maxY),
        localGround.z + 0.85f);
    if (boxMin.x > boxMax.x) std::swap(boxMin.x, boxMax.x);
    if (boxMin.y > boxMax.y) std::swap(boxMin.y, boxMax.y);
    if (boxMin.z > boxMax.z) std::swap(boxMin.z, boxMax.z);

    CColPoint col{};
    col.m_vecPoint = CVector(origin->x, origin->y, nativeGroundZ);
    col.m_vecNormal = CVector(0.0f, 0.0f, 1.0f);
    col.m_nSurfaceTypeA = SURFACE_TARMAC;
    col.m_nSurfaceTypeB = SURFACE_TARMAC;
    col.m_nLightingA.value = 0xFF;
    col.m_nLightingB.value = 0xFF;
    col.m_fDepth = origin->z - nativeGroundZ;

    ++result.directHits;
    if (!result.entity || nativeGroundZ > result.z) {
        result.entity = entity;
        result.col = col;
        result.z = nativeGroundZ;
    }

    static uint32_t s_lastNativeHolePatchLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastNativeHolePatchLogTick >= 500) {
        s_lastNativeHolePatchLogTick = now;
        char modelName[22]{};
        CopyModelName64(modelName, modelInfo);
        FLog("[COL_HOLE_QUERY64] reason=%s model=%d,%s native=%.2f origin=%.2f %.2f %.2f target=%.2f local=%.2f %.2f %.2f box={%.2f %.2f %.2f -> %.2f %.2f %.2f}",
             reason ? reason : "native-hole",
             entity->m_nModelIndex,
             modelName[0] ? modelName : "?",
             nativeGroundZ,
             origin->x,
             origin->y,
             origin->z,
             targetZ,
             localGround.x,
             localGround.y,
             localGround.z,
             boxMin.x,
             boxMin.y,
             boxMin.z,
             boxMax.x,
             boxMax.y,
             boxMax.z);
    }

    return true;
}

static bool EnsureSupplementalSurfaceColBox64(CEntityGTA* entity,
                                              CBaseModelInfo* modelInfo,
                                              CColModel* colModel,
                                              const CVector& sample,
                                              const char* reason,
                                              float* outSurfaceZ)
{
    if (!kArm64SyntheticWorldCollisionEnabled) {
        return false;
    }

    if (!entity || !modelInfo || !colModel || !colModel->m_pColData ||
        !IsTargetedHighSurfaceModel64(modelInfo) ||
        !std::isfinite(sample.x) ||
        !std::isfinite(sample.y) ||
        !std::isfinite(sample.z)) {
        return false;
    }

    CVector worldMin{};
    CVector worldMax{};
    CVector localMin{};
    CVector localMax{};
    GetStableSyntheticSurfaceBounds64(colModel, entity->m_nModelIndex, localMin, localMax);
    const float boundPad = GetSyntheticSurfaceBoundPad64(modelInfo);
    ComputeEntityLocalBoundsWorld64(entity, localMin, localMax, boundPad, worldMin, worldMax);
    if (sample.x < worldMin.x || sample.x > worldMax.x ||
        sample.y < worldMin.y || sample.y > worldMax.y ||
        sample.z < worldMin.z - 180.0f ||
        sample.z > worldMax.z + 180.0f) {
        LogSyntheticSurfaceSkip64(entity, modelInfo, colModel, sample, reason, "supp-bounds", worldMin, worldMax, boundPad);
        return false;
    }

    CMatrix& matrix = entity->GetMatrix();
    CVector localSampleSurface = TransformWorldToLocalColPoint64(
        matrix,
        CVector(sample.x, sample.y, sample.z - 1.0f));

    const float looseLocalMinZ = localMin.z - 180.0f;
    const float looseLocalMaxZ = localMax.z + 180.0f;
    const float retainedSurfaceLocalZ = ClampFloat64(
        RetainSyntheticSurfaceLocalZ64(
            colModel,
            entity->m_nModelIndex,
            localSampleSurface.x,
            localSampleSurface.y,
            ClampFloat64(localSampleSurface.z, looseLocalMinZ, looseLocalMaxZ)),
        looseLocalMinZ,
        looseLocalMaxZ);

    const char* name = modelInfo->m_modelName;
    const bool targetedSurface = IsTargetedHighSurfaceModel64(modelInfo);
    const float patchRadius = GetSupplementalSurfacePatchRadius64(modelInfo);
    const float boxBottomPad = targetedSurface
        ? (ModelNameContains64(name, "ladocks2_las2") ? 10.00f : 8.00f)
        : (IsLikelyGroundSurfaceModel64(modelInfo) ? 5.50f : 4.50f);
    const float boxTopPad = targetedSurface
        ? (ModelNameContains64(name, "ladocks2_las2") ? 2.00f : 1.50f)
        : 0.95f;
    const float minX = localMin.x - boundPad;
    const float maxX = localMax.x + boundPad;
    const float minY = localMin.y - boundPad;
    const float maxY = localMax.y + boundPad;

    CVector boxMin(
        ClampFloat64(localSampleSurface.x - patchRadius, minX, maxX),
        ClampFloat64(localSampleSurface.y - patchRadius, minY, maxY),
        retainedSurfaceLocalZ - boxBottomPad);
    CVector boxMax(
        ClampFloat64(localSampleSurface.x + patchRadius, minX, maxX),
        ClampFloat64(localSampleSurface.y + patchRadius, minY, maxY),
        retainedSurfaceLocalZ + boxTopPad);
    if (boxMin.x > boxMax.x) std::swap(boxMin.x, boxMax.x);
    if (boxMin.y > boxMax.y) std::swap(boxMin.y, boxMax.y);
    if (boxMin.z > boxMax.z) std::swap(boxMin.z, boxMax.z);

    CVector expandedLocalMin = localMin;
    CVector expandedLocalMax = localMax;
    ExpandBoundsWithPoint64(expandedLocalMin, expandedLocalMax, boxMin);
    ExpandBoundsWithPoint64(expandedLocalMin, expandedLocalMax, boxMax);

    const bool appended = AppendSupplementalColBox64(
        colModel,
        modelInfo,
        entity->m_nModelIndex,
        boxMin,
        boxMax,
        expandedLocalMin,
        expandedLocalMax,
        reason);
    if (!appended) {
        return false;
    }

    entity->m_bUsesCollision = true;
    RefreshWorldEntityCollisionRegistration64(entity, modelInfo, reason);
    if (outSurfaceZ) {
        const CVector worldSurface = TransformLocalColPoint64(
            matrix,
            CVector(localSampleSurface.x, localSampleSurface.y, retainedSurfaceLocalZ));
        *outSurfaceZ = worldSurface.z;
    }
    return true;
}

static bool TryProcessSupplementalColDataSurface64(CEntityGTA* entity,
                                                   CBaseModelInfo* modelInfo,
                                                   CColModel* colModel,
                                                   CVector* origin,
                                                   float targetZ,
                                                   VerticalEntityFallbackResult64& result)
{
    if (!kArm64SyntheticWorldCollisionEnabled) {
        return false;
    }

    if (!entity || !modelInfo || !colModel || !origin || origin->z <= targetZ ||
        !colModel->m_pColData ||
        !ShouldUseSupplementalSurfaceColBox64(modelInfo)) {
        return false;
    }

    float surfaceZ = 0.0f;
    const bool targetedSurface = IsTargetedHighSurfaceModel64(modelInfo);
    if (targetedSurface) {
        if (!EnsureSupplementalSurfaceColBox64(
                entity,
                modelInfo,
                colModel,
                *origin,
                "vertical-existing",
                &surfaceZ)) {
            return false;
        }
    } else {
        CVector worldMin{};
        CVector worldMax{};
        CVector localMin{};
        CVector localMax{};
        GetStableSyntheticSurfaceBounds64(colModel, entity->m_nModelIndex, localMin, localMax);
        const float boundPad = GetSyntheticSurfaceBoundPad64(modelInfo);
        ComputeEntityLocalBoundsWorld64(entity, localMin, localMax, boundPad, worldMin, worldMax);
        if (origin->x < worldMin.x || origin->x > worldMax.x ||
            origin->y < worldMin.y || origin->y > worldMax.y ||
            origin->z < worldMin.z - 80.0f ||
            origin->z > worldMax.z + 80.0f ||
            targetZ > worldMax.z + 80.0f) {
            ++result.boundsSkipped;
            return false;
        }

        CMatrix& matrix = entity->GetMatrix();
        const CVector localSampleSurface = TransformWorldToLocalColPoint64(
            matrix,
            CVector(origin->x, origin->y, origin->z - 1.0f));
        const float looseLocalMinZ = localMin.z - 12.0f;
        const float looseLocalMaxZ = localMax.z + 12.0f;
        const float retainedSurfaceLocalZ = ClampFloat64(
            RetainSyntheticSurfaceLocalZ64(
                colModel,
                entity->m_nModelIndex,
                localSampleSurface.x,
                localSampleSurface.y,
                ClampFloat64(localSampleSurface.z, looseLocalMinZ, looseLocalMaxZ)),
            looseLocalMinZ,
            looseLocalMaxZ);
        const CVector worldSurface = TransformLocalColPoint64(
            matrix,
            CVector(localSampleSurface.x, localSampleSurface.y, retainedSurfaceLocalZ));
        surfaceZ = worldSurface.z;
    }

    if (!std::isfinite(surfaceZ) ||
        surfaceZ < targetZ - 0.35f ||
        surfaceZ > origin->z + 0.45f) {
        ++result.rejectedZ;
        return false;
    }

    CPlayerPool* playerPool = pNetGame ? pNetGame->GetPlayerPool() : nullptr;
    CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
    CPlayerPed* localPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;
    if (localPed && localPed->m_pPed) {
        const CVector playerPos = localPed->m_pPed->GetPosition();
        const float maxSurfaceAbovePlayer = localPed->IsInVehicle() ? 5.50f : 2.25f;
        if (std::isfinite(playerPos.z) && surfaceZ > playerPos.z + maxSurfaceAbovePlayer) {
            ++result.rejectedZ;
            static uint32_t s_lastHighSurfaceSkipTick = 0;
            const uint32_t now = GetTickCount();
            if (now - s_lastHighSurfaceSkipTick >= 700) {
                s_lastHighSurfaceSkipTick = now;
                char modelName[22]{};
                CopyModelName64(modelName, modelInfo);
                FLog("[COL_SUPP64] skip high-surface mode=%s origin=%.2f %.2f %.2f target=%.2f z=%.2f player=%.2f %.2f %.2f maxAbove=%.2f model=%d,%s",
                     targetedSurface ? "box" : "transient",
                     origin->x,
                     origin->y,
                     origin->z,
                     targetZ,
                     surfaceZ,
                     playerPos.x,
                     playerPos.y,
                     playerPos.z,
                     maxSurfaceAbovePlayer,
                     entity->m_nModelIndex,
                     modelName[0] ? modelName : "?");
            }
            return false;
        }
    }

    CColPoint col{};
    col.m_vecPoint = CVector(origin->x, origin->y, surfaceZ);
    col.m_vecNormal = CVector(0.0f, 0.0f, 1.0f);
    col.m_nSurfaceTypeA = SURFACE_TARMAC;
    col.m_nSurfaceTypeB = SURFACE_TARMAC;
    col.m_nLightingA.value = 0xFF;
    col.m_nLightingB.value = 0xFF;
    col.m_fDepth = origin->z - surfaceZ;

    ++result.directHits;
    if (!result.entity || surfaceZ > result.z) {
        result.entity = entity;
        result.col = col;
        result.z = surfaceZ;
    }

    static uint32_t s_lastSupplementHitLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastSupplementHitLogTick >= 450) {
        s_lastSupplementHitLogTick = now;
        char modelName[22]{};
        CopyModelName64(modelName, modelInfo);
        FLog("[COL_SUPP64] vertical fixed mode=%s origin=%.2f %.2f %.2f target=%.2f z=%.2f model=%d,%s boxes=%u",
             targetedSurface ? "box" : "transient",
             origin->x,
             origin->y,
             origin->z,
             targetZ,
             surfaceZ,
             entity->m_nModelIndex,
             modelName[0] ? modelName : "?",
             colModel->m_pColData ? static_cast<unsigned>(colModel->m_pColData->m_nNumBoxes) : 0u);
    }
    return true;
}

static bool IsPlayerNearEntityCollisionBounds64(CEntityGTA* entity,
                                                CColModel* colModel,
                                                const CVector& playerPos,
                                                bool criticalSurface,
                                                CVector& worldMin,
                                                CVector& worldMax)
{
    if (!entity || !colModel) {
        return false;
    }

    ComputeEntityColWorldBounds64(entity, colModel, criticalSurface ? 18.0f : 9.0f, worldMin, worldMax);
    const bool insideXY =
        playerPos.x >= worldMin.x &&
        playerPos.x <= worldMax.x &&
        playerPos.y >= worldMin.y &&
        playerPos.y <= worldMax.y;
    const bool plausibleZ =
        playerPos.z >= worldMin.z - 160.0f &&
        playerPos.z <= worldMax.z + 190.0f;
    if (insideXY && plausibleZ) {
        return true;
    }

    const CVector entityPos = entity->GetPosition();
    const float dx = entityPos.x - playerPos.x;
    const float dy = entityPos.y - playerPos.y;
    const float radius = criticalSurface ? 230.0f : 95.0f;
    return dx * dx + dy * dy <= radius * radius &&
           playerPos.z >= worldMin.z - 190.0f &&
           playerPos.z <= worldMax.z + 220.0f;
}

static void StoreNearbyWorldCollisionLog64(NearbyWorldCollisionLog64 logs[],
                                           int& logCount,
                                           int maxLogs,
                                           const char* poolName,
                                           int slot,
                                           CEntityGTA* entity,
                                           CBaseModelInfo* modelInfo,
                                           CColModel* colModel,
                                           int32 expectedColSlot,
                                           bool beforeData,
                                           bool road,
                                           const CVector& worldMin,
                                           const CVector& worldMax)
{
    if (!logs || logCount >= maxLogs || !entity || !modelInfo || !colModel) {
        return;
    }

    NearbyWorldCollisionLog64& log = logs[logCount++];
    log.poolName = poolName;
    log.slot = slot;
    log.modelId = entity->m_nModelIndex;
    log.entityArea = static_cast<uint8_t>(entity->m_nAreaCode);
    log.colSlot = colModel->m_nColSlot;
    log.colFlags = colModel->m_nFlags;
    log.expectedColSlot = expectedColSlot > 0 && expectedColSlot < 256
                              ? static_cast<uint8_t>(expectedColSlot)
                              : 0;
    log.beforeData = beforeData;
    log.road = road;
    log.ownCol = modelInfo->bDoWeOwnTheColModel;
    log.modelFlags = modelInfo->m_nFlags;
    log.colModel = colModel;
    log.entityPos = entity->GetPosition();
    log.worldMin = worldMin;
    log.worldMax = worldMax;
    CopyModelName64(log.modelName, modelInfo);

    Supremo::Collision::ColSlotInfo slotInfo{};
    if (colModel->m_nColSlot > 0 &&
        Supremo::Collision::ReadSlotInfo(colModel->m_nColSlot, slotInfo) &&
        !slotInfo.empty) {
        log.colModelStart = slotInfo.modelIdStart;
        log.colModelEnd = slotInfo.modelIdEnd;
        log.colRefCount = slotInfo.refCount;
        log.colActive = slotInfo.active;
        log.colRequired = slotInfo.required;
        std::strncpy(log.colSlotName, slotInfo.name, sizeof(log.colSlotName) - 1);
        log.colSlotName[sizeof(log.colSlotName) - 1] = '\0';
    }

    const int32 colResourceId = Supremo::Streaming::ColModelIdFromSlot(colModel->m_nColSlot);
    if (Supremo::Streaming::IsValidResourceId(colResourceId)) {
        log.colLoadState = Supremo::Streaming::ReadModelDiagnostic(colResourceId).loadState;
    }

    const int32 expectedResourceId = log.expectedColSlot ? Supremo::Streaming::ColModelIdFromSlot(log.expectedColSlot) : -1;
    if (Supremo::Streaming::IsValidResourceId(expectedResourceId)) {
        const Supremo::Streaming::ModelStreamDiagnostic expectedInfo =
            Supremo::Streaming::ReadModelDiagnostic(expectedResourceId);
        log.expectedColLoadState = expectedInfo.loadState;
        log.expectedColImgId = expectedInfo.imgId;
        log.expectedColCdPosn = expectedInfo.cdPosn;
        log.expectedColCdSize = expectedInfo.cdSize;
        log.expectedColNextOnCd = expectedInfo.nextIndexOnCd;
    }
}

template <typename TPool>
static void RequestMissingNearbyWorldCollisionPool64(const char* poolName,
                                                     TPool* pool,
                                                     const CVector& playerPos,
                                                     float radius,
                                                     uint8_t playerArea,
                                                     int maxRequests,
                                                     bool immediate,
                                                     NearbyWorldCollisionStats64& stats,
                                                     NearbyWorldCollisionLog64 logs[],
                                                     int& logCount,
                                                     int maxLogs)
{
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0) {
        return;
    }

    const float radius2 = radius * radius;
    const float coarseRadius = radius < 390.0f ? 390.0f : radius;
    const float coarseRadius2 = coarseRadius * coarseRadius;
    for (int32 i = 0; i < pool->m_nSize; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        ++stats.scanned;
        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity) {
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        const float dx = entityPos.x - playerPos.x;
        const float dy = entityPos.y - playerPos.y;
        const float dz = entityPos.z - playerPos.z;
        const float dist2d = dx * dx + dy * dy;
        if (dist2d > coarseRadius2) {
            continue;
        }

        if (!IsWorldEntityAreaActive64(entity, playerArea)) {
            ++stats.skippedArea;
            continue;
        }

        const int modelId = entity->m_nModelIndex;
        if (!Supremo::Streaming::IsValidResourceId(modelId)) {
            ++stats.badModel;
            continue;
        }
        if (IsTempVehicleComponentModel64(modelId)) {
            SuppressTempVehicleComponentCollision64(entity);
            ++stats.skippedTempVehicleParts;
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
        if (!modelInfo) {
            ++stats.noModelInfo;
            continue;
        }
        if (IsUnstableBreakableWorldProp64(modelId, modelInfo)) {
            PreserveBreakableWorldPropCollision64(entity);
            ++stats.skippedUnstableProps;
            continue;
        }
        if (IsLodWorldModel64(modelInfo)) {
            ++stats.skippedLod;
            continue;
        }

        const float modelRadius = IsTargetedHighSurfaceModel64(modelInfo) && radius < 390.0f
            ? 390.0f
            : radius;
        const float modelRadius2 = modelRadius * modelRadius;
        if (dist2d > modelRadius2) {
            continue;
        }

        CColModel* colModel = modelInfo->m_pColModel;
        if (!colModel) {
            ++stats.noColModel;
            continue;
        }

        const bool criticalSurface = IsCriticalNearbySurfaceModel64(modelInfo);
        const bool propCollision = IsLikelyPhysicalPropModel64(modelInfo);
        if (!criticalSurface && dist2d + dz * dz > modelRadius2) {
            continue;
        }
        ++stats.near;
        CVector worldMin{};
        CVector worldMax{};
        if (!IsPlayerNearEntityCollisionBounds64(entity, colModel, playerPos, criticalSurface, worldMin, worldMax)) {
            ++stats.skippedBounds;
            continue;
        }

        char modelName[22]{};
        CopyModelName64(modelName, modelInfo);
        const int32 expectedColSlot = GetExpectedAirportColSlot64(modelId, modelInfo);
        const int32 currentColSlot = colModel->m_nColSlot;
        const bool beforeData = colModel->m_pColData != nullptr;
        const uint32_t beforeTriCount = colModel->m_pColData ? colModel->m_pColData->m_nNumTriangles : 0;
        const bool protectEntity =
            criticalSurface ||
            IsWatchedWorldModel64(modelId) ||
            IsCoreBallysCollisionModel64(modelId);

        entity->m_bUsesCollision = true;
        if (protectEntity) {
            entity->m_bDontStream = false;
        }
        if (protectEntity && !entity->m_bStreamingDontDelete) {
            entity->m_bStreamingDontDelete = true;
            ++stats.protectedEntities;
        }
        if (!entity->m_bIsVisible && ShouldRestoreWorldEntityVisibility64(entity, modelInfo)) {
            entity->m_bIsVisible = true;
            ++stats.unhidden;
        }

        if (protectEntity) {
            Supremo::Streaming::MarkModelDontRemoveInLoadScene(modelId);
            if (!Supremo::Streaming::IsModelLoaded(modelId) || !modelInfo->m_pRwObject) {
                Supremo::Streaming::RequestModel(modelId, STREAMING_DONTREMOVE_IN_LOADSCENE);
                ++stats.keptModel;
            }
        } else if (!Supremo::Streaming::IsModelLoaded(modelId) || !modelInfo->m_pRwObject) {
            Supremo::Streaming::RequestModel(modelId, STREAMING_DEFAULT);
            ++stats.requested;
        }

        const bool slotMismatch =
            criticalSurface &&
            expectedColSlot > 0 &&
            expectedColSlot < 256 &&
            expectedColSlot != currentColSlot;
        const bool loadedWrongAirportSlot = beforeData && slotMismatch;
        if ((!beforeData || loadedWrongAirportSlot) && slotMismatch) {
            colModel->m_nColSlot = static_cast<uint8_t>(expectedColSlot);
            ++stats.remappedSlots;
            if (loadedWrongAirportSlot) {
                ++stats.mismatchedData;
                MarkNearbyColSlotReload64(stats, expectedColSlot);
                MarkNearbyColSlotReload64(stats, currentColSlot);
            }
            FLog("[COL_SLOT_REMAP64] pool=%s slot=%d model=%d,%s colSlot=%d->%d data=%d tris=%u mode=%s pos=%.2f %.2f %.2f",
                 poolName ? poolName : "?",
                 i,
                 modelId,
                 modelName[0] ? modelName : "?",
                 currentColSlot,
                 expectedColSlot,
                 beforeData ? 1 : 0,
                 beforeTriCount,
                 loadedWrongAirportSlot ? "loaded-mismatch" : "missing-data",
                 playerPos.x,
                 playerPos.y,
                 playerPos.z);
        }

        auto touchColSlot = [&](int32 slot, bool hinted) {
            Supremo::Collision::ColSlotInfo slotInfo{};
            if (slot <= 0 ||
                slot >= 256 ||
                !Supremo::Collision::ReadSlotInfo(slot, slotInfo) ||
                slotInfo.empty) {
                return;
            }

            Supremo::Collision::IncludeModelIndex(slot, modelId);
            if (protectEntity) {
                RetainWatchedColSlot64(slot, modelId, modelName, playerPos);
                ++stats.retainedSlots;
            }
            if (hinted) {
                ++stats.hintedSlots;
            }

            const int32 colModelId = Supremo::Streaming::ColModelIdFromSlot(slot);
            if (protectEntity && Supremo::Streaming::IsValidResourceId(colModelId)) {
                Supremo::Streaming::MarkModelDontRemoveInLoadScene(colModelId);
                if (!Supremo::Streaming::IsModelLoaded(colModelId)) {
                    Supremo::Streaming::RequestModel(colModelId, STREAMING_DONTREMOVE_IN_LOADSCENE);
                }
            }
        };

        touchColSlot(currentColSlot, false);
        if (expectedColSlot > 0 && expectedColSlot != currentColSlot) {
            touchColSlot(expectedColSlot, true);
        }

        if (beforeData) {
            ++stats.alreadyData;
            if (criticalSurface || propCollision) {
                RefreshWorldEntityCollisionRegistration64(entity, modelInfo, "near-existing-real");
            }
            if (criticalSurface &&
                expectedColSlot > 0 &&
                expectedColSlot == currentColSlot &&
                IsVegasWestKnownColSlot64(currentColSlot)) {
                MarkNearbyColSlotReload64(stats, currentColSlot);
            }
            if (criticalSurface &&
                IsTargetedHighSurfaceModel64(modelInfo) &&
                EnsureSupplementalSurfaceColBox64(entity, modelInfo, colModel, playerPos, "near-existing")) {
                ++stats.synthesizedData;
            }
            if ((criticalSurface || propCollision) && logCount < maxLogs) {
                StoreNearbyWorldCollisionLog64(logs, logCount, maxLogs, poolName, i, entity, modelInfo, colModel, expectedColSlot, true, criticalSurface, worldMin, worldMax);
            }
            continue;
        }

        ++stats.missingData;
        if (currentColSlot > 0 && currentColSlot < 256) {
            MarkNearbyColSlotReload64(stats, currentColSlot);
        }
        if (stats.requested >= maxRequests) {
            if ((criticalSurface &&
                 EnsureSyntheticSurfaceColData64(entity, modelInfo, colModel, playerPos, "near-limit")) ||
                (propCollision &&
                 EnsureSyntheticPropColData64(entity, modelInfo, colModel, playerPos, "near-limit"))) {
                ++stats.synthesizedData;
                StoreNearbyWorldCollisionLog64(logs, logCount, maxLogs, poolName, i, entity, modelInfo, colModel, expectedColSlot, false, criticalSurface, worldMin, worldMax);
            }
            ++stats.limited;
            continue;
        }

        if (criticalSurface && expectedColSlot > 0) {
            MarkNearbyColSlotReload64(stats, expectedColSlot);
            if (expectedColSlot > 0 && expectedColSlot != currentColSlot) {
                MarkNearbyColSlotReload64(stats, currentColSlot);
            }
        }
        RequestEntityCollisionSamples64(entity, colModel, playerPos, playerArea);
        if (immediate) {
            Supremo::Streaming::LoadAllRequestedModels(false);
        }
        if (((criticalSurface && !colModel->m_pColData &&
              EnsureSyntheticSurfaceColData64(entity, modelInfo, colModel, playerPos, "near-request")) ||
             (propCollision && !colModel->m_pColData &&
              EnsureSyntheticPropColData64(entity, modelInfo, colModel, playerPos, "near-request")))) {
            ++stats.synthesizedData;
        }
        if (criticalSurface || propCollision) {
            StoreNearbyWorldCollisionLog64(logs, logCount, maxLogs, poolName, i, entity, modelInfo, colModel, expectedColSlot, false, criticalSurface, worldMin, worldMax);
        }
        ++stats.requested;
    }
}

static void RequestMissingNearbyWorldCollision64(const CVector& pos,
                                                 uint8_t playerArea,
                                                 uint32_t now,
                                                 bool immediate)
{
    static uint32_t s_lastNearbyRequestTick = 0;
    static uint32_t s_lastNearbySummaryTick = 0;
    static uint32_t s_lastNearbyDetailTick = 0;
    static bool s_hasLastNearbyRequestPos = false;
    static CVector s_lastNearbyRequestPos(0.0f, 0.0f, 0.0f);

    if (!immediate) {
        const uint32_t elapsed = now - s_lastNearbyRequestTick;
        const bool movedEnough =
            !s_hasLastNearbyRequestPos ||
            Distance2DSquared64(pos, s_lastNearbyRequestPos) >= 18.0f * 18.0f ||
            fabsf(pos.z - s_lastNearbyRequestPos.z) >= 4.0f;
        if ((elapsed < 1600u || !movedEnough) && elapsed < 5000u) {
            return;
        }
    }
    s_lastNearbyRequestTick = now;
    s_lastNearbyRequestPos = pos;
    s_hasLastNearbyRequestPos = true;

    constexpr int kMaxLogs = 10;
    NearbyWorldCollisionLog64 logs[kMaxLogs]{};
    int logCount = 0;
    NearbyWorldCollisionStats64 stats{};
    const int maxRequests = immediate ? 48 : 10;
    const int activeMaxLogs = kArm64CollisionDeepDiagnosticsEnabled ? kMaxLogs : 0;

    if (kArm64CollisionDeepDiagnosticsEnabled) {
        LogAirportColSlots64(pos, now);
    }
    RequestMissingNearbyWorldCollisionPool64("B", GetBuildingPool(), pos, immediate ? 320.0f : 250.0f, playerArea, maxRequests, immediate, stats, logs, logCount, activeMaxLogs);
    RequestMissingNearbyWorldCollisionPool64("O", GetObjectPoolGta(), pos, immediate ? 230.0f : 190.0f, playerArea, maxRequests, immediate, stats, logs, logCount, activeMaxLogs);
    RequestMissingNearbyWorldCollisionPool64("D", GetDummyPool(), pos, immediate ? 230.0f : 190.0f, playerArea, maxRequests, immediate, stats, logs, logCount, activeMaxLogs);

    ReloadPendingNearbyColSlots64(stats, pos, now, immediate);
    WarmupActualNearbyCollision64(pos, playerArea, stats, now, immediate);

    const bool hasNearbyCollisionWork =
        stats.requested > 0 ||
        stats.missingData > 0 ||
        stats.synthesizedData > 0 ||
        stats.reloadRequested > 0 ||
        stats.reloadPending > 0 ||
        stats.remappedSlots > 0 ||
        stats.mismatchedData > 0;
    const bool emitDetailedLogs =
        kArm64CollisionDeepDiagnosticsEnabled &&
        ((immediate && now - s_lastNearbyDetailTick >= 5000) ||
         stats.synthesizedData > 0 ||
         (hasNearbyCollisionWork && now - s_lastNearbyDetailTick >= 5000));
    for (int i = 0; i < logCount; ++i) {
        NearbyWorldCollisionLog64& log = logs[i];
        CCollisionData* afterData = log.colModel ? log.colModel->m_pColData : nullptr;
        if (afterData) {
            ++stats.loadedAfter;
        }
        if (!emitDetailedLogs) {
            continue;
        }

        FLog("[COL_NEAR_REQ64] mode=%s pool=%s slot=%d model=%d,%s area=%u colSlot=%u,%s colState=%u expSlot=%u expState=%u expCd={img:%u,pos:%u,size:%u,next:%d} colActive=%d colReq=%d colRef=%u range=%d-%d colFlags=0x%02X data=%d->%d tris=%u boxes=%u lines=%u road=%d ownCol=%d miFlags=0x%04X ent=%.2f %.2f %.2f world={%.2f %.2f %.2f -> %.2f %.2f %.2f} player=%.2f %.2f %.2f",
             immediate ? "spawn" : "runtime",
             log.poolName ? log.poolName : "?",
             log.slot,
             log.modelId,
             log.modelName[0] ? log.modelName : "?",
             static_cast<unsigned>(log.entityArea),
             static_cast<unsigned>(log.colSlot),
             log.colSlotName[0] ? log.colSlotName : "?",
             static_cast<unsigned>(log.colLoadState),
             static_cast<unsigned>(log.expectedColSlot),
             static_cast<unsigned>(log.expectedColLoadState),
             static_cast<unsigned>(log.expectedColImgId),
             log.expectedColCdPosn,
             log.expectedColCdSize,
             static_cast<int>(log.expectedColNextOnCd),
             log.colActive ? 1 : 0,
             log.colRequired ? 1 : 0,
             static_cast<unsigned>(log.colRefCount),
             static_cast<int>(log.colModelStart),
             static_cast<int>(log.colModelEnd),
             static_cast<unsigned>(log.colFlags),
             log.beforeData ? 1 : 0,
             afterData ? 1 : 0,
             afterData ? static_cast<unsigned>(afterData->m_nNumTriangles) : 0u,
             afterData ? static_cast<unsigned>(afterData->m_nNumBoxes) : 0u,
             afterData ? static_cast<unsigned>(afterData->m_nNumLines) : 0u,
             log.road ? 1 : 0,
             log.ownCol ? 1 : 0,
             static_cast<unsigned>(log.modelFlags),
             log.entityPos.x,
             log.entityPos.y,
             log.entityPos.z,
             log.worldMin.x,
             log.worldMin.y,
             log.worldMin.z,
             log.worldMax.x,
             log.worldMax.y,
             log.worldMax.z,
             pos.x,
             pos.y,
             pos.z);
    }
    if (emitDetailedLogs && logCount > 0) {
        s_lastNearbyDetailTick = now;
    }

    const bool hasNearbyKeepAlive =
        stats.retainedSlots > 0 ||
        stats.protectedEntities > 0 ||
        stats.hintedSlots > 0;
    const uint32_t nearbySummaryInterval = hasNearbyCollisionWork ? 3500u : 12000u;
    const bool emitImmediateSummary =
        immediate &&
        (hasNearbyCollisionWork || now - s_lastNearbySummaryTick >= 5000);
    if (kArm64CollisionRuntimeSummaryLogsEnabled &&
        (emitImmediateSummary ||
        ((hasNearbyCollisionWork || hasNearbyKeepAlive) &&
         now - s_lastNearbySummaryTick >= nearbySummaryInterval))) {
        s_lastNearbySummaryTick = now;
        FLog("[COL_NEAR_SUM64] mode=%s pos=%.2f %.2f %.2f area=%u scanned=%d near=%d skip={area:%d,lod:%d,tempVeh:%d,unstable:%d,bad:%d,noMI:%d,noCol:%d,bounds:%d,limit:%d} keep={model:%d,slot:%d,hint:%d,remap:%d,mismatch:%d,protect:%d,unhide:%d} data={ok:%d,miss:%d,loaded:%d,synth:%d} req=%d reload={pending:%d,req:%d,throttle:%d} mem=%zu/%zu onlyBB=%d",
             immediate ? "spawn" : "runtime",
             pos.x,
             pos.y,
             pos.z,
             static_cast<unsigned>(playerArea),
             stats.scanned,
             stats.near,
             stats.skippedArea,
             stats.skippedLod,
             stats.skippedTempVehicleParts,
             stats.skippedUnstableProps,
             stats.badModel,
             stats.noModelInfo,
             stats.noColModel,
             stats.skippedBounds,
             stats.limited,
             stats.keptModel,
             stats.retainedSlots,
             stats.hintedSlots,
             stats.remappedSlots,
             stats.mismatchedData,
             stats.protectedEntities,
             stats.unhidden,
             stats.alreadyData,
             stats.missingData,
             stats.loadedAfter,
             stats.synthesizedData,
             stats.requested,
             stats.reloadPending,
             stats.reloadRequested,
             stats.reloadThrottled,
             Supremo::Streaming::MemoryUsed(),
             Supremo::Streaming::MemoryAvailable(),
             Supremo::Collision::OnlyBoundingBoxes() ? 1 : 0);
    }
}

extern "C" void RequestArm64WorldCollisionForPosition(const CVector& pos, uint8_t area, bool immediate)
{
#if !VER_x32
    RequestNearbyWorldCollisionAreas64(pos, area);
    RequestMissingNearbyWorldCollision64(pos, area, GetTickCount(), immediate);
#else
    (void)pos;
    (void)area;
    (void)immediate;
#endif
}

extern "C" void WarmKnownArm64CollisionSlotsForPosition(const CVector& pos, uint8_t area)
{
#if !VER_x32
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) {
        return;
    }

    if (Supremo::Collision::PoolSize() <= 0) {
        return;
    }

    Supremo::Streaming::WorldStreamRequest streamRequest{};
    streamRequest.areaCode = static_cast<int32>(area);
    streamRequest.streamingFlags = STREAMING_DEFAULT;
    streamRequest.reason = "known-col-slots";
    Supremo::Streaming::PrepareWorldAtPoint(pos, streamRequest);
    RequestNearbyWorldCollisionAreas64(pos, area);

    AirportColSlots64& slots = GetAirportColSlots64();
    auto warmSlot = [&](int32 slot, const char* name) {
        Supremo::Collision::ColSlotInfo slotInfo{};
        if (slot <= 0 ||
            slot >= 256 ||
            !name ||
            !Supremo::Collision::ReadSlotInfo(slot, slotInfo) ||
            slotInfo.empty) {
            return;
        }
        IncludeKnownModelIndexesForColSlot64(slot);
        const int32 colModelId = Supremo::Streaming::ColModelIdFromSlot(slot);
        const bool loaded =
            Supremo::Streaming::IsValidResourceId(colModelId) &&
            Supremo::Streaming::IsModelLoaded(colModelId);
        if (!loaded || !slotInfo.active) {
            Supremo::Collision::LoadCol(slot, name);
        }
    };

    const bool nearLosSantosAirport =
        pos.x >= 850.0f && pos.x <= 1900.0f &&
        pos.y >= -2850.0f && pos.y <= -1850.0f;
    if (nearLosSantosAirport) {
        warmSlot(slots.las1, "las_1");
        warmSlot(slots.las2, "las_2");
        warmSlot(slots.las3, "las_3");
        warmSlot(slots.las4, "las_4");
        warmSlot(slots.las5, "las_5");
    }

    Supremo::Streaming::PrepareWorldAtPoint(pos, streamRequest);
#else
    (void)pos;
    (void)area;
#endif
}

template <typename TPool>
static void RequestMissingWatchedCollisionPool64(const char* poolName,
                                                 TPool* pool,
                                                 const CVector& playerPos,
                                                 float radius,
                                                 uint8_t playerArea,
                                                 int& checked,
                                                 int& missing,
                                                 int& requested)
{
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0 || requested >= 10) {
        return;
    }

    const float radius2 = radius * radius;
    for (int32 i = 0; i < pool->m_nSize && requested < 10; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity || !IsCoreBallysCollisionModel64(entity->m_nModelIndex)) {
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        if (DistanceSquared3D(entityPos, playerPos) > radius2) {
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(entity->m_nModelIndex);
        CColModel* colModel = modelInfo ? modelInfo->m_pColModel : nullptr;
        if (!colModel) {
            continue;
        }

        char modelName[22]{};
        CopyModelName64(modelName, modelInfo);
        if (colModel->m_nColSlot > 0 && colModel->m_nColSlot < 256) {
            Supremo::Collision::IncludeModelIndex(colModel->m_nColSlot, entity->m_nModelIndex);
            RetainWatchedColSlot64(colModel->m_nColSlot, entity->m_nModelIndex, modelName, playerPos);
        }

        ++checked;
        CCollisionData* beforeData = colModel->m_pColData;
        if (beforeData) {
            continue;
        }
        if (!IsBallysRequiredColDataModel64(entity->m_nModelIndex)) {
            continue;
        }

        ++missing;
        if (Supremo::Streaming::IsValidResourceId(entity->m_nModelIndex)) {
            Supremo::Streaming::MarkModelDontRemoveInLoadScene(entity->m_nModelIndex);
            Supremo::Streaming::RequestModel(entity->m_nModelIndex, STREAMING_DONTREMOVE_IN_LOADSCENE);
        }

        RequestEntityCollisionSamples64(entity, colModel, playerPos, playerArea);
        Supremo::Streaming::LoadAllRequestedModels(false);

        const CVector center = GetEntityColWorldCenter64(entity, colModel);
        const CVector playerInside = ClampPointToEntityColBounds64(
            entity,
            colModel,
            CVector(playerPos.x, playerPos.y, center.z));
        const CCollisionData* afterData = colModel->m_pColData;

        FLog("[COL_ENTITY_REQ64] pool=%s slot=%d model=%d,%s area=%u colSlot=%u colFlags=0x%02X colVol=%d colActive=%d ownCol=%d road=%d miFlags=0x%04X data=%d->%d ent=%.2f %.2f %.2f center=%.2f %.2f %.2f playerIn=%.2f %.2f %.2f tris=%u",
             poolName ? poolName : "?",
             i,
             entity->m_nModelIndex,
             modelName[0] ? modelName : "?",
             static_cast<unsigned>(entity->m_nAreaCode),
             static_cast<unsigned>(colModel->m_nColSlot),
             static_cast<unsigned>(colModel->m_nFlags),
             colModel->m_bHasCollisionVolumes ? 1 : 0,
             colModel->m_bIsActive ? 1 : 0,
             modelInfo && modelInfo->bDoWeOwnTheColModel ? 1 : 0,
             modelInfo && modelInfo->bIsRoad ? 1 : 0,
             modelInfo ? static_cast<unsigned>(modelInfo->m_nFlags) : 0,
             beforeData ? 1 : 0,
             afterData ? 1 : 0,
             entityPos.x,
             entityPos.y,
             entityPos.z,
             center.x,
             center.y,
             center.z,
             playerInside.x,
             playerInside.y,
             playerInside.z,
             afterData ? static_cast<unsigned>(afterData->m_nNumTriangles) : 0);
        ++requested;
    }
}

static void RequestMissingWatchedEntityCollision64(const CVector& pos, uint8_t playerArea, uint32_t now)
{
    static uint32_t s_lastEntityRequestTick = 0;
    static uint32_t s_lastSummaryTick = 0;
    static bool s_triedLoadAllCollision = false;
    static const CVector kBallysSpawn64(1958.3783f, 1343.1572f, 15.3746f);

    if (Distance2DSquared64(pos, kBallysSpawn64) > 125.0f * 125.0f ||
        now - s_lastEntityRequestTick < 1600) {
        return;
    }
    s_lastEntityRequestTick = now;

    int checked = 0;
    int missing = 0;
    int requested = 0;
    RequestMissingWatchedCollisionPool64("B", GetBuildingPool(), pos, 230.0f, playerArea, checked, missing, requested);
    RequestMissingWatchedCollisionPool64("O", GetObjectPoolGta(), pos, 180.0f, playerArea, checked, missing, requested);
    RequestMissingWatchedCollisionPool64("D", GetDummyPool(), pos, 180.0f, playerArea, checked, missing, requested);

    if (missing > 0 && !s_triedLoadAllCollision) {
        s_triedLoadAllCollision = true;
        Supremo::Collision::LoadAllCollision();
        FLog("[COL_LOADALL64] requested because watched models still miss colData pos=%.2f %.2f %.2f checked=%d missing=%d onlyBB=%d",
             pos.x,
             pos.y,
             pos.z,
             checked,
             missing,
             Supremo::Collision::OnlyBoundingBoxes() ? 1 : 0);
    }

    if ((requested > 0 || missing > 0) && now - s_lastSummaryTick >= 3200) {
        s_lastSummaryTick = now;
        FLog("[COL_ENTITY_REQ_SUM64] pos=%.2f %.2f %.2f area=%u checked=%d missing=%d requested=%d",
             pos.x,
             pos.y,
             pos.z,
             static_cast<unsigned>(playerArea),
             checked,
             missing,
             requested);
    }
}

static void LogEntityVerticalProbes64(const CVector& pos, uint32_t now)
{
    static uint32_t s_lastEntityProbeTick = 0;
    static const CVector kBallysSpawn64(1958.3783f, 1343.1572f, 15.3746f);
    static const CVector kVegasWestColProbe64(1637.391f, 2155.0f, 17.82f);

    const bool nearBallys = Distance2DSquared64(pos, kBallysSpawn64) <= 115.0f * 115.0f;
    const bool nearVegasWest = Distance2DSquared64(pos, kVegasWestColProbe64) <= 125.0f * 125.0f;
    if ((!nearBallys && !nearVegasWest) ||
        now - s_lastEntityProbeTick < 5000) {
        return;
    }
    s_lastEntityProbeTick = now;

    struct SampleOffset64 {
        float x;
        float y;
    };
    static constexpr SampleOffset64 kOffsets[] = {
        {0.0f, 0.0f},
        {6.7f, -0.1f},
        {8.0f, 8.0f},
        {-8.0f, 8.0f},
        {0.0f, -8.0f},
    };

    for (const SampleOffset64& offset : kOffsets) {
        const CVector sample(pos.x + offset.x, pos.y + offset.y, pos.z);
        int logged = 0;
        LogEntityVerticalProbePool64("B", GetBuildingPool(), sample, 230.0f, logged);
        LogEntityVerticalProbePool64("O", GetObjectPoolGta(), sample, 180.0f, logged);
        LogEntityVerticalProbePool64("D", GetDummyPool(), sample, 180.0f, logged);
    }
}

static int CountEntityInPtrList64(const CPtrList& list, CEntityGTA* entity, int maxNodes = 4096)
{
    if (!entity) {
        return 0;
    }

    int count = 0;
    for (CPtrNode* node = list.GetNode(); node && maxNodes-- > 0; node = node->GetNext()) {
        if (node->m_item == entity) {
            ++count;
        }
    }
    return count;
}

static uint16_t GetWorldCurrentScanCode64()
{
    if (VER_x32 || !g_libGTASA) {
        return 0;
    }
    return GTASAEngineApi::WorldCurrentScanCode();
}

static CEntityGTA* GetWorldIgnoreEntity64()
{
    if (VER_x32 || !g_libGTASA) {
        return nullptr;
    }
    return GTASAEngineApi::WorldIgnoreEntity();
}

static bool EnsureStaticEntityInProbeSector64(CEntityGTA* entity,
                                              const CVector& probePos,
                                              int& beforeCount,
                                              int& afterCount,
                                              int& sectorX,
                                              int& sectorY)
{
    beforeCount = 0;
    afterCount = 0;
    sectorX = CWorld::GetSectorX(probePos.x);
    sectorY = CWorld::GetSectorY(probePos.y);

    if (!entity || entity->IsPed() || entity->IsVehicle()) {
        return false;
    }

    CSector* sector = GetSector(sectorX, sectorY);
    if (!sector) {
        return false;
    }

    if (entity->IsBuilding()) {
        CPtrList& buildingList = sector->m_buildings;
        beforeCount = CountEntityInPtrList64(buildingList, entity);
        if (beforeCount <= 0) {
            reinterpret_cast<CPtrListSingleLink*>(&sector->m_buildings)->AddItem(entity);
        }
        afterCount = CountEntityInPtrList64(buildingList, entity);
        return afterCount > beforeCount;
    }

    if (entity->IsDummy()) {
        CPtrList& dummyList = sector->m_dummies;
        beforeCount = CountEntityInPtrList64(dummyList, entity);
        if (beforeCount <= 0) {
            sector->m_dummies.AddItem(entity);
        }
        afterCount = CountEntityInPtrList64(dummyList, entity);
        return afterCount > beforeCount;
    }

    return false;
}

static bool RepairWorldEntitySectorRegistration64(CEntityGTA* entity,
                                                  CBaseModelInfo* modelInfo,
                                                  const CVector& probePos,
                                                  float directZ,
                                                  const char* reason,
                                                  uint32_t now)
{
    if (!entity ||
        !modelInfo ||
        entity->IsPed() ||
        entity->IsVehicle() ||
        entity->m_bRemoveFromWorld ||
        IsLodWorldModel64(modelInfo) ||
        IsUnstableBreakableWorldProp64(entity->m_nModelIndex, modelInfo)) {
        return false;
    }

    CColModel* colModel = modelInfo->m_pColModel;
    if (!colModel || !colModel->m_pColData) {
        return false;
    }

    const bool criticalSurface = IsCriticalNearbySurfaceModel64(modelInfo);
    const bool physicalProp = IsLikelyPhysicalPropModel64(modelInfo);
    if (!criticalSurface && !physicalProp) {
        return false;
    }

    struct SectorRepair64 {
        CEntityGTA* entity = nullptr;
        int32 modelId = -1;
        uint32_t lastTick = 0;
    };

    static SectorRepair64 s_repairs[256]{};
    int freeIndex = -1;
    int oldestIndex = 0;
    uint32_t oldestTick = 0xFFFFFFFFu;
    for (int i = 0; i < static_cast<int>(sizeof(s_repairs) / sizeof(s_repairs[0])); ++i) {
        SectorRepair64& repair = s_repairs[i];
        if (repair.entity == entity && repair.modelId == entity->m_nModelIndex) {
            if (now - repair.lastTick < 8000) {
                return false;
            }
            repair.lastTick = now;
            goto do_repair;
        }
        if (!repair.entity && freeIndex < 0) {
            freeIndex = i;
        }
        if (repair.lastTick < oldestTick) {
            oldestTick = repair.lastTick;
            oldestIndex = i;
        }
    }

    {
        SectorRepair64& repair = s_repairs[freeIndex >= 0 ? freeIndex : oldestIndex];
        repair.entity = entity;
        repair.modelId = entity->m_nModelIndex;
        repair.lastTick = now;
    }

do_repair:
    entity->SetCollisionChecking(true);
    if (entity->IsBuilding() || entity->IsDummy()) {
        entity->m_bIsStatic = true;
    }
    entity->m_bIsStaticWaitingForCollision = false;
    entity->m_bDontStream = false;
    if (criticalSurface) {
        entity->m_bStreamingDontDelete = true;
    }
    entity->m_nScanCode = 0;

    const uint32_t beforeFlags = entity->m_nFlags;
    CVector worldMin{};
    CVector worldMax{};
    ComputeEntityColWorldBounds64(entity, colModel, criticalSurface ? 18.0f : 6.0f, worldMin, worldMax);
    char modelName[22]{};
    CopyModelName64(modelName, modelInfo);

    CRect sectorRect(
        worldMin.x - 4.0f,
        worldMin.y - 4.0f,
        worldMax.x + 4.0f,
        worldMax.y + 4.0f);
    CWorld::Remove(entity);
    entity->Add(&sectorRect);
    entity->m_nScanCode = 0;
    int probeSectorBefore = 0;
    int probeSectorAfter = 0;
    int probeSectorX = 0;
    int probeSectorY = 0;
    const bool injectedProbeSector = EnsureStaticEntityInProbeSector64(
        entity,
        probePos,
        probeSectorBefore,
        probeSectorAfter,
        probeSectorX,
        probeSectorY);
    entity->m_nScanCode = 0;

    CColPoint postCol{};
    CEntityGTA* postEntity = nullptr;
    CStoredCollPoly postStored{};
    CVector postOrigin(
        probePos.x,
        probePos.y,
        fmaxf(probePos.z + 8.0f, directZ + 28.0f));
    const uint16_t scanBeforePost = GetWorldCurrentScanCode64();
    const uint16_t entityScanBeforePost = entity->m_nScanCode;
    CEntityGTA* ignoreBeforePost = GetWorldIgnoreEntity64();
    const bool postHit = CWorld::ProcessVerticalLine(
        &postOrigin,
        directZ - 8.0f,
        &postCol,
        &postEntity,
        true,
        false,
        false,
        true,
        true,
        false,
        &postStored);
    const uint16_t scanAfterPost = GetWorldCurrentScanCode64();

    FLog("[COL_RESECTOR64] reason=%s model=%d,%s type=%u colSlot=%u tris=%u boxes=%u directZ=%.2f post={hit:%d,z:%.2f,model:%d} probe=%.2f %.2f %.2f ent=%.2f %.2f %.2f rect={%.2f %.2f %.2f %.2f} sector={%d,%d,%d->%d,inject:%d} flags=0x%08X->0x%08X",
         reason ? reason : "?",
         entity->m_nModelIndex,
         modelName[0] ? modelName : "?",
         static_cast<unsigned>(entity->GetType()),
         static_cast<unsigned>(colModel->m_nColSlot),
         static_cast<unsigned>(colModel->m_pColData ? colModel->m_pColData->m_nNumTriangles : 0),
         static_cast<unsigned>(colModel->m_pColData ? colModel->m_pColData->m_nNumBoxes : 0),
         directZ,
         postHit ? 1 : 0,
         postHit ? postCol.m_vecPoint.z : 0.0f,
         postEntity ? postEntity->m_nModelIndex : -1,
         probePos.x,
         probePos.y,
         probePos.z,
         entity->GetPosition().x,
         entity->GetPosition().y,
         entity->GetPosition().z,
         sectorRect.left,
         sectorRect.bottom,
         sectorRect.right,
         sectorRect.top,
         probeSectorX,
         probeSectorY,
         probeSectorBefore,
         probeSectorAfter,
         injectedProbeSector ? 1 : 0,
         beforeFlags,
         entity->m_nFlags);
    FLog("[COL_RESECTOR_STATE64] model=%d sector=%d,%d count=%d->%d inject=%d scan=%u->%u entityScan=%u ignore=%d/%d post=%d postModel=%d",
         entity->m_nModelIndex,
         probeSectorX,
         probeSectorY,
         probeSectorBefore,
         probeSectorAfter,
         injectedProbeSector ? 1 : 0,
         static_cast<unsigned>(scanBeforePost),
         static_cast<unsigned>(scanAfterPost),
         static_cast<unsigned>(entityScanBeforePost),
         ignoreBeforePost ? ignoreBeforePost->m_nModelIndex : -1,
         ignoreBeforePost == entity ? 1 : 0,
         postHit ? 1 : 0,
         postEntity ? postEntity->m_nModelIndex : -1);

    return true;
}

struct DirectVerticalCandidateDiag64 {
    CEntityGTA* entity = nullptr;
    int modelId = -1;
    char modelName[22]{};
    float z = -100000.0f;
    CVector entityPos{};
    CVector localSample{};
    uint8_t entityType = ENTITY_TYPE_NOTHING;
    uint8_t entityArea = 0;
    uint8_t colSlot = 0;
    uint32_t triCount = 0;
    uint32_t boxCount = 0;
    uint32_t flags = 0;
};

static void StoreDirectVerticalCandidate64(DirectVerticalCandidateDiag64* candidates,
                                           int maxCandidates,
                                           const DirectVerticalCandidateDiag64& candidate)
{
    if (!candidates || maxCandidates <= 0 || !candidate.entity || !std::isfinite(candidate.z)) {
        return;
    }

    int insertAt = -1;
    for (int i = 0; i < maxCandidates; ++i) {
        if (!candidates[i].entity || candidate.z > candidates[i].z) {
            insertAt = i;
            break;
        }
    }
    if (insertAt < 0) {
        return;
    }

    for (int i = maxCandidates - 1; i > insertAt; --i) {
        candidates[i] = candidates[i - 1];
    }
    candidates[insertAt] = candidate;
}

template <typename TPool>
static void CollectDirectVerticalCandidatesFromPool64(TPool* pool,
                                                      CVector* origin,
                                                      float targetZ,
                                                      uint8_t playerArea,
                                                      DirectVerticalCandidateDiag64* candidates,
                                                      int maxCandidates,
                                                      int& scanned,
                                                      int& hits)
{
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || pool->m_nSize <= 0 ||
        !origin || !candidates || maxCandidates <= 0) {
        return;
    }

    for (int32 i = 0; i < pool->m_nSize; ++i) {
        if (pool->m_byteMap[i].bEmpty) {
            continue;
        }

        typename TPool::base_type* baseEntity = static_cast<typename TPool::base_type*>(&pool->m_pObjects[i]);
        CEntityGTA* entity = static_cast<CEntityGTA*>(baseEntity);
        if (!entity ||
            !IsEntityAreaActiveForVerticalFallback64(static_cast<uint8_t>(entity->m_nAreaCode), playerArea)) {
            continue;
        }

        const int modelId = entity->m_nModelIndex;
        if (modelId < 0 ||
            !Supremo::Streaming::IsValidResourceId(modelId) ||
            IsTempVehicleComponentModel64(modelId)) {
            continue;
        }

        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
        if (!modelInfo ||
            IsUnstableBreakableWorldProp64(modelId, modelInfo) ||
            IsLodWorldModel64(modelInfo)) {
            continue;
        }

        const CVector entityPos = entity->GetPosition();
        const float dx = entityPos.x - origin->x;
        const float dy = entityPos.y - origin->y;
        const float rangeLimit =
            (IsTargetedHighSurfaceModel64(modelInfo) || IsCriticalNearbySurfaceModel64(modelInfo))
                ? 420.0f
                : 230.0f;
        if (dx * dx + dy * dy > rangeLimit * rangeLimit) {
            continue;
        }

        ++scanned;
        VerticalEntityFallbackResult64 probe{};
        ProbeEntityVerticalFallback64(entity, origin, targetZ, probe);
        if (!probe.entity || !std::isfinite(probe.z)) {
            continue;
        }

        ++hits;
        DirectVerticalCandidateDiag64 candidate{};
        candidate.entity = probe.entity;
        candidate.modelId = probe.entity->m_nModelIndex;
        candidate.z = probe.z;
        candidate.entityPos = probe.entity->GetPosition();
        candidate.entityType = static_cast<uint8_t>(probe.entity->GetType());
        candidate.entityArea = static_cast<uint8_t>(probe.entity->m_nAreaCode);
        candidate.flags = probe.entity->m_nFlags;
        CBaseModelInfo* hitModelInfo = CModelInfo::GetModelInfo(candidate.modelId);
        CopyModelName64(candidate.modelName, hitModelInfo);
        CColModel* colModel = hitModelInfo ? hitModelInfo->m_pColModel : nullptr;
        CCollisionData* colData = colModel ? colModel->m_pColData : nullptr;
        candidate.colSlot = colModel ? colModel->m_nColSlot : 0;
        candidate.triCount = colData ? colData->m_nNumTriangles : 0;
        candidate.boxCount = colData ? colData->m_nNumBoxes : 0;
        candidate.localSample = TransformWorldToLocalColPoint64(
            probe.entity->GetMatrix(),
            CVector(origin->x, origin->y, probe.z));
        StoreDirectVerticalCandidate64(candidates, maxCandidates, candidate);
    }
}

static void LogDirectVerticalCandidates64(CVector* origin,
                                          float targetZ,
                                          uint8_t playerArea,
                                          float nativeGroundZ,
                                          const GroundProbe64& verticalProbe)
{
    if (!origin) {
        return;
    }

    constexpr int kMaxCandidates = 8;
    DirectVerticalCandidateDiag64 candidates[kMaxCandidates]{};
    int scanned = 0;
    int hits = 0;
    CollectDirectVerticalCandidatesFromPool64(GetBuildingPool(), origin, targetZ, playerArea, candidates, kMaxCandidates, scanned, hits);
    CollectDirectVerticalCandidatesFromPool64(GetObjectPoolGta(), origin, targetZ, playerArea, candidates, kMaxCandidates, scanned, hits);
    CollectDirectVerticalCandidatesFromPool64(GetDummyPool(), origin, targetZ, playerArea, candidates, kMaxCandidates, scanned, hits);

    FLog("[COL_DIRECT_LIST64] origin=%.2f %.2f %.2f target=%.2f native=%.2f vertical={hit:%d,z:%.2f,model:%d,%s} scanned=%d hits=%d",
         origin->x,
         origin->y,
         origin->z,
         targetZ,
         nativeGroundZ,
         verticalProbe.hit ? 1 : 0,
         verticalProbe.z,
         verticalProbe.modelId,
         verticalProbe.modelName[0] ? verticalProbe.modelName : "?",
         scanned,
         hits);

    for (int i = 0; i < kMaxCandidates; ++i) {
        const DirectVerticalCandidateDiag64& candidate = candidates[i];
        if (!candidate.entity) {
            break;
        }
        FLog("[COL_DIRECT_HIT64] rank=%d z=%.2f model=%d,%s type=%u area=%u colSlot=%u tris=%u boxes=%u ent=%.2f %.2f %.2f local=%.2f %.2f %.2f flags=0x%08X",
             i + 1,
             candidate.z,
             candidate.modelId,
             candidate.modelName[0] ? candidate.modelName : "?",
             static_cast<unsigned>(candidate.entityType),
             static_cast<unsigned>(candidate.entityArea),
             static_cast<unsigned>(candidate.colSlot),
             candidate.triCount,
             candidate.boxCount,
             candidate.entityPos.x,
             candidate.entityPos.y,
             candidate.entityPos.z,
             candidate.localSample.x,
             candidate.localSample.y,
             candidate.localSample.z,
             candidate.flags);
    }
}

static void LogDirectEntityVerticalDiagnostic64(const CVector& pos,
                                                float nativeGroundZ,
                                                uint8_t playerArea,
                                                const GroundProbe64& verticalProbe,
                                                uint32_t now)
{
    static uint32_t s_lastDirectDiagTick = 0;
    if (now - s_lastDirectDiagTick < 850) {
        return;
    }
    if (!std::isfinite(pos.x) ||
        !std::isfinite(pos.y) ||
        !std::isfinite(pos.z) ||
        !std::isfinite(nativeGroundZ) ||
        nativeGroundZ < -90.0f ||
        nativeGroundZ > 3000.0f) {
        return;
    }

    const bool worldMiss = !verticalProbe.hit;
    const bool worldLow =
        verticalProbe.hit &&
        std::isfinite(verticalProbe.z) &&
        verticalProbe.z < nativeGroundZ - 3.0f;
    const bool playerBelowNativeGround = pos.z < nativeGroundZ - 4.0f;
    if (!worldMiss && !worldLow && !playerBelowNativeGround) {
        return;
    }

    s_lastDirectDiagTick = now;

    const float startZ = fmaxf(fmaxf(pos.z + 12.0f, nativeGroundZ + 8.0f), 27.0f);
    const float targetZ = fminf(nativeGroundZ - 85.0f, pos.z - 10.0f);
    if (startZ <= targetZ + 1.0f) {
        return;
    }

    CVector origin(pos.x, pos.y, startZ);
    VerticalEntityFallbackResult64 result{};
    ProbeWorldPoolVerticalFallback64(GetBuildingPool(), &origin, targetZ, playerArea, result);
    ProbeWorldPoolVerticalFallback64(GetObjectPoolGta(), &origin, targetZ, playerArea, result);
    ProbeWorldPoolVerticalFallback64(GetDummyPool(), &origin, targetZ, playerArea, result);

    char modelName[22]{};
    int modelId = -1;
    uint8_t entityArea = 0;
    uint8_t entityType = ENTITY_TYPE_NOTHING;
    uint32_t entityFlags = 0;
    uint8_t colSlot = 0;
    uint32_t triCount = 0;
    uint32_t boxCount = 0;
    if (result.entity) {
        modelId = result.entity->m_nModelIndex;
        entityArea = static_cast<uint8_t>(result.entity->m_nAreaCode);
        entityType = static_cast<uint8_t>(result.entity->GetType());
        entityFlags = result.entity->m_nFlags;
        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
        CopyModelName64(modelName, modelInfo);
        CColModel* colModel = modelInfo ? modelInfo->m_pColModel : nullptr;
        CCollisionData* colData = colModel ? colModel->m_pColData : nullptr;
        colSlot = colModel ? colModel->m_nColSlot : 0;
        triCount = colData ? colData->m_nNumTriangles : 0;
        boxCount = colData ? colData->m_nNumBoxes : 0;
        if (worldMiss || worldLow) {
            RepairWorldEntitySectorRegistration64(
                result.entity,
                modelInfo,
                pos,
                result.z,
                worldMiss ? "direct-world-miss" : "direct-world-low",
                now);
        }
    }

    FLog("[COL_DIRECT64] pos=%.2f %.2f %.2f native=%.2f vertical={hit:%d,z:%.2f,model:%d,%s,use:%d} direct={hit:%d,z:%.2f,model:%d,%s,type:%u,area:%u,colSlot:%u,tris:%u,boxes:%u,flags:0x%08X} origin=%.2f %.2f %.2f target=%.2f scan={scanned:%d,candidates:%d,skipArea:%d,skipRange:%d,disabled:%d,bad:%d,noCol:%d,noData:%d,bounds:%d,miss:%d,rejectZ:%d,hits:%d}",
         pos.x,
         pos.y,
         pos.z,
         nativeGroundZ,
         verticalProbe.hit ? 1 : 0,
         verticalProbe.z,
         verticalProbe.modelId,
         verticalProbe.modelName[0] ? verticalProbe.modelName : "?",
         verticalProbe.usesCollision ? 1 : 0,
         result.entity ? 1 : 0,
         result.entity ? result.z : 0.0f,
         modelId,
         modelName[0] ? modelName : "?",
         static_cast<unsigned>(entityType),
         static_cast<unsigned>(entityArea),
         static_cast<unsigned>(colSlot),
         triCount,
         boxCount,
         entityFlags,
         origin.x,
         origin.y,
         origin.z,
         targetZ,
         result.scanned,
         result.candidates,
         result.areaSkipped,
         result.rangeSkipped,
         result.disabledCollision,
         result.badModel,
         result.missingColModel,
         result.missingColData,
         result.boundsSkipped,
         result.directMiss,
         result.rejectedZ,
         result.directHits);

    LogDirectVerticalCandidates64(&origin, targetZ, playerArea, nativeGroundZ, verticalProbe);

    if (!result.entity) {
        LogVerticalHoleCandidates64(&origin, targetZ, playerArea);
    }
}

static float Distance2DSquared64(const CVector& a, const CVector& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static void LogCollisionGrid64(const CVector& pos, uint32_t now)
{
    static uint32_t s_lastGridLogTick = 0;
    static const CVector kBallysSpawn64(1958.3783f, 1343.1572f, 15.3746f);

    if (Distance2DSquared64(pos, kBallysSpawn64) > 95.0f * 95.0f ||
        now - s_lastGridLogTick < 5000) {
        return;
    }
    s_lastGridLogTick = now;

    struct SampleOffset64 {
        float x;
        float y;
    };
    static constexpr SampleOffset64 kOffsets[] = {
        {0.0f, 0.0f},
        {2.0f, 0.0f},
        {-2.0f, 0.0f},
        {0.0f, 2.0f},
        {0.0f, -2.0f},
        {5.0f, 0.0f},
        {-5.0f, 0.0f},
        {0.0f, 5.0f},
        {0.0f, -5.0f},
        {8.0f, 8.0f},
        {-8.0f, 8.0f},
        {8.0f, -8.0f},
        {-8.0f, -8.0f},
    };

    for (const SampleOffset64& offset : kOffsets) {
        const CVector sample(pos.x + offset.x, pos.y + offset.y, pos.z);
        const GroundProbe64 lineProbe = ProbeGroundLine64(sample, 27.0f, -35.0f);
        const GroundProbe64 verticalProbe = ProbeVerticalLine64(sample, 27.0f, -35.0f);

        FLog("[COL_GRID64] sample=%.2f %.2f from=27.00 to=-35.00 "
             "line={hit:%d,z:%.2f,model:%d,%s,area:%u,use:%d} "
             "vertical={hit:%d,z:%.2f,model:%d,%s,area:%u,use:%d}",
             sample.x,
             sample.y,
             lineProbe.hit ? 1 : 0,
             lineProbe.z,
             lineProbe.modelId,
             lineProbe.modelName[0] ? lineProbe.modelName : "?",
             static_cast<unsigned>(lineProbe.area),
             lineProbe.usesCollision ? 1 : 0,
             verticalProbe.hit ? 1 : 0,
             verticalProbe.z,
             verticalProbe.modelId,
             verticalProbe.modelName[0] ? verticalProbe.modelName : "?",
              static_cast<unsigned>(verticalProbe.area),
              verticalProbe.usesCollision ? 1 : 0);
    }

    static constexpr SampleOffset64 kStackOffsets[] = {
        {0.0f, 0.0f},
        {6.7f, -0.1f},
        {8.0f, 8.0f},
        {-8.0f, 8.0f},
    };

    for (const SampleOffset64& offset : kStackOffsets) {
        const CVector sample(pos.x + offset.x, pos.y + offset.y, pos.z);
        const GroundProbe64 fromRoof = ProbeGroundLine64(sample, 18.4f, 7.0f);
        const GroundProbe64 fromSpawn = ProbeGroundLine64(sample, 16.6f, 7.0f);
        const GroundProbe64 fromFloor = ProbeGroundLine64(sample, 15.8f, 7.0f);

        FLog("[COL_STACK64] sample=%.2f %.2f "
             "18.4={hit:%d,z:%.2f,model:%d,%s,use:%d} "
             "16.6={hit:%d,z:%.2f,model:%d,%s,use:%d} "
             "15.8={hit:%d,z:%.2f,model:%d,%s,use:%d}",
             sample.x,
             sample.y,
             fromRoof.hit ? 1 : 0,
             fromRoof.z,
             fromRoof.modelId,
             fromRoof.modelName[0] ? fromRoof.modelName : "?",
             fromRoof.usesCollision ? 1 : 0,
             fromSpawn.hit ? 1 : 0,
             fromSpawn.z,
             fromSpawn.modelId,
             fromSpawn.modelName[0] ? fromSpawn.modelName : "?",
             fromSpawn.usesCollision ? 1 : 0,
             fromFloor.hit ? 1 : 0,
             fromFloor.z,
             fromFloor.modelId,
             fromFloor.modelName[0] ? fromFloor.modelName : "?",
             fromFloor.usesCollision ? 1 : 0);
    }
}

static bool BuildUpperStaticSurfaceAnchor64(
    const CVector& pos,
    float groundZ,
    const GroundProbe64& upperProbe,
    CVector& outAnchor)
{
    if (!upperProbe.hit || !upperProbe.usesCollision || upperProbe.modelId < 0) {
        return false;
    }

    const float upperDelta = upperProbe.z - pos.z;
    const float lowerDelta = pos.z - groundZ;
    if (upperDelta < 2.75f || upperDelta > 9.25f || lowerDelta < 3.0f) {
        return false;
    }

    CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(upperProbe.modelId);
    if (IsLikelyRoadModel64(modelInfo)) {
        return false;
    }

    outAnchor = CVector(pos.x, pos.y, upperProbe.z + 1.0f);
    return true;
}

static bool IsNearActiveSampObject64(const CVector& pos, float radius, float zRange)
{
    if (!pNetGame) {
        return false;
    }

    CObjectPool* objectPool = pNetGame->GetObjectPool();
    if (!objectPool) {
        return false;
    }

    const float radius2 = radius * radius;
    for (OBJECTID objectId = 0; objectId < MAX_OBJECTS; ++objectId) {
        CObject* object = objectPool->GetAt(objectId);
        if (!object || !object->m_pEntity) {
            continue;
        }

        const CVector objectPos = object->m_pEntity->GetPosition();
        const float dx = objectPos.x - pos.x;
        const float dy = objectPos.y - pos.y;
        const float dz = objectPos.z - pos.z;
        if (dx * dx + dy * dy <= radius2 && fabsf(dz) <= zRange) {
            return true;
        }
    }

    return false;
}

#endif

static void MaintainCollisionResidency64()
{
    static uint32_t s_lastBaseTick = 0;
    static uint32_t s_lastBurstTick = 0;
    static uint32_t s_lastLogTick = 0;
    static uint32_t s_lastRescueTick = 0;
    static uint32_t s_lastSafeTick = 0;
#if !VER_x32
    static uint32_t s_lastFloatSettleTick = 0;
    static uint32_t s_lastElevatedAnchorTick = 0;
    static uint32_t s_lastElevatedRecoverTick = 0;
    static uint32_t s_lastElevatedAnchorLogTick = 0;
    static uint32_t s_lastSupportTick = 0;
    static bool s_elevatedAnchorActive = false;
    static CVector s_elevatedAnchor(0.0f, 0.0f, 0.0f);
#endif
    static bool s_hasSafeAnchor = false;
    static CVector s_safeAnchor(0.0f, 0.0f, 0.0f);
    static bool s_loggedStreamingState = false;

    if (!pGame || !pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
        return;
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    if (!playerPool) {
        return;
    }

    CLocalPlayer* localPlayer = playerPool->GetLocalPlayer();
    if (!localPlayer || !localPlayer->m_bIsActive) {
        return;
    }

    CPlayerPed* localPed = localPlayer->GetPlayerPed();
    if (!localPed || !localPed->m_pPed) {
        return;
    }

    if (Supremo::Streaming::EnableStreamingIfDisabled()) {
        FLog("[COL_ROOT64] streaming was disabled, forcing enable");
    } else if (!s_loggedStreamingState) {
        s_loggedStreamingState = true;
        FLog("[COL_ROOT64] streaming enabled");
    }

    const uint32_t now = GetTickCount();
    const CVector pos = localPed->m_pPed->GetPosition();
    const uint8_t effectivePlayerArea = NormalizeLocalExteriorArea64(localPed, pos, "collision-residency");
    const CVector vel = localPed->m_pPed->GetMoveSpeed();
    const bool inVehicle = localPed->IsInVehicle();
    bool supportedByTargetedHighProbe = false;
#if !VER_x32
    if (IsLocalPedWaterActive64(localPed)) {
        static uint32_t s_lastWaterSkipLogTick = 0;
        if (now - s_lastWaterSkipLogTick >= 1500) {
            s_lastWaterSkipLogTick = now;
            const uint8_t cover = localPed->m_pPed->m_pPlayerData
                ? localPed->m_pPed->m_pPlayerData->m_nWaterCoverPerc
                : 0;
            const float waterHeight = localPed->m_pPed->m_pPlayerData
                ? localPed->m_pPed->m_pPlayerData->m_fWaterHeight
                : 0.0f;
            FLog("[COL_ROOT64] water-skip pos=%.2f %.2f %.2f vel=%.3f %.3f %.3f cover=%u water=%.2f flags={sub:%d,touch:%d}",
                 pos.x,
                 pos.y,
                 pos.z,
                 vel.x,
                 vel.y,
                 vel.z,
                 static_cast<unsigned>(cover),
                 waterHeight,
                 localPed->m_pPed->physicalFlags.bSubmergedInWater ? 1 : 0,
                 localPed->m_pPed->physicalFlags.bTouchingWater ? 1 : 0);
        }
        return;
    }
    SuppressNearbyTempVehicleComponents64(pos);
#endif
    const float groundZ = pGame->FindGroundZForCoord(pos.x, pos.y, pos.z + 3.0f);
#if !VER_x32
    const float nativeGroundZ = CWorld::FindGroundZForCoord(pos.x, pos.y);
    const float highStartZ = pos.z + 12.0f;
    const GroundProbe64 highGroundProbe = ProbeGroundLine64(pos, highStartZ, pos.z - 60.0f);
    const GroundProbe64 verticalGroundProbe = ProbeVerticalLine64(pos, 27.0f, -35.0f);
    CBaseModelInfo* highProbeModelInfo = highGroundProbe.modelId >= 0
        ? CModelInfo::GetModelInfo(highGroundProbe.modelId)
        : nullptr;
    CBaseModelInfo* verticalProbeModelInfo = verticalGroundProbe.modelId >= 0
        ? CModelInfo::GetModelInfo(verticalGroundProbe.modelId)
        : nullptr;
    supportedByTargetedHighProbe =
        !inVehicle &&
        ((highGroundProbe.hit &&
          highGroundProbe.usesCollision &&
          IsTargetedHighSurfaceModel64(highProbeModelInfo) &&
          std::isfinite(highGroundProbe.z) &&
          highGroundProbe.z >= pos.z - 2.50f &&
          highGroundProbe.z <= pos.z + 2.75f) ||
         (verticalGroundProbe.hit &&
          verticalGroundProbe.usesCollision &&
          IsTargetedHighSurfaceModel64(verticalProbeModelInfo) &&
          std::isfinite(verticalGroundProbe.z) &&
          verticalGroundProbe.z >= pos.z - 2.50f &&
          verticalGroundProbe.z <= pos.z + 2.75f));
    static uint32_t s_lastDeepDiagTick = 0;
    const bool runDeepDiagnostics =
        kArm64CollisionDeepDiagnosticsEnabled &&
        now - s_lastDeepDiagTick >= 2200u;
    if (runDeepDiagnostics) {
        s_lastDeepDiagTick = now;
        LogCollisionGrid64(pos, now);
        LogCollisionStoreSlots64(pos, now);
    }
    RequestMissingNearbyWorldCollision64(pos, effectivePlayerArea, now, false);
    RequestMissingWatchedEntityCollision64(pos, effectivePlayerArea, now);
    if (runDeepDiagnostics) {
        LogEntityVerticalProbes64(pos, now);
        LogDirectEntityVerticalDiagnostic64(
            pos,
            nativeGroundZ,
            effectivePlayerArea,
            verticalGroundProbe,
            now);
    }
#endif
    const float dz = pos.z - groundZ;

    auto requestAt = [&](const CVector& sample) {
        CVector streamSample(sample.x, sample.y, sample.z);
        const float nativeAtSample = CWorld::FindGroundZForCoord(sample.x, sample.y);
        if (std::isfinite(nativeAtSample) &&
            sample.z < 5.0f &&
            nativeAtSample > sample.z + 8.0f &&
            nativeAtSample < 3000.0f) {
            streamSample.z = nativeAtSample + 2.0f;
        } else if (streamSample.z < 18.0f) {
            streamSample.z = 18.0f;
        }

        Supremo::Streaming::WorldStreamRequest streamRequest{};
        streamRequest.areaCode = static_cast<int32>(effectivePlayerArea);
        streamRequest.streamingFlags = STREAMING_DEFAULT;
        streamRequest.setCollisionRequired = false;
        streamRequest.addIpls = false;
        streamRequest.loadIpls = false;
        streamRequest.ensureIpls = false;
        streamRequest.loadSceneCollision = true;
        streamRequest.reason = "collision-residency";
        Supremo::Streaming::PrepareWorldAtPoint(streamSample, streamRequest);
#if !VER_x32
        RequestNearbyWorldCollisionAreas64(streamSample, effectivePlayerArea);
#endif
        if (pGame) {
            pGame->RefreshStreamingAt(streamSample.x, streamSample.y);
        }
    };

    auto requestCross = [&](const CVector& center, float radius, bool includeDiagonals) {
        const float diag = radius * 0.70710678f;
        requestAt(center);
        requestAt(CVector(center.x + radius, center.y, center.z));
        requestAt(CVector(center.x - radius, center.y, center.z));
        requestAt(CVector(center.x, center.y + radius, center.z));
        requestAt(CVector(center.x, center.y - radius, center.z));
        if (includeDiagonals) {
            requestAt(CVector(center.x + diag, center.y + diag, center.z));
            requestAt(CVector(center.x + diag, center.y - diag, center.z));
            requestAt(CVector(center.x - diag, center.y + diag, center.z));
            requestAt(CVector(center.x - diag, center.y - diag, center.z));
        }
    };

    localPed->m_pPed->m_bUsesCollision = true;
    localPed->m_pPed->physicalFlags.bCollidable = true;
    localPed->m_pPed->physicalFlags.bCanBeCollidedWith = true;
    localPed->m_pPed->physicalFlags.bDisableSimpleCollision = false;
    localPed->m_pPed->SetCollisionChecking(true);
    localPed->m_pPed->physicalFlags.bApplyGravity = true;

    const uint32_t baseIntervalMs = inVehicle ? 750u : 500u;
    if (now - s_lastBaseTick >= baseIntervalMs) {
        s_lastBaseTick = now;
        requestCross(pos, inVehicle ? 24.0f : 16.0f, false);
        Supremo::Streaming::LoadAllRequestedModels(false);
    }

    const float speedSq = vel.x * vel.x + vel.y * vel.y + vel.z * vel.z;
    const bool groundFinite = std::isfinite(groundZ);
    const bool unstableGround =
        groundFinite &&
        (dz < -2.5f ||
         (!inVehicle && groundZ <= 0.01f && pos.z > 5.0f));
    const bool movingFast = !inVehicle && speedSq > 0.25f;

    const bool positionFinite = std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z);
    const bool safeSample =
        groundFinite &&
        positionFinite &&
        dz > -1.8f &&
        dz < 6.0f &&
        speedSq < (inVehicle ? 1.40f : 0.35f) &&
        pos.z > -200.0f &&
        pos.z < 3000.0f;
    if (safeSample) {
#if !VER_x32
        CVector elevatedAnchorCandidate(pos.x, pos.y, pos.z);
        const bool upperSurfaceAnchor = false;
        const bool elevatedSample =
            kArm64ElevatedAnchorAssistEnabled &&
            !inVehicle &&
            speedSq < 0.08f &&
            dz > 2.25f &&
            dz < 8.0f;
        const bool nearElevatedAnchor =
            s_elevatedAnchorActive &&
            Distance2DSquared64(pos, s_elevatedAnchor) <= 42.0f * 42.0f;
        s_safeAnchor = elevatedSample && nearElevatedAnchor && pos.z < s_elevatedAnchor.z - 0.15f
                       ? s_elevatedAnchor
                       : (elevatedSample
                          ? elevatedAnchorCandidate
                          : CVector(pos.x, pos.y, groundZ + (inVehicle ? 1.2f : 1.0f)));
#else
        s_safeAnchor = CVector(pos.x, pos.y, groundZ + (inVehicle ? 1.2f : 1.0f));
#endif
        s_lastSafeTick = now;
        s_hasSafeAnchor = true;
#if !VER_x32
        if (elevatedSample) {
            const bool newArea =
                !s_elevatedAnchorActive ||
                Distance2DSquared64(elevatedAnchorCandidate, s_elevatedAnchor) > 42.0f * 42.0f;
            const bool raisesAnchor =
                s_elevatedAnchorActive &&
                Distance2DSquared64(elevatedAnchorCandidate, s_elevatedAnchor) <= 42.0f * 42.0f &&
                elevatedAnchorCandidate.z > s_elevatedAnchor.z + 0.15f;
            if (newArea || raisesAnchor) {
                const CVector oldAnchor = s_elevatedAnchor;
                s_elevatedAnchor = elevatedAnchorCandidate;
                s_lastElevatedAnchorTick = now;
                s_elevatedAnchorActive = true;
                if (now - s_lastElevatedAnchorLogTick >= 750) {
                    s_lastElevatedAnchorLogTick = now;
                    FLog("[COL_ANCHOR64] %s old=%.2f %.2f %.2f new=%.2f %.2f %.2f dz=%.2f ground=%.2f upper={on:%d,z:%.2f,model:%d,%s}",
                         newArea ? "new" : "raised",
                         oldAnchor.x,
                         oldAnchor.y,
                         oldAnchor.z,
                         s_elevatedAnchor.x,
                         s_elevatedAnchor.y,
                         s_elevatedAnchor.z,
                         dz,
                         groundZ,
                         upperSurfaceAnchor ? 1 : 0,
                         highGroundProbe.z,
                         highGroundProbe.modelId,
                         highGroundProbe.modelName[0] ? highGroundProbe.modelName : "?");
                }
            } else if (s_elevatedAnchorActive &&
                       Distance2DSquared64(pos, s_elevatedAnchor) <= 42.0f * 42.0f) {
                s_lastElevatedAnchorTick = now;
            }
        } else if (s_elevatedAnchorActive &&
                   Distance2DSquared64(pos, s_elevatedAnchor) > 42.0f * 42.0f) {
            s_elevatedAnchorActive = false;
        }
#endif
    }

#if !VER_x32
    const float settledSupportZ =
        groundFinite && std::isfinite(groundZ)
            ? fmaxf(verticalGroundProbe.z, groundZ)
            : verticalGroundProbe.z;
    const bool hasHigherSupportBelowPed =
        highGroundProbe.hit &&
        highGroundProbe.usesCollision &&
        std::isfinite(highGroundProbe.z) &&
        highGroundProbe.z > settledSupportZ + 0.35f &&
        highGroundProbe.z <= pos.z + 0.35f;
    const bool nearActiveSampObject = IsNearActiveSampObject64(pos, 8.0f, 8.0f);

    static uint32_t s_lastJumpPressTick = 0;
    if (Supremo::Input::IsJumpDown()) {
        s_lastJumpPressTick = now;
    }
    const bool isJumpingRecently = (now - s_lastJumpPressTick < 2000u);

    // If the ped is standing on a vehicle (e.g. on top of a plane/car), the vertical
    // probe may pass through the vehicle mesh and hit the real ground far below,
    // computing a settledPedZ well beneath the vehicle and teleporting the player down.
    // Skip settling entirely when the ped is standing on any vehicle.
    const bool standingOnVehicle =
        localPed->m_pPed->m_pEntityStandingOn != nullptr &&
        localPed->m_pPed->m_pEntityStandingOn->IsVehicle();
    // Also skip when the vertical probe itself landed on a vehicle entity.
    const bool probeLandedOnVehicle =
        verticalGroundProbe.hit &&
        verticalGroundProbe.entityType == static_cast<uint8_t>(ENTITY_TYPE_VEHICLE);
    const bool canSettleFloatingPed =
        !inVehicle &&
        !isJumpingRecently &&
        !standingOnVehicle &&
        !probeLandedOnVehicle &&
        positionFinite &&
        verticalGroundProbe.hit &&
        verticalGroundProbe.usesCollision &&
        std::isfinite(verticalGroundProbe.z) &&
        !hasHigherSupportBelowPed &&
        !nearActiveSampObject &&
        speedSq < 0.020f &&
        std::fabs(vel.z) < 0.040f;
    const float settledPedZ = settledSupportZ + 1.0f;
    const float floatingGap = pos.z - settledPedZ;
    if (canSettleFloatingPed &&
        floatingGap > 0.45f &&
        floatingGap < 4.50f &&
        now - s_lastFloatSettleTick >= 350) {
        s_lastFloatSettleTick = now;
        CVector settledPos(pos.x, pos.y, settledPedZ);
        localPed->m_pPed->SetPosn(settledPos);
        localPed->m_pPed->ResetMoveSpeed();
        localPed->m_pPed->ResetTurnSpeed();
        localPed->m_pPed->m_bHasContacted = false;
        localPed->m_pPed->m_bHasHitWall = false;
        localPed->m_pPed->m_bIsInSafePosition = false;
        localPed->m_pPed->physicalFlags.bOnSolidSurface = false;
        localPed->m_pPed->physicalFlags.bApplyGravity = true;
        localPed->m_pPed->bIsStanding = false;
        localPed->m_pPed->bWasStanding = false;
        localPed->m_pPed->bIsInTheAir = true;
        localPed->m_pPed->UpdateRW();
        localPed->m_pPed->UpdateRwFrame();

        FLog("[COL_FLOAT64] lowered floating ped old=%.2f %.2f %.2f new=%.2f %.2f %.2f gap=%.2f ground=%.2f model=%d,%s",
             pos.x,
             pos.y,
             pos.z,
             settledPos.x,
             settledPos.y,
             settledPos.z,
             floatingGap,
             verticalGroundProbe.z,
             verticalGroundProbe.modelId,
             verticalGroundProbe.modelName[0] ? verticalGroundProbe.modelName : "?");
    }

    const float pedGroundOffset = inVehicle ? 1.2f : 1.0f;
    const bool recentSafeAnchor = s_hasSafeAnchor && (now - s_lastSafeTick) <= 12000;
    const float expectedSupportZ =
        recentSafeAnchor
            ? s_safeAnchor.z - pedGroundOffset
            : (verticalGroundProbe.hit
               ? verticalGroundProbe.z
               : (groundFinite ? groundZ : pos.z - pedGroundOffset));
    const bool smallCollisionGap =
        kArm64ElevatedAnchorAssistEnabled &&
        !inVehicle &&
        positionFinite &&
        recentSafeAnchor &&
        Distance2DSquared64(pos, s_safeAnchor) <= 18.0f * 18.0f &&
        vel.z < -0.020f &&
        pos.z < s_safeAnchor.z - 0.35f &&
        pos.z < expectedSupportZ - 0.50f &&
        pos.z > s_safeAnchor.z - 20.0f &&
        !verticalGroundProbe.hit;

    if (smallCollisionGap && now - s_lastSupportTick >= 220) {
        s_lastSupportTick = now;
        requestCross(pos, 28.0f, true);
        Supremo::Streaming::LoadAllRequestedModels(false);
        FLog("[COL_FALL64] support-disabled gap pos=%.2f %.2f %.2f safe=%.2f %.2f %.2f expected=%.2f vel=%.3f %.3f %.3f vertical={hit:%d,z:%.2f,model:%d,%s}",
             pos.x,
             pos.y,
             pos.z,
             s_safeAnchor.x,
             s_safeAnchor.y,
             s_safeAnchor.z,
             expectedSupportZ,
             vel.x,
             vel.y,
             vel.z,
             verticalGroundProbe.hit ? 1 : 0,
             verticalGroundProbe.z,
             verticalGroundProbe.modelId,
             verticalGroundProbe.modelName[0] ? verticalGroundProbe.modelName : "?");
    }

    if (kArm64ElevatedAnchorAssistEnabled &&
        s_elevatedAnchorActive &&
        !inVehicle &&
        positionFinite &&
        now - s_lastElevatedAnchorTick <= 600000 &&
        Distance2DSquared64(pos, s_elevatedAnchor) <= 24.0f * 24.0f &&
        pos.z < s_elevatedAnchor.z - 0.85f &&
        now - s_lastElevatedRecoverTick >= 450) {
        s_lastElevatedRecoverTick = now;
        s_lastElevatedAnchorTick = now;
        requestCross(s_elevatedAnchor, 32.0f, true);
        Supremo::Streaming::LoadAllRequestedModels(false);
        FLog("[COL_ELEV64] skipped anchor teleport old=%.2f %.2f %.2f ground=%.2f native=%.2f high={hit:%d,z:%.2f,model:%d,%s,area:%u,use:%d} anchor=%.2f %.2f %.2f",
             pos.x, pos.y, pos.z,
             groundZ,
             nativeGroundZ,
             highGroundProbe.hit ? 1 : 0,
             highGroundProbe.z,
             highGroundProbe.modelId,
             highGroundProbe.modelName[0] ? highGroundProbe.modelName : "?",
             static_cast<unsigned>(highGroundProbe.area),
             highGroundProbe.usesCollision ? 1 : 0,
             s_elevatedAnchor.x,
             s_elevatedAnchor.y,
             s_elevatedAnchor.z);
    }
#endif

    const uint32_t burstIntervalMs = inVehicle ? 650u : 260u;
    if ((unstableGround || movingFast) && now - s_lastBurstTick >= burstIntervalMs) {
        s_lastBurstTick = now;
        requestCross(pos, inVehicle ? 42.0f : 34.0f, !inVehicle);
        const CVector ahead(
            pos.x + vel.x * (inVehicle ? 18.0f : 11.0f),
            pos.y + vel.y * (inVehicle ? 18.0f : 11.0f),
            pos.z + vel.z * (inVehicle ? 8.0f : 5.0f));
        requestCross(ahead, inVehicle ? 32.0f : 24.0f, false);
        Supremo::Streaming::LoadAllRequestedModels(false);
    }

    const bool severeBelowKnownGround =
        groundFinite &&
        dz < -45.0f &&
        (pos.z < -20.0f ||
         vel.z < -0.12f ||
         speedSq > (inVehicle ? 0.25f : 0.04f));
    const bool severeDrop =
        (!positionFinite || pos.z < -80.0f || severeBelowKnownGround) &&
        !supportedByTargetedHighProbe;
    if (severeDrop && now - s_lastRescueTick >= 900) {
        requestCross(pos, inVehicle ? 48.0f : 38.0f, true);
        Supremo::Streaming::LoadAllRequestedModels(false);

        s_lastRescueTick = now;
#if !VER_x32
        const bool nativeRescueGround =
            !inVehicle &&
            !IsLocalPedWaterActive64(localPed) &&
            positionFinite &&
            std::isfinite(nativeGroundZ) &&
            nativeGroundZ > -100.0f &&
            nativeGroundZ < 3000.0f &&
            nativeGroundZ > pos.z + 3.0f;
        if (nativeRescueGround) {
            const CVector rescuePos(pos.x, pos.y, nativeGroundZ + 1.0f);
            localPed->m_pPed->SetPosn(rescuePos);
            localPed->m_pPed->ResetMoveSpeed();
            localPed->m_pPed->ResetTurnSpeed();
            localPed->m_pPed->m_bHasContacted = false;
            localPed->m_pPed->m_bHasHitWall = false;
            localPed->m_pPed->m_bIsInSafePosition = false;
            localPed->m_pPed->physicalFlags.bOnSolidSurface = false;
            localPed->m_pPed->physicalFlags.bApplyGravity = true;
            localPed->m_pPed->bIsStanding = false;
            localPed->m_pPed->bWasStanding = false;
            localPed->m_pPed->bIsInTheAir = true;
            localPed->m_pPed->UpdateRW();
            localPed->m_pPed->UpdateRwFrame();
            s_safeAnchor = rescuePos;
            s_lastSafeTick = now;
            s_hasSafeAnchor = true;
            FLog("[COL_FALL64] rescued-native-ground old=%.2f %.2f %.2f new=%.2f %.2f %.2f native=%.2f dz=%.2f vel=%.3f %.3f %.3f",
                 pos.x,
                 pos.y,
                 pos.z,
                 rescuePos.x,
                 rescuePos.y,
                 rescuePos.z,
                 nativeGroundZ,
                 dz,
                 vel.x,
                 vel.y,
                 vel.z);
            return;
        }
#endif
        FLog("[COL_FALL64] recover-disabled severe pos=%.2f %.2f %.2f ground=%.2f dz=%.2f safe=%.2f %.2f %.2f area=%d vel=%.3f %.3f %.3f",
             pos.x, pos.y, pos.z,
             groundZ,
             dz,
             s_safeAnchor.x,
             s_safeAnchor.y,
             s_safeAnchor.z,
             localPed->m_pPed->m_nAreaCode,
             vel.x,
             vel.y,
             vel.z);
    }

    if (now - s_lastLogTick >= 12000) {
        s_lastLogTick = now;
        FLog("[COL_ROOT64] hook-stream pos=%.2f %.2f %.2f ground=%.2f dz=%.2f vel2=%.3f area=%d coll={use:%d,col:%d,hit:%d,simple:%d}"
#if !VER_x32
             " native=%.2f high={hit:%d,z:%.2f,model:%d,%s,area:%u,use:%d} vertical={hit:%d,z:%.2f,model:%d,%s,area:%u,use:%d} elev={on:%d,age:%u,anchor:%.2f %.2f %.2f}"
#endif
             ,
             pos.x,
             pos.y,
             pos.z,
             groundZ,
             dz,
             speedSq,
             localPed->m_pPed->m_nAreaCode,
             localPed->m_pPed->m_bUsesCollision ? 1 : 0,
             localPed->m_pPed->physicalFlags.bCollidable ? 1 : 0,
             localPed->m_pPed->physicalFlags.bCanBeCollidedWith ? 1 : 0,
             localPed->m_pPed->physicalFlags.bDisableSimpleCollision ? 0 : 1
#if !VER_x32
             ,
             nativeGroundZ,
             highGroundProbe.hit ? 1 : 0,
             highGroundProbe.z,
             highGroundProbe.modelId,
             highGroundProbe.modelName[0] ? highGroundProbe.modelName : "?",
             static_cast<unsigned>(highGroundProbe.area),
             highGroundProbe.usesCollision ? 1 : 0,
             verticalGroundProbe.hit ? 1 : 0,
             verticalGroundProbe.z,
             verticalGroundProbe.modelId,
             verticalGroundProbe.modelName[0] ? verticalGroundProbe.modelName : "?",
             static_cast<unsigned>(verticalGroundProbe.area),
             verticalGroundProbe.usesCollision ? 1 : 0,
             s_elevatedAnchorActive ? 1 : 0,
             s_elevatedAnchorActive ? static_cast<unsigned>(now - s_lastElevatedAnchorTick) : 0u,
             s_elevatedAnchor.x,
             s_elevatedAnchor.y,
             s_elevatedAnchor.z
#endif
        );
    }
}

void (*CCamera_Process)(CCamera* camera) = nullptr;
void CCamera_Process_hook(CCamera* camera)
{
    if ((Supremo::Input::IsSourceCameraLookRequested() || Supremo::Input::IsSourceCameraLookActive()) && camera) {
        if (camera->m_nActiveCam >= 3) {
            // Script/cinematic cameras own processing once active; do not leave
            // the native camera process suppressed by source-camera mode.
            Supremo::Input::ResetSourceCameraLook();
        } else {
            Supremo::Input::ApplySourceCameraLook();
            return;
        }
    }

    if (CCamera_Process) {
        CCamera_Process(camera);
    }
}

void CGame_Process_hook()
{
    if(pGame->bIsGameExiting)return;

    Supremo::Input::RestoreCameraMatrixForGameplay();

#if !VER_x32
    // Preserve queued map-scan work and streaming recovery from the existing
    // process path; these calls are distinct from native CGame::Process.
    SuppressNativeLoadingDuringGameplay64("game-process-begin");
    EnsureArm64StreamingMemoryBudget("game-process");
    Supremo::Streaming::EnableStreamingIfDisabled();
    ProcessQueuedMapScanTeleport64();
    ProcessQueuedMapScanProbe64();
#endif

    // Keep the native process order intact.  Loading-screen state is owned by
    // the game/network state machine; do not clear its globals from this hook.
    MainLoop();
#if !VER_x32
    SuppressNativeLoadingDuringGameplay64("game-process-before-camera");
#endif
    CGame_Process();
#if !VER_x32
    SuppressNativeLoadingDuringGameplay64("game-process-after-native");
#endif

    if (pNetGame)
    {
        if(pGame && pGame->FindPlayerPed() && pUI && pUI->buttonpanel() && pUI->buttonpanel()->m_bH)
        {
            if(pGame->FindPlayerPed()->IsInVehicle())
            {
                pUI->buttonpanel()->m_bH->setCaption("D/B");
            }
            else
                pUI->buttonpanel()->m_bH->setCaption("H");
        }

        CObjectPool* pObjectPool = pNetGame->GetObjectPool();
        if (pObjectPool) {
            pObjectPool->Process();
            pObjectPool->ProcessMaterialText();
        }

        CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();
        if (pTextDrawPool) {
            pTextDrawPool->SnapshotProcess();
        }
    }

#if !VER_x32
    MaintainWorldObjectStreaming64();
    MaintainCollisionResidency64();
    if (pGame && pGame->FindPlayerPed() && pGame->FindPlayerPed()->m_pPed) {
        LogWatchedWorldMatricesAroundPlayerABI(pGame->FindPlayerPed()->m_pPed->GetPosition());
    }
#endif

    CGame::ProcessMainThreadTasks();
}

float (*CDraw__SetFOV)(float thiz, float a2);
float CDraw__SetFOV_hook(float thiz, float a2)
{
    float tmp = (((*GTASAEngineApi::AspectRatio() - 1.3333f) * 11.0f) / 0.44444f) + thiz;
    if(tmp > 100) tmp = 100.0;
    GTASAEngineApi::SetDrawFov(tmp);
    return thiz;
}

void(*CStreaming__Init2)();
void CStreaming__Init2_hook()
{
    CStreaming__Init2();
#if VER_x32
    GTASAEngineApi::SetLegacyStreamingInitMemoryBudget(536870912);
#else
    EnsureArm64StreamingMemoryBudget("init2");
#endif
}

int(*mpg123_param)(void* mh, int key, long val, int ZERO, double fval);
int mpg123_param_hook(void* mh, int key, long val, int ZERO, double fval)
{
    // 0x2000 = MPG123_SKIP_ID3V2
    // 0x200  = MPG123_FUZZY
    // 0x100  = MPG123_SEEKBUFFER
    // 0x40   = MPG123_GAPLESS
    return mpg123_param(mh, key, val | (0x2000 | 0x200 | 0x100 | 0x40), ZERO, fval);
}

#if !VER_x32
void (*CBike__SetupModelNodes)(CVehicleGTA* thiz);
void (*CBike__SetupSuspensionLines)(CVehicleGTA* thiz);
void CBike__SetupSuspensionLines_hook(CVehicleGTA* thiz)
{
    if (!thiz) {
        FLog("[VEH_GUARD64] skipped CBike::SetupSuspensionLines null bike");
        return;
    }

    if (CBike__SetupModelNodes) {
        CBike__SetupModelNodes(thiz);
    }

    auto* base = reinterpret_cast<uint8_t*>(thiz);
    void** nodes = reinterpret_cast<void**>(base + 0x758);
    const bool hasSuspensionNodes = nodes[2] && nodes[3] && nodes[4] && nodes[5];
    if (!hasSuspensionNodes) {
        static uint32_t s_missingBikeNodes = 0;
        ++s_missingBikeNodes;
        if (s_missingBikeNodes <= 16 || (s_missingBikeNodes % 100) == 0) {
            FLog("[VEH_GUARD64] skipped CBike::SetupSuspensionLines missing nodes bike=%p model=%d nodes={%p,%p,%p,%p} count=%u",
                 thiz,
                 static_cast<int>(thiz->m_nModelIndex),
                 nodes[2],
                 nodes[3],
                 nodes[4],
                 nodes[5],
                 s_missingBikeNodes);
        }
        return;
    }

    CBike__SetupSuspensionLines(thiz);
}

void (*CTheCarGenerators__Process)();
void CTheCarGenerators__Process_hook()
{
    static bool s_logged = false;
    if (!s_logged) {
        s_logged = true;
        FLog("[VEH_GUARD64] disabled native CTheCarGenerators::Process on arm64; multiplayer/server vehicles only");
    }
}

using CScriptsForBrains__CheckIfNewEntityNeedsScriptFn = void (*)(void* thiz, CEntityGTA* entity, int8_t attachType, void* pedGenerator);
CScriptsForBrains__CheckIfNewEntityNeedsScriptFn CScriptsForBrains__CheckIfNewEntityNeedsScript = nullptr;
void CScriptsForBrains__CheckIfNewEntityNeedsScript_hook(void* thiz, CEntityGTA* entity, int8_t attachType, void* pedGenerator)
{
    if (pNetGame) {
        static uint32_t s_skipCheckCount = 0;
        ++s_skipCheckCount;
        if (s_skipCheckCount <= 8 || (s_skipCheckCount % 50000) == 0) {
            const CVector pos = entity ? entity->GetPosition() : CVector{0.0f, 0.0f, 0.0f};
            FLog("[BRAIN_GUARD64] skipped script-brain entity check in multiplayer entity=%p model=%d type=%d area=%u attach=%d pos=%.2f %.2f %.2f count=%u",
                 entity,
                 entity ? static_cast<int32_t>(entity->m_nModelIndex) : -1,
                 entity ? static_cast<int>(entity->GetType()) : -1,
                 entity ? static_cast<unsigned>(entity->m_nAreaCode) : 255u,
                 static_cast<int>(attachType),
                 pos.x,
                 pos.y,
                 pos.z,
                 s_skipCheckCount);
        }
        return;
    }

    CScriptsForBrains__CheckIfNewEntityNeedsScript(thiz, entity, attachType, pedGenerator);
}

using CTheScripts__AddToWaitingForScriptBrainArrayFn = void (*)(CEntityGTA* entity, int16_t brainIndex);
CTheScripts__AddToWaitingForScriptBrainArrayFn CTheScripts__AddToWaitingForScriptBrainArray = nullptr;
void CTheScripts__AddToWaitingForScriptBrainArray_hook(CEntityGTA* entity, int16_t brainIndex)
{
    if (pNetGame) {
        static uint32_t s_skipWaitAddCount = 0;
        ++s_skipWaitAddCount;
        if (s_skipWaitAddCount <= 4 || (s_skipWaitAddCount % 50000) == 0) {
            FLog("[BRAIN_GUARD64] skipped adding multiplayer entity to script-brain wait array entity=%p model=%d brain=%d count=%u",
                 entity,
                 entity ? static_cast<int32_t>(entity->m_nModelIndex) : -1,
                 static_cast<int>(brainIndex),
                 s_skipWaitAddCount);
        }
        return;
    }

    CTheScripts__AddToWaitingForScriptBrainArray(entity, brainIndex);
}

using CTheScripts__ProcessWaitingForScriptBrainArrayFn = void (*)();
CTheScripts__ProcessWaitingForScriptBrainArrayFn CTheScripts__ProcessWaitingForScriptBrainArray = nullptr;
void CTheScripts__ProcessWaitingForScriptBrainArray_hook()
{
    if (pNetGame) {
        static bool s_logged = false;
        if (!s_logged) {
            s_logged = true;
            FLog("[BRAIN_GUARD64] skipped native script-brain wait processing in multiplayer");
        }
        return;
    }

    CTheScripts__ProcessWaitingForScriptBrainArray();
}

using CScriptsForBrains__StartNewStreamedScriptBrainFn = void (*)(void* thiz, uint8_t brainIndex, CEntityGTA* entity, uint8_t attachType);
CScriptsForBrains__StartNewStreamedScriptBrainFn CScriptsForBrains__StartNewStreamedScriptBrain = nullptr;
void CScriptsForBrains__StartNewStreamedScriptBrain_hook(void* thiz, uint8_t brainIndex, CEntityGTA* entity, uint8_t attachType)
{
    if (pNetGame) {
        static uint32_t s_skipBrainCount = 0;
        static int32_t s_loggedModels[32] = {};
        static uint8_t s_loggedModelCount = 0;
        ++s_skipBrainCount;

        const int32_t modelId = entity ? static_cast<int32_t>(entity->m_nModelIndex) : -1;
        bool firstModelLog = false;
        if (modelId >= 0 && s_loggedModelCount < sizeof(s_loggedModels) / sizeof(s_loggedModels[0])) {
            bool seen = false;
            for (uint8_t i = 0; i < s_loggedModelCount; ++i) {
                if (s_loggedModels[i] == modelId) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                s_loggedModels[s_loggedModelCount++] = modelId;
                firstModelLog = true;
            }
        }

        if (s_skipBrainCount <= 8 || firstModelLog || (s_skipBrainCount % 50000) == 0) {
            const CVector pos = entity ? entity->GetPosition() : CVector{0.0f, 0.0f, 0.0f};
            FLog("[BRAIN_GUARD64] skipped streamed brain in multiplayer brain=%u attach=%u entity=%p model=%d type=%d area=%u pos=%.2f %.2f %.2f count=%u",
                 static_cast<unsigned>(brainIndex),
                 static_cast<unsigned>(attachType),
                 entity,
                 modelId,
                 entity ? static_cast<int>(entity->GetType()) : -1,
                 entity ? static_cast<unsigned>(entity->m_nAreaCode) : 255u,
                 pos.x,
                 pos.y,
                 pos.z,
                 s_skipBrainCount);
        }
        return;
    }

    if (!thiz || !entity) {
        static uint32_t s_badBrainCount = 0;
        ++s_badBrainCount;
        if (s_badBrainCount <= 4 || (s_badBrainCount % 50000) == 0) {
            FLog("[BRAIN_GUARD64] skipped invalid streamed brain thiz=%p entity=%p brain=%u attach=%u count=%u",
                 thiz,
                 entity,
                 static_cast<unsigned>(brainIndex),
                 static_cast<unsigned>(attachType),
                 s_badBrainCount);
        }
        return;
    }

    CScriptsForBrains__StartNewStreamedScriptBrain(thiz, brainIndex, entity, attachType);
}

void (*CPopulation__ConvertToDummyObject)(CObjectGta* object);
void CPopulation__ConvertToDummyObject_hook(CObjectGta* object)
{
    if (!object) {
        FLog("[DUMMY_GUARD64] skipped null ConvertToDummyObject object");
        return;
    }

    if (!object->m_pDummyObject) {
        static uint32_t s_nullDummyCount = 0;
        ++s_nullDummyCount;
        if (s_nullDummyCount <= 24 || (s_nullDummyCount % 200) == 0) {
            const CVector& pos = object->GetPosition();
            FLog("[DUMMY_GUARD64] ConvertToDummyObject null-dummy object=%p model=%d type=%d status=%d flags=0x%08x objFlags=0x%08x pos=%.2f %.2f %.2f count=%u",
                 object,
                 static_cast<int>(object->m_nModelIndex),
                 static_cast<int>(object->GetType()),
                 static_cast<int>(object->GetStatus()),
                 object->m_nFlags,
                 object->m_nObjectFlags,
                 pos.x,
                 pos.y,
                 pos.z,
                 s_nullDummyCount);
        }
    }

    CPopulation__ConvertToDummyObject(object);
}

void (*CObject__ProcessGarageDoorBehaviour)(CObjectGta* thiz);
void CObject__ProcessGarageDoorBehaviour_hook(CObjectGta* thiz)
{
    static uint32_t s_skipCount = 0;
    ++s_skipCount;
    if (s_skipCount <= 12 || (s_skipCount % 250) == 0) {
        FLog("[GARAGE_GUARD64] skip ProcessGarageDoorBehaviour object=%p model=%d garage=%d dummy=%p count=%u",
             thiz,
             thiz ? static_cast<int>(thiz->m_nModelIndex) : -1,
             thiz ? static_cast<int>(thiz->m_nGarageDoorGarageIndex) : -999,
             thiz ? thiz->m_pDummyObject : nullptr,
             s_skipCount);
    }
}
#endif

#include "Widgets/TouchInterface.h"
void InjectHooks()
{
    FLog("InjectHooks");
    GTASAEngineApi::BindSceneGlobal(&Scene);

#if !VER_x32 // mb all.. wtf crash x64?
    GTASAEngineApi::DisableNativePlayerSkinLoad();
    GTASAEngineApi::DisableNativePopulationInitialise();
    FLog("[POP_FIX64] native CPopulation Initialise disabled on arm64");
#endif
    CCustomCarEnvMapPipeline::InjectHooks();
    CCamera::InjectHooks(); //
    CReferences::InjectHooks(); //
    CModelInfo::injectHooks(); //
    CTimer::InjectHooks(); //
    //cTransmission::InjectHooks(); //
    CAnimBlendAssociation::InjectHooks(); //
    //cHandlingDataMgr::InjectHooks(); //
    CPools::InjectHooks(); //
    CVehicleGTA::InjectHooks(); //
    CMatrixLink::InjectHooks(); //
    CMatrixLinkList::InjectHooks(); //
    Supremo::Streaming::InstallStreamingHooks();
    CPlaceable::InjectHooks(); //
    CMatrix::InjectHooks(); //
    CCollision::InjectHooks(); //
    //CIdleCam::InjectHooks(); //
    CTouchInterface::InjectHooks(); //
    CWidgetGta::InjectHooks();
    CEntityGTA::InjectHooks(); //
    CPhysical::InjectHooks(); //
    CAnimManager::InjectHooks(); //
    //CCarEnterExit::InjectHooks();
    CPlayerPedGta::InjectHooks(); //
    CTaskManager::InjectHooks(); //
    //CPedIntelligence::InjectHooks(); //
    CWorld::InjectHooks(); //
    CGame::InjectHooks();
#if VER_x32
    ES2VertexBuffer::InjectHooks();
    CRQ_Commands::InjectHooks();
#else
    FLog("[RQ_GUARD64] legacy ES2/RQ command hooks skipped on arm64");
#endif
    CTxdStore::InjectHooks();
    CVisibilityPlugins::InjectHooks();
    //CAdjustableHUD::InjectHooks();

    // new
    //CClouds::InjectHooks();
    //CWeather::InjectHooks();
    //RenderBuffer::InjectHooks();
    CTimeCycle::InjectHooks();
    CCoronas::InjectHooks();
    //CDraw::InjectHooks();
    //CClock::InjectHooks();
    //CBirds::Init();
    CVehicleModelInfo::InjectHooks();
    //CPathFind::InjectHooks();
    CSprite2d::InjectHooks();
    Supremo::FileLoader::InstallHooks();
    //CShadows::InjectHooks();
    CPickups::InjectHooks();
    CRenderer::InjectHooks();
    Supremo::Streaming::InstallStreamingInfoHooks();
    TextureDatabase::InjectHooks();
    TextureDatabaseEntry::InjectHooks();
    TextureDatabaseRuntime::InjectHooks();
#if !VER_x32
    InstallRadar64Hooks();
    InstallTextureLookup64Hooks();
    InstallTextureRuntimeStreaming64Hooks();
#endif
    CCustomBuildingDNPipeline::InjectHooks();
    //CWidgetRadar::InjectHooks();

    //CRealTimeShadowManager::InjectHooks();
    GTASAEngineApi::BindOcclusionGlobals(&COcclusion::aOccluders, &COcclusion::NumOccludersOnMap);
}

void InstallSpecialHooks()
{
    InjectHooks();
    using NativePltSlot = GTASAEngineApi::NativePltSlot;
    CHook::Redirect("_ZN5CGame20InitialiseRenderWareEv", &CGame::InitialiseRenderWare);
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::StartGameScreenOnNewGameCheck, (uintptr_t)StartGameScreen__OnNewGameCheck_hook, (uintptr_t*)&StartGameScreen__OnNewGameCheck);

    CHook::InlineHook("_Z10NvUtilInitv", &NvUtilInit_hook, &NvUtilInit);

    CHook::RET("_ZN12CCutsceneMgr16LoadCutsceneDataEPKc"); // LoadCutsceneData
    CHook::RET("_ZN12CCutsceneMgr10InitialiseEv");			// CCutsceneMgr::Initialise

    CHook::Redirect("_Z7NvFOpenPKcS0_bb", &NvFOpen);

    CHook::InlineHook("_ZN14MainMenuScreen6UpdateEf", &MainMenuScreen__Update_hook, &MainMenuScreen__Update);

    CHook::RET("_ZN4CPed31RemoveWeaponWhenEnteringVehicleEi"); // CPed::RemoveWeaponWhenEnteringVehicle

#if VER_x32
    GTASAEngineApi::InstallNativePltHook(NativePltSlot::RleDecompress, (uintptr_t)RLEDecompress_hook, (uintptr_t*)&RLEDecompress);
    CHook::InlineHook("_Z11OS_FileReadPvS_i", &OS_FileRead_hook, &OS_FileRead);
#else
    CHook::InlineHook("_Z13RLEDecompressPhjPKhjj", &RLEDecompress_hook, &RLEDecompress);
    CHook::InlineHook("_Z11OS_FileReadPvS_i", &OS_FileRead_hook, &OS_FileRead);
    CHook::InlineHook("_ZN7CObjectC2EP12CDummyObject", &CObjectFromDummyCtor_hook, &CObjectFromDummyCtor);
    FLog("[RLE_FIX64] enabled safe RLE/OS_FileRead hooks on arm64");
    FLog("[OBJ_POOL64] guarded CObject(CDummyObject*) null-constructor path on arm64");
#endif

    const GTASAEngineApi::NativeInlineHook renderCallbacks[] = {
        {"_Z32_rxOpenGLDefaultAllInOneRenderCBP10RwResEntryPvhj", reinterpret_cast<uintptr_t>(&rxOpenGLDefaultAllInOneRenderCB_hook), reinterpret_cast<void**>(&rxOpenGLDefaultAllInOneRenderCB)},
        {"_ZN25CCustomBuildingDNPipeline18CustomPipeRenderCBEP10RwResEntryPvhj", reinterpret_cast<uintptr_t>(&CCustomBuildingDNPipeline__CustomPipeRenderCB_hook), reinterpret_cast<void**>(&CCustomBuildingDNPipeline__CustomPipeRenderCB)},
    };
    GTASAEngineApi::InstallRenderPipelineInlineHooksNative(
        renderCallbacks,
        sizeof(renderCallbacks) / sizeof(renderCallbacks[0]));
}

#include <EGL/egl.h>
#include <GLES2/gl2.h>   // If using OpenGL ES 2.0 or 3.0
void SetUpGLHooks();
void InstallHooks()
{
    //SetUpGLHooks();
    // Preserve the engine's complete Render2dStuff pass, then append the
    // SA-MP textdraw/chat layer once.  InlineHook supplies the original
    // trampoline; Redirect would replace the stock radar/touch pass.
    CHook::InlineHook("_Z13Render2dStuffv",
                      reinterpret_cast<void*>(&Render2dStuff_hook),
                      reinterpret_cast<void**>(&Render2dStuffNative));

    // Capture the stock Android touch entry point so the NV event path can
    // forward gameplay touches to the original native widgets.  The custom
    // camera/touch pipeline remains disabled for the vanilla mobile baseline.
    GTASAEngineApi::InstallAndroidTouchEventHookNative(
        reinterpret_cast<uintptr_t>(&AND_TouchEvent_hook),
        reinterpret_cast<void**>(&AND_TouchEvent));

    GTASAEngineApi::InstallHudColourHooksNative(reinterpret_cast<uintptr_t>(&CHudColours__GetIntColour),
                                                reinterpret_cast<uintptr_t>(&CRadar__GetRadarTraceColor));
    GTASAEngineApi::InstallRadarCoordBlipHookNative(reinterpret_cast<uintptr_t>(&CRadar__SetCoordBlip_hook),
                                                    reinterpret_cast<void**>(&CRadar__SetCoordBlip));
    GTASAEngineApi::InstallRadarGangOverlayHookNative(reinterpret_cast<uintptr_t>(&CRadar_DrawRadarGangOverlay_hook),
                                                      reinterpret_cast<void**>(&CRadar_DrawRadarGangOverlay));

    GTASAEngineApi::InstallTextureGetHookNative(reinterpret_cast<uintptr_t>(&CUtil::GetTexture));

    CHook::InlineHook("_ZN14MainMenuScreen6OnExitEv", &MainMenuScreen__OnExit_hook, &MainMenuScreen__OnExit);

    CHook::InlineHook("_ZN17CTaskSimpleUseGun17RemoveStanceAnimsEP4CPedf", &CTaskSimpleUseGun__RemoveStanceAnims_hook, &CTaskSimpleUseGun__RemoveStanceAnims);

    // Bullet sync
    CHook::InlineHook("_ZN7CWeapon14FireInstantHitEP7CEntityP7CVectorS3_S1_S3_S3_bb", &CWeapon__FireInstantHit_hook, &CWeapon__FireInstantHit);
    CHook::InlineHook("_ZN7CWeapon10FireSniperEP4CPedP7CEntityP7CVector", &CWeapon__FireSniper_hook, &CWeapon__FireSniper);
    GTASAEngineApi::InstallWorldLineOfSightHookNative(
        reinterpret_cast<uintptr_t>(&CWorld__ProcessLineOfSight_hook),
        reinterpret_cast<void**>(&CWorld__ProcessLineOfSight));
#if !VER_x32
    GTASAEngineApi::InstallWorldVerticalLineHookNative(
        reinterpret_cast<uintptr_t>(&CWorld__ProcessVerticalLine_hook),
        reinterpret_cast<void**>(&CWorld__ProcessVerticalLineNative));
    FLog("[COL_VENTITY64] CWorld::ProcessVerticalLine hook installed");
    GTASAEngineApi::InstallPhysicalCheckCollisionHookNative(
        reinterpret_cast<uintptr_t>(&CPhysical__CheckCollision_hook),
        reinterpret_cast<void**>(&CPhysical__CheckCollisionNative));
    FLog("[COL_PHYS64] CPhysical::CheckCollision hook installed");
#endif
    CHook::InlineHook("_ZN28CPedDamageResponseCalculator21ComputeDamageResponseEP4CPedR18CPedDamageResponseb", &CPedDamageResponseCalculator__ComputeDamageResponse_hook, &CPedDamageResponseCalculator__ComputeDamageResponse);
    CHook::InlineHook("_ZN7CWeapon18ProcessLineOfSightERK7CVectorS2_R9CColPointRP7CEntity11eWeaponTypeS6_bbbbbbb", &CWeapon__ProcessLineOfSight_hook, &CWeapon__ProcessLineOfSight);
    CHook::InlineHook("_ZN11CBulletInfo9AddBulletEP7CEntity11eWeaponType7CVectorS3_", &CBulletInfo_AddBullet_hook, &CBulletInfo_AddBullet);

    //CHook::InlineHook("_ZN11CFileLoader18LoadObjectInstanceEPKc", &CFileLoader__LoadObjectInstance_hook, &CFileLoader__LoadObjectInstance);

    GTASAEngineApi::InstallRadarClearBlipHookNative(reinterpret_cast<uintptr_t>(&CRadar_ClearBlip_hook),
                                                    reinterpret_cast<void**>(&CRadar_ClearBlip));

    GTASAEngineApi::InstallCollisionVerticalLineHookNative(
        reinterpret_cast<uintptr_t>(&CCollision__ProcessVerticalLine_hook),
        reinterpret_cast<void**>(&CCollision__ProcessVerticalLine));

    CHook::InlineHook("_ZN19CUpsideDownCarCheck15IsCarUpsideDownEPK8CVehicle", &CUpsideDownCarCheck__IsCarUpsideDown_hook, &CUpsideDownCarCheck__IsCarUpsideDown);

    CHook::InlineHook("_ZN16CTaskSimpleGetUp10ProcessPedEP4CPed", &CTaskSimpleGetUp__ProcessPed_hook, &CTaskSimpleGetUp__ProcessPed); // CTaskSimpleGetUp::ProcessPed
#if !VER_x32
    CBike__SetupModelNodes = GTASAEngineApi::BikeSetupModelNodes();
    CHook::InlineHook("_ZN5CBike20SetupSuspensionLinesEv",
                      &CBike__SetupSuspensionLines_hook,
                      &CBike__SetupSuspensionLines);
    const GTASAEngineApi::NativeInlineHook singleplayerDeadWeightHooks[] = {
        {"_ZN17CTheCarGenerators7ProcessEv", reinterpret_cast<uintptr_t>(&CTheCarGenerators__Process_hook), reinterpret_cast<void**>(&CTheCarGenerators__Process)},
        {"_ZN17CScriptsForBrains27CheckIfNewEntityNeedsScriptEP7CEntityaP13CPedGenerator", reinterpret_cast<uintptr_t>(&CScriptsForBrains__CheckIfNewEntityNeedsScript_hook), reinterpret_cast<void**>(&CScriptsForBrains__CheckIfNewEntityNeedsScript)},
        {"_ZN11CTheScripts31AddToWaitingForScriptBrainArrayEP7CEntitys", reinterpret_cast<uintptr_t>(&CTheScripts__AddToWaitingForScriptBrainArray_hook), reinterpret_cast<void**>(&CTheScripts__AddToWaitingForScriptBrainArray)},
        {"_ZN11CTheScripts33ProcessWaitingForScriptBrainArrayEv", reinterpret_cast<uintptr_t>(&CTheScripts__ProcessWaitingForScriptBrainArray_hook), reinterpret_cast<void**>(&CTheScripts__ProcessWaitingForScriptBrainArray)},
        {"_ZN17CScriptsForBrains27StartNewStreamedScriptBrainEhP7CEntityh", reinterpret_cast<uintptr_t>(&CScriptsForBrains__StartNewStreamedScriptBrain_hook), reinterpret_cast<void**>(&CScriptsForBrains__StartNewStreamedScriptBrain)},
        {"_ZN8CShadows18StoreShadowForPoleEP7CEntityfffffj", reinterpret_cast<uintptr_t>(&CShadows__StoreShadowForPole_hook), reinterpret_cast<void**>(&CShadows__StoreShadowForPole)},
        {"_ZN11CPopulation20ConvertToDummyObjectEP7CObject", reinterpret_cast<uintptr_t>(&CPopulation__ConvertToDummyObject_hook), reinterpret_cast<void**>(&CPopulation__ConvertToDummyObject)},
        {"_ZN7CObject26ProcessGarageDoorBehaviourEv", reinterpret_cast<uintptr_t>(&CObject__ProcessGarageDoorBehaviour_hook), reinterpret_cast<void**>(&CObject__ProcessGarageDoorBehaviour)},
    };
    GTASAEngineApi::InstallSingleplayerDeadWeightHooksNative(
        singleplayerDeadWeightHooks,
        sizeof(singleplayerDeadWeightHooks) / sizeof(singleplayerDeadWeightHooks[0]));
    FLog("[VEH_GUARD64] bike suspension and car-generator guards installed");
    FLog("[BRAIN_GUARD64] streamed object brain scripts and wait queue guarded on arm64 multiplayer");
    FLog("[SHADOW_GUARD64] unsafe pole shadow guard installed");
    GTASAEngineApi::DisableNativeStoredShadowRenderer();
    FLog("[SHADOW_GUARD64] stored shadow renderer disabled on arm64 multiplayer");
    const GTASAEngineApi::NativeInlineHook rendererPreRenderHooks[] = {
        {"_ZN9CRenderer9PreRenderEv", reinterpret_cast<uintptr_t>(&CRenderer_PreRender_hook), reinterpret_cast<void**>(&CRenderer_PreRender)},
    };
    GTASAEngineApi::InstallRenderPipelineInlineHooksNative(
        rendererPreRenderHooks,
        sizeof(rendererPreRenderHooks) / sizeof(rendererPreRenderHooks[0]));
    FLog("[RENDER_GUARD64] prerender list sanitizer installed");

    GTASAEngineApi::PatchObjectDestructorBridgeGuard();
    FLog("[DUMMY_GUARD64] CObject::~CObject bridge null-dummy branch patched");
    FLog("[GARAGE_GUARD64] CObject::ProcessGarageDoorBehaviour hook installed");
#endif
    CHook::InlineHook("_ZN7CObject6RenderEv", &CObject_Render_hook, & CObject_Render);

    CHook::Redirect("_Z19PlayerIsEnteringCarv", &PlayerIsEnteringCar);
    if(GTASAEngineApi::ShouldRedirectTextureListingMipCount())
    {
        GTASAEngineApi::InstallTextureListingMipCountHookNative(reinterpret_cast<uintptr_t>(&getmip));
    }

    if (!eglGetProcAddress("glAlphaFuncQCOM")) {
        // If "glAlphaFuncQCOM" is not available, try "glAlphaFunc"

        if (eglGetProcAddress("glAlphaFunc")) {
            // If "glAlphaFunc" is found, store the address in the global library
            GTASAEngineApi::SetAlphaFuncProc((void*)eglGetProcAddress("glAlphaFunc"));
        } else {
            // If neither function is available, hook the fallback symbol
            GTASAEngineApi::InstallRqSetAlphaTestHookNative(reinterpret_cast<uintptr_t>(&RQCommand_rqSetAlphaTest));
        }
    }

#if !VER_x32
    GTASAEngineApi::InstallRqVertexBufferDeleteHookNative(reinterpret_cast<uintptr_t>(&RQCommand_VertexBufferDeleteGuard64));
    FLog("[RQ_GUARD64] vertex-buffer delete guard installed");
#endif

    GTASAEngineApi::InstallInputTypeHookNative(reinterpret_cast<uintptr_t>(&GetInputType));

#if VER_x32
    CHook::InlineHook("_ZN14CAnimBlendNode12FindKeyFrameEf", &CAnimBlendNode__FindKeyFrame_hook, &CAnimBlendNode__FindKeyFrame);
    CHook::InlineHook("_ZN15CClumpModelInfo14GetFrameFromIdEP7RpClumpi", &CClumpModelInfo_GetFrameFromId_hook, &CClumpModelInfo_GetFrameFromId);
#endif

    const GTASAEngineApi::NativeInlineHook renderPipelineHooks[] = {
        {"_ZN13FxEmitterBP_c6RenderEP8RwCamerajfh", reinterpret_cast<uintptr_t>(&FxEmitterBP_c__Render_hook), reinterpret_cast<void**>(&FxEmitterBP_c__Render)},
        {"_Z23RwResourcesFreeResEntryP10RwResEntry", reinterpret_cast<uintptr_t>(&RwResourcesFreeResEntry_hook), reinterpret_cast<void**>(&RwResourcesFreeResEntry)},
        {"_ZN9CRenderer24RenderEverythingBarRoadsEv", reinterpret_cast<uintptr_t>(&CRenderer_RenderEverythingBarRoads_hook), reinterpret_cast<void**>(&CRenderer_RenderEverythingBarRoads)},
        {"_ZN9CRenderer11RenderRoadsEv", reinterpret_cast<uintptr_t>(&CRenderer_RenderRoads_hook), reinterpret_cast<void**>(&CRenderer_RenderRoads)},
    };
    GTASAEngineApi::InstallRenderPipelineInlineHooksNative(
        renderPipelineHooks,
        sizeof(renderPipelineHooks) / sizeof(renderPipelineHooks[0]));

    ms_fAspectRatio = GTASAEngineApi::AspectRatio();
    GTASAEngineApi::InstallCrosshairHookNative(reinterpret_cast<uintptr_t>(&DrawCrosshair_hook),
                                               reinterpret_cast<void**>(&DrawCrosshair));

    // retexture
    CHook::InlineHook("_ZN7CEntity6RenderEv", &CEntity_Render_hook, &CEntity_Render);

    //CHook::InlineHook("_ZN26CAEGlobalWeaponAudioEntity21ServiceAmbientGunFireEv", &TaskEnterVehicleHook, &TaskEnterVehicle);
    GTASAEngineApi::PatchRendererFrameTiming();

    CHook::InlineHook("_ZN5CDraw6SetFOVEfb", &CDraw__SetFOV_hook, &CDraw__SetFOV);

    CHook::InlineHook("_ZN10CStreaming5Init2Ev", &CStreaming__Init2_hook, &CStreaming__Init2);

    HookCPad();

    uintptr_t cameraProcessAddr = g_libGTASA + (VER_x32 ? (0x003DC7D0 + 1) : 0x4BAB78);
    CHook::InlineHook(cameraProcessAddr, &CCamera_Process_hook, &CCamera_Process);
}

