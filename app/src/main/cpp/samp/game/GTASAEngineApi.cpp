#include "GTASAEngineApi.h"
#include "GTASAEngineAnimationBindings.h"
#include "GTASAEngineNativeHookBindings.h"
#include "GTASAEnginePedInputBindings.h"
#include "GTASAEnginePhysicalRenderBindings.h"
#include "GTASAEnginePoolBindings.h"
#include "GTASAEngineRwBindings.h"
#include "GTASAEngineStreamingBindings.h"
#include "GTASAEngineTaskPedBindings.h"
#include "GTASAEngineTxdBindings.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "../main.h"
#include "../vendor/armhook/patch.h"
#include "../perf/PerformanceProfiler.h"
#include "Collision/ColPoint.h"
#include "Collision/ColStore.h"
#include "Core/Quaternion.h"
#include "Core/Rect.h"
#include "Entity/CEntityGTA.h"
#include "Enums/OSDeviceForm.h"
#include "IplStore.h"
#include "Models/BaseModelInfo.h"
#include "RW/RenderWare.h"
#include "StoredCollPoly.h"
#include "Streaming.h"
#include "World.h"
#include "game.h"

extern CGame* pGame;

namespace {
constexpr uintptr_t kCalcScreenCoors32 = 0x005C57E8 + 1;
constexpr uintptr_t kCalcScreenCoors64 = 0x6E9DF8;
constexpr uintptr_t kGameCamera32 = 0x00951FA8;
constexpr uintptr_t kGameCamera64 = 0xBBA8D0;
constexpr uintptr_t kCameraInit32 = 0x0046F8C0 + 1;
constexpr uintptr_t kCameraInit64 = 0x55BA30;
constexpr uintptr_t kCameraSetRwCamera32 = 0x003E161C + 1;
constexpr uintptr_t kCameraSetRwCamera64 = 0x4BF318;
constexpr uintptr_t kCameraTakeControl32 = 0x003E1714 + 1;
constexpr uintptr_t kCameraTakeControl64 = 0x4BF474;
constexpr uintptr_t kCameraCalculateGroundHeight32 = 0x3DC5C8 + 1;
constexpr uintptr_t kCameraCalculateGroundHeight64 = 0x4BA958;
constexpr uintptr_t kCameraRestoreWithJumpCut32 = 0x3DB154 + 1;
constexpr uintptr_t kCameraRestoreWithJumpCut64 = 0x4B94B4;
constexpr uintptr_t kCameraProcess32 = 0x003DC7D0 + 1;
constexpr uintptr_t kCameraProcess64 = 0x4BAB78;
constexpr uintptr_t kCameraClearPlayerWeaponModePatch32 = 0x004C5902;
constexpr uintptr_t kCameraClearPlayerWeaponModePatch64 = 0x5C3258;
constexpr uintptr_t kPickupGetRidOfObjects32 = 0x0031D790 + 1;
constexpr uintptr_t kPickupGetRidOfObjects64 = 0x3E4A20;
constexpr uintptr_t kPickupGiveUsObject32 = 0x0031D510 + 1;
constexpr uintptr_t kPickupGiveUsObject64 = 0x3E46D4;
constexpr uintptr_t kPickupRemove32 = 0x002D7DC8 + 1;
constexpr uintptr_t kPickupRemove64 = 0x3E427C;
constexpr uintptr_t kPickupsArray32 = 0x007AFD70;
constexpr uintptr_t kPickupsArray64 = 0x991AB8;
constexpr uintptr_t kAspectRatio32 = 0x00A26A90;
constexpr uintptr_t kAspectRatio64 = 0xCC7F00;
constexpr uintptr_t kPlayerStats32 = 0x9647E4;
constexpr uintptr_t kPlayerStats64 = 0xBD585C;
constexpr uintptr_t kCurrentPlayer32 = 0x96B9C4;
constexpr uintptr_t kCurrentPlayer64 = 0xBDCAE8;
constexpr uintptr_t kTimerCodePause32 = 0x006794BC;
constexpr uintptr_t kTimerCodePause64 = 0x8509A0;
constexpr uintptr_t kTimerFrameCounter32 = 0x0067892C;
constexpr uintptr_t kTimerFrameCounter64 = 0x84F280;
constexpr uintptr_t kTimerGameFps32 = 0x00677674;
constexpr uintptr_t kTimerGameFps64 = 0x84CD28;
constexpr uintptr_t kTimerUserPause32 = 0x006776A8;
constexpr uintptr_t kTimerUserPause64 = 0x84CD90;
constexpr uintptr_t kTimerTimeScale32 = 0x0067689C;
constexpr uintptr_t kTimerTimeScale64 = 0x84B1A8;
constexpr uintptr_t kTimerTimeMs32 = 0x00676FF0;
constexpr uintptr_t kTimerTimeMs64 = 0x84C030;
constexpr uintptr_t kTimerSkipProcessFrame32 = 0x00679248;
constexpr uintptr_t kTimerSkipProcessFrame64 = 0x8504B8;
constexpr uintptr_t kTimerTimeStep32 = 0x0067770C;
constexpr uintptr_t kTimerTimeStep64 = 0x84CE50;
constexpr uintptr_t kTimerPppPreviousTimeMs32 = 0x0067949C;
constexpr uintptr_t kTimerPppPreviousTimeMs64 = 0x850960;
constexpr uintptr_t kTimerPpPreviousTimeMs32 = 0x00677DF0;
constexpr uintptr_t kTimerPpPreviousTimeMs64 = 0x84DC10;
constexpr uintptr_t kTimerPPreviousTimeMs32 = 0x00679D3C;
constexpr uintptr_t kTimerPPreviousTimeMs64 = 0x851A90;
constexpr uintptr_t kTimerPreviousTimeMs32 = 0x006779A8;
constexpr uintptr_t kTimerPreviousTimeMs64 = 0x84D388;
constexpr uintptr_t kTimerTimeMsNonClipped32 = 0x00679DBC;
constexpr uintptr_t kTimerTimeMsNonClipped64 = 0x851B90;
constexpr uintptr_t kTimerPreviousTimeMsNonClipped32 = 0x006775F8;
constexpr uintptr_t kTimerPreviousTimeMsNonClipped64 = 0x84CC30;
constexpr uintptr_t kTimerRunning32 = 0x0096B524;
constexpr uintptr_t kTimerRunning64 = 0xBDC5AC;
constexpr uintptr_t kTimerCyclesPerMillisecond32 = 0x0042100C + 1;
constexpr uintptr_t kTimerCyclesPerMillisecond64 = 0x504858;
constexpr uintptr_t kTimerCurrentTimeInCycles32 = 0x00421040 + 1;
constexpr uintptr_t kTimerCurrentTimeInCycles64 = 0x504888;
constexpr uintptr_t kCameraSize32 = 0x005D32AC + 1;
constexpr uintptr_t kCameraSize64 = 0x6F7F84;
constexpr uintptr_t kCameraDestroy32 = 0x005D33A4 + 1;
constexpr uintptr_t kCameraDestroy64 = 0x6F80C0;
constexpr uintptr_t kLightsCreate32 = 0x0046FC08 + 1;
constexpr uintptr_t kLightsCreate64 = 0x55BDCC;
constexpr uintptr_t kHudInitialise32 = 0x0046FF38 + 1;
constexpr uintptr_t kHudInitialise64 = 0x55C1C8;
constexpr uintptr_t kPlayerSkinInitialise32 = 0x005B1188 + 1;
constexpr uintptr_t kPlayerSkinInitialise64 = 0x6D5970;
constexpr uintptr_t kPostEffectsInitialise32 = 0x005B28D4 + 1;
constexpr uintptr_t kPostEffectsInitialise64 = 0x6D6E30;
constexpr uintptr_t kFrameLimiterByteA32 = 0x005E49E0;
constexpr uintptr_t kFrameLimiterByteB32 = 0x005E492E;
constexpr uintptr_t kFrameLimiterMovW9A64 = 0x70A38C;
constexpr uintptr_t kFrameLimiterMovW8B64 = 0x70A43C;
constexpr uintptr_t kFrameLimiterMovW9C64 = 0x70A458;
constexpr uintptr_t kZonesVisited32 = 0x0098D252;
constexpr uintptr_t kZonesVisited64 = 0xC1BF92;
constexpr uintptr_t kZonesRevealed32 = 0x0098D2B8;
constexpr uintptr_t kZonesRevealed64 = 0xC1BFF8;
constexpr uintptr_t kPlayerPedCtorTaskFix32 = 0x004C36E2;
constexpr uintptr_t kPlayerPedCtorTaskFix64 = 0x5C0BC4;
constexpr uintptr_t kRadarDrawBlipsPatchA32 = 0x0043FE5A;
constexpr uintptr_t kRadarDrawBlipsPatchA64 = 0x52522C;
constexpr uintptr_t kRadarDrawBlipsPatchB32 = 0x004409AE;
constexpr uintptr_t kRadarDrawBlipsPatchB64 = 0x525E14;
constexpr uintptr_t kVehicleAudioProcessingPatchA32 = 0x00553E96;
constexpr uintptr_t kVehicleAudioProcessingPatchA64 = 0x674610;
constexpr uintptr_t kVehicleAudioProcessingPatchB32 = 0x00561AC2;
constexpr uintptr_t kVehicleAudioProcessingPatchB64 = 0x682C1C;
constexpr uintptr_t kVehicleAudioProcessingPatchC32 = 0x0056BED4;
constexpr uintptr_t kVehicleAudioProcessingPatchC64 = 0x68DD0C;
constexpr uintptr_t kMenuMapLegendTextPatch32 = 0x002ABA08;
constexpr uintptr_t kMenuMapLegendTextPatch64 = 0x36A6E8;
constexpr uintptr_t kMenuMapLegendIconPatch32 = 0x002ABA14;
constexpr uintptr_t kMenuMapLegendIconPatch64 = 0x36A6F8;
constexpr uintptr_t kMenuMapLegendAreaNamePatch32 = 0x002AB4A6;
constexpr uintptr_t kMenuMapLegendAreaNamePatch64 = 0x36A190;
constexpr uintptr_t kClockMinute32 = 0x00953143;
constexpr uintptr_t kClockMinute64 = 0xBBBC1B;
constexpr uintptr_t kClockHour32 = 0x00953142;
constexpr uintptr_t kClockHour64 = 0xBBBC1A;
constexpr uintptr_t kWeatherSetNow32 = 0x005CDF88 + 1;
constexpr uintptr_t kWeatherSetNow64 = 0x6F24E8;
constexpr uintptr_t kWeatherOld32 = 0x00A7D136;
constexpr uintptr_t kWeatherOld64 = 0xD216F2;
constexpr uintptr_t kWeatherNew32 = 0x00A7D134;
constexpr uintptr_t kWeatherNew64 = 0xD216F0;
constexpr uintptr_t kGameClockMilliseconds32 = 0x96B4D8;
constexpr uintptr_t kGravity32 = 0x3A0B64;
constexpr uintptr_t kGravity32Ver21 = 0x003FE810;
constexpr uintptr_t kGamePaused32 = 0x96B514;
constexpr uintptr_t kGamePaused64 = 0xBDC594;
constexpr uintptr_t kRadarDrawArea32 = 0x00443C60 + 1;
constexpr uintptr_t kRadarDrawArea64 = 0x528EC4;
constexpr uintptr_t kStreamingRemoveModel32 = 0x2D0128 + 1;
constexpr uintptr_t kStreamingRemoveModel64 = 0x391FF0;
constexpr uintptr_t kEntryExitManager32 = 0x700120;
constexpr uintptr_t kEntryExitManager32Ver21 = 0x007A1E20;
constexpr uintptr_t kCjWalkPatch32 = 0x004C5F6A;
constexpr uintptr_t kCjWalkPatch64 = 0x5C3970;
constexpr uintptr_t kTaskOperatorNew32 = 0x4D6A70;
constexpr uintptr_t kTaskOperatorNew64 = 0x5D7414;
constexpr uintptr_t kEnterCarAsDriverCtor32 = 0x4F6FE0;
constexpr uintptr_t kEnterCarAsDriverCtor64 = 0x6007E0;
constexpr uintptr_t kTaskManagerSetTask32 = 0x53397A;
constexpr uintptr_t kTaskManagerSetTask64 = 0x64E084;
constexpr uintptr_t kPedGetWeaponSkillLegacy = 0x4A55E2 + 1;
constexpr uintptr_t kNewGameCheck32 = 0x002A7270 + 1;
constexpr uintptr_t kNewGameCheck64 = 0x365EA0;
constexpr uintptr_t kHudVisibleFlag32 = 0x819D88;
constexpr uintptr_t kHudVisibleFlag64 = 0x9FF3A8;
constexpr uintptr_t kHudDisableFlag32 = 0x991FD8;
constexpr uintptr_t kHudDisableFlag64 = 0xC20DFC;
constexpr uintptr_t kMessagesAddBig32 = 0x0054C62C + 1;
constexpr uintptr_t kMessagesAddBig64 = 0x66C150;
constexpr uintptr_t kGameCurrentArea32 = 0x00678C38;
constexpr uintptr_t kGameCurrentArea64 = 0x84F8A0;
constexpr uintptr_t kGameWorkingMatrix132 = 0x006796E8;
constexpr uintptr_t kGameWorkingMatrix164 = 0x850DF0;
constexpr uintptr_t kGameWorkingMatrix232 = 0x00677B38;
constexpr uintptr_t kGameWorkingMatrix264 = 0x84D6A0;
constexpr uintptr_t kShaderSkinBoneCount32 = 0x006B8BAC;
constexpr uintptr_t kShaderSkinBoneCount64 = 0x896140;
constexpr uintptr_t kShaderSpecularDisabled32 = 0x006B8BA4;
constexpr uintptr_t kShaderSpecularDisabled64 = 0x896138;
constexpr uintptr_t kShaderHighSpecularExponent32 = 0x006B8BA9;
constexpr uintptr_t kShaderHighSpecularExponent64 = 0x89613D;
constexpr uintptr_t kPadUpdatePads32 = 0x003F8B50 + 1;
constexpr uintptr_t kPadUpdatePads64 = 0x4DB464;
constexpr uintptr_t kTouchInterfaceClear32 = 0x002B03F8 + 1;
constexpr uintptr_t kTouchInterfaceClear64 = 0x36F374;
constexpr uintptr_t kHidUpdate32 = 0x0028C178 + 1;
constexpr uintptr_t kHidUpdate64 = 0x3467BC;
constexpr uintptr_t kFrontEndMenuManager32 = 0x98F0F8;
constexpr uintptr_t kFrontEndMenuManager64 = 0xC1DE40;
constexpr uintptr_t kFrontEndMenuActiveOffset32 = 0x120;
constexpr uintptr_t kFrontEndMenuActiveOffset64 = 0x198;
constexpr uintptr_t kMobileMenuGlobal32 = 0x006E0074;
constexpr uintptr_t kMobileMenuGlobal64 = 0x8BE780;
constexpr uintptr_t kMobileMenuUpdate32 = 0x0029A730 + 1;
constexpr uintptr_t kMobileMenuUpdate64 = 0x356A7C;
constexpr uintptr_t kMobileMenuTopScreenOffset32 = 0x2C;
constexpr uintptr_t kMobileMenuTopScreenOffset64 = 0x30;
constexpr uintptr_t kMobileMenuSwitchOffToGame32 = 0x2A92F0 + 1;
constexpr uintptr_t kMobileMenuSwitchOffToGame64 = 0x3682F0;
constexpr uintptr_t kMobileMenuPopAllScreens32 = 0x29A6A4 + 1;
constexpr uintptr_t kMobileMenuPopAllScreens64 = 0x356A40;
constexpr uintptr_t kWindModifiersNumber32 = 0x00A7D22C;
constexpr uintptr_t kWindModifiersNumber64 = 0xD217F8;
constexpr uintptr_t kSpriteInitPerFrame32 = 0x005C89F8 + 1;
constexpr uintptr_t kSpriteInitPerFrame64 = 0x6ECF00;
constexpr uintptr_t kFontInitPerFrame32 = 0x005A8A74 + 1;
constexpr uintptr_t kFontInitPerFrame64 = 0x6CC898;
constexpr uintptr_t kWeatherUpdate32 = 0x005CC2E8 + 1;
constexpr uintptr_t kWeatherUpdate64 = 0x6F0BD8;
constexpr uintptr_t kScriptsProcess32 = 0x0032AED8 + 1;
constexpr uintptr_t kScriptsProcess64 = 0x3F3AD8;
constexpr uintptr_t kTrainUpdate32 = 0x57D098 + 1;
constexpr uintptr_t kTrainUpdate64 = 0x6A0A14;
constexpr uintptr_t kSkidmarksUpdate32 = 0x005BE838 + 1;
constexpr uintptr_t kSkidmarksUpdate64 = 0x6E2F08;
constexpr uintptr_t kGlassUpdate32 = 0x005AB4C8 + 1;
constexpr uintptr_t kGlassUpdate64 = 0x6D032C;
constexpr uintptr_t kFireManagerGlobal32 = 0x00958800;
constexpr uintptr_t kFireManagerGlobal64 = 0xBC12D8;
constexpr uintptr_t kFireManagerUpdate32 = 0x003F1628 + 1;
constexpr uintptr_t kFireManagerUpdate64 = 0x4D361C;
constexpr uintptr_t kPopulationUpdate32 = 0x004CC380 + 1;
constexpr uintptr_t kWeaponUpdate32 = 0x005DB8E8 + 1;
constexpr uintptr_t kWeaponUpdate64 = 0x700AF4;
constexpr uintptr_t kMovingThingsUpdate32 = 0x005A6720 + 1;
constexpr uintptr_t kMovingThingsUpdate64 = 0x6CA130;
constexpr uintptr_t kWaterCannonsUpdate32 = 0x005CBB20 + 1;
constexpr uintptr_t kWaterCannonsUpdate64 = 0x6F04CC;
constexpr uintptr_t kWorldProcess32 = 0x00427744 + 1;
constexpr uintptr_t kWorldProcess64 = 0x50BE40;
constexpr uintptr_t kGaragesUpdate32 = 0x30E760 + 1;
constexpr uintptr_t kGaragesUpdate64 = 0x3D4134;
constexpr uintptr_t kStuntJumpManagerUpdate32 = 0x3616C4 + 1;
constexpr uintptr_t kStuntJumpManagerUpdate64 = 0x4304D0;
constexpr uintptr_t kBirdsUpdate32 = 0x0059CFC0 + 1;
constexpr uintptr_t kBirdsUpdate64 = 0x6C13F0;
constexpr uintptr_t kSpecialFxUpdate32 = 0x005C03E4 + 1;
constexpr uintptr_t kSpecialFxUpdate64 = 0x6E4A7C;
constexpr uintptr_t kPostEffectsUpdate32 = 0x005B28D8 + 1;
constexpr uintptr_t kPostEffectsUpdate64 = 0x6D6E34;
constexpr uintptr_t kTimeCycleUpdate32 = 0x0041EF78 + 1;
constexpr uintptr_t kTimeCycleUpdate64 = 0x502ADC;
constexpr uintptr_t kGameLogicUpdate32 = 0x307D8C + 1;
constexpr uintptr_t kGameLogicUpdate64 = 0x3CD630;
constexpr uintptr_t kCoronaRegisterTexture32 = 0x005A3AAC + 1;
constexpr uintptr_t kCoronaRegisterTexture64 = 0x6C71D0;
constexpr uintptr_t kCoronaRegisterType32 = 0x005A3A20 + 1;
constexpr uintptr_t kCoronaRegisterType64 = 0x6C7174;
constexpr uintptr_t kCoronaSunScreenX32 = 0x00679A84;
constexpr uintptr_t kCoronaSunScreenX64 = 0x851528;
constexpr uintptr_t kCoronaSunScreenY32 = 0x00679784;
constexpr uintptr_t kCoronaSunScreenY64 = 0x850F20;
constexpr uintptr_t kCoronaSunBlockedByClouds32 = 0x00679F40;
constexpr uintptr_t kCoronaSunBlockedByClouds64 = 0x851E90;
constexpr uintptr_t kCoronaMoonSize32 = 0x00675F6C;
constexpr uintptr_t kCoronaMoonSize64 = 0x849F60;
constexpr uintptr_t kCoronasDoSunAndMoon32 = 0x005A3E40 + 1;
constexpr uintptr_t kCoronasDoSunAndMoon64 = 0x6C75E4;
constexpr uintptr_t kCoronasUpdate32 = 0x005A22C8 + 1;
constexpr uintptr_t kCoronasUpdate64 = 0x6C5BE0;
constexpr uintptr_t kCoronasRender32 = 0x005A23B8 + 1;
constexpr uintptr_t kCoronasRender64 = 0x6C5CD4;
constexpr uintptr_t kShadowsUpdatePermanent32 = 0x005BD370 + 1;
constexpr uintptr_t kShadowsUpdatePermanent64 = 0x6E1BC4;
constexpr uintptr_t kCustomBuildingRendererUpdate32 = 0x002CA3A4 + 1;
constexpr uintptr_t kCustomBuildingRendererUpdate64 = 0x38B6DC;
constexpr uintptr_t kFxManager32 = 0x00820520;
constexpr uintptr_t kFxManager64 = 0xA062A8;
constexpr uintptr_t kFxUpdate32 = 0x00363DE0 + 1;
constexpr uintptr_t kFxUpdate64 = 0x433F48;
constexpr uintptr_t kFxRender32 = 0x00363DF0 + 1;
constexpr uintptr_t kFxRender64 = 0x433F54;
constexpr uintptr_t kBreakManager32 = 0x0099DD14;
constexpr uintptr_t kBreakManager64 = 0xC31CF0;
constexpr uintptr_t kBreakManagerUpdate32 = 0x0045267C + 1;
constexpr uintptr_t kBreakManagerUpdate64 = 0x53AFDC;
constexpr uintptr_t kWaterPreRender32 = 0x00596540 + 1;
constexpr uintptr_t kWaterPreRender64 = 0x6BB3A8;
constexpr uintptr_t kRenderEffectStage0_32 = 0x0059DA40 + 1;
constexpr uintptr_t kRenderEffectStage0_64 = 0x6C1D6C;
constexpr uintptr_t kRenderEffectStage1_32 = 0x005BE914 + 1;
constexpr uintptr_t kRenderEffectStage1_64 = 0x6E2FB4;
constexpr uintptr_t kRenderEffectStage2_32 = 0x005A6BC8 + 1;
constexpr uintptr_t kRenderEffectStage2_64 = 0x6CA5D0;
constexpr uintptr_t kRenderEffectStage3_32 = 0x005CBBAC + 1;
constexpr uintptr_t kRenderEffectStage3_64 = 0x6F054C;
constexpr uintptr_t kRenderEffectStage4_32 = 0x0059BF84 + 1;
constexpr uintptr_t kRenderEffectStage4_64 = 0x6C0268;
constexpr uintptr_t kRenderEffectStage5_32 = 0x005A1C38 + 1;
constexpr uintptr_t kRenderEffectStage5_64 = 0x6C552C;
constexpr uintptr_t kRenderEffectStage6_32 = 0x005E3390 + 1;
constexpr uintptr_t kRenderEffectStage6_64 = 0x708DF0;
constexpr uintptr_t kRenderEffectStage7_32 = 0x005C0B14 + 1;
constexpr uintptr_t kRenderEffectStage7_64 = 0x6E50CC;
constexpr uintptr_t kRenderEffectStage8_32 = 0x005B19D0 + 1;
constexpr uintptr_t kRenderEffectStage8_64 = 0x6D6068;
constexpr uintptr_t kRenderEffectStage9_32 = 0x005B5F78 + 1;
constexpr uintptr_t kRenderEffectStage9_64 = 0x6DA2B8;
constexpr uintptr_t kAltRenderTargetCheck32 = 0x001BB7F4 + 1;
constexpr uintptr_t kAltRenderTargetCheck64 = 0x24EA90;
constexpr uintptr_t kAltRenderTargetFlush32 = 0x001BC20C + 1;
constexpr uintptr_t kAltRenderTargetFlush64 = 0x24F5B8;
constexpr uintptr_t kTouchInterfaceDrawAll32 = 0x002B0BD8 + 1;
constexpr uintptr_t kTouchInterfaceDrawAll64 = 0x36FB00;
constexpr uintptr_t kMultiTouchPointsArray32 = 0x00679E90;
constexpr uintptr_t kMultiTouchPointsArray64 = 0x851D38;
constexpr uintptr_t kMultiTouchPointersArray32 = 0x006D7178;
constexpr uintptr_t kMultiTouchPointersArray64 = 0x8B5028;
constexpr uintptr_t kPointerGetNumberPatch32 = 0x0026B03C;
constexpr uintptr_t kPointerGetNumberPatch64 = 0x320844;
constexpr uintptr_t kPointerGetTypePatch32 = 0x0026B046;
constexpr uintptr_t kPointerGetTypePatch64 = 0x320850;
constexpr uintptr_t kPointerGetCoordinatesPatch32 = 0x002700AC;
constexpr uintptr_t kPointerGetCoordinatesPatch64 = 0x326FEC;
constexpr uintptr_t kPointerGetWheelPatch32 = 0x00270118;
constexpr uintptr_t kPointerGetWheelPatch64 = 0x32706C;
constexpr uintptr_t kPointerDoubleClickedPatch32 = 0x00270164;
constexpr uintptr_t kPointerDoubleClickedPatch64 = 0x3270AC;
constexpr uintptr_t kPointerGetButtonPatch32 = 0x002700F2;
constexpr uintptr_t kPointerGetButtonPatch64 = 0x327040;
constexpr uintptr_t kMessagesDisplay32 = 0x0054BDD4 + 1;
constexpr uintptr_t kMessagesDisplay64 = 0x66B678;
constexpr uintptr_t kFontRenderBuffer32 = 0x005A9120 + 1;
constexpr uintptr_t kFontRenderBuffer64 = 0x6CCEA0;
constexpr uintptr_t kHudDrawRadar32 = 0x00437B0C + 1;
constexpr uintptr_t kHudDrawRadar64 = 0x51CFF0;
constexpr uintptr_t kRadarStreamSections64 = 0x528340;
constexpr uintptr_t kStorageRootBuffer32 = 0x6D687C;
constexpr uintptr_t kStorageRootBuffer64 = 0x8B46A8;
constexpr uintptr_t kFileMgrSetDir32 = 0x003F0C54 + 1;
constexpr uintptr_t kFileMgrSetDir64 = 0x4D293C;
constexpr uintptr_t kFileLoaderLoadLineBuffer32 = 0x003EEFD8 + 1;
constexpr uintptr_t kFileLoaderLoadLineBuffer64 = 0x4D0108;
constexpr uintptr_t kFileLoaderLoadObject32 = 0x00469490 + 1;
constexpr uintptr_t kFileLoaderLoadObject64 = 0x005547BC;
constexpr uintptr_t kFileLoaderLoadObjectInstance32 = 0x003F059C + 1;
constexpr uintptr_t kFileLoaderLoadObjectInstance64 = 0x4D20FC;
constexpr uintptr_t kCdStreamOpen32 = 0x002C9D38 + 1;
constexpr uintptr_t kCdStreamOpen64 = 0x38AF9C;
constexpr uintptr_t kCdStreamSync32 = 0x2C9C3C + 1;
constexpr uintptr_t kCdStreamSync64 = 0x38AE10;
constexpr uintptr_t kCdStreamGetStatus32 = 0x2C9BFC + 1;
constexpr uintptr_t kCdStreamGetStatus64 = 0x38ADA4;
constexpr uintptr_t kCdStreamRead32 = 0x2C9B3C + 1;
constexpr uintptr_t kCdStreamRead64 = 0x38ACB8;
constexpr uintptr_t kCdStreamGetLastPosn32 = 0x2C9C30 + 1;
constexpr uintptr_t kCdStreamGetLastPosn64 = 0x38AE04;
constexpr uintptr_t kPoolColModel32 = 0x0095AC58;
constexpr uintptr_t kPoolColModel64 = 0xBC3BE0;
constexpr uintptr_t kPoolEvent32 = 0x0095AC60;
constexpr uintptr_t kPoolEvent64 = 0xBC3BF0;
constexpr uintptr_t kPoolPointRoute32 = 0x0095AC64;
constexpr uintptr_t kPoolPointRoute64 = 0xBC3BF8;
constexpr uintptr_t kPoolPatrolRoute32 = 0x0095AC68;
constexpr uintptr_t kPoolPatrolRoute64 = 0xBC3C00;
constexpr uintptr_t kPoolNodeRoute32 = 0x0095AC6C;
constexpr uintptr_t kPoolNodeRoute64 = 0xBC3C08;
constexpr uintptr_t kPoolTaskAllocator32 = 0x0095AC70;
constexpr uintptr_t kPoolTaskAllocator64 = 0xBC3C10;
constexpr uintptr_t kPoolPedIntelligence32 = 0x0095AC74;
constexpr uintptr_t kPoolPedIntelligence64 = 0xBC3C18;
constexpr uintptr_t kPoolPedAttractor32 = 0x0095AC78;
constexpr uintptr_t kPoolPedAttractor64 = 0xBC3C20;
constexpr uintptr_t kPoolBuilding32 = 0x00678EC4;
constexpr uintptr_t kPoolBuilding64 = 0x84FDB8;
constexpr uintptr_t kPoolDummy32 = 0x00676B1C;
constexpr uintptr_t kPoolDummy64 = 0x84B698;
constexpr uintptr_t kPoolEntryInfoNode32 = 0x00679C1C;
constexpr uintptr_t kPoolEntryInfoNode64 = 0x851850;
constexpr uintptr_t kPoolPtrNodeSingleLink32 = 0x00677F0C;
constexpr uintptr_t kPoolPtrNodeSingleLink64 = 0x84DE48;
constexpr uintptr_t kPoolPtrNodeDoubleLink32 = 0x00678364;
constexpr uintptr_t kPoolPtrNodeDoubleLink64 = 0x84E6F0;
constexpr uintptr_t kPoolPed32 = 0x00676C84;
constexpr uintptr_t kPoolPed64 = 0x84B968;
constexpr uintptr_t kPoolVehicle32 = 0x00678534;
constexpr uintptr_t kPoolVehicle64 = 0x84EA90;
constexpr uintptr_t kPoolObject32 = 0x00676BB0;
constexpr uintptr_t kPoolObject64 = 0x84B7C0;
constexpr uintptr_t kPoolTask32 = 0x00676158;
constexpr uintptr_t kPoolTask64 = 0x84A330;
constexpr uintptr_t kTxdStoreGetNumRefs32 = 0x5D3E34 + 1;
constexpr uintptr_t kTxdStoreGetNumRefs64 = 0x6F8DF4;
constexpr uintptr_t kTxdStoreRemoveTxd32 = 0x5D40B8 + 1;
constexpr uintptr_t kTxdStoreRemoveTxd64 = 0x6F9130;
constexpr uintptr_t kTxdStoreFindSlot32 = 0x005D3EB0 + 1;
constexpr uintptr_t kTxdStoreFindSlot64 = 0x6F8EA0;
constexpr uintptr_t kTxdStoreAddSlot32 = 0x005D3B84 + 1;
constexpr uintptr_t kTxdStoreAddSlot64 = 0x6F8A68;
constexpr uintptr_t kTxdStoreInitialise32 = 0x005D3A90 + 1;
constexpr uintptr_t kTxdStoreInitialise64 = 0x6F8928;
constexpr uintptr_t kTxdStorePushCurrent32 = 0x005D41D4 + 1;
constexpr uintptr_t kTxdStorePushCurrent64 = 0x6F92A8;
constexpr uintptr_t kTxdStorePopCurrent32 = 0x005D4214 + 1;
constexpr uintptr_t kTxdStorePopCurrent64 = 0x6F92D4;
constexpr uintptr_t kTxdStoreSetCurrent32 = 0x5D4144 + 1;
constexpr uintptr_t kTxdStoreSetCurrent64 = 0x6F91EC;
constexpr uintptr_t kDrawFov32 = 0x006B1CB8;
constexpr uintptr_t kDrawFov64 = 0x88E6BC;
constexpr uintptr_t kSceneGlobal32 = 0x678954;
constexpr uintptr_t kSceneGlobal64 = 0x84F2D0;
constexpr uintptr_t kOcclusionArray32 = 0xA41140;
constexpr uintptr_t kOcclusionArray64 = 0xCE3EE8;
constexpr uintptr_t kOcclusionCount32 = 0xA45790;
constexpr uintptr_t kOcclusionCount64 = 0xCE8538;
constexpr uintptr_t kBoundingBoxFailedCount32 = 0x00678A9C;
constexpr uintptr_t kBoundingBoxFailedCount64 = 0x84F568;
constexpr uintptr_t kMatrixCount32 = 0x00677C70;
constexpr uintptr_t kMatrixCount64 = 0x84D910;
constexpr uintptr_t kMatrixLinkList32 = 0x006776AC;
constexpr uintptr_t kMatrixLinkList64 = 0x84CD98;
constexpr uintptr_t kES2VertexBufferCurrentCpu32 = 0x006777E0;
constexpr uintptr_t kES2VertexBufferCurrentCpu64 = 0x84CFF8;
constexpr uintptr_t kStreamingInfoArrayBase32 = 0x00678578;
constexpr uintptr_t kStreamingInfoArrayBase64 = 0x84EB18;
constexpr uintptr_t kReferencesEmptyList32 = 0x676DC0;
constexpr uintptr_t kReferencesEmptyList64 = 0x84BBE0;
constexpr uintptr_t kReferencesArray32 = 0x67901C;
constexpr uintptr_t kReferencesArray64 = 0x850068;
constexpr uintptr_t kCollisionColModelCache32 = 0x00677A88;
constexpr uintptr_t kCollisionColModelCache64 = 0x84D540;
constexpr uintptr_t kCollisionProcessLineCrossings32 = 0x006783D0;
constexpr uintptr_t kCollisionProcessLineCrossings64 = 0x84E7C8;
constexpr uintptr_t kCollisionInMemory32 = 0x006773BC;
constexpr uintptr_t kCollisionInMemory64 = 0x84C7C0;
constexpr uintptr_t kCollisionCameraCollideVehicles32 = 0x00678D74;
constexpr uintptr_t kCollisionCameraCollideVehicles64 = 0x84FB18;
constexpr uintptr_t kTimeCycleCurrentColours32 = 0x00676BB8;
constexpr uintptr_t kTimeCycleCurrentColours64 = 0x84B7D0;
constexpr uintptr_t kTimeCycleBelowHorizonGrey32 = 0x006770C0;
constexpr uintptr_t kTimeCycleBelowHorizonGrey64 = 0x84C1D0;
constexpr uintptr_t kRendererOutsideTunnels32 = 0x6764D0;
constexpr uintptr_t kRendererOutsideTunnels64 = 0x84AA10;
constexpr uintptr_t kRendererLoadingPriority32 = 0x67914C;
constexpr uintptr_t kRendererLoadingPriority64 = 0x8502C8;
constexpr uintptr_t kRendererVisibleEntityPtrs32 = 0x6778EC;
constexpr uintptr_t kRendererVisibleEntityPtrs64 = 0x84D210;
constexpr uintptr_t kRendererVisibleEntityCount32 = 0x6771F0;
constexpr uintptr_t kRendererVisibleEntityCount64 = 0x84C428;
constexpr uintptr_t kRendererVisibleLodPtrs64 = 0x84A958;
constexpr uintptr_t kRendererVisibleLodCount64 = 0x84A4A0;
constexpr uintptr_t kRendererVisibleSuperLodPtrs64 = 0x84B6A0;
constexpr uintptr_t kRendererVisibleSuperLodCount64 = 0x84EB20;
constexpr uintptr_t kRendererInvisibleEntityPtrs64 = 0x84F738;
constexpr uintptr_t kRendererInvisibleEntityCount64 = 0x850790;
constexpr uintptr_t kLineRenderNoClipPrepare32 = 0x3FCAF0 + 1;
constexpr uintptr_t kLineRenderNoClipPrepare64 = 0x4E0224;
constexpr uintptr_t kLineRenderWithClippingLegacy32 = 0x5ADBD8 + 1;
constexpr uintptr_t kBuildingReplaceWithNewModel32 = 0x00280198 + 1;
constexpr uintptr_t kBuildingReplaceWithNewModel64 = 0x33ABCC;
constexpr uintptr_t kMemoryMove32 = 0x005D3032 + 1;
constexpr uintptr_t kMemoryMove64 = 0x6F7C90;
constexpr uintptr_t kTaskIsTaskPtr32 = 0x004D69F0 + 1;
constexpr uintptr_t kTaskIsTaskPtr64 = 0x5D7350;
constexpr uintptr_t kRealTimeShadowGet32 = 0x5B87AC + 1;
constexpr uintptr_t kRealTimeShadowGet64 = 0x6DD0C4;
constexpr uintptr_t kEventNew32 = 0x0036FB60 + 1;
constexpr uintptr_t kEventNew64 = 0x441324;
constexpr uintptr_t kEventDelete32 = 0x0036FBC4 + 1;
constexpr uintptr_t kEventDelete64 = 0x4413A8;
constexpr uintptr_t kEventGetSoundLevel32 = 0x0036FC18 + 1;
constexpr uintptr_t kEventGetSoundLevel64 = 0x441438;
constexpr uintptr_t kScriptThreadProcess32 = 0x0032B708 + 1;
constexpr uintptr_t kScriptThreadProcess64 = 0x3F445C;
constexpr uintptr_t kEventHandlerHandleEvents32 = 0x0037B818 + 1;
constexpr uintptr_t kEventHandlerHandleEvents64 = 0x45084C;
constexpr uintptr_t kTaskManagerFindActiveTaskByType32 = 0x00533B20 + 1;
constexpr uintptr_t kTaskManagerFindActiveTaskByType64 = 0x64E348;
constexpr uintptr_t kShadowsStoreStatic32 = 0x005B8D38 + 1;
constexpr uintptr_t kShadowsStoreStatic64 = 0x6DD6A4;
constexpr uintptr_t kTextureListingMipRedirectFlag32 = 0x6B8B9C;
constexpr uintptr_t kTextureListingMipRedirectFlag64 = 0x896135;
constexpr uintptr_t kAlphaFuncProcSlot32 = 0x6BCBF8;
constexpr uintptr_t kAlphaFuncProcSlot64 = 0x89A1B0;
constexpr uintptr_t kRqVertexBufferState32 = 0x6B8AF0;
constexpr uintptr_t kNativeTimeStep32 = 0x96B500;
constexpr uintptr_t kNativeRendererTimeStep32 = 0x4DD9E8;
constexpr uintptr_t kLegacyStreamingInitBudget32 = 0x00685FA0;
constexpr uintptr_t kObjectDestructorBridgePatch64 = 0x53BF1C;
constexpr uintptr_t kRendererFrameTimingPatch64A = 0x5DF790;
constexpr uintptr_t kRendererFrameTimingPatch64B = 0x5DF794;
constexpr uintptr_t kApplyCollision64 = 0x4E5598;
constexpr uintptr_t kApplyFriction64 = 0x4E1A50;
constexpr uintptr_t kClumpModelInfoGetFrameFromId32 = 0x00335CC0 + 1;
constexpr uintptr_t kClumpModelInfoGetFrameFromId32Ver21 = 0x003856D0 + 1;
constexpr uintptr_t kStreamingMemoryAvailableNative64 = 0x85EBD8;
constexpr uintptr_t kRenderRoadEntity64 = 0x4F501C;
constexpr uintptr_t kRenderNonRoadEntity64 = 0x4F56E0;
constexpr uintptr_t kRendererNativeVisible64 = 0xBCF8E4;
constexpr uintptr_t kRendererNativeLods64 = 0xBCF8E8;
constexpr uintptr_t kRendererNativeInvisible64 = 0xBCF8EC;
constexpr uintptr_t kRendererNativeSuperLods64 = 0xBCF8F0;
constexpr uintptr_t kRendererGotVisibleList64 = 0x84D210;
constexpr uintptr_t kRendererGotVisibleCount64 = 0x84C428;
constexpr uintptr_t kRendererGotLodList64 = 0x84A958;
constexpr uintptr_t kRendererGotLodCount64 = 0x84A4A0;
constexpr uintptr_t kWorldCurrentScanCode64 = 0x00C18350;
constexpr uintptr_t kWorldIgnoreEntity64 = 0x00BDCAF8;
constexpr uintptr_t kBikeSetupModelNodes64 = 0x681810;
constexpr uintptr_t kPltRqVertexBufferSelect = 0x677498;
constexpr uintptr_t kPltRqVertexBufferDelete = 0x679B14;
constexpr uintptr_t kPltCustomRoadsignRenderAtomic = 0x66F5AC;
constexpr uintptr_t kPltRwTextureDestroy = 0x67332C;
constexpr uintptr_t kPltPedUpdatePosition = 0x671458;
constexpr uintptr_t kPltRwFrameAddChild = 0x675490;
constexpr uintptr_t kPltTextureDatabaseRuntimeGetEntry = 0x672D14;
constexpr uintptr_t kPltRpMaterialListDeinitialize = 0x6730F0;
constexpr uintptr_t kPltAnimManagerUncompressAnimation = 0x6750D4;
constexpr uintptr_t kPltTaskComplexLeaveCarPrimary = 0x671984;
constexpr uintptr_t kPltTaskComplexLeaveCarSecondary = 0x675320;
constexpr uintptr_t kPltFindPlayerSpeed = 0x671BBC;
constexpr uintptr_t kPltTxdStoreFindCb = 0x676034;
constexpr uintptr_t kPltCameraProcess = 0x6717BC;
constexpr uintptr_t kPltVehicleModelSetupCommonData = 0x674280;
constexpr uintptr_t kPltVehicleAudioSettings = 0x06D008;
constexpr uintptr_t kPltRadarClearBlip = 0x66FF0C;
constexpr uintptr_t kPltPedGetWeaponSkill = 0x6749D0;
constexpr uintptr_t kPltStartGameScreenOnNewGameCheck32 = 0x6785FC;
constexpr uintptr_t kPltStartGameScreenOnNewGameCheck64 = 0x84EC20;
constexpr uintptr_t kPltRleDecompress32 = 0x6701D4;
constexpr uintptr_t kPltRleDecompress64 = 0x840708;
constexpr uintptr_t kTextureDatabaseGetDatabase32 = 0x1EAC8C + 1;
constexpr uintptr_t kTextureDatabaseRegister32 = 0x1E9BC8 + 1;
constexpr uintptr_t kTextureDatabaseGetTexture32 = 0x1E9C64 + 1;
constexpr uintptr_t kTextureDatabaseUnregister32 = 0x1E9C80 + 1;
constexpr uintptr_t kTextureDatabaseRegisteredCount32 = 0x6BD174 + 4;
constexpr uintptr_t kTextureDatabaseRegisteredList32 = 0x6BD174 + 8;
constexpr uintptr_t kTextureDatabaseRuntimeLoad32 = 0x001EA864 + 1;
constexpr uintptr_t kTextureDatabaseRuntimeLoad64 = 0x28771C;
constexpr uintptr_t kTextureDatabaseRuntimeRegister32 = 0x1E9B48 + 1;
constexpr uintptr_t kTextureDatabaseRuntimeRegister64 = 0x2865D8;
constexpr uintptr_t kTextureDatabaseRuntimeUnregister32 = 0x1E9C00 + 1;
constexpr uintptr_t kTextureDatabaseRuntimeUnregister64 = 0x2866A4;
constexpr uintptr_t kTextureDatabaseRuntimeGetTexture32 = 0x001E9C64 + 1;
constexpr uintptr_t kTextureDatabaseRuntimeGetTexture64 = 0x286718;
constexpr uintptr_t kTextureDatabaseRuntimeUpdateStreaming32 = 0x1E9518 + 1;
constexpr uintptr_t kTextureDatabaseRuntimeUpdateStreaming64 = 0x285DBC;
constexpr uintptr_t kTextureDatabaseRuntimeGetDatabase32 = 0x1EAC0C + 1;
constexpr uintptr_t kTextureDatabaseRuntimeGetDatabase64 = 0x287AF4;
constexpr uintptr_t kTextureDatabaseRuntimeGetRWTexture32 = 0x001E9110 + 1;
constexpr uintptr_t kTextureDatabaseRuntimeGetRWTexture64 = 0x285710;
constexpr uintptr_t kTextureDatabaseRuntimeSetAsRendered32 = 0x001E9A14 + 1;
constexpr uintptr_t kTextureDatabaseRuntimeSetAsRendered64 = 0x286360;
constexpr uintptr_t kTextureDatabaseRuntimeLoadFullTexture32 = 0x001E91C8 + 1;
constexpr uintptr_t kTextureDatabaseRuntimeLoadFullTexture64 = 0x28582C;
constexpr uintptr_t kTextureDatabaseLoadThumbs64 = 0x284A9C;
constexpr uintptr_t kTextureDatabaseRuntimeSortEntries64 = 0x286CF8;
constexpr uintptr_t kTextureAnnihilate32 = 0x1DB33C + 1;
constexpr uintptr_t kTextureAnnihilate64 = 0x273A1C;
constexpr uintptr_t kRwTexDictionaryGetCurrent32 = 0x1DBA64 + 1;
constexpr uintptr_t kRwTexDictionaryFindNamedTexture32 = 0x1DB9B0 + 1;
constexpr uintptr_t kTxdStoreGetTxdParent32 = 0x5D428C + 1;
constexpr uintptr_t kFontInitialise32 = 0x0046FD8C + 1;
constexpr uintptr_t kFontInitialise64 = 0x55BFEC;
constexpr uintptr_t kFontAsciiToGxt32 = 0x005A83C0 + 1;
constexpr uintptr_t kFontAsciiToGxt64 = 0x6CBEF0;
constexpr uintptr_t kFontScaleX32 = 0xA297B8;
constexpr uintptr_t kFontScaleX64 = 0xCCC324 + 0x4;
constexpr uintptr_t kFontScaleY32 = 0xA297BC;
constexpr uintptr_t kFontScaleY64 = 0xCCC324 + 0x8;
constexpr uintptr_t kFontSetColor32 = 0x005AB038 + 1;
constexpr uintptr_t kFontSetColor64 = 0x6CEE0C;
constexpr uintptr_t kFontSetJustify32 = 0x005AB364 + 1;
constexpr uintptr_t kFontSetJustify64 = 0x6CF1CC;
constexpr uintptr_t kFontSetOrientation32 = 0x005AB374 + 1;
constexpr uintptr_t kFontSetOrientation64 = 0x6CF1DC;
constexpr uintptr_t kFontSetWrapX32 = 0x005AB248 + 1;
constexpr uintptr_t kFontSetWrapX64 = 0x6CF0B0;
constexpr uintptr_t kFontSetCentreSize32 = 0x005AB258 + 1;
constexpr uintptr_t kFontSetCentreSize64 = 0x6CF0C0;
constexpr uintptr_t kFontSetRightJustifyWrap32 = 0x05AB268 + 1;
constexpr uintptr_t kFontSetRightJustifyWrap64 = 0x0053384C;
constexpr uintptr_t kFontSetBackground32 = 0x005AB330 + 1;
constexpr uintptr_t kFontSetBackground64 = 0x6CF18C;
constexpr uintptr_t kFontSetBackgroundColor32 = 0x005AB340 + 1;
constexpr uintptr_t kFontSetBackgroundColor64 = 0x6CF1A0;
constexpr uintptr_t kFontSetProportional32 = 0x005AB320 + 1;
constexpr uintptr_t kFontSetProportional64 = 0x6CF17C;
constexpr uintptr_t kFontSetDropColor32 = 0x005AB320 + 1;
constexpr uintptr_t kFontSetDropColor64 = 0x6CF0F0;
constexpr uintptr_t kFontSetDropShadowPosition32 = 0x005A8ADC + 1;
constexpr uintptr_t kFontSetDropShadowPosition64 = 0x6CC8E8;
constexpr uintptr_t kFontPrintString32 = 0x005AA200 + 1;
constexpr uintptr_t kFontPrintString64 = 0x6CDEB0;
constexpr uintptr_t kFontSetStyle32 = 0x005AB1BC + 1;
constexpr uintptr_t kFontSetStyle64 = 0x6CEFF8;
constexpr uintptr_t kFontSetEdge32 = 0x005AB2EC + 1;
constexpr uintptr_t kFontSetEdge64 = 0x6CF148;
constexpr uintptr_t kGetWeaponRadius32 = 0x004C69E8 + 1;
constexpr uintptr_t kGetWeaponRadius64 = 0x5C4528;
constexpr uintptr_t kCameraIsTargetingActive32 = 0x3D9F54 + 1;
constexpr uintptr_t kCameraIsTargetingActive64 = 0x4B8154;
constexpr uintptr_t kWorldSectors32 = 0x96B9F4;
constexpr uintptr_t kWorldSectors64 = 0xBDCB20;
constexpr uintptr_t kWorldRepeatSectors32 = 0x987BF4;
constexpr uintptr_t kWorldRepeatSectors64 = 0xC14F20;
constexpr uintptr_t kProcessLineOfSight32 = 0x424B94 + 1;
constexpr uintptr_t kProcessLineOfSight64 = 0x508C7C;
constexpr uintptr_t kProcessVerticalLine32 = 0x4261E0 + 1;
constexpr uintptr_t kProcessVerticalLine64 = 0x50A700;
constexpr uintptr_t kFindGroundZForCoord32 = 0x0042A7C4 + 1;
constexpr uintptr_t kFindGroundZForCoord64 = 0x50F3A0;
constexpr uintptr_t kGetIsLineOfSightClear32 = 0x00423468 + 1;
constexpr uintptr_t kGetIsLineOfSightClear64 = 0x5075A4;
constexpr uintptr_t kWorldAdd32 = 0x00423418 + 1;
constexpr uintptr_t kWorldAdd64 = 0x507518;
constexpr uintptr_t kWorldRemove32 = 0x0042330C + 1;
constexpr uintptr_t kWorldRemove64 = 0x5073A0;
constexpr uintptr_t kWorldProcessPedsAfterPreRenderPlt32 = 0x675C58;
constexpr uintptr_t kWorldProcessPedsAfterPreRenderPlt64 = 0x849A20;
constexpr uintptr_t kColRequestCollision32 = 0x2E2E50 + 1;
constexpr uintptr_t kColRequestCollision64 = 0x3A60E0;
constexpr uintptr_t kColFindSlot32 = 0x2E261C + 1;
constexpr uintptr_t kColFindSlot64 = 0x3A5734;
constexpr uintptr_t kColAddRef32 = 0x2E26C0 + 1;
constexpr uintptr_t kColAddRef64 = 0x3A5824;
constexpr uintptr_t kColRemoveRef32 = 0x2E26DC + 1;
constexpr uintptr_t kColRemoveRef64 = 0x3A5850;
constexpr uintptr_t kColSetRequired32 = 0x2E28A8 + 1;
constexpr uintptr_t kColSetRequired64 = 0x3A5A54;
constexpr uintptr_t kColHasLoaded32 = 0x2E2F90 + 1;
constexpr uintptr_t kColHasLoaded64 = 0x3A625C;
constexpr uintptr_t kColIncludeModel32 = 0x2E2718 + 1;
constexpr uintptr_t kColIncludeModel64 = 0x3A58B0;
constexpr uintptr_t kColRemove32 = 0x002E25A0 + 1;
constexpr uintptr_t kColRemove64 = 0x3A5688;
constexpr uintptr_t kColLoadCol32 = 0x2E2624 + 1;
constexpr uintptr_t kColLoadCol64 = 0x3A573C;
constexpr uintptr_t kColAddNeeded32 = 0x2E2750 + 1;
constexpr uintptr_t kColAddNeeded64 = 0x3A5904;
constexpr uintptr_t kColLoadCollision32 = 0x2E291C + 1;
constexpr uintptr_t kColLoadCollision64 = 0x3A5AFC;
constexpr uintptr_t kColLoadAllCollision32 = 0x2E319C + 1;
constexpr uintptr_t kColLoadAllCollision64 = 0x3A6504;
constexpr uintptr_t kColLoadAllBoundingBoxes32 = 0x2E3140 + 1;
constexpr uintptr_t kColLoadAllBoundingBoxes64 = 0x3A6480;
constexpr uintptr_t kColEnsureInMemory32 = 0x2E2C50 + 1;
constexpr uintptr_t kColEnsureInMemory64 = 0x3A5EA4;
constexpr uintptr_t kColInitialise32 = 0x2E20B0 + 1;
constexpr uintptr_t kColInitialise64 = 0x3A50C8;
constexpr uintptr_t kColPool32 = 0x00796108;
constexpr uintptr_t kColPool64 = 0x00976090;
constexpr uintptr_t kColOnlyBB32 = 0x0079610C;
constexpr uintptr_t kColOnlyBB64 = 0x00976098;
constexpr uintptr_t kIplLoad32 = 0x28195C + 1;
constexpr uintptr_t kIplLoad64 = 0x33CA20;
constexpr uintptr_t kIplEnsure32 = 0x281BB8 + 1;
constexpr uintptr_t kIplEnsure64 = 0x33CD08;
constexpr uintptr_t kIplAddNeeded32 = 0x2811F4 + 1;
constexpr uintptr_t kIplAddNeeded64 = 0x33C140;
constexpr uintptr_t kIplRemove32 = 0x280758 + 1;
constexpr uintptr_t kIplRemove64 = 0x33B320;
constexpr uintptr_t kStreamingAddLods32 = 0x2D0C00 + 1;
constexpr uintptr_t kStreamingAddLods64 = 0x392B6C;
constexpr uintptr_t kStreamingAddModels32 = 0x2D0900 + 1;
constexpr uintptr_t kStreamingAddModels64 = 0x392874;
constexpr uintptr_t kStreamingLoadRequested32 = 0x2D2104 + 1;
constexpr uintptr_t kStreamingLoadRequested64 = 0x394008;
constexpr uintptr_t kStreamingLoadScene32 = 0x2D4A00 + 1;
constexpr uintptr_t kStreamingLoadScene64 = 0x396EEC;
constexpr uintptr_t kStreamingLoadSceneCollision32 = 0x2D5068 + 1;
constexpr uintptr_t kStreamingLoadSceneCollision64 = 0x39769C;
constexpr uintptr_t kStreamingFlushChannels32 = 0x2D4878 + 1;
constexpr uintptr_t kStreamingFlushChannels64 = 0x396D1C;
constexpr uintptr_t kStreamingSetModelDeletable32 = 0x2D6788 + 1;
constexpr uintptr_t kStreamingSetModelDeletable64 = 0x399090;
constexpr uintptr_t kStreamingRemoveBuildingsNotInArea32 = 0x002D5328 + 1;
constexpr uintptr_t kStreamingRemoveBuildingsNotInArea64 = 0x3979AC;
constexpr uintptr_t kStreamingRequestModel64 = 0x3949E0;
constexpr uintptr_t kStreamingDeleteAllRwObjects32 = 0x2D3E98 + 1;
constexpr uintptr_t kStreamingLoadAllRequested64 = 0x396B28;
constexpr uintptr_t kStreamingModelStreamNotLoaded32 = 0x792FBC;
constexpr uintptr_t kStreamingModelStreamNotLoaded64 = 0x972E80;
constexpr uintptr_t kStreamingConvertBufferToObject32 = 0x2D2FD0 + 1;
constexpr uintptr_t kStreamingConvertBufferToObject64 = 0x395114;
constexpr uintptr_t kStreamingFinishLargeFile32 = 0x2D36B0 + 1;
constexpr uintptr_t kStreamingFinishLargeFile64 = 0x395948;
constexpr uintptr_t kStreamingRetryLoadFile32 = 0x2D2314 + 1;
constexpr uintptr_t kStreamingRetryLoadFile64 = 0x394220;
constexpr uintptr_t kStreamingDeleteRwBehindCamera32 = 0x002D5CD8 + 1;
constexpr uintptr_t kStreamingDeleteRwBehindCamera64 = 0x3984F0;
constexpr uintptr_t kStreamingDeleteLeastUsedEntity32 = 0x2D5A80 + 1;
constexpr uintptr_t kStreamingDeleteLeastUsedEntity64 = 0x39825C;
constexpr uintptr_t kStreamingHasVehicleUpgrade32 = 0x002D2F74 + 1;
constexpr uintptr_t kStreamingHasVehicleUpgrade64 = 0x395098;
constexpr uintptr_t kStreamingModelCDName32 = 0x2CF5D0 + 1;
constexpr uintptr_t kStreamingModelCDName64 = 0x391320;
constexpr uintptr_t kStreamingEndRequestedList32 = 0x677D04;
constexpr uintptr_t kStreamingEndRequestedList64 = 0x84DA38;
constexpr uintptr_t kStreamingStartRequestedList32 = 0x6766C0;
constexpr uintptr_t kStreamingStartRequestedList64 = 0x84ADF0;
constexpr uintptr_t kStreamingEndLoadedList32 = 0x678458;
constexpr uintptr_t kStreamingEndLoadedList64 = 0x84E8D8;
constexpr uintptr_t kStreamingStartLoadedList32 = 0x677238;
constexpr uintptr_t kStreamingStartLoadedList64 = 0x84C4B8;
constexpr uintptr_t kStreamingEnableRequestPurge32 = 0x676464;
constexpr uintptr_t kStreamingEnableRequestPurge64 = 0x84A938;
constexpr uintptr_t kStreamingDisableStreaming32 = 0x6767DC;
constexpr uintptr_t kStreamingDisableStreaming64 = 0x84B028;
constexpr uintptr_t kStreamingChannel32 = 0x67832C;
constexpr uintptr_t kStreamingChannel64 = 0x84E680;
constexpr uintptr_t kStreamingNumModelsRequested32 = 0x676280;
constexpr uintptr_t kStreamingNumModelsRequested64 = 0x84A580;
constexpr uintptr_t kStreamingNumPriorityRequests32 = 0x678E40;
constexpr uintptr_t kStreamingNumPriorityRequests64 = 0x84FCB0;
constexpr uintptr_t kStreamingLoadingBigModel32 = 0x677D50;
constexpr uintptr_t kStreamingLoadingBigModel64 = 0x84DAD0;
constexpr uintptr_t kStreamingChannelError32 = 0x676CE0;
constexpr uintptr_t kStreamingChannelError64 = 0x84BA20;
constexpr uintptr_t kStreamingBuffer32 = 0x676214;
constexpr uintptr_t kStreamingBuffer64 = 0x84A4A8;
constexpr uintptr_t kStreamingBufferSize32 = 0x67962C;
constexpr uintptr_t kStreamingBufferSize64 = 0x850C78;
constexpr uintptr_t kStreamingMemoryUsed32 = 0x00679EB4;
constexpr uintptr_t kStreamingMemoryUsed64 = 0x851D80;
constexpr uintptr_t kStreamingMemoryAvailable32 = 0x006791EC;
constexpr uintptr_t kStreamingMemoryAvailable64 = 0x850408;
constexpr uintptr_t kStreamingDesiredVehicles32 = 0x006795A4;
constexpr uintptr_t kStreamingDesiredVehicles64 = 0x850B68;
constexpr uintptr_t kStreamingFiles32 = 0x00676AB8;
constexpr uintptr_t kStreamingFiles64 = 0x84B5D0;
constexpr uintptr_t kStreamingRwObjectInstances32 = 0x00677564;
constexpr uintptr_t kStreamingRwObjectInstances64 = 0x84CB10;
constexpr uintptr_t kStreamingInfoForModel32 = 0x00677DD0;
constexpr uintptr_t kStreamingInfoForModel64 = 0x84DBD0;
constexpr uintptr_t kVehicleAudioService32 = 0x003ACE04 + 1;
constexpr uintptr_t kVehicleAudioService64 = 0x489B78;
constexpr uintptr_t kAutomobileFix32 = 0x55D5C0 + 1;
constexpr uintptr_t kAutomobileFix64 = 0x67DF0C;
constexpr uintptr_t kAutomobileSetupDamageAfterLoad32 = 0x55D886 + 1;
constexpr uintptr_t kAutomobileSetupDamageAfterLoad64 = 0x67E368;
constexpr uintptr_t kPedUpdatePositionPatch32 = 0x004A2A92;
constexpr uintptr_t kPedUpdatePositionPatch64 = 0x598D1C;
constexpr uintptr_t kTaskUseGun32 = 0x004DDB70 + 1;
constexpr uintptr_t kTaskUseGun64 = 0x5DFCC4;
constexpr uintptr_t kPadTaskProcess32 = 0x00539F9C + 1;
constexpr uintptr_t kPadTaskProcess64 = 0x655E28;
constexpr uintptr_t kPedProcessControlPlt32 = 0x6692A4;
constexpr uintptr_t kPedProcessControlPlt64 = 0x833150;
constexpr uintptr_t kTaskUseGunPlt32 = 0x66968C;
constexpr uintptr_t kTaskUseGunPlt64 = 0x833928;
constexpr uintptr_t kPadTaskProcessPlt32 = 0x66CF4C;
constexpr uintptr_t kPadTaskProcessPlt64 = 0x83ACF8;
constexpr uintptr_t kVehicleProcessControlPlt32[] = {
    0x66D6A4, 0x66DA4C, 0x66D81C, 0x66DDB0, 0x66DB60,
    0x66D934, 0x66DC88, 0x66DED8, 0x66E128
};
constexpr uintptr_t kVehicleProcessControlPlt64[] = {
    0x83BBA8, 0x83C2F8, 0x83BE98, 0x83C9C0, 0x83C520,
    0x83C0C8, 0x83C770, 0x83CC10, 0x83D0B0
};
constexpr uintptr_t kWidgetButtonUpdatePlt32 = 0x00671424;
constexpr uintptr_t kWidgetButtonUpdatePlt64 = 0x842408;
constexpr uintptr_t kWidgetIsTouched32 = 0x002B3324 + 1;
constexpr uintptr_t kWidgetIsTouched64 = 0x3725D0;
constexpr uintptr_t kWidgetIsReleased32 = 0x002B3484 + 1;
constexpr uintptr_t kWidgetIsReleased64 = 0x372794;
constexpr uintptr_t kOSDeviceForm32 = 0x0026BAD4 + 1;
constexpr uintptr_t kOSDeviceForm64 = 0x32155C;
constexpr uintptr_t kTouchInterfaceWidgets32 = 0x00679474;
constexpr uintptr_t kTouchInterfaceWidgets64 = 0x850910;
constexpr uintptr_t kPedPoolGetAt32 = 0x00483DB8 + 1;
constexpr uintptr_t kPedPoolGetAt64 = 0x575D0C;
constexpr uintptr_t kPedPoolGetIndex32 = 0x483DAA + 1;
constexpr uintptr_t kPedPoolGetIndex64 = 0x575CFC;
constexpr uintptr_t kObjectPoolGetAt32 = 0x00483DD2 + 1;
constexpr uintptr_t kObjectPoolGetAt64 = 0x575D30;
constexpr uintptr_t kVehiclePoolGetIndex32 = 0x00483D90 + 1;
constexpr uintptr_t kVehiclePoolGetIndex64 = 0x575CD8;
constexpr uintptr_t kVehiclePoolGetAt32 = 0x00483D9E + 1;
constexpr uintptr_t kVehiclePoolGetAt64 = 0x575CE8;
constexpr uintptr_t kVehicleCarVTable32 = 0x0066D678;
constexpr uintptr_t kVehicleCarVTable64 = 0x83BB50;
constexpr uintptr_t kVehicleBoatVTable32 = 0x0066DA20;
constexpr uintptr_t kVehicleBoatVTable64 = 0x83C2A0;
constexpr uintptr_t kVehicleBikeVTable32 = 0x0066D7F0;
constexpr uintptr_t kVehicleBikeVTable64 = 0x83BE40;
constexpr uintptr_t kVehiclePlaneVTable32 = 0x0066DD84;
constexpr uintptr_t kVehiclePlaneVTable64 = 0x83C968;
constexpr uintptr_t kVehicleHeliVTable32 = 0x0066DB34;
constexpr uintptr_t kVehicleHeliVTable64 = 0x83C4C8;
constexpr uintptr_t kVehiclePushBikeVTable32 = 0x0066D908;
constexpr uintptr_t kVehiclePushBikeVTable64 = 0x83C070;
constexpr uintptr_t kVehicleTrainVTable32 = 0x0066E0FC;
constexpr uintptr_t kVehicleTrainVTable64 = 0x83D058;
constexpr uintptr_t kPedModelInfoVTable32 = 0x00667658;
constexpr uintptr_t kPedModelInfoVTable64 = 0x82F310;
constexpr uintptr_t kSpriteSetTextureLegacy32 = 0x5C8838 + 1;
constexpr uintptr_t kSpriteSetVerticesLegacy32 = 0x5C9014 + 1;
constexpr uintptr_t kSpriteVertexBufferLegacy32 = 0xA7C264;
constexpr uintptr_t kSpriteDrawLegacy32 = 0x5C9120 + 1;
constexpr uintptr_t kSprite2dDraw32 = 0x005C8F20 + 1;
constexpr uintptr_t kSprite2dDraw64 = 0x6ED440;
constexpr uintptr_t kSprite2dRecipNearClip32 = 0x006766AC;
constexpr uintptr_t kSprite2dRecipNearClip64 = 0x84ADC8;
constexpr uintptr_t kSprite2dNearScreenZ32 = 0x00675F8C;
constexpr uintptr_t kSprite2dNearScreenZ64 = 0x849FA0;
constexpr uintptr_t kSpriteDrawUV32 = 0x1974EC + 1;
constexpr uintptr_t kSpriteDrawUV64 = 0x6EDC5C;
constexpr uintptr_t kVisibilityInitialise32 = 0x005D446C + 1;
constexpr uintptr_t kVisibilityInitialise64 = 0x6F954C;
constexpr uintptr_t kVisibilitySetRenderWareCamera32 = 0x005D6248 + 1;
constexpr uintptr_t kVisibilitySetRenderWareCamera64 = 0x6FB5BC;
constexpr uintptr_t kVisibilityGetClumpAlpha32 = 0x005D4FEC + 1;
constexpr uintptr_t kVisibilityGetClumpAlpha64 = 0x6FA290;
constexpr uintptr_t kVisibilityRenderAlphaAtomic32 = 0x005D6D20 + 1;
constexpr uintptr_t kVisibilityRenderAlphaAtomic64 = 0x6FC1A8;
constexpr uintptr_t kVisibilitySetupVehicleVariables32 = 0x005D4B90 + 1;
constexpr uintptr_t kVisibilitySetupVehicleVariables64 = 0x6F9DB0;
constexpr uintptr_t kVisibilityRenderReallyDrawLast32 = 0x005D6EC4 + 1;
constexpr uintptr_t kVisibilityRenderReallyDrawLast64 = 0x6FC3DC;
constexpr uintptr_t kVisibilityGetAtomicId32 = 0x5D4B54 + 1;
constexpr uintptr_t kVisibilityGetAtomicId64 = 0x6F9D68;
constexpr uintptr_t kTextureListingCreateRaster32 = 0x1E8CF8 + 1;
constexpr uintptr_t kTextureListingCreateRaster64 = 0x2851DC;
constexpr uintptr_t kRpSkinGeometryGetSkin32 = 0x001C98FC + 1;
constexpr uintptr_t kRpSkinGeometryGetSkin64 = 0x25DEA0;
constexpr uintptr_t kRpSkinAtomicGetHAnimHierarchy32 = 0x001C98EC + 1;
constexpr uintptr_t kRpSkinAtomicGetHAnimHierarchy64 = 0x25DE8C;
constexpr uintptr_t kAtomicModelInfoSetFlags32 = 0x0046AD34 + 1;
constexpr uintptr_t kAtomicModelInfoSetFlags64 = 0x556530;
constexpr uintptr_t kClumpBoundingSphere32 = 0x5D0E3C + 1;
constexpr uintptr_t kClumpBoundingSphere64 = 0x6F51F8;
constexpr uintptr_t kCustomBuildingDNBalance32 = 0x676E88;
constexpr uintptr_t kCustomBuildingDNBalance64 = 0x84BD68;
constexpr uintptr_t kAnimBlendHierarchyMoveMemory32 = 0x0038A862 + 1;
constexpr uintptr_t kAnimBlendHierarchyMoveMemory64 = 0x462C28;
constexpr uintptr_t kAnimBlendStaticAssociationCtor32 = 0x00389940 + 1;
constexpr uintptr_t kAnimBlendStaticAssociationCtor64 = 0x461758;
constexpr uintptr_t kAnimBlendStaticAssociationInit32 = 0x0038998C + 1;
constexpr uintptr_t kAnimBlendStaticAssociationInit64 = 0x46177C;
constexpr uintptr_t kAnimBlendNodeNextKeyFrame32 = 0x0038AC1C + 1;
constexpr uintptr_t kAnimBlendNodeNextKeyFrame64 = 0x463114;
constexpr uintptr_t kAnimBlendNodeNextKeyFrameCompressed32 = 0x0038B3FC + 1;
constexpr uintptr_t kAnimBlendNodeNextKeyFrameCompressed64 = 0x463954;
constexpr uintptr_t kAnimBlendNodeNextKeyFrameNoCalc32 = 0x0038A9EE + 1;
constexpr uintptr_t kAnimBlendNodeNextKeyFrameNoCalc64 = 0x462E90;
constexpr uintptr_t kAnimBlendNodeUpdate32 = 0x0038AA94 + 1;
constexpr uintptr_t kAnimBlendNodeUpdate64 = 0x462F74;
constexpr uintptr_t kAnimBlendNodeUpdateCompressed32 = 0x0038B1D8 + 1;
constexpr uintptr_t kAnimBlendNodeUpdateCompressed64 = 0x4636D0;
constexpr uintptr_t kAnimBlendAssocGroupCtor32 = 0x003891AC + 1;
constexpr uintptr_t kAnimBlendAssocGroupCtor64 = 0x460C28;
constexpr uintptr_t kAnimBlendAssocGroupCopyByName32 = 0x00389878 + 1;
constexpr uintptr_t kAnimBlendAssocGroupCopyByName64 = 0x4615E8;
constexpr uintptr_t kAnimBlendAssocGroupCopyById32 = 0x003898C4 + 1;
constexpr uintptr_t kAnimBlendAssocGroupCopyById64 = 0x461670;
constexpr uintptr_t kAnimBlendAssocGroupCreateBlock32 = 0x003892E8 + 1;
constexpr uintptr_t kAnimBlendAssocGroupCreateBlock64 = 0x460DE0;
constexpr uintptr_t kAnimBlendAssocGroupCreateFromClump32 = 0x003896F0 + 1;
constexpr uintptr_t kAnimBlendAssocGroupCreateFromClump64 = 0x461380;
constexpr uintptr_t kAnimBlendAssocGroupCreateFromNames32 = 0x00389434 + 1;
constexpr uintptr_t kAnimBlendAssocGroupCreateFromNames64 = 0x460FAC;
constexpr uintptr_t kAnimBlendAssocGroupDestroyAssociations32 = 0x003891FE + 1;
constexpr uintptr_t kAnimBlendAssocGroupDestroyAssociations64 = 0x460C9C;
constexpr uintptr_t kAnimBlendAssocGroupGetByName32 = 0x00389836 + 1;
constexpr uintptr_t kAnimBlendAssocGroupGetByName64 = 0x461574;
constexpr uintptr_t kAnimBlendAssocGroupGetId32 = 0x003898F0 + 1;
constexpr uintptr_t kAnimBlendAssocGroupGetId64 = 0x4616C4;
constexpr uintptr_t kAnimBlendAssocGroupInitEmpty32 = 0x00389800 + 1;
constexpr uintptr_t kAnimBlendAssocGroupInitEmpty64 = 0x461508;
constexpr uintptr_t kAnimBlendAssocGroupDtor32 = 0x00389800 + 1;
constexpr uintptr_t kAnimBlendAssocGroupDtor64 = 0x460C3C;
constexpr uintptr_t kRtQuatConvertFromMatrix32 = 0x210ED0 + 1;
constexpr uintptr_t kRtQuatConvertFromMatrix64 = 0x2B6854;
constexpr uintptr_t kRtQuatRotate32 = 0x211134 + 1;
constexpr uintptr_t kRtQuatRotate64 = 0x2B6AF8;
constexpr uintptr_t kRtQuatQueryRotate32 = 0x2113C0 + 1;
constexpr uintptr_t kRtQuatQueryRotate64 = 0x2B6D20;
constexpr uintptr_t kRtQuatTransformVectors32 = 0x2114D0 + 1;
constexpr uintptr_t kRtQuatTransformVectors64 = 0x2B6E3C;
constexpr uintptr_t kRtQuatModulus32 = 0x2115DA + 1;
constexpr uintptr_t kRtQuatModulus64 = 0x2B7044;
constexpr uintptr_t kRenderRwClump32 = 0x21425C + 1;
constexpr uintptr_t kRenderRwClump64 = 0x2BA6A4;
constexpr uintptr_t kRwMatrixDestroy32 = 0x001E446C + 1;
constexpr uintptr_t kRwMatrixDestroy64 = 0x27F220;
constexpr uintptr_t kRwV3dTransformPoint32 = 0x001E690C + 1;
constexpr uintptr_t kRwV3dTransformPoint64 = 0x28231C;
constexpr uintptr_t kRwV3dTransformPoints32 = 0x001E6934 + 1;
constexpr uintptr_t kRwV3dTransformPoints64 = 0x28235C;
constexpr uintptr_t kRwMatrixOrthoNormalize32 = 0x001E3420 + 1;
constexpr uintptr_t kRwMatrixOrthoNormalize64 = 0x27E280;
constexpr uintptr_t kRwStreamRead32 = 0x001E56D4 + 1;
constexpr uintptr_t kRwStreamRead64 = 0x28091C;
constexpr uintptr_t kRwStreamOpen32 = 0x001E59F0 + 1;
constexpr uintptr_t kRwStreamOpen64 = 0x280E44;
constexpr uintptr_t kRwStreamClose32 = 0x001E5958 + 1;
constexpr uintptr_t kRwStreamClose64 = 0x280D24;
constexpr uintptr_t kRwMatrixTransform32 = 0x001E402C + 1;
constexpr uintptr_t kRwMatrixTransform64 = 0x27EDE0;
constexpr uintptr_t kRwMatrixCreate32 = 0x001E4494 + 1;
constexpr uintptr_t kRwMatrixCreate64 = 0x27F260;
constexpr uintptr_t kRpClumpForAllAtomics32 = 0x00213D66 + 1;
constexpr uintptr_t kRpClumpForAllAtomics64 = 0x2BA020;
constexpr uintptr_t kRpGeometryForAllMaterials32 = 0x00215F30 + 1;
constexpr uintptr_t kRpGeometryForAllMaterials64 = 0x2BCE78;
constexpr uintptr_t kRpClumpDestroy32 = 0x0021458C + 1;
constexpr uintptr_t kRpClumpDestroy64 = 0x2BAAB0;
constexpr uintptr_t kRpLightCreate32 = 0x00216DB0 + 1;
constexpr uintptr_t kRpLightCreate64 = 0x2BE078;
constexpr uintptr_t kRpLightDestroy32 = 0x00216EF4 + 1;
constexpr uintptr_t kRpLightDestroy64 = 0x2BE210;
constexpr uintptr_t kRpWorldCreate32 = 0x0021D144 + 1;
constexpr uintptr_t kRpWorldCreate64 = 0x2C6714;
constexpr uintptr_t kRpWorldAddCamera32 = 0x0021DF84 + 1;
constexpr uintptr_t kRpWorldAddCamera64 = 0x2C78F0;
constexpr uintptr_t kRpLightSetColor32 = 0x00216746 + 1;
constexpr uintptr_t kRpLightSetColor64 = 0x2BD930;
constexpr uintptr_t kAtomicDefaultRenderCallBack32 = 0x002138DC + 1;
constexpr uintptr_t kAtomicDefaultRenderCallBack64 = 0x2B9B08;
constexpr uintptr_t kRpWorldAddLight32 = 0x0021E7B0 + 1;
constexpr uintptr_t kRpWorldAddLight64 = 0x2C8588;
constexpr uintptr_t kRpWorldRemoveLight32 = 0x0021E7F4 + 1;
constexpr uintptr_t kRpWorldRemoveLight64 = 0x2C85F4;
constexpr uintptr_t kRpAtomicDestroy32 = 0x0021416C + 1;
constexpr uintptr_t kRpAtomicDestroy64 = 0x2BA534;
constexpr uintptr_t kRpClumpGtaCancelStream32 = 0x005D0BA8 + 1;
constexpr uintptr_t kRpClumpGtaCancelStream64 = 0x6F4E38;
constexpr uintptr_t kRtAnimAnimationFreeListCreateParams32 = 0x001EADA4 + 1;
constexpr uintptr_t kRtAnimAnimationFreeListCreateParams64 = 0x287D38;
constexpr uintptr_t kRtAnimInitialize32 = 0x001EADBC + 1;
constexpr uintptr_t kRtAnimInitialize64 = 0x287D4C;
constexpr uintptr_t kRtAnimRegisterInterpolationScheme32 = 0x001EAE68 + 1;
constexpr uintptr_t kRtAnimRegisterInterpolationScheme64 = 0x287E1C;
constexpr uintptr_t kRtAnimGetInterpolatorInfo32 = 0x001EAEF4 + 1;
constexpr uintptr_t kRtAnimGetInterpolatorInfo64 = 0x287EB0;
constexpr uintptr_t kRtAnimAnimationCreate32 = 0x001EAF2C + 1;
constexpr uintptr_t kRtAnimAnimationCreate64 = 0x287F04;
constexpr uintptr_t kRtAnimAnimationDestroy32 = 0x001EAFBC + 1;
constexpr uintptr_t kRtAnimAnimationDestroy64 = 0x287FE8;
constexpr uintptr_t kRtAnimAnimationRead32 = 0x001EAFD8 + 1;
constexpr uintptr_t kRtAnimAnimationRead64 = 0x288010;
constexpr uintptr_t kRtAnimAnimationWrite32 = 0x001EB130 + 1;
constexpr uintptr_t kRtAnimAnimationWrite64 = 0x28824C;
constexpr uintptr_t kRtAnimAnimationStreamRead32 = 0x001EB010 + 1;
constexpr uintptr_t kRtAnimAnimationStreamRead64 = 0x288080;
constexpr uintptr_t kRtAnimAnimationStreamWrite32 = 0x001EB15C + 1;
constexpr uintptr_t kRtAnimAnimationStreamWrite64 = 0x2882A0;
constexpr uintptr_t kRtAnimAnimationStreamGetSize32 = 0x001EB1E4 + 1;
constexpr uintptr_t kRtAnimAnimationStreamGetSize64 = 0x288374;
constexpr uintptr_t kRtAnimAnimationGetNumNodes32 = 0x001EB1F2 + 1;
constexpr uintptr_t kRtAnimAnimationGetNumNodes64 = 0x288394;
constexpr uintptr_t kRtAnimInterpolatorCreate32 = 0x001EB218 + 1;
constexpr uintptr_t kRtAnimInterpolatorCreate64 = 0x2883D8;
constexpr uintptr_t kRtAnimInterpolatorDestroy32 = 0x001EB270 + 1;
constexpr uintptr_t kRtAnimInterpolatorDestroy64 = 0x28844C;
constexpr uintptr_t kRtAnimInterpolatorSetCurrentAnim32 = 0x001EB284 + 1;
constexpr uintptr_t kRtAnimInterpolatorSetCurrentAnim64 = 0x288460;
constexpr uintptr_t kRtAnimInterpolatorSetKeyFrameCallBacks32 = 0x001EB370 + 1;
constexpr uintptr_t kRtAnimInterpolatorSetKeyFrameCallBacks64 = 0x288588;
constexpr uintptr_t kRtAnimInterpolatorSetAnimLoopCallBack32 = 0x001EB3D8 + 1;
constexpr uintptr_t kRtAnimInterpolatorSetAnimLoopCallBack64 = 0x288608;
constexpr uintptr_t kRtAnimInterpolatorSetAnimCallBack32 = 0x001EB3DE + 1;
constexpr uintptr_t kRtAnimInterpolatorSetAnimCallBack64 = 0x288610;
constexpr uintptr_t kRtAnimInterpolatorCopy32 = 0x001EB3E6 + 1;
constexpr uintptr_t kRtAnimInterpolatorCopy64 = 0x28861C;
constexpr uintptr_t kRtAnimInterpolatorSubAnimTime32 = 0x001EB3FC + 1;
constexpr uintptr_t kRtAnimInterpolatorSubAnimTime64 = 0x288648;
constexpr uintptr_t kRtAnimInterpolatorAddAnimTime32 = 0x001EB530 + 1;
constexpr uintptr_t kRtAnimInterpolatorAddAnimTime64 = 0x2887B4;
constexpr uintptr_t kRtAnimInterpolatorSetCurrentTime32 = 0x001EB6DC + 1;
constexpr uintptr_t kRtAnimInterpolatorSetCurrentTime64 = 0x2889AC;
constexpr uintptr_t kRtAnimAnimationMakeDelta32 = 0x001EB710 + 1;
constexpr uintptr_t kRtAnimAnimationMakeDelta64 = 0x2889E0;
constexpr uintptr_t kRtAnimInterpolatorBlend32 = 0x001EB854 + 1;
constexpr uintptr_t kRtAnimInterpolatorBlend64 = 0x288B84;
constexpr uintptr_t kRtAnimInterpolatorAddTogether32 = 0x001EB8B0 + 1;
constexpr uintptr_t kRtAnimInterpolatorAddTogether64 = 0x288C24;
constexpr uintptr_t kRtAnimInterpolatorCreateSubInterpolator32 = 0x001EB900 + 1;
constexpr uintptr_t kRtAnimInterpolatorCreateSubInterpolator64 = 0x288CB4;
constexpr uintptr_t kRtAnimInterpolatorBlendSubInterpolator32 = 0x001EB96C + 1;
constexpr uintptr_t kRtAnimInterpolatorBlendSubInterpolator64 = 0x288D4C;
constexpr uintptr_t kRtAnimInterpolatorAddSubInterpolator32 = 0x001EBB0E + 1;
constexpr uintptr_t kRtAnimInterpolatorAddSubInterpolator64 = 0x288F7C;
constexpr uintptr_t kRpHAnimHierarchySetFreeListCreateParams32 = 0x001C201C + 1;
constexpr uintptr_t kRpHAnimHierarchySetFreeListCreateParams64 = 0x254B18;
constexpr uintptr_t kRpHAnimHierarchyCreate32 = 0x001C2674 + 1;
constexpr uintptr_t kRpHAnimHierarchyCreate64 = 0x255390;
constexpr uintptr_t kRpHAnimHierarchyCreateFromHierarchy32 = 0x001C28DC + 1;
constexpr uintptr_t kRpHAnimHierarchyCreateFromHierarchy64 = 0x25570C;
constexpr uintptr_t kRpHAnimHierarchyDestroy32 = 0x001C274C + 1;
constexpr uintptr_t kRpHAnimHierarchyDestroy64 = 0x2554D4;
constexpr uintptr_t kRpHAnimHierarchyCreateSubHierarchy32 = 0x001C27D4 + 1;
constexpr uintptr_t kRpHAnimHierarchyCreateSubHierarchy64 = 0x255580;
constexpr uintptr_t kRpHAnimHierarchyAttach32 = 0x001C29D0 + 1;
constexpr uintptr_t kRpHAnimHierarchyAttach64 = 0x2558A0;
constexpr uintptr_t kRpHAnimHierarchyDetach32 = 0x001C2A98 + 1;
constexpr uintptr_t kRpHAnimHierarchyDetach64 = 0x2559C0;
constexpr uintptr_t kRpHAnimHierarchyAttachFrameIndex32 = 0x001C2ABC + 1;
constexpr uintptr_t kRpHAnimHierarchyAttachFrameIndex64 = 0x2559F4;
constexpr uintptr_t kRpHAnimHierarchyDetachFrameIndex32 = 0x001C2B9C + 1;
constexpr uintptr_t kRpHAnimHierarchyDetachFrameIndex64 = 0x255B54;
constexpr uintptr_t kRpHAnimFrameSetHierarchy32 = 0x001C2BAC + 1;
constexpr uintptr_t kRpHAnimFrameSetHierarchy64 = 0x255B70;
constexpr uintptr_t kRpHAnimFrameGetHierarchy32 = 0x001C2BD8 + 1;
constexpr uintptr_t kRpHAnimFrameGetHierarchy64 = 0x255BA0;
constexpr uintptr_t kRpHAnimHierarchyGetMatrixArray32 = 0x001C2BA8 + 1;
constexpr uintptr_t kRpHAnimHierarchyGetMatrixArray64 = 0x255B68;
constexpr uintptr_t kRpHAnimHierarchyUpdateMatrices32 = 0x001C2C34 + 1;
constexpr uintptr_t kRpHAnimHierarchyUpdateMatrices64 = 0x255C20;
constexpr uintptr_t kRpHAnimIDGetIndex32 = 0x001C2C10 + 1;
constexpr uintptr_t kRpHAnimIDGetIndex64 = 0x255BE8;
constexpr uintptr_t kRpHAnimPluginAttach32 = 0x001C2034 + 1;
constexpr uintptr_t kRpHAnimPluginAttach64 = 0x254B2C;
constexpr uintptr_t kRpHAnimKeyFrameApply32 = 0x001C34F8 + 1;
constexpr uintptr_t kRpHAnimKeyFrameApply64 = 0x256440;
constexpr uintptr_t kRpHAnimKeyFrameBlend32 = 0x001C3A78 + 1;
constexpr uintptr_t kRpHAnimKeyFrameBlend64 = 0x25694C;
constexpr uintptr_t kRpHAnimKeyFrameInterpolate32 = 0x001C35C8 + 1;
constexpr uintptr_t kRpHAnimKeyFrameInterpolate64 = 0x256500;
constexpr uintptr_t kRpHAnimKeyFrameAdd32 = 0x001C4112 + 1;
constexpr uintptr_t kRpHAnimKeyFrameAdd64 = 0x256FB8;
constexpr uintptr_t kRpHAnimKeyFrameMulRecip32 = 0x001C3FF6 + 1;
constexpr uintptr_t kRpHAnimKeyFrameMulRecip64 = 0x256EFC;
constexpr uintptr_t kRpHAnimKeyFrameStreamRead32 = 0x001C3F18 + 1;
constexpr uintptr_t kRpHAnimKeyFrameStreamRead64 = 0x256D84;
constexpr uintptr_t kRpHAnimKeyFrameStreamWrite32 = 0x001C3F8A + 1;
constexpr uintptr_t kRpHAnimKeyFrameStreamWrite64 = 0x256E58;
constexpr uintptr_t kRpHAnimKeyFrameStreamGetSize32 = 0x001C3FEC + 1;
constexpr uintptr_t kRpHAnimKeyFrameStreamGetSize64 = 0x256EEC;
constexpr uintptr_t kRpHAnimFrameSetID32 = 0x001C2BEC + 1;
constexpr uintptr_t kRpHAnimFrameSetID64 = 0x255BB8;
constexpr uintptr_t kRpHAnimFrameGetID32 = 0x001C2C00 + 1;
constexpr uintptr_t kRpHAnimFrameGetID64 = 0x255BD4;
constexpr uintptr_t kRsGlobal32 = 0x009FC8FC;
constexpr uintptr_t kRsGlobal64 = 0xC9B320;
constexpr uintptr_t kRwFrameUpdateObjects32 = 0x001D802C + 1;
constexpr uintptr_t kRwFrameUpdateObjects64 = 0x26F7B4;
constexpr uintptr_t kRwTextureCreate32 = 0x001DB7BC + 1;
constexpr uintptr_t kRwTextureCreate64 = 0x273F74;
constexpr uintptr_t kRwCameraCreate32 = 0x001D5EE0 + 1;
constexpr uintptr_t kRwCameraCreate64 = 0x26D454;
constexpr uintptr_t kRwFrameCreate32 = 0x001D81AC + 1;
constexpr uintptr_t kRwFrameCreate64 = 0x26F9E0;
constexpr uintptr_t kRwCameraClear32 = 0x001D5CF0 + 1;
constexpr uintptr_t kRwCameraClear64 = 0x26D1E8;
constexpr uintptr_t kRwCameraSetNearClipPlane32 = 0x001D5A38 + 1;
constexpr uintptr_t kRwCameraSetNearClipPlane64 = 0x26CF8C;
constexpr uintptr_t kRwCameraSetFarClipPlane32 = 0x001D5ACC + 1;
constexpr uintptr_t kRwCameraSetFarClipPlane64 = 0x26D034;
constexpr uintptr_t kRwFrameTranslate32 = 0x001D8614 + 1;
constexpr uintptr_t kRwFrameTranslate64 = 0x270060;
constexpr uintptr_t kRwFrameRotate32 = 0x001D8728 + 1;
constexpr uintptr_t kRwFrameRotate64 = 0x270204;
constexpr uintptr_t kRwCameraSetViewWindow32 = 0x001D5E04 + 1;
constexpr uintptr_t kRwCameraSetViewWindow64 = 0x26D330;
constexpr uintptr_t kRwCameraSetProjection32 = 0x001D5D28 + 1;
constexpr uintptr_t kRwCameraSetProjection64 = 0x26D24C;
constexpr uintptr_t kRwObjectHasFrameSetFrame32 = 0x001DCF64 + 1;
constexpr uintptr_t kRwObjectHasFrameSetFrame64 = 0x275CF0;
constexpr uintptr_t kRwFrameGetLTM32 = 0x001D849C + 1;
constexpr uintptr_t kRwFrameGetLTM64 = 0x26FE10;
constexpr uintptr_t kRwCameraEndUpdate32 = 0x001D5A14 + 1;
constexpr uintptr_t kRwCameraEndUpdate64 = 0x26CF48;
constexpr uintptr_t kRwIm3DEnd32 = 0x001DD03C + 1;
constexpr uintptr_t kRwIm3DEnd64 = 0x275E20;
constexpr uintptr_t kRwIm3DRenderPrimitive32 = 0x001DD1C4 + 1;
constexpr uintptr_t kRwIm3DRenderPrimitive64 = 0x275FD8;
constexpr uintptr_t kRwIm3DRenderIndexedPrimitive32 = 0x001DD084 + 1;
constexpr uintptr_t kRwIm3DRenderIndexedPrimitive64 = 0x275E70;
constexpr uintptr_t kRwIm3DTransform32 = 0x001DCFAC + 1;
constexpr uintptr_t kRwIm3DTransform64 = 0x275D54;
constexpr uintptr_t kRwTextureRead32 = 0x001DBA3C + 1;
constexpr uintptr_t kRwTextureRead64 = 0x2742C8;
constexpr uintptr_t kRwFrameForAllObjects32 = 0x001D8858 + 1;
constexpr uintptr_t kRwFrameForAllObjects64 = 0x2703BC;
constexpr uintptr_t kRwFrameDestroy32 = 0x001D83EC + 1;
constexpr uintptr_t kRwFrameDestroy64 = 0x26FD08;
constexpr uintptr_t kRwTextureSetRaster32 = 0x001DB4D4 + 1;
constexpr uintptr_t kRwTextureSetRaster64 = 0x273C28;
constexpr uintptr_t kRwCameraDestroy32 = 0x001D5EA0 + 1;
constexpr uintptr_t kRwCameraDestroy64 = 0x26D3F8;
constexpr uintptr_t kRwIm3DRenderLine32 = 0x001DD3B4 + 1;
constexpr uintptr_t kRwIm3DRenderLine64 = 0x276238;
constexpr uintptr_t kRwTextureSetName32 = 0x001DB820 + 1;
constexpr uintptr_t kRwTextureSetName64 = 0x274000;
constexpr uintptr_t kRwFrameOrthoNormalize32 = 0x001D87FC + 1;
constexpr uintptr_t kRwFrameOrthoNormalize64 = 0x27032C;
constexpr uintptr_t kRwTextureSetFindCallBack32 = 0x001DB3A4 + 1;
constexpr uintptr_t kRwTextureSetFindCallBack64 = 0x273AB4;
constexpr uintptr_t kRwTextureSetReadCallBack32 = 0x001DB3E0 + 1;
constexpr uintptr_t kRwTextureSetReadCallBack64 = 0x273AFC;
constexpr uintptr_t kRwEngineInstance32 = 0x006BCD38;
constexpr uintptr_t kRwEngineInstance64 = 0x89A358;
constexpr uintptr_t kRxPipelineGlobalsOffset32 = 0x006BCF98;
constexpr uintptr_t kRxPipelineGlobalsOffset64 = 0x89A778;
constexpr uintptr_t kFrameNodeNameLegacy32 = 0x48248C + 1;
constexpr uintptr_t kRpAnimBlendPluginAttach32 = 0x00390470 + 1;
constexpr uintptr_t kRpAnimBlendPluginAttach64 = 0x46A30C;
constexpr uintptr_t kRtAnimBlendKeyFrameApply32 = 0x003902D8 + 1;
constexpr uintptr_t kRtAnimBlendKeyFrameApply64 = 0x46A1B8;
constexpr uintptr_t kRpAnimBlendAllocateData32 = 0x003902B4 + 1;
constexpr uintptr_t kRpAnimBlendAllocateData64 = 0x46A17C;
constexpr uintptr_t kRpAnimBlendClumpAddAssociation32 = 0x0039093C + 1;
constexpr uintptr_t kRpAnimBlendClumpAddAssociation64 = 0x46A9A0;
constexpr uintptr_t kRpAnimBlendClumpFillFrameArray32 = 0x00390750 + 1;
constexpr uintptr_t kRpAnimBlendClumpFillFrameArray64 = 0x46A6F0;
constexpr uintptr_t kRpAnimBlendClumpFindBone32 = 0x0039070C + 1;
constexpr uintptr_t kRpAnimBlendClumpFindBone64 = 0x46A694;
constexpr uintptr_t kRpAnimBlendClumpFindFrame32 = 0x003905C4 + 1;
constexpr uintptr_t kRpAnimBlendClumpFindFrame64 = 0x46A4A4;
constexpr uintptr_t kRpAnimBlendClumpFindFrameFromHash32 = 0x0039066C + 1;
constexpr uintptr_t kRpAnimBlendClumpFindFrameFromHash64 = 0x46A5A0;
constexpr uintptr_t kRpAnimBlendClumpGetAssociationByHierarchy32 = 0x00390A78 + 1;
constexpr uintptr_t kRpAnimBlendClumpGetAssociationByHierarchy64 = 0x46AB7C;
constexpr uintptr_t kRpAnimBlendClumpGetAssociationByName32 = 0x00390A24 + 1;
constexpr uintptr_t kRpAnimBlendClumpGetAssociationByName64 = 0x46AAF4;
constexpr uintptr_t kRpAnimBlendClumpGetAssociationByAnimId32 = 0x00390A50 + 1;
constexpr uintptr_t kRpAnimBlendClumpGetAssociationByAnimId64 = 0x46AB48;
constexpr uintptr_t kRpAnimBlendClumpGetFirstAssociationByFlags32 = 0x00390C04 + 1;
constexpr uintptr_t kRpAnimBlendClumpGetFirstAssociationByFlags64 = 0x46AD68;
constexpr uintptr_t kRpAnimBlendClumpGetMainAssociation32 = 0x00390A9C + 1;
constexpr uintptr_t kRpAnimBlendClumpGetMainAssociation64 = 0x46ABB0;
constexpr uintptr_t kRpAnimBlendClumpGetMainAssociationN32 = 0x00390B9C + 1;
constexpr uintptr_t kRpAnimBlendClumpGetMainAssociationN64 = 0x46ACE8;
constexpr uintptr_t kRpAnimBlendClumpGetMainPartialAssociation32 = 0x00390B44 + 1;
constexpr uintptr_t kRpAnimBlendClumpGetMainPartialAssociation64 = 0x46AC80;
constexpr uintptr_t kRpAnimBlendClumpGetMainPartialAssociationN32 = 0x00390BD0 + 1;
constexpr uintptr_t kRpAnimBlendClumpGetMainPartialAssociationN64 = 0x46AD28;
constexpr uintptr_t kRpAnimBlendClumpGetNumAssociations32 = 0x00390CB0 + 1;
constexpr uintptr_t kRpAnimBlendClumpGetNumAssociations64 = 0x46AE54;
constexpr uintptr_t kRpAnimBlendClumpGetNumNonPartialAssociations32 = 0x00390CF4 + 1;
constexpr uintptr_t kRpAnimBlendClumpGetNumNonPartialAssociations64 = 0x46AEB4;
constexpr uintptr_t kRpAnimBlendClumpGetNumPartialAssociations32 = 0x00390CCC + 1;
constexpr uintptr_t kRpAnimBlendClumpGetNumPartialAssociations64 = 0x46AE78;
constexpr uintptr_t kRpAnimBlendClumpGiveAssociations32 = 0x00390D3C + 1;
constexpr uintptr_t kRpAnimBlendClumpGiveAssociations64 = 0x46AF18;
constexpr uintptr_t kRpAnimBlendClumpInit32 = 0x003907E8 + 1;
constexpr uintptr_t kRpAnimBlendClumpInit64 = 0x46A7D4;
constexpr uintptr_t kRpAnimBlendClumpIsInitialized32 = 0x39091C + 1;
constexpr uintptr_t kRpAnimBlendClumpIsInitialized64 = 0x46A974;
constexpr uintptr_t kRpAnimBlendClumpPauseAllAnimations32 = 0x00390C60 + 1;
constexpr uintptr_t kRpAnimBlendClumpPauseAllAnimations64 = 0x46ADF4;
constexpr uintptr_t kRpAnimBlendClumpRemoveAssociations32 = 0x003909EC + 1;
constexpr uintptr_t kRpAnimBlendClumpRemoveAssociations64 = 0x46AA98;
constexpr uintptr_t kRpAnimBlendClumpSetBlendDeltas32 = 0x00390988 + 1;
constexpr uintptr_t kRpAnimBlendClumpSetBlendDeltas64 = 0x46AA14;
constexpr uintptr_t kRpAnimBlendClumpUnPauseAllAnimations32 = 0x00390C88 + 1;
constexpr uintptr_t kRpAnimBlendClumpUnPauseAllAnimations64 = 0x46AE24;
constexpr uintptr_t kRpAnimBlendClumpUpdateAnimations32 = 0x0038BF50 + 1;
constexpr uintptr_t kRpAnimBlendClumpUpdateAnimations64 = 0x464624;
constexpr uintptr_t kRpAnimBlendCreateAnimationForHierarchy32 = 0x00390594 + 1;
constexpr uintptr_t kRpAnimBlendCreateAnimationForHierarchy64 = 0x46A460;
constexpr uintptr_t kRpAnimBlendFrameGetName32 = 0x003902A0 + 1;
constexpr uintptr_t kRpAnimBlendFrameGetName64 = 0x46A154;
constexpr uintptr_t kRpAnimBlendFrameSetName32 = 0x003902A4 + 1;
constexpr uintptr_t kRpAnimBlendFrameSetName64 = 0x46A158;
constexpr uintptr_t kRpAnimBlendGetNextAssociation32 = 0x00390C34 + 1;
constexpr uintptr_t kRpAnimBlendGetNextAssociation64 = 0x46ADAC;
constexpr uintptr_t kRpAnimBlendGetNextAssociationByFlags32 = 0x00390C3E + 1;
constexpr uintptr_t kRpAnimBlendGetNextAssociationByFlags64 = 0x46ADC0;
constexpr uintptr_t kRpAnimBlendKeyFrameInterpolate32 = 0x00390440 + 1;
constexpr uintptr_t kRpAnimBlendKeyFrameInterpolate64 = 0x46A2F4;
constexpr uintptr_t kAnimManagerReadAssocDefinitions32 = 0x0047459C + 1;
constexpr uintptr_t kAnimManagerReadAssocDefinitions64 = 0x560CC4;
constexpr uintptr_t kAnimManagerAddAnimation32 = 0x0038E068 + 1;
constexpr uintptr_t kAnimManagerAddAnimation64 = 0x466B64;
constexpr uintptr_t kAnimManagerAddAnimationByHierarchy32 = 0x0038E0E0 + 1;
constexpr uintptr_t kAnimManagerAddAnimationByHierarchy64 = 0x466C14;
constexpr uintptr_t kAnimManagerAddAnimationAndSync32 = 0x0038E168 + 1;
constexpr uintptr_t kAnimManagerAddAnimationAndSync64 = 0x466CE0;
constexpr uintptr_t kAnimManagerAddAssocDefinition32 = 0x0038E84C + 1;
constexpr uintptr_t kAnimManagerAddAssocDefinition64 = 0x467500;
constexpr uintptr_t kModelInfoCreateClumpVtableOffset32 = 0x2C;
constexpr uintptr_t kModelInfoCreateClumpVtableOffset64 = 0x2C * 2;
constexpr uintptr_t kBaseModelInfoGetModelTypeVtableOffset32 = 0x14;
constexpr uintptr_t kBaseModelInfoGetModelTypeVtableOffset64 = 0x14 * 2;
constexpr uintptr_t kBaseModelInfoDeleteRwObjectVtableOffset32 = 0x24;
constexpr uintptr_t kBaseModelInfoDeleteRwObjectVtableOffset64 = 0x24 * 2;
constexpr uintptr_t kBaseModelInfoGetAnimFileIndexVtableOffset32 = 0x3C;
constexpr uintptr_t kBaseModelInfoGetAnimFileIndexVtableOffset64 = 0x3C * 2;
constexpr uintptr_t kClumpModelInfoGetFrameFromName32 = 0x003856F4 + 1;
constexpr uintptr_t kClumpModelInfoGetFrameFromName64 = 0x45BE78;
constexpr uintptr_t kVehicleModelInfoCtor32 = 0x386E98 + 1;
constexpr uintptr_t kVehicleModelInfoCtor64 = 0x45DF50;
constexpr uintptr_t kVehicleModelInfoVtable32 = 0x006676A8;
constexpr uintptr_t kVehicleModelInfoVtable64 = 0x82F3B0;
constexpr uintptr_t kPedModelInfoCtor32 = 0x00384FD8 + 1;
constexpr uintptr_t kPedModelInfoCtor64 = 0x45B3BC;
constexpr uintptr_t kPedModelInfoVtable32 = 0x00667658;
constexpr uintptr_t kPedModelInfoVtable64 = 0x82F310;
constexpr uintptr_t kAtomicModelInfoCtor32 = 0x00384FD8 + 1;
constexpr uintptr_t kAtomicModelInfoCtor64 = 0x45B3BC;
constexpr uintptr_t kAtomicModelInfoVtable32 = 0x00667444;
constexpr uintptr_t kAtomicModelInfoVtable64 = 0x82EEE8;
constexpr uintptr_t kModelInfoPostConstructorVtableOffset32 = 0x1C;
constexpr uintptr_t kModelInfoPostConstructorVtableOffset64 = 0x1C * 2;
constexpr uintptr_t kModelInfoAddDamageAtomic32 = 0x00385F94 + 1;
constexpr uintptr_t kModelInfoAddDamageAtomic64 = 0x49BAE8;
constexpr uintptr_t kModelInfoAtomicStore32 = 0x676A34;
constexpr uintptr_t kModelInfoAtomicStore64 = 0x84B4C8;
constexpr uintptr_t kModelInfoPedStore32 = 0x6773CC;
constexpr uintptr_t kModelInfoPedStore64 = 0x84C7E0;
constexpr uintptr_t kModelInfoVehicleStore32 = 0x678C8C;
constexpr uintptr_t kModelInfoVehicleStore64 = 0x84F948;
constexpr uintptr_t kModelInfoPtrs32 = 0x6796CC;
constexpr uintptr_t kModelInfoPtrs64 = 0x850DB8;
constexpr uintptr_t kVehicleModelInfoDescs32 = 0x00687C7C;
constexpr uintptr_t kVehicleModelInfoDescs64 = 0x8614C0;
constexpr int32 kVehicleModelInfoDescCount = 12;
constexpr uintptr_t kCustomCarWorldSectorAllInOnePipelineInit64 = 0x2CC144;
constexpr uintptr_t kCustomCarAtomicAllInOnePipelineInit64 = 0x2CB7C8;
constexpr uintptr_t kCustomCarObjPipeline32 = 0x6765A0;
constexpr uintptr_t kCustomCarObjPipeline64 = 0x84ABB0;
constexpr uintptr_t kCustomCarEnvMapPipeMatDataPool32 = 0x676A7C;
constexpr uintptr_t kCustomCarEnvMapPipeMatDataPool64 = 0x84B558;
constexpr uintptr_t kCustomCarEnvMapPipeAtmDataPool32 = 0x676924;
constexpr uintptr_t kCustomCarEnvMapPipeAtmDataPool64 = 0x84B2B0;
constexpr uintptr_t kCustomCarSpecMapPipeMatDataPool32 = 0x678598;
constexpr uintptr_t kCustomCarSpecMapPipeMatDataPool64 = 0x84EB58;
constexpr uintptr_t kAnimManagerRemoveLastAnimFile32 = 0x0038F774 + 1;
constexpr uintptr_t kAnimManagerRemoveLastAnimFile64 = 0x468710;
constexpr uintptr_t kAnimManagerRemoveAnimBlock32 = 0x0038F840 + 1;
constexpr uintptr_t kAnimManagerRemoveAnimBlock64 = 0x4687D4;
constexpr uintptr_t kAnimManagerUncompressAnimation32 = 0x0038DD54 + 1;
constexpr uintptr_t kAnimManagerUncompressAnimation64 = 0x466740;
constexpr uintptr_t kAnimManagerRemoveFromUncompressedCache32 = 0x0038DE60 + 1;
constexpr uintptr_t kAnimManagerRemoveFromUncompressedCache64 = 0x4668AC;
constexpr uintptr_t kAnimManagerBlendAnimationById32 = 0x0038E340 + 1;
constexpr uintptr_t kAnimManagerBlendAnimationById64 = 0x466F3C;
constexpr uintptr_t kAnimManagerNumAnimAssocDefinitions32 = 0x00678508;
constexpr uintptr_t kAnimManagerNumAnimAssocDefinitions64 = 0x84EA38;
constexpr uintptr_t kAnimManagerAnimBlocks32 = 0x006793AC;
constexpr uintptr_t kAnimManagerAnimBlocks64 = 0x850780;
constexpr uintptr_t kAnimManagerNumAnimBlocks32 = 0x00678A44;
constexpr uintptr_t kAnimManagerNumAnimBlocks64 = 0x84F4B0;
constexpr uintptr_t kAnimManagerAnimAssocGroups32 = 0x00677F80;
constexpr uintptr_t kAnimManagerAnimAssocGroups64 = 0x84DF30;
constexpr uintptr_t kAnimManagerAnimations32 = 0x006771C8;
constexpr uintptr_t kAnimManagerAnimations64 = 0x84C3D8;
constexpr uintptr_t kAnimManagerNumAnimations32 = 0x00677EE8;
constexpr uintptr_t kAnimManagerNumAnimations64 = 0x84DE00;
constexpr uintptr_t kAnimManagerAnimCache32 = 0x00678BD8;
constexpr uintptr_t kAnimManagerAnimCache64 = 0x84F7E0;
constexpr uintptr_t kPlayerPedDestructor32 = 0x004CE6A0 + 1;
constexpr uintptr_t kPlayerPedDestructor64 = 0x5CDC64;
constexpr uintptr_t kRwMatrixRotate32 = 0x001E38F4 + 1;
constexpr uintptr_t kRwMatrixRotate64 = 0x27E710;
constexpr uintptr_t kRwMatrixInvertLegacy32 = 0x1E3A28 + 1;
constexpr uintptr_t kTaskVTableMinLegacy32 = 0x6653F4;
constexpr uintptr_t kTaskVTableMaxLegacy32 = 0x66D641;
constexpr uintptr_t kSetScissorRect32 = 0x002B3EC4 + 1;
constexpr uintptr_t kSetScissorRect64 = 0x373290;
constexpr uintptr_t kIsPedPointerValid32 = 0x004A72C4 + 1;
constexpr uintptr_t kIsPedPointerValid64 = 0x59DE5C;
constexpr uintptr_t kRenderOneNonRoad32 = 0x0041030C + 1;
constexpr uintptr_t kRenderOneNonRoad64 = 0x4F56E0;
constexpr uintptr_t kPlayerSetupPed32 = 0x4C39A4 + 1;
constexpr uintptr_t kPlayerSetupPed64 = 0x5C0FD4;
constexpr uintptr_t kPlayerDeactivatePed32 = 0x4C3AD4 + 1;
constexpr uintptr_t kPlayerDeactivatePed64 = 0x5C1140;
constexpr uintptr_t kPlayerReactivatePed32 = 0x4C3AEC + 1;
constexpr uintptr_t kPlayerReactivatePed64 = 0x5C1158;
constexpr uintptr_t kClearSpaceForMissionEntity32 = 0x34DA34 + 1;
constexpr uintptr_t kClearSpaceForMissionEntity64 = 0x419BE0;
constexpr uintptr_t kPlayerPedInitialState32 = 0x004C37B4 + 1;
constexpr uintptr_t kPlayerPedInitialState64 = 0x5C0D50;
constexpr uintptr_t kPedShutdown32 = 0x49F6A4 + 1;
constexpr uintptr_t kPedShutdown64 = 0x59541C;
constexpr uintptr_t kPedGiveWeapon32 = 0x0049F588 + 1;
constexpr uintptr_t kPedGiveWeapon64 = 0x59525C;
constexpr uintptr_t kPedSetCurrentWeapon32 = 0x4A51AC + 1;
constexpr uintptr_t kPedSetCurrentWeapon64 = 0x59B86C;
constexpr uintptr_t kPedSetCurrentWeaponAfterGive32 = 0x004A521C + 1;
constexpr uintptr_t kPedClearWeapons32 = 0x0049F836 + 1;
constexpr uintptr_t kPedClearWeapons64 = 0x595604;
constexpr uintptr_t kPedBonePosition32 = 0x004A4B0C + 1;
constexpr uintptr_t kPedBonePosition64 = 0x59AEE4;
constexpr uintptr_t kPedTransformedBonePosition32 = 0x4A24A8 + 1;
constexpr uintptr_t kPedTransformedBonePosition64 = 0x598670;
constexpr uintptr_t kRunNamedAnimTaskVTableLegacy32 = 0x6694F0;
constexpr uintptr_t kSlideToCoordTaskVTableLegacy32 = 0x66C4E0;
constexpr uintptr_t kAnimAssocGroups32 = 0x00942184;
constexpr uintptr_t kAnimAssocGroups64 = 0xBA88A8;
constexpr uintptr_t kWeaponInfo32 = 0x005E42E8 + 1;
constexpr uintptr_t kWeaponInfo64 = 0x709BA8;
constexpr uintptr_t kPedIntelligenceCrouch32 = 0x004C07B0 + 1;
constexpr uintptr_t kPedIntelligenceCrouch64 = 0x5BCE70;
constexpr uintptr_t kPedIntelligenceResetCrouch32 = 0x004C08A8 + 1;
constexpr uintptr_t kPedIntelligenceResetCrouch64 = 0x5BCFF8;
constexpr uintptr_t kPedIntelligenceFlush32 = 0x004C1508 + 1;
constexpr uintptr_t kPedIntelligenceFlush64 = 0x5BE0E0;
constexpr uintptr_t kPedIntelligenceFlushImmediately32 = 0x004C0AB4 + 1;
constexpr uintptr_t kPedIntelligenceFlushImmediately64 = 0x5BD2D0;
constexpr uintptr_t kPedIntelligenceProcessAfterPreRender32 = 0x004C11BC + 1;
constexpr uintptr_t kPedIntelligenceProcessAfterPreRender64 = 0x5BDC18;
constexpr uintptr_t kJetpackCheat32 = 0x2FE258 + 1;
constexpr uintptr_t kJetpackCheat64 = 0x3C2A40;
constexpr uintptr_t kEntityAdd32 = 0x3ED8D8 + 1;
constexpr uintptr_t kEntityAdd64 = 0x4CD574;
constexpr uintptr_t kEntityAddRect32 = 0x3ED8FC + 1;
constexpr uintptr_t kEntityAddRect64 = 0x4CD5C0;
constexpr uintptr_t kEntityRemove32 = 0x3EDBE8 + 1;
constexpr uintptr_t kEntityRemove64 = 0x4CD888;
constexpr uintptr_t kStopExtraColour32 = 0x00420898 + 1;
constexpr uintptr_t kStopExtraColour64 = 0x504194;
constexpr uintptr_t kPlaceableSetPositionVtableOffset32 = 0x3C;
constexpr uintptr_t kPlaceableSetPositionVtableOffset64 = 0x3C * 2;
constexpr uintptr_t kPlaceableVTable32 = 0x00667D14;
constexpr uintptr_t kPlaceableVTable64 = 0x830098;
constexpr uintptr_t kEntityDeleteRwObjectVtableOffset32 = 0x24;
constexpr uintptr_t kEntityDeleteRwObjectVtableOffset64 = 0x24 * 2;
constexpr uintptr_t kEntityPreRenderVtableOffset32 = 0x48;
constexpr uintptr_t kEntityPreRenderVtableOffset64 = 0x48 * 2;
constexpr uintptr_t kPhysicalAdd32 = 0x3FCE3C + 1;
constexpr uintptr_t kPhysicalAdd64 = 0x4E0608;
constexpr uintptr_t kPhysicalRemove32 = 0x3FD02C + 1;
constexpr uintptr_t kPhysicalRemove64 = 0x4E07EC;
constexpr uintptr_t kPedRender32 = 0x4A6964 + 1;
constexpr uintptr_t kPedRender64 = 0x59D3B8;
constexpr uintptr_t kVehicleAddUpgrade32 = 0x0058C66C + 1;
constexpr uintptr_t kVehicleAddUpgrade64 = 0x6AFF4C;
constexpr uintptr_t kVehicleRemoveUpgrade32 = 0x58CC2C + 1;
constexpr uintptr_t kVehicleRemoveUpgrade64 = 0x6B0718;
constexpr uintptr_t kVehicleSpecialColModel32 = 0x675F10;
constexpr uintptr_t kVehicleSpecialColModel64 = 0x849EA8;

using CalcScreenCoorsFn = void (*)(CVector*, CVector*, float*, float*, bool, bool);

uintptr_t GtaAddress(uintptr_t address32, uintptr_t address64)
{
    return g_libGTASA + (VER_x32 ? address32 : address64);
}

bool IsFiniteVector(const CVector& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

int32 WrapRepeatSectorIndex(int32 value, int32 max)
{
    const int32 wrapped = value % max;
    return wrapped < 0 ? wrapped + max : wrapped;
}

void RequestCollisionNative(const CVector& pos, int32 areaCode)
{
    Xyron::Perf::ScopedZone perfCollision(Xyron::Perf::Zone::Collision);
    CHook::CallFunction<void>(GtaAddress(kColRequestCollision32, kColRequestCollision64), &pos, areaCode);
}

void WriteGlobalPointer(uintptr_t address32, uintptr_t address64, void* value)
{
    if (!value) {
        return;
    }
    CHook::Write(GtaAddress(address32, address64), value);
}
}

namespace GTASAEngineApi {

CCamera& Camera()
{
    return *reinterpret_cast<CCamera*>(GtaAddress(kGameCamera32, kGameCamera64));
}

CCam& ActiveCamera()
{
    CCamera& camera = Camera();
    return camera.m_aCams[camera.m_nActiveCam];
}

float* AspectRatio()
{
    return reinterpret_cast<float*>(GtaAddress(kAspectRatio32, kAspectRatio64));
}

float* PlayerStats()
{
    return reinterpret_cast<float*>(GtaAddress(kPlayerStats32, kPlayerStats64));
}

uint8_t* CurrentPlayerIndex()
{
    return reinterpret_cast<uint8_t*>(GtaAddress(kCurrentPlayer32, kCurrentPlayer64));
}

CPickup* PickupsArrayNative()
{
    return reinterpret_cast<CPickup*>(GtaAddress(kPickupsArray32, kPickupsArray64));
}

void PickupGetRidOfObjects(CPickup* pickup)
{
    if (!pickup) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPickupGetRidOfObjects32, kPickupGetRidOfObjects64), pickup);
}

