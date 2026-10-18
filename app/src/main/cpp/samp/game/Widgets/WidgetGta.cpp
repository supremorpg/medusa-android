//
// Created by x1y2z on 03.02.2023.
//

#include "WidgetGta.h"
#include "main.h"
#include "game/game.h"
#include "game/GTASAEngineApi.h"
#include "game/GTASAEnginePedInputBindings.h"
#include "net/netgame.h"
#include "vendor/armhook/patch.h"
#include "WidgetRegionLook.h"
#include "TouchInterface.h"
#include "WidgetButton.h"
#include "src/ui/SampUiController.h"
#include "util/CUtil.h"

extern CNetGame *pNetGame;
extern CGame *pGame;

CWidgetGta* m_pWidgets[WidgetIDs::NUM_WIDGETS];

enum eWidgetState {
    STATE_NONE,
    STATE_FIXED
};

WidgetIDs GetWidgetTypeFromWidget(CWidgetGta* pWidget)
{
    if(!pWidget) return static_cast<WidgetIDs>(-1);

    for(int i = 0; i < WidgetIDs::NUM_WIDGETS; ++i)
    {
        if(m_pWidgets[i] && pWidget == m_pWidgets[i]) return static_cast<WidgetIDs>(i);
        if(CTouchInterface::m_pWidgets &&
           CTouchInterface::m_pWidgets[i] &&
           pWidget == CTouchInterface::m_pWidgets[i])
        {
            return static_cast<WidgetIDs>(i);
        }
    }

    return static_cast<WidgetIDs>(-1);
}

void SetWidgetFromId(int idWidget, CWidgetGta* pWidget)
{
    if(idWidget < 0 || idWidget >= WidgetIDs::NUM_WIDGETS) return;
    m_pWidgets[idWidget] = pWidget;
}

