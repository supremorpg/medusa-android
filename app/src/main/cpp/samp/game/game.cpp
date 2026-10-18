#include "../main.h"
#include "game.h"
#include "../vendor/armhook/patch.h"
#include "SampStreamingApi.h"
#include "MemoryMgr.h"
#include "CFileMgr.h"
#include "game/Textures/TextureDatabaseRuntime.h"
#include "Scene.h"
#include "TxdStore.h"
#include "VisibilityPlugins.h"
#include "net/netgame.h"
#include "CrossHair.h"
#include "Pickups.h"
#include "game/Models/ModelInfo.h"
#include "Enums/eModelID.h"
#include "game/Collision/Collision.h"
#include "World.h"
#include "GTASAEngineApi.h"
#include "SampCollisionApi.h"
#include "SampInputApi.h"
#include "SampIplApi.h"
#include "perf/PerformanceProfiler.h"

#include <cmath>

void ApplySAMPPatchesInGame();
void InitScripting();

bool bUsedPlayerSlots[PLAYER_PED_SLOTS];

alignas(16) static uint16_t gGameTextMessageBuffer[1076]{};
uint16_t *szGameTextMessage = gGameTextMessageBuffer;

bool CGame::bIsGameExiting = false;

#if !VER_x32
extern bool IsPedModel(unsigned int iModelID);

static bool EnsurePedStreamingModelLoaded64(int modelId, const char* context)
{
    if (!Xyron::Streaming::IsValidResourceId(modelId)) {
        FLog("[PED64] invalid model=%d context=%s", modelId, context ? context : "?");
        return false;
    }

    if (Xyron::Streaming::IsModelLoaded(modelId)) {
        return true;
    }

    FLog("[PED64] preloading model=%d context=%s", modelId, context ? context : "?");
    Xyron::Streaming::RequestModel(modelId, STREAMING_GAME_REQUIRED | STREAMING_KEEP_IN_MEMORY);
    Xyron::Streaming::LoadAllRequestedModels(false);

    if (!Xyron::Streaming::IsModelLoaded(modelId)) {
        Xyron::Streaming::TryLoadModel(modelId);
    }

    if (!Xyron::Streaming::IsModelLoaded(modelId)) {
        FLog("[PED64] model=%d still not loaded context=%s", modelId, context ? context : "?");
        return false;
    }

    return true;
}

static int NormalizeNetworkPedSkin64(int skin)
{
    if (!Xyron::Streaming::IsValidResourceId(skin) || skin == MODEL_PLAYER ||
        !IsPedModel(static_cast<unsigned int>(skin))) {
        FLog("[PED64] invalid ped skin=%d, using fallback=%d", skin, MODEL_MALE01);
        return MODEL_MALE01;
    }

    return skin;
}
#endif

static TextureDatabaseRuntime* LoadRuntimeTextureDatabase(const char* name,
                                                          bool fullyLoad,
                                                          TextureDatabaseFormat format)
{
    TextureDatabaseRuntime* db = TextureDatabaseRuntime::Load(name, fullyLoad, format);
#if !VER_x32
    if (db) {
        FLog("[TEX_INIT64] name=%s requested=%d loaded=%d entries=%u thumbs={dxt:%u,pvr:%u,etc:%u}",
             name ? name : "?",
             static_cast<int>(format),
             static_cast<int>(db->loadedFormat),
             db->entries.numEntries,
             db->thumbs[TextureDatabaseFormat::DF_DXT].numEntries,
             db->thumbs[TextureDatabaseFormat::DF_PVR].numEntries,
             db->thumbs[TextureDatabaseFormat::DF_ETC].numEntries);
    } else {
        FLog("[TEX_INIT64] name=%s requested=%d result=null",
             name ? name : "?",
             static_cast<int>(format));
    }
#endif
    return db;
}

inline int FindFirstFreePlayerPedSlot()
{
    for (uint8_t x = 2; x < PLAYER_PED_SLOTS; ++x) {
        if (!bUsedPlayerSlots[x]) {
            FLog("Found free slot: %d", x);
            return x;
        }
    }
    FLog("No free slot found!");
    // Return -1 or an appropriate error code if no free slot is found
    return -1;
}

CGame::CGame()
{
	m_pGamePlayer = nullptr;
	m_bCheckpointsEnabled = false;
	m_bRaceCheckpointsEnabled = false;
	m_dwRaceCheckpointHandle = 0;

	m_bClockEnabled = false;
	m_bInputEnable = true;

	memset(bUsedPlayerSlots, 0, sizeof(bUsedPlayerSlots));
	memset(m_bPreloadedVehicleModels, 0, sizeof(m_bPreloadedVehicleModels));
}

CGame::~CGame()
{

}

void ApplyGlobalPatches();
void InstallHooks();
void CGame::StartGame()
{
	FLog("Starting game..");

    InstallHooks();
    ApplyGlobalPatches();

	GameAimSyncInit();
	InitScripting();
}

void InstallSAMPHooks();
void InstallWidgetHooks();