void PickupGiveUsObject(CPickup* pickup, CObjectGta** object, int32 slotIndex)
{
    if (!pickup) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPickupGiveUsObject32, kPickupGiveUsObject64),
                              pickup,
                              object,
                              slotIndex);
}

void PickupRemove(CPickup* pickup)
{
    if (!pickup) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPickupRemove32, kPickupRemove64), pickup);
}

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
                           bool reflectionDelay)
{
    CHook::CallFunction<void>(GtaAddress(kCoronaRegisterTexture32, kCoronaRegisterTexture64),
                              id,
                              attachTo,
                              red,
                              green,
                              blue,
                              alpha,
                              posn,
                              radius,
                              farClip,
                              texture,
                              flareType,
                              enableReflection,
                              checkObstacles,
                              paramNotUsed,
                              angle,
                              longDistance,
                              nearClip,
                              fadeState,
                              fadeSpeed,
                              onlyFromBelow,
                              reflectionDelay);
}

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
                        bool reflectionDelay)
{
    CHook::CallFunction<void>(GtaAddress(kCoronaRegisterType32, kCoronaRegisterType64),
                              id,
                              attachTo,
                              red,
                              green,
                              blue,
                              alpha,
                              posn,
                              radius,
                              farClip,
                              coronaType,
                              flareType,
                              enableReflection,
                              checkObstacles,
                              paramNotUsed,
                              angle,
                              longDistance,
                              nearClip,
                              fadeState,
                              fadeSpeed,
                              onlyFromBelow,
                              reflectionDelay);
}

