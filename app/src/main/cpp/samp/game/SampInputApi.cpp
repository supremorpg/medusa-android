#include "SampInputApi.h"

#include "../main.h"
#include "GTASAEngineApi.h"
#include "Camera.h"
#include "SampAppCommandApi.h"
#include "SampCollisionApi.h"
#include "SampIplApi.h"
#include "SampStreamingApi.h"
#include "World.h"
#include "Collision/ColPoint.h"
#include "RW/rwcore.h"
#include "playerped.h"
#include "pad.h"
#include "../net/netgame.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <cmath>

extern CNetGame* pNetGame;
extern bool bNeedEnterVehicle;

namespace Xyron::Input {
namespace {

static SourceHudState g_sourceHudState{};
static bool g_sourceHudJumpJustPressed = false;

static constexpr int SOURCE_ACTION_ATTACK = 2;
static constexpr int SOURCE_ACTION_SECONDARY = 3;
static constexpr int SOURCE_ACTION_ACCELERATE = 4;
static constexpr int SOURCE_ACTION_BRAKE = 5;
static constexpr int SOURCE_ACTION_HANDBRAKE = 6;
static constexpr int SOURCE_ACTION_SPRINT = 7;
static constexpr int SOURCE_ACTION_JUMP = 8;
static constexpr int SOURCE_ACTION_HORN = 9;
static constexpr float SOURCE_CAMERA_MIN_PITCH = -0.75f;
static constexpr float SOURCE_CAMERA_MAX_PITCH = 0.55f;
static constexpr float SOURCE_CAMERA_YAW_SENSITIVITY = 0.0034f;
static constexpr float SOURCE_CAMERA_PITCH_SENSITIVITY = 0.0028f;
static constexpr float SOURCE_CAMERA_INPUT_BLEND = 0.66f;
static constexpr float SOURCE_CAMERA_TARGET_BLEND = 0.58f;
static constexpr float SOURCE_CAMERA_SOURCE_BLEND = 0.72f;
static constexpr float SOURCE_CAMERA_MIN_DISTANCE = 2.15f;
static constexpr float SOURCE_CAMERA_MAX_DISTANCE = 8.0f;
static constexpr float SOURCE_CAMERA_DEFAULT_DISTANCE = 4.5f;
static constexpr float SOURCE_CAMERA_COLLISION_MIN_DISTANCE = 0.90f;
static constexpr float SOURCE_CAMERA_MIN_SOURCE_BELOW_TARGET = 12.0f;
static constexpr uint32_t SOURCE_CAMERA_STREAM_COOLDOWN_MS = 80;
static constexpr uint32_t SOURCE_CAMERA_STREAM_REFRESH_MS = 220;
static constexpr float SOURCE_CAMERA_PREFETCH_NEAR = 35.0f;
static constexpr float SOURCE_CAMERA_PREFETCH_MID = 75.0f;
static constexpr float SOURCE_CAMERA_PREFETCH_FAR = 135.0f;
static constexpr float SOURCE_CAMERA_PREFETCH_WIDE = 42.0f;
static constexpr float SOURCE_CAMERA_PREFETCH_VERTICAL = 26.0f;
static constexpr uint32_t PASSENGER_VEHICLE_ACTION_COOLDOWN_MS = 1000;

static std::atomic<int32_t> g_sourceCameraDeltaX1000{0};
static std::atomic<int32_t> g_sourceCameraDeltaY1000{0};
static std::atomic<int32_t> g_sourceCameraScreenWidth{0};
static std::atomic<int32_t> g_sourceCameraScreenHeight{0};
static std::atomic<bool> g_sourceCameraLookRequested{false};
static bool g_sourceCameraLookActive = false;
static bool g_sourceCameraLookSeeded = false;
static float g_sourceCameraYaw = 0.0f;
static float g_sourceCameraPitch = 0.10f;
static float g_sourceCameraFilteredDeltaX = 0.0f;
static float g_sourceCameraFilteredDeltaY = 0.0f;
static bool g_sourceCameraSmoothedFrameValid = false;
static CVector g_sourceCameraSmoothedTarget{};
static CVector g_sourceCameraSmoothedSource{};
// Velocity vectors for the critically-damped spring smoother
static CVector g_sourceCameraSourceVel{};
static CVector g_sourceCameraTargetVel{};
static uint32_t g_sourceCameraLastTick = 0;
static uint32_t g_sourceCameraLogCount = 0;
static uint32_t g_sourceCameraStreamingLogCount = 0;
static uint32_t g_lastPassengerVehicleActionTick = 0;
static bool g_driverEnterVehicleTaskWasActive = false;
static bool g_exitVehicleTaskWasActive = false;

// -----------------------------------------------------------------------
// CriticallyDampedSpring
//
// Smoothly moves 'current' towards 'target' using a critically-damped
// spring formula (no overshoot, no oscillation) that is frame-rate
// independent via the supplied delta-time.
//
// Parameters:
//   current     – value to update (in/out)
//   velocity    – spring velocity state (in/out, persist between frames)
//   target      – desired final value
//   halflife    – time (seconds) to halve the remaining distance;
//                 smaller = snappier (0.04 ≈ very fast, 0.12 ≈ smooth)
//   dt          – frame delta time in seconds
// -----------------------------------------------------------------------
static float CriticallyDampedSpring1D(float current, float& velocity, float target,
                                      float halflife, float dt)
{
    // Clamp dt to avoid blow-up on first frame / long stalls
    const float safeDt = std::clamp(dt, 0.0f, 0.1f);
    if (halflife <= 0.0f || safeDt <= 0.0f) {
        velocity = 0.0f;
        return target;
    }
    // omega from halflife: omega = ln(2) / halflife
    const float omega = 0.6931472f / halflife;
    const float x     = omega * safeDt;
    // Stable approximation for small x (avoids exp() instability)
    const float expTerm = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    const float diff    = current - target;
    const float c       = (velocity + diff * omega) * safeDt;
    velocity = (velocity - c * omega) * expTerm;
    return target + (diff + c) * expTerm;
}

static CVector CriticallyDampedSpringVec(const CVector& current, CVector& velocity,
                                         const CVector& target, float halflife, float dt)
{
    return CVector(
        CriticallyDampedSpring1D(current.x, velocity.x, target.x, halflife, dt),
        CriticallyDampedSpring1D(current.y, velocity.y, target.y, halflife, dt),
        CriticallyDampedSpring1D(current.z, velocity.z, target.z, halflife, dt)
    );
}
// g_sourceCameraSwappedForRender removed: m_mCameraMatrix is now always in gameplay layout.

bool IsFiniteVector(const CVector& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool NormalizeSafe(CVector* value)
{
    if (!value || !IsFiniteVector(*value)) {
        return false;
    }

    const float sqMagnitude = value->SquaredMagnitude();
    if (!std::isfinite(sqMagnitude) || sqMagnitude < 0.000001f) {
        return false;
    }

    value->Normalise();
    return IsFiniteVector(*value);
}

CPlayerPed* LocalPlayerPedForSourceCamera()
{
    if (!pNetGame) {
        return nullptr;
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
    CPlayerPed* playerPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;
    if (!playerPed || !playerPed->m_pPed || playerPed->IsDead()) {
        return nullptr;
    }

    return playerPed;
}

CVector SourceCameraTargetForPed(CPlayerPed* playerPed)
{
    CVector target = playerPed->m_pPed->GetPosition();
    target.z += playerPed->IsInVehicle() ? 1.35f : 0.92f;
    return target;
}

CVector LerpVector(const CVector& from, const CVector& to, float blend)
{
    const float t = std::clamp(blend, 0.0f, 1.0f);
    return from + ((to - from) * t);
}

bool BuildSourceCameraBasis(CVector& front, CVector& right, CVector& cameraUp)
{
    if (!NormalizeSafe(&front)) {
        return false;
    }

    CVector referenceUp{0.0f, 0.0f, 1.0f};
    // If looking almost straight up or down, switch reference vector to avoid singularity/zero cross product
    if (std::abs(front.z) > 0.98f) {
        referenceUp = CVector{0.0f, 1.0f, 0.0f};
    }

    right = referenceUp.Cross(front);
    if (!NormalizeSafe(&right)) {
        right = CVector{1.0f, 0.0f, 0.0f};
    }

    cameraUp = front.Cross(right);
    if (!NormalizeSafe(&cameraUp)) {
        cameraUp = referenceUp;
    }

    // Re-orthogonalize right to ensure perfect orthonormality and prevent any shearing/distortion
    right = cameraUp.Cross(front);
    if (!NormalizeSafe(&right)) {
        right = CVector{1.0f, 0.0f, 0.0f};
    }

    return true;
}

float SourceCameraDistance(const CCam& /*cam*/, const CVector& /*target*/)
{
    // Use a stable fixed distance. Reading cam.m_fCameraDistance or cam.Distance
    // is unreliable: the engine overwrites these fields each frame when in native
    // camera modes, causing the camera to drift arbitrarily far from the player.
    return SOURCE_CAMERA_DEFAULT_DISTANCE;
}

void SeedSourceCameraAngles(CCam& cam, const CVector& target)
{
    CVector front = cam.Front;
    if (!NormalizeSafe(&front)) {
        front = target - cam.Source;
        if (!NormalizeSafe(&front)) {
            front = CVector{0.0f, 1.0f, 0.0f};
        }
    }

    g_sourceCameraYaw = std::atan2(front.x, front.y);
    g_sourceCameraPitch = std::asin(std::clamp(front.z, -0.95f, 0.95f));
    g_sourceCameraPitch = std::clamp(g_sourceCameraPitch, SOURCE_CAMERA_MIN_PITCH, SOURCE_CAMERA_MAX_PITCH);
    g_sourceCameraFilteredDeltaX = 0.0f;
    g_sourceCameraFilteredDeltaY = 0.0f;
    g_sourceCameraSmoothedTarget = target;
    g_sourceCameraSmoothedSource = cam.Source;
    g_sourceCameraSmoothedFrameValid = IsFiniteVector(cam.Source);
    g_sourceCameraLookSeeded = true;
}

CVector ResolveSourceCameraCollision(const CVector& target, const CVector& desiredSource)
{
    if (!IsFiniteVector(target) || !IsFiniteVector(desiredSource)) {
        return desiredSource;
    }

    CVector ray = desiredSource - target;
    const float desiredDistance = std::sqrt(std::max(0.0f, ray.SquaredMagnitude()));
    if (!std::isfinite(desiredDistance) || desiredDistance < SOURCE_CAMERA_COLLISION_MIN_DISTANCE) {
        return desiredSource;
    }
    ray = ray * (1.0f / desiredDistance);

    CColPoint hit{};
    CEntityGTA* hitEntity = nullptr;
    const bool blocked = CWorld::ProcessLineOfSight(
        &target,
        &desiredSource,
        &hit,
        &hitEntity,
        true,
        false,
        false,
        true,
        true,
        true,
        true,
        false);
    if (!blocked || !IsFiniteVector(hit.m_vecPoint)) {
        return desiredSource;
    }

    const CVector targetToHit = hit.m_vecPoint - target;
    const float hitDistance = std::sqrt(std::max(0.0f, targetToHit.SquaredMagnitude()));
    if (!std::isfinite(hitDistance) || hitDistance <= 0.0f || hitDistance >= desiredDistance) {
        return desiredSource;
    }

    const float safeDistance = std::clamp(hitDistance - 0.32f,
                                          SOURCE_CAMERA_COLLISION_MIN_DISTANCE,
                                          desiredDistance);
    return target + (ray * safeDistance);
}

void WarmSourceCameraStreaming(const CVector& source, const CVector& target, int32 areaCode, bool inputMoved)
{
    if (!IsFiniteVector(source) || !IsFiniteVector(target)) {
        return;
    }

    static bool s_seeded = false;
    static uint32_t s_lastWarmTick = 0;
    static uint32_t s_lastRefreshTick = 0;
    static CVector s_lastWarmSource{};
    static CVector s_lastWarmTarget{};

    const uint32_t now = GetTickCount();
    const float sourceMoveSq = s_seeded ? (source - s_lastWarmSource).SquaredMagnitude() : 9999.0f;
    const float targetMoveSq = s_seeded ? (target - s_lastWarmTarget).SquaredMagnitude() : 9999.0f;
    const bool movedEnough = sourceMoveSq > 4.0f || targetMoveSq > 2.0f;
    if (s_seeded && now - s_lastWarmTick < SOURCE_CAMERA_STREAM_COOLDOWN_MS && !movedEnough) {
        return;
    }

    s_seeded = true;
    s_lastWarmTick = now;
    s_lastWarmSource = source;
    s_lastWarmTarget = target;

    CVector forward = target - source;
    forward.z = 0.0f; // Flatten to horizontal plane to prevent prefetching sky/ground limits
    if (!NormalizeSafe(&forward)) {
        forward = CVector{0.0f, 1.0f, 0.0f};
    }

    const CVector worldUp{0.0f, 0.0f, 1.0f};
    CVector right = worldUp.Cross(forward);
    if (!NormalizeSafe(&right)) {
        right = CVector{1.0f, 0.0f, 0.0f};
    }

    CVector cameraUp = forward.Cross(right);
    if (!NormalizeSafe(&cameraUp)) {
        cameraUp = worldUp;
    }

    const bool refreshStreaming = inputMoved || movedEnough || now - s_lastRefreshTick >= SOURCE_CAMERA_STREAM_REFRESH_MS;

    auto warmPoint = [&](CVector sample, const char* reason, bool includeModels, bool includeLods, bool includeCollision, bool refreshGame) {
        if (!IsFiniteVector(sample)) {
            return;
        }
        if (sample.z < 18.0f) {
            sample.z = 18.0f;
        }

        Xyron::Streaming::WorldStreamRequest request{};
        request.areaCode = areaCode;
        request.streamingFlags = STREAMING_DEFAULT;
        request.addModels = includeModels;
        request.addLods = includeLods;
        request.addCollisionNeeded = includeCollision;
        request.setCollisionRequired = includeCollision;
        request.requestCollision = includeCollision;
        request.loadCollision = includeCollision && refreshStreaming;
        request.ensureCollision = includeCollision && refreshStreaming;
        request.addIpls = includeModels;
        request.loadIpls = false;
        request.ensureIpls = false;
        request.loadSceneCollision = includeCollision && refreshStreaming;
        request.loadScene = false;
        request.refreshGame = refreshGame && refreshStreaming;
        request.reason = reason;
        request.ignorePlayerVehicleCollision = true;
        Xyron::Streaming::PrepareWorldAtPoint(sample, request);
    };

    const CVector nearForward = target + (forward * SOURCE_CAMERA_PREFETCH_NEAR);
    const CVector midForward = target + (forward * SOURCE_CAMERA_PREFETCH_MID);
    const CVector farForward = target + (forward * SOURCE_CAMERA_PREFETCH_FAR);
    const CVector leftNear = nearForward - (right * SOURCE_CAMERA_PREFETCH_WIDE);
    const CVector rightNear = nearForward + (right * SOURCE_CAMERA_PREFETCH_WIDE);
    const CVector leftMid = midForward - (right * (SOURCE_CAMERA_PREFETCH_WIDE * 0.75f));
    const CVector rightMid = midForward + (right * (SOURCE_CAMERA_PREFETCH_WIDE * 0.75f));
    const CVector upperNear = nearForward + (cameraUp * SOURCE_CAMERA_PREFETCH_VERTICAL);
    const CVector lowerNear = nearForward - (cameraUp * SOURCE_CAMERA_PREFETCH_VERTICAL);

    warmPoint(target, "source-camera-target", true, true, true, false);
    warmPoint(source, "source-camera-source", false, false, false, false);
    warmPoint(nearForward, "source-camera-frustum-near", true, true, true, false);
    warmPoint(midForward, "source-camera-frustum-mid", true, true, false, false);
    warmPoint(farForward, "source-camera-frustum-far", true, true, false, false);
    warmPoint(leftNear, "source-camera-frustum-left-near", true, true, false, false);
    warmPoint(rightNear, "source-camera-frustum-right-near", true, true, false, false);
    warmPoint(leftMid, "source-camera-frustum-left-mid", true, true, false, false);
    warmPoint(rightMid, "source-camera-frustum-right-mid", true, true, false, false);
    warmPoint(upperNear, "source-camera-frustum-upper", true, true, false, false);
    warmPoint(lowerNear, "source-camera-frustum-lower", true, true, false, false);

    Xyron::Collision::AddCollisionNeededAtPosn(target);
    Xyron::Collision::RequestCollision(target, areaCode);
    Xyron::Collision::AddCollisionNeededAtPosn(nearForward);
    Xyron::Collision::RequestCollision(nearForward, areaCode);
    Xyron::Ipl::AddNeededAtPosition(target);
    Xyron::Ipl::AddNeededAtPosition(nearForward);
    if (refreshStreaming) {
        Xyron::Ipl::LoadAtPosition(target, false);
        Xyron::Ipl::LoadAtPosition(nearForward, false);
        s_lastRefreshTick = now;
    }

    static uint32_t s_lastLoadTick = 0;
    if ((inputMoved || movedEnough || refreshStreaming) && now - s_lastLoadTick >= 110) {
        s_lastLoadTick = now;
        Xyron::Streaming::LoadRequestedModels();
    }

    if (g_sourceCameraStreamingLogCount < 20 && (inputMoved || movedEnough)) {
        ++g_sourceCameraStreamingLogCount;
        FLog("[CAM_STREAM64] cone src=%.2f %.2f %.2f target=%.2f %.2f %.2f near=%.2f %.2f %.2f mid=%.2f %.2f %.2f far=%.2f %.2f %.2f area=%d refresh=%d",
             source.x, source.y, source.z,
             target.x, target.y, target.z,
             nearForward.x, nearForward.y, nearForward.z,
             midForward.x, midForward.y, midForward.z,
             farForward.x, farForward.y, farForward.z,
             areaCode,
             refreshStreaming ? 1 : 0);
    }
}

void WriteSourceCameraFrame(CCamera& camera, CCam& cam, const CVector& rawTarget, float distance, bool inputMoved, int32 areaCode)
{
    const int32 effectiveAreaCode = Xyron::Streaming::ResolveEffectiveWorldAreaCode(rawTarget, areaCode);

    // ---------------------------------------------------------------
    // Frame delta time – frame-rate independent smoothing
    // ---------------------------------------------------------------
    const uint32_t nowTick = GetTickCount();
    float dt = 0.016f; // assume 60 fps as safe default
    if (g_sourceCameraLastTick != 0) {
        const uint32_t elapsed = nowTick - g_sourceCameraLastTick;
        dt = std::clamp(static_cast<float>(elapsed) * 0.001f, 0.001f, 0.1f);
    }
    g_sourceCameraLastTick = nowTick;

    const float sinYaw = std::sin(g_sourceCameraYaw);
    const float cosYaw = std::cos(g_sourceCameraYaw);
    const float sinPitch = std::sin(g_sourceCameraPitch);
    const float cosPitch = std::cos(g_sourceCameraPitch);

    CVector front{sinYaw * cosPitch, cosYaw * cosPitch, sinPitch};
    CVector right{};
    CVector cameraUp{};
    if (!BuildSourceCameraBasis(front, right, cameraUp)) {
        return;
    }

    if (!g_sourceCameraSmoothedFrameValid || !IsFiniteVector(g_sourceCameraSmoothedTarget) || !IsFiniteVector(g_sourceCameraSmoothedSource)) {
        g_sourceCameraSmoothedTarget = rawTarget;
        g_sourceCameraSmoothedSource = cam.Source;
        g_sourceCameraSourceVel = {};
        g_sourceCameraTargetVel = {};
        g_sourceCameraSmoothedFrameValid = true;
    }

    // ---------------------------------------------------------------
    // Target smoothing
    // When idle: snap immediately (no residual drift that causes shake)
    // When moving: use critically-damped spring (halflife ~30ms = very fast)
    // This eliminates the feedback loop where lerp lag caused oscillation.
    // ---------------------------------------------------------------
    CVector target;
    if (!inputMoved) {
        target = rawTarget;
        g_sourceCameraTargetVel = {};
    } else {
        // halflife 0.030s = snappy tracking without overshoot
        target = CriticallyDampedSpringVec(
            g_sourceCameraSmoothedTarget,
            g_sourceCameraTargetVel,
            rawTarget,
            0.030f, dt);
    }
    if (!IsFiniteVector(target)) {
        target = rawTarget;
        g_sourceCameraTargetVel = {};
    }
    g_sourceCameraSmoothedTarget = target;

    // Sphere orbit: camera sits at 'distance' behind the player along the look direction.
    CVector desiredSource = target - (front * distance);
    if (!IsFiniteVector(desiredSource)) {
        desiredSource = cam.Source;
    }

    // ---------------------------------------------------------------
    // Source smoothing
    // When idle: snap immediately to eliminate any residual position drift.
    // When moving: critically-damped spring with halflife ~45ms.
    //   - Fast enough to keep up with smooth player/camera movement
    //   - Slow enough to absorb single-frame position spikes (anti-shake)
    // NOTE: The old fixed lerp(0.72/frame) was frame-rate dependent and
    // accumulated lag that appeared as jitter when the camera rotated.
    // ---------------------------------------------------------------
    CVector source;
    if (!inputMoved) {
        source = desiredSource;
        g_sourceCameraSourceVel = {};
    } else {
        // halflife 0.045s — converges in ~2 frames at 60fps, no overshoot
        source = CriticallyDampedSpringVec(
            g_sourceCameraSmoothedSource,
            g_sourceCameraSourceVel,
            desiredSource,
            0.045f, dt);
    }
    if (!IsFiniteVector(source)) {
        source = desiredSource;
        g_sourceCameraSourceVel = {};
    }

    // Apply collision AFTER smoothing: the final rendered position is always above ground.
    source = ResolveSourceCameraCollision(target, source);

    const float minSourceZ = target.z - SOURCE_CAMERA_MIN_SOURCE_BELOW_TARGET;
    if (std::isfinite(minSourceZ) && source.z < minSourceZ) {
        source.z = minSourceZ;
    }
    g_sourceCameraSmoothedSource = source;

    // Derive the ACTUAL look direction from the camera's physical position to the target.
    // Collision may have moved the source; the render basis must come from where the
    // camera actually is — otherwise the world is rendered with a mismatched transform.
    CVector actualFront = target - source;
    CVector actualRight{};
    CVector actualCameraUp{};
    if (!BuildSourceCameraBasis(actualFront, actualRight, actualCameraUp)) {
        // Degenerate (camera at target): fall back to controlled-angle basis
        actualFront    = front;
        actualRight    = right;
        actualCameraUp = cameraUp;
    } else {
        // NOTE: We intentionally do NOT re-derive g_sourceCameraYaw/Pitch from actualFront.
        // The controlled angles are the single source of truth; overwriting them from the
        // not-yet-converged smoothed source creates a feedback loop that causes shake:
        //   input adds delta → yaw updated → source lerps (slowly) → actualFront is
        //   intermediate → yaw partially overwritten → next frame undoes some rotation →
        //   oscillation. By keeping the controlled angles untouched here the camera
        //   smoothly follows without any feedback-driven shake.
    }

    const float actualPitch = g_sourceCameraPitch;
    const float actualYaw   = g_sourceCameraYaw;

    cam.Front = actualFront;
    cam.Source = source;
    cam.Up = actualCameraUp;
    cam.m_cvecTargetCoorsForFudgeInter = target;
    cam.Alpha = actualPitch;
    cam.m_fTrueAlpha = actualPitch;
    cam.m_fIdealAlpha = actualPitch;
    cam.m_fTrueBeta = actualYaw;
    cam.m_fHorizontalAngle = actualYaw;
    cam.BetaSpeed = 0.0f;
    cam.AlphaSpeed = 0.0f;
    cam.Rotating = inputMoved;
    cam.bBehindPlayerDesired = false;

    camera.m_bCamDirectlyBehind = false;
    camera.m_bCamDirectlyInFront = false;
    camera.m_bCameraJustRestored = false;
    camera.m_bLookingAtVector = false;
    camera.m_bLookingAtPlayer = true;
    const float horizontalMag = std::sqrt(actualFront.x * actualFront.x + actualFront.y * actualFront.y);
    if (horizontalMag > 0.000001f) {
        camera.m_fCamFrontXNorm = actualFront.x / horizontalMag;
        camera.m_fCamFrontYNorm = actualFront.y / horizontalMag;
    } else {
        camera.m_fCamFrontXNorm = 0.0f;
        camera.m_fCamFrontYNorm = 1.0f;
    }
    camera.m_vecGameCamPos = source;
    camera.GetPosition() = source;

    // GTA SA CMatrix layout: Column 0 = Right, Column 1 = Look/Front direction, Column 2 = Screen Up.
    // This is the standard GTA SA camera matrix layout that culling, shaders, and movement expect.
    camera.m_mCameraMatrix.GetRight()    = actualRight;
    camera.m_mCameraMatrix.GetForward()  = actualFront;       // Look vector
    camera.m_mCameraMatrix.GetUp()       = actualCameraUp;    // Up vector
    camera.m_mCameraMatrix.GetPosition() = source;
    RwMatrixUpdate(reinterpret_cast<RwMatrix*>(&camera.m_mCameraMatrix));

    // Calculate view matrix from the standard GTA SA camera matrix.
    CMatrix worldMatrix = camera.m_mCameraMatrix;
    CMatrix viewMatrix  = worldMatrix;
    Invert(worldMatrix, viewMatrix);
    RwMatrixUpdate(reinterpret_cast<RwMatrix*>(&viewMatrix));
    camera.m_viewMatrix = viewMatrix;

    // RenderWare camera expects local Y (Up) = Screen Up, local Z (At) = Look direction.
    // We update the RenderWare camera frame on the fly by swapping columns 1 and 2
    // of a temporary matrix before pushing the update to RenderWare.
    if (camera.m_pRwCamera) {
        RwFrame* frame = RwCameraGetFrame(camera.m_pRwCamera);
        if (frame) {
            CMatrix rwCamMatrix = camera.m_mCameraMatrix;
            rwCamMatrix.GetForward() = actualCameraUp; // Maps to RwMatrix::up (Col 1)
            rwCamMatrix.GetUp()      = actualFront;    // Maps to RwMatrix::at (Col 2)
            rwCamMatrix.UpdateRwMatrix(RwFrameGetMatrix(frame));
            RwFrameUpdateObjects(frame);
        }
    }

    WarmSourceCameraStreaming(source, target, effectiveAreaCode, inputMoved);

    if (inputMoved && g_sourceCameraLogCount < 256) {
        ++g_sourceCameraLogCount;
        FLog("[CAM_SOURCE64] apply yaw=%.3f/%.3f pitch=%.3f/%.3f dist=%.2f src=%.2f %.2f %.2f front=%.3f %.3f %.3f area=%d/%d",
              g_sourceCameraYaw,
              actualYaw,
              g_sourceCameraPitch,
              actualPitch,
              distance,
              source.x,
              source.y,
              source.z,
              actualFront.x,
              actualFront.y,
              actualFront.z,
              areaCode,
              effectiveAreaCode);
    }
}

int16_t ClampSourceHudAxis(int value)
{
    if (value < -128) {
        return -128;
    }
    if (value > 127) {
        return 127;
    }
    return static_cast<int16_t>(value);
}

uint16_t SourceHudAxisToPad(int16_t value)
{
    return static_cast<uint16_t>(value);
}

bool TryGetMappedPadKeyIndex(ePadKeys key, int* outIndex)
{
    const int index = static_cast<int>(key);
    if (index < 0 || index == ePadKeys::SIZE || index >= ePadKeys::SIZE + 4) {
        return false;
    }

    if (outIndex) {
        *outIndex = index;
    }
    return true;
}

bool SetMappedPadKey(ePadKeys key, bool pressed)
{
    int index = 0;
    if (!TryGetMappedPadKeyIndex(key, &index)) {
        return false;
    }

    LocalPlayerKeys.bKeys[index] = pressed;
    return true;
}

bool IsMappedPadKeyDown(ePadKeys key)
{
    int index = 0;
    if (!TryGetMappedPadKeyIndex(key, &index)) {
        return false;
    }

    return LocalPlayerKeys.bKeys[index];
}

void SetMappedAnalogLeftRight(uint16_t value)
{
    LocalPlayerKeys.wKeyLR = value;
}

void SetMappedAnalogUpDown(uint16_t value)
{
    LocalPlayerKeys.wKeyUD = value;
}

uint16_t MappedAnalogLeftRight()
{
    return LocalPlayerKeys.wKeyLR;
}

uint16_t MappedAnalogUpDown()
{
    return LocalPlayerKeys.wKeyUD;
}

bool PressMappedPadKey(ePadKeys key)
{
    return SetMappedPadKey(key, true);
}

bool TryMapSourceButton(SourceButton button, ePadKeys* out)
{
    if (!out) {
        return false;
    }

    switch (button) {
        case SourceButton::Action:
            *out = ePadKeys::KEY_ACTION;
            return true;
        case SourceButton::Crouch:
            *out = ePadKeys::KEY_CROUCH;
            return true;
        case SourceButton::Fire:
            *out = ePadKeys::KEY_FIRE;
            return true;
        case SourceButton::Sprint:
            *out = ePadKeys::KEY_SPRINT;
            return true;
        case SourceButton::SecondaryAttack:
            *out = ePadKeys::KEY_SECONDARY_ATTACK;
            return true;
        case SourceButton::Jump:
            *out = ePadKeys::KEY_JUMP;
            return true;
        case SourceButton::LookRight:
            *out = ePadKeys::KEY_LOOK_RIGHT;
            return true;
        case SourceButton::Handbrake:
            *out = ePadKeys::KEY_HANDBRAKE;
            return true;
        case SourceButton::LookLeft:
            *out = ePadKeys::KEY_LOOK_LEFT;
            return true;
        case SourceButton::Submission:
            *out = ePadKeys::KEY_SUBMISSION;
            return true;
        case SourceButton::Walk:
            *out = ePadKeys::KEY_WALK;
            return true;
        case SourceButton::AnalogUp:
            *out = ePadKeys::KEY_ANALOG_UP;
            return true;
        case SourceButton::AnalogDown:
            *out = ePadKeys::KEY_ANALOG_DOWN;
            return true;
        case SourceButton::AnalogLeft:
            *out = ePadKeys::KEY_ANALOG_LEFT;
            return true;
        case SourceButton::AnalogRight:
            *out = ePadKeys::KEY_ANALOG_RIGHT;
            return true;
        case SourceButton::Yes:
            *out = ePadKeys::KEY_YES;
            return true;
        case SourceButton::No:
            *out = ePadKeys::KEY_NO;
            return true;
        case SourceButton::CtrlBack:
            *out = ePadKeys::KEY_CTRL_BACK;
            return true;
        case SourceButton::Count:
            return false;
    }

    return false;
}

void ClearSourceButton(SourceButton button)
{
    ePadKeys nativeKey = ePadKeys::SIZE;
    if (!TryMapSourceButton(button, &nativeKey)) {
        return;
    }

    SetMappedPadKey(nativeKey, false);
}

void ApplySourceHudAnalogKeys()
{
    SetMappedAnalogLeftRight(SourceHudAxisToPad(g_sourceHudState.analogLeftRight));
    SetMappedAnalogUpDown(SourceHudAxisToPad(g_sourceHudState.analogUpDown));
    SetMappedPadKey(ePadKeys::KEY_ANALOG_LEFT, g_sourceHudState.analogLeftRight < -16);
    SetMappedPadKey(ePadKeys::KEY_ANALOG_RIGHT, g_sourceHudState.analogLeftRight > 16);
    SetMappedPadKey(ePadKeys::KEY_ANALOG_UP, g_sourceHudState.analogUpDown < -16);
    SetMappedPadKey(ePadKeys::KEY_ANALOG_DOWN, g_sourceHudState.analogUpDown > 16);
}

void ApplySourceHudDigitalKeys()
{
    SetMappedPadKey(ePadKeys::KEY_FIRE, g_sourceHudState.firePressed);
    SetMappedPadKey(ePadKeys::KEY_SECONDARY_ATTACK, g_sourceHudState.secondaryPressed);
    SetMappedPadKey(ePadKeys::KEY_SPRINT, g_sourceHudState.sprintPressed || g_sourceHudState.acceleratePressed);
    SetMappedPadKey(ePadKeys::KEY_JUMP, g_sourceHudState.jumpPressed || g_sourceHudState.brakePressed);
    SetMappedPadKey(ePadKeys::KEY_HANDBRAKE, g_sourceHudState.handbrakePressed);
    SetMappedPadKey(ePadKeys::KEY_CROUCH, g_sourceHudState.hornPressed);
}

void ApplySourceHudInputKeys()
{
    ApplySourceHudAnalogKeys();
    ApplySourceHudDigitalKeys();
}

bool IsSourceButtonDownFromStore(SourceButton button)
{
    switch (button) {
        case SourceButton::Fire:
            return g_sourceHudState.firePressed;
        case SourceButton::Sprint:
            return g_sourceHudState.sprintPressed || g_sourceHudState.acceleratePressed;
        case SourceButton::SecondaryAttack:
            return g_sourceHudState.secondaryPressed;
        case SourceButton::Jump:
            return g_sourceHudState.jumpPressed || g_sourceHudState.brakePressed;
        case SourceButton::Handbrake:
            return g_sourceHudState.handbrakePressed;
        case SourceButton::Crouch:
            return g_sourceHudState.hornPressed;
        case SourceButton::AnalogUp:
            return g_sourceHudState.analogUpDown < -16;
        case SourceButton::AnalogDown:
            return g_sourceHudState.analogUpDown > 16;
        case SourceButton::AnalogLeft:
            return g_sourceHudState.analogLeftRight < -16;
        case SourceButton::AnalogRight:
            return g_sourceHudState.analogLeftRight > 16;
        default:
            break;
    }

    ePadKeys nativeKey = ePadKeys::SIZE;
    if (!TryMapSourceButton(button, &nativeKey)) {
        return false;
    }

    return IsMappedPadKeyDown(nativeKey);
}

}

void ProcessNativeFrame(const NativeInputFrameOptions& options)
{
    GTASAEngineApi::ProcessNativeInputFrame(
        options.updatePads,
        options.clearTouchInterface,
        options.updateHid);
}

CWidgetGta** NativeTouchWidgets()
{
    return GTASAEngineApi::TouchInterfaceWidgets();
}

void BindNativeTouchWidgets(CWidgetGta*** target)
{
    if (!target) {
        return;
    }

    *target = NativeTouchWidgets();
}

SourceHudState SnapshotHudState()
{
    return g_sourceHudState;
}

bool IsSourceInputOverrideEnabled()
{
    return true;
}

bool IsSourceFirePressed()
{
    return g_sourceHudState.firePressed;
}

bool IsSourceSecondaryPressed()
{
    return g_sourceHudState.secondaryPressed;
}

bool IsSourceAcceleratePressed()
{
    return g_sourceHudState.acceleratePressed;
}

bool IsSourceBrakePressed()
{
    return g_sourceHudState.brakePressed;
}

bool IsSourceHandbrakePressed()
{
    return g_sourceHudState.handbrakePressed;
}

bool IsSourceSprintPressed()
{
    return g_sourceHudState.sprintPressed;
}

bool IsSourceSprintActionPressed()
{
    return g_sourceHudState.sprintPressed || g_sourceHudState.acceleratePressed;
}

bool IsSourceJumpPressed()
{
    return g_sourceHudState.jumpPressed;
}

bool IsSourceHornPressed()
{
    return g_sourceHudState.hornPressed;
}

bool IsSourceCtrlBackPressed()
{
    return IsSourceButtonDown(SourceButton::CtrlBack);
}

bool IsPassengerDriveByInputPressed()
{
    return IsSourceCtrlBackPressed();
}

int16_t SourceAnalogLeftRight()
{
    return g_sourceHudState.analogLeftRight;
}

int16_t SourceAnalogUpDown()
{
    return g_sourceHudState.analogUpDown;
}

bool IsSourceButtonDown(SourceButton button)
{
    return IsSourceButtonDownFromStore(button);
}

bool IsCompatibilityButtonDown(SourceButton button)
{
    ePadKeys nativeKey = ePadKeys::SIZE;
    if (!TryMapSourceButton(button, &nativeKey)) {
        return false;
    }

    return IsMappedPadKeyDown(nativeKey);
}

void SetCompatibilityButton(SourceButton button, bool pressed)
{
    ePadKeys nativeKey = ePadKeys::SIZE;
    if (!TryMapSourceButton(button, &nativeKey)) {
        return;
    }

    SetMappedPadKey(nativeKey, pressed);
}

void ClearCompatibilityButton(SourceButton button)
{
    SetCompatibilityButton(button, false);
}

void ClearCompatibilityButtons(const SourceButton* buttons, std::size_t count)
{
    if (!buttons) {
        return;
    }

    for (std::size_t i = 0; i < count; ++i) {
        ClearCompatibilityButton(buttons[i]);
    }
}

void ClearAllCompatibilityButtons()
{
    std::memset(LocalPlayerKeys.bKeys, 0, static_cast<std::size_t>(ePadKeys::SIZE) * sizeof(LocalPlayerKeys.bKeys[0]));
}

uint16_t CompatibilityAnalogLeftRight()
{
    return MappedAnalogLeftRight();
}

uint16_t CompatibilityAnalogUpDown()
{
    return MappedAnalogUpDown();
}

void SetCompatibilityAnalogLeftRight(uint16_t value)
{
    SetMappedAnalogLeftRight(value);
}

void SetCompatibilityAnalogUpDown(uint16_t value)
{
    SetMappedAnalogUpDown(value);
}

void SetCompatibilityAnalog(uint16_t leftRight, uint16_t upDown)
{
    SetMappedAnalogLeftRight(leftRight);
    SetMappedAnalogUpDown(upDown);
}

void ClearCompatibilityAnalog()
{
    SetCompatibilityAnalog(0, 0);
}

void ResetCompatibilityPadState()
{
    std::memset(&LocalPlayerKeys, 0, sizeof(LocalPlayerKeys));
}

bool IsJumpDown()
{
    return IsSourceButtonDown(SourceButton::Jump);
}

bool PressSourceButton(SourceButton button)
{
    ePadKeys nativeKey = ePadKeys::SIZE;
    if (!TryMapSourceButton(button, &nativeKey)) {
        return false;
    }

    return PressMappedPadKey(nativeKey);
}

void SetHudActionState(int action, bool pressed)
{
    switch (action) {
        case SOURCE_ACTION_ATTACK:
            g_sourceHudState.firePressed = pressed;
            break;
        case SOURCE_ACTION_SECONDARY:
            g_sourceHudState.secondaryPressed = pressed;
            break;
        case SOURCE_ACTION_ACCELERATE:
            g_sourceHudState.acceleratePressed = pressed;
            break;
        case SOURCE_ACTION_BRAKE:
            g_sourceHudState.brakePressed = pressed;
            break;
        case SOURCE_ACTION_HANDBRAKE:
            g_sourceHudState.handbrakePressed = pressed;
            break;
        case SOURCE_ACTION_SPRINT:
            g_sourceHudState.sprintPressed = pressed;
            break;
        case SOURCE_ACTION_JUMP:
            if (pressed && !g_sourceHudState.jumpPressed) {
                g_sourceHudJumpJustPressed = true;
            }
            g_sourceHudState.jumpPressed = pressed;
            break;
        case SOURCE_ACTION_HORN:
            g_sourceHudState.hornPressed = pressed;
            break;
        default:
            break;
    }

    ApplySourceHudDigitalKeys();
}

void SetHudAnalogState(int leftRight, int upDown)
{
    g_sourceHudState.analogLeftRight = ClampSourceHudAxis(leftRight);
    g_sourceHudState.analogUpDown = ClampSourceHudAxis(upDown);
    ApplySourceHudAnalogKeys();
}

void AddCameraLookDelta(float deltaX, float deltaY, int screenWidth, int screenHeight)
{
    if (!std::isfinite(deltaX) || !std::isfinite(deltaY)) {
        return;
    }

    const float clampedX = std::clamp(deltaX, -320.0f, 320.0f);
    const float clampedY = std::clamp(deltaY, -320.0f, 320.0f);
    const auto scaledX = static_cast<int32_t>(std::lround(clampedX * 1000.0f));
    const auto scaledY = static_cast<int32_t>(std::lround(clampedY * 1000.0f));
    g_sourceCameraDeltaX1000.fetch_add(scaledX, std::memory_order_relaxed);
    g_sourceCameraDeltaY1000.fetch_add(scaledY, std::memory_order_relaxed);

    if (screenWidth > 0) {
        g_sourceCameraScreenWidth.store(screenWidth, std::memory_order_relaxed);
    }
    if (screenHeight > 0) {
        g_sourceCameraScreenHeight.store(screenHeight, std::memory_order_relaxed);
    }

    g_sourceCameraLookRequested.store(true, std::memory_order_release);
}

void ApplySourceCameraLook()
{
    const int32_t scaledDeltaX = g_sourceCameraDeltaX1000.exchange(0, std::memory_order_acq_rel);
    const int32_t scaledDeltaY = g_sourceCameraDeltaY1000.exchange(0, std::memory_order_acq_rel);
    const bool inputMoved = scaledDeltaX != 0 || scaledDeltaY != 0;
    const bool requested = g_sourceCameraLookRequested.exchange(false, std::memory_order_acq_rel);
    if (requested) {
        g_sourceCameraLookActive = true;
    }
    if (!g_sourceCameraLookActive) {
        return;
    }

    CPlayerPed* playerPed = LocalPlayerPedForSourceCamera();
    if (!playerPed) {
        g_sourceCameraLookActive = false;
        g_sourceCameraLookSeeded = false;
        return;
    }

    CCamera& camera = CCamera::GetGameCamera();
    if (camera.m_nActiveCam >= 3) {
        return;
    }

    CCam& cam = camera.GetActiveCamera();
    const CVector target = SourceCameraTargetForPed(playerPed);
    if (!g_sourceCameraLookSeeded) {
        SeedSourceCameraAngles(cam, target);
    }

    if (inputMoved) {
        const float width = static_cast<float>(std::max(g_sourceCameraScreenWidth.load(std::memory_order_relaxed), 1));
        const float height = static_cast<float>(std::max(g_sourceCameraScreenHeight.load(std::memory_order_relaxed), 1));
        const float densityScale = std::clamp(1080.0f / width, 0.65f, 1.35f);
        const float verticalScale = std::clamp(720.0f / height, 0.65f, 1.35f);
        const float deltaX = static_cast<float>(scaledDeltaX) * 0.001f;
        const float deltaY = static_cast<float>(scaledDeltaY) * 0.001f;
        g_sourceCameraFilteredDeltaX = (g_sourceCameraFilteredDeltaX * (1.0f - SOURCE_CAMERA_INPUT_BLEND)) +
                                       (deltaX * SOURCE_CAMERA_INPUT_BLEND);
        g_sourceCameraFilteredDeltaY = (g_sourceCameraFilteredDeltaY * (1.0f - SOURCE_CAMERA_INPUT_BLEND)) +
                                       (deltaY * SOURCE_CAMERA_INPUT_BLEND);
        // FIX: negate X so dragging right rotates camera right (positive deltaX = right).
        g_sourceCameraYaw -= g_sourceCameraFilteredDeltaX * SOURCE_CAMERA_YAW_SENSITIVITY * densityScale;
        g_sourceCameraPitch -= g_sourceCameraFilteredDeltaY * SOURCE_CAMERA_PITCH_SENSITIVITY * verticalScale;
        g_sourceCameraPitch = std::clamp(g_sourceCameraPitch, SOURCE_CAMERA_MIN_PITCH, SOURCE_CAMERA_MAX_PITCH);
    } else {
        // FIX: zero the filtered deltas immediately when no input is active to prevent
        // residual micro-movements that accumulate each frame and cause camera shake.
        g_sourceCameraFilteredDeltaX = 0.0f;
        g_sourceCameraFilteredDeltaY = 0.0f;
    }

    WriteSourceCameraFrame(camera, cam, target, SourceCameraDistance(cam, target), inputMoved, playerPed->m_pPed->m_nAreaCode);
}

bool IsSourceCameraLookActive()
{
    return g_sourceCameraLookActive;
}

bool IsSourceCameraLookRequested()
{
    return g_sourceCameraLookRequested.load(std::memory_order_relaxed);
}

void RestoreCameraMatrixForGameplay()
{
    // No-op: m_mCameraMatrix is always kept in gameplay layout (col1=CamUp, col2=Look).
    // The column-swap that previously existed has been removed; this function is
    // retained only for call-site compatibility and can be removed in a future cleanup.
}

void ResetSourceCameraLook()
{
    g_sourceCameraDeltaX1000.store(0, std::memory_order_release);
    g_sourceCameraDeltaY1000.store(0, std::memory_order_release);
    g_sourceCameraLookRequested.store(false, std::memory_order_release);
    g_sourceCameraLookActive = false;
    g_sourceCameraLookSeeded = false;
    g_sourceCameraFilteredDeltaX = 0.0f;
    g_sourceCameraFilteredDeltaY = 0.0f;
    g_sourceCameraSmoothedFrameValid = false;
    g_sourceCameraSourceVel = {};
    g_sourceCameraTargetVel = {};
    g_sourceCameraLastTick = 0;
}

void ResetHudControls()
{
    g_sourceHudState = {};
    g_sourceHudJumpJustPressed = false;
    ApplySourceHudInputKeys();
}

void PressVehicleAction()
{
    if (!pNetGame) {
        return;
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
    if (!localPlayer) {
        return;
    }

    localPlayer->HandleSourceVehicleAction();
}

void PressVehicleLockAction()
{
    Xyron::AppCommand::SendVehicleLockToggle();
}

void RequestPassengerVehicleAction()
{
    bNeedEnterVehicle = true;
}

bool HasPendingPassengerVehicleAction()
{
    return bNeedEnterVehicle;
}

void ClearPassengerVehicleActionRequest()
{
    bNeedEnterVehicle = false;
}

bool CanProcessPassengerVehicleAction(uint32_t nowTick)
{
    if (g_lastPassengerVehicleActionTick == 0) {
        g_lastPassengerVehicleActionTick = nowTick;
        return false;
    }

    return nowTick - g_lastPassengerVehicleActionTick >= PASSENGER_VEHICLE_ACTION_COOLDOWN_MS;
}

void MarkPassengerVehicleActionHandled(uint32_t nowTick)
{
    g_lastPassengerVehicleActionTick = nowTick;
}

bool ConsumeDriverEnterVehicleTaskStarted(bool taskActive)
{
    const bool started = taskActive && !g_driverEnterVehicleTaskWasActive;
    g_driverEnterVehicleTaskWasActive = taskActive;
    return started;
}

bool ConsumeExitVehicleTaskStarted(bool taskActive)
{
    const bool started = taskActive && !g_exitVehicleTaskWasActive;
    g_exitVehicleTaskWasActive = taskActive;
    return started;
}

void PressCameraAction()
{
    ResetSourceCameraLook();
    CCamera::GetGameCamera().Restore();
    CCamera::SetBehindPlayer();
    FLog("[CAM_INPUT64] reset-behind-player");
}

bool ConsumeJumpJustPressed()
{
    const bool pressed = g_sourceHudJumpJustPressed;
    g_sourceHudJumpJustPressed = false;
    return pressed;
}

uint8_t ConsumeAdditionalKey()
{
    if (IsSourceButtonDown(SourceButton::Yes)) {
        ClearSourceButton(SourceButton::Yes);
        return 1;
    }

    if (IsSourceButtonDown(SourceButton::No)) {
        ClearSourceButton(SourceButton::No);
        return 2;
    }

    if (IsSourceButtonDown(SourceButton::CtrlBack)) {
        ClearSourceButton(SourceButton::CtrlBack);
        return 3;
    }

    return 0;
}

uint16_t PackSampKeys(bool inVehicle, uint16_t* lrAnalog, uint16_t* udAnalog, bool clearMainButtons)
{
    ApplySourceHudInputKeys();

    if (lrAnalog) {
        *lrAnalog = SourceHudAxisToPad(g_sourceHudState.analogLeftRight);
    }
    if (udAnalog) {
        *udAnalog = SourceHudAxisToPad(g_sourceHudState.analogUpDown);
    }

    uint16_t packed = 0;

    if (IsSourceButtonDown(SourceButton::AnalogRight)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::AnalogLeft)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::AnalogDown)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::AnalogUp)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::Walk)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::Submission)) packed |= 1;
    packed <<= 1;
    if (inVehicle && IsSourceButtonDown(SourceButton::LookLeft)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::Handbrake)) packed |= 1;
    packed <<= 1;
    if (inVehicle && IsSourceButtonDown(SourceButton::LookRight)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::Jump)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::SecondaryAttack)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::Sprint)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::Fire)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::Crouch)) packed |= 1;
    packed <<= 1;
    if (IsSourceButtonDown(SourceButton::Action)) packed |= 1;

    if (clearMainButtons) {
        ClearAllCompatibilityButtons();
    }

    return packed;
}

}