void CGame::Initialize()
{
	FLog("CGame initializing..");

    ApplySAMPPatchesInGame();
	GameResetRadarColors();

    szGameTextMessage = gGameTextMessageBuffer;
}
// 0.3.7
void CGame::SetMaxStats()
{
    GTASAEngineApi::ApplyMaxStatsPatch();
}
// 0.3.7
void CGame::ToggleThePassingOfTime(bool bOnOff)
{
    m_bClockEnabled = bOnOff;
}
// 0.3.7
void CGame::EnableClock(bool bEnable)
{
    ToggleThePassingOfTime(bEnable);
}
// 0.3.7
void CGame::EnableZoneNames(bool bEnable)
{
	ScriptCommand(&enable_zone_names, bEnable);
}
// 0.3.7
void CGame::SetWorldTime(int iHour, int iMinute)
{
    GTASAEngineApi::SetClockTime(iHour, iMinute);
    ScriptCommand(&set_current_time, iHour, iMinute);
}
// 0.3.7
void CGame::GetWorldTime(int *iHour, int *iMinute)
{
    GTASAEngineApi::GetClockTime(iHour, iMinute);
}
// 0.3.7
void CGame::PreloadObjectsAnims()
{
	// keep the throwable weapon models loaded
	if(!IsModelLoaded(WEAPON_MODEL_TEARGAS)) RequestModel(WEAPON_MODEL_TEARGAS);
	if(!IsModelLoaded(WEAPON_MODEL_GRENADE)) RequestModel(WEAPON_MODEL_GRENADE);
	if(!IsModelLoaded(WEAPON_MODEL_MOLOTOV)) RequestModel(WEAPON_MODEL_MOLOTOV);

	// special action object
	if(!IsModelLoaded(330)) RequestModel(330);
	if(!IsModelLoaded(OBJECT_PARACHUTE)) RequestModel(OBJECT_PARACHUTE);
	if(!IsModelLoaded(OBJECT_CJ_CIGGY)) RequestModel(OBJECT_CJ_CIGGY);
	if(!IsModelLoaded(OBJECT_DYN_BEER_1)) RequestModel(OBJECT_DYN_BEER_1);
	if(!IsModelLoaded(OBJECT_CJ_BEER_B_2)) RequestModel(OBJECT_CJ_BEER_B_2);
	if(!IsModelLoaded(OBJECT_CJ_PINT_GLASS)) RequestModel(OBJECT_CJ_PINT_GLASS);
	if(!IsModelLoaded(18631)) RequestModel(18631);

	// special action anim
	if(IsAnimationLoaded("PARACHUTE") == 0) RequestAnimation("PARACHUTE");
	if(IsAnimationLoaded("PAULNMAC") == 0) RequestAnimation("PAULNMAC");
	if(IsAnimationLoaded("BAR") == 0) RequestAnimation("BAR");
	if(IsAnimationLoaded("SMOKING") == 0) RequestAnimation("SMOKING");
	if(IsAnimationLoaded("DANCING") == 0) RequestAnimation("DANCING");
	if(IsAnimationLoaded("GFUNK") == 0) RequestAnimation("GFUNK");
	if(IsAnimationLoaded("RUNNINGMAN") == 0) RequestAnimation("RUNNINGMAN");
	if(IsAnimationLoaded("STRIP") == 0) RequestAnimation("STRIP");
	if(IsAnimationLoaded("WOP") == 0) RequestAnimation("WOP");

    // Force preloading and locking of Shamal (519) model in memory
    Xyron::Streaming::TryLoadModel(519);
    CBaseModelInfo* shamalInfo = CModelInfo::GetModelInfo(519);
    if (shamalInfo) {
        shamalInfo->m_nRefCount = 999;
    }
}
// 0.3.7
void CGame::SetWorldWeather(int byteWeatherID)
{
    GTASAEngineApi::SetWeatherNow(byteWeatherID);

    if(!m_bClockEnabled)
    {
        GTASAEngineApi::ForceWeatherNow(byteWeatherID);
    }
}
// 0.3.7
void CGame::DisplayHUD(bool bDisp)
{
    GTASAEngineApi::SetNativeHudVisible(bDisp);
}
// 0.3.7
uint8_t CGame::GetActiveInterior()
{
	return CGame::currArea;
}

const char* CGame::GetDataDirectory()
{
	return g_pszStorage ? g_pszStorage : "";
}
// 0.3.7
void CGame::UpdateCheckpoints()
{
	if (m_bCheckpointsEnabled)
	{
		CPlayerPed* pPlayerPed = this->FindPlayerPed();
		if (pPlayerPed) 
		{
			ScriptCommand(&is_actor_near_point_3d, pPlayerPed->m_dwGTAId,
				m_vecCheckpointPos.x, m_vecCheckpointPos.y, m_vecCheckpointPos.z,
				m_vecCheckpointExtent.x, m_vecCheckpointExtent.y, m_vecCheckpointExtent.z, 1);

			if (!m_dwCheckpointMarker)
			{
				m_dwCheckpointMarker = CreateRadarMarkerIcon(0, m_vecCheckpointPos.x,
					m_vecCheckpointPos.y, m_vecCheckpointPos.z, 1005, 0);
			}
		}
	}
	else if (m_dwCheckpointMarker)
	{
		DisableMarker(m_dwCheckpointMarker);
		m_dwCheckpointMarker = 0;
	}

	if (m_bRaceCheckpointsEnabled)
	{
		CPlayerPed* pPlayerPed = this->FindPlayerPed();
		if (pPlayerPed)
		{
			if (!m_dwRaceCheckpointMarker)
			{
				m_dwRaceCheckpointMarker = CreateRadarMarkerIcon(0, m_vecRaceCheckpointPos.x,
					m_vecRaceCheckpointPos.y, m_vecRaceCheckpointPos.z, 1005, 0);
			}
		}
	}
	else if (m_dwRaceCheckpointMarker)
	{
		DisableMarker(m_dwRaceCheckpointMarker);
		DisableRaceCheckpoint();
		m_dwRaceCheckpointMarker = 0;
	}
}
// 0.3.7
uint8_t CGame::GetPedSlotsUsed()
{
	uint8_t count = 0;
	for (int i = 2; i < PLAYER_PED_SLOTS; i++)
	{
		if (bUsedPlayerSlots[i])
			count++;
	}

	return count;
}

