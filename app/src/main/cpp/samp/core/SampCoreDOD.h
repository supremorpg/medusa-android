#pragma once

#include <cstddef>
#include <cstdint>

namespace Xyron::Core
{
constexpr std::size_t kCacheLineSize = 64;
constexpr std::size_t kMaxPlayers = 1004;
constexpr std::size_t kMaxTextDraws = 3072;

struct Matrix4x4
{
    float m[16];
};

struct alignas(kCacheLineSize) PlayerSoA
{
    uint16_t id[kMaxPlayers];
    float x[kMaxPlayers];
    float y[kMaxPlayers];
    float z[kMaxPlayers];
    float distance[kMaxPlayers];
    float screenX[kMaxPlayers];
    float screenY[kMaxPlayers];
    float clipW[kMaxPlayers];
    uint8_t active[kMaxPlayers];
    uint8_t screenVisible[kMaxPlayers];
    std::size_t count;
};

struct alignas(kCacheLineSize) TextDrawSoA
{
    uint16_t id[kMaxTextDraws];
    float left[kMaxTextDraws];
    float top[kMaxTextDraws];
    float right[kMaxTextDraws];
    float bottom[kMaxTextDraws];
    uint8_t active[kMaxTextDraws];
    std::size_t count;
};

struct FrameStats
{
    uint32_t frameIndex;
    uint16_t playerCount;
    uint16_t nearestPlayerId;
    float nearestDistance;
    uint8_t hasProjection;
    uint8_t simdEnabled;
};

void Initialize() noexcept;
void BeginFrame() noexcept;

PlayerSoA& MutablePlayers() noexcept;
const PlayerSoA& Players() noexcept;
TextDrawSoA& MutableTextDraws() noexcept;
const TextDrawSoA& TextDraws() noexcept;

bool AppendPlayer(uint16_t id, float x, float y, float z) noexcept;
bool AppendTextDraw(uint16_t id, float left, float top, float right, float bottom) noexcept;

std::size_t ComputePlayerDistances(float originX, float originY, float originZ) noexcept;
std::size_t ProjectPlayersRowMajor(const Matrix4x4& clipFromWorld, float viewportW, float viewportH) noexcept;

void SetProjectionMatrixRowMajor(const Matrix4x4& clipFromWorld, float viewportW, float viewportH) noexcept;
void ClearProjectionMatrix() noexcept;

void TickFromNetGame() noexcept;
FrameStats GetFrameStats() noexcept;
const char* GetAuditString() noexcept;
}

extern "C" void XyronCoreInitialize() noexcept;
extern "C" void XyronCoreFrameTick() noexcept;
extern "C" const char* XyronCoreAuditString() noexcept;