void BindCoronasGlobals(float* sunScreenX, float* sunScreenY, bool* sunBlockedByClouds, uint32* moonSize)
{
    WriteGlobalPointer(kCoronaSunScreenX32, kCoronaSunScreenX64, sunScreenX);
    WriteGlobalPointer(kCoronaSunScreenY32, kCoronaSunScreenY64, sunScreenY);
    WriteGlobalPointer(kCoronaSunBlockedByClouds32, kCoronaSunBlockedByClouds64, sunBlockedByClouds);
    WriteGlobalPointer(kCoronaMoonSize32, kCoronaMoonSize64, moonSize);
}

void InitCamera(CCamera* camera)
{
    if (!camera) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kCameraInit32, kCameraInit64), camera);
}

void SetRwCamera(CCamera* camera, RwCamera* rwCamera)
{
    if (!camera) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kCameraSetRwCamera32, kCameraSetRwCamera64), camera, rwCamera);
}

void TakeCameraControl(CCamera* camera,
                       CEntityGTA* target,
                       eCamMode modeToGoTo,
                       eSwitchType switchType,
                       int32 whoIsInControlOfTheCamera)
{
    if (!camera) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kCameraTakeControl32, kCameraTakeControl64),
        camera,
        target,
        modeToGoTo,
        switchType,
        whoIsInControlOfTheCamera);
}