void SetWidgetFromName(const char* name, CWidgetGta* pWidget)
{
    if(!name || !pWidget) return;

    if(!strcmp("accelerate", name)) SetWidgetFromId(WidgetIDs::WIDGET_ACCELERATE, pWidget);
    if(!strcmp("hud_car", name) || !strcmp("enter_car", name) || !strcmp("exit_car", name) || !strcmp("button_enter_car", name) || !strcmp("enter_exit", name)) SetWidgetFromId(WidgetIDs::WIDGET_ENTER_CAR, pWidget);
    if(!strcmp("brake", name)) SetWidgetFromId(WidgetIDs::WIDGET_BRAKE, pWidget);
    if(!strcmp("attack", name) || !strcmp("targeting", name) || !strcmp("button_attack", name) || !strcmp("punch", name)) SetWidgetFromId(WidgetIDs::WIDGET_ATTACK, pWidget);
    if(!strcmp("car_shoot", name) || !strcmp("vehicle_fire", name) || !strcmp("shoot", name) || !strcmp("fire", name) || !strcmp("button_fire", name) || !strcmp("tank_fire", name) || !strcmp("tank_shoot", name)) SetWidgetFromId(WidgetIDs::WIDGET_CAR_SHOOT, pWidget);
    if(!strcmp("vehicle_shoot_left", name) || !strcmp("leftshoot", name) || !strcmp("vehicle_fire_left", name) || !strcmp("tank_fire_left", name) || !strcmp("turret_left", name)) SetWidgetFromId(WidgetIDs::WIDGET_VEHICLE_SHOOT_LEFT, pWidget);
    if(!strcmp("vehicle_shoot_right", name) || !strcmp("rightshoot", name) || !strcmp("vehicle_fire_right", name) || !strcmp("tank_fire_right", name) || !strcmp("turret_right", name)) SetWidgetFromId(WidgetIDs::WIDGET_VEHICLE_SHOOT_RIGHT, pWidget);
    if(!strcmp("vc_shoot", name)) SetWidgetFromId(WidgetIDs::WIDGET_VC_SHOOT, pWidget);
    if(!strcmp("vc_shoot_alt", name)) SetWidgetFromId(WidgetIDs::WIDGET_VC_SHOOT_ALT, pWidget);
    if(!strcmp("air_gun", name)) SetWidgetFromId(WidgetIDs::WIDGET_AIR_GUN, pWidget);
    if(!strcmp("rocket", name) || !strcmp("hud_rockets", name)) SetWidgetFromId(WidgetIDs::WIDGET_ROCKET, pWidget);
    if(!strcmp("vehicle_bomb", name) || !strcmp("hud_detonator", name)) SetWidgetFromId(WidgetIDs::WIDGET_VEHICLE_BOMB, pWidget);
    if(!strcmp("vehicle_turret_left", name) || !strcmp("hud_tank_left", name)) SetWidgetFromId(WidgetIDs::WIDGET_VEHICLE_TURRET_LEFT, pWidget);
    if(!strcmp("vehicle_turret_right", name) || !strcmp("hud_tank_right", name)) SetWidgetFromId(WidgetIDs::WIDGET_VEHICLE_TURRET_RIGHT, pWidget);
    if(!strcmp("handbrake", name) || !strcmp("air_shoot", name)) SetWidgetFromId(WidgetIDs::WIDGET_HANDBRAKE, pWidget);
    if(!strcmp("horn", name)) SetWidgetFromId(WidgetIDs::WIDGET_HORN, pWidget);
    if(!strcmp("sprint", name) || !strcmp("run", name) || !strcmp("button_run", name)) SetWidgetFromId(WidgetIDs::WIDGET_SPRINT, pWidget);
    if(!strcmp("button_sprint", name) || !strcmp("sprint_swim", name) || !strcmp("button_jump", name) || !strcmp("jump", name)) SetWidgetFromId(WidgetIDs::WIDGET_BUTTON_SPRINT, pWidget);
    if(!strcmp("button_swim", name)) SetWidgetFromId(WidgetIDs::WIDGET_BUTTON_SWIM, pWidget);
    if(!strcmp("button_crouch", name)) SetWidgetFromId(WidgetIDs::WIDGET_BUTTON_CROUCH, pWidget);
    if(!strcmp("button_dive", name) || !strcmp("hud_dive", name)) SetWidgetFromId(WidgetIDs::WIDGET_BUTTON_DIVE, pWidget);
    if(!strcmp("nitro", name) || !strcmp("hud_nitro", name)) SetWidgetFromId(WidgetIDs::WIDGET_NITRO, pWidget);
    if(!strcmp("hydraulics", name)) SetWidgetFromId(WidgetIDs::WIDGET_HYDRAULICS, pWidget);
    if(!strcmp("auto_hydraulics", name)) SetWidgetFromId(WidgetIDs::WIDGET_AUTO_HYDRAULICS, pWidget);
    if(!strcmp("drive", name)) SetWidgetFromId(WidgetIDs::WIDGET_DRIVE_HYBRID, pWidget);
    if(!strcmp("shoot_look", name)) SetWidgetFromId(WidgetIDs::WIDGET_SHOOT_LOOK, pWidget);
    if(!strcmp("cam_toggle", name) || !strcmp("cam-toggle", name) || !strcmp("camera", name) || !strcmp("camera_toggle", name) || !strcmp("button_camera", name)) SetWidgetFromId(WidgetIDs::WIDGET_CAM_TOGGLE, pWidget);
    if(!strcmp("vehicle_steer_left", name)) SetWidgetFromId(WidgetIDs::WIDGET_VEHICLE_STEER_LEFT, pWidget);
    if(!strcmp("vehicle_steer_right", name)) SetWidgetFromId(WidgetIDs::WIDGET_VEHICLE_STEER_RIGHT, pWidget);
    if(!strcmp("vehicle_steer_analog", name)) SetWidgetFromId(WidgetIDs::WIDGET_VEHICLE_STEER_ANALOG, pWidget);
    if(!strcmp("ped_move", name)) SetWidgetFromId(WidgetIDs::WIDGET_PED_MOVE, pWidget);
    if(!strcmp("hud_lockon", name)) SetWidgetFromId(WidgetIDs::WIDGET_ENTER_TARGETING, pWidget);
    if(!strcmp("hud_swapgun", name)) SetWidgetFromId(WidgetIDs::WIDGET_SWAP_WEAPONS, pWidget);
    if(!strcmp("hud_circle", name)) SetWidgetFromId(WidgetIDs::WIDGET_THUMB_CIRCLE, pWidget);
    if(!strcmp("look", name) || !strcmp("camera_look", name) || !strcmp("look_region", name)) SetWidgetFromId(WidgetIDs::WIDGET_LOOK, pWidget);
    if(!strcmp("hud_parachute", name)) SetWidgetFromId(WidgetIDs::WIDGET_PARACHUTE, pWidget);
}

