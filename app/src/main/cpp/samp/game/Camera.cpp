//
// Created by x1y2z on 26.07.2023.
//

#include "Camera.h"
#include "GTASAEngineApi.h"
#include "../vendor/armhook/patch.h"
#include "scripting.h"

CCamera& CCamera::GetGameCamera() {
    return GTASAEngineApi::Camera();
}

#if !VER_x32
static bool IsServerScriptCamera64(const CVector& requested)
{
    return requested.x > 1800.0f && requested.x < 2100.0f &&
           requested.y > 1200.0f && requested.y < 1500.0f;
}

static bool IsStartupOrFallbackScriptCamera64(const CVector& requested)
{
    const bool startupCamera = requested.x > 300.0f && requested.x < 1200.0f &&
                               requested.y > -2100.0f && requested.y < -1400.0f;
    const bool localFallback = requested.x > 40.0f && requested.x < 70.0f &&
                               requested.y > 40.0f && requested.y < 70.0f;

    return startupCamera || localFallback;
}

static void LogScriptCameraState64(const char* tag, const CVector& requested, int type)
{
    static uint32_t sServerCameraStateLogCount = 0;
    static uint32_t sOtherCameraStateLogCount = 0;
    const bool serverCamera = IsServerScriptCamera64(requested);
    if (serverCamera) {
        if (sServerCameraStateLogCount >= 12) {
            return;
        }
        ++sServerCameraStateLogCount;
    } else {
        if (!IsStartupOrFallbackScriptCamera64(requested) || sOtherCameraStateLogCount >= 24) {
            return;
        }
        ++sOtherCameraStateLogCount;
    }

    CCamera& camera = CCamera::GetGameCamera();
    if (camera.m_nActiveCam >= 3) {
        FLog("[CAM_STATE64] %s req=%.2f %.2f %.2f type=%d invalidActive=%u",
             tag,
             requested.x, requested.y, requested.z,
             type,
             static_cast<unsigned>(camera.m_nActiveCam));
        return;
    }
    CCam& activeCam = camera.GetActiveCamera();

    FLog("[CAM_STATE64] %s req=%.2f %.2f %.2f type=%d active=%u mode=%u goto=%u ctrl=%d lookPlayer=%d lookVec=%d persist=%d/%d target=%p attached=%p camTarget=%p src=%.2f %.2f %.2f front=%.3f %.3f %.3f fixedSrc=%.2f %.2f %.2f fixedVec=%.2f %.2f %.2f",
         tag,
         requested.x, requested.y, requested.z,
         type,
         static_cast<unsigned>(camera.m_nActiveCam),
         static_cast<unsigned>(activeCam.m_nMode),
         static_cast<unsigned>(camera.m_nModeToGoTo),
         camera.WhoIsInControlOfTheCamera,
         camera.m_bLookingAtPlayer ? 1 : 0,
         camera.m_bLookingAtVector ? 1 : 0,
         camera.m_bPersistCamPos ? 1 : 0,
         camera.m_bPersistCamLookAt ? 1 : 0,
         reinterpret_cast<void*>(camera.pTargetEntity),
         reinterpret_cast<void*>(camera.pAttachedEntity),
         reinterpret_cast<void*>(activeCam.CamTargetEntity),
         activeCam.Source.x, activeCam.Source.y, activeCam.Source.z,
         activeCam.Front.x, activeCam.Front.y, activeCam.Front.z,
         camera.m_vecFixedModeSource.x, camera.m_vecFixedModeSource.y, camera.m_vecFixedModeSource.z,
         camera.m_vecFixedModeVector.x, camera.m_vecFixedModeVector.y, camera.m_vecFixedModeVector.z);
}
#endif

void CCamera::InjectHooks() {

}

CCam& CCamera::GetActiveCamera() {
    return GTASAEngineApi::ActiveCamera();
}

void CCamera::Init() {
    GTASAEngineApi::InitCamera(this);
}

void CCamera::SetRwCamera(RwCamera *pCamera) {
    GTASAEngineApi::SetRwCamera(this, pCamera);
}

void CCamera::TakeControl(CEntityGTA *target, eCamMode modeToGoTo, eSwitchType switchType, int32 whoIsInControlOfTheCamera) {
    GTASAEngineApi::TakeCameraControl(this, target, modeToGoTo, switchType, whoIsInControlOfTheCamera);
}

float CCamera::CalculateGroundHeight(eGroundHeightType type) {
    return GTASAEngineApi::CalculateCameraGroundHeight(this, type);
}

void CCamera::RestoreWithJumpCut() {
    GTASAEngineApi::RestoreCameraWithJumpCut(this);
}

void CCamera::Restore()
{
    ScriptCommand(&lock_camera_position, 0);
    ScriptCommand(&restore_camera_to_user);
    ScriptCommand(&restore_camera_jumpcut);
    m_bCameraJustRestored = true;
    m_bLookingAtVector = false;
    m_bUseNearClipScript = false;
    WhoIsInControlOfTheCamera = 0;
}

void CCamera::SetBehindPlayer()
{
    ScriptCommand(&lock_camera_position, 0);
    ScriptCommand(&restore_camera_to_user);
    ScriptCommand(&set_camera_behind_player);
    ScriptCommand(&restore_camera_jumpcut);
}

// 0.3.7
void CCamera::SetPosition(float fX, float fY, float fZ, float fRotationX, float fRotationY, float fRotationZ)
{
    ScriptCommand(&restore_camera_to_user);
    ScriptCommand(&set_camera_position, fX, fY, fZ, fRotationX, fRotationY, fRotationZ);

#if !VER_x32
    LogScriptCameraState64("after-set-pos", CVector(fX, fY, fZ), -1);
#endif
}


// 0.3.7
void CCamera::LookAtPoint(float fX, float fY, float fZ, int iType)
{
    ScriptCommand(&restore_camera_to_user);
    ScriptCommand(&point_camera, fX, fY, fZ, iType);

#if !VER_x32
    LogScriptCameraState64("after-look-at", CVector(fX, fY, fZ), iType);
#endif
}

// 0.3.7
void CCamera::InterpolateCameraPos(CVector *posFrom, CVector *posTo, int time, uint8_t mode)
{
    CCamera& TheCamera = CCamera::GetGameCamera();

    ScriptCommand(&restore_camera_to_user);
    ScriptCommand(&lock_camera_position1, 1);
    ScriptCommand(&set_camera_pos_time_smooth, posFrom->x, posFrom->y, posFrom->z, posTo->x, posTo->y, posTo->z, time, mode);
}

// 0.3.7
void CCamera::InterpolateCameraLookAt(CVector *posFrom, CVector *posTo, int time, uint8_t mode)
{
    ScriptCommand(&lock_camera_position, 1);
    ScriptCommand(&point_camera_transverse, posFrom->x, posFrom->y, posFrom->z, posTo->x, posTo->y, posTo->z, time, mode);
}