float CalculateCameraGroundHeight(CCamera* camera, eGroundHeightType type)
{
    if (!camera) {
        return 0.0f;
    }
    return CHook::CallFunction<float>(
        GtaAddress(kCameraCalculateGroundHeight32, kCameraCalculateGroundHeight64),
        camera,
        type);
}

void RestoreCameraWithJumpCut(CCamera* camera)
{
    if (!camera) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kCameraRestoreWithJumpCut32, kCameraRestoreWithJumpCut64), camera);
}

void ProcessCamera(CCamera* camera)
{
    if (!camera) {
        return;
    }
    Xyron::Perf::ScopedZone perfCamera(Xyron::Perf::Zone::Camera);
    CHook::CallFunction<void>(GtaAddress(kCameraProcess32, kCameraProcess64), camera);
}

void BindTimerGlobals(const TimerGlobals& globals)
{
    WriteGlobalPointer(kTimerCodePause32, kTimerCodePause64, globals.codePause);
    WriteGlobalPointer(kTimerFrameCounter32, kTimerFrameCounter64, globals.frameCounter);
    WriteGlobalPointer(kTimerGameFps32, kTimerGameFps64, globals.gameFps);
    WriteGlobalPointer(kTimerUserPause32, kTimerUserPause64, globals.userPause);
    WriteGlobalPointer(kTimerTimeScale32, kTimerTimeScale64, globals.timeScale);
    WriteGlobalPointer(kTimerTimeMs32, kTimerTimeMs64, globals.timeInMilliseconds);
    WriteGlobalPointer(kTimerSkipProcessFrame32, kTimerSkipProcessFrame64, globals.skipProcessThisFrame);
    WriteGlobalPointer(kTimerTimeStep32, kTimerTimeStep64, globals.timeStep);
    WriteGlobalPointer(kTimerPppPreviousTimeMs32, kTimerPppPreviousTimeMs64, globals.pppPreviousTimeInMilliseconds);
    WriteGlobalPointer(kTimerPpPreviousTimeMs32, kTimerPpPreviousTimeMs64, globals.ppPreviousTimeInMilliseconds);
    WriteGlobalPointer(kTimerPPreviousTimeMs32, kTimerPPreviousTimeMs64, globals.pPreviousTimeInMilliseconds);
    WriteGlobalPointer(kTimerPreviousTimeMs32, kTimerPreviousTimeMs64, globals.previousTimeInMilliseconds);
    WriteGlobalPointer(kTimerTimeMsNonClipped32, kTimerTimeMsNonClipped64, globals.timeInMillisecondsNonClipped);
    WriteGlobalPointer(kTimerPreviousTimeMsNonClipped32,
                       kTimerPreviousTimeMsNonClipped64,
                       globals.previousTimeInMillisecondsNonClipped);
}

uint8_t* TimerRunning()
{
    return reinterpret_cast<uint8_t*>(GtaAddress(kTimerRunning32, kTimerRunning64));
}

void InstallTimerControlHooks(void (*startUserPause)(),
                              void (*endUserPause)(),
                              void (*stop)(),
                              bool (*getIsSlowMotionActive)())
{
    if (startUserPause) {
        CHook::Redirect("_ZN6CTimer14StartUserPauseEv", startUserPause);
    }
    if (endUserPause) {
        CHook::Redirect("_ZN6CTimer12EndUserPauseEv", endUserPause);
    }
    if (stop) {
        CHook::Redirect("_ZN6CTimer4StopEv", stop);
    }
    if (getIsSlowMotionActive) {
        CHook::Redirect("_ZN6CTimer21GetIsSlowMotionActiveEv", getIsSlowMotionActive);
    }
}

void TimerSuspend()
{
    CHook::CallFunction<void>("_ZN6CTimer7SuspendEv");
}

void TimerResume()
{
    CHook::CallFunction<void>("_ZN6CTimer6ResumeEv");
}

uint32_t GetCyclesPerMillisecond()
{
    return CHook::CallFunction<uint32_t>(GtaAddress(kTimerCyclesPerMillisecond32, kTimerCyclesPerMillisecond64));
}

uint64_t GetCurrentTimeInCycles()
{
    return CHook::CallFunction<uint64_t>(GtaAddress(kTimerCurrentTimeInCycles32, kTimerCurrentTimeInCycles64));
}

void InitialiseMobileSettings()
{
    CHook::CallFunction<void>("_ZN14MobileSettings10InitializeEv");
}

void InitialiseLocalisation()
{
    CHook::CallFunction<void>("_ZN13CLocalisation10InitialiseEv");
}

void InitialisePad()
{
    CHook::CallFunction<void>("_ZN4CPad10InitialiseEv");
}

void SizeCamera(RwCamera* camera, RwRect* rect, RwReal viewWindow, RwReal aspectRatio)
{
    CHook::CallFunction<void>(GtaAddress(kCameraSize32, kCameraSize64), camera, rect, viewWindow, aspectRatio);
}

void DestroyCamera(RwCamera* camera)
{
    if (!camera) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kCameraDestroy32, kCameraDestroy64), camera);
}

void CreateLights(RpWorld* world)
{
    if (!world) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kLightsCreate32, kLightsCreate64), world);
}

void InitialiseHud()
{
    CHook::CallFunction<void>(GtaAddress(kHudInitialise32, kHudInitialise64));
}

void InitialisePlayerSkin()
{
    CHook::CallFunction<void>(GtaAddress(kPlayerSkinInitialise32, kPlayerSkinInitialise64));
}

void InitialisePostEffects()
{
    CHook::CallFunction<void>(GtaAddress(kPostEffectsInitialise32, kPostEffectsInitialise64));
}

void ApplyMaxStatsPatch()
{
    CHook::CallFunction<void>("_ZN6CCheat18VehicleSkillsCheatEv");
    CHook::CallFunction<void>("_ZN6CCheat17WeaponSkillsCheatEv");
    CHook::RET("_ZN6CStats12SetStatValueEtf");
}

void DisableCameraClearPlayerWeaponModePatch()
{
    CHook::NOP(
        GtaAddress(kCameraClearPlayerWeaponModePatch32, kCameraClearPlayerWeaponModePatch64),
        VER_x32 ? 2 : 1);
}

void PatchFrameLimiter(uint8_t fps)
{
#if VER_x32
    CHook::WriteMemory(GtaAddress(kFrameLimiterByteA32, 0), reinterpret_cast<uintptr_t>(&fps), 1);
    CHook::WriteMemory(GtaAddress(kFrameLimiterByteB32, 0), reinterpret_cast<uintptr_t>(&fps), 1);
#else
    const uint32_t safeFps = std::max<uint32_t>(1, fps);
    const auto movzW = [](uint32_t reg, uint32_t imm) -> uint32_t {
        return 0x52800000u | ((imm & 0xFFFFu) << 5u) | (reg & 0x1Fu);
    };
    const uint32_t movW9Fps = movzW(9u, safeFps);
    const uint32_t movW8Fps = movzW(8u, safeFps);
    CHook::WriteMemory(GtaAddress(0, kFrameLimiterMovW9A64), reinterpret_cast<uintptr_t>(&movW9Fps), sizeof(movW9Fps));
    CHook::WriteMemory(GtaAddress(0, kFrameLimiterMovW8B64), reinterpret_cast<uintptr_t>(&movW8Fps), sizeof(movW8Fps));
    CHook::WriteMemory(GtaAddress(0, kFrameLimiterMovW9C64), reinterpret_cast<uintptr_t>(&movW9Fps), sizeof(movW9Fps));
#endif
}

void RevealWholeNativeMap()
{
    std::memset(reinterpret_cast<void*>(GtaAddress(kZonesVisited32, kZonesVisited64)), 1, 100);
    *reinterpret_cast<uint32_t*>(GtaAddress(kZonesRevealed32, kZonesRevealed64)) = 100;
}

void PatchPlayerPedConstructorTaskFix()
{
#if VER_x32
    CHook::WriteMemory(GtaAddress(kPlayerPedCtorTaskFix32, 0), reinterpret_cast<uintptr_t>("\xE0"), 1);
#else
    CHook::WriteMemory(GtaAddress(0, kPlayerPedCtorTaskFix64), reinterpret_cast<uintptr_t>("\x34\x00\x80\x52"), 4);
#endif
}

void DisableNativeRadarBlipPatches()
{
    CHook::NOP(GtaAddress(kRadarDrawBlipsPatchA32, kRadarDrawBlipsPatchA64), 2);
    CHook::NOP(GtaAddress(kRadarDrawBlipsPatchB32, kRadarDrawBlipsPatchB64), 2);
}

void DisableNativeVehicleAudioProcessingPatches()
{
    CHook::NOP(GtaAddress(kVehicleAudioProcessingPatchA32, kVehicleAudioProcessingPatchA64), 2);
    CHook::NOP(GtaAddress(kVehicleAudioProcessingPatchB32, kVehicleAudioProcessingPatchB64), 2);
    CHook::NOP(GtaAddress(kVehicleAudioProcessingPatchC32, kVehicleAudioProcessingPatchC64), 2);
}

void DisableNativeMenuMapLegendPatches()
{
    CHook::NOP(GtaAddress(kMenuMapLegendTextPatch32, kMenuMapLegendTextPatch64), 2);
    CHook::NOP(GtaAddress(kMenuMapLegendIconPatch32, kMenuMapLegendIconPatch64), 2);
    CHook::NOP(GtaAddress(kMenuMapLegendAreaNamePatch32, kMenuMapLegendAreaNamePatch64), 2);
}

void SetClockTime(int hour, int minute)
{
    *reinterpret_cast<uint8_t*>(GtaAddress(kClockMinute32, kClockMinute64)) = static_cast<uint8_t>(minute);
    *reinterpret_cast<uint8_t*>(GtaAddress(kClockHour32, kClockHour64)) = static_cast<uint8_t>(hour);
}

void GetClockTime(int* hour, int* minute)
{
    if (minute) {
        *minute = *reinterpret_cast<uint8_t*>(GtaAddress(kClockMinute32, kClockMinute64));
    }
    if (hour) {
        *hour = *reinterpret_cast<uint8_t*>(GtaAddress(kClockHour32, kClockHour64));
    }
}

void SetWeatherNow(int weatherId)
{
    CHook::CallFunction<void>(GtaAddress(kWeatherSetNow32, kWeatherSetNow64), weatherId);
}

void ForceWeatherNow(int weatherId)
{
    const auto weather = static_cast<uint16_t>(weatherId);
    *reinterpret_cast<uint16_t*>(GtaAddress(kWeatherOld32, kWeatherOld64)) = weather;
    *reinterpret_cast<uint16_t*>(GtaAddress(kWeatherNew32, kWeatherNew64)) = weather;
}

bool SetGameClockMilliseconds(uint32_t milliseconds)
{
#if VER_x32
    *reinterpret_cast<uint32_t*>(g_libGTASA + kGameClockMilliseconds32) = milliseconds & 0x3FFFFFFF;
    return true;
#else
    (void)milliseconds;
    return false;
#endif
}

bool SetGravity(float gravity)
{
#if VER_x32
    const uintptr_t address = g_libGTASA + (VER_2_1 ? kGravity32Ver21 : kGravity32);
    CHook::UnFuck(address);
    *reinterpret_cast<float*>(address) = gravity;
    return true;
#else
    (void)gravity;
    return false;
#endif
}

bool IsGamePaused()
{
    return *reinterpret_cast<uint8_t*>(GtaAddress(kGamePaused32, kGamePaused64)) != 0;
}

void DrawRadarArea(float* position, uint32_t color, uint32_t unknown)
{
    CHook::CallFunction<void>(GtaAddress(kRadarDrawArea32, kRadarDrawArea64), position, &color, unknown);
}

void RemoveStreamingModel(int modelId)
{
    CHook::CallFunction<void>(GtaAddress(kStreamingRemoveModel32, kStreamingRemoveModel64), modelId);
}

void RemoveStreamingModelNative(int32 modelId)
{
    RemoveStreamingModel(modelId);
}

void DisableWeaponLockOnTarget()
{
    CHook::RET("_ZN10CPlayerPed22FindWeaponLockOnTargetEv");
    CHook::RET("_ZN10CPlayerPed26FindNextWeaponLockOnTargetEP7CEntityb");
    CHook::RET("_ZN4CPed21SetWeaponLockOnTargetEP7CEntity");
}

bool DisableEnterExits()
{
#if VER_x32
    uintptr_t manager = *reinterpret_cast<uintptr_t*>(g_libGTASA + (VER_2_1 ? kEntryExitManager32Ver21 : kEntryExitManager32));
    if (!manager) {
        return false;
    }

    const auto count = *reinterpret_cast<uint32_t*>(manager + 8);
    uintptr_t entries = *reinterpret_cast<uintptr_t*>(manager);
    if (!entries || count > 4096) {
        return false;
    }

    for (uint32_t i = 0; i < count; ++i) {
        *reinterpret_cast<uint16_t*>(entries + 0x30) = 0;
        entries += 0x3C;
    }
    return true;
#else
    return false;
#endif
}

void DisableCjWalkPatch()
{
    CHook::NOP(GtaAddress(kCjWalkPatch32, kCjWalkPatch64), 2);
}

void SetNativeHudVisible(bool visible)
{
    *reinterpret_cast<uint8_t*>(GtaAddress(kHudVisibleFlag32, kHudVisibleFlag64)) = visible ? 1 : 0;
    *reinterpret_cast<uint8_t*>(GtaAddress(kHudDisableFlag32, kHudDisableFlag64)) = visible ? 0 : 1;
}

void SetNativeHudDisabled(bool disabled)
{
    if (!g_libGTASA) {
        return;
    }
    *reinterpret_cast<uint8_t*>(GtaAddress(kHudDisableFlag32, kHudDisableFlag64)) = disabled ? 1 : 0;
}

void AddBigMessage(uint16_t* text, int time, int style)
{
    if (!text) {
        return;
    }

    CHook::CallFunction<void>(GtaAddress(kMessagesAddBig32, kMessagesAddBig64), text, time, style);
}

void BindGameGlobals(int32_t* currentArea, RwMatrix** workingMatrix1, RwMatrix** workingMatrix2)
{
    WriteGlobalPointer(kGameCurrentArea32, kGameCurrentArea64, currentArea);
    WriteGlobalPointer(kGameWorkingMatrix132, kGameWorkingMatrix164, workingMatrix1);
    WriteGlobalPointer(kGameWorkingMatrix232, kGameWorkingMatrix264, workingMatrix2);
}

int GetMobileEffectSettingNative()
{
    return CHook::CallFunction<int>("_Z22GetMobileEffectSettingv");
}

uint32_t GetSkinBoneUniformCountNative()
{
    return *reinterpret_cast<uint32_t*>(GtaAddress(kShaderSkinBoneCount32, kShaderSkinBoneCount64));
}

bool IsSpecularLightingDisabledNative()
{
    return *reinterpret_cast<uint8_t*>(GtaAddress(kShaderSpecularDisabled32, kShaderSpecularDisabled64)) != 0;
}

bool UsesHighSpecularExponentNative()
{
    return *reinterpret_cast<uint8_t*>(GtaAddress(kShaderHighSpecularExponent32, kShaderHighSpecularExponent64)) != 0;
}

void UpdatePads()
{
    CHook::CallFunction<void>(GtaAddress(kPadUpdatePads32, kPadUpdatePads64));
}

void ClearTouchInterface()
{
    CHook::CallFunction<void>(GtaAddress(kTouchInterfaceClear32, kTouchInterfaceClear64));
}

void UpdateHid()
{
    CHook::CallFunction<void>(GtaAddress(kHidUpdate32, kHidUpdate64));
}

void ProcessNativeInputFrame(bool updatePads, bool clearTouchInterface, bool updateHid)
{
    if (updatePads) {
        UpdatePads();
    }
    if (clearTouchInterface) {
        ClearTouchInterface();
    }
    if (updateHid) {
        UpdateHid();
    }
}

uintptr_t* MobileMenu()
{
    return reinterpret_cast<uintptr_t*>(GtaAddress(kMobileMenuGlobal32, kMobileMenuGlobal64));
}

bool IsNativeFrontEndMenuActive()
{
    if (!g_libGTASA) {
        return false;
    }

    const uintptr_t frontEndMenuManager = GtaAddress(kFrontEndMenuManager32, kFrontEndMenuManager64);
    const uintptr_t menuActiveOffset = VER_x32 ? kFrontEndMenuActiveOffset32 : kFrontEndMenuActiveOffset64;
    return *reinterpret_cast<uint8_t*>(frontEndMenuManager + menuActiveOffset) != 0;
}

bool IsNativeMobileMenuActive()
{
    if (!g_libGTASA) {
        return false;
    }

    const uintptr_t mobileMenu = reinterpret_cast<uintptr_t>(MobileMenu());
    const bool hasPendingScreens = *reinterpret_cast<uint32_t*>(mobileMenu + 0x24) != 0;
    const uintptr_t topScreenOffset = VER_x32 ? kMobileMenuTopScreenOffset32 : kMobileMenuTopScreenOffset64;
    const bool hasTopScreen = *reinterpret_cast<uintptr_t*>(mobileMenu + topScreenOffset) != 0;
    return hasPendingScreens || hasTopScreen;
}

void ForceCloseNativeMobileMenu()
{
    if (!g_libGTASA || !IsNativeMobileMenuActive()) {
        return;
    }

    const uintptr_t mobileMenu = reinterpret_cast<uintptr_t>(MobileMenu());
    const uintptr_t topScreenOffset = VER_x32 ? kMobileMenuTopScreenOffset32 : kMobileMenuTopScreenOffset64;

    CHook::CallFunction<void>(GtaAddress(kMobileMenuSwitchOffToGame32, kMobileMenuSwitchOffToGame64));
    CHook::CallFunction<void>(GtaAddress(kMobileMenuPopAllScreens32, kMobileMenuPopAllScreens64), mobileMenu);
    *reinterpret_cast<uint32_t*>(mobileMenu + 0x24) = 0;
    *reinterpret_cast<uintptr_t*>(mobileMenu + topScreenOffset) = 0;
}

void UpdateMobileMenu()
{
    CHook::CallFunction<void>(GtaAddress(kMobileMenuUpdate32, kMobileMenuUpdate64), MobileMenu());
}

void SetWindModifiersCount(int32_t count)
{
    *reinterpret_cast<int32_t*>(GtaAddress(kWindModifiersNumber32, kWindModifiersNumber64)) = count;
}

void InitSprite2dPerFrame()
{
    CHook::CallFunction<void>(GtaAddress(kSpriteInitPerFrame32, kSpriteInitPerFrame64));
}

void InitFontPerFrame()
{
    CHook::CallFunction<void>(GtaAddress(kFontInitPerFrame32, kFontInitPerFrame64));
}

void UpdateWeather()
{
    CHook::CallFunction<void>(GtaAddress(kWeatherUpdate32, kWeatherUpdate64));
}

void ProcessScripts()
{
    CHook::CallFunction<void>(GtaAddress(kScriptsProcess32, kScriptsProcess64));
}

void UpdateTrains()
{
    CHook::CallFunction<void>(GtaAddress(kTrainUpdate32, kTrainUpdate64));
}

void UpdateSkidmarks()
{
    CHook::CallFunction<void>(GtaAddress(kSkidmarksUpdate32, kSkidmarksUpdate64));
}

void UpdateGlass()
{
    CHook::CallFunction<void>(GtaAddress(kGlassUpdate32, kGlassUpdate64));
}

void UpdateFireManager()
{
    auto* fireManager = reinterpret_cast<uintptr_t*>(GtaAddress(kFireManagerGlobal32, kFireManagerGlobal64));
    CHook::CallFunction<void>(GtaAddress(kFireManagerUpdate32, kFireManagerUpdate64), fireManager);
}

void UpdatePopulation(bool generatePeds)
{
#if VER_x32
    CHook::CallFunction<void>(g_libGTASA + kPopulationUpdate32, generatePeds);
#else
    (void)generatePeds;
#endif
}

void UpdateWeapons()
{
    CHook::CallFunction<void>(GtaAddress(kWeaponUpdate32, kWeaponUpdate64));
}

void UpdateMovingThings()
{
    CHook::CallFunction<void>(GtaAddress(kMovingThingsUpdate32, kMovingThingsUpdate64));
}

void UpdateWaterCannons()
{
    CHook::CallFunction<void>(GtaAddress(kWaterCannonsUpdate32, kWaterCannonsUpdate64));
}

void ProcessWorld()
{
    Xyron::Perf::ScopedZone perfWorld(Xyron::Perf::Zone::WorldProcess);
    CHook::CallFunction<void>(GtaAddress(kWorldProcess32, kWorldProcess64));
}

void UpdateGarages()
{
    CHook::CallFunction<void>(GtaAddress(kGaragesUpdate32, kGaragesUpdate64));
}

void UpdateStuntJumps()
{
    CHook::CallFunction<void>(GtaAddress(kStuntJumpManagerUpdate32, kStuntJumpManagerUpdate64));
}

void UpdateBirds()
{
    CHook::CallFunction<void>(GtaAddress(kBirdsUpdate32, kBirdsUpdate64));
}

void UpdateSpecialFx()
{
    CHook::CallFunction<void>(GtaAddress(kSpecialFxUpdate32, kSpecialFxUpdate64));
}

void UpdatePostEffects()
{
    CHook::CallFunction<void>(GtaAddress(kPostEffectsUpdate32, kPostEffectsUpdate64));
}

void UpdateTimeCycle()
{
    CHook::CallFunction<void>(GtaAddress(kTimeCycleUpdate32, kTimeCycleUpdate64));
}

void UpdateGameLogic()
{
    CHook::CallFunction<void>(GtaAddress(kGameLogicUpdate32, kGameLogicUpdate64));
}

void DoSunAndMoon()
{
    CHook::CallFunction<void>(GtaAddress(kCoronasDoSunAndMoon32, kCoronasDoSunAndMoon64));
}

void UpdateCoronas()
{
    CHook::CallFunction<void>(GtaAddress(kCoronasUpdate32, kCoronasUpdate64));
}

void RenderCoronas()
{
    CHook::CallFunction<void>(GtaAddress(kCoronasRender32, kCoronasRender64));
}

void UpdatePermanentShadows()
{
    CHook::CallFunction<void>(GtaAddress(kShadowsUpdatePermanent32, kShadowsUpdatePermanent64));
}

void UpdateCustomBuildingRenderer()
{
    CHook::CallFunction<void>(GtaAddress(kCustomBuildingRendererUpdate32, kCustomBuildingRendererUpdate64));
}

bool InitialiseCustomBuildingRendererNative()
{
    return CHook::CallFunction<bool>("_ZN23CCustomBuildingRenderer10InitialiseEv");
}

uintptr_t* FxManager()
{
    return reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(GtaAddress(kFxManager32, kFxManager64)));
}

void UpdateFx(uintptr_t* fxManager, RwCamera* camera, float timeStep)
{
    if (!fxManager || !camera) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kFxUpdate32, kFxUpdate64), &fxManager, camera, timeStep);
}

void RenderFx(uintptr_t* fxManager, RwCamera* camera, bool heatHaze)
{
    if (!fxManager || !camera) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kFxRender32, kFxRender64), &fxManager, camera, heatHaze);
}

uintptr_t* BreakManager()
{
    return reinterpret_cast<uintptr_t*>(GtaAddress(kBreakManager32, kBreakManager64));
}

void UpdateBreakManager(uintptr_t* breakManager, float timeStep)
{
    if (!breakManager) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kBreakManagerUpdate32, kBreakManagerUpdate64), breakManager, timeStep);
}

void PreRenderWater()
{
    CHook::CallFunction<void>(GtaAddress(kWaterPreRender32, kWaterPreRender64));
}

void RenderNativeEffectStage(NativeRenderEffectStage stage)
{
    uintptr_t address32 = 0;
    uintptr_t address64 = 0;

    switch (stage) {
        case NativeRenderEffectStage::Stage0:
            address32 = kRenderEffectStage0_32;
            address64 = kRenderEffectStage0_64;
            break;
        case NativeRenderEffectStage::Stage1:
            address32 = kRenderEffectStage1_32;
            address64 = kRenderEffectStage1_64;
            break;
        case NativeRenderEffectStage::Stage2:
            address32 = kRenderEffectStage2_32;
            address64 = kRenderEffectStage2_64;
            break;
        case NativeRenderEffectStage::Stage3:
            address32 = kRenderEffectStage3_32;
            address64 = kRenderEffectStage3_64;
            break;
        case NativeRenderEffectStage::Stage4:
            address32 = kRenderEffectStage4_32;
            address64 = kRenderEffectStage4_64;
            break;
        case NativeRenderEffectStage::Stage5:
            address32 = kRenderEffectStage5_32;
            address64 = kRenderEffectStage5_64;
            break;
        case NativeRenderEffectStage::Stage6:
            address32 = kRenderEffectStage6_32;
            address64 = kRenderEffectStage6_64;
            break;
        case NativeRenderEffectStage::Stage7:
            address32 = kRenderEffectStage7_32;
            address64 = kRenderEffectStage7_64;
            break;
        case NativeRenderEffectStage::Stage8:
            address32 = kRenderEffectStage8_32;
            address64 = kRenderEffectStage8_64;
            break;
        case NativeRenderEffectStage::Stage9:
            address32 = kRenderEffectStage9_32;
            address64 = kRenderEffectStage9_64;
            break;
    }

    if (!address32 || !address64) {
        return;
    }

    CHook::CallFunction<void>(GtaAddress(address32, address64));
}

void DrawNativeHud()
{
    CHook::CallFunction<void>("_ZN4CHud4DrawEv");
}

bool IsAltRenderTarget()
{
    return CHook::CallFunction<bool>(GtaAddress(kAltRenderTargetCheck32, kAltRenderTargetCheck64));
}

void FlushAltRenderTarget()
{
    CHook::CallFunction<void>(GtaAddress(kAltRenderTargetFlush32, kAltRenderTargetFlush64));
}

void DrawNativeTouchInterface(bool draw)
{
    CHook::CallFunction<void>(GtaAddress(kTouchInterfaceDrawAll32, kTouchInterfaceDrawAll64), draw);
}

void SetEmuGamma(bool enabled)
{
    CHook::CallFunction<void>("_Z12emu_GammaSeth", enabled ? 1 : 0);
}

void DisplayMessages(bool immediate)
{
    CHook::CallFunction<void>(GtaAddress(kMessagesDisplay32, kMessagesDisplay64), immediate);
}

void RenderFontBuffer()
{
    CHook::CallFunction<void>(GtaAddress(kFontRenderBuffer32, kFontRenderBuffer64));
}

void InstallRender2dHookNative(uintptr_t hook)
{
    CHook::Redirect("_Z13Render2dStuffv", reinterpret_cast<void*>(hook));
}

void InstallRQShaderBuildSourceHookNative(uintptr_t hook)
{
    CHook::Redirect("_ZN8RQShader11BuildSourceEjPPKcS2_", reinterpret_cast<void*>(hook));
}

void InstallAndroidTouchEventHookNative(uintptr_t hook, void** original)
{
    CHook::InlineHook("_Z14AND_TouchEventiiii", reinterpret_cast<void*>(hook), original);
}

void BindMultiTouchPointerArrays(void* pointsArray, void* pointersArray)
{
    WriteGlobalPointer(kMultiTouchPointsArray32, kMultiTouchPointsArray64, pointsArray);
    WriteGlobalPointer(kMultiTouchPointersArray32, kMultiTouchPointersArray64, pointersArray);
}

void PatchMultiTouchPointerLimits()
{
#if VER_x32
    CHook::WriteMemory(GtaAddress(kPointerGetNumberPatch32, kPointerGetNumberPatch64),
                       reinterpret_cast<uintptr_t>("\x03\x20"),
                       2);
    CHook::WriteMemory(GtaAddress(kPointerGetTypePatch32, kPointerGetTypePatch64),
                       reinterpret_cast<uintptr_t>("\x03\x28"),
                       2);
    CHook::WriteMemory(GtaAddress(kPointerGetCoordinatesPatch32, kPointerGetCoordinatesPatch64),
                       reinterpret_cast<uintptr_t>("\x03\x28"),
                       2);
    CHook::WriteMemory(GtaAddress(kPointerGetWheelPatch32, kPointerGetWheelPatch64),
                       reinterpret_cast<uintptr_t>("\x03\x28"),
                       2);
    CHook::WriteMemory(GtaAddress(kPointerDoubleClickedPatch32, kPointerDoubleClickedPatch64),
                       reinterpret_cast<uintptr_t>("\x03\x28"),
                       2);
    CHook::WriteMemory(GtaAddress(kPointerGetButtonPatch32, kPointerGetButtonPatch64),
                       reinterpret_cast<uintptr_t>("\x03\x2A"),
                       2);
#else
    CHook::WriteMemory(GtaAddress(kPointerGetNumberPatch32, kPointerGetNumberPatch64),
                       reinterpret_cast<uintptr_t>("\x60\x00\x80\x52"),
                       4);
    CHook::WriteMemory(GtaAddress(kPointerGetTypePatch32, kPointerGetTypePatch64),
                       reinterpret_cast<uintptr_t>("\x1F\x0C\x00\x71"),
                       4);
    CHook::WriteMemory(GtaAddress(kPointerGetCoordinatesPatch32, kPointerGetCoordinatesPatch64),
                       reinterpret_cast<uintptr_t>("\x1F\x0C\x00\x71"),
                       4);
    CHook::WriteMemory(GtaAddress(kPointerGetWheelPatch32, kPointerGetWheelPatch64),
                       reinterpret_cast<uintptr_t>("\x1F\x0C\x00\x71"),
                       4);
    CHook::WriteMemory(GtaAddress(kPointerDoubleClickedPatch32, kPointerDoubleClickedPatch64),
                       reinterpret_cast<uintptr_t>("\x1F\x0C\x00\x71"),
                       4);
    CHook::WriteMemory(GtaAddress(kPointerGetButtonPatch32, kPointerGetButtonPatch64),
                       reinterpret_cast<uintptr_t>("\x1F\x0D\x00\x71"),
                       4);
#endif
}

void InstallHudColourHooksNative(uintptr_t hudColourHook, uintptr_t radarTraceColourHook)
{
    CHook::Redirect("_ZN11CHudColours12GetIntColourEh", reinterpret_cast<void*>(hudColourHook));
    CHook::Redirect("_ZN6CRadar19GetRadarTraceColourEjhh", reinterpret_cast<void*>(radarTraceColourHook));
}

void InstallHudDrawRadarHook(HudDrawRadarFn hook, HudDrawRadarFn* original)
{
    CHook::InlineHook(GtaAddress(kHudDrawRadar32, kHudDrawRadar64), hook, original);
}

void InstallRadarCoordBlipHookNative(uintptr_t hook, void** original)
{
    CHook::InlineHook("_ZN6CRadar12SetCoordBlipE9eBlipType7CVectorj12eBlipDisplayPc",
                      reinterpret_cast<void*>(hook),
                      original);
}

void InstallRadarGangOverlayHookNative(uintptr_t hook, void** original)
{
    CHook::InlineHook("_ZN6CRadar20DrawRadarGangOverlayEb", reinterpret_cast<void*>(hook), original);
}

void InstallRadarClearBlipHookNative(uintptr_t hook, void** original)
{
    CHook::InlineHook("_ZN6CRadar9ClearBlipEi", reinterpret_cast<void*>(hook), original);
}

void InstallCrosshairHookNative(uintptr_t hook, void** original)
{
    CHook::InlineHook("_ZN4CHud14DrawCrossHairsEv", reinterpret_cast<void*>(hook), original);
}

void InstallInputTypeHookNative(uintptr_t hook)
{
    CHook::Redirect("_ZN4CHID12GetInputTypeEv", reinterpret_cast<void*>(hook));
}

void InstallRenderEffectsHookNative(uintptr_t hook)
{
    if (!hook) {
        return;
    }
    CHook::Redirect("_Z13RenderEffectsv", reinterpret_cast<void*>(hook));
}

void InstallTextureGetHookNative(uintptr_t hook)
{
    if (!hook) {
        return;
    }
    CHook::Redirect("_Z10GetTexturePKc", reinterpret_cast<void*>(hook));
}

void InstallTextureListingMipCountHookNative(uintptr_t hook)
{
    if (!hook) {
        return;
    }
    CHook::Redirect("_ZNK14TextureListing11GetMipCountEv", reinterpret_cast<void*>(hook));
}

void InstallRqSetAlphaTestHookNative(uintptr_t hook)
{
    if (!hook) {
        return;
    }
    CHook::Redirect("_Z25RQ_Command_rqSetAlphaTestRPc", reinterpret_cast<void*>(hook));
}

void InstallRqVertexBufferDeleteHookNative(uintptr_t hook)
{
    if (!hook) {
        return;
    }
    CHook::Redirect("_Z31RQ_Command_rqVertexBufferDeleteRPc", reinterpret_cast<void*>(hook));
}

void InstallRenderPipelineInlineHooksNative(const NativeInlineHook* hooks, size_t hookCount)
{
    if (!hooks || hookCount == 0) {
        return;
    }

    for (size_t i = 0; i < hookCount; ++i) {
        const NativeInlineHook& hook = hooks[i];
        if (!hook.symbol || !hook.hook || !hook.original) {
            continue;
        }
        CHook::InlineHook(hook.symbol, reinterpret_cast<void*>(hook.hook), hook.original);
    }
}

void InstallSingleplayerDeadWeightHooksNative(const NativeInlineHook* hooks, size_t hookCount)
{
    if (!hooks || hookCount == 0) {
        return;
    }

    for (size_t i = 0; i < hookCount; ++i) {
        const NativeInlineHook& hook = hooks[i];
        if (!hook.symbol || !hook.hook || !hook.original) {
            continue;
        }
        CHook::InlineHook(hook.symbol, reinterpret_cast<void*>(hook.hook), hook.original);
    }
}

void DisableNativePopulationInitialise()
{
    CHook::RET("_ZN11CPopulation10InitialiseEv");
}

void DisableNativePlayerSkinLoad()
{
    CHook::RET("_ZN11CPlayerInfo14LoadPlayerSkinEv");
}

void DisableNativeStoredShadowRenderer()
{
    CHook::RET("_ZN8CShadows19RenderStoredShadowsEb");
}

void DisableAmbientPopulationAndTrafficNative()
{
    CHook::RET("_ZN11CPopulation6AddPedE8ePedTypejRK7CVectorb");
    CHook::RET("_ZN6CPlane27DoPlaneGenerationAndRemovalEv");
    CHook::RET("_ZN10CEntryExit19GenerateAmbientPedsERK7CVector");
    CHook::RET("_ZN8CCarCtrl31GenerateOneEmergencyServicesCarEj7CVector");
    CHook::RET("_ZN11CPopulation17AddPedAtAttractorEiP9C2dEffect7CVectorP7CEntityi");
}

void InstallRadarStreamSectionsHook(RadarStreamSectionsFn hook, RadarStreamSectionsFn* original)
{
#if !VER_x32
    CHook::InlineHook(g_libGTASA + kRadarStreamSections64, hook, original);
#else
    (void)hook;
    (void)original;
#endif
}

uintptr_t NativePltAddress(NativePltSlot slot)
{
    switch (slot) {
        case NativePltSlot::RqVertexBufferSelect:
            return g_libGTASA + kPltRqVertexBufferSelect;
        case NativePltSlot::RqVertexBufferDelete:
            return g_libGTASA + kPltRqVertexBufferDelete;
        case NativePltSlot::CustomRoadsignRenderAtomic:
            return g_libGTASA + kPltCustomRoadsignRenderAtomic;
        case NativePltSlot::RwTextureDestroy:
            return g_libGTASA + kPltRwTextureDestroy;
        case NativePltSlot::PedUpdatePosition:
            return g_libGTASA + kPltPedUpdatePosition;
        case NativePltSlot::RwFrameAddChild:
            return g_libGTASA + kPltRwFrameAddChild;
        case NativePltSlot::TextureDatabaseRuntimeGetEntry:
            return g_libGTASA + kPltTextureDatabaseRuntimeGetEntry;
        case NativePltSlot::RpMaterialListDeinitialize:
            return g_libGTASA + kPltRpMaterialListDeinitialize;
        case NativePltSlot::AnimManagerUncompressAnimation:
            return g_libGTASA + kPltAnimManagerUncompressAnimation;
        case NativePltSlot::TaskComplexLeaveCarPrimary:
            return g_libGTASA + kPltTaskComplexLeaveCarPrimary;
        case NativePltSlot::TaskComplexLeaveCarSecondary:
            return g_libGTASA + kPltTaskComplexLeaveCarSecondary;
        case NativePltSlot::FindPlayerSpeed:
            return g_libGTASA + kPltFindPlayerSpeed;
        case NativePltSlot::TxdStoreFindCb:
            return g_libGTASA + kPltTxdStoreFindCb;
        case NativePltSlot::CameraProcess:
            return g_libGTASA + kPltCameraProcess;
        case NativePltSlot::VehicleModelSetupCommonData:
            return g_libGTASA + kPltVehicleModelSetupCommonData;
        case NativePltSlot::VehicleAudioSettings:
            return g_libGTASA + kPltVehicleAudioSettings;
        case NativePltSlot::RadarClearBlip:
            return g_libGTASA + kPltRadarClearBlip;
        case NativePltSlot::PedGetWeaponSkill:
            return g_libGTASA + kPltPedGetWeaponSkill;
        case NativePltSlot::StartGameScreenOnNewGameCheck:
            return GtaAddress(kPltStartGameScreenOnNewGameCheck32, kPltStartGameScreenOnNewGameCheck64);
        case NativePltSlot::RleDecompress:
            return GtaAddress(kPltRleDecompress32, kPltRleDecompress64);
    }

    return 0;
}

void InstallNativePltHook(NativePltSlot slot, uintptr_t hookFunction, uintptr_t* originalFunction)
{
    const uintptr_t address = NativePltAddress(slot);
    if (!address) {
        return;
    }

    CHook::InstallPLT(address, hookFunction, originalFunction);
}

void InstallNativePltHook(NativePltSlot slot, uintptr_t hookFunction)
{
    const uintptr_t address = NativePltAddress(slot);
    if (!address) {
        return;
    }

    CHook::InstallPLT(address, hookFunction);
}

