#pragma once

#include <cstdint>

class CPedIntelligence;
class CVector2D;
class CWidgetGta;
struct CWidgetButton;

namespace GTASAEngineApi {

using PedProcessControlFn = void (*)(uintptr_t);
using VehicleProcessControlFn = void (*)(uintptr_t);
using TaskUseGunFn = uint32_t (*)(uintptr_t, uintptr_t);
using PadTaskProcessFn = uint32_t (*)(uintptr_t, uintptr_t, int, int);
using WidgetButtonUpdateFn = void (*)(CWidgetButton*);
using WidgetTouchFn = bool (*)(uintptr_t*, CVector2D*);

void FlushPedIntelligenceNative(CPedIntelligence* intelligence);
void FlushPedIntelligenceImmediatelyNative(CPedIntelligence* intelligence, bool setPrimaryDefaultTask);
void ProcessPedIntelligenceAfterPreRenderNative(CPedIntelligence* intelligence);
void PatchRemotePedUpdatePosition(bool disable);
uint32_t CallTaskUseGun(uintptr_t task, uintptr_t ped);
uint32_t CallPadTaskProcess(uintptr_t task, uintptr_t ped, int arg0, int arg1);
void InstallPedProcessControlHook(PedProcessControlFn hook, PedProcessControlFn* original);
void InstallVehicleProcessControlHooks(VehicleProcessControlFn hook);
void InstallTaskUseGunHook(TaskUseGunFn hook);
void InstallPadTaskProcessHook(PadTaskProcessFn hook);
void InstallWidgetButtonUpdateHook(WidgetButtonUpdateFn hook, WidgetButtonUpdateFn* original);
void InstallWidgetTouchHooks(WidgetTouchFn touchedHook,
                             WidgetTouchFn* touchedOriginal,
                             WidgetTouchFn releasedHook,
                             WidgetTouchFn* releasedOriginal);
bool WidgetIsReleased(CWidgetGta* widget, CVector2D* out);

}