void CGame::PlaySound(int iSound, float fX, float fY, float fZ)
{
	ScriptCommand(&play_sound, fX, fY, fZ, iSound);
}
// 0.3.7
void CGame::RefreshStreamingAt(float x, float y)
{
	ScriptCommand(&refresh_streaming_at, x, y);
}
// 0.3.7
void CGame::DisableTrainTraffic()
{
	ScriptCommand(&enable_train_traffic, 0);
}
// 0.3.7
void CGame::UpdateGlobalTimer(uint32_t dwTimer)
{
	if (!m_bClockEnabled)
	{
        GTASAEngineApi::SetGameClockMilliseconds(dwTimer);
	}
}
// 0.3.7
void CGame::SetGravity(float fGravity)
{
    GTASAEngineApi::SetGravity(fGravity);
}

bool CGame::IsGamePaused()
{
	return GTASAEngineApi::IsGamePaused();
}

bool CGame::IsGameLoaded()
{
	return true;
}

void CGame::DrawGangZone(float fPos[], uint32_t dwColor, uint32_t dwUnk)
{
    GTASAEngineApi::DrawRadarArea(fPos, dwColor, dwUnk);
}
// 0.3.7
uint32_t CGame::CreatePickup(int iModel, int iType, float x, float y, float z, int *pdwIndex)
{
    uintptr hnd;

    auto dwModelArray = CModelInfo::ms_modelInfoPtrs;
    if(dwModelArray[iModel] == nullptr)
        iModel = 18631; // вопросик

    ScriptCommand(&create_pickup, iModel, iType, x, y, z, &hnd);

    int lol = 32 * (uint16_t)hnd;
    if(lol) lol /= 32;
    if(pdwIndex) *pdwIndex = lol;

    return hnd;
}
// 0.3.7
bool CGame::IsModelLoaded(int iModel)
{
	if (iModel > 20000 || iModel < 0) {
		return true;
	}
	else {
		return ScriptCommand(&is_model_available, iModel);
	}
}
// 0.3.7
void CGame::RequestModel(uint16_t iModelId, uint8_t iLoadingStream) 
{
    ScriptCommand(&request_model, iModelId);
}
// 0.3.7
void CGame::LoadRequestedModels()
{
	ScriptCommand(&load_requested_models);
}
// 0.3.7
void CGame::RemoveModel(int iModel, bool bFromStreaming)
{
	if (iModel >= 0 && iModel < 20000)
	{
		if (bFromStreaming)
		{
			if(ScriptCommand(&is_model_available, iModel))
                Xyron::Streaming::RemoveStreamingModel(iModel);
		}
		else
		{
			if (ScriptCommand(&is_model_available, iModel))
				ScriptCommand(&release_model, iModel);
		}
	}
}
// 0.3.7 (������������ 2 ��������� ��������� ��������� � 0.3DL)
CObject* CGame::NewObject(int iModel, CVector vecPos, CVector vecRot, float fDrawDistance)
{
	CObject *pObjectNew = new CObject(iModel, vecPos, vecRot, fDrawDistance, 0);
	return pObjectNew;
}
// 0.3.7 (�� ����������� ������ bIsNPC)
CPlayerPed* CGame::NewPlayer(int iSkin, float fX, float fY, float fZ, float fRotation, bool unk, bool bIsNPC)
{
	uint8_t bytePedSlot = FindFirstFreePlayerPedSlot();
	if (!bytePedSlot) return nullptr;
#if !VER_x32
	iSkin = NormalizeNetworkPedSkin64(iSkin);
	if (!EnsurePedStreamingModelLoaded64(MODEL_PLAYER, "NewPlayer base") ||
		!EnsurePedStreamingModelLoaded64(MODEL_MALE01, "NewPlayer fallback") ||
		!EnsurePedStreamingModelLoaded64(iSkin, "NewPlayer skin")) {
		FLog("[PED64] aborting NewPlayer to avoid null ped clump, skin=%d", iSkin);
		return nullptr;
	}
#endif
	auto pPed = new CPlayerPed(bytePedSlot, iSkin, fX, fY, fZ, fRotation);
	if (pPed && pPed->m_pPed) {
		bUsedPlayerSlots[bytePedSlot] = true;
	}

    return pPed;
}
// 0.3.7
bool CGame::RemovePlayer(CPlayerPed* pPed)
{
	if (!pPed) return false;

	delete pPed;
	bUsedPlayerSlots[pPed->m_bytePlayerNumber] = false;
	return true;
}
// 0.3.7
void CGame::DisableMarker(uint32_t dwMarker)
{
	ScriptCommand(&disable_marker, dwMarker);
}
// 0.3.7
uint32_t CGame::CreateRadarMarkerIcon(uint8_t byteType, float fPosX, float fPosY, float fPosZ, uint32_t dwColor, uint8_t byteStyle)
{
    uintptr dwMarkerID = 0;

    if(byteStyle == 1)
        ScriptCommand(&create_marker_icon, fPosX, fPosY, fPosZ, byteType, &dwMarkerID);
    else if(byteStyle == 2)
        ScriptCommand(&create_radar_marker_icon, fPosX, fPosY, fPosZ, byteType, &dwMarkerID);
    else if(byteStyle == 3)
        ScriptCommand(&create_icon_marker_sphere, fPosX, fPosY, fPosZ, byteType, &dwMarkerID);
    else
        ScriptCommand(&create_radar_marker_without_sphere, fPosX, fPosY, fPosZ, byteType, &dwMarkerID);

    if(byteType == 0)
    {
        if(dwColor >= 1004)
        {
            ScriptCommand(&set_marker_color, dwMarkerID, dwColor);
            ScriptCommand(&show_on_radar, dwMarkerID, 3);
        }
        else
        {
            ScriptCommand(&set_marker_color, dwMarkerID, dwColor);
            ScriptCommand(&show_on_radar, dwMarkerID, 2);
        }
    }

    return dwMarkerID;
}
// 0.3.7
bool CGame::IsAnimationLoaded(const char* szAnimLib)
{
	return ScriptCommand(&is_animation_loaded, szAnimLib);
}
// 0.3.7
void CGame::RequestAnimation(const char* szAnimLib)
{
	ScriptCommand(&request_animation, szAnimLib);
}
// 0.3.7
float CGame::FindGroundZForCoord(float fX, float fY, float fZ)
{
    // Use a local downward ray around the supplied Z so multi-level maps
    // return the support under the player, not the highest platform above.
    const float nativeGroundZ = CWorld::FindGroundZForCoord(fX, fY);
    float startZ = fZ + 0.8f;
    float endZ = fZ - 120.0f;
    if (std::isfinite(nativeGroundZ) &&
        fZ < -20.0f &&
        nativeGroundZ > fZ + 8.0f &&
        nativeGroundZ < 3000.0f) {
        startZ = nativeGroundZ + 8.0f;
        endZ = fZ - 120.0f;
    }

    CColPoint col{};
    CEntityGTA* ent = nullptr;
    const CVector start(fX, fY, startZ);
    const CVector end(fX, fY, endZ);
    const bool hit = CWorld::ProcessLineOfSight(
        &start,
        &end,
        &col,
        &ent,
        true,   // buildings
        false,  // vehicles
        false,  // peds
        true,   // objects
        true,   // dummies
        false,  // see-through
        false,  // camera ignore
        false   // shoot-through
    );

    if (hit && ent && ent->m_bUsesCollision) {
        return col.m_vecPoint.z;
    }

    return nativeGroundZ;
}
// 0.3.7
void CGame::DisableAutoAim()
{
    GTASAEngineApi::DisableWeaponLockOnTarget();
}