namespace {
constexpr size_t kMaxSuppressedSourceHudNativeWidgets = 256;
CWidgetGta* s_suppressedSourceHudNativeWidgets[kMaxSuppressedSourceHudNativeWidgets]{};

bool IsSourceHudNativeWidgetName(const char* name)
{
    if(!name || !name[0]) return false;

    return !strcmp("accelerate", name) ||
           !strcmp("brake", name) ||
           !strcmp("handbrake", name) ||
           !strcmp("hud_car", name) ||
           !strcmp("enter_car", name) ||
           !strcmp("exit_car", name) ||
           !strcmp("button_enter_car", name) ||
           !strcmp("enter_exit", name) ||
           !strcmp("attack", name) ||
           !strcmp("targeting", name) ||
           !strcmp("button_attack", name) ||
           !strcmp("punch", name) ||
           !strcmp("shoot", name) ||
           !strcmp("fire", name) ||
           !strcmp("button_fire", name) ||
           !strcmp("car_shoot", name) ||
           !strcmp("vehicle_fire", name) ||
           !strcmp("tank_fire", name) ||
           !strcmp("tank_shoot", name) ||
           !strcmp("leftshoot", name) ||
           !strcmp("rightshoot", name) ||
           !strcmp("vehicle_shoot_left", name) ||
           !strcmp("vehicle_shoot_right", name) ||
           !strcmp("vehicle_fire_left", name) ||
           !strcmp("vehicle_fire_right", name) ||
           !strcmp("tank_fire_left", name) ||
           !strcmp("tank_fire_right", name) ||
           !strcmp("turret_left", name) ||
           !strcmp("turret_right", name) ||
           !strcmp("vc_shoot", name) ||
           !strcmp("vc_shoot_alt", name) ||
           !strcmp("air_gun", name) ||
           !strcmp("rocket", name) ||
           !strcmp("hud_rockets", name) ||
           !strcmp("vehicle_bomb", name) ||
           !strcmp("hud_detonator", name) ||
           !strcmp("vehicle_turret_left", name) ||
           !strcmp("vehicle_turret_right", name) ||
           !strcmp("hud_tank_left", name) ||
           !strcmp("hud_tank_right", name) ||
           !strcmp("horn", name) ||
           !strcmp("sprint", name) ||
           !strcmp("run", name) ||
           !strcmp("button_run", name) ||
           !strcmp("button_sprint", name) ||
           !strcmp("sprint_swim", name) ||
           !strcmp("button_jump", name) ||
           !strcmp("jump", name) ||
           !strcmp("button_swim", name) ||
           !strcmp("button_crouch", name) ||
           !strcmp("button_dive", name) ||
           !strcmp("hud_dive", name) ||
           !strcmp("nitro", name) ||
           !strcmp("hud_nitro", name) ||
           !strcmp("hydraulics", name) ||
           !strcmp("auto_hydraulics", name) ||
           !strcmp("drive", name) ||
           !strcmp("cam_toggle", name) ||
           !strcmp("cam-toggle", name) ||
           !strcmp("camera", name) ||
           !strcmp("camera_toggle", name) ||
           !strcmp("button_camera", name) ||
           !strcmp("vehicle_steer_left", name) ||
           !strcmp("vehicle_steer_right", name) ||
           !strcmp("vehicle_steer_analog", name) ||
           !strcmp("hud_lockon", name) ||
           !strcmp("hud_swapgun", name) ||
           !strcmp("hud_circle", name);
}

void RememberSuppressedSourceHudNativeWidget(CWidgetGta* widget)
{
    if(!widget) return;

    for(CWidgetGta* stored : s_suppressedSourceHudNativeWidgets)
    {
        if(stored == widget) return;
    }

    for(CWidgetGta*& stored : s_suppressedSourceHudNativeWidgets)
    {
        if(!stored)
        {
            stored = widget;
            return;
        }
    }
}

bool IsRememberedSuppressedSourceHudNativeWidget(CWidgetGta* widget)
{
    if(!widget) return false;

    for(CWidgetGta* stored : s_suppressedSourceHudNativeWidgets)
    {
        if(stored == widget) return true;
    }

    return false;
}

bool IsSourceHudNativeWidgetId(WidgetIDs widgetType)
{
    switch(widgetType)
    {
        case WidgetIDs::WIDGET_ENTER_CAR:
        case WidgetIDs::WIDGET_ATTACK:
        case WidgetIDs::WIDGET_ENTER_TARGETING:
        case WidgetIDs::WIDGET_EXIT_TARGETING:
        case WidgetIDs::WIDGET_CAR_SHOOT:
        case WidgetIDs::WIDGET_HYDRAULICS:
        case WidgetIDs::WIDGET_AUTO_HYDRAULICS:
        case WidgetIDs::WIDGET_VEHICLE_SHOOT_LEFT:
        case WidgetIDs::WIDGET_VEHICLE_SHOOT_RIGHT:
        case WidgetIDs::WIDGET_VC_SHOOT:
        case WidgetIDs::WIDGET_VC_SHOOT_ALT:
        case WidgetIDs::WIDGET_AIR_GUN:
        case WidgetIDs::WIDGET_ROCKET:
        case WidgetIDs::WIDGET_VEHICLE_BOMB:
        case WidgetIDs::WIDGET_NITRO:
        case WidgetIDs::WIDGET_VEHICLE_TURRET_LEFT:
        case WidgetIDs::WIDGET_VEHICLE_TURRET_RIGHT:
        case WidgetIDs::WIDGET_ACCELERATE:
        case WidgetIDs::WIDGET_BRAKE:
        case WidgetIDs::WIDGET_HANDBRAKE:
        case WidgetIDs::WIDGET_HORN:
        case WidgetIDs::WIDGET_CAM_TOGGLE:
        case WidgetIDs::WIDGET_SWAP_WEAPONS:
        case WidgetIDs::WIDGET_BUTTON_SPRINT:
        case WidgetIDs::WIDGET_BUTTON_CROUCH:
        case WidgetIDs::WIDGET_BUTTON_DIVE:
        case WidgetIDs::WIDGET_BUTTON_SWIM:
        case WidgetIDs::WIDGET_SPRINT:
        case WidgetIDs::WIDGET_BASKETBALL_JUMP:
        case WidgetIDs::WIDGET_THUMB_CIRCLE:
        case WidgetIDs::WIDGET_DRIVE_HYBRID:
        case WidgetIDs::WIDGET_VEHICLE_STEER_LEFT:
        case WidgetIDs::WIDGET_VEHICLE_STEER_RIGHT:
        case WidgetIDs::WIDGET_VEHICLE_STEER_ANALOG:
        case WidgetIDs::WIDGET_VEHICLE_LANECORRECTION:
        case WidgetIDs::WIDGET_VEHICLE_FLICK:
            return true;
        default:
            return false;
    }
}

bool IsSourceHudNativeHid(HIDMapping mapping)
{
    switch(mapping)
    {
        case HID_MAPPING_ATTACK:
        case HID_MAPPING_SPRINT:
        case HID_MAPPING_JUMP:
        case HID_MAPPING_CROUCH:
        case HID_MAPPING_ENTER_CAR:
        case HID_MAPPING_BRAKE:
        case HID_MAPPING_HANDBRAKE:
        case HID_MAPPING_ACCELERATE:
        case HID_MAPPING_CAMERA_CLOSER:
        case HID_MAPPING_CAMERA_FARTHER:
        case HID_MAPPING_PED_LOOK_BACK:
        case HID_MAPPING_VEHICLE_LOOK_LEFT:
        case HID_MAPPING_VEHICLE_LOOK_RIGHT:
        case HID_MAPPING_VEHICLE_LOOK_BACK:
        case HID_MAPPING_HORN:
        case HID_MAPPING_NITRO:
        case HID_MAPPING_AUTO_HYDRAULICS:
        case HID_MAPPING_VEHICLE_BOMB:
        case HID_MAPPING_TURRET_LEFT:
        case HID_MAPPING_TURRET_RIGHT:
        case HID_MAPPING_SWAP_WEAPONS_AND_PURCHASE:
        case HID_MAPPING_WEAPON_ZOOM_IN:
        case HID_MAPPING_WEAPON_ZOOM_OUT:
        case HID_MAPPING_ENTER_AND_EXIT_TARGETING:
        case HID_MAPPING_FLIGHT_PRIMARY_ATTACK:
        case HID_MAPPING_FLIGHT_SECONDARY_ATTACK:
        case HID_MAPPING_FLIGHT_ASCEND:
        case HID_MAPPING_FLIGHT_DESCEND:
        case HID_MAPPING_FLIGHT_ALT_LEFT:
        case HID_MAPPING_FLIGHT_ALT_RIGHT:
        case HID_MAPPING_FLIGHT_ALT_UP:
        case HID_MAPPING_FLIGHT_ALT_DOWN:
        case HID_MAPPING_ALT_ATTACK:
        case HID_MAPPING_BLOCK:
        case HID_MAPPING_TAKE_COVER_LEFT:
        case HID_MAPPING_TAKE_COVER_RIGHT:
        case HID_MAPPING_TOGGLE_LANDING_GEAR:
        case HID_MAPPING_BASKETBALL_SHOOT:
        case HID_MAPPING_BUNNY_HOP:
        case HID_MAPPING_VEHICLE_STEER_X:
        case HID_MAPPING_VEHICLE_STEER_Y:
        case HID_MAPPING_VEHICLE_STEER_LEFT:
        case HID_MAPPING_VEHICLE_STEER_RIGHT:
            return true;
        default:
            return false;
    }
}

bool ShouldSuppressSourceHudNativeWidget(CWidgetGta* pWidget)
{
    if(!pNetGame || !pWidget) return false;

    if(IsRememberedSuppressedSourceHudNativeWidget(pWidget)) return true;

    WidgetIDs widgetType = GetWidgetTypeFromWidget(pWidget);
    if(IsSourceHudNativeWidgetId(widgetType)) return true;

    return IsSourceHudNativeHid(pWidget->m_HIDMapping);
}

void ResetSuppressedWidgetTouch(CWidgetGta* pWidget, CVector2D* pVecOut)
{
    if(pVecOut)
    {
        pVecOut->x = 0.0f;
        pVecOut->y = 0.0f;
    }

    if(!pWidget) return;

    pWidget->m_fTapHoldTime = 0.0f;
    pWidget->m_bTaphold = false;
    pWidget->m_nTouchIndex = -1;
}

void DisableSuppressedNativeWidget(CWidgetGta* pWidget)
{
    if(!pWidget) return;

    pWidget->m_bEnabled = false;
    pWidget->m_bCachedEnabled = false;
    pWidget->m_Color.a = 0;
    ResetSuppressedWidgetTouch(pWidget, nullptr);
}
}