uintptr_t FindTextureInNativeTexDictionaries(const char* name, const char* const* texdbs, size_t texdbCount)
{
#if VER_x32
    if (!name || !texdbs || !texdbCount) {
        return 0;
    }

    for (size_t i = 0; i < texdbCount; ++i) {
        const char* texdb = texdbs[i];
        if (!texdb) {
            continue;
        }

        uintptr_t db = CHook::CallFunction<uintptr_t>(g_libGTASA + kTextureDatabaseGetDatabase32, texdb);
        const uint32_t registeredCount = *reinterpret_cast<uint32_t*>(g_libGTASA + kTextureDatabaseRegisteredCount32);
        bool alreadyRegistered = false;
        if (registeredCount) {
            const uintptr_t registeredList = *reinterpret_cast<uintptr_t*>(g_libGTASA + kTextureDatabaseRegisteredList32);
            for (uint32_t index = 0; registeredList && index < registeredCount; ++index) {
                if (*reinterpret_cast<uint32_t*>(registeredList + 4 * index) == db) {
                    alreadyRegistered = true;
                    break;
                }
            }
        }

        if (alreadyRegistered) {
            continue;
        }

        CHook::CallFunction<void>(g_libGTASA + kTextureDatabaseRegister32, db);
        uintptr_t texture = CHook::CallFunction<uintptr_t>(g_libGTASA + kTextureDatabaseGetTexture32, name);
        CHook::CallFunction<void>(g_libGTASA + kTextureDatabaseUnregister32, db);
        if (texture) {
            return texture;
        }
    }

    int current = CHook::CallFunction<int>(g_libGTASA + kRwTexDictionaryGetCurrent32);
    while (current) {
        uintptr_t texture = CHook::CallFunction<uintptr_t>(g_libGTASA + kRwTexDictionaryFindNamedTexture32,
                                                           current,
                                                           name);
        if (texture) {
            return texture;
        }
        current = CHook::CallFunction<int>(g_libGTASA + kTxdStoreGetTxdParent32, current);
    }
#else
    (void)name;
    (void)texdbs;
    (void)texdbCount;
#endif

    return 0;
}

char* StorageRootBuffer()
{
    return reinterpret_cast<char*>(GtaAddress(kStorageRootBuffer32, kStorageRootBuffer64));
}

void SetFileManagerDir(const char* path)
{
    CHook::CallFunction<void>(GtaAddress(kFileMgrSetDir32, kFileMgrSetDir64), path);
}

uintptr_t ZipFileCreateNative(const char* path)
{
    return CHook::CallFunction<uintptr_t>("_Z14ZIP_FileCreatePKc", path);
}

bool ZipAddStorageNative(uintptr_t zipFile)
{
    return CHook::CallFunction<bool>("_Z14ZIP_AddStorageP7ZIPFile", zipFile);
}

void InitialiseFileManagerNative()
{
    CHook::CallFunction<void>("_ZN8CFileMgr10InitialiseEv");
}

void InitialiseMemoryMgrNative()
{
    CHook::CallFunction<void>("_ZN10CMemoryMgr4InitEv");
}

void CdStreamInitNative(int32 streamCount)
{
    CHook::CallFunction<void>("_Z12CdStreamIniti", streamCount);
}

int32 CdStreamOpenNative(const char* fileName, bool notPlayerImg)
{
    return CHook::CallFunction<int32>(GtaAddress(kCdStreamOpen32, kCdStreamOpen64), fileName, notPlayerImg);
}

int32 CdStreamSyncNative(int32 streamId)
{
    return CHook::CallFunction<int32>(GtaAddress(kCdStreamSync32, kCdStreamSync64), streamId);
}

int32 CdStreamGetStatusNative(int32 streamId)
{
    return CHook::CallFunction<int32>(GtaAddress(kCdStreamGetStatus32, kCdStreamGetStatus64), streamId);
}

bool CdStreamReadNative(int32 streamId, void* buffer, uint32 offsetAndHandle, int32 sectorCount)
{
    return CHook::CallFunction<bool>(GtaAddress(kCdStreamRead32, kCdStreamRead64),
                                     streamId,
                                     buffer,
                                     offsetAndHandle,
                                     sectorCount);
}

uint32 CdStreamGetLastPositionNative()
{
    return CHook::CallFunction<uint32>(GtaAddress(kCdStreamGetLastPosn32, kCdStreamGetLastPosn64));
}

char* FileLoaderLoadLineFromBufferNative(char* bufferIt, int32 buffSize)
{
    return CHook::CallFunction<char*>(
        GtaAddress(kFileLoaderLoadLineBuffer32, kFileLoaderLoadLineBuffer64),
        bufferIt,
        buffSize);
}

int32 FileLoaderLoadObjectNative(const char* line)
{
    return CHook::CallFunction<int32>(
        GtaAddress(kFileLoaderLoadObject32, kFileLoaderLoadObject64),
        line);
}

CEntityGTA* FileLoaderLoadObjectInstanceNative(CFileObjectInstance* objInstance, const char* modelName)
{
    return CHook::CallFunction<CEntityGTA*>(
        GtaAddress(kFileLoaderLoadObjectInstance32, kFileLoaderLoadObjectInstance64),
        objInstance,
        modelName);
}

void InstallFileLoaderHooksNative(uintptr_t loadObjectHook,
                                  void** loadObjectOriginal,
                                  uintptr_t loadObjectInstanceHook,
                                  void** loadObjectInstanceOriginal)
{
    CHook::InlineHook("_ZN11CFileLoader10LoadObjectEPKc",
                      reinterpret_cast<void*>(loadObjectHook),
                      loadObjectOriginal);
    CHook::InlineHook("_ZN11CFileLoader18LoadObjectInstanceEPKc",
                      reinterpret_cast<void*>(loadObjectInstanceHook),
                      loadObjectInstanceOriginal);
}

void BindAuxiliaryPoolsNative(void* colModelPool,
                              void* eventPool,
                              void* pointRoutePool,
                              void* patrolRoutePool,
                              void* nodeRoutePool,
                              void* taskAllocatorPool,
                              void* pedIntelligencePool,
                              void* pedAttractorPool)
{
    *reinterpret_cast<void**>(GtaAddress(kPoolColModel32, kPoolColModel64)) = colModelPool;
    *reinterpret_cast<void**>(GtaAddress(kPoolEvent32, kPoolEvent64)) = eventPool;
    *reinterpret_cast<void**>(GtaAddress(kPoolPointRoute32, kPoolPointRoute64)) = pointRoutePool;
    *reinterpret_cast<void**>(GtaAddress(kPoolPatrolRoute32, kPoolPatrolRoute64)) = patrolRoutePool;
    *reinterpret_cast<void**>(GtaAddress(kPoolNodeRoute32, kPoolNodeRoute64)) = nodeRoutePool;
    *reinterpret_cast<void**>(GtaAddress(kPoolTaskAllocator32, kPoolTaskAllocator64)) = taskAllocatorPool;
    *reinterpret_cast<void**>(GtaAddress(kPoolPedIntelligence32, kPoolPedIntelligence64)) = pedIntelligencePool;
    *reinterpret_cast<void**>(GtaAddress(kPoolPedAttractor32, kPoolPedAttractor64)) = pedAttractorPool;
}

void InstallPoolsHooksNative(uintptr_t initialiseHook,
                             void* buildingPoolRef,
                             void* dummyPoolRef,
                             void* entryInfoNodePoolRef,
                             void* ptrNodeSingleLinkPoolRef,
                             void* ptrNodeDoubleLinkPoolRef,
                             void* pedPoolRef,
                             void* vehiclePoolRef,
                             void* objectPoolRef,
                             void* taskPoolRef)
{
    CHook::Redirect("_ZN6CPools10InitialiseEv", reinterpret_cast<void*>(initialiseHook));

    CHook::Write(GtaAddress(kPoolBuilding32, kPoolBuilding64), buildingPoolRef);
    CHook::Write(GtaAddress(kPoolDummy32, kPoolDummy64), dummyPoolRef);
    CHook::Write(GtaAddress(kPoolEntryInfoNode32, kPoolEntryInfoNode64), entryInfoNodePoolRef);
    CHook::Write(GtaAddress(kPoolPtrNodeSingleLink32, kPoolPtrNodeSingleLink64), ptrNodeSingleLinkPoolRef);
    CHook::Write(GtaAddress(kPoolPtrNodeDoubleLink32, kPoolPtrNodeDoubleLink64), ptrNodeDoubleLinkPoolRef);
    CHook::Write(GtaAddress(kPoolPed32, kPoolPed64), pedPoolRef);
    CHook::Write(GtaAddress(kPoolVehicle32, kPoolVehicle64), vehiclePoolRef);
    CHook::Write(GtaAddress(kPoolObject32, kPoolObject64), objectPoolRef);
    CHook::Write(GtaAddress(kPoolTask32, kPoolTask64), taskPoolRef);
}

int32 TxdStoreGetNumRefsNative(int32 index)
{
    return CHook::CallFunction<int32>(GtaAddress(kTxdStoreGetNumRefs32, kTxdStoreGetNumRefs64), index);
}

void TxdStoreRemoveTxdNative(int32 index)
{
    CHook::CallFunction<void>(GtaAddress(kTxdStoreRemoveTxd32, kTxdStoreRemoveTxd64), index);
}

int32 TxdStoreFindSlotNative(const char* name)
{
    return CHook::CallFunction<int32>(GtaAddress(kTxdStoreFindSlot32, kTxdStoreFindSlot64), name);
}

int32 TxdStoreAddSlotNative(const char* name, const char* dbName, bool keepCPU)
{
    return CHook::CallFunction<int32>(GtaAddress(kTxdStoreAddSlot32, kTxdStoreAddSlot64), name, dbName, keepCPU);
}

void TxdStoreInitialiseNative()
{
    CHook::CallFunction<void>(GtaAddress(kTxdStoreInitialise32, kTxdStoreInitialise64));
}

void TxdStorePushCurrentNative()
{
    CHook::CallFunction<void>(GtaAddress(kTxdStorePushCurrent32, kTxdStorePushCurrent64));
}

void TxdStorePopCurrentNative()
{
    CHook::CallFunction<void>(GtaAddress(kTxdStorePopCurrent32, kTxdStorePopCurrent64));
}

void TxdStoreSetCurrentNative(int32 index)
{
    CHook::CallFunction<void>(GtaAddress(kTxdStoreSetCurrent32, kTxdStoreSetCurrent64), index, nullptr);
}

void SetDrawFov(float fov)
{
    *reinterpret_cast<float*>(GtaAddress(kDrawFov32, kDrawFov64)) = fov;
}

void ResetRqVertexBufferState()
{
#if VER_x32
    *reinterpret_cast<uint32_t*>(g_libGTASA + kRqVertexBufferState32) = 0;
#endif
}

float NativeTimeStep()
{
#if VER_x32
    return *reinterpret_cast<float*>(g_libGTASA + kNativeTimeStep32);
#else
    return 0.0f;
#endif
}

float NativeRendererTimeStep()
{
#if VER_x32
    return *reinterpret_cast<float*>(g_libGTASA + kNativeRendererTimeStep32);
#else
    return 0.0f;
#endif
}

void SetLegacyStreamingInitMemoryBudget(uint32_t bytes)
{
#if VER_x32
    *reinterpret_cast<uint32_t*>(g_libGTASA + kLegacyStreamingInitBudget32) = bytes;
#else
    (void)bytes;
#endif
}

void PatchObjectDestructorBridgeGuard()
{
#if !VER_x32
    CHook::Write(g_libGTASA + kObjectDestructorBridgePatch64, 0x14000012);
#endif
}

void PatchRendererFrameTiming()
{
#if VER_x32
    CHook::UnFuck(g_libGTASA + kNativeRendererTimeStep32);
    *reinterpret_cast<float*>(g_libGTASA + kNativeRendererTimeStep32) = 0.015f;
#else
    CHook::Write(g_libGTASA + kRendererFrameTimingPatch64A, 0x90000AA9);
    CHook::Write(g_libGTASA + kRendererFrameTimingPatch64B, 0xBD48D521);
#endif
}

uintptr_t CreateTask()
{
    return CHook::CallFunction<uintptr_t>(GtaAddress(kTaskOperatorNew32, kTaskOperatorNew64));
}

void ConstructEnterCarAsDriverTask(uintptr_t task, uintptr_t vehicle)
{
    CHook::CallFunction<void>(GtaAddress(kEnterCarAsDriverCtor32, kEnterCarAsDriverCtor64), task, vehicle);
}

void SetTaskManagerTask(uintptr_t taskManager, uintptr_t task, int priority, int argument)
{
    CHook::CallFunction<int>(GtaAddress(kTaskManagerSetTask32, kTaskManagerSetTask64),
                             taskManager,
                             task,
                             priority,
                             argument);
}

uint32_t GetPedWeaponSkill(CPedGTA* ped, uint32_t weaponType)
{
    return CHook::CallFunction<uint32_t>(g_libGTASA + kPedGetWeaponSkillLegacy, ped, weaponType);
}

RwFrame* GetFrameFromId(RpClump* clump, int id)
{
#if VER_x32
    return CHook::CallFunction<RwFrame*>(g_libGTASA + (VER_2_1 ? kClumpModelInfoGetFrameFromId32Ver21
                                                               : kClumpModelInfoGetFrameFromId32),
                                        clump,
                                        id);
#else
    (void)clump;
    (void)id;
    return nullptr;
#endif
}

bool ApplyPhysicalCollision(CPhysical* physical, CEntityGTA* entity, CColPoint& colPoint, float& damageIntensity)
{
#if !VER_x32
    using ApplyCollisionFn = bool (*)(CPhysical*, CEntityGTA*, CColPoint&, float&);
    return reinterpret_cast<ApplyCollisionFn>(g_libGTASA + kApplyCollision64)(physical,
                                                                              entity,
                                                                              colPoint,
                                                                              damageIntensity);
#else
    (void)physical;
    (void)entity;
    (void)colPoint;
    (void)damageIntensity;
    return false;
#endif
}

bool ApplyPhysicalFriction(CPhysical* physical, float friction, CColPoint& colPoint)
{
#if !VER_x32
    using ApplyFrictionFn = bool (*)(CPhysical*, float, CColPoint&);
    return reinterpret_cast<ApplyFrictionFn>(g_libGTASA + kApplyFriction64)(physical, friction, colPoint);
#else
    (void)physical;
    (void)friction;
    (void)colPoint;
    return false;
#endif
}

void RenderStaticRoadEntity(CEntityGTA* entity)
{
#if !VER_x32
    CHook::CallFunction<void>(g_libGTASA + kRenderRoadEntity64, entity);
#else
    (void)entity;
#endif
}

void RenderStaticNonRoadEntity(CEntityGTA* entity)
{
#if !VER_x32
    CHook::CallFunction<void>(g_libGTASA + kRenderNonRoadEntity64, entity);
#else
    (void)entity;
#endif
}

uint32_t* StreamingMemoryAvailableNative()
{
#if !VER_x32
    return reinterpret_cast<uint32_t*>(g_libGTASA + kStreamingMemoryAvailableNative64);
#else
    return nullptr;
#endif
}

bool GetRendererListDebugState(RendererListDebugState& out)
{
#if !VER_x32
    out.nativeVisible = *reinterpret_cast<int32_t*>(g_libGTASA + kRendererNativeVisible64);
    out.nativeLods = *reinterpret_cast<int32_t*>(g_libGTASA + kRendererNativeLods64);
    out.nativeInvisible = *reinterpret_cast<int32_t*>(g_libGTASA + kRendererNativeInvisible64);
    out.nativeSuperLods = *reinterpret_cast<int32_t*>(g_libGTASA + kRendererNativeSuperLods64);
    out.visibleListPtr = *reinterpret_cast<uintptr_t*>(g_libGTASA + kRendererGotVisibleList64);
    out.visibleCountPtr = *reinterpret_cast<uintptr_t*>(g_libGTASA + kRendererGotVisibleCount64);
    out.lodListPtr = *reinterpret_cast<uintptr_t*>(g_libGTASA + kRendererGotLodList64);
    out.lodCountPtr = *reinterpret_cast<uintptr_t*>(g_libGTASA + kRendererGotLodCount64);
    return true;
#else
    (void)out;
    return false;
#endif
}

uint16_t WorldCurrentScanCode()
{
#if !VER_x32
    return *reinterpret_cast<uint16_t*>(g_libGTASA + kWorldCurrentScanCode64);
#else
    return 0;
#endif
}

CEntityGTA* WorldIgnoreEntity()
{
#if !VER_x32
    return *reinterpret_cast<CEntityGTA**>(g_libGTASA + kWorldIgnoreEntity64);
#else
    return nullptr;
#endif
}

BikeSetupModelNodesFn BikeSetupModelNodes()
{
#if !VER_x32
    return reinterpret_cast<BikeSetupModelNodesFn>(g_libGTASA + kBikeSetupModelNodes64);
#else
    return nullptr;
#endif
}

void RunNewGameCheck()
{
    CHook::CallFunction<void>(GtaAddress(kNewGameCheck32, kNewGameCheck64));
}

void BindSceneGlobal(void* scene)
{
    CHook::Write(GtaAddress(kSceneGlobal32, kSceneGlobal64), scene);
}

void BindOcclusionGlobals(void* occluders, void* occluderCount)
{
    CHook::Write(GtaAddress(kOcclusionArray32, kOcclusionArray64), occluders);
    CHook::Write(GtaAddress(kOcclusionCount32, kOcclusionCount64), occluderCount);
}

void BindBoundingBoxGlobals(void* failedCount)
{
    CHook::Write(GtaAddress(kBoundingBoxFailedCount32, kBoundingBoxFailedCount64), failedCount);
}

void BindMatrixGlobals(void* matrixCount)
{
    CHook::Write(GtaAddress(kMatrixCount32, kMatrixCount64), matrixCount);
}

void BindMatrixLinkListGlobal(void* matrixList)
{
    CHook::Write(GtaAddress(kMatrixLinkList32, kMatrixLinkList64), matrixList);
}

void BindES2VertexBufferGlobals(void* currentCpuBuffer)
{
    CHook::Write(GtaAddress(kES2VertexBufferCurrentCpu32, kES2VertexBufferCurrentCpu64), currentCpuBuffer);
}

void BindStreamingInfoGlobals(void* arrayBase)
{
    CHook::Write(GtaAddress(kStreamingInfoArrayBase32, kStreamingInfoArrayBase64), arrayBase);
}

void BindReferencesGlobals(void* emptyList, void* references)
{
    CHook::Write(GtaAddress(kReferencesEmptyList32, kReferencesEmptyList64), emptyList);
    CHook::Write(GtaAddress(kReferencesArray32, kReferencesArray64), references);
}

void BindCollisionGlobals(void* colModelCache,
                          void* processLineCrossings,
                          void* collisionInMemory,
                          void* cameraCollideWithVehicles)
{
    CHook::Write(GtaAddress(kCollisionColModelCache32, kCollisionColModelCache64), colModelCache);
    CHook::Write(GtaAddress(kCollisionProcessLineCrossings32, kCollisionProcessLineCrossings64), processLineCrossings);
    CHook::Write(GtaAddress(kCollisionInMemory32, kCollisionInMemory64), collisionInMemory);
    CHook::Write(GtaAddress(kCollisionCameraCollideVehicles32, kCollisionCameraCollideVehicles64), cameraCollideWithVehicles);
}

void InstallCollisionHooksNative(uintptr_t initHook)
{
    CHook::Redirect("_ZN10CCollision4InitEv", reinterpret_cast<void*>(initHook));
}

void InstallPlaceableMatrixHooksNative(uintptr_t initMatrixArrayHook,
                                       uintptr_t shutdownMatrixArrayHook,
                                       uintptr_t allocateStaticMatrixHook,
                                       uintptr_t allocateMatrixHook,
                                       uintptr_t freeStaticMatrixHook,
                                       uintptr_t removeMatrixHook)
{
    CHook::Redirect("_ZN10CPlaceable15InitMatrixArrayEv", reinterpret_cast<void*>(initMatrixArrayHook));
    CHook::Redirect("_ZN10CPlaceable19ShutdownMatrixArrayEv", reinterpret_cast<void*>(shutdownMatrixArrayHook));
    CHook::Redirect("_ZN10CPlaceable20AllocateStaticMatrixEv", reinterpret_cast<void*>(allocateStaticMatrixHook));
    CHook::Redirect("_ZN10CPlaceable14AllocateMatrixEv", reinterpret_cast<void*>(allocateMatrixHook));
    CHook::Redirect("_ZN10CPlaceable16FreeStaticMatrixEv", reinterpret_cast<void*>(freeStaticMatrixHook));
    CHook::Redirect("_ZN10CPlaceable12RemoveMatrixEv", reinterpret_cast<void*>(removeMatrixHook));
}

void BindTimeCycleGlobals(void* currentColours, void* belowHorizonGrey)
{
    CHook::Write(GtaAddress(kTimeCycleCurrentColours32, kTimeCycleCurrentColours64), currentColours);
    CHook::Write(GtaAddress(kTimeCycleBelowHorizonGrey32, kTimeCycleBelowHorizonGrey64), belowHorizonGrey);
}

void BindRendererGlobals(void* renderOutsideTunnels,
                         void* loadingPriority,
                         void* visibleEntityPtrs,
                         void* visibleEntityCount,
                         void* visibleLodPtrs,
                         void* visibleLodCount,
                         void* visibleSuperLodPtrs,
                         void* visibleSuperLodCount,
                         void* invisibleEntityPtrs,
                         void* invisibleEntityCount)
{
    CHook::Write(GtaAddress(kRendererOutsideTunnels32, kRendererOutsideTunnels64), renderOutsideTunnels);
    CHook::Write(GtaAddress(kRendererLoadingPriority32, kRendererLoadingPriority64), loadingPriority);
    CHook::Write(GtaAddress(kRendererVisibleEntityPtrs32, kRendererVisibleEntityPtrs64), visibleEntityPtrs);
    CHook::Write(GtaAddress(kRendererVisibleEntityCount32, kRendererVisibleEntityCount64), visibleEntityCount);
#if !VER_x32
    CHook::Write(g_libGTASA + kRendererVisibleLodPtrs64, visibleLodPtrs);
    CHook::Write(g_libGTASA + kRendererVisibleLodCount64, visibleLodCount);
    CHook::Write(g_libGTASA + kRendererVisibleSuperLodPtrs64, visibleSuperLodPtrs);
    CHook::Write(g_libGTASA + kRendererVisibleSuperLodCount64, visibleSuperLodCount);
    CHook::Write(g_libGTASA + kRendererInvisibleEntityPtrs64, invisibleEntityPtrs);
    CHook::Write(g_libGTASA + kRendererInvisibleEntityCount64, invisibleEntityCount);
#else
    (void)visibleLodPtrs;
    (void)visibleLodCount;
    (void)visibleSuperLodPtrs;
    (void)visibleSuperLodCount;
    (void)invisibleEntityPtrs;
    (void)invisibleEntityCount;
#endif
}

void PrepareLineRenderNoClipping()
{
    CHook::CallFunction<void>(GtaAddress(kLineRenderNoClipPrepare32, kLineRenderNoClipPrepare64));
}

void RenderLineWithClippingNative(float startX,
                                  float startY,
                                  float startZ,
                                  float endX,
                                  float endY,
                                  float endZ,
                                  uint32 startColor,
                                  uint32 endColor)
{
#if VER_x32
    CHook::CallFunction<void>(g_libGTASA + kLineRenderWithClippingLegacy32,
                              startX,
                              startY,
                              startZ,
                              endX,
                              endY,
                              endZ,
                              startColor,
                              endColor);
#else
    (void)startX;
    (void)startY;
    (void)startZ;
    (void)endX;
    (void)endY;
    (void)endZ;
    (void)startColor;
    (void)endColor;
#endif
}

void ReplaceBuildingWithNewModel(CBuilding* building, int32 modelIndex)
{
    if (!building) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kBuildingReplaceWithNewModel32, kBuildingReplaceWithNewModel64), building, modelIndex);
}

void* MoveNativeMemory(void* memory)
{
    return CHook::CallFunction<void*>(GtaAddress(kMemoryMove32, kMemoryMove64), memory);
}

bool IsNativeTaskPtr(CTask* task)
{
    return CHook::CallFunction<bool>(GtaAddress(kTaskIsTaskPtr32, kTaskIsTaskPtr64), task);
}

CRealTimeShadow* GetNativeRealTimeShadow(CRealTimeShadowManager* manager, CPhysical* physical)
{
    return CHook::CallFunction<CRealTimeShadow*>(GtaAddress(kRealTimeShadowGet32, kRealTimeShadowGet64),
                                                 manager,
                                                 physical);
}

void* CreateNativeEvent(uint32 size)
{
    return CHook::CallFunction<void*>(GtaAddress(kEventNew32, kEventNew64), size);
}

void DeleteNativeEvent(void* event)
{
    CHook::CallFunction<void>(GtaAddress(kEventDelete32, kEventDelete64), event);
}

float GetNativeEventSoundLevel(CEvent* event, const CEntityGTA* entity, CVector& position)
{
    return CHook::CallFunction<float>(GtaAddress(kEventGetSoundLevel32, kEventGetSoundLevel64),
                                      event,
                                      entity,
                                      position);
}

void ProcessScriptThreadNative(void* scriptThread)
{
    if (!scriptThread) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kScriptThreadProcess32, kScriptThreadProcess64), scriptThread);
}

void HandleEventsNative(CEventHandler* handler)
{
    if (!handler) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kEventHandlerHandleEvents32, kEventHandlerHandleEvents64), handler);
}

CTask* FindActiveTaskByTypeNative(CTaskManager* manager, int32 taskType)
{
    if (!manager) {
        return nullptr;
    }
    return CHook::CallFunction<CTask*>(
        GtaAddress(kTaskManagerFindActiveTaskByType32, kTaskManagerFindActiveTaskByType64),
        manager,
        taskType);
}

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
                             float upDistance)
{
    return CHook::CallFunction<bool>(
        GtaAddress(kShadowsStoreStatic32, kShadowsStoreStatic64),
        id,
        type,
        texture,
        posn,
        frontX,
        frontY,
        sideX,
        sideY,
        intensity,
        red,
        green,
        blue,
        zDistance,
        scale,
        drawDistance,
        temporaryShadow,
        upDistance);
}

bool ShouldRedirectTextureListingMipCount()
{
    return *reinterpret_cast<uint8_t*>(GtaAddress(kTextureListingMipRedirectFlag32, kTextureListingMipRedirectFlag64)) != 0;
}

TextureDatabaseRuntime* TextureDatabaseRuntimeLoadNative(const char* withName,
                                                         bool fullyLoad,
                                                         int32 forcedFormat)
{
    return CHook::CallFunction<TextureDatabaseRuntime*>(
        GtaAddress(kTextureDatabaseRuntimeLoad32, kTextureDatabaseRuntimeLoad64),
        withName,
        fullyLoad,
        forcedFormat);
}

void TextureDatabaseRuntimeRegisterNative(TextureDatabaseRuntime* toRegister)
{
    CHook::CallFunction<void>(
        GtaAddress(kTextureDatabaseRuntimeRegister32, kTextureDatabaseRuntimeRegister64),
        toRegister);
}

void TextureDatabaseRuntimeUnregisterNative(TextureDatabaseRuntime* toUnregister)
{
    CHook::CallFunction<void>(
        GtaAddress(kTextureDatabaseRuntimeUnregister32, kTextureDatabaseRuntimeUnregister64),
        toUnregister);
}

RwTexture* TextureDatabaseRuntimeGetTextureNative(const char* name)
{
    return CHook::CallFunction<RwTexture*>(
        GtaAddress(kTextureDatabaseRuntimeGetTexture32, kTextureDatabaseRuntimeGetTexture64),
        name);
}

void TextureDatabaseRuntimeUpdateStreamingNative(float deltaTime, bool flush)
{
    CHook::CallFunction<void>(
        GtaAddress(kTextureDatabaseRuntimeUpdateStreaming32, kTextureDatabaseRuntimeUpdateStreaming64),
        deltaTime,
        flush);
}

TextureDatabaseRuntime* TextureDatabaseRuntimeGetDatabaseNative(const char* dbName)
{
    return CHook::CallFunction<TextureDatabaseRuntime*>(
        GtaAddress(kTextureDatabaseRuntimeGetDatabase32, kTextureDatabaseRuntimeGetDatabase64),
        dbName);
}

RwTexture* TextureDatabaseRuntimeGetRWTextureNative(TextureDatabaseRuntime* database, int32 index)
{
    return CHook::CallFunction<RwTexture*>(
        GtaAddress(kTextureDatabaseRuntimeGetRWTexture32, kTextureDatabaseRuntimeGetRWTexture64),
        database,
        index);
}

void TextureDatabaseRuntimeSetAsRenderedNative(TextureDatabaseRuntime* database, uint32 index)
{
    CHook::CallFunction<void>(
        GtaAddress(kTextureDatabaseRuntimeSetAsRendered32, kTextureDatabaseRuntimeSetAsRendered64),
        database,
        index);
}

void TextureDatabaseRuntimeLoadFullTextureNative(TextureDatabaseRuntime* database, uint32 index)
{
    CHook::CallFunction<void>(
        GtaAddress(kTextureDatabaseRuntimeLoadFullTexture32, kTextureDatabaseRuntimeLoadFullTexture64),
        database,
        index);
}

void InstallTextureDatabaseRuntimeHooksNative(uintptr_t loadThumbsHook,
                                              void** loadThumbsOriginal,
                                              uintptr_t sortEntriesHook,
                                              void** sortEntriesOriginal)
{
#if !VER_x32
    if (loadThumbsHook && loadThumbsOriginal) {
        CHook::InlineHook(g_libGTASA + kTextureDatabaseLoadThumbs64,
                          reinterpret_cast<void*>(loadThumbsHook),
                          loadThumbsOriginal);
    }

    if (sortEntriesHook && sortEntriesOriginal) {
        CHook::InlineHook(g_libGTASA + kTextureDatabaseRuntimeSortEntries64,
                          reinterpret_cast<void*>(sortEntriesHook),
                          sortEntriesOriginal);
    }
#else
    (void)loadThumbsHook;
    (void)loadThumbsOriginal;
    (void)sortEntriesHook;
    (void)sortEntriesOriginal;
#endif
}

int32 TextureAnnihilateNative(RwTexture* texture)
{
    return CHook::CallFunction<int32>(GtaAddress(kTextureAnnihilate32, kTextureAnnihilate64), texture);
}

void SetAlphaFuncProc(void* proc)
{
    *reinterpret_cast<void**>(GtaAddress(kAlphaFuncProcSlot32, kAlphaFuncProcSlot64)) = proc;
}

void InitialiseFont()
{
    CHook::CallFunction<void>(GtaAddress(kFontInitialise32, kFontInitialise64));
}

void AsciiToGxtChar(const char* ascii, uint16_t* gxt)
{
    if (!ascii || !gxt) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kFontAsciiToGxt32, kFontAsciiToGxt64), ascii, gxt);
}

void SetFontScale(float x, float y)
{
    *reinterpret_cast<float*>(GtaAddress(kFontScaleX32, kFontScaleX64)) = x;
    *reinterpret_cast<float*>(GtaAddress(kFontScaleY32, kFontScaleY64)) = y;
}

void SetFontColor(uint32_t* color)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetColor32, kFontSetColor64), color);
}

void SetFontJustify(uint8_t justify)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetJustify32, kFontSetJustify64), justify);
}

void SetFontOrientation(uint8_t orientation)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetOrientation32, kFontSetOrientation64), orientation);
}

void SetFontWrapX(float wrapX)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetWrapX32, kFontSetWrapX64), wrapX);
}

void SetFontCentreSize(float size)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetCentreSize32, kFontSetCentreSize64), size);
}

void SetFontRightJustifyWrap(float wrap)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetRightJustifyWrap32, kFontSetRightJustifyWrap64), wrap);
}

void SetFontBackground(uint8_t background, uint8_t onlyText)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetBackground32, kFontSetBackground64), background, onlyText);
}

void SetFontBackgroundColor(uint32_t* color)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetBackgroundColor32, kFontSetBackgroundColor64), color);
}

void SetFontProportional(uint8_t proportional)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetProportional32, kFontSetProportional64), proportional);
}

void SetFontDropColor(uint32_t* color)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetDropColor32, kFontSetDropColor64), color);
}

void SetFontDropShadowPosition(uint8_t position)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetDropShadowPosition32, kFontSetDropShadowPosition64), position);
}

void PrintGxtString(float x, float y, uint16_t* text)
{
    if (!text) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kFontPrintString32, kFontPrintString64), x, y, text);
}

void SetFontStyle(uint8_t style)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetStyle32, kFontSetStyle64), style);
}

void SetFontEdge(uint8_t edge)
{
    CHook::CallFunction<void>(GtaAddress(kFontSetEdge32, kFontSetEdge64), edge);
}

void CalcScreenCoors(const CVector& world,
                     CVector& screen,
                     float* outWidth,
                     float* outHeight,
                     bool checkMaxVisible,
                     bool checkMinVisible)
{
    auto calcScreenCoors = reinterpret_cast<CalcScreenCoorsFn>(
        GtaAddress(kCalcScreenCoors32, kCalcScreenCoors64));
    calcScreenCoors(const_cast<CVector*>(&world),
                    &screen,
                    outWidth,
                    outHeight,
                    checkMaxVisible,
                    checkMinVisible);
}

bool WorldToScreen(const CVector& world, CVector& screen, float* depth, float minDepth)
{
    CVector projected{};
    CalcScreenCoors(world, projected, nullptr, nullptr, false, false);

    screen = projected;
    if (depth) {
        *depth = projected.z;
    }

    return std::isfinite(projected.x) &&
           std::isfinite(projected.y) &&
           std::isfinite(projected.z) &&
           projected.z >= minDepth;
}

float GetWeaponRadiusOnScreen(CPedGTA* ped)
{
    if (!ped) {
        return 0.0f;
    }

    return CHook::CallFunction<float>(GtaAddress(kGetWeaponRadius32, kGetWeaponRadius64), ped);
}

bool CameraIsTargetingActive()
{
    return CHook::CallFunction<bool>(
        GtaAddress(kCameraIsTargetingActive32, kCameraIsTargetingActive64),
        &Camera());
}

CSector* Sector(int32 x, int32 y)
{
    static CSector(&sectors)[MAX_SECTORS_Y][MAX_SECTORS_X] =
        *reinterpret_cast<CSector(*)[MAX_SECTORS_Y][MAX_SECTORS_X]>(
            GtaAddress(kWorldSectors32, kWorldSectors64));

    const int32 clampedX = std::clamp<int32>(x, 0, MAX_SECTORS_X - 1);
    const int32 clampedY = std::clamp<int32>(y, 0, MAX_SECTORS_Y - 1);
    return &sectors[clampedY][clampedX];
}

CRepeatSector* RepeatSector(int32 x, int32 y)
{
    static CRepeatSector(&repeatSectors)[MAX_REPEAT_SECTORS_Y][MAX_REPEAT_SECTORS_X] =
        *reinterpret_cast<CRepeatSector(*)[MAX_REPEAT_SECTORS_Y][MAX_REPEAT_SECTORS_X]>(
            GtaAddress(kWorldRepeatSectors32, kWorldRepeatSectors64));

    return &repeatSectors[WrapRepeatSectorIndex(y, MAX_REPEAT_SECTORS_Y)]
                         [WrapRepeatSectorIndex(x, MAX_REPEAT_SECTORS_X)];
}

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
                        bool doShootThroughCheck)
{
    if (!origin || !target || !IsFiniteVector(*origin) || !IsFiniteVector(*target)) {
        if (outEntity) {
            *outEntity = nullptr;
        }
        return false;
    }

    Xyron::Perf::ScopedZone perfCollision(Xyron::Perf::Zone::Collision);
    return CHook::CallFunction<bool>(
        GtaAddress(kProcessLineOfSight32, kProcessLineOfSight64),
        origin,
        target,
        outColPoint,
        outEntity,
        buildings,
        vehicles,
        peds,
        objects,
        dummies,
        doSeeThroughCheck,
        doCameraIgnoreCheck,
        doShootThroughCheck);
}

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
                         CStoredCollPoly* storedPoly)
{
    if (!origin || !IsFiniteVector(*origin) || !std::isfinite(targetZ)) {
        if (outEntity) {
            *outEntity = nullptr;
        }
        return false;
    }

    Xyron::Perf::ScopedZone perfCollision(Xyron::Perf::Zone::Collision);
    return CHook::CallFunction<bool>(
        GtaAddress(kProcessVerticalLine32, kProcessVerticalLine64),
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

void InstallWorldLineOfSightHookNative(uintptr_t hook, void** original)
{
    if (!hook || !original) {
        return;
    }
    CHook::InlineHook("_ZN6CWorld18ProcessLineOfSightERK7CVectorS2_R9CColPointRP7CEntitybbbbbbbb",
                      reinterpret_cast<void*>(hook),
                      original);
}

void InstallWorldVerticalLineHookNative(uintptr_t hook, void** original)
{
    if (!hook || !original) {
        return;
    }
    CHook::InlineHook("_ZN6CWorld19ProcessVerticalLineERK7CVectorfR9CColPointRP7CEntitybbbbbbP15CStoredCollPoly",
                      reinterpret_cast<void*>(hook),
                      original);
}

void InstallPhysicalCheckCollisionHookNative(uintptr_t hook, void** original)
{
    if (!hook || !original) {
        return;
    }
    CHook::InlineHook("_ZN9CPhysical14CheckCollisionEv",
                      reinterpret_cast<void*>(hook),
                      original);
}

void InstallCollisionVerticalLineHookNative(uintptr_t hook, void** original)
{
    if (!hook || !original) {
        return;
    }
    CHook::InlineHook("_ZN10CCollision19ProcessVerticalLineERK8CColLineRK7CMatrixR9CColModelR9CColPointRfbbP15CStoredCollPoly",
                      reinterpret_cast<void*>(hook),
                      original);
}

float FindGroundZForCoord(float x, float y)
{
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return 0.0f;
    }

    Xyron::Perf::ScopedZone perfCollision(Xyron::Perf::Zone::Collision);
    return CHook::CallFunction<float>(GtaAddress(kFindGroundZForCoord32, kFindGroundZForCoord64), x, y);
}

bool GetIsLineOfSightClear(const CVector& origin,
                           const CVector& target,
                           bool buildings,
                           bool vehicles,
                           bool peds,
                           bool objects,
                           bool dummies,
                           bool doSeeThroughCheck,
                           bool doCameraIgnoreCheck)
{
    if (!IsFiniteVector(origin) || !IsFiniteVector(target)) {
        return false;
    }

    return CHook::CallFunction<bool>(
        GtaAddress(kGetIsLineOfSightClear32, kGetIsLineOfSightClear64),
        &origin,
        &target,
        buildings,
        vehicles,
        peds,
        objects,
        dummies,
        doSeeThroughCheck,
        doCameraIgnoreCheck);
}

void AddWorldEntity(CEntityGTA* entity)
{
    if (!entity) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kWorldAdd32, kWorldAdd64), entity);
}

void RemoveWorldEntity(CEntityGTA* entity)
{
    if (!entity) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kWorldRemove32, kWorldRemove64), entity);
}

void InstallWorldProcessPedsAfterPreRenderHook(void (*hook)())
{
    if (!hook) {
        return;
    }

    CHook::InstallPLT(
        GtaAddress(kWorldProcessPedsAfterPreRenderPlt32, kWorldProcessPedsAfterPreRenderPlt64),
        hook);
}