// 0.3.7
void CGame::EnabledAutoAim()
{
    GTASAEngineApi::DisableWeaponLockOnTarget();
}
// 0.3.7
CVehicle* CGame::NewVehicle(int iVehicleType, float fX, float fY, float fZ, float fRotation, bool bAddSiren)
{
	bool bPreloaded = false;
	if (m_bPreloadedVehicleModels[iVehicleType - 400] == true) {
		bPreloaded = true;
	}

	CVehicle* pNewVehicle = new CVehicle(iVehicleType, fX, fY, fZ, fRotation, bPreloaded, bAddSiren);

	return pNewVehicle;
}
// 0.3.7
void CGame::SetCheckpointInformation(CVector* vecPos, CVector* vecSize)
{
	m_vecCheckpointPos.x = vecPos->x;
	m_vecCheckpointPos.y = vecPos->y;
	m_vecCheckpointPos.z = vecPos->z;

	m_vecCheckpointExtent.x = vecSize->x;
	m_vecCheckpointExtent.y = vecSize->y;
	m_vecCheckpointExtent.z = vecSize->z;
	
	if (m_dwCheckpointMarker)
	{
		DisableMarker(m_dwCheckpointMarker);
		m_dwCheckpointMarker = 0;

		m_dwCheckpointMarker = CreateRadarMarkerIcon(0,
			m_vecCheckpointPos.x,
			m_vecCheckpointPos.y,
			m_vecCheckpointPos.z,
			1005, 0);
	}
}
// 0.3.7
void CGame::SetRaceCheckpointInformation(uint8_t byteType, CVector* vecPos, CVector* vecNextPos, float fRadius)
{
	m_vecRaceCheckpointPos.x = vecPos->x;
	m_vecRaceCheckpointPos.y = vecPos->y;
	m_vecRaceCheckpointPos.z = vecPos->z;

	m_vecRaceCheckpointNextPos.x = vecNextPos->x;
	m_vecRaceCheckpointNextPos.y = vecNextPos->y;
	m_vecRaceCheckpointNextPos.z = vecNextPos->z;

	m_byteRaceType = byteType;
	m_fRaceCheckpointRadius = fRadius;

	if (m_dwRaceCheckpointMarker)
	{
		DisableMarker(m_dwRaceCheckpointMarker);
		
		m_dwRaceCheckpointMarker = CreateRadarMarkerIcon(0,
			m_vecRaceCheckpointPos.x,
			m_vecRaceCheckpointPos.y,
			m_vecRaceCheckpointPos.z,
			1005,
			0);
	}

	MakeRaceCheckpoint();
}
// 0.3.7
void CGame::MakeRaceCheckpoint()
{
	DisableRaceCheckpoint();

	ScriptCommand(&create_racing_checkpoint, (int)m_byteRaceType,
		m_vecRaceCheckpointPos.x, m_vecRaceCheckpointPos.y, m_vecRaceCheckpointPos.z,
		m_vecRaceCheckpointNextPos.x, m_vecRaceCheckpointNextPos.y, m_vecRaceCheckpointNextPos.z,
		m_fRaceCheckpointRadius, &m_dwRaceCheckpointHandle);

	m_bRaceCheckpointsEnabled = true;
}
// 0.3.7
void CGame::DisableRaceCheckpoint()
{
	if (m_dwRaceCheckpointHandle)
	{
		ScriptCommand(&destroy_racing_checkpoint, m_dwRaceCheckpointHandle);
		m_dwRaceCheckpointHandle = 0;
	}

	m_bRaceCheckpointsEnabled = false;
}

