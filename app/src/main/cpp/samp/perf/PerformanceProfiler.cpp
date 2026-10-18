#include "PerformanceProfiler.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <time.h>

#include "../main.h"

namespace Xyron::Perf {
namespace {

constexpr uint64_t kReportIntervalNs = 1500ull * 1000ull * 1000ull;
constexpr size_t kZoneCount = static_cast<size_t>(Zone::Count);

struct ZoneCounters {
    std::atomic<uint64_t> calls{0};
    std::atomic<uint64_t> totalNs{0};
    std::atomic<uint64_t> maxNs{0};
};

std::array<ZoneCounters, kZoneCount> g_zones{};
std::atomic<uint64_t> g_frameCount{0};
std::atomic<uint64_t> g_frameTotalNs{0};
std::atomic<uint64_t> g_frameMaxNs{0};
std::atomic<uint32_t> g_lastFps{0};
std::atomic<uint64_t> g_frameStartNs{0};
std::atomic<uint64_t> g_reportStartNs{0};
std::atomic<uint64_t> g_reportFrameCount{0};

uint64_t NowNs() noexcept
{
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec);
}

void AtomicMax(std::atomic<uint64_t>& target, uint64_t value) noexcept
{
    uint64_t current = target.load(std::memory_order_relaxed);
    while (current < value &&
           !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

uint64_t ExchangeZoneTotal(Zone zone) noexcept
{
    return g_zones[static_cast<size_t>(zone)].totalNs.exchange(0, std::memory_order_relaxed);
}

uint64_t ExchangeZoneCalls(Zone zone) noexcept
{
    return g_zones[static_cast<size_t>(zone)].calls.exchange(0, std::memory_order_relaxed);
}

uint64_t ExchangeZoneMax(Zone zone) noexcept
{
    return g_zones[static_cast<size_t>(zone)].maxNs.exchange(0, std::memory_order_relaxed);
}

void LogReport(uint64_t elapsedNs) noexcept
{
    const uint64_t frames = g_reportFrameCount.exchange(0, std::memory_order_relaxed);
    if (frames == 0 || elapsedNs == 0) {
        return;
    }

    const uint32_t fps = static_cast<uint32_t>((frames * 1000000000ull + elapsedNs / 2ull) / elapsedNs);
    g_lastFps.store(std::min<uint32_t>(fps, 999), std::memory_order_relaxed);

    const uint64_t frameTotal = ExchangeZoneTotal(Zone::Frame);
    const uint64_t frameMax = ExchangeZoneMax(Zone::Frame);
    ExchangeZoneCalls(Zone::Frame);

    auto zoneMs = [](uint64_t totalNs, uint64_t calls) -> float {
        return calls ? static_cast<float>(totalNs) / static_cast<float>(calls) / 1000000.0f : 0.0f;
    };
    auto maxMs = [](uint64_t ns) -> float {
        return static_cast<float>(ns) / 1000000.0f;
    };

    const uint64_t mainCalls = ExchangeZoneCalls(Zone::MainLoop);
    const uint64_t mainTotal = ExchangeZoneTotal(Zone::MainLoop);
    const uint64_t mainMax = ExchangeZoneMax(Zone::MainLoop);
    const uint64_t gameCalls = ExchangeZoneCalls(Zone::GameProcess);
    const uint64_t gameTotal = ExchangeZoneTotal(Zone::GameProcess);
    const uint64_t gameMax = ExchangeZoneMax(Zone::GameProcess);
    const uint64_t netCalls = ExchangeZoneCalls(Zone::NetGame);
    const uint64_t netTotal = ExchangeZoneTotal(Zone::NetGame);
    const uint64_t netMax = ExchangeZoneMax(Zone::NetGame);
    const uint64_t poolCalls = ExchangeZoneCalls(Zone::Pools);
    const uint64_t poolTotal = ExchangeZoneTotal(Zone::Pools);
    const uint64_t poolMax = ExchangeZoneMax(Zone::Pools);
    const uint64_t objCalls = ExchangeZoneCalls(Zone::Objects);
    const uint64_t objTotal = ExchangeZoneTotal(Zone::Objects);
    const uint64_t objMax = ExchangeZoneMax(Zone::Objects);
    const uint64_t streamCalls = ExchangeZoneCalls(Zone::Streaming);
    const uint64_t streamTotal = ExchangeZoneTotal(Zone::Streaming);
    const uint64_t streamMax = ExchangeZoneMax(Zone::Streaming);
    const uint64_t colCalls = ExchangeZoneCalls(Zone::Collision);
    const uint64_t colTotal = ExchangeZoneTotal(Zone::Collision);
    const uint64_t colMax = ExchangeZoneMax(Zone::Collision);
    const uint64_t texCalls = ExchangeZoneCalls(Zone::Texture);
    const uint64_t texTotal = ExchangeZoneTotal(Zone::Texture);
    const uint64_t texMax = ExchangeZoneMax(Zone::Texture);
    const uint64_t uiCalls = ExchangeZoneCalls(Zone::UiRender);
    const uint64_t uiTotal = ExchangeZoneTotal(Zone::UiRender);
    const uint64_t uiMax = ExchangeZoneMax(Zone::UiRender);
    const uint64_t audioCalls = ExchangeZoneCalls(Zone::Audio);
    const uint64_t audioTotal = ExchangeZoneTotal(Zone::Audio);
    const uint64_t audioMax = ExchangeZoneMax(Zone::Audio);
    const uint64_t cameraCalls = ExchangeZoneCalls(Zone::Camera);
    const uint64_t cameraTotal = ExchangeZoneTotal(Zone::Camera);
    const uint64_t cameraMax = ExchangeZoneMax(Zone::Camera);
    const uint64_t worldCalls = ExchangeZoneCalls(Zone::WorldProcess);
    const uint64_t worldTotal = ExchangeZoneTotal(Zone::WorldProcess);
    const uint64_t worldMax = ExchangeZoneMax(Zone::WorldProcess);
    const uint64_t networkRxCalls = ExchangeZoneCalls(Zone::NetworkReceive);
    const uint64_t networkRxTotal = ExchangeZoneTotal(Zone::NetworkReceive);
    const uint64_t networkRxMax = ExchangeZoneMax(Zone::NetworkReceive);
    const uint64_t materialCalls = ExchangeZoneCalls(Zone::ObjectMaterial);
    const uint64_t materialTotal = ExchangeZoneTotal(Zone::ObjectMaterial);
    const uint64_t materialMax = ExchangeZoneMax(Zone::ObjectMaterial);

    g_frameCount.fetch_add(frames, std::memory_order_relaxed);
    g_frameTotalNs.fetch_add(frameTotal, std::memory_order_relaxed);
    AtomicMax(g_frameMaxNs, frameMax);

    FLog("[PERF] fps=%u frame=%.2fms max=%.2fms main=%.2f/%.2f game=%.2f/%.2f net=%.2f/%.2f pools=%.2f/%.2f obj=%.2f/%.2f mat=%.2f/%.2f stream=%.2f/%.2f col=%.2f/%.2f tex=%.2f/%.2f ui=%.2f/%.2f audio=%.2f/%.2f cam=%.2f/%.2f world=%.2f/%.2f rx=%.2f/%.2f calls{s=%llu,c=%llu,t=%llu,o=%llu}",
         fps,
         zoneMs(frameTotal, frames),
         maxMs(frameMax),
         zoneMs(mainTotal, mainCalls),
         maxMs(mainMax),
         zoneMs(gameTotal, gameCalls),
         maxMs(gameMax),
         zoneMs(netTotal, netCalls),
         maxMs(netMax),
         zoneMs(poolTotal, poolCalls),
         maxMs(poolMax),
         zoneMs(objTotal, objCalls),
         maxMs(objMax),
         zoneMs(materialTotal, materialCalls),
         maxMs(materialMax),
         zoneMs(streamTotal, streamCalls),
         maxMs(streamMax),
         zoneMs(colTotal, colCalls),
         maxMs(colMax),
         zoneMs(texTotal, texCalls),
         maxMs(texMax),
         zoneMs(uiTotal, uiCalls),
         maxMs(uiMax),
         zoneMs(audioTotal, audioCalls),
         maxMs(audioMax),
         zoneMs(cameraTotal, cameraCalls),
         maxMs(cameraMax),
         zoneMs(worldTotal, worldCalls),
         maxMs(worldMax),
         zoneMs(networkRxTotal, networkRxCalls),
         maxMs(networkRxMax),
         static_cast<unsigned long long>(streamCalls),
         static_cast<unsigned long long>(colCalls),
         static_cast<unsigned long long>(texCalls),
         static_cast<unsigned long long>(objCalls));
}

} // namespace

void Reset() noexcept
{
    for (auto& zone : g_zones) {
        zone.calls.store(0, std::memory_order_relaxed);
        zone.totalNs.store(0, std::memory_order_relaxed);
        zone.maxNs.store(0, std::memory_order_relaxed);
    }
    g_frameCount.store(0, std::memory_order_relaxed);
    g_frameTotalNs.store(0, std::memory_order_relaxed);
    g_frameMaxNs.store(0, std::memory_order_relaxed);
    g_lastFps.store(0, std::memory_order_relaxed);
    const uint64_t now = NowNs();
    g_frameStartNs.store(now, std::memory_order_relaxed);
    g_reportStartNs.store(now, std::memory_order_relaxed);
    g_reportFrameCount.store(0, std::memory_order_relaxed);
}

void BeginFrame() noexcept
{
    const uint64_t now = NowNs();
    if (g_reportStartNs.load(std::memory_order_relaxed) == 0) {
        g_reportStartNs.store(now, std::memory_order_relaxed);
    }
    g_frameStartNs.store(now, std::memory_order_relaxed);
}

void EndFrame() noexcept
{
    const uint64_t now = NowNs();
    const uint64_t start = g_frameStartNs.load(std::memory_order_relaxed);
    if (start != 0 && now >= start) {
        AddSample(Zone::Frame, now - start);
    }
    g_reportFrameCount.fetch_add(1, std::memory_order_relaxed);

    uint64_t reportStart = g_reportStartNs.load(std::memory_order_relaxed);
    if (reportStart != 0 && now - reportStart >= kReportIntervalNs) {
        if (g_reportStartNs.compare_exchange_strong(
                reportStart,
                now,
                std::memory_order_relaxed)) {
            LogReport(now - reportStart);
        }
    }
}

void AddSample(Zone zone, uint64_t elapsedNs) noexcept
{
    const size_t index = static_cast<size_t>(zone);
    if (index >= kZoneCount || elapsedNs == 0) {
        return;
    }

    ZoneCounters& counters = g_zones[index];
    counters.calls.fetch_add(1, std::memory_order_relaxed);
    counters.totalNs.fetch_add(elapsedNs, std::memory_order_relaxed);
    AtomicMax(counters.maxNs, elapsedNs);
}

Snapshot GetSnapshot() noexcept
{
    Snapshot snapshot{};
    snapshot.fps = g_lastFps.load(std::memory_order_relaxed);
    snapshot.frameCount = g_frameCount.load(std::memory_order_relaxed);
    const uint64_t totalFrameNs = g_frameTotalNs.load(std::memory_order_relaxed);
    snapshot.avgFrameNs = snapshot.frameCount ? totalFrameNs / snapshot.frameCount : 0;
    snapshot.maxFrameNs = g_frameMaxNs.load(std::memory_order_relaxed);

    for (size_t i = 0; i < kZoneCount; ++i) {
        snapshot.zones[i].calls = g_zones[i].calls.load(std::memory_order_relaxed);
        snapshot.zones[i].totalNs = g_zones[i].totalNs.load(std::memory_order_relaxed);
        snapshot.zones[i].maxNs = g_zones[i].maxNs.load(std::memory_order_relaxed);
    }
    return snapshot;
}

const char* ZoneName(Zone zone) noexcept
{
    switch (zone) {
        case Zone::Frame: return "frame";
        case Zone::MainLoop: return "main";
        case Zone::GameProcess: return "game";
        case Zone::NetGame: return "net";
        case Zone::NetworkReceive: return "rx";
        case Zone::Pools: return "pools";
        case Zone::Objects: return "objects";
        case Zone::ObjectMaterial: return "material";
        case Zone::Streaming: return "stream";
        case Zone::Collision: return "collision";
        case Zone::Texture: return "texture";
        case Zone::UiRender: return "ui";
        case Zone::Audio: return "audio";
        case Zone::Camera: return "camera";
        case Zone::WorldProcess: return "world";
        case Zone::Count: break;
    }
    return "?";
}

ScopedZone::ScopedZone(Zone zone) noexcept
    : m_zone(zone),
      m_startNs(NowNs())
{
}

ScopedZone::~ScopedZone() noexcept
{
    const uint64_t now = NowNs();
    if (now >= m_startNs) {
        AddSample(m_zone, now - m_startNs);
    }
}

} // namespace Xyron::Perf
