#pragma once

#include <cstddef>
#include <cstdint>

#include "Camera.h"
#include "Core/Vector.h"

class CColPoint;
class CRect;
class CRepeatSector;
class CVector2D;
class CWidgetGta;
class CPedIntelligence;
class CSector;
class CObjectGta;
class CPickup;
class CBuilding;
class CTask;
class CEvent;
class CAnimBlendHierarchy;
class CAnimBlendAssociation;
class CAnimBlendAssocGroup;
class CAnimBlendClumpData;
class CAnimBlendNode;
class CAnimBlendStaticAssociation;
class AnimBlendFrameData;
class CBaseModelInfo;
class CQuaternion;
class CRealTimeShadow;
class CRealTimeShadowManager;
class TextureDatabaseRuntime;
class CClumpModelInfo;
class CVehicleModelInfo;
class CDamageAtomicModelInfo;
class CEventHandler;
class CTaskManager;
class CFileObjectInstance;
class CPlaceable;
struct CWidgetButton;
struct CEntityGTA;
struct CPhysical;
struct CStoredCollPoly;
struct CPedGTA;
struct RpAtomic;
struct RpClump;
struct RpGeometry;
struct RpHAnimHierarchy;
struct RpLight;
struct RpMaterial;
struct RpSkin;
struct CVehicleGTA;
struct RtAnimAnimation;
struct RtAnimInterpolator;
struct RtAnimInterpolatorInfo;
struct RxNodeDefinition;
struct RxPipeline;
struct RxPipelineNode;
struct RsGlobalType;
struct RwBBox;
struct RwCamera;
struct RwMatrixTag;
struct RwFrame;
struct RwObject;
struct RwRaster;
struct RwResEntry;
struct RwRect;
struct RwRGBA;
struct RwRGBAReal;
struct RwSphere;
struct RwStream;
struct RwTexture;
struct RwV2d;
struct RwV3d;
struct RwObjectNameIdAssocation;
struct RpWorld;
struct RtQuat;
struct AnimAssocDefinition;
struct AnimDescriptor;
typedef RwMatrixTag RwMatrix;
typedef float RwReal;
enum OSDeviceForm : int32_t;

