#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Xyron::Perf {

enum class Zone : uint8_t {
    Frame = 0,
    MainLoop,
    GameProcess,
    NetGame,
    NetworkReceive,
    Pools,
    Objects,
    ObjectMaterial,
    Streaming,
    Collision,
    Texture,
    UiRender,
    Audio,
    Camera,
    WorldProcess,
    Count
};

struct ZoneSnapshot {
    uint64_t calls = 0;
    uint64_t totalNs = 0;
    uint64_t maxNs = 0;
};

struct Snapshot {
    uint32_t fps = 0;
    uint64_t frameCount = 0;
    uint64_t avgFrameNs = 0;
    uint64_t maxFrameNs = 0;
    std::array<ZoneSnapshot, static_cast<size_t>(Zone::Count)> zones{};
};

void Reset() noexcept;
void BeginFrame() noexcept;
void EndFrame() noexcept;
void AddSample(Zone zone, uint64_t elapsedNs) noexcept;
Snapshot GetSnapshot() noexcept;
const char* ZoneName(Zone zone) noexcept;

class ScopedZone {
public:
    explicit ScopedZone(Zone zone) noexcept;
    ~ScopedZone() noexcept;

    ScopedZone(const ScopedZone&) = delete;
    ScopedZone& operator=(const ScopedZone&) = delete;

private:
    Zone m_zone;
    uint64_t m_startNs;
};

} // namespace Xyron::Perf