void AddCollisionNeededAtPosn(const CVector& pos)
{
    if (!IsFiniteVector(pos)) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kColAddNeeded32, kColAddNeeded64), &pos);
}

void SetCollisionRequired(const CVector& pos, int32 areaCode)
{
    if (!IsFiniteVector(pos)) {
        return;
    }
    Xyron::Perf::ScopedZone perfCollision(Xyron::Perf::Zone::Collision);
    CHook::CallFunction<void>(GtaAddress(kColSetRequired32, kColSetRequired64), &pos, areaCode);
}

void RequestCollision(const CVector& pos, int32 areaCode)
{
    if (!IsFiniteVector(pos)) {
        return;
    }
    RequestCollisionNative(pos, areaCode);
    LoadCollision(pos, false);
    EnsureCollisionIsInMemory(pos);
}

bool HasCollisionLoaded(const CVector& pos, int32 areaCode)
{
    if (!IsFiniteVector(pos)) {
        return false;
    }
    Xyron::Perf::ScopedZone perfCollision(Xyron::Perf::Zone::Collision);
    return CHook::CallFunction<bool>(GtaAddress(kColHasLoaded32, kColHasLoaded64), &pos, areaCode);
}

void LoadCollision(const CVector& pos, bool ignorePlayerVehicle)
{
    if (!IsFiniteVector(pos)) {
        return;
    }
    Xyron::Perf::ScopedZone perfCollision(Xyron::Perf::Zone::Collision);
    CHook::CallFunction<void>(GtaAddress(kColLoadCollision32, kColLoadCollision64), &pos, ignorePlayerVehicle);
}

void EnsureCollisionIsInMemory(const CVector& pos)
{
    if (!IsFiniteVector(pos)) {
        return;
    }
    Xyron::Perf::ScopedZone perfCollision(Xyron::Perf::Zone::Collision);
    CHook::CallFunction<void>(GtaAddress(kColEnsureInMemory32, kColEnsureInMemory64), &pos);
}

int32 FindColSlot(const char* name)
{
    if (!name || !name[0]) {
        return -1;
    }
    return CHook::CallFunction<int32>(GtaAddress(kColFindSlot32, kColFindSlot64), name);
}

void AddColRef(int32 colSlot)
{
    CHook::CallFunction<void>(GtaAddress(kColAddRef32, kColAddRef64), colSlot);
}

void RemoveColRef(int32 colSlot)
{
    CHook::CallFunction<void>(GtaAddress(kColRemoveRef32, kColRemoveRef64), colSlot);
}

void IncludeColModelIndex(int32 colSlot, int32 modelId)
{
    CHook::CallFunction<void>(GtaAddress(kColIncludeModel32, kColIncludeModel64), colSlot, modelId);
}

void RemoveCol(int32 colSlot)
{
    CHook::CallFunction<void>(GtaAddress(kColRemove32, kColRemove64), colSlot);
}

void LoadCol(int32 colSlot, const char* name)
{
    if (!name || !name[0]) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kColLoadCol32, kColLoadCol64), colSlot, name);
}

void LoadAllCollision()
{
    CHook::CallFunction<void>(GtaAddress(kColLoadAllCollision32, kColLoadAllCollision64));
}

void LoadAllBoundingBoxes()
{
    CHook::CallFunction<void>(GtaAddress(kColLoadAllBoundingBoxes32, kColLoadAllBoundingBoxes64));
}

void InitialiseColStore()
{
    CHook::CallFunction<void>(GtaAddress(kColInitialise32, kColInitialise64));
}

CColPool* ColStorePool()
{
    return *reinterpret_cast<CColPool**>(GtaAddress(kColPool32, kColPool64));
}

bool ColStoreOnlyBB()
{
    return *reinterpret_cast<bool*>(GtaAddress(kColOnlyBB32, kColOnlyBB64));
}

void AddIplsNeededAtPosn(const CVector& pos)
{
    if (!IsFiniteVector(pos)) {
        return;
    }
    Xyron::Perf::ScopedZone perfStreaming(Xyron::Perf::Zone::Streaming);
    CHook::CallFunction<void>(GtaAddress(kIplAddNeeded32, kIplAddNeeded64), &pos);
}

void LoadIpls(const CVector& pos, bool avoidLoadInPlayerVehicleMovingDirection)
{
    if (!IsFiniteVector(pos)) {
        return;
    }
    Xyron::Perf::ScopedZone perfStreaming(Xyron::Perf::Zone::Streaming);
    CHook::CallFunction<void>(
        GtaAddress(kIplLoad32, kIplLoad64),
        pos,
        avoidLoadInPlayerVehicleMovingDirection);
}

void EnsureIplsAreInMemory(const CVector& pos)
{
    if (!IsFiniteVector(pos)) {
        return;
    }
    Xyron::Perf::ScopedZone perfStreaming(Xyron::Perf::Zone::Streaming);
    CHook::CallFunction<void>(GtaAddress(kIplEnsure32, kIplEnsure64), &pos);
}

void RemoveIpl(int32 iplSlotIndex)
{
    CHook::CallFunction<void>(GtaAddress(kIplRemove32, kIplRemove64), iplSlotIndex);
}

void AddModelsToRequestList(const CVector& point, int32 streamingFlags)
{
    if (!IsFiniteVector(point)) {
        return;
    }
    Xyron::Perf::ScopedZone perfStreaming(Xyron::Perf::Zone::Streaming);
    CHook::CallFunction<void>(GtaAddress(kStreamingAddModels32, kStreamingAddModels64), &point, streamingFlags);
}

void AddLodsToRequestList(const CVector& point, int32 streamingFlags)
{
    if (!IsFiniteVector(point)) {
        return;
    }
    Xyron::Perf::ScopedZone perfStreaming(Xyron::Perf::Zone::Streaming);
    CHook::CallFunction<void>(GtaAddress(kStreamingAddLods32, kStreamingAddLods64), &point, streamingFlags);
}

void LoadSceneCollision(const CVector& point)
{
    if (!IsFiniteVector(point)) {
        return;
    }
    Xyron::Perf::ScopedZone perfStreaming(Xyron::Perf::Zone::Streaming);
    CHook::CallFunction<void>(
        GtaAddress(kStreamingLoadSceneCollision32, kStreamingLoadSceneCollision64),
        &point);
}

void LoadScene(const CVector& point)
{
    if (!IsFiniteVector(point)) {
        return;
    }

#if !VER_x32
    LoadSceneCollision(point);
#else
    CHook::CallFunction<void>(GtaAddress(kStreamingLoadScene32, kStreamingLoadScene64), &point);
#endif
}

void LoadRequestedModels()
{
    Xyron::Perf::ScopedZone perfStreaming(Xyron::Perf::Zone::Streaming);
    CHook::CallFunction<void>(GtaAddress(kStreamingLoadRequested32, kStreamingLoadRequested64));
}

void FlushStreamingChannels()
{
    CHook::CallFunction<void>(GtaAddress(kStreamingFlushChannels32, kStreamingFlushChannels64));
}

void SetModelIsDeletable(int32 modelId)
{
    CHook::CallFunction<void>(
        GtaAddress(kStreamingSetModelDeletable32, kStreamingSetModelDeletable64),
        modelId);
}

void RemoveBuildingsNotInArea(int32 areaCode)
{
    CHook::CallFunction<void>(
        GtaAddress(kStreamingRemoveBuildingsNotInArea32, kStreamingRemoveBuildingsNotInArea64),
        areaCode);
}

void BindStreamingGlobals()
{
    CHook::Write(GtaAddress(kStreamingEndRequestedList32, kStreamingEndRequestedList64), &CStreaming::ms_pEndRequestedList);
    CHook::Write(GtaAddress(kStreamingStartRequestedList32, kStreamingStartRequestedList64), &CStreaming::ms_pStartRequestedList);
    CHook::Write(GtaAddress(kStreamingEndLoadedList32, kStreamingEndLoadedList64), &CStreaming::ms_pEndLoadedList);
    CHook::Write(GtaAddress(kStreamingStartLoadedList32, kStreamingStartLoadedList64), &CStreaming::ms_startLoadedList);

    CHook::Write(GtaAddress(kStreamingEnableRequestPurge32, kStreamingEnableRequestPurge64), &CStreaming::ms_bEnableRequestListPurge);
    CHook::Write(GtaAddress(kStreamingDisableStreaming32, kStreamingDisableStreaming64), &CStreaming::ms_disableStreaming);
    CHook::Write(GtaAddress(kStreamingChannel32, kStreamingChannel64), &CStreaming::ms_channel);
    CHook::Write(GtaAddress(kStreamingNumModelsRequested32, kStreamingNumModelsRequested64), &CStreaming::ms_numModelsRequested);
    CHook::Write(GtaAddress(kStreamingNumPriorityRequests32, kStreamingNumPriorityRequests64), &CStreaming::ms_numPriorityRequests);
    CHook::Write(GtaAddress(kStreamingLoadingBigModel32, kStreamingLoadingBigModel64), &CStreaming::ms_bLoadingBigModel);
    CHook::Write(GtaAddress(kStreamingChannelError32, kStreamingChannelError64), &CStreaming::ms_channelError);
    CHook::Write(GtaAddress(kStreamingBuffer32, kStreamingBuffer64), &CStreaming::ms_pStreamingBuffer);
    CHook::Write(GtaAddress(kStreamingBufferSize32, kStreamingBufferSize64), &CStreaming::ms_streamingBufferSize);

    CHook::Write(GtaAddress(kStreamingMemoryUsed32, kStreamingMemoryUsed64), &CStreaming::ms_memoryUsed);
    CHook::Write(GtaAddress(kStreamingMemoryAvailable32, kStreamingMemoryAvailable64), &CStreaming::ms_memoryAvailable);
    CHook::Write(GtaAddress(kStreamingDesiredVehicles32, kStreamingDesiredVehicles64), &CStreaming::desiredNumVehiclesLoaded);
    CHook::Write(GtaAddress(kStreamingFiles32, kStreamingFiles64), &CStreaming::ms_files);
    CHook::Write(GtaAddress(kStreamingRwObjectInstances32, kStreamingRwObjectInstances64), &CStreaming::ms_rwObjectInstances);
    CHook::Write(GtaAddress(kStreamingInfoForModel32, kStreamingInfoForModel64), &CStreaming::ms_aInfoForModel);
}

void InstallStreamingHooksNative(uintptr_t initImageListHook,
                                 uintptr_t makeSpaceForHook,
                                 uintptr_t deleteAllRwObjectsHook)
{
    CHook::Redirect("_ZN10CStreaming13InitImageListEv", reinterpret_cast<void*>(initImageListHook));
    CHook::Redirect("_ZN10CStreaming12MakeSpaceForEi", reinterpret_cast<void*>(makeSpaceForHook));
#if !VER_x32
    if (deleteAllRwObjectsHook != 0) {
        CHook::Redirect("_ZN10CStreaming18DeleteAllRwObjectsEv",
                        reinterpret_cast<void*>(deleteAllRwObjectsHook));
    }
#else
    (void)deleteAllRwObjectsHook;
#endif
}

void RequestModelNativeArm64(int32 modelId, int32 streamingFlags)
{
#if !VER_x32
    CHook::CallFunction<void>(g_libGTASA + kStreamingRequestModel64, modelId, streamingFlags);
#else
    (void)modelId;
    (void)streamingFlags;
#endif
}

void DeleteAllRwObjectsNative32()
{
#if VER_x32
    CHook::CallFunction<void>(g_libGTASA + kStreamingDeleteAllRwObjects32);
#endif
}

void LoadAllRequestedModelsNativeArm64(bool priorityRequestsOnly)
{
#if !VER_x32
    CHook::CallFunction<void>(g_libGTASA + kStreamingLoadAllRequested64, priorityRequestsOnly);
#else
    (void)priorityRequestsOnly;
#endif
}

void ClearModelStreamNotLoadedFlag()
{
    bool& modelStreamNotLoaded =
        *reinterpret_cast<bool*>(GtaAddress(kStreamingModelStreamNotLoaded32, kStreamingModelStreamNotLoaded64));
    if (modelStreamNotLoaded) {
        modelStreamNotLoaded = false;
    }
}

bool ConvertStreamingBufferToObject(uint8* fileBuffer, int32 modelId)
{
    if (!fileBuffer) {
        return false;
    }

    return CHook::CallFunction<bool>(
        GtaAddress(kStreamingConvertBufferToObject32, kStreamingConvertBufferToObject64),
        fileBuffer,
        modelId);
}

void FinishLoadingLargeStreamingFile(uint8* fileBuffer, int32 modelId)
{
    if (!fileBuffer) {
        return;
    }

    CHook::CallFunction<void>(
        GtaAddress(kStreamingFinishLargeFile32, kStreamingFinishLargeFile64),
        fileBuffer,
        modelId);
}

void RetryStreamingLoadFile(int32 channelIndex)
{
    CHook::CallFunction<void>(
        GtaAddress(kStreamingRetryLoadFile32, kStreamingRetryLoadFile64),
        channelIndex);
}

void DeleteRwObjectsBehindCamera(size_t memoryToCleanInBytes)
{
    CHook::CallFunction<void>(
        GtaAddress(kStreamingDeleteRwBehindCamera32, kStreamingDeleteRwBehindCamera64),
        memoryToCleanInBytes);
}

bool DeleteLeastUsedEntityRwObject(bool notOnScreen, int32 streamingFlags)
{
    return CHook::CallFunction<bool>(
        GtaAddress(kStreamingDeleteLeastUsedEntity32, kStreamingDeleteLeastUsedEntity64),
        notOnScreen,
        streamingFlags);
}

bool HasVehicleUpgradeLoaded(int32 modelId)
{
    return CHook::CallFunction<bool>(
        GtaAddress(kStreamingHasVehicleUpgrade32, kStreamingHasVehicleUpgrade64),
        modelId);
}

char* GetModelCDName(int32 index)
{
    return CHook::CallFunction<char*>(GtaAddress(kStreamingModelCDName32, kStreamingModelCDName64), index);
}

void ForceStreamingEnabled(const char* reason)
{
    if (!CStreaming::ms_disableStreaming) {
        return;
    }

    CStreaming::ms_disableStreaming = false;
    static uint32_t s_lastLogTick = 0;
    const uint32_t now = GetTickCount();
    if (now - s_lastLogTick >= 1500) {
        s_lastLogTick = now;
        FLog("[WORLD_STREAM] streaming re-enabled reason=%s", reason ? reason : "?");
    }
}

WorldStreamStatus PrepareWorldAtPoint(const CVector& point, const WorldStreamRequest& request)
{
    WorldStreamStatus status{};
    if (!IsFiniteVector(point)) {
        FLog("[WORLD_STREAM] invalid point reason=%s pos=%.3f %.3f %.3f",
             request.reason ? request.reason : "?",
             point.x,
             point.y,
             point.z);
        return status;
    }

    status.valid = true;
    ForceStreamingEnabled(request.reason);

    if (request.addModels) {
        AddModelsToRequestList(point, request.streamingFlags);
        ++status.operations;
    }
    if (request.addLods) {
        AddLodsToRequestList(point, request.streamingFlags);
        ++status.operations;
    }
    if (request.addCollisionNeeded) {
        AddCollisionNeededAtPosn(point);
        ++status.operations;
    }
    if (request.setCollisionRequired) {
        SetCollisionRequired(point, request.areaCode);
        ++status.operations;
    }
    if (request.requestCollision) {
        RequestCollisionNative(point, request.areaCode);
        ++status.operations;
    }
    if (request.loadCollision) {
        LoadCollision(point, request.ignorePlayerVehicleCollision);
        ++status.operations;
    }
    if (request.ensureCollision) {
        EnsureCollisionIsInMemory(point);
        ++status.operations;
    }
    if (request.addIpls) {
        AddIplsNeededAtPosn(point);
        ++status.operations;
    }
    if (request.loadIpls) {
        LoadIpls(point, false);
        ++status.operations;
    }
    if (request.ensureIpls) {
        EnsureIplsAreInMemory(point);
        ++status.operations;
    }
    if (request.loadSceneCollision) {
        LoadSceneCollision(point);
        ++status.operations;
    }
    if (request.loadScene) {
        LoadScene(point);
        ++status.operations;
    }
    if (request.refreshGame && pGame) {
        pGame->RefreshStreamingAt(point.x, point.y);
        ++status.operations;
    }

    const bool shouldValidateCollision =
        request.setCollisionRequired ||
        request.requestCollision ||
        request.loadCollision ||
        request.ensureCollision ||
        request.loadSceneCollision ||
        request.loadScene;
    status.collisionLoaded = !shouldValidateCollision || HasCollisionLoaded(point, request.areaCode);
    if (shouldValidateCollision && !status.collisionLoaded) {
        static uint32_t s_lastCollisionMissLogTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastCollisionMissLogTick >= 1200) {
            s_lastCollisionMissLogTick = now;
            FLog("[WORLD_STREAM] collision still missing reason=%s pos=%.2f %.2f %.2f area=%d ops=%d",
                 request.reason ? request.reason : "?",
                 point.x,
                 point.y,
                 point.z,
                 request.areaCode,
                 status.operations);
        }
    }

    return status;
}

GroundProbeResult ProbeGround(const CVector& pos, float upDistance, float downDistance)
{
    GroundProbeResult result{};
    if (!IsFiniteVector(pos)) {
        return result;
    }

    CColPoint col{};
    CEntityGTA* entity = nullptr;
    const CVector start(pos.x, pos.y, pos.z + std::max(0.0f, upDistance));
    const CVector end(pos.x, pos.y, pos.z - std::max(0.0f, downDistance));
    result.hit = ProcessLineOfSight(
        &start,
        &end,
        &col,
        &entity,
        true,
        true,
        false,
        true,
        true,
        false,
        false,
        false);
    result.entity = entity;
    if (result.hit) {
        result.point = col.m_vecPoint;
        result.entityUsesCollision = entity && entity->m_bUsesCollision;
    }
    return result;
}

CPedGTA* GetPedFromPool(int id)
{
    return CHook::CallFunction<CPedGTA*>(GtaAddress(kPedPoolGetAt32, kPedPoolGetAt64), id);
}

int GetPedPoolIndex(CPedGTA* ped)
{
    if (!ped) {
        return -1;
    }
    return CHook::CallFunction<int>(GtaAddress(kPedPoolGetIndex32, kPedPoolGetIndex64), ped);
}

CPhysical* GetObjectFromPool(int id)
{
    return CHook::CallFunction<CPhysical*>(GtaAddress(kObjectPoolGetAt32, kObjectPoolGetAt64), id);
}

uintptr_t GetVehiclePoolIndex(CVehicleGTA* vehicle)
{
    if (!vehicle) {
        return 0;
    }
    return CHook::CallFunction<uintptr_t>(
        GtaAddress(kVehiclePoolGetIndex32, kVehiclePoolGetIndex64),
        vehicle);
}

CVehicleGTA* GetVehicleFromPool(int id)
{
    return CHook::CallFunction<CVehicleGTA*>(GtaAddress(kVehiclePoolGetAt32, kVehiclePoolGetAt64), id);
}

int GetVehicleSubtype(CVehicleGTA* vehicle)
{
    if (!vehicle) {
        return 0;
    }

    const uintptr_t vtable = *reinterpret_cast<uintptr_t*>(vehicle);
    if (vtable == GtaAddress(kVehicleCarVTable32, kVehicleCarVTable64)) {
        return VEHICLE_SUBTYPE_CAR;
    }
    if (vtable == GtaAddress(kVehicleBoatVTable32, kVehicleBoatVTable64)) {
        return VEHICLE_SUBTYPE_BOAT;
    }
    if (vtable == GtaAddress(kVehicleBikeVTable32, kVehicleBikeVTable64)) {
        return VEHICLE_SUBTYPE_BIKE;
    }
    if (vtable == GtaAddress(kVehiclePlaneVTable32, kVehiclePlaneVTable64)) {
        return VEHICLE_SUBTYPE_PLANE;
    }
    if (vtable == GtaAddress(kVehicleHeliVTable32, kVehicleHeliVTable64)) {
        return VEHICLE_SUBTYPE_HELI;
    }
    if (vtable == GtaAddress(kVehiclePushBikeVTable32, kVehiclePushBikeVTable64)) {
        return VEHICLE_SUBTYPE_PUSHBIKE;
    }
    if (vtable == GtaAddress(kVehicleTrainVTable32, kVehicleTrainVTable64)) {
        return VEHICLE_SUBTYPE_TRAIN;
    }

    return 0;
}

bool IsPedModelInfo(uintptr_t modelInfo)
{
    return modelInfo &&
           *reinterpret_cast<uintptr_t*>(modelInfo) ==
               GtaAddress(kPedModelInfoVTable32, kPedModelInfoVTable64);
}

void ServiceVehicleAudioEntity(uintptr_t audioEntity)
{
    if (!audioEntity) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kVehicleAudioService32, kVehicleAudioService64), audioEntity);
}

void ProcessVehicleControl(CVehicleGTA* vehicle, uintptr_t nativeProcessOffset)
{
    if (!vehicle || !nativeProcessOffset) {
        return;
    }
    CHook::CallFunction<void>(g_libGTASA + nativeProcessOffset + (VER_x32 ? 1 : 0), vehicle);
}

void AutomobileFix(CVehicleGTA* vehicle)
{
    if (!vehicle) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kAutomobileFix32, kAutomobileFix64), vehicle);
}

void AutomobileSetupDamageAfterLoad(CVehicleGTA* vehicle)
{
    if (!vehicle) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kAutomobileSetupDamageAfterLoad32, kAutomobileSetupDamageAfterLoad64),
        vehicle);
}

void DestroyPlayerPed(CPedGTA* ped)
{
    if (!ped) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPlayerPedDestructor32, kPlayerPedDestructor64), ped);
}

void ShutdownPed(CPedGTA* ped)
{
    if (!ped) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPedShutdown32, kPedShutdown64), ped);
}

void SetupPlayerPed(int playerNumber)
{
    CHook::CallFunction<void>(GtaAddress(kPlayerSetupPed32, kPlayerSetupPed64), playerNumber);
}

void DeactivatePlayerPed(int playerNumber)
{
    CHook::CallFunction<void>(GtaAddress(kPlayerDeactivatePed32, kPlayerDeactivatePed64), playerNumber);
}

void ReactivatePlayerPed(int playerNumber)
{
    CHook::CallFunction<void>(GtaAddress(kPlayerReactivatePed32, kPlayerReactivatePed64), playerNumber);
}

void ClearSpaceForMissionEntity(const CVector& pos, CEntityGTA* entity)
{
    if (!entity || !IsFiniteVector(pos)) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kClearSpaceForMissionEntity32, kClearSpaceForMissionEntity64), &pos, entity);
}

void SetPlayerPedInitialState(CPedGTA* ped)
{
    if (!ped) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPlayerPedInitialState32, kPlayerPedInitialState64), ped);
}

void ClearPedWeapons(CPedGTA* ped)
{
    if (!ped) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPedClearWeapons32, kPedClearWeapons64), ped);
}

void GivePedWeapon(CPedGTA* ped, int weaponId, int ammo)
{
    if (!ped) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPedGiveWeapon32, kPedGiveWeapon64), ped, weaponId, ammo);
}

void SetPedCurrentWeapon(CPedGTA* ped, int weaponId)
{
    if (!ped) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPedSetCurrentWeapon32, kPedSetCurrentWeapon64), ped, weaponId);
}

void SetPedCurrentWeaponAfterGive(CPedGTA* ped, int weaponId)
{
    if (!ped) {
        return;
    }
#if VER_x32
    CHook::CallFunction<void>(g_libGTASA + kPedSetCurrentWeaponAfterGive32, ped, weaponId);
#else
    FLog("[WEAPON_API64] SetCurrentWeaponAfterGive fallback weapon=%d ped=%p reason=no-verified-arm64-offset", weaponId, ped);
    SetPedCurrentWeapon(ped, weaponId);
#endif
}

void GetPedBonePosition(CPedGTA* ped, RwV3d* out, uint32_t boneTag, bool calledFromCamera)
{
    if (!ped || !out) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPedBonePosition32, kPedBonePosition64), ped, out, boneTag, calledFromCamera);
}

void GetPedTransformedBonePosition(CPedGTA* ped, CVector* out, int boneId, bool calledFromCamera)
{
    if (!ped || !out) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kPedTransformedBonePosition32, kPedTransformedBonePosition64),
        ped,
        out,
        boneId,
        calledFromCamera);
}

bool IsRunNamedOrSlideTask(const void* task)
{
    if (!task) {
        return false;
    }

    const uintptr_t vtable = *reinterpret_cast<const uintptr_t*>(task);
    return vtable == g_libGTASA + kSlideToCoordTaskVTableLegacy32 ||
           vtable == g_libGTASA + kRunNamedAnimTaskVTableLegacy32;
}

bool IsAnimAssocGroupLoaded(int groupId)
{
    if (groupId < 0) {
        return false;
    }

    auto groups = *reinterpret_cast<uintptr_t**>(GtaAddress(kAnimAssocGroups32, kAnimAssocGroups64));
    if (!groups) {
        return false;
    }

    uintptr_t* group = reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(groups) + static_cast<uintptr_t>(groupId) * 20U);
    return *group != 0;
}

uintptr_t GetWeaponInfo(int weaponId, int skill)
{
    return CHook::CallFunction<uintptr_t>(GtaAddress(kWeaponInfo32, kWeaponInfo64), weaponId, skill);
}

void ApplyPedCrouch(CPedIntelligence* intelligence, uint16_t arg)
{
    if (!intelligence) {
        return;
    }
    CHook::CallFunction<int>(GtaAddress(kPedIntelligenceCrouch32, kPedIntelligenceCrouch64), intelligence, arg);
}

void ResetPedCrouch(CPedIntelligence* intelligence)
{
    if (!intelligence) {
        return;
    }
    CHook::CallFunction<int>(GtaAddress(kPedIntelligenceResetCrouch32, kPedIntelligenceResetCrouch64), intelligence);
}

void FlushPedIntelligenceNative(CPedIntelligence* intelligence)
{
    if (!intelligence) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPedIntelligenceFlush32, kPedIntelligenceFlush64), intelligence);
}

void FlushPedIntelligenceImmediatelyNative(CPedIntelligence* intelligence, bool setPrimaryDefaultTask)
{
    if (!intelligence) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kPedIntelligenceFlushImmediately32, kPedIntelligenceFlushImmediately64),
        intelligence,
        setPrimaryDefaultTask);
}

void ProcessPedIntelligenceAfterPreRenderNative(CPedIntelligence* intelligence)
{
    if (!intelligence) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kPedIntelligenceProcessAfterPreRender32, kPedIntelligenceProcessAfterPreRender64),
        intelligence);
}

void TriggerJetpackCheat()
{
    CHook::CallFunction<void>(GtaAddress(kJetpackCheat32, kJetpackCheat64));
}

void PatchRemotePedUpdatePosition(bool disable)
{
    const uintptr_t patchAddress = GtaAddress(kPedUpdatePositionPatch32, kPedUpdatePositionPatch64);
    if (disable) {
        CHook::NOP(patchAddress, 2);
        return;
    }

#if VER_x32
    CHook::WriteMemory(patchAddress, "\xFE\xF7\x81\xFF", 4);
#else
    CHook::WriteMemory(patchAddress, "\x7A\xFB\xFF\x97", 4);
#endif
}

uint32_t CallTaskUseGun(uintptr_t task, uintptr_t ped)
{
    return CHook::CallFunction<uint32_t>(GtaAddress(kTaskUseGun32, kTaskUseGun64), task, ped);
}

uint32_t CallPadTaskProcess(uintptr_t task, uintptr_t ped, int arg0, int arg1)
{
    return CHook::CallFunction<uint32_t>(
        GtaAddress(kPadTaskProcess32, kPadTaskProcess64),
        task,
        ped,
        arg0,
        arg1);
}

void InstallPedProcessControlHook(PedProcessControlFn hook, PedProcessControlFn* original)
{
    if (!hook || !original) {
        return;
    }
    CHook::InstallPLT(GtaAddress(kPedProcessControlPlt32, kPedProcessControlPlt64), hook, original);
}

void InstallVehicleProcessControlHooks(VehicleProcessControlFn hook)
{
    if (!hook) {
        return;
    }

    constexpr size_t kVehicleHookCount =
        sizeof(kVehicleProcessControlPlt32) / sizeof(kVehicleProcessControlPlt32[0]);
    for (size_t i = 0; i < kVehicleHookCount; ++i) {
        CHook::InstallPLT(
            GtaAddress(kVehicleProcessControlPlt32[i], kVehicleProcessControlPlt64[i]),
            hook);
    }
}

void InstallTaskUseGunHook(TaskUseGunFn hook)
{
    if (!hook) {
        return;
    }
    CHook::InstallPLT(GtaAddress(kTaskUseGunPlt32, kTaskUseGunPlt64), hook);
}

void InstallPadTaskProcessHook(PadTaskProcessFn hook)
{
    if (!hook) {
        return;
    }
    CHook::InstallPLT(GtaAddress(kPadTaskProcessPlt32, kPadTaskProcessPlt64), hook);
}

void InstallPadInputHooksNative(const NativeInlineHook* hooks, size_t hookCount)
{
    if (!hooks || hookCount == 0) {
        return;
    }

    for (size_t i = 0; i < hookCount; ++i) {
        const NativeInlineHook& hook = hooks[i];
        if (!hook.symbol || !hook.hook || !hook.original) {
            continue;
        }
        CHook::InlineHook(hook.symbol, reinterpret_cast<void*>(hook.hook), hook.original);
    }
}

void InstallWidgetButtonUpdateHook(WidgetButtonUpdateFn hook, WidgetButtonUpdateFn* original)
{
    if (!hook || !original) {
        return;
    }
    CHook::InstallPLT(GtaAddress(kWidgetButtonUpdatePlt32, kWidgetButtonUpdatePlt64), hook, original);
}

void InstallWidgetTouchHooks(WidgetTouchFn touchedHook,
                             WidgetTouchFn* touchedOriginal,
                             WidgetTouchFn releasedHook,
                             WidgetTouchFn* releasedOriginal)
{
    if (touchedHook && touchedOriginal) {
        CHook::InlineHook(
            GtaAddress(kWidgetIsTouched32, kWidgetIsTouched64),
            touchedHook,
            touchedOriginal);
    }
    if (releasedHook && releasedOriginal) {
        CHook::InlineHook(
            GtaAddress(kWidgetIsReleased32, kWidgetIsReleased64),
            releasedHook,
            releasedOriginal);
    }
}

bool WidgetIsReleased(CWidgetGta* widget, CVector2D* out)
{
    if (!widget) {
        return false;
    }
    return CHook::CallFunction<bool>(
        GtaAddress(kWidgetIsReleased32, kWidgetIsReleased64),
        reinterpret_cast<uintptr_t*>(widget),
        out);
}

bool WidgetIsTouched(CWidgetGta* widget, CVector2D* out)
{
    if (!widget) {
        return false;
    }
    return CHook::CallFunction<bool>(
        GtaAddress(kWidgetIsTouched32, kWidgetIsTouched64),
        reinterpret_cast<uintptr_t*>(widget),
        out);
}

OSDeviceForm GetOSDeviceForm()
{
    return CHook::CallFunction<OSDeviceForm>(GtaAddress(kOSDeviceForm32, kOSDeviceForm64));
}

CWidgetGta** TouchInterfaceWidgets()
{
    return *reinterpret_cast<CWidgetGta***>(GtaAddress(kTouchInterfaceWidgets32, kTouchInterfaceWidgets64));
}

uintptr_t RebaseFromGta(uintptr_t address)
{
    return address >= g_libGTASA ? address - g_libGTASA : address;
}

void SetSpriteTexture(void* spriteStorage, const char* textureName)
{
    if (!spriteStorage || !textureName || !textureName[0]) {
        return;
    }
    CHook::CallFunction<void>(g_libGTASA + kSpriteSetTextureLegacy32, spriteStorage, textureName);
}

void SetSprite2dTextureNative(void* sprite, const char* textureName)
{
    if (!sprite || !textureName || !textureName[0]) {
        return;
    }
    CHook::CallFunction<void>("_ZN9CSprite2d10SetTextureEPc", sprite, textureName);
}

void DrawSprite2dNative(void* sprite, float x, float y, float width, float height, void* color)
{
    if (!sprite || !color) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kSprite2dDraw32, kSprite2dDraw64),
                              sprite,
                              x,
                              y,
                              width,
                              height,
                              color);
}

void BindSprite2dGlobals(void* recipNearClip, void* nearScreenZ)
{
    CHook::Write(GtaAddress(kSprite2dRecipNearClip32, kSprite2dRecipNearClip64), recipNearClip);
    CHook::Write(GtaAddress(kSprite2dNearScreenZ32, kSprite2dNearScreenZ64), nearScreenZ);
}

void VisibilityInitialiseNative()
{
    CHook::CallFunction<void>(GtaAddress(kVisibilityInitialise32, kVisibilityInitialise64));
}

void VisibilitySetRenderWareCameraNative(RwCamera* camera)
{
    CHook::CallFunction<void>(GtaAddress(kVisibilitySetRenderWareCamera32, kVisibilitySetRenderWareCamera64), camera);
}

int32 VisibilityGetClumpAlphaNative(RpClump* clump)
{
    return CHook::CallFunction<int32>(GtaAddress(kVisibilityGetClumpAlpha32, kVisibilityGetClumpAlpha64), clump);
}

void VisibilityRenderAlphaAtomicNative(RpAtomic* atomic, int32 alpha)
{
    CHook::CallFunction<void>(GtaAddress(kVisibilityRenderAlphaAtomic32, kVisibilityRenderAlphaAtomic64), atomic, alpha);
}

void VisibilitySetupVehicleVariablesNative(RpClump* clump)
{
    CHook::CallFunction<void>(GtaAddress(kVisibilitySetupVehicleVariables32, kVisibilitySetupVehicleVariables64), clump);
}

void VisibilityRenderReallyDrawLastObjectsNative()
{
    CHook::CallFunction<void>(GtaAddress(kVisibilityRenderReallyDrawLast32, kVisibilityRenderReallyDrawLast64));
}

uint16 VisibilityGetAtomicIdNative(RpAtomic* atomic)
{
    return CHook::CallFunction<uint16>(GtaAddress(kVisibilityGetAtomicId32, kVisibilityGetAtomicId64), atomic);
}

RwRaster* CreateTextureListingRasterNative(void* container, const void* entry)
{
    if (!container || !entry) {
        return nullptr;
    }
    return CHook::CallFunction<RwRaster*>(
        GtaAddress(kTextureListingCreateRaster32, kTextureListingCreateRaster64),
        container,
        entry);
}

RpSkin* RpSkinGeometryGetSkinNative(RpGeometry* geometry)
{
    if (!geometry) {
        return nullptr;
    }
    return CHook::CallFunction<RpSkin*>(GtaAddress(kRpSkinGeometryGetSkin32, kRpSkinGeometryGetSkin64), geometry);
}

RpHAnimHierarchy* RpSkinAtomicGetHAnimHierarchyNative(const RpAtomic* atomic)
{
    if (!atomic) {
        return nullptr;
    }
    return CHook::CallFunction<RpHAnimHierarchy*>(
        GtaAddress(kRpSkinAtomicGetHAnimHierarchy32, kRpSkinAtomicGetHAnimHierarchy64),
        atomic);
}

void SetAtomicModelInfoFlagsNative(void* modelInfo, uint32 flags)
{
    if (!modelInfo) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kAtomicModelInfoSetFlags32, kAtomicModelInfoSetFlags64), modelInfo, flags);
}

RpClump* GetClumpBoundingSphereNative(RpClump* clump, RwSphere* sphere, bool useLtm)
{
    if (!clump || !sphere) {
        return nullptr;
    }
    return CHook::CallFunction<RpClump*>(GtaAddress(kClumpBoundingSphere32, kClumpBoundingSphere64),
                                         clump,
                                         sphere,
                                         useLtm);
}

void BindCustomBuildingDNBalance(void* balanceParam)
{
    if (!balanceParam) {
        return;
    }
    CHook::Write(GtaAddress(kCustomBuildingDNBalance32, kCustomBuildingDNBalance64), balanceParam);
}

void AnimBlendHierarchyMoveMemoryNative(CAnimBlendHierarchy* hierarchy)
{
    if (!hierarchy) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kAnimBlendHierarchyMoveMemory32, kAnimBlendHierarchyMoveMemory64),
        hierarchy);
}

void ConstructAnimBlendStaticAssociationNative(CAnimBlendStaticAssociation* association,
                                               RpClump* clump,
                                               CAnimBlendHierarchy* hierarchy)
{
    if (!association) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kAnimBlendStaticAssociationCtor32, kAnimBlendStaticAssociationCtor64),
        association,
        clump,
        hierarchy);
}

void InitAnimBlendStaticAssociationNative(CAnimBlendStaticAssociation* association,
                                          RpClump* clump,
                                          CAnimBlendHierarchy* hierarchy)
{
    if (!association) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kAnimBlendStaticAssociationInit32, kAnimBlendStaticAssociationInit64),
        association,
        clump,
        hierarchy);
}

bool AnimBlendNodeNextKeyFrameNative(CAnimBlendNode* node)
{
    return node &&
           CHook::CallFunction<bool>(GtaAddress(kAnimBlendNodeNextKeyFrame32, kAnimBlendNodeNextKeyFrame64), node);
}

bool AnimBlendNodeNextKeyFrameCompressedNative(CAnimBlendNode* node)
{
    return node &&
           CHook::CallFunction<bool>(
               GtaAddress(kAnimBlendNodeNextKeyFrameCompressed32, kAnimBlendNodeNextKeyFrameCompressed64),
               node);
}

bool AnimBlendNodeNextKeyFrameNoCalcNative(CAnimBlendNode* node)
{
    return node &&
           CHook::CallFunction<bool>(
               GtaAddress(kAnimBlendNodeNextKeyFrameNoCalc32, kAnimBlendNodeNextKeyFrameNoCalc64),
               node);
}

bool AnimBlendNodeUpdateNative(CAnimBlendNode* node, CVector& trans, CQuaternion& rot, float weight)
{
    return node &&
           CHook::CallFunction<bool>(
               GtaAddress(kAnimBlendNodeUpdate32, kAnimBlendNodeUpdate64),
               node,
               trans,
               rot,
               weight);
}

bool AnimBlendNodeUpdateCompressedNative(CAnimBlendNode* node, CVector& trans, CQuaternion& rot, float weight)
{
    return node &&
           CHook::CallFunction<bool>(
               GtaAddress(kAnimBlendNodeUpdateCompressed32, kAnimBlendNodeUpdateCompressed64),
               node,
               trans,
               rot,
               weight);
}

void ConstructAnimBlendAssocGroupNative(CAnimBlendAssocGroup* group)
{
    if (!group) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kAnimBlendAssocGroupCtor32, kAnimBlendAssocGroupCtor64), group);
}

CAnimBlendAssociation* CopyAnimBlendAssocGroupAnimationNative(CAnimBlendAssocGroup* group, const char* animName)
{
    if (!group || !animName) {
        return nullptr;
    }
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kAnimBlendAssocGroupCopyByName32, kAnimBlendAssocGroupCopyByName64),
        group,
        animName);
}