namespace GTASAEngineApi {

struct WorldStreamRequest {
    int32 areaCode = 0;
    int32 streamingFlags = 0;
    bool addModels = true;
    bool addLods = true;
    bool addCollisionNeeded = true;
    bool setCollisionRequired = true;
    bool requestCollision = true;
    bool loadCollision = true;
    bool ensureCollision = true;
    bool addIpls = true;
    bool loadIpls = true;
    bool ensureIpls = true;
    bool loadSceneCollision = true;
    bool loadScene = false;
    bool refreshGame = false;
    bool ignorePlayerVehicleCollision = false;
    const char* reason = nullptr;
};

struct WorldStreamStatus {
    bool valid = false;
    bool collisionLoaded = false;
    int operations = 0;
};

struct GroundProbeResult {
    bool hit = false;
    bool entityUsesCollision = false;
    CVector point{};
    CEntityGTA* entity = nullptr;
};

struct TimerGlobals {
    bool* codePause = nullptr;
    uint32_t* frameCounter = nullptr;
    float* gameFps = nullptr;
    bool* userPause = nullptr;
    float* timeScale = nullptr;
    uint32_t* timeInMilliseconds = nullptr;
    bool* skipProcessThisFrame = nullptr;
    float* timeStep = nullptr;
    uint32_t* pppPreviousTimeInMilliseconds = nullptr;
    uint32_t* ppPreviousTimeInMilliseconds = nullptr;
    uint32_t* pPreviousTimeInMilliseconds = nullptr;
    uint32_t* previousTimeInMilliseconds = nullptr;
    uint32_t* timeInMillisecondsNonClipped = nullptr;
    uint32_t* previousTimeInMillisecondsNonClipped = nullptr;
};

enum class NativeRenderEffectStage : uint8_t {
    Stage0 = 0,
    Stage1,
    Stage2,
    Stage3,
    Stage4,
    Stage5,
    Stage6,
    Stage7,
    Stage8,
    Stage9,
};

CCamera& Camera();
CCam& ActiveCamera();
float* AspectRatio();
float* PlayerStats();
uint8_t* CurrentPlayerIndex();
CPickup* PickupsArrayNative();
void PickupGetRidOfObjects(CPickup* pickup);
void PickupGiveUsObject(CPickup* pickup, CObjectGta** object, int32 slotIndex);
void PickupRemove(CPickup* pickup);
void RegisterCoronaTexture(uint32 id,
                           CEntityGTA* attachTo,
                           uint8 red,
                           uint8 green,
                           uint8 blue,
                           uint8 alpha,
                           const CVector& posn,
                           float radius,
                           float farClip,
                           RwTexture* texture,
                           uint8 flareType,
                           bool enableReflection,
                           bool checkObstacles,
                           int32 paramNotUsed,
                           float angle,
                           bool longDistance,
                           float nearClip,
                           uint8 fadeState,
                           float fadeSpeed,
                           bool onlyFromBelow,
                           bool reflectionDelay);
void RegisterCoronaType(uint32 id,
                        CEntityGTA* attachTo,
                        uint8 red,
                        uint8 green,
                        uint8 blue,
                        uint8 alpha,
                        const CVector* posn,
                        float radius,
                        float farClip,
                        int32 coronaType,
                        uint8 flareType,
                        bool enableReflection,
                        bool checkObstacles,
                        int32 paramNotUsed,
                        float angle,
                        bool longDistance,
                        float nearClip,
                        uint8 fadeState,
                        float fadeSpeed,
                        bool onlyFromBelow,
                        bool reflectionDelay);
void BindCoronasGlobals(float* sunScreenX, float* sunScreenY, bool* sunBlockedByClouds, uint32* moonSize);

void InitCamera(CCamera* camera);
void SetRwCamera(CCamera* camera, RwCamera* rwCamera);
void TakeCameraControl(CCamera* camera,
                       CEntityGTA* target,
                       eCamMode modeToGoTo,
                       eSwitchType switchType,
                       int32 whoIsInControlOfTheCamera);
float CalculateCameraGroundHeight(CCamera* camera, eGroundHeightType type);
void RestoreCameraWithJumpCut(CCamera* camera);
void ProcessCamera(CCamera* camera);

void InitialiseMobileSettings();
void InitialiseLocalisation();
void InitialisePad();
void SizeCamera(RwCamera* camera, RwRect* rect, RwReal viewWindow, RwReal aspectRatio);
void DestroyCamera(RwCamera* camera);
void CreateLights(RpWorld* world);
void InitialiseHud();
void InitialisePlayerSkin();
void InitialisePostEffects();
void ApplyMaxStatsPatch();
void DisableCameraClearPlayerWeaponModePatch();
void PatchFrameLimiter(uint8_t fps);
void RevealWholeNativeMap();
void PatchPlayerPedConstructorTaskFix();
void SetClockTime(int hour, int minute);
void GetClockTime(int* hour, int* minute);
void SetWeatherNow(int weatherId);
void ForceWeatherNow(int weatherId);
bool SetGameClockMilliseconds(uint32_t milliseconds);
bool SetGravity(float gravity);
bool IsGamePaused();
void DrawRadarArea(float* position, uint32_t color, uint32_t unknown);
void RemoveStreamingModel(int modelId);
void DisableWeaponLockOnTarget();
bool DisableEnterExits();
void DisableCjWalkPatch();
void SetNativeHudVisible(bool visible);
void SetNativeHudDisabled(bool disabled);
void AddBigMessage(uint16_t* text, int time, int style);
void BindGameGlobals(int32_t* currentArea, RwMatrix** workingMatrix1, RwMatrix** workingMatrix2);
int GetMobileEffectSettingNative();
uint32_t GetSkinBoneUniformCountNative();
bool IsSpecularLightingDisabledNative();
bool UsesHighSpecularExponentNative();

void UpdatePads();
void ClearTouchInterface();
void UpdateHid();
void ProcessNativeInputFrame(bool updatePads, bool clearTouchInterface, bool updateHid);
uintptr_t* MobileMenu();
bool IsNativeFrontEndMenuActive();
bool IsNativeMobileMenuActive();
void ForceCloseNativeMobileMenu();
void UpdateMobileMenu();
void SetWindModifiersCount(int32_t count);
void InitSprite2dPerFrame();
void InitFontPerFrame();
void UpdateWeather();
void ProcessScripts();
void UpdateTrains();
void UpdateSkidmarks();
void UpdateGlass();
void UpdateFireManager();
void UpdatePopulation(bool generatePeds);
void UpdateWeapons();
void UpdateMovingThings();
void UpdateWaterCannons();
void ProcessWorld();
void UpdateGarages();
void UpdateStuntJumps();
void UpdateBirds();
void UpdateSpecialFx();
void UpdatePostEffects();
void UpdateTimeCycle();
void UpdateGameLogic();
void DoSunAndMoon();
void UpdateCoronas();
void RenderCoronas();
void UpdatePermanentShadows();
void UpdateCustomBuildingRenderer();
bool InitialiseCustomBuildingRendererNative();
uintptr_t* FxManager();
void UpdateFx(uintptr_t* fxManager, RwCamera* camera, float timeStep);
void RenderFx(uintptr_t* fxManager, RwCamera* camera, bool heatHaze);
uintptr_t* BreakManager();
void UpdateBreakManager(uintptr_t* breakManager, float timeStep);
void PreRenderWater();
void RenderNativeEffectStage(NativeRenderEffectStage stage);
void DrawNativeHud();
bool IsAltRenderTarget();
void FlushAltRenderTarget();
void DrawNativeTouchInterface(bool draw);
void SetEmuGamma(bool enabled);
void DisplayMessages(bool immediate);
void RenderFontBuffer();
uintptr_t FindTextureInNativeTexDictionaries(const char* name, const char* const* texdbs, size_t texdbCount);
char* StorageRootBuffer();
void SetFileManagerDir(const char* path);
uintptr_t ZipFileCreateNative(const char* path);
bool ZipAddStorageNative(uintptr_t zipFile);
void InitialiseFileManagerNative();
void InitialiseMemoryMgrNative();
void SetDrawFov(float fov);
void ResetRqVertexBufferState();
float NativeTimeStep();
float NativeRendererTimeStep();
void SetLegacyStreamingInitMemoryBudget(uint32_t bytes);
void RunNewGameCheck();
void BindSceneGlobal(void* scene);
void BindOcclusionGlobals(void* occluders, void* occluderCount);
void BindBoundingBoxGlobals(void* failedCount);
void BindMatrixGlobals(void* matrixCount);
void BindMatrixLinkListGlobal(void* matrixList);
void BindES2VertexBufferGlobals(void* currentCpuBuffer);
void BindStreamingInfoGlobals(void* arrayBase);
void BindReferencesGlobals(void* emptyList, void* references);
void BindCollisionGlobals(void* colModelCache,
                          void* processLineCrossings,
                          void* collisionInMemory,
                          void* cameraCollideWithVehicles);
void InstallCollisionHooksNative(uintptr_t initHook);
void InstallPlaceableMatrixHooksNative(uintptr_t initMatrixArrayHook,
                                       uintptr_t shutdownMatrixArrayHook,
                                       uintptr_t allocateStaticMatrixHook,
                                       uintptr_t allocateMatrixHook,
                                       uintptr_t freeStaticMatrixHook,
                                       uintptr_t removeMatrixHook);
void BindTimeCycleGlobals(void* currentColours, void* belowHorizonGrey);
void BindRendererGlobals(void* renderOutsideTunnels,
                         void* loadingPriority,
                         void* visibleEntityPtrs,
                         void* visibleEntityCount,
                         void* visibleLodPtrs,
                         void* visibleLodCount,
                         void* visibleSuperLodPtrs,
                         void* visibleSuperLodCount,
                         void* invisibleEntityPtrs,
                         void* invisibleEntityCount);
void PrepareLineRenderNoClipping();
void RenderLineWithClippingNative(float startX,
                                  float startY,
                                  float startZ,
                                  float endX,
                                  float endY,
                                  float endZ,
                                  uint32 startColor,
                                  uint32 endColor);
void ReplaceBuildingWithNewModel(CBuilding* building, int32 modelIndex);
void* MoveNativeMemory(void* memory);
bool IsNativeTaskPtr(CTask* task);
CRealTimeShadow* GetNativeRealTimeShadow(CRealTimeShadowManager* manager, CPhysical* physical);
void* CreateNativeEvent(uint32 size);
void DeleteNativeEvent(void* event);
float GetNativeEventSoundLevel(CEvent* event, const CEntityGTA* entity, CVector& position);
void ProcessScriptThreadNative(void* scriptThread);
void HandleEventsNative(CEventHandler* handler);
CTask* FindActiveTaskByTypeNative(CTaskManager* manager, int32 taskType);
bool StoreStaticShadowNative(uint32 id,
                             uint8 type,
                             RwTexture* texture,
                             const CVector* posn,
                             float frontX,
                             float frontY,
                             float sideX,
                             float sideY,
                             int16 intensity,
                             uint8 red,
                             uint8 green,
                             uint8 blue,
                             float zDistance,
                             float scale,
                             float drawDistance,
                             bool temporaryShadow,
                             float upDistance);
bool ShouldRedirectTextureListingMipCount();
void SetAlphaFuncProc(void* proc);
TextureDatabaseRuntime* TextureDatabaseRuntimeLoadNative(const char* withName,
                                                         bool fullyLoad,
                                                         int32 forcedFormat);
void TextureDatabaseRuntimeRegisterNative(TextureDatabaseRuntime* toRegister);
void TextureDatabaseRuntimeUnregisterNative(TextureDatabaseRuntime* toUnregister);
RwTexture* TextureDatabaseRuntimeGetTextureNative(const char* name);
void TextureDatabaseRuntimeUpdateStreamingNative(float deltaTime, bool flush);
TextureDatabaseRuntime* TextureDatabaseRuntimeGetDatabaseNative(const char* dbName);
RwTexture* TextureDatabaseRuntimeGetRWTextureNative(TextureDatabaseRuntime* database, int32 index);
void TextureDatabaseRuntimeSetAsRenderedNative(TextureDatabaseRuntime* database, uint32 index);
void TextureDatabaseRuntimeLoadFullTextureNative(TextureDatabaseRuntime* database, uint32 index);
void InstallTextureDatabaseRuntimeHooksNative(uintptr_t loadThumbsHook,
                                              void** loadThumbsOriginal,
                                              uintptr_t sortEntriesHook,
                                              void** sortEntriesOriginal);
int32 TextureAnnihilateNative(RwTexture* texture);

void InitialiseFont();
void AsciiToGxtChar(const char* ascii, uint16_t* gxt);
void SetFontScale(float x, float y);
void SetFontColor(uint32_t* color);
void SetFontJustify(uint8_t justify);
void SetFontOrientation(uint8_t orientation);
void SetFontWrapX(float wrapX);
void SetFontCentreSize(float size);
void SetFontRightJustifyWrap(float wrap);
void SetFontBackground(uint8_t background, uint8_t onlyText);
void SetFontBackgroundColor(uint32_t* color);
void SetFontProportional(uint8_t proportional);
void SetFontDropColor(uint32_t* color);
void SetFontDropShadowPosition(uint8_t position);
void PrintGxtString(float x, float y, uint16_t* text);
void SetFontStyle(uint8_t style);
void SetFontEdge(uint8_t edge);

void BindTimerGlobals(const TimerGlobals& globals);
uint8_t* TimerRunning();
void InstallTimerControlHooks(void (*startUserPause)(),
                              void (*endUserPause)(),
                              void (*stop)(),
                              bool (*getIsSlowMotionActive)());
void TimerSuspend();
void TimerResume();
uint32_t GetCyclesPerMillisecond();
uint64_t GetCurrentTimeInCycles();

void CalcScreenCoors(const CVector& world,
                     CVector& screen,
                     float* outWidth = nullptr,
                     float* outHeight = nullptr,
                     bool checkMaxVisible = false,
                     bool checkMinVisible = false);

bool WorldToScreen(const CVector& world,
                   CVector& screen,
                   float* depth = nullptr,
                   float minDepth = 1.0f);

float GetWeaponRadiusOnScreen(CPedGTA* ped);
bool CameraIsTargetingActive();

CSector* Sector(int32 x, int32 y);
CRepeatSector* RepeatSector(int32 x, int32 y);

bool ProcessLineOfSight(const CVector* origin,
                        const CVector* target,
                        CColPoint* outColPoint,
                        CEntityGTA** outEntity,
                        bool buildings,
                        bool vehicles,
                        bool peds,
                        bool objects,
                        bool dummies,
                        bool doSeeThroughCheck,
                        bool doCameraIgnoreCheck,
                        bool doShootThroughCheck);

bool ProcessVerticalLine(const CVector* origin,
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

float FindGroundZForCoord(float x, float y);
bool GetIsLineOfSightClear(const CVector& origin,
                           const CVector& target,
                           bool buildings,
                           bool vehicles,
                           bool peds,
                           bool objects,
                           bool dummies,
                           bool doSeeThroughCheck,
                           bool doCameraIgnoreCheck);

void AddWorldEntity(CEntityGTA* entity);
void RemoveWorldEntity(CEntityGTA* entity);
void InstallWorldProcessPedsAfterPreRenderHook(void (*hook)());

void AddCollisionNeededAtPosn(const CVector& pos);
void SetCollisionRequired(const CVector& pos, int32 areaCode);
void RequestCollision(const CVector& pos, int32 areaCode);
bool HasCollisionLoaded(const CVector& pos, int32 areaCode);
void LoadCollision(const CVector& pos, bool ignorePlayerVehicle);
void EnsureCollisionIsInMemory(const CVector& pos);
int32 FindColSlot(const char* name);
void AddColRef(int32 colSlot);
void RemoveColRef(int32 colSlot);
void IncludeColModelIndex(int32 colSlot, int32 modelId);
void RemoveCol(int32 colSlot);
void LoadCol(int32 colSlot, const char* name);
void LoadAllCollision();
void LoadAllBoundingBoxes();
void InitialiseColStore();
bool ColStoreOnlyBB();

void AddIplsNeededAtPosn(const CVector& pos);
void LoadIpls(const CVector& pos, bool avoidLoadInPlayerVehicleMovingDirection);
void EnsureIplsAreInMemory(const CVector& pos);
void RemoveIpl(int32 iplSlotIndex);

void RemoveBuildingsNotInArea(int32 areaCode);
void BindStreamingGlobals();
void InstallStreamingHooksNative(uintptr_t initImageListHook,
                                 uintptr_t makeSpaceForHook,
                                 uintptr_t deleteAllRwObjectsHook);
void RequestModelNativeArm64(int32 modelId, int32 streamingFlags);
void DeleteAllRwObjectsNative32();
void LoadAllRequestedModelsNativeArm64(bool priorityRequestsOnly);
void ClearModelStreamNotLoadedFlag();
bool ConvertStreamingBufferToObject(uint8* fileBuffer, int32 modelId);
void FinishLoadingLargeStreamingFile(uint8* fileBuffer, int32 modelId);
void RetryStreamingLoadFile(int32 channelIndex);
void DeleteRwObjectsBehindCamera(size_t memoryToCleanInBytes);
bool DeleteLeastUsedEntityRwObject(bool notOnScreen, int32 streamingFlags);
bool HasVehicleUpgradeLoaded(int32 modelId);
char* GetModelCDName(int32 index);
void ForceStreamingEnabled(const char* reason = nullptr);
WorldStreamStatus PrepareWorldAtPoint(const CVector& point, const WorldStreamRequest& request);
GroundProbeResult ProbeGround(const CVector& pos, float upDistance = 80.0f, float downDistance = 140.0f);

CPedGTA* GetPedFromPool(int id);
int GetPedPoolIndex(CPedGTA* ped);
CPhysical* GetObjectFromPool(int id);
uintptr_t GetVehiclePoolIndex(CVehicleGTA* vehicle);
CVehicleGTA* GetVehicleFromPool(int id);
int GetVehicleSubtype(CVehicleGTA* vehicle);
bool IsPedModelInfo(uintptr_t modelInfo);

void ServiceVehicleAudioEntity(uintptr_t audioEntity);
void ProcessVehicleControl(CVehicleGTA* vehicle, uintptr_t nativeProcessOffset);
void AutomobileFix(CVehicleGTA* vehicle);
void AutomobileSetupDamageAfterLoad(CVehicleGTA* vehicle);
void DestroyPlayerPed(CPedGTA* ped);
void ShutdownPed(CPedGTA* ped);
void SetupPlayerPed(int playerNumber);
void DeactivatePlayerPed(int playerNumber);
void ReactivatePlayerPed(int playerNumber);
void ClearSpaceForMissionEntity(const CVector& pos, CEntityGTA* entity);
void SetPlayerPedInitialState(CPedGTA* ped);
void ClearPedWeapons(CPedGTA* ped);
void GivePedWeapon(CPedGTA* ped, int weaponId, int ammo);
void SetPedCurrentWeapon(CPedGTA* ped, int weaponId);
void SetPedCurrentWeaponAfterGive(CPedGTA* ped, int weaponId);
void GetPedBonePosition(CPedGTA* ped, RwV3d* out, uint32_t boneTag, bool calledFromCamera);
void GetPedTransformedBonePosition(CPedGTA* ped, CVector* out, int boneId, bool calledFromCamera);
bool IsRunNamedOrSlideTask(const void* task);
bool IsAnimAssocGroupLoaded(int groupId);
uintptr_t GetWeaponInfo(int weaponId, int skill);
void ApplyPedCrouch(CPedIntelligence* intelligence, uint16_t arg);
void ResetPedCrouch(CPedIntelligence* intelligence);
void TriggerJetpackCheat();
bool WidgetIsTouched(CWidgetGta* widget, CVector2D* out);
OSDeviceForm GetOSDeviceForm();
CWidgetGta** TouchInterfaceWidgets();
uintptr_t RebaseFromGta(uintptr_t address);

void SetSpriteTexture(void* spriteStorage, const char* textureName);
void SetSprite2dTextureNative(void* sprite, const char* textureName);
void DrawSprite2dNative(void* sprite, float x, float y, float width, float height, void* color);
void BindSprite2dGlobals(void* recipNearClip, void* nearScreenZ);
void VisibilityInitialiseNative();
void VisibilitySetRenderWareCameraNative(RwCamera* camera);
int32 VisibilityGetClumpAlphaNative(RpClump* clump);
void VisibilityRenderAlphaAtomicNative(RpAtomic* atomic, int32 alpha);
void VisibilitySetupVehicleVariablesNative(RpClump* clump);
void VisibilityRenderReallyDrawLastObjectsNative();
uint16 VisibilityGetAtomicIdNative(RpAtomic* atomic);
RwRaster* CreateTextureListingRasterNative(void* container, const void* entry);
void SetAtomicModelInfoFlagsNative(void* modelInfo, uint32 flags);
RpClump* GetClumpBoundingSphereNative(RpClump* clump, RwSphere* sphere, bool useLtm);
void BindCustomBuildingDNBalance(void* balanceParam);
RpClump* CreateModelInfoClumpNative(CBaseModelInfo* model);
uint8_t BaseModelInfoGetModelTypeNative(CBaseModelInfo* model);
int32 BaseModelInfoGetAnimFileIndexNative(CBaseModelInfo* model);
void BaseModelInfoDeleteRwObjectNative(CBaseModelInfo* model);
void ClumpModelInfoSetClumpNative(CClumpModelInfo* model, RpClump* clump);
void ClumpModelInfoSetFrameIdsNative(CClumpModelInfo* model, RwObjectNameIdAssocation* data);
void ClumpModelInfoDeleteRwObjectNative(CClumpModelInfo* model);
RwFrame* ClumpModelInfoGetFrameFromNameNative(RpClump* clump, const char* name);
void ConstructVehicleModelInfoNative(CBaseModelInfo* model);
void ConstructPedModelInfoNative(CBaseModelInfo* model);
void ConstructAtomicModelInfoNative(CBaseModelInfo* model);
CDamageAtomicModelInfo* AddDamageAtomicModelNative(int32 index);
void InstallModelInfoHooksNative(void* atomicModelInfoStore,
                                 void* pedModelInfoStore,
                                 void* vehicleModelInfoStore,
                                 void* modelInfoPtrs,
                                 uintptr_t addPedModelHook,
                                 uintptr_t addVehicleModelHook,
                                 uintptr_t addAtomicModelHook);
RwObjectNameIdAssocation* VehicleModelInfoDescriptorNative(int32 vehicleType);
void VehicleModelInfoSetAtomicRenderCallbacksNative(CVehicleModelInfo* model);
void VehicleModelInfoReduceMaterialsNative(CVehicleModelInfo* model);
void VehicleModelInfoPreprocessHierarchyNative(CVehicleModelInfo* model);
void VehicleModelInfoSetRenderPipelinesNative(CVehicleModelInfo* model);
RxPipeline* CreateCustomCarOpenGLObjPipeNative();
void CustomBuildingDNSetFxEnvTextureNative(RpMaterial* material, RwTexture* texture);
RwReal CustomCarEnvMapGetFxEnvShininessNative(RpMaterial* material);
RwTexture* CustomCarEnvMapGetFxEnvTextureNative(RpMaterial* material);
RwReal CustomCarEnvMapGetFxSpecSpecularityNative(RpMaterial* material);
RwTexture* CustomCarEnvMapGetFxSpecTextureNative(RpMaterial* material);
void InstallCustomCarEnvMapPipelineHooksNative(uintptr_t worldSectorInitHook,
                                               void** worldSectorInitOriginal,
                                               uintptr_t atomicInitHook,
                                               void** atomicInitOriginal,
                                               void* objPipeline,
                                               void* envMapPipeMatDataPool,
                                               void* envMapPipeAtmDataPool,
                                               void* specMapPipeMatDataPool);
int32 CustomCarEnvMapPipeInstanceCBNative(void* object,
                                          void* instanceData,
                                          int32 instanceDLandVA,
                                          int32 reinstance);
void CustomCarEnvMapPipeRenderCBNative(RwResEntry* repEntry, void* object, uint8 type, uint32 flags);
void RotateRwMatrix(RwMatrix* matrix, RwV3d* axis, float angle);
void InvertRwMatrix(RwMatrix* out, RwMatrix* in);
int GetTaskTypeFromTask(const void* task);
void SetScissorRect(void* rect);
bool IsPedPointerValid(CPedGTA* ped);
void RenderOneNonRoad(CEntityGTA* entity);
void EntityAdd(CEntityGTA* entity);
void EntityAdd(CEntityGTA* entity, const CRect* rect);
void EntityRemove(CEntityGTA* entity);
void CreateEntityRwObject(CEntityGTA* entity);
void DeleteEntityRwObjectVirtual(CEntityGTA* entity);
void EntityPreRenderVirtual(CEntityGTA* entity);
void StopExtraColour(bool fade);
bool IsBasePlaceable(const CPhysical* physical);
void PlaceableSetPositionVirtual(CPlaceable* placeable, float x, float y, float z, bool resetMatrix);
void PhysicalAdd(CPhysical* physical);
void PhysicalRemove(CPhysical* physical);
void RenderPed(CPedGTA* ped);
void AddVehicleUpgrade(CVehicleGTA* vehicle, int32_t modelId);
void RemoveVehicleUpgrade(CVehicleGTA* vehicle, int32_t upgradeModelIndex);
void BindVehicleSpecialColModel(void* specialColModel);
void InstallPlayerPedHooksNative(uintptr_t reApplyMoveAnimsHook);
void InstallVehicleHooksNative(uintptr_t renderDriverAndPassengersHook,
                               uintptr_t setDriverHook,
                               uintptr_t doTailLightEffectHook,
                               uintptr_t doVehicleLightsHook,
                               void** doVehicleLightsOriginal,
                               uintptr_t getVehicleLightsStatusHook);
RwObject* CreateModelInfoRwObjectNative(CBaseModelInfo* model);

}