void CGame::SetWantedLevel(uint8_t level)
{
    (void)level;
}

void CGame::EnableStuntBonus(bool bEnable)
{
    (void)bEnable;
}
// 0.3.7
void CGame::DisplayGameText(const char* szStr, int iTime, int iSize)
{
    ScriptCommand(&text_clear_all);
    CFont::AsciiToGxtChar(szStr, szGameTextMessage);

    GTASAEngineApi::AddBigMessage(szGameTextMessage, iTime, iSize);
}
// 0.3.7
void CGame::AddToLocalMoney(int iAmmount)
{
	ScriptCommand(&add_to_player_money, 0, iAmmount);
}
// 0.3.7
void CGame::ResetLocalMoney()
{
	int iMoney = GetLocalMoney();
	if (!iMoney) return;

	if (iMoney < 0)
		AddToLocalMoney(abs(iMoney));
	else
		AddToLocalMoney(-(iMoney));
}
// 0.3.7
int CGame::GetLocalMoney()
{
	return 0;
}
// 0.3.7
void CGame::DisableEnterExits()
{
    GTASAEngineApi::DisableEnterExits();
}

void CGame::ToggleCJWalk(bool bUseCJWalk)
{
    (void)bUseCJWalk;
    GTASAEngineApi::DisableCjWalkPatch();
}

void CGame::InitialiseOnceBeforeRW() {
    CMemoryMgr::Init();
    GTASAEngineApi::InitialiseMobileSettings();
    GTASAEngineApi::InitialiseLocalisation();
    CFileMgr::Initialise();
    Xyron::Streaming::InitialiseCdStreams();
    GTASAEngineApi::InitialisePad();
}

void CameraSize(RwCamera* camera, RwRect* rect, RwReal viewWindow, RwReal aspectRatio) {
    GTASAEngineApi::SizeCamera(camera, rect, viewWindow, aspectRatio);
}

void CameraDestroy(RwCamera* camera) {
    GTASAEngineApi::DestroyCamera(camera);
}


void LightsCreate(RpWorld* world) {
    GTASAEngineApi::CreateLights(world);
}

void InitGui();