CAnimBlendAssociation* CopyAnimBlendAssocGroupAnimationNative(CAnimBlendAssocGroup* group, uint32 animId)
{
    if (!group) {
        return nullptr;
    }
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kAnimBlendAssocGroupCopyById32, kAnimBlendAssocGroupCopyById64),
        group,
        animId);
}

void CreateAnimBlendAssociationsNative(const char* blockName)
{
    if (!blockName) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kAnimBlendAssocGroupCreateBlock32, kAnimBlendAssocGroupCreateBlock64),
                              blockName);
}

void CreateAnimBlendAssociationsNative(CAnimBlendAssocGroup* group,
                                       const char* animName,
                                       RpClump* clump,
                                       char** names,
                                       int32 animationCount)
{
    if (!group || !animName) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kAnimBlendAssocGroupCreateFromClump32, kAnimBlendAssocGroupCreateFromClump64),
        group,
        animName,
        clump,
        names,
        animationCount);
}

void CreateAnimBlendAssociationsNative(CAnimBlendAssocGroup* group,
                                       const char* animName,
                                       const char* arg2,
                                       const char* arg3,
                                       uint32 storageSize)
{
    if (!group || !animName) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kAnimBlendAssocGroupCreateFromNames32, kAnimBlendAssocGroupCreateFromNames64),
        group,
        animName,
        arg2,
        arg3,
        storageSize);
}

void DestroyAnimBlendAssociationsNative(CAnimBlendAssocGroup* group)
{
    if (!group) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kAnimBlendAssocGroupDestroyAssociations32, kAnimBlendAssocGroupDestroyAssociations64),
        group);
}

CAnimBlendStaticAssociation* GetAnimBlendAssocGroupAnimationNative(CAnimBlendAssocGroup* group,
                                                                   const char* animName)
{
    if (!group || !animName) {
        return nullptr;
    }
    return CHook::CallFunction<CAnimBlendStaticAssociation*>(
        GtaAddress(kAnimBlendAssocGroupGetByName32, kAnimBlendAssocGroupGetByName64),
        group,
        animName);
}

uint32 GetAnimBlendAssocGroupAnimationIdNative(CAnimBlendAssocGroup* group, const char* animName)
{
    if (!group || !animName) {
        return 0;
    }
    return CHook::CallFunction<uint32>(
        GtaAddress(kAnimBlendAssocGroupGetId32, kAnimBlendAssocGroupGetId64),
        group,
        animName);
}

void InitEmptyAnimBlendAssociationsNative(CAnimBlendAssocGroup* group, RpClump* clump)
{
    if (!group) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kAnimBlendAssocGroupInitEmpty32, kAnimBlendAssocGroupInitEmpty64),
                              group,
                              clump);
}

void DestroyAnimBlendAssocGroupNative(CAnimBlendAssocGroup* group)
{
    if (!group) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kAnimBlendAssocGroupDtor32, kAnimBlendAssocGroupDtor64), group);
}

int32 RtQuatConvertFromMatrixNative(RtQuat* quat, const RwMatrix* matrix)
{
    if (!quat || !matrix) {
        return 0;
    }
    return CHook::CallFunction<int32>(GtaAddress(kRtQuatConvertFromMatrix32, kRtQuatConvertFromMatrix64),
                                      quat,
                                      matrix);
}

RtQuat* RtQuatRotateNative(RtQuat* quat, const RwV3d* axis, RwReal angle, int32 combineOp)
{
    if (!quat || !axis) {
        return nullptr;
    }
    return CHook::CallFunction<RtQuat*>(GtaAddress(kRtQuatRotate32, kRtQuatRotate64),
                                        quat,
                                        axis,
                                        angle,
                                        combineOp);
}

const RtQuat* RtQuatQueryRotateNative(const RtQuat* quat, RwV3d* unitAxis, RwReal* angle)
{
    if (!quat || !unitAxis || !angle) {
        return nullptr;
    }
    return CHook::CallFunction<const RtQuat*>(GtaAddress(kRtQuatQueryRotate32, kRtQuatQueryRotate64),
                                              quat,
                                              unitAxis,
                                              angle);
}

RwV3d* RtQuatTransformVectorsNative(RwV3d* vectorsOut,
                                    const RwV3d* vectorsIn,
                                    int32 numPoints,
                                    const RtQuat* quat)
{
    if (!vectorsOut || !vectorsIn || !quat || numPoints <= 0) {
        return vectorsOut;
    }
    return CHook::CallFunction<RwV3d*>(GtaAddress(kRtQuatTransformVectors32, kRtQuatTransformVectors64),
                                       vectorsOut,
                                       vectorsIn,
                                       numPoints,
                                       quat);
}

RwReal RtQuatModulusNative(RtQuat* quat)
{
    if (!quat) {
        return 0.0f;
    }
    return CHook::CallFunction<RwReal>(GtaAddress(kRtQuatModulus32, kRtQuatModulus64), quat);
}

void DrawRaster(RwRaster* raster, const CRect& rect)
{
    if (!raster) {
        return;
    }

    const uint32_t white = 0xFFFFFFFF;
    auto setVertices = reinterpret_cast<void (*)(RwRaster*, const CRect&, uint32_t, uint32_t, uint32_t, uint32_t)>(
        g_libGTASA + kSpriteSetVerticesLegacy32);
    setVertices(raster, rect, white, white, white, white);
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, raster);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwIm2DRenderPrimitive(
        rwPRIMTYPETRIFAN,
        reinterpret_cast<RwIm2DVertex*>(g_libGTASA + kSpriteVertexBufferLegacy32),
        4);
}

void DrawTexture(uintptr_t texture, CRect* rect, uint32_t color)
{
    if (!texture || !rect) {
        return;
    }
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    auto draw = reinterpret_cast<void (*)(uintptr_t, CRect*, uint32_t*)>(
        g_libGTASA + kSpriteDrawLegacy32);
    draw(texture, rect, &color);
}

void DrawTextureUV(uintptr_t texture, CRect* rect, uint32_t color, const float* uv)
{
    if (!texture || !rect || !uv) {
        return;
    }
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    CHook::CallFunction<void>(
        GtaAddress(kSpriteDrawUV32, kSpriteDrawUV64),
        texture,
        rect,
        &color,
        uv[0],
        uv[1],
        uv[2],
        uv[3],
        uv[4],
        uv[5],
        uv[6],
        uv[7]);
}

void RenderRwClump(uintptr_t rwObject)
{
    if (!rwObject) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kRenderRwClump32, kRenderRwClump64), rwObject);
}

int32 RwMatrixDestroyNative(RwMatrix* matrix)
{
    if (!matrix) {
        return 0;
    }
    return CHook::CallFunction<int32>(GtaAddress(kRwMatrixDestroy32, kRwMatrixDestroy64), matrix);
}

RwV3d* RwV3dTransformPointNative(RwV3d* pointOut, const RwV3d* pointIn, const RwMatrix* matrix)
{
    if (!pointOut || !pointIn || !matrix) {
        return nullptr;
    }
    return CHook::CallFunction<RwV3d*>(
        GtaAddress(kRwV3dTransformPoint32, kRwV3dTransformPoint64),
        pointOut,
        pointIn,
        matrix);
}

RwV3d* RwV3dTransformPointsNative(RwV3d* pointsOut,
                                  const RwV3d* pointsIn,
                                  int32 numPoints,
                                  const RwMatrix* matrix)
{
    if (!pointsOut || !pointsIn || !matrix || numPoints <= 0) {
        return nullptr;
    }
    return CHook::CallFunction<RwV3d*>(
        GtaAddress(kRwV3dTransformPoints32, kRwV3dTransformPoints64),
        pointsOut,
        pointsIn,
        numPoints,
        matrix);
}

RwMatrix* RwMatrixOrthoNormalizeNative(RwMatrix* matrixOut, const RwMatrix* matrixIn)
{
    if (!matrixOut || !matrixIn) {
        return nullptr;
    }
    return CHook::CallFunction<RwMatrix*>(
        GtaAddress(kRwMatrixOrthoNormalize32, kRwMatrixOrthoNormalize64),
        matrixOut,
        matrixIn);
}

uint32 RwStreamReadNative(RwStream* stream, void* buffer, uint32 length)
{
    if (!stream || !buffer || length == 0) {
        return 0;
    }
    return CHook::CallFunction<uint32>(
        GtaAddress(kRwStreamRead32, kRwStreamRead64),
        stream,
        buffer,
        length);
}

RwStream* RwStreamOpenNative(int32 type, int32 accessType, const void* data)
{
    return CHook::CallFunction<RwStream*>(
        GtaAddress(kRwStreamOpen32, kRwStreamOpen64),
        type,
        accessType,
        data);
}

int32 RwStreamCloseNative(RwStream* stream, void* data)
{
    if (!stream) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRwStreamClose32, kRwStreamClose64),
        stream,
        data);
}

RwMatrix* RwMatrixTransformNative(RwMatrix* matrix, const RwMatrix* transform, int32 combineOp)
{
    if (!matrix || !transform) {
        return nullptr;
    }
    return CHook::CallFunction<RwMatrix*>(
        GtaAddress(kRwMatrixTransform32, kRwMatrixTransform64),
        matrix,
        transform,
        combineOp);
}

RwMatrix* RwMatrixCreateNative()
{
    return CHook::CallFunction<RwMatrix*>(GtaAddress(kRwMatrixCreate32, kRwMatrixCreate64));
}

RwMatrix* RwMatrixRotateNative(RwMatrix* matrix, const RwV3d* axis, RwReal angle, int32 combineOp)
{
    if (!matrix || !axis || !std::isfinite(angle)) {
        return nullptr;
    }
    return CHook::CallFunction<RwMatrix*>(
        GtaAddress(kRwMatrixRotate32, kRwMatrixRotate64),
        matrix,
        axis,
        angle,
        combineOp);
}

RwMatrix* RwMatrixTranslateNative(RwMatrix* matrix, const RwV3d* translation, int32 combineOp)
{
    if (!matrix || !translation) {
        return nullptr;
    }
    return CHook::CallFunction<RwMatrix*>(
        "_Z17RwMatrixTranslateP11RwMatrixTagPK5RwV3d15RwOpCombineType",
        matrix,
        translation,
        combineOp);
}

RpClump* RpClumpForAllAtomicsNative(RpClump* clump, RpAtomicCallbackFn callback, void* data)
{
    if (!clump || !callback) {
        return nullptr;
    }
    return CHook::CallFunction<RpClump*>(
        GtaAddress(kRpClumpForAllAtomics32, kRpClumpForAllAtomics64),
        clump,
        callback,
        data);
}

RpGeometry* RpGeometryForAllMaterialsNative(RpGeometry* geometry, RpMaterialCallbackFn callback, void* data)
{
    if (!geometry || !callback) {
        return nullptr;
    }
    return CHook::CallFunction<RpGeometry*>(
        GtaAddress(kRpGeometryForAllMaterials32, kRpGeometryForAllMaterials64),
        geometry,
        callback,
        data);
}

int32 RpClumpDestroyNative(RpClump* clump)
{
    if (!clump) {
        return 0;
    }
    return CHook::CallFunction<int32>(GtaAddress(kRpClumpDestroy32, kRpClumpDestroy64), clump);
}

RpClump* RpClumpRenderNative(RpClump* clump)
{
    if (!clump) {
        return nullptr;
    }
    return CHook::CallFunction<RpClump*>("_Z13RpClumpRenderP7RpClump", clump);
}

RpLight* RpLightCreateNative(int32 type)
{
    return CHook::CallFunction<RpLight*>(GtaAddress(kRpLightCreate32, kRpLightCreate64), type);
}

int32 RpLightDestroyNative(RpLight* light)
{
    if (!light) {
        return 0;
    }
    return CHook::CallFunction<int32>(GtaAddress(kRpLightDestroy32, kRpLightDestroy64), light);
}

RpWorld* RpWorldCreateNative(RwBBox* boundingBox)
{
    if (!boundingBox) {
        return nullptr;
    }
    return CHook::CallFunction<RpWorld*>(GtaAddress(kRpWorldCreate32, kRpWorldCreate64), boundingBox);
}

RpWorld* RpWorldAddCameraNative(RpWorld* world, RwCamera* camera)
{
    if (!world || !camera) {
        return nullptr;
    }
    return CHook::CallFunction<RpWorld*>(GtaAddress(kRpWorldAddCamera32, kRpWorldAddCamera64), world, camera);
}

RpLight* RpLightSetColorNative(RpLight* light, const RwRGBAReal* color)
{
    if (!light || !color) {
        return nullptr;
    }
    return CHook::CallFunction<RpLight*>(GtaAddress(kRpLightSetColor32, kRpLightSetColor64), light, color);
}

RpAtomic* AtomicDefaultRenderCallBackNative(RpAtomic* atomic)
{
    if (!atomic) {
        return nullptr;
    }
    return CHook::CallFunction<RpAtomic*>(
        GtaAddress(kAtomicDefaultRenderCallBack32, kAtomicDefaultRenderCallBack64),
        atomic);
}

RpWorld* RpWorldAddLightNative(RpWorld* world, RpLight* light)
{
    if (!world || !light) {
        return nullptr;
    }
    return CHook::CallFunction<RpWorld*>(GtaAddress(kRpWorldAddLight32, kRpWorldAddLight64), world, light);
}

RpWorld* RpWorldRemoveLightNative(RpWorld* world, RpLight* light)
{
    if (!world || !light) {
        return nullptr;
    }
    return CHook::CallFunction<RpWorld*>(GtaAddress(kRpWorldRemoveLight32, kRpWorldRemoveLight64), world, light);
}

int32 RpAtomicDestroyNative(RpAtomic* atomic)
{
    if (!atomic) {
        return 0;
    }
    return CHook::CallFunction<int32>(GtaAddress(kRpAtomicDestroy32, kRpAtomicDestroy64), atomic);
}

void RpClumpGtaCancelStreamNative()
{
    CHook::CallFunction<void>(GtaAddress(kRpClumpGtaCancelStream32, kRpClumpGtaCancelStream64));
}

void RtAnimAnimationFreeListCreateParamsNative(int32 blockSize, int32 numBlocksToPrealloc)
{
    CHook::CallFunction<void>(
        GtaAddress(kRtAnimAnimationFreeListCreateParams32, kRtAnimAnimationFreeListCreateParams64),
        blockSize,
        numBlocksToPrealloc);
}

int32 RtAnimInitializeNative()
{
    return CHook::CallFunction<int32>(GtaAddress(kRtAnimInitialize32, kRtAnimInitialize64));
}

int32 RtAnimRegisterInterpolationSchemeNative(RtAnimInterpolatorInfo* interpolatorInfo)
{
    if (!interpolatorInfo) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimRegisterInterpolationScheme32, kRtAnimRegisterInterpolationScheme64),
        interpolatorInfo);
}

RtAnimInterpolatorInfo* RtAnimGetInterpolatorInfoNative(int32 typeID)
{
    return CHook::CallFunction<RtAnimInterpolatorInfo*>(
        GtaAddress(kRtAnimGetInterpolatorInfo32, kRtAnimGetInterpolatorInfo64),
        typeID);
}

RtAnimAnimation* RtAnimAnimationCreateNative(int32 typeID, int32 numFrames, int32 flags, RwReal duration)
{
    if (!std::isfinite(duration)) {
        return nullptr;
    }
    return CHook::CallFunction<RtAnimAnimation*>(
        GtaAddress(kRtAnimAnimationCreate32, kRtAnimAnimationCreate64),
        typeID,
        numFrames,
        flags,
        duration);
}

int32 RtAnimAnimationDestroyNative(RtAnimAnimation* animation)
{
    if (!animation) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimAnimationDestroy32, kRtAnimAnimationDestroy64),
        animation);
}

RtAnimAnimation* RtAnimAnimationReadNative(const char* filename)
{
    if (!filename) {
        return nullptr;
    }
    return CHook::CallFunction<RtAnimAnimation*>(
        GtaAddress(kRtAnimAnimationRead32, kRtAnimAnimationRead64),
        filename);
}

int32 RtAnimAnimationWriteNative(const RtAnimAnimation* animation, const char* filename)
{
    if (!animation || !filename) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimAnimationWrite32, kRtAnimAnimationWrite64),
        animation,
        filename);
}

RtAnimAnimation* RtAnimAnimationStreamReadNative(RwStream* stream)
{
    if (!stream) {
        return nullptr;
    }
    return CHook::CallFunction<RtAnimAnimation*>(
        GtaAddress(kRtAnimAnimationStreamRead32, kRtAnimAnimationStreamRead64),
        stream);
}

int32 RtAnimAnimationStreamWriteNative(const RtAnimAnimation* animation, RwStream* stream)
{
    if (!animation || !stream) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimAnimationStreamWrite32, kRtAnimAnimationStreamWrite64),
        animation,
        stream);
}

int32 RtAnimAnimationStreamGetSizeNative(const RtAnimAnimation* animation)
{
    if (!animation) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimAnimationStreamGetSize32, kRtAnimAnimationStreamGetSize64),
        animation);
}

uint32 RtAnimAnimationGetNumNodesNative(const RtAnimAnimation* animation)
{
    if (!animation) {
        return 0;
    }
    return CHook::CallFunction<uint32>(
        GtaAddress(kRtAnimAnimationGetNumNodes32, kRtAnimAnimationGetNumNodes64),
        animation);
}

RtAnimInterpolator* RtAnimInterpolatorCreateNative(int32 numNodes, int32 maxInterpKeyFrameSize)
{
    return CHook::CallFunction<RtAnimInterpolator*>(
        GtaAddress(kRtAnimInterpolatorCreate32, kRtAnimInterpolatorCreate64),
        numNodes,
        maxInterpKeyFrameSize);
}

void RtAnimInterpolatorDestroyNative(RtAnimInterpolator* anim)
{
    if (!anim) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kRtAnimInterpolatorDestroy32, kRtAnimInterpolatorDestroy64), anim);
}

int32 RtAnimInterpolatorSetCurrentAnimNative(RtAnimInterpolator* animI, RtAnimAnimation* anim)
{
    if (!animI || !anim) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimInterpolatorSetCurrentAnim32, kRtAnimInterpolatorSetCurrentAnim64),
        animI,
        anim);
}

int32 RtAnimInterpolatorSetKeyFrameCallBacksNative(RtAnimInterpolator* anim, int32 keyFrameTypeID)
{
    if (!anim) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimInterpolatorSetKeyFrameCallBacks32, kRtAnimInterpolatorSetKeyFrameCallBacks64),
        anim,
        keyFrameTypeID);
}

void RtAnimInterpolatorSetAnimLoopCallBackNative(RtAnimInterpolator* anim, RtAnimCallbackFn callBack, void* data)
{
    if (!anim) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kRtAnimInterpolatorSetAnimLoopCallBack32, kRtAnimInterpolatorSetAnimLoopCallBack64),
        anim,
        callBack,
        data);
}

void RtAnimInterpolatorSetAnimCallBackNative(RtAnimInterpolator* anim,
                                             RtAnimCallbackFn callBack,
                                             RwReal time,
                                             void* data)
{
    if (!anim || !std::isfinite(time)) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kRtAnimInterpolatorSetAnimCallBack32, kRtAnimInterpolatorSetAnimCallBack64),
        anim,
        callBack,
        time,
        data);
}

int32 RtAnimInterpolatorCopyNative(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim)
{
    if (!outAnim || !inAnim) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimInterpolatorCopy32, kRtAnimInterpolatorCopy64),
        outAnim,
        inAnim);
}

int32 RtAnimInterpolatorSubAnimTimeNative(RtAnimInterpolator* anim, RwReal time)
{
    if (!anim || !std::isfinite(time)) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimInterpolatorSubAnimTime32, kRtAnimInterpolatorSubAnimTime64),
        anim,
        time);
}

int32 RtAnimInterpolatorAddAnimTimeNative(RtAnimInterpolator* anim, RwReal time)
{
    if (!anim || !std::isfinite(time)) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimInterpolatorAddAnimTime32, kRtAnimInterpolatorAddAnimTime64),
        anim,
        time);
}

int32 RtAnimInterpolatorSetCurrentTimeNative(RtAnimInterpolator* anim, RwReal time)
{
    if (!anim || !std::isfinite(time)) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimInterpolatorSetCurrentTime32, kRtAnimInterpolatorSetCurrentTime64),
        anim,
        time);
}

int32 RtAnimAnimationMakeDeltaNative(RtAnimAnimation* animation, int32 numNodes, RwReal time)
{
    if (!animation || !std::isfinite(time)) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimAnimationMakeDelta32, kRtAnimAnimationMakeDelta64),
        animation,
        numNodes,
        time);
}

int32 RtAnimInterpolatorBlendNative(RtAnimInterpolator* outAnim,
                                    RtAnimInterpolator* inAnim1,
                                    RtAnimInterpolator* inAnim2,
                                    RwReal alpha)
{
    if (!outAnim || !inAnim1 || !inAnim2 || !std::isfinite(alpha)) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimInterpolatorBlend32, kRtAnimInterpolatorBlend64),
        outAnim,
        inAnim1,
        inAnim2,
        alpha);
}

int32 RtAnimInterpolatorAddTogetherNative(RtAnimInterpolator* outAnim,
                                          RtAnimInterpolator* inAnim1,
                                          RtAnimInterpolator* inAnim2)
{
    if (!outAnim || !inAnim1 || !inAnim2) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimInterpolatorAddTogether32, kRtAnimInterpolatorAddTogether64),
        outAnim,
        inAnim1,
        inAnim2);
}

RtAnimInterpolator* RtAnimInterpolatorCreateSubInterpolatorNative(RtAnimInterpolator* parentAnim,
                                                                  int32 startNode,
                                                                  int32 numNodes,
                                                                  int32 maxInterpKeyFrameSize)
{
    if (!parentAnim) {
        return nullptr;
    }
    return CHook::CallFunction<RtAnimInterpolator*>(
        GtaAddress(kRtAnimInterpolatorCreateSubInterpolator32, kRtAnimInterpolatorCreateSubInterpolator64),
        parentAnim,
        startNode,
        numNodes,
        maxInterpKeyFrameSize);
}

int32 RtAnimInterpolatorBlendSubInterpolatorNative(RtAnimInterpolator* outAnim,
                                                   RtAnimInterpolator* inAnim1,
                                                   RtAnimInterpolator* inAnim2,
                                                   RwReal alpha)
{
    if (!outAnim || !inAnim1 || !inAnim2 || !std::isfinite(alpha)) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimInterpolatorBlendSubInterpolator32, kRtAnimInterpolatorBlendSubInterpolator64),
        outAnim,
        inAnim1,
        inAnim2,
        alpha);
}

int32 RtAnimInterpolatorAddSubInterpolatorNative(RtAnimInterpolator* outAnim,
                                                 RtAnimInterpolator* mainAnim,
                                                 RtAnimInterpolator* subAnim)
{
    if (!outAnim || !mainAnim || !subAnim) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRtAnimInterpolatorAddSubInterpolator32, kRtAnimInterpolatorAddSubInterpolator64),
        outAnim,
        mainAnim,
        subAnim);
}

void RpHAnimHierarchySetFreeListCreateParamsNative(int32 blockSize, int32 numBlocksToPrealloc)
{
    CHook::CallFunction<void>(
        GtaAddress(kRpHAnimHierarchySetFreeListCreateParams32, kRpHAnimHierarchySetFreeListCreateParams64),
        blockSize,
        numBlocksToPrealloc);
}

RpHAnimHierarchy* RpHAnimHierarchyCreateNative(int32 numNodes,
                                               uint32* nodeFlags,
                                               int32* nodeIDs,
                                               int32 flags,
                                               int32 maxInterpKeyFrameSize)
{
    if (!nodeFlags || !nodeIDs) {
        return nullptr;
    }
    return CHook::CallFunction<RpHAnimHierarchy*>(
        GtaAddress(kRpHAnimHierarchyCreate32, kRpHAnimHierarchyCreate64),
        numNodes,
        nodeFlags,
        nodeIDs,
        flags,
        maxInterpKeyFrameSize);
}

RpHAnimHierarchy* RpHAnimHierarchyCreateFromHierarchyNative(RpHAnimHierarchy* hierarchy,
                                                            int32 flags,
                                                            int32 maxInterpKeyFrameSize)
{
    if (!hierarchy) {
        return nullptr;
    }
    return CHook::CallFunction<RpHAnimHierarchy*>(
        GtaAddress(kRpHAnimHierarchyCreateFromHierarchy32, kRpHAnimHierarchyCreateFromHierarchy64),
        hierarchy,
        flags,
        maxInterpKeyFrameSize);
}

RpHAnimHierarchy* RpHAnimHierarchyDestroyNative(RpHAnimHierarchy* hierarchy)
{
    if (!hierarchy) {
        return nullptr;
    }
    return CHook::CallFunction<RpHAnimHierarchy*>(
        GtaAddress(kRpHAnimHierarchyDestroy32, kRpHAnimHierarchyDestroy64),
        hierarchy);
}

RpHAnimHierarchy* RpHAnimHierarchyCreateSubHierarchyNative(RpHAnimHierarchy* parentHierarchy,
                                                           int32 startNode,
                                                           int32 flags,
                                                           int32 maxInterpKeyFrameSize)
{
    if (!parentHierarchy) {
        return nullptr;
    }
    return CHook::CallFunction<RpHAnimHierarchy*>(
        GtaAddress(kRpHAnimHierarchyCreateSubHierarchy32, kRpHAnimHierarchyCreateSubHierarchy64),
        parentHierarchy,
        startNode,
        flags,
        maxInterpKeyFrameSize);
}

RpHAnimHierarchy* RpHAnimHierarchyAttachNative(RpHAnimHierarchy* hierarchy)
{
    if (!hierarchy) {
        return nullptr;
    }
    return CHook::CallFunction<RpHAnimHierarchy*>(
        GtaAddress(kRpHAnimHierarchyAttach32, kRpHAnimHierarchyAttach64),
        hierarchy);
}

RpHAnimHierarchy* RpHAnimHierarchyDetachNative(RpHAnimHierarchy* hierarchy)
{
    if (!hierarchy) {
        return nullptr;
    }
    return CHook::CallFunction<RpHAnimHierarchy*>(
        GtaAddress(kRpHAnimHierarchyDetach32, kRpHAnimHierarchyDetach64),
        hierarchy);
}

RpHAnimHierarchy* RpHAnimHierarchyAttachFrameIndexNative(RpHAnimHierarchy* hierarchy, int32 nodeIndex)
{
    if (!hierarchy) {
        return nullptr;
    }
    return CHook::CallFunction<RpHAnimHierarchy*>(
        GtaAddress(kRpHAnimHierarchyAttachFrameIndex32, kRpHAnimHierarchyAttachFrameIndex64),
        hierarchy,
        nodeIndex);
}

RpHAnimHierarchy* RpHAnimHierarchyDetachFrameIndexNative(RpHAnimHierarchy* hierarchy, int32 nodeIndex)
{
    if (!hierarchy) {
        return nullptr;
    }
    return CHook::CallFunction<RpHAnimHierarchy*>(
        GtaAddress(kRpHAnimHierarchyDetachFrameIndex32, kRpHAnimHierarchyDetachFrameIndex64),
        hierarchy,
        nodeIndex);
}

int32 RpHAnimFrameSetHierarchyNative(RwFrame* frame, RpHAnimHierarchy* hierarchy)
{
    if (!frame) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRpHAnimFrameSetHierarchy32, kRpHAnimFrameSetHierarchy64),
        frame,
        hierarchy);
}

RpHAnimHierarchy* RpHAnimFrameGetHierarchyNative(RwFrame* frame)
{
    if (!frame) {
        return nullptr;
    }
    return CHook::CallFunction<RpHAnimHierarchy*>(
        GtaAddress(kRpHAnimFrameGetHierarchy32, kRpHAnimFrameGetHierarchy64),
        frame);
}

RwMatrix* RpHAnimHierarchyGetMatrixArrayNative(RpHAnimHierarchy* hierarchy)
{
    if (!hierarchy) {
        return nullptr;
    }
    return CHook::CallFunction<RwMatrix*>(
        GtaAddress(kRpHAnimHierarchyGetMatrixArray32, kRpHAnimHierarchyGetMatrixArray64),
        hierarchy);
}

int32 RpHAnimHierarchyUpdateMatricesNative(RpHAnimHierarchy* hierarchy)
{
    if (!hierarchy) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRpHAnimHierarchyUpdateMatrices32, kRpHAnimHierarchyUpdateMatrices64),
        hierarchy);
}

int32 RpHAnimIDGetIndexNative(RpHAnimHierarchy* hierarchy, int32 id)
{
    if (!hierarchy) {
        return -1;
    }
    return CHook::CallFunction<int32>(GtaAddress(kRpHAnimIDGetIndex32, kRpHAnimIDGetIndex64), hierarchy, id);
}

int32 RpHAnimPluginAttachNative()
{
    return CHook::CallFunction<int32>(GtaAddress(kRpHAnimPluginAttach32, kRpHAnimPluginAttach64));
}

void RpHAnimKeyFrameApplyNative(void* matrix, void* voidIFrame)
{
    if (!matrix || !voidIFrame) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kRpHAnimKeyFrameApply32, kRpHAnimKeyFrameApply64), matrix, voidIFrame);
}

void RpHAnimKeyFrameBlendNative(void* voidOut, void* voidIn1, void* voidIn2, RwReal alpha)
{
    if (!voidOut || !voidIn1 || !voidIn2 || !std::isfinite(alpha)) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kRpHAnimKeyFrameBlend32, kRpHAnimKeyFrameBlend64),
        voidOut,
        voidIn1,
        voidIn2,
        alpha);
}

void RpHAnimKeyFrameInterpolateNative(void* voidOut,
                                      void* voidIn1,
                                      void* voidIn2,
                                      RwReal time,
                                      void* customData)
{
    if (!voidOut || !voidIn1 || !voidIn2 || !std::isfinite(time)) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kRpHAnimKeyFrameInterpolate32, kRpHAnimKeyFrameInterpolate64),
        voidOut,
        voidIn1,
        voidIn2,
        time,
        customData);
}

void RpHAnimKeyFrameAddNative(void* voidOut, void* voidIn1, void* voidIn2)
{
    if (!voidOut || !voidIn1 || !voidIn2) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kRpHAnimKeyFrameAdd32, kRpHAnimKeyFrameAdd64), voidOut, voidIn1, voidIn2);
}

void RpHAnimKeyFrameMulRecipNative(void* voidFrame, void* voidStart)
{
    if (!voidFrame || !voidStart) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kRpHAnimKeyFrameMulRecip32, kRpHAnimKeyFrameMulRecip64),
        voidFrame,
        voidStart);
}

RtAnimAnimation* RpHAnimKeyFrameStreamReadNative(RwStream* stream, RtAnimAnimation* animation)
{
    if (!stream) {
        return nullptr;
    }
    return CHook::CallFunction<RtAnimAnimation*>(
        GtaAddress(kRpHAnimKeyFrameStreamRead32, kRpHAnimKeyFrameStreamRead64),
        stream,
        animation);
}

int32 RpHAnimKeyFrameStreamWriteNative(const RtAnimAnimation* animation, RwStream* stream)
{
    if (!animation || !stream) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRpHAnimKeyFrameStreamWrite32, kRpHAnimKeyFrameStreamWrite64),
        animation,
        stream);
}

int32 RpHAnimKeyFrameStreamGetSizeNative(const RtAnimAnimation* animation)
{
    if (!animation) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRpHAnimKeyFrameStreamGetSize32, kRpHAnimKeyFrameStreamGetSize64),
        animation);
}

int32 RpHAnimFrameSetIDNative(RwFrame* frame, int32 id)
{
    if (!frame) {
        return 0;
    }
    return CHook::CallFunction<int32>(GtaAddress(kRpHAnimFrameSetID32, kRpHAnimFrameSetID64), frame, id);
}

int32 RpHAnimFrameGetIDNative(RwFrame* frame)
{
    if (!frame) {
        return -1;
    }
    return CHook::CallFunction<int32>(GtaAddress(kRpHAnimFrameGetID32, kRpHAnimFrameGetID64), frame);
}

RsGlobalType* GetRsGlobalNative()
{
    return reinterpret_cast<RsGlobalType*>(GtaAddress(kRsGlobal32, kRsGlobal64));
}

RwFrame* RwFrameUpdateObjectsNative(RwFrame* frame)
{
    if (!frame) {
        return nullptr;
    }
    return CHook::CallFunction<RwFrame*>(
        GtaAddress(kRwFrameUpdateObjects32, kRwFrameUpdateObjects64),
        frame);
}

RwTexture* RwTextureCreateNative(RwRaster* raster)
{
    if (!raster) {
        return nullptr;
    }
    return CHook::CallFunction<RwTexture*>(GtaAddress(kRwTextureCreate32, kRwTextureCreate64), raster);
}

RwCamera* RwCameraCreateNative()
{
    return CHook::CallFunction<RwCamera*>(GtaAddress(kRwCameraCreate32, kRwCameraCreate64));
}

RwFrame* RwFrameCreateNative()
{
    return CHook::CallFunction<RwFrame*>(GtaAddress(kRwFrameCreate32, kRwFrameCreate64));
}

RwCamera* RwCameraClearNative(RwCamera* camera, RwRGBA* colour, int32 clearMode)
{
    if (!camera) {
        return nullptr;
    }
    return CHook::CallFunction<RwCamera*>(
        GtaAddress(kRwCameraClear32, kRwCameraClear64),
        camera,
        colour,
        clearMode);
}

RwCamera* RwCameraSetNearClipPlaneNative(RwCamera* camera, RwReal nearClip)
{
    if (!camera || !std::isfinite(nearClip)) {
        return nullptr;
    }
    return CHook::CallFunction<RwCamera*>(
        GtaAddress(kRwCameraSetNearClipPlane32, kRwCameraSetNearClipPlane64),
        camera,
        nearClip);
}

RwCamera* RwCameraSetFarClipPlaneNative(RwCamera* camera, RwReal farClip)
{
    if (!camera || !std::isfinite(farClip)) {
        return nullptr;
    }
    return CHook::CallFunction<RwCamera*>(
        GtaAddress(kRwCameraSetFarClipPlane32, kRwCameraSetFarClipPlane64),
        camera,
        farClip);
}

RwFrame* RwFrameTranslateNative(RwFrame* frame, const RwV3d* v, int32 combine)
{
    if (!frame || !v) {
        return nullptr;
    }
    return CHook::CallFunction<RwFrame*>(
        GtaAddress(kRwFrameTranslate32, kRwFrameTranslate64),
        frame,
        v,
        combine);
}

RwFrame* RwFrameRotateNative(RwFrame* frame, const RwV3d* axis, RwReal angle, int32 combine)
{
    if (!frame || !axis || !std::isfinite(angle)) {
        return nullptr;
    }
    return CHook::CallFunction<RwFrame*>(
        GtaAddress(kRwFrameRotate32, kRwFrameRotate64),
        frame,
        axis,
        angle,
        combine);
}

RwCamera* RwCameraSetViewWindowNative(RwCamera* camera, const RwV2d* viewWindow)
{
    if (!camera || !viewWindow) {
        return nullptr;
    }
    return CHook::CallFunction<RwCamera*>(
        GtaAddress(kRwCameraSetViewWindow32, kRwCameraSetViewWindow64),
        camera,
        viewWindow);
}

RwCamera* RwCameraSetProjectionNative(RwCamera* camera, int32 projection)
{
    if (!camera) {
        return nullptr;
    }
    return CHook::CallFunction<RwCamera*>(
        GtaAddress(kRwCameraSetProjection32, kRwCameraSetProjection64),
        camera,
        projection);
}

void RwObjectHasFrameSetFrameNative(void* object, RwFrame* frame)
{
    if (!object) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kRwObjectHasFrameSetFrame32, kRwObjectHasFrameSetFrame64), object, frame);
}

RwMatrix* RwFrameGetLTMNative(RwFrame* frame)
{
    if (!frame) {
        return nullptr;
    }
    return CHook::CallFunction<RwMatrix*>(GtaAddress(kRwFrameGetLTM32, kRwFrameGetLTM64), frame);
}

RwCamera* RwCameraEndUpdateNative(RwCamera* camera)
{
    if (!camera) {
        return nullptr;
    }
    return CHook::CallFunction<RwCamera*>(GtaAddress(kRwCameraEndUpdate32, kRwCameraEndUpdate64), camera);
}

int32 RwIm3DEndNative()
{
    return CHook::CallFunction<int32>(GtaAddress(kRwIm3DEnd32, kRwIm3DEnd64));
}

int32 RwIm3DRenderPrimitiveNative(int32 primType)
{
    return CHook::CallFunction<int32>(
        GtaAddress(kRwIm3DRenderPrimitive32, kRwIm3DRenderPrimitive64),
        primType);
}

int32 RwIm3DRenderIndexedPrimitiveNative(int32 primType, uint32* indices, int32 numIndices)
{
    if (!indices && numIndices > 0) {
        return 0;
    }
    return CHook::CallFunction<int32>(
        GtaAddress(kRwIm3DRenderIndexedPrimitive32, kRwIm3DRenderIndexedPrimitive64),
        primType,
        indices,
        numIndices);
}

void* RwIm3DTransformNative(void* pVerts, uint32 numVerts, RwMatrix* ltm, uint32 flags)
{
    if (!pVerts && numVerts > 0) {
        return nullptr;
    }
    return CHook::CallFunction<void*>(
        GtaAddress(kRwIm3DTransform32, kRwIm3DTransform64),
        pVerts,
        numVerts,
        ltm,
        flags);
}

RwTexture* RwTextureReadNative(const char* name, const char* maskName)
{
    if (!name) {
        return nullptr;
    }
    return CHook::CallFunction<RwTexture*>(GtaAddress(kRwTextureRead32, kRwTextureRead64), name, maskName);
}

RwFrame* RwFrameForAllObjectsNative(RwFrame* frame, RwObjectCallbackFn callBack, void* data)
{
    if (!frame || !callBack) {
        return nullptr;
    }
    return CHook::CallFunction<RwFrame*>(
        GtaAddress(kRwFrameForAllObjects32, kRwFrameForAllObjects64),
        frame,
        callBack,
        data);
}

int32 RwFrameDestroyNative(RwFrame* frame)
{
    if (!frame) {
        return 0;
    }
    return CHook::CallFunction<int32>(GtaAddress(kRwFrameDestroy32, kRwFrameDestroy64), frame);
}

RwTexture* RwTextureSetRasterNative(RwTexture* texture, RwRaster* raster)
{
    if (!texture) {
        return nullptr;
    }
    return CHook::CallFunction<RwTexture*>(
        GtaAddress(kRwTextureSetRaster32, kRwTextureSetRaster64),
        texture,
        raster);
}

int32 RwCameraDestroyNative(RwCamera* camera)
{
    if (!camera) {
        return 0;
    }
    return CHook::CallFunction<int32>(GtaAddress(kRwCameraDestroy32, kRwCameraDestroy64), camera);
}

int32 RwIm3DRenderLineNative(int32 vert1, int32 vert2)
{
    return CHook::CallFunction<int32>(GtaAddress(kRwIm3DRenderLine32, kRwIm3DRenderLine64), vert1, vert2);
}

RwTexture* RwTextureSetNameNative(RwTexture* texture, const char* name)
{
    if (!texture || !name) {
        return nullptr;
    }
    return CHook::CallFunction<RwTexture*>(GtaAddress(kRwTextureSetName32, kRwTextureSetName64), texture, name);
}