eWidgetState ProcessFixedWidget(CWidgetGta* pWidget)
{
    WidgetIDs widgetType = GetWidgetTypeFromWidget(pWidget);

    CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
    switch(widgetType)
    {
        case -1:
            return STATE_NONE;
        case WidgetIDs::WIDGET_ATTACK:
        case WidgetIDs::WIDGET_SPRINT:
            if(pPlayerPed->IsInVehicle() ||
               pPlayerPed->IsInJetpackMode())
            {
                return STATE_FIXED;
            }
            break;
        case WidgetIDs::WIDGET_ACCELERATE:
        case WidgetIDs::WIDGET_BRAKE:
            if(!pPlayerPed->IsInVehicle() &&
               !pPlayerPed->IsInJetpackMode())
            {
                return STATE_FIXED;
            }
            break;
        case WidgetIDs::WIDGET_ENTER_CAR:
            if(pPlayerPed->IsInJetpackMode()) return STATE_NONE;

            if(pNetGame)
            {
                CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
                if(pVehiclePool)
                {
                    VEHICLEID vehicleId = pVehiclePool->FindNearestToLocalPlayerPed();
                    if(vehicleId == INVALID_VEHICLE_ID) return STATE_FIXED;

                    if(vehicleId != INVALID_VEHICLE_ID)
                    {
                        CVehicle *pVehicle = pVehiclePool->GetAt(vehicleId);
                        if(pVehicle)
                        {
                            if(!pPlayerPed->IsInVehicle() &&
                               pVehicle->m_pVehicle->GetDistanceFromLocalPlayerPed() > 10.0f)
                            {
                                return STATE_FIXED;
                            }
                        }
                    }
                }
            }
            break;
    }

    return STATE_NONE;
}

