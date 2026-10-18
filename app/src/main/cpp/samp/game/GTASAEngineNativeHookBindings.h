#pragma once

#include <cstddef>
#include <cstdint>

namespace GTASAEngineApi {

struct NativeInlineHook {
    const char* symbol = nullptr;
    uintptr_t hook = 0;
    void** original = nullptr;
};

enum class NativePltSlot : uint8_t {
    RqVertexBufferSelect,
    RqVertexBufferDelete,
    CustomRoadsignRenderAtomic,
    RwTextureDestroy,
    PedUpdatePosition,
    RwFrameAddChild,
    TextureDatabaseRuntimeGetEntry,
    RpMaterialListDeinitialize,
    AnimManagerUncompressAnimation,
    TaskComplexLeaveCarPrimary,
    TaskComplexLeaveCarSecondary,
    FindPlayerSpeed,
    TxdStoreFindCb,
    CameraProcess,
    VehicleModelSetupCommonData,
    VehicleAudioSettings,
    RadarClearBlip,
    PedGetWeaponSkill,
    StartGameScreenOnNewGameCheck,
    RleDecompress,
};

using HudDrawRadarFn = void (*)();
using RadarStreamSectionsFn = void (*)(int, int);

void DisableNativeRadarBlipPatches();
void DisableNativeVehicleAudioProcessingPatches();
void DisableNativeMenuMapLegendPatches();
void InstallRender2dHookNative(uintptr_t hook);
void InstallRQShaderBuildSourceHookNative(uintptr_t hook);
void InstallAndroidTouchEventHookNative(uintptr_t hook, void** original);
void BindMultiTouchPointerArrays(void* pointsArray, void* pointersArray);
void PatchMultiTouchPointerLimits();
void InstallHudColourHooksNative(uintptr_t hudColourHook, uintptr_t radarTraceColourHook);
void InstallHudDrawRadarHook(HudDrawRadarFn hook, HudDrawRadarFn* original);
void InstallRadarCoordBlipHookNative(uintptr_t hook, void** original);
void InstallRadarGangOverlayHookNative(uintptr_t hook, void** original);
void InstallRadarClearBlipHookNative(uintptr_t hook, void** original);
void InstallCrosshairHookNative(uintptr_t hook, void** original);
void InstallInputTypeHookNative(uintptr_t hook);
void InstallRenderEffectsHookNative(uintptr_t hook);
void InstallTextureGetHookNative(uintptr_t hook);
void InstallTextureListingMipCountHookNative(uintptr_t hook);
void InstallRqSetAlphaTestHookNative(uintptr_t hook);
void InstallRqVertexBufferDeleteHookNative(uintptr_t hook);
void InstallRenderPipelineInlineHooksNative(const NativeInlineHook* hooks, size_t hookCount);
void InstallSingleplayerDeadWeightHooksNative(const NativeInlineHook* hooks, size_t hookCount);
void DisableNativePopulationInitialise();
void DisableNativePlayerSkinLoad();
void DisableNativeStoredShadowRenderer();
void DisableAmbientPopulationAndTrafficNative();
void InstallRadarStreamSectionsHook(RadarStreamSectionsFn hook, RadarStreamSectionsFn* original);
void InstallNativePltHook(NativePltSlot slot, uintptr_t hookFunction, uintptr_t* originalFunction);
void InstallNativePltHook(NativePltSlot slot, uintptr_t hookFunction);
void PatchObjectDestructorBridgeGuard();
void PatchRendererFrameTiming();
void InstallWorldLineOfSightHookNative(uintptr_t hook, void** original);
void InstallWorldVerticalLineHookNative(uintptr_t hook, void** original);
void InstallPhysicalCheckCollisionHookNative(uintptr_t hook, void** original);
void InstallCollisionVerticalLineHookNative(uintptr_t hook, void** original);
void InstallPadInputHooksNative(const NativeInlineHook* hooks, size_t hookCount);

}
