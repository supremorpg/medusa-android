#pragma once

#include <array>
#include <cstdint>

#include "imgui.h"
#include "game/Core/Vector.h"
#include "game/RW/RenderWare.h"

class NativeRadarCrosshair {
public:
    bool SuppressNativeRadar() const { return m_replaceRadar; }
    bool SuppressNativeCrosshair() const { return m_replaceCrosshair; }

    void SetReplaceRadar(bool enabled) { m_replaceRadar = enabled; }
    void SetReplaceCrosshair(bool enabled) { m_replaceCrosshair = enabled; }

    void RenderRadar(ImDrawList* draw, const ImVec2& displaySize);
    void RenderCrosshair(ImDrawList* draw, const ImVec2& displaySize);

private:
    static ImVec2 WorldToRadar(const CVector& local, const RwMatrix& localMatrix,
                               const CVector& world, const ImVec2& center, float radius);
    static void DrawBlip(ImDrawList* draw, const ImVec2& pos, ImU32 color, float radius);
    void RefreshRadarCache(const CVector& local, float scale);

    struct RadarBlip {
        CVector position{};
        ImU32 color = 0;
        float radius = 0.0f;
    };

    static constexpr int kMaxCachedVehicles = 96;
    static constexpr int kMaxCachedPlayers = 64;
    static constexpr uint32_t kRadarCacheIntervalMs = 125;
    static constexpr float kRadarCullDistance = 260.0f;

    bool m_replaceRadar = true;
    bool m_replaceCrosshair = true;
    uint32_t m_lastRadarCacheMs = 0;
    int m_vehicleBlipCount = 0;
    int m_playerBlipCount = 0;
    std::array<RadarBlip, kMaxCachedVehicles> m_vehicleBlips{};
    std::array<RadarBlip, kMaxCachedPlayers> m_playerBlips{};
};