RwFrame* RwFrameOrthoNormalizeNative(RwFrame* frame)
{
    if (!frame) {
        return nullptr;
    }
    return CHook::CallFunction<RwFrame*>(
        GtaAddress(kRwFrameOrthoNormalize32, kRwFrameOrthoNormalize64),
        frame);
}

int32 RwTextureSetFindCallBackNative(RwTextureFindCallbackFn callBack)
{
    return CHook::CallFunction<int32>(
        GtaAddress(kRwTextureSetFindCallBack32, kRwTextureSetFindCallBack64),
        callBack);
}

int32 RwTextureSetReadCallBackNative(RwTextureReadCallbackFn callBack)
{
    return CHook::CallFunction<int32>(
        GtaAddress(kRwTextureSetReadCallBack32, kRwTextureSetReadCallBack64),
        callBack);
}

void*& RwEngineInstanceRefNative()
{
    return *reinterpret_cast<void**>(GtaAddress(kRwEngineInstance32, kRwEngineInstance64));
}

int32& RxPipelineGlobalsOffsetRefNative()
{
    return *reinterpret_cast<int32*>(GtaAddress(kRxPipelineGlobalsOffset32, kRxPipelineGlobalsOffset64));
}

RxPipeline* RxPipelineCreateNative()
{
    return CHook::CallFunction<RxPipeline*>("_Z16RxPipelineCreatev");
}

uint32 RxChaseDependenciesNative(RxPipeline* pipeline)
{
    if (!pipeline) {
        return 0;
    }
    return CHook::CallFunction<uint32>("_Z20_rxChaseDependenciesP10RxPipeline", pipeline);
}

RxPipeline* RxPipelineLockNative(RxPipeline* pipeline)
{
    if (!pipeline) {
        return nullptr;
    }
    return CHook::CallFunction<RxPipeline*>("_Z14RxPipelineLockP10RxPipeline", pipeline);
}

RxPipelineNode* RxPipelineFindNodeByNameNative(RxPipeline* pipeline,
                                               const char* name,
                                               RxPipelineNode* start,
                                               int32* nodeIndex)
{
    if (!pipeline || !name) {
        return nullptr;
    }
    return CHook::CallFunction<RxPipelineNode*>(
        "_Z24RxPipelineFindNodeByNameP10RxPipelinePKcP14RxPipelineNodePi",
        pipeline,
        name,
        start,
        nodeIndex);
}

RxPipeline* RxLockedPipeAddFragmentNative(RxPipeline* pipeline,
                                          uint32* firstIndex,
                                          RxNodeDefinition* nodeDef0,
                                          uint32 nodeUnk)
{
    if (!pipeline || !firstIndex || !nodeDef0) {
        return nullptr;
    }
    return CHook::CallFunction<RxPipeline*>(
        "_Z23RxLockedPipeAddFragmentP10RxPipelinePjP16RxNodeDefinitionz",
        pipeline,
        firstIndex,
        nodeDef0,
        nodeUnk);
}

RxNodeDefinition* RxNodeDefinitionGetOpenGLAtomicAllInOneNative()
{
    return CHook::CallFunction<RxNodeDefinition*>("_Z39RxNodeDefinitionGetOpenGLAtomicAllInOnev");
}

void RxOpenGLAllInOneSetInstanceCallBackNative(RxPipelineNode* node, uintptr_t instanceCB)
{
    if (!node || !instanceCB) {
        return;
    }

    CHook::CallFunction<void>(
        "_Z35RxOpenGLAllInOneSetInstanceCallBackP14RxPipelineNodePFiPvP24RxOpenGLMeshInstanceDataiiE",
        node,
        reinterpret_cast<void*>(instanceCB));
}

void RxOpenGLAllInOneSetRenderCallBackNative(RxPipelineNode* node, uintptr_t renderCB)
{
    if (!node || !renderCB) {
        return;
    }

    CHook::CallFunction<void>(
        "_Z33RxOpenGLAllInOneSetRenderCallBackP14RxPipelineNodePFvP10RwResEntryPvhjE",
        node,
        reinterpret_cast<void*>(renderCB));
}

void RxPipelineDestroyNative(RxPipeline* pipeline)
{
    if (!pipeline) {
        return;
    }

    CHook::CallFunction<void>("_Z18_rxPipelineDestroyP10RxPipeline", pipeline);
}

const char* GetFrameNodeNameNative(RwFrame* frame)
{
    if (!frame) {
        return nullptr;
    }

    return CHook::CallFunction<const char*>(g_libGTASA + kFrameNodeNameLegacy32, frame);
}

bool RpAnimBlendPluginAttachNative()
{
    return CHook::CallFunction<bool>(GtaAddress(kRpAnimBlendPluginAttach32, kRpAnimBlendPluginAttach64));
}

void* RtAnimBlendKeyFrameApplyNative(void* result, void* frame)
{
    return CHook::CallFunction<void*>(
        GtaAddress(kRtAnimBlendKeyFrameApply32, kRtAnimBlendKeyFrameApply64),
        result,
        frame);
}

CAnimBlendClumpData* RpAnimBlendAllocateDataNative(RpClump* clump)
{
    return CHook::CallFunction<CAnimBlendClumpData*>(
        GtaAddress(kRpAnimBlendAllocateData32, kRpAnimBlendAllocateData64),
        clump);
}

CAnimBlendAssociation* RpAnimBlendClumpAddAssociationNative(RpClump* clump,
                                                            CAnimBlendAssociation* association,
                                                            uint32 flags,
                                                            RwReal startTime,
                                                            RwReal blendAmount)
{
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kRpAnimBlendClumpAddAssociation32, kRpAnimBlendClumpAddAssociation64),
        clump,
        association,
        flags,
        startTime,
        blendAmount);
}

void RpAnimBlendClumpFillFrameArrayNative(RpClump* clump, AnimBlendFrameData** frameData)
{
    CHook::CallFunction<void>(
        GtaAddress(kRpAnimBlendClumpFillFrameArray32, kRpAnimBlendClumpFillFrameArray64),
        clump,
        frameData);
}

AnimBlendFrameData* RpAnimBlendClumpFindBoneNative(RpClump* clump, uint32 id)
{
    return CHook::CallFunction<AnimBlendFrameData*>(
        GtaAddress(kRpAnimBlendClumpFindBone32, kRpAnimBlendClumpFindBone64),
        clump,
        id);
}

AnimBlendFrameData* RpAnimBlendClumpFindFrameNative(RpClump* clump, const char* name)
{
    return CHook::CallFunction<AnimBlendFrameData*>(
        GtaAddress(kRpAnimBlendClumpFindFrame32, kRpAnimBlendClumpFindFrame64),
        clump,
        name);
}

AnimBlendFrameData* RpAnimBlendClumpFindFrameFromHashKeyNative(RpClump* clump, uint32 key)
{
    return CHook::CallFunction<AnimBlendFrameData*>(
        GtaAddress(kRpAnimBlendClumpFindFrameFromHash32, kRpAnimBlendClumpFindFrameFromHash64),
        clump,
        key);
}

CAnimBlendAssociation* RpAnimBlendClumpGetAssociationByHierarchyNative(RpClump* clump,
                                                                       bool stopFunctionConfusion,
                                                                       CAnimBlendHierarchy* hierarchy)
{
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kRpAnimBlendClumpGetAssociationByHierarchy32, kRpAnimBlendClumpGetAssociationByHierarchy64),
        clump,
        stopFunctionConfusion,
        hierarchy);
}

CAnimBlendAssociation* RpAnimBlendClumpGetAssociationByNameNative(RpClump* clump, const char* name)
{
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kRpAnimBlendClumpGetAssociationByName32, kRpAnimBlendClumpGetAssociationByName64),
        clump,
        name);
}

CAnimBlendAssociation* RpAnimBlendClumpGetAssociationByAnimIdNative(RpClump* clump, uint32 animId)
{
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kRpAnimBlendClumpGetAssociationByAnimId32, kRpAnimBlendClumpGetAssociationByAnimId64),
        clump,
        animId);
}

CAnimBlendAssociation* RpAnimBlendClumpGetFirstAssociationByFlagsNative(RpClump* clump, uint32 flags)
{
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kRpAnimBlendClumpGetFirstAssociationByFlags32,
                   kRpAnimBlendClumpGetFirstAssociationByFlags64),
        clump,
        flags);
}

CAnimBlendAssociation* RpAnimBlendClumpGetMainAssociationNative(RpClump* clump,
                                                                CAnimBlendAssociation** pp2ndAnim,
                                                                RwReal* pBlendVal2nd)
{
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kRpAnimBlendClumpGetMainAssociation32, kRpAnimBlendClumpGetMainAssociation64),
        clump,
        pp2ndAnim,
        pBlendVal2nd);
}

CAnimBlendAssociation* RpAnimBlendClumpGetMainAssociationNNative(RpClump* clump, int32 n)
{
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kRpAnimBlendClumpGetMainAssociationN32, kRpAnimBlendClumpGetMainAssociationN64),
        clump,
        n);
}

CAnimBlendAssociation* RpAnimBlendClumpGetMainPartialAssociationNative(RpClump* clump)
{
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kRpAnimBlendClumpGetMainPartialAssociation32, kRpAnimBlendClumpGetMainPartialAssociation64),
        clump);
}

CAnimBlendAssociation* RpAnimBlendClumpGetMainPartialAssociationNNative(RpClump* clump, int32 n)
{
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kRpAnimBlendClumpGetMainPartialAssociationN32,
                   kRpAnimBlendClumpGetMainPartialAssociationN64),
        clump,
        n);
}

uint32 RpAnimBlendClumpGetNumAssociationsNative(RpClump* clump)
{
    return CHook::CallFunction<uint32>(
        GtaAddress(kRpAnimBlendClumpGetNumAssociations32, kRpAnimBlendClumpGetNumAssociations64),
        clump);
}

uint32 RpAnimBlendClumpGetNumNonPartialAssociationsNative(RpClump* clump)
{
    return CHook::CallFunction<uint32>(
        GtaAddress(kRpAnimBlendClumpGetNumNonPartialAssociations32,
                   kRpAnimBlendClumpGetNumNonPartialAssociations64),
        clump);
}

uint32 RpAnimBlendClumpGetNumPartialAssociationsNative(RpClump* clump)
{
    return CHook::CallFunction<uint32>(
        GtaAddress(kRpAnimBlendClumpGetNumPartialAssociations32, kRpAnimBlendClumpGetNumPartialAssociations64),
        clump);
}

void RpAnimBlendClumpGiveAssociationsNative(RpClump* clump, CAnimBlendAssociation* association)
{
    CHook::CallFunction<void>(
        GtaAddress(kRpAnimBlendClumpGiveAssociations32, kRpAnimBlendClumpGiveAssociations64),
        clump,
        association);
}

void RpAnimBlendClumpInitNative(RpClump* clump)
{
    CHook::CallFunction<void>(GtaAddress(kRpAnimBlendClumpInit32, kRpAnimBlendClumpInit64), clump);
}

bool RpAnimBlendClumpIsInitializedNative(RpClump* clump)
{
    return CHook::CallFunction<bool>(
        GtaAddress(kRpAnimBlendClumpIsInitialized32, kRpAnimBlendClumpIsInitialized64),
        clump);
}

void RpAnimBlendClumpPauseAllAnimationsNative(RpClump* clump)
{
    CHook::CallFunction<void>(
        GtaAddress(kRpAnimBlendClumpPauseAllAnimations32, kRpAnimBlendClumpPauseAllAnimations64),
        clump);
}

void RpAnimBlendClumpRemoveAssociationsNative(RpClump* clump, uint32 flags)
{
    CHook::CallFunction<void>(
        GtaAddress(kRpAnimBlendClumpRemoveAssociations32, kRpAnimBlendClumpRemoveAssociations64),
        clump,
        flags);
}

void RpAnimBlendClumpSetBlendDeltasNative(RpClump* clump, uint32 flags, RwReal delta)
{
    CHook::CallFunction<void>(
        GtaAddress(kRpAnimBlendClumpSetBlendDeltas32, kRpAnimBlendClumpSetBlendDeltas64),
        clump,
        flags,
        delta);
}

void RpAnimBlendClumpUnPauseAllAnimationsNative(RpClump* clump)
{
    CHook::CallFunction<void>(
        GtaAddress(kRpAnimBlendClumpUnPauseAllAnimations32, kRpAnimBlendClumpUnPauseAllAnimations64),
        clump);
}

void RpAnimBlendClumpUpdateAnimationsNative(RpClump* clump, RwReal step, bool onScreen)
{
    CHook::CallFunction<void>(
        GtaAddress(kRpAnimBlendClumpUpdateAnimations32, kRpAnimBlendClumpUpdateAnimations64),
        clump,
        step,
        onScreen);
}

RtAnimAnimation* RpAnimBlendCreateAnimationForHierarchyNative(RpHAnimHierarchy* hierarchy)
{
    return CHook::CallFunction<RtAnimAnimation*>(
        GtaAddress(kRpAnimBlendCreateAnimationForHierarchy32, kRpAnimBlendCreateAnimationForHierarchy64),
        hierarchy);
}

char* RpAnimBlendFrameGetNameNative(RwFrame* frame)
{
    return CHook::CallFunction<char*>(
        GtaAddress(kRpAnimBlendFrameGetName32, kRpAnimBlendFrameGetName64),
        frame);
}

void RpAnimBlendFrameSetNameNative(RwFrame* frame, char* name)
{
    CHook::CallFunction<void>(
        GtaAddress(kRpAnimBlendFrameSetName32, kRpAnimBlendFrameSetName64),
        frame,
        name);
}

CAnimBlendAssociation* RpAnimBlendGetNextAssociationNative(CAnimBlendAssociation* association)
{
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kRpAnimBlendGetNextAssociation32, kRpAnimBlendGetNextAssociation64),
        association);
}

CAnimBlendAssociation* RpAnimBlendGetNextAssociationByFlagsNative(CAnimBlendAssociation* association, uint32 flags)
{
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kRpAnimBlendGetNextAssociationByFlags32, kRpAnimBlendGetNextAssociationByFlags64),
        association,
        flags);
}

void RpAnimBlendKeyFrameInterpolateNative(void* pVoidOut,
                                          void* pVoidIn1,
                                          void* pVoidIn2,
                                          RwReal time,
                                          void* customData)
{
    CHook::CallFunction<void>(
        GtaAddress(kRpAnimBlendKeyFrameInterpolate32, kRpAnimBlendKeyFrameInterpolate64),
        pVoidOut,
        pVoidIn1,
        pVoidIn2,
        time,
        customData);
}

void ReadAnimAssociationDefinitionsNative()
{
    CHook::CallFunction<void>(
        GtaAddress(kAnimManagerReadAssocDefinitions32, kAnimManagerReadAssocDefinitions64));
}

CAnimBlendAssociation* AddAnimationNative(RpClump* clump, int32 groupId, int32 animId)
{
    if (!clump) {
        return nullptr;
    }
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kAnimManagerAddAnimation32, kAnimManagerAddAnimation64),
        clump,
        groupId,
        animId);
}

CAnimBlendAssociation* AddAnimationByHierarchyNative(RpClump* clump,
                                                     CAnimBlendHierarchy* hier,
                                                     int32 clumpAssocFlag)
{
    if (!clump || !hier) {
        return nullptr;
    }
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kAnimManagerAddAnimationByHierarchy32, kAnimManagerAddAnimationByHierarchy64),
        clump,
        hier,
        clumpAssocFlag);
}

CAnimBlendAssociation* AddAnimationAndSyncNative(RpClump* clump,
                                                 CAnimBlendAssociation* animBlendAssoc,
                                                 int32 groupId,
                                                 int32 animId)
{
    if (!clump) {
        return nullptr;
    }
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kAnimManagerAddAnimationAndSync32, kAnimManagerAddAnimationAndSync64),
        clump,
        animBlendAssoc,
        groupId,
        animId);
}

AnimAssocDefinition* AddAnimAssocDefinitionNative(const char* groupName,
                                                  const char* blockName,
                                                  uint32 modelIndex,
                                                  uint32 animsCount,
                                                  AnimDescriptor* descriptor)
{
    if (!groupName || !blockName || !descriptor) {
        return nullptr;
    }
    return CHook::CallFunction<AnimAssocDefinition*>(
        GtaAddress(kAnimManagerAddAssocDefinition32, kAnimManagerAddAssocDefinition64),
        groupName,
        blockName,
        modelIndex,
        animsCount,
        descriptor);
}

RpClump* CreateModelInfoClumpNative(CBaseModelInfo* model)
{
    if (!model || !model->vtable) {
        return nullptr;
    }
    const uintptr_t createClumpFn = *reinterpret_cast<uintptr_t*>(
        model->vtable + (VER_x32 ? kModelInfoCreateClumpVtableOffset32 : kModelInfoCreateClumpVtableOffset64));
    if (!createClumpFn) {
        return nullptr;
    }
    return CHook::CallFunction<RpClump*>(createClumpFn, model);
}

void RemoveLastAnimFileNative()
{
    CHook::CallFunction<void>(
        GtaAddress(kAnimManagerRemoveLastAnimFile32, kAnimManagerRemoveLastAnimFile64));
}

void RemoveAnimBlockNative(int32 index)
{
    CHook::CallFunction<void>(
        GtaAddress(kAnimManagerRemoveAnimBlock32, kAnimManagerRemoveAnimBlock64),
        index);
}

void UncompressAnimationNative(CAnimBlendHierarchy* hier)
{
    if (!hier) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kAnimManagerUncompressAnimation32, kAnimManagerUncompressAnimation64),
        hier);
}

void RemoveFromUncompressedCacheNative(CAnimBlendHierarchy* hier)
{
    if (!hier) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kAnimManagerRemoveFromUncompressedCache32, kAnimManagerRemoveFromUncompressedCache64),
        hier);
}

CAnimBlendAssociation* BlendAnimationByIdNative(RpClump* clump,
                                                int32 groupId,
                                                int32 animId,
                                                float blendData)
{
    if (!clump || !std::isfinite(blendData)) {
        return nullptr;
    }
    return CHook::CallFunction<CAnimBlendAssociation*>(
        GtaAddress(kAnimManagerBlendAnimationById32, kAnimManagerBlendAnimationById64),
        clump,
        groupId,
        animId,
        blendData);
}

void InstallAnimManagerHooksNative(void* numAnimAssocDefinitions,
                                   void* animBlocks,
                                   void* numAnimBlocks,
                                   void* animAssocGroups,
                                   void* animations,
                                   void* numAnimations,
                                   void* animCache,
                                   uintptr_t getAssocByIdHook,
                                   uintptr_t getAssocByNameHook,
                                   uintptr_t initialiseHook,
                                   uintptr_t loadAnimFilesHook)
{
    CHook::Write(GtaAddress(kAnimManagerNumAnimAssocDefinitions32, kAnimManagerNumAnimAssocDefinitions64),
                 numAnimAssocDefinitions);
    CHook::Write(GtaAddress(kAnimManagerAnimBlocks32, kAnimManagerAnimBlocks64), animBlocks);
    CHook::Write(GtaAddress(kAnimManagerNumAnimBlocks32, kAnimManagerNumAnimBlocks64), numAnimBlocks);
    CHook::Write(GtaAddress(kAnimManagerAnimAssocGroups32, kAnimManagerAnimAssocGroups64), animAssocGroups);
    CHook::Write(GtaAddress(kAnimManagerAnimations32, kAnimManagerAnimations64), animations);
    CHook::Write(GtaAddress(kAnimManagerNumAnimations32, kAnimManagerNumAnimations64), numAnimations);
    CHook::Write(GtaAddress(kAnimManagerAnimCache32, kAnimManagerAnimCache64), animCache);

    CHook::Redirect("_ZN12CAnimManager18GetAnimAssociationE12AssocGroupId11AnimationId",
                    reinterpret_cast<void*>(getAssocByIdHook));
    CHook::Redirect("_ZN12CAnimManager18GetAnimAssociationE12AssocGroupIdPKc",
                    reinterpret_cast<void*>(getAssocByNameHook));
    CHook::Redirect("_ZN12CAnimManager10InitialiseEv", reinterpret_cast<void*>(initialiseHook));
    CHook::Redirect("_ZN12CAnimManager13LoadAnimFilesEv", reinterpret_cast<void*>(loadAnimFilesHook));
}

uint8_t BaseModelInfoGetModelTypeNative(CBaseModelInfo* model)
{
    if (!model || !model->vtable) {
        return 0;
    }

    const uintptr_t fn = *reinterpret_cast<uintptr_t*>(
        model->vtable + (VER_x32 ? kBaseModelInfoGetModelTypeVtableOffset32
                                  : kBaseModelInfoGetModelTypeVtableOffset64));
    if (!fn) {
        return 0;
    }

    return CHook::CallFunction<uint8_t>(fn, model);
}

int32 BaseModelInfoGetAnimFileIndexNative(CBaseModelInfo* model)
{
    if (!model || !model->vtable) {
        return -1;
    }

    const uintptr_t fn = *reinterpret_cast<uintptr_t*>(
        model->vtable + (VER_x32 ? kBaseModelInfoGetAnimFileIndexVtableOffset32
                                  : kBaseModelInfoGetAnimFileIndexVtableOffset64));
    if (!fn) {
        return -1;
    }

    return CHook::CallFunction<int32>(fn, model);
}

void BaseModelInfoDeleteRwObjectNative(CBaseModelInfo* model)
{
    if (!model || !model->vtable) {
        return;
    }

    const uintptr_t fn = *reinterpret_cast<uintptr_t*>(
        model->vtable + (VER_x32 ? kBaseModelInfoDeleteRwObjectVtableOffset32
                                  : kBaseModelInfoDeleteRwObjectVtableOffset64));
    if (!fn) {
        return;
    }

    CHook::CallFunction<void>(fn, model);
}

void ClumpModelInfoSetClumpNative(CClumpModelInfo* model, RpClump* clump)
{
    if (!model) {
        return;
    }

    CHook::CallFunction<void>("_ZN15CClumpModelInfo8SetClumpEP7RpClump", model, clump);
}

void ClumpModelInfoSetFrameIdsNative(CClumpModelInfo* model, RwObjectNameIdAssocation* data)
{
    if (!model || !data) {
        return;
    }

    CHook::CallFunction<void>("_ZN15CClumpModelInfo11SetFrameIdsEP24RwObjectNameIdAssocation", model, data);
}

void ClumpModelInfoDeleteRwObjectNative(CClumpModelInfo* model)
{
    if (!model) {
        return;
    }

    CHook::CallFunction<void>("_ZN15CClumpModelInfo14DeleteRwObjectEv", model);
}

RwFrame* ClumpModelInfoGetFrameFromNameNative(RpClump* clump, const char* name)
{
    if (!clump || !name) {
        return nullptr;
    }

    return CHook::CallFunction<RwFrame*>(
        GtaAddress(kClumpModelInfoGetFrameFromName32, kClumpModelInfoGetFrameFromName64),
        clump,
        name);
}

static void ConstructModelInfoNative(CBaseModelInfo* model, uintptr_t ctor32, uintptr_t ctor64, uintptr_t vtable32, uintptr_t vtable64)
{
    if (!model) {
        return;
    }

    CHook::CallFunction<void>(GtaAddress(ctor32, ctor64), model);
    model->vtable = GtaAddress(vtable32, vtable64);

    const uintptr_t postCtor = *reinterpret_cast<uintptr_t*>(
        model->vtable + (VER_x32 ? kModelInfoPostConstructorVtableOffset32
                                  : kModelInfoPostConstructorVtableOffset64));
    if (postCtor) {
        CHook::CallFunction<void>(postCtor, model);
    }
}

void ConstructVehicleModelInfoNative(CBaseModelInfo* model)
{
    ConstructModelInfoNative(model,
                             kVehicleModelInfoCtor32,
                             kVehicleModelInfoCtor64,
                             kVehicleModelInfoVtable32,
                             kVehicleModelInfoVtable64);
}

void ConstructPedModelInfoNative(CBaseModelInfo* model)
{
    ConstructModelInfoNative(model,
                             kPedModelInfoCtor32,
                             kPedModelInfoCtor64,
                             kPedModelInfoVtable32,
                             kPedModelInfoVtable64);
}

void ConstructAtomicModelInfoNative(CBaseModelInfo* model)
{
    ConstructModelInfoNative(model,
                             kAtomicModelInfoCtor32,
                             kAtomicModelInfoCtor64,
                             kAtomicModelInfoVtable32,
                             kAtomicModelInfoVtable64);
}

CDamageAtomicModelInfo* AddDamageAtomicModelNative(int32 index)
{
    return CHook::CallFunction<CDamageAtomicModelInfo*>(
        GtaAddress(kModelInfoAddDamageAtomic32, kModelInfoAddDamageAtomic64),
        index);
}

void InstallModelInfoHooksNative(void* atomicModelInfoStore,
                                 void* pedModelInfoStore,
                                 void* vehicleModelInfoStore,
                                 void* modelInfoPtrs,
                                 uintptr_t addPedModelHook,
                                 uintptr_t addVehicleModelHook,
                                 uintptr_t addAtomicModelHook)
{
    CHook::Write(GtaAddress(kModelInfoAtomicStore32, kModelInfoAtomicStore64), atomicModelInfoStore);
    CHook::Write(GtaAddress(kModelInfoPedStore32, kModelInfoPedStore64), pedModelInfoStore);
    CHook::Write(GtaAddress(kModelInfoVehicleStore32, kModelInfoVehicleStore64), vehicleModelInfoStore);
    CHook::Write(GtaAddress(kModelInfoPtrs32, kModelInfoPtrs64), modelInfoPtrs);

    CHook::Redirect("_ZN10CModelInfo11AddPedModelEi", reinterpret_cast<void*>(addPedModelHook));
    CHook::Redirect("_ZN10CModelInfo15AddVehicleModelEi", reinterpret_cast<void*>(addVehicleModelHook));
    CHook::Redirect("_ZN10CModelInfo14AddAtomicModelEi", reinterpret_cast<void*>(addAtomicModelHook));
}

RwObjectNameIdAssocation* VehicleModelInfoDescriptorNative(int32 vehicleType)
{
    if (vehicleType < 0 || vehicleType >= kVehicleModelInfoDescCount) {
        return nullptr;
    }

    auto* descriptors = reinterpret_cast<RwObjectNameIdAssocation**>(
        GtaAddress(kVehicleModelInfoDescs32, kVehicleModelInfoDescs64));
    return descriptors[vehicleType];
}

void VehicleModelInfoSetAtomicRenderCallbacksNative(CVehicleModelInfo* model)
{
    if (!model) {
        return;
    }

    CHook::CallFunction<void>("_ZN17CVehicleModelInfo24SetAtomicRenderCallbacksEv", model);
}

void VehicleModelInfoReduceMaterialsNative(CVehicleModelInfo* model)
{
    if (!model) {
        return;
    }

    CHook::CallFunction<void>("_ZN17CVehicleModelInfo24ReduceMaterialsInVehicleEv", model);
}

void VehicleModelInfoPreprocessHierarchyNative(CVehicleModelInfo* model)
{
    if (!model) {
        return;
    }

    CHook::CallFunction<void>("_ZN17CVehicleModelInfo19PreprocessHierarchyEv", model);
}

void VehicleModelInfoSetRenderPipelinesNative(CVehicleModelInfo* model)
{
    if (!model) {
        return;
    }

    CHook::CallFunction<void>("_ZN17CVehicleModelInfo18SetRenderPipelinesEv", model);
}

RxPipeline* CreateCustomCarOpenGLObjPipeNative()
{
    return CHook::CallFunction<RxPipeline*>("_ZN24CCustomCarEnvMapPipeline25CreateCustomOpenGLObjPipeEv");
}

int32 RpMatFXMaterialGetEffectsNative(RpMaterial* material)
{
    return CHook::CallFunction<int32>("_Z25RpMatFXMaterialGetEffectsPK10RpMaterial", material);
}

void CustomBuildingDNSetFxEnvTextureNative(RpMaterial* material, RwTexture* texture)
{
    CHook::CallFunction<void>(
        "_ZN25CCustomBuildingDNPipeline15SetFxEnvTextureEP10RpMaterialP9RwTexture",
        material,
        texture);
}

RwReal CustomCarEnvMapGetFxEnvShininessNative(RpMaterial* material)
{
    return CHook::CallFunction<RwReal>("_ZN24CCustomCarEnvMapPipeline17GetFxEnvShininessEP10RpMaterial",
                                       material);
}

RwTexture* CustomCarEnvMapGetFxEnvTextureNative(RpMaterial* material)
{
    return CHook::CallFunction<RwTexture*>("_ZN24CCustomCarEnvMapPipeline15GetFxEnvTextureEP10RpMaterial",
                                           material);
}

RwReal CustomCarEnvMapGetFxSpecSpecularityNative(RpMaterial* material)
{
    return CHook::CallFunction<RwReal>("_ZN24CCustomCarEnvMapPipeline20GetFxSpecSpecularityEP10RpMaterial",
                                       material);
}

RwTexture* CustomCarEnvMapGetFxSpecTextureNative(RpMaterial* material)
{
    return CHook::CallFunction<RwTexture*>("_ZN24CCustomCarEnvMapPipeline16GetFxSpecTextureEP10RpMaterial",
                                           material);
}

void InstallCustomCarEnvMapPipelineHooksNative(uintptr_t worldSectorInitHook,
                                               void** worldSectorInitOriginal,
                                               uintptr_t atomicInitHook,
                                               void** atomicInitOriginal,
                                               void* objPipeline,
                                               void* envMapPipeMatDataPool,
                                               void* envMapPipeAtmDataPool,
                                               void* specMapPipeMatDataPool)
{
#if !VER_x32
    if (worldSectorInitHook && worldSectorInitOriginal) {
        CHook::InlineHook(g_libGTASA + kCustomCarWorldSectorAllInOnePipelineInit64,
                          reinterpret_cast<void*>(worldSectorInitHook),
                          worldSectorInitOriginal);
    }

    if (atomicInitHook && atomicInitOriginal) {
        CHook::InlineHook(g_libGTASA + kCustomCarAtomicAllInOnePipelineInit64,
                          reinterpret_cast<void*>(atomicInitHook),
                          atomicInitOriginal);
    }
#else
    (void)worldSectorInitHook;
    (void)worldSectorInitOriginal;
    (void)atomicInitHook;
    (void)atomicInitOriginal;
#endif

    CHook::Write(GtaAddress(kCustomCarObjPipeline32, kCustomCarObjPipeline64), objPipeline);
    CHook::Write(GtaAddress(kCustomCarEnvMapPipeMatDataPool32, kCustomCarEnvMapPipeMatDataPool64),
                 envMapPipeMatDataPool);
    CHook::Write(GtaAddress(kCustomCarEnvMapPipeAtmDataPool32, kCustomCarEnvMapPipeAtmDataPool64),
                 envMapPipeAtmDataPool);
    CHook::Write(GtaAddress(kCustomCarSpecMapPipeMatDataPool32, kCustomCarSpecMapPipeMatDataPool64),
                 specMapPipeMatDataPool);
}

int32 CustomCarEnvMapPipeInstanceCBNative(void* object,
                                          void* instanceData,
                                          int32 instanceDLandVA,
                                          int32 reinstance)
{
    return CHook::CallFunction<int32>(
        "_ZN24CCustomCarEnvMapPipeline20CustomPipeInstanceCBEPvP24RxOpenGLMeshInstanceDataii",
        object,
        instanceData,
        instanceDLandVA,
        reinstance);
}

void CustomCarEnvMapPipeRenderCBNative(RwResEntry* repEntry, void* object, uint8 type, uint32 flags)
{
    CHook::CallFunction<void>(
        "_ZN24CCustomCarEnvMapPipeline18CustomPipeRenderCBEP10RwResEntryPvhj",
        repEntry,
        object,
        type,
        flags);
}

void RotateRwMatrix(RwMatrix* matrix, RwV3d* axis, float angle)
{
    if (!matrix || !axis || !std::isfinite(angle)) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kRwMatrixRotate32, kRwMatrixRotate64),
        matrix,
        axis,
        angle,
        1);
}

void InvertRwMatrix(RwMatrix* out, RwMatrix* in)
{
    if (!out || !in) {
        return;
    }
    CHook::CallFunction<void>(g_libGTASA + kRwMatrixInvertLegacy32, out, in);
}

int GetTaskTypeFromTask(const void* task)
{
    if (!task) {
        return 0;
    }

    const uintptr_t taskVtable = *reinterpret_cast<const uintptr_t*>(task);
    const uintptr_t minTaskVtable = g_libGTASA + kTaskVTableMinLegacy32;
    const uintptr_t maxTaskVtable = g_libGTASA + kTaskVTableMaxLegacy32;
    if (taskVtable < minTaskVtable || taskVtable > maxTaskVtable) {
        return 0;
    }

    auto getTaskType = *reinterpret_cast<int (**)(uintptr_t)>(taskVtable + (VER_x32 ? 0x14 : 0x28));
    return getTaskType ? getTaskType(taskVtable) : 0;
}

void SetScissorRect(void* rect)
{
    if (!rect) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kSetScissorRect32, kSetScissorRect64), rect);
}

bool IsPedPointerValid(CPedGTA* ped)
{
    if (!ped) {
        return false;
    }
    return CHook::CallFunction<bool>(GtaAddress(kIsPedPointerValid32, kIsPedPointerValid64), ped);
}

void RenderOneNonRoad(CEntityGTA* entity)
{
    if (!entity) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kRenderOneNonRoad32, kRenderOneNonRoad64), entity);
}

void EntityAdd(CEntityGTA* entity)
{
    if (!entity) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kEntityAdd32, kEntityAdd64), entity);
}

void EntityAdd(CEntityGTA* entity, const CRect* rect)
{
    if (!entity || !rect) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kEntityAddRect32, kEntityAddRect64), entity, rect);
}

void EntityRemove(CEntityGTA* entity)
{
    if (!entity) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kEntityRemove32, kEntityRemove64), entity);
}

void CreateEntityRwObject(CEntityGTA* entity)
{
    if (!entity) {
        return;
    }
    CHook::CallFunction<void>("_ZN7CEntity14CreateRwObjectEv", entity);
}

void DeleteEntityRwObjectVirtual(CEntityGTA* entity)
{
    if (!entity || !*reinterpret_cast<uintptr_t*>(entity)) {
        return;
    }

    const uintptr_t vtable = *reinterpret_cast<uintptr_t*>(entity);
    const uintptr_t fn = *reinterpret_cast<uintptr_t*>(
        vtable + (VER_x32 ? kEntityDeleteRwObjectVtableOffset32 : kEntityDeleteRwObjectVtableOffset64));
    if (fn) {
        CHook::CallFunction<void>(fn, entity);
    }
}

void EntityPreRenderVirtual(CEntityGTA* entity)
{
    if (!entity || !*reinterpret_cast<uintptr_t*>(entity)) {
        return;
    }

    const uintptr_t vtable = *reinterpret_cast<uintptr_t*>(entity);
    const uintptr_t fn = *reinterpret_cast<uintptr_t*>(
        vtable + (VER_x32 ? kEntityPreRenderVtableOffset32 : kEntityPreRenderVtableOffset64));
    if (fn) {
        CHook::CallFunction<void>(fn, entity);
    }
}

void StopExtraColour(bool fade)
{
    CHook::CallFunction<void>(GtaAddress(kStopExtraColour32, kStopExtraColour64), fade);
}

bool IsBasePlaceable(const CPhysical* physical)
{
    if (!physical) {
        return true;
    }
    return *reinterpret_cast<const uintptr_t*>(physical) ==
           GtaAddress(kPlaceableVTable32, kPlaceableVTable64);
}

void PlaceableSetPositionVirtual(CPlaceable* placeable, float x, float y, float z, bool resetMatrix)
{
    if (!placeable || !*reinterpret_cast<uintptr_t*>(placeable)) {
        return;
    }

    const uintptr_t vtable = *reinterpret_cast<uintptr_t*>(placeable);
    const uintptr_t fn = *reinterpret_cast<uintptr_t*>(
        vtable + (VER_x32 ? kPlaceableSetPositionVtableOffset32 : kPlaceableSetPositionVtableOffset64));
    if (fn) {
        CHook::CallFunction<void>(fn, placeable, x, y, z, resetMatrix);
    }
}

void PhysicalAdd(CPhysical* physical)
{
    if (!physical) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPhysicalAdd32, kPhysicalAdd64), physical);
}

void PhysicalRemove(CPhysical* physical)
{
    if (!physical) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPhysicalRemove32, kPhysicalRemove64), physical);
}

void RenderPed(CPedGTA* ped)
{
    if (!ped) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kPedRender32, kPedRender64), ped);
}

void AddVehicleUpgrade(CVehicleGTA* vehicle, int32_t modelId)
{
    if (!vehicle) {
        return;
    }
    CHook::CallFunction<void>(GtaAddress(kVehicleAddUpgrade32, kVehicleAddUpgrade64), vehicle, modelId);
}

void RemoveVehicleUpgrade(CVehicleGTA* vehicle, int32_t upgradeModelIndex)
{
    if (!vehicle) {
        return;
    }
    CHook::CallFunction<void>(
        GtaAddress(kVehicleRemoveUpgrade32, kVehicleRemoveUpgrade64),
        vehicle,
        upgradeModelIndex);
}

void BindVehicleSpecialColModel(void* specialColModel)
{
    if (!specialColModel) {
        return;
    }
    CHook::Write(GtaAddress(kVehicleSpecialColModel32, kVehicleSpecialColModel64), specialColModel);
}

void InstallPlayerPedHooksNative(uintptr_t reApplyMoveAnimsHook)
{
    CHook::Redirect("_ZN10CPlayerPed16ReApplyMoveAnimsEv", reinterpret_cast<void*>(reApplyMoveAnimsHook));
}

void InstallVehicleHooksNative(uintptr_t renderDriverAndPassengersHook,
                               uintptr_t setDriverHook,
                               uintptr_t doTailLightEffectHook,
                               uintptr_t doVehicleLightsHook,
                               void** doVehicleLightsOriginal,
                               uintptr_t getVehicleLightsStatusHook)
{
    CHook::Redirect("_ZN8CVehicle25RenderDriverAndPassengersEv",
                    reinterpret_cast<void*>(renderDriverAndPassengersHook));
    CHook::Redirect("_ZN8CVehicle9SetDriverEP4CPed", reinterpret_cast<void*>(setDriverHook));
    CHook::Redirect("_ZN8CVehicle17DoTailLightEffectEiR7CMatrixhhjh",
                    reinterpret_cast<void*>(doTailLightEffectHook));
    CHook::InlineHook("_ZN8CVehicle15DoVehicleLightsER7CMatrixj",
                      reinterpret_cast<void*>(doVehicleLightsHook),
                      doVehicleLightsOriginal);
    CHook::Redirect("_ZN8CVehicle22GetVehicleLightsStatusEv",
                    reinterpret_cast<void*>(getVehicleLightsStatusHook));
}

RwObject* CreateModelInfoRwObjectNative(CBaseModelInfo* model)
{
    if (!model || !model->vtable) {
        return nullptr;
    }

    const uintptr_t fn = *reinterpret_cast<uintptr_t*>(
        model->vtable + (VER_x32 ? kModelInfoCreateClumpVtableOffset32 : kModelInfoCreateClumpVtableOffset64));
    if (!fn) {
        return nullptr;
    }

    return CHook::CallFunction<RwObject*>(fn, model);
}

}