bool CGame::InitialiseRenderWare() {
    FLog("InitialiseRenderWare ..");

    CCamera& TheCamera = GTASAEngineApi::Camera();

    CTxdStore::Initialise();
    CVisibilityPlugins::Initialise();

#if VER_x32
    constexpr TextureDatabaseFormat sampTexFormat = TextureDatabaseFormat::DF_Default;
    constexpr TextureDatabaseFormat txdTexFormat = TextureDatabaseFormat::DF_Default;
    constexpr TextureDatabaseFormat gta3TexFormat = TextureDatabaseFormat::DF_Default;
    constexpr TextureDatabaseFormat gtaIntTexFormat = TextureDatabaseFormat::DF_Default;
    constexpr TextureDatabaseFormat mobileTexFormat = TextureDatabaseFormat::DF_Default;
#else
    // The bundled world texdb package on this data set ships gta3/gta_int/txd/mobile
    // as .dxt banks only. DF_Default may choose ETC on Adreno and leaves Loading
    // placeholder textures on large world surfaces.
    constexpr TextureDatabaseFormat sampTexFormat = TextureDatabaseFormat::DF_Default;
    constexpr TextureDatabaseFormat txdTexFormat = TextureDatabaseFormat::DF_DXT;
    constexpr TextureDatabaseFormat gta3TexFormat = TextureDatabaseFormat::DF_DXT;
    constexpr TextureDatabaseFormat gtaIntTexFormat = TextureDatabaseFormat::DF_DXT;
    constexpr TextureDatabaseFormat mobileTexFormat = TextureDatabaseFormat::DF_DXT;
#endif

#if VER_SAMP
    LoadRuntimeTextureDatabase("samp", false, sampTexFormat);
    LoadRuntimeTextureDatabase("mobile", false, mobileTexFormat);
    LoadRuntimeTextureDatabase("txd", false, txdTexFormat);
    LoadRuntimeTextureDatabase("gta3", false, gta3TexFormat);
    LoadRuntimeTextureDatabase("gta_int", false, gtaIntTexFormat);
    LoadRuntimeTextureDatabase("cutscene", false, TextureDatabaseFormat::DF_Default);
    LoadRuntimeTextureDatabase("player", false, TextureDatabaseFormat::DF_PVR);
    LoadRuntimeTextureDatabase("menu", false, TextureDatabaseFormat::DF_PVR);
#else
    LoadRuntimeTextureDatabase("samp", false, sampTexFormat);
    LoadRuntimeTextureDatabase("mobile", false, mobileTexFormat);
    LoadRuntimeTextureDatabase("txd", false, txdTexFormat);
    LoadRuntimeTextureDatabase("gta3", false, gta3TexFormat);
    LoadRuntimeTextureDatabase("gta_int", false, gtaIntTexFormat);
    LoadRuntimeTextureDatabase("player", false, TextureDatabaseFormat::DF_PVR);
    LoadRuntimeTextureDatabase("menu", false, TextureDatabaseFormat::DF_PVR);
    //TextureDatabaseRuntime::Load("cutscene", false, TextureDatabaseFormat::DF_Default);

#endif


    const auto camera = RwCameraCreate();
    if (!camera) {
        CameraDestroy(camera);
        return false;
    }

    const auto frame = RwFrameCreate();
    rwObjectHasFrameSetFrame(&camera->object.object, frame);
    camera->frameBuffer = RwRasterCreate(RsGlobal->maximumWidth, RsGlobal->maximumHeight, 0, rwRASTERTYPECAMERA);
    camera->zBuffer = RwRasterCreate(RsGlobal->maximumWidth, RsGlobal->maximumHeight, 0, rwRASTERTYPEZBUFFER);
    if (!camera->object.object.parent) {
        CameraDestroy(camera);
        return false;
    }
    Scene.m_pRwCamera = camera;
    TheCamera.Init();
    TheCamera.SetRwCamera(Scene.m_pRwCamera);
    RwCameraSetFarClipPlane(Scene.m_pRwCamera, 2000.0f);
    RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.9f);
    CameraSize(Scene.m_pRwCamera, nullptr, 0.7f, 4.0f / 3.0f);

    RwBBox bb;
    bb.sup = { 10'000.0f,  10'000.0f,  10'000.0f};
    bb.inf = {-10'000.0f, -10'000.0f, -10'000.0f};

    if (Scene.m_pRpWorld = RpWorldCreate(&bb); !Scene.m_pRpWorld) {
        CameraDestroy(Scene.m_pRwCamera);
        Scene.m_pRwCamera = nullptr;

        return false;
    }
    RpWorldAddCamera(Scene.m_pRpWorld, Scene.m_pRwCamera);
    LightsCreate(Scene.m_pRpWorld);
//	CreateDebugFont();
    CFont::Initialise();
    GTASAEngineApi::InitialiseHud();
    GTASAEngineApi::InitialisePlayerSkin();
    GTASAEngineApi::InitialisePostEffects();
    CGame::m_pWorkingMatrix1 = RwMatrixCreate();
    CGame::m_pWorkingMatrix2 = RwMatrixCreate();

    InitGui();

    return true;
}

void CGame::PostToMainThread(std::function<void()> task)
{
    std::lock_guard<std::mutex> lock(mtx);
    tasks.push(std::move(task));
}

void CGame::ProcessMainThreadTasks()
{
    if (tasks.empty())
        return;

    std::function<void()> task;
    {
        std::lock_guard<std::mutex> lock(mtx);

        task = std::move(tasks.front());
        tasks.pop();
    }
    task();
}
extern CGame* pGame;
extern CNetGame* pNetGame;
extern UI *pUI;

namespace {
void ForceSpawnCollisionDebug()
{
    static uint32_t s_lastBaseTick = 0;
    static uint32_t s_lastBurstTick = 0;
    static uint32_t s_lastBlockingLoadTick = 0;
    static uint32_t s_lastLogTick = 0;

    if (!pNetGame || !pGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) {
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

    CPlayerPed* localPlayerPed = localPlayer->GetPlayerPed();
    if (!localPlayerPed || !localPlayerPed->m_pPed) {
        return;
    }

    const uint32_t now = GetTickCount();
    const CVector pos = localPlayerPed->m_pPed->GetPosition();
    const CVector vel = localPlayerPed->m_pPed->GetMoveSpeed();
    const float groundZ = pGame->FindGroundZForCoord(pos.x, pos.y, pos.z + 3.0f);
    const float dz = pos.z - groundZ;
    const bool inVehicle = localPlayerPed->IsInVehicle();
    const float speedSq = vel.x * vel.x + vel.y * vel.y + vel.z * vel.z;
    const bool unstableGround = (groundZ <= 0.01f) || (dz < -2.5f);
    const bool movingFast = speedSq > (inVehicle ? 1.00f : 0.25f);
    const bool stationaryStable = !unstableGround && speedSq <= 0.0025f;

    auto requestAt = [&](const CVector& sample) {
        CVector loadSample(sample.x, sample.y, sample.z);
        const float nativeAtSample = CWorld::FindGroundZForCoord(sample.x, sample.y);
        if (std::isfinite(nativeAtSample) &&
            sample.z < 5.0f &&
            nativeAtSample > sample.z + 8.0f &&
            nativeAtSample < 3000.0f) {
            loadSample.z = nativeAtSample + 2.0f;
        } else if (loadSample.z < 18.0f) {
            loadSample.z = 18.0f;
        }

        Xyron::Collision::AddCollisionNeededAtPosn(loadSample);
        Xyron::Collision::RequestCollision(loadSample, localPlayerPed->m_pPed->m_nAreaCode);
        Xyron::Ipl::AddNeededAtPosition(loadSample);
        Xyron::Ipl::LoadAtPosition(loadSample, false);
        Xyron::Ipl::EnsureInMemory(loadSample);
        Xyron::Streaming::LoadSceneCollision(loadSample);
        if (pGame) {
            pGame->RefreshStreamingAt(loadSample.x, loadSample.y);
        }
    };

    auto requestCross = [&](const CVector& center, float radius) {
        const float diag = radius * 0.70710678f;
        requestAt(center);
        requestAt(CVector(center.x + radius, center.y, center.z));
        requestAt(CVector(center.x - radius, center.y, center.z));
        requestAt(CVector(center.x, center.y + radius, center.z));
        requestAt(CVector(center.x, center.y - radius, center.z));
        requestAt(CVector(center.x + diag, center.y + diag, center.z));
        requestAt(CVector(center.x + diag, center.y - diag, center.z));
        requestAt(CVector(center.x - diag, center.y + diag, center.z));
        requestAt(CVector(center.x - diag, center.y - diag, center.z));
    };

    localPlayerPed->m_pPed->m_bUsesCollision = true;
    localPlayerPed->m_pPed->physicalFlags.bCollidable = true;
    localPlayerPed->m_pPed->physicalFlags.bCanBeCollidedWith = true;
    localPlayerPed->m_pPed->physicalFlags.bDisableSimpleCollision = false;
    localPlayerPed->m_pPed->SetCollisionChecking(true);
    localPlayerPed->m_pPed->physicalFlags.bApplyGravity = true;

    auto flushStreamingQueue = [&](bool allowBlocking) {
        if (allowBlocking && now - s_lastBlockingLoadTick >= 1800) {
            s_lastBlockingLoadTick = now;
            Xyron::Streaming::LoadAllRequestedModels(false);
        } else {
            Xyron::Streaming::LoadRequestedModels();
        }
    };

    const uint32_t baseInterval =
        stationaryStable ? 1800u :
        (inVehicle ? 650u : 850u);
    if (now - s_lastBaseTick >= baseInterval) {
        s_lastBaseTick = now;
        requestCross(pos, inVehicle ? 30.0f : 16.0f);
        flushStreamingQueue(false);
    }

    if ((unstableGround || movingFast) && now - s_lastBurstTick >= 320) {
        s_lastBurstTick = now;
        requestCross(pos, inVehicle ? 58.0f : 34.0f);
        const CVector ahead(
            pos.x + vel.x * (inVehicle ? 18.0f : 11.0f),
            pos.y + vel.y * (inVehicle ? 18.0f : 11.0f),
            pos.z + vel.z * (inVehicle ? 8.0f : 5.0f));
        requestCross(ahead, inVehicle ? 42.0f : 24.0f);
        flushStreamingQueue(unstableGround);
    }

    if (now - s_lastLogTick >= 1500) {
        s_lastLogTick = now;
        FLog("[COL_ROOT64] stream-watch pos=%.2f %.2f %.2f ground=%.2f dz=%.2f vel2=%.3f area=%d coll={use:%d,col:%d,hit:%d,simple:%d}",
             pos.x,
             pos.y,
             pos.z,
             groundZ,
             dz,
             speedSq,
             localPlayerPed->m_pPed->m_nAreaCode,
             localPlayerPed->m_pPed->m_bUsesCollision ? 1 : 0,
             localPlayerPed->m_pPed->physicalFlags.bCollidable ? 1 : 0,
             localPlayerPed->m_pPed->physicalFlags.bCanBeCollidedWith ? 1 : 0,
             localPlayerPed->m_pPed->physicalFlags.bDisableSimpleCollision ? 0 : 1);
    }
}
}

void MainLoop();
void CGame::Process() {
    if(bIsGameExiting)return;
    Xyron::Perf::ScopedZone perfGameProcess(Xyron::Perf::Zone::GameProcess);

    MainLoop();
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
            {
                Xyron::Perf::ScopedZone perfObjects(Xyron::Perf::Zone::Objects);
                pObjectPool->Process();
            }
            {
                Xyron::Perf::ScopedZone perfObjectMaterial(Xyron::Perf::Zone::ObjectMaterial);
                pObjectPool->ProcessMaterialText();
            }
        }

        CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();
        if (pTextDrawPool) {
            pTextDrawPool->SnapshotProcess();
        }
    }

    ProcessMainThreadTasks();

    uint32_t CurrentTimeInCycles;
    uint32_t v1; // r4
    uint32_t v2; // r5
    uint32_t v3; // r5

    Xyron::Input::ProcessNativeFrame();

//	CLoadMonitor::BeginFrame(&g_LoadMonitor);
    CurrentTimeInCycles = CTimer::GetCurrentTimeInCycles();
    v1 = CurrentTimeInCycles / CTimer::GetCyclesPerMillisecond();

    {
        Xyron::Perf::ScopedZone perfStreaming(Xyron::Perf::Zone::Streaming);
        Xyron::Streaming::UpdateStreamingFrame();
    }

    v2 = CTimer::GetCurrentTimeInCycles();
    v3 = v2 / CTimer::GetCyclesPerMillisecond();

    {
        Xyron::Perf::ScopedZone perfCollision(Xyron::Perf::Zone::Collision);
        ForceSpawnCollisionDebug();
    }

    //	CCutsceneMgr::Update();

    if ( !(CTimer::m_CodePause << 0x18) )
    {
        GTASAEngineApi::UpdateMobileMenu();
    }

    // CTheZones::Update()

    // CCover::Update()

    CCamera& TheCamera = GTASAEngineApi::Camera();

//	auto p_tx = (CSimpleTransform *)&TheCamera + 0x14 + 0x30;
//	if ( !TheCamera.m_pMat )
//		p_tx = *TheCamera + 0x4;

    //CAudioZones::Update(0, p_tx->m_translate);

    GTASAEngineApi::SetWindModifiersCount(0);

    if ( !CTimer::m_CodePause && !CTimer::m_UserPause )
    {
        CSprite2d::SetRecipNearClip();
        GTASAEngineApi::InitSprite2dPerFrame();
        GTASAEngineApi::InitFontPerFrame();
        // CCheat::DoCheats();
        // CClock::Update()

        GTASAEngineApi::UpdateWeather();
        if (!pNetGame) {
            GTASAEngineApi::ProcessScripts();
        } else {
            static bool s_scriptsProcessSkipLogged = false;
            if (!s_scriptsProcessSkipLogged) {
                s_scriptsProcessSkipLogged = true;
                FLog("[SCRIPT_GUARD] skipped CTheScripts::Process in multiplayer runtime");
            }
        }
        // CCollision::Update()
        //CCollision::Update();

        GTASAEngineApi::UpdateTrains();
        GTASAEngineApi::UpdateSkidmarks();
        GTASAEngineApi::UpdateGlass();
        // CWanted::UpdateEachFrame();
        // CCreepingFire::Update();
        // CSetPieces::Update();

        GTASAEngineApi::UpdateFireManager();

        if (VER_x32) {
            GTASAEngineApi::UpdatePopulation(false);
        } else {
            static bool s_populationUpdateSkipLogged = false;
            if (!s_populationUpdateSkipLogged) {
                s_populationUpdateSkipLogged = true;
                FLog("[POP_FIX64] skipped CPopulation::Update on arm64");
            }
        }

        GTASAEngineApi::UpdateWeapons();
//		if ( !CCutsceneMgr::ms_running )
//			CTheCarGenerators::Process();
//		CCranes::UpdateCranes();
//		CClouds::Update();
        GTASAEngineApi::UpdateMovingThings();
        GTASAEngineApi::UpdateWaterCannons();
//		CUserDisplay::Process();
        {
            GTASAEngineApi::ProcessWorld();
        }

//		CLoadMonitor::EndFrame(&g_LoadMonitor);

        if ( !CTimer::bSkipProcessThisFrame )
        {
            CPickups::Update();
//			CCarCtrl::PruneVehiclesOfInterest();
            GTASAEngineApi::UpdateGarages();
// 			CEntryExitManager::Update();
            GTASAEngineApi::UpdateStuntJumps();
            GTASAEngineApi::UpdateBirds();
            GTASAEngineApi::UpdateSpecialFx();
            // CRopes::Update();
        }
        GTASAEngineApi::UpdatePostEffects();
        GTASAEngineApi::UpdateTimeCycle();
        // CPopCycle::Update()

        // CInterestingEvents::ScanForNearbyEntities

        {
            GTASAEngineApi::ProcessCamera(&TheCamera); // CCamera::Process()
            // ApplySourceCameraLook() is called once per frame right before
            // CGame_Process() in CGame_Process_hook (hooks.cpp). Calling it
            // here again would cause double-smoothing and camera tremor.
        }

        // CCullZones::Update() менты не могут найти?
        GTASAEngineApi::UpdateGameLogic();
        // CGangWars::Update();
        // CConversations::Update()
        // CPedToPlayerConversations::Update()
        // CBridge::Update()

        GTASAEngineApi::DoSunAndMoon();
        GTASAEngineApi::UpdateCoronas();
        GTASAEngineApi::UpdatePermanentShadows();

        // CPlantMgr::Update

        GTASAEngineApi::UpdateCustomBuildingRenderer();
//		if ( v6 <= 3 )
//			CCarCtrl::GenerateRandomCars();
//		CRoadBlocks::GenerateRoadBlocks();
//		CCarCtrl::RemoveDistantCars();
//		CCarCtrl::RemoveCarsIfThePoolGetsFull();
        auto temp = TheCamera.m_pRwCamera;

        GTASAEngineApi::UpdateFx(GTASAEngineApi::FxManager(), temp, CTimer::ms_fTimeStep / 50.0f);

        GTASAEngineApi::UpdateBreakManager(GTASAEngineApi::BreakManager(), CTimer::ms_fTimeStep);

        // InteriorManager_c::Update(&g_interiorMan);
        // ProcObjectMan_c::Update

        // WaterCreatureManager_c::Update

        GTASAEngineApi::PreRenderWater();
    }

//	CCheat::ProcessAllCheats();
    static bool once = false;
    if (!once)
    {
        //CCrossHair::Init();
        once = true;
        return;
    }
}

void CGame::InjectHooks()
{
    extern void CGame_Process_hook();
    extern void (*CGame_Process)();

    CHook::Redirect("_ZN5CGame22InitialiseOnceBeforeRWEv", &CGame::InitialiseOnceBeforeRW);
    CHook::InlineHook("_ZN5CGame7ProcessEv", &CGame_Process_hook, &CGame_Process);

    GTASAEngineApi::BindGameGlobals(&CGame::currArea, &CGame::m_pWorkingMatrix1, &CGame::m_pWorkingMatrix2);
}

bool CGame::CanSeeOutSideFromCurrArea() {
    return currArea == AREA_CODE_NORMAL_WORLD;
}