void CWidgetGta::SetEnabled(bool bEnabled) {
    m_bEnabled = bEnabled;
}

bool (*CWidget__IsTouched)(uintptr_t *thiz, CVector2D *pVecOut);
bool CWidget__IsTouched_hook(uintptr_t *thiz, CVector2D *pVecOut) {
//    if(*thiz == CWidgetGta::pWidgets[WIDGET_POSITION_HORN]) {
//        return true;
//    }
//    if(!CHUD::bIsShow)
//        return false;

    CWidgetGta* widget = reinterpret_cast<CWidgetGta*>(thiz);
    if(ShouldSuppressSourceHudNativeWidget(widget))
    {
        DisableSuppressedNativeWidget(widget);
        ResetSuppressedWidgetTouch(widget, pVecOut);
        return false;
    }

    if (SampUiController::ShouldSuppressNativeRadar() && widget &&
        (widget->m_HIDMapping == HID_MAPPING_RADAR ||
         (CTouchInterface::m_pWidgets && CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_RADAR] == widget))) {
        if (pVecOut) {
            pVecOut->x = 0.0f;
            pVecOut->y = 0.0f;
        }
        widget->m_fTapHoldTime = 0.0f;
        widget->m_bTaphold = false;
        widget->m_nTouchIndex = -1;
        return false;
    }

    return CWidget__IsTouched(thiz, pVecOut);
}

