#include "SampCoreDOD.h"

#include "../main.h"
#include "../game/playerped.h"
#include "../net/netgame.h"
#include "../net/localplayer.h"
#include "../net/playerpool.h"
#include "../net/remoteplayer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#if defined(__aarch64__) || defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define XYRON_CORE_HAS_NEON 1
#else
#define XYRON_CORE_HAS_NEON 0
#endif

extern CNetGame* pNetGame;

namespace Xyron::Core
{
namespace
{
alignas(kCacheLineSize) PlayerSoA g_players;
alignas(kCacheLineSize) TextDrawSoA g_textDraws;
alignas(kCacheLineSize) Matrix4x4 g_clipFromWorld;
FrameStats g_stats = {};
bool g_initialized = false;
bool g_hasProjection = false;
float g_viewportW = 0.0f;
float g_viewportH = 0.0f;

bool IsFinite3(float x, float y, float z) noexcept
{
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

void UpdateNearest() noexcept
{
    float nearest = std::numeric_limits<float>::max();
    uint16_t nearestId = 0xFFFF;

    for (std::size_t i = 0; i < g_players.count; ++i)
    {
        const float d = g_players.distance[i];
        if (d < nearest)
        {
            nearest = d;
            nearestId = g_players.id[i];
        }
    }

    g_stats.nearestDistance = nearestId == 0xFFFF ? 0.0f : nearest;
    g_stats.nearestPlayerId = nearestId;
}

void StoreProjectionLane(std::size_t index, float sx, float sy, float cw, uint8_t visible) noexcept
{
    g_players.screenX[index] = sx;
    g_players.screenY[index] = sy;
    g_players.clipW[index] = cw;
    g_players.screenVisible[index] = visible;
}

#if XYRON_CORE_HAS_NEON
float32x4_t MulAdd4(float32x4_t acc, float32x4_t a, float32x4_t b) noexcept
{
#if defined(__aarch64__)
    return vfmaq_f32(acc, a, b);
#else
    return vmlaq_f32(acc, a, b);
#endif
}

float32x4_t Div4(float32x4_t numerator, float32x4_t denominator) noexcept
{
#if defined(__aarch64__)
    return vdivq_f32(numerator, denominator);
#else
    float32x4_t reciprocal = vrecpeq_f32(denominator);
    reciprocal = vmulq_f32(vrecpsq_f32(denominator, reciprocal), reciprocal);
    reciprocal = vmulq_f32(vrecpsq_f32(denominator, reciprocal), reciprocal);
    return vmulq_f32(numerator, reciprocal);
#endif
}

float32x4_t Sqrt4(float32x4_t value) noexcept
{
#if defined(__aarch64__)
    return vsqrtq_f32(value);
#else
    const float32x4_t zero = vdupq_n_f32(0.0f);
    const float32x4_t epsilon = vdupq_n_f32(0.000001f);
    const uint32x4_t positiveMask = vcgtq_f32(value, zero);
    const float32x4_t clamped = vmaxq_f32(value, epsilon);
    float32x4_t reciprocalSqrt = vrsqrteq_f32(clamped);
    reciprocalSqrt = vmulq_f32(vrsqrtsq_f32(vmulq_f32(clamped, reciprocalSqrt), reciprocalSqrt), reciprocalSqrt);
    reciprocalSqrt = vmulq_f32(vrsqrtsq_f32(vmulq_f32(clamped, reciprocalSqrt), reciprocalSqrt), reciprocalSqrt);
    const float32x4_t root = vmulq_f32(clamped, reciprocalSqrt);
    return vbslq_f32(positiveMask, root, zero);
#endif
}
#endif
}

void Initialize() noexcept
{
    std::memset(&g_players, 0, sizeof(g_players));
    std::memset(&g_textDraws, 0, sizeof(g_textDraws));
    std::memset(&g_clipFromWorld, 0, sizeof(g_clipFromWorld));
    g_stats = {};
    g_stats.nearestPlayerId = 0xFFFF;
    g_stats.simdEnabled = XYRON_CORE_HAS_NEON ? 1 : 0;
    g_initialized = true;
}

void BeginFrame() noexcept
{
    if (!g_initialized)
    {
        Initialize();
    }

    g_players.count = 0;
    g_textDraws.count = 0;
    g_stats.frameIndex++;
    g_stats.playerCount = 0;
    g_stats.nearestPlayerId = 0xFFFF;
    g_stats.nearestDistance = 0.0f;
    g_stats.hasProjection = g_hasProjection ? 1 : 0;
    g_stats.simdEnabled = XYRON_CORE_HAS_NEON ? 1 : 0;
}

PlayerSoA& MutablePlayers() noexcept
{
    return g_players;
}

const PlayerSoA& Players() noexcept
{
    return g_players;
}

TextDrawSoA& MutableTextDraws() noexcept
{
    return g_textDraws;
}

const TextDrawSoA& TextDraws() noexcept
{
    return g_textDraws;
}

bool AppendPlayer(uint16_t id, float x, float y, float z) noexcept
{
    if (g_players.count >= kMaxPlayers || !IsFinite3(x, y, z))
    {
        return false;
    }

    const std::size_t index = g_players.count++;
    g_players.id[index] = id;
    g_players.x[index] = x;
    g_players.y[index] = y;
    g_players.z[index] = z;
    g_players.distance[index] = 0.0f;
    g_players.screenX[index] = 0.0f;
    g_players.screenY[index] = 0.0f;
    g_players.clipW[index] = 0.0f;
    g_players.active[index] = 1;
    g_players.screenVisible[index] = 0;
    g_stats.playerCount = static_cast<uint16_t>(g_players.count);
    return true;
}

bool AppendTextDraw(uint16_t id, float left, float top, float right, float bottom) noexcept
{
    if (g_textDraws.count >= kMaxTextDraws)
    {
        return false;
    }

    const std::size_t index = g_textDraws.count++;
    g_textDraws.id[index] = id;
    g_textDraws.left[index] = left;
    g_textDraws.top[index] = top;
    g_textDraws.right[index] = right;
    g_textDraws.bottom[index] = bottom;
    g_textDraws.active[index] = 1;
    return true;
}

std::size_t ComputePlayerDistances(float originX, float originY, float originZ) noexcept
{
    const std::size_t count = g_players.count;
    std::size_t i = 0;

#if XYRON_CORE_HAS_NEON
    const float32x4_t ox = vdupq_n_f32(originX);
    const float32x4_t oy = vdupq_n_f32(originY);
    const float32x4_t oz = vdupq_n_f32(originZ);
    const float32x4_t zero = vdupq_n_f32(0.0f);

    for (; i + 4 <= count; i += 4)
    {
        const float32x4_t px = vld1q_f32(&g_players.x[i]);
        const float32x4_t py = vld1q_f32(&g_players.y[i]);
        const float32x4_t pz = vld1q_f32(&g_players.z[i]);

        const float32x4_t dx = vsubq_f32(px, ox);
        const float32x4_t dy = vsubq_f32(py, oy);
        const float32x4_t dz = vsubq_f32(pz, oz);

        float32x4_t distSq = vmulq_f32(dx, dx);
        distSq = MulAdd4(distSq, dy, dy);
        distSq = MulAdd4(distSq, dz, dz);
        vst1q_f32(&g_players.distance[i], Sqrt4(vmaxq_f32(distSq, zero)));
    }
#endif

    for (; i < count; ++i)
    {
        const float dx = g_players.x[i] - originX;
        const float dy = g_players.y[i] - originY;
        const float dz = g_players.z[i] - originZ;
        g_players.distance[i] = std::sqrt(std::max(0.0f, dx * dx + dy * dy + dz * dz));
    }

    UpdateNearest();
    return count;
}

std::size_t ProjectPlayersRowMajor(const Matrix4x4& clipFromWorld, float viewportW, float viewportH) noexcept
{
    if (viewportW <= 0.0f || viewportH <= 0.0f)
    {
        return 0;
    }

    const std::size_t count = g_players.count;
    std::size_t i = 0;

#if XYRON_CORE_HAS_NEON
    const float32x4_t m00 = vdupq_n_f32(clipFromWorld.m[0]);
    const float32x4_t m01 = vdupq_n_f32(clipFromWorld.m[1]);
    const float32x4_t m02 = vdupq_n_f32(clipFromWorld.m[2]);
    const float32x4_t m03 = vdupq_n_f32(clipFromWorld.m[3]);
    const float32x4_t m10 = vdupq_n_f32(clipFromWorld.m[4]);
    const float32x4_t m11 = vdupq_n_f32(clipFromWorld.m[5]);
    const float32x4_t m12 = vdupq_n_f32(clipFromWorld.m[6]);
    const float32x4_t m13 = vdupq_n_f32(clipFromWorld.m[7]);
    const float32x4_t m30 = vdupq_n_f32(clipFromWorld.m[12]);
    const float32x4_t m31 = vdupq_n_f32(clipFromWorld.m[13]);
    const float32x4_t m32 = vdupq_n_f32(clipFromWorld.m[14]);
    const float32x4_t m33 = vdupq_n_f32(clipFromWorld.m[15]);
    const float32x4_t half = vdupq_n_f32(0.5f);
    const float32x4_t one = vdupq_n_f32(1.0f);
    const float32x4_t minW = vdupq_n_f32(0.0001f);
    const float32x4_t vw = vdupq_n_f32(viewportW);
    const float32x4_t vh = vdupq_n_f32(viewportH);

    for (; i + 4 <= count; i += 4)
    {
        const float32x4_t x = vld1q_f32(&g_players.x[i]);
        const float32x4_t y = vld1q_f32(&g_players.y[i]);
        const float32x4_t z = vld1q_f32(&g_players.z[i]);

        float32x4_t clipX = MulAdd4(m03, x, m00);
        clipX = MulAdd4(clipX, y, m01);
        clipX = MulAdd4(clipX, z, m02);

        float32x4_t clipY = MulAdd4(m13, x, m10);
        clipY = MulAdd4(clipY, y, m11);
        clipY = MulAdd4(clipY, z, m12);

        float32x4_t clipW = MulAdd4(m33, x, m30);
        clipW = MulAdd4(clipW, y, m31);
        clipW = MulAdd4(clipW, z, m32);

        const uint32x4_t validW = vcgtq_f32(clipW, minW);
        const float32x4_t invW = Div4(one, clipW);
        const float32x4_t ndcX = vmulq_f32(clipX, invW);
        const float32x4_t ndcY = vmulq_f32(clipY, invW);
        const float32x4_t screenX = vmulq_f32(vaddq_f32(vmulq_f32(ndcX, half), half), vw);
        const float32x4_t screenY = vmulq_f32(vsubq_f32(half, vmulq_f32(ndcY, half)), vh);

        alignas(16) float sx[4];
        alignas(16) float sy[4];
        alignas(16) float cw[4];
        alignas(16) uint32_t visibleW[4];
        vst1q_f32(sx, screenX);
        vst1q_f32(sy, screenY);
        vst1q_f32(cw, clipW);
        vst1q_u32(visibleW, validW);

        for (std::size_t lane = 0; lane < 4; ++lane)
        {
            const uint8_t visible = visibleW[lane] != 0 &&
                sx[lane] >= 0.0f && sx[lane] <= viewportW &&
                sy[lane] >= 0.0f && sy[lane] <= viewportH;
            StoreProjectionLane(i + lane, sx[lane], sy[lane], cw[lane], visible);
        }
    }
#endif

    for (; i < count; ++i)
    {
        const float x = g_players.x[i];
        const float y = g_players.y[i];
        const float z = g_players.z[i];
        const float clipX = clipFromWorld.m[0] * x + clipFromWorld.m[1] * y + clipFromWorld.m[2] * z + clipFromWorld.m[3];
        const float clipY = clipFromWorld.m[4] * x + clipFromWorld.m[5] * y + clipFromWorld.m[6] * z + clipFromWorld.m[7];
        const float clipW = clipFromWorld.m[12] * x + clipFromWorld.m[13] * y + clipFromWorld.m[14] * z + clipFromWorld.m[15];

        if (clipW <= 0.0001f)
        {
            StoreProjectionLane(i, 0.0f, 0.0f, clipW, 0);
            continue;
        }

        const float ndcX = clipX / clipW;
        const float ndcY = clipY / clipW;
        const float sx = (ndcX * 0.5f + 0.5f) * viewportW;
        const float sy = (0.5f - ndcY * 0.5f) * viewportH;
        const uint8_t visible = sx >= 0.0f && sx <= viewportW && sy >= 0.0f && sy <= viewportH;
        StoreProjectionLane(i, sx, sy, clipW, visible);
    }

    return count;
}

void SetProjectionMatrixRowMajor(const Matrix4x4& clipFromWorld, float viewportW, float viewportH) noexcept
{
    g_clipFromWorld = clipFromWorld;
    g_viewportW = viewportW;
    g_viewportH = viewportH;
    g_hasProjection = viewportW > 0.0f && viewportH > 0.0f;
}

void ClearProjectionMatrix() noexcept
{
    g_hasProjection = false;
    g_viewportW = 0.0f;
    g_viewportH = 0.0f;
}

void TickFromNetGame() noexcept
{
    BeginFrame();

    if (!pNetGame || !pNetGame->GetPlayerPool())
    {
        return;
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    CVector origin(0.0f, 0.0f, 0.0f);
    bool hasOrigin = false;

    if (CLocalPlayer* localPlayer = playerPool->GetLocalPlayer())
    {
        if (CPlayerPed* localPed = localPlayer->GetPlayerPed())
        {
            if (localPed->m_pPed)
            {
                origin = localPed->m_pPed->GetPosition();
                hasOrigin = true;
                AppendPlayer(playerPool->GetLocalPlayerID(), origin.x, origin.y, origin.z);
            }
        }
    }

    for (PLAYERID playerId = 0; playerId < MAX_PLAYERS; ++playerId)
    {
        CRemotePlayer* remotePlayer = playerPool->GetAt(playerId);
        if (!remotePlayer || !remotePlayer->IsActive())
        {
            continue;
        }

        CPlayerPed* ped = remotePlayer->GetPlayerPed();
        if (!ped || !ped->m_pPed || ped->m_pPed->m_bRemoveFromWorld)
        {
            continue;
        }

        const CVector pos = ped->m_pPed->GetPosition();
        AppendPlayer(playerId, pos.x, pos.y, pos.z);
    }

    if (hasOrigin)
    {
        ComputePlayerDistances(origin.x, origin.y, origin.z);
    }

    if (g_hasProjection)
    {
        ProjectPlayersRowMajor(g_clipFromWorld, g_viewportW, g_viewportH);
    }
}

FrameStats GetFrameStats() noexcept
{
    return g_stats;
}

const char* GetAuditString() noexcept
{
#if XYRON_CORE_HAS_NEON
    return "core=SoA players=1004 textdraws=3072 neon=on alloc_frame=zero projection=caller_matrix";
#else
    return "core=SoA players=1004 textdraws=3072 neon=off alloc_frame=zero projection=caller_matrix";
#endif
}
}

extern "C" void XyronCoreInitialize() noexcept
{
    Xyron::Core::Initialize();
}

extern "C" void XyronCoreFrameTick() noexcept
{
    Xyron::Core::TickFromNetGame();
}

extern "C" const char* XyronCoreAuditString() noexcept
{
    return Xyron::Core::GetAuditString();
}