bool (*CWidget__IsReleased)(uintptr_t *thiz, CVector2D *pVecOut);
bool CWidget__IsReleased_hook(uintptr_t *thiz, CVector2D *pVecOut) {
    CWidgetGta* widget = reinterpret_cast<CWidgetGta*>(thiz);
    if(ShouldSuppressSourceHudNativeWidget(widget))
    {
        DisableSuppressedNativeWidget(widget);
        ResetSuppressedWidgetTouch(widget, pVecOut);
        return false;
    }

    if (SampUiController::ShouldSuppressNativeRadar() && widget &&
        (widget->m_HIDMapping == HID_MAPPING_RADAR ||
         (CTouchInterface::m_pWidgets && CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_RADAR] == widget))) {
        if (pVecOut) {
            pVecOut->x = 0.0f;
            pVecOut->y = 0.0f;
        }
        widget->m_fTapHoldTime = 0.0f;
        widget->m_bTaphold = false;
        widget->m_nTouchIndex = -1;
        return false;
    }

    return CWidget__IsReleased(thiz, pVecOut);
}

uintptr_t (*CWidget)(CWidgetButton* thiz, const char* name, uintptr_t* a3, int a4, uintptr_t* a5);
uintptr_t CWidget_hook(CWidgetButton* thiz, const char* name, uintptr_t*  a3, int a4, uintptr_t* a5)
{
    FLog("New Widget: \"%s\" 0x%zX", name, static_cast<size_t>(GTASAEngineApi::RebaseFromGta(reinterpret_cast<uintptr_t>(thiz))));

    SetWidgetFromName(name, thiz);
    if(IsSourceHudNativeWidgetName(name))
    {
        RememberSuppressedSourceHudNativeWidget(thiz);
    }
    return CWidget(thiz, name, a3, a4, a5);
}

void (*CWidget__SetEnabled)(CWidgetGta* pWidget, bool bEnabled);
void CWidget__SetEnabled_hook(CWidgetGta* pWidget, bool bEnabled)
{
    if(ShouldSuppressSourceHudNativeWidget(pWidget))
    {
        DisableSuppressedNativeWidget(pWidget);
        bEnabled = false;
    }

    if(pNetGame)
    {
        switch(ProcessFixedWidget(pWidget))
        {
            case STATE_NONE: break;
            case STATE_FIXED:
                bEnabled = false;
                break;
        }
    }

    CWidget__SetEnabled(pWidget, bEnabled);
}

void (*CWidgetButton__Update)(CWidgetButton* thiz);
void CWidgetButton__Update_hook(CWidgetButton* thiz) {
    if(ShouldSuppressSourceHudNativeWidget(thiz))
    {
        DisableSuppressedNativeWidget(thiz);
        return;
    }

    if(pNetGame)
    {
        switch(ProcessFixedWidget(thiz))
        {
            case STATE_NONE: break;
            case STATE_FIXED: return;
        }
    }

    CWidgetButton__Update(thiz);
}

void CWidgetGta::InjectHooks() {
    GTASAEngineApi::InstallWidgetButtonUpdateHook(&CWidgetButton__Update_hook, &CWidgetButton__Update);
    CHook::InlineHook("_ZN13CWidgetButtonC2EPKcRK14WidgetPositionjj10HIDMapping", &CWidget_hook, &CWidget);
    CHook::InlineHook("_ZN7CWidget10SetEnabledEb", &CWidget__SetEnabled_hook, &CWidget__SetEnabled);
    GTASAEngineApi::InstallWidgetTouchHooks(&CWidget__IsTouched_hook, &CWidget__IsTouched,
                                            &CWidget__IsReleased_hook, &CWidget__IsReleased);
}

void CWidgetGta::SetTexture(const char *name) {
    m_Sprite.m_pTexture = CUtil::LoadTextureFromDB("mobile", name);
}

bool CWidgetGta::IsReleased(CVector2D *pVecOut) {
    return GTASAEngineApi::WidgetIsReleased(this, pVecOut);
}

bool CWidgetGta::IsTouched(CVector2D *pVecOut) {
    return GTASAEngineApi::WidgetIsTouched(this, pVecOut);
}
