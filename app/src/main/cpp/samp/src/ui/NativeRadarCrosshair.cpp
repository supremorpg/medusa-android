#include "NativeRadarCrosshair.h"

#include <algorithm>
#include <cmath>

#include "main.h"
#include "game/game.h"
#include "game/GTASAEngineApi.h"
#include "game/aimstuff.h"
#include "net/netgame.h"

extern CGame* pGame;
extern CNetGame* pNetGame;
extern bool g_bHudMapEnabled;

ImVec2 NativeRadarCrosshair::WorldToRadar(const CVector& local, const RwMatrix& localMatrix,
                                          const CVector& world, const ImVec2& center, float radius) {
    const float dx = world.x - local.x;
    const float dy = world.y - local.y;
    const float invRange = radius / 180.0f;

    const float forwardX = localMatrix.up.x;
    const float forwardY = localMatrix.up.y;
    const float rightX = localMatrix.right.x;
    const float rightY = localMatrix.right.y;

    float rx = (dx * rightX + dy * rightY) * invRange;
    float ry = -(dx * forwardX + dy * forwardY) * invRange;

    const float len = std::sqrt(rx * rx + ry * ry);
    if (len > radius - 5.0f) {
        const float scale = (radius - 5.0f) / len;
        rx *= scale;
        ry *= scale;
    }

    return ImVec2(center.x + rx, center.y + ry);
}

void NativeRadarCrosshair::DrawBlip(ImDrawList* draw, const ImVec2& pos, ImU32 color, float radius) {
    draw->AddCircleFilled(pos, radius + 1.0f, IM_COL32(0, 0, 0, 190), 6);
    draw->AddCircleFilled(pos, radius, color, 6);
}

void NativeRadarCrosshair::RefreshRadarCache(const CVector& local, float scale) {
    m_vehicleBlipCount = 0;
    m_playerBlipCount = 0;

    if (!pNetGame) {
        return;
    }

    const float maxDistanceSq = kRadarCullDistance * kRadarCullDistance;
    if (CVehiclePool* vehicles = pNetGame->GetVehiclePool()) {
        for (VEHICLEID id = 0; id < MAX_VEHICLES && m_vehicleBlipCount < kMaxCachedVehicles; ++id) {
            if (!vehicles->GetSlotState(id)) continue;
            CVehicle* vehicle = vehicles->GetAt(id);
            if (!vehicle || !vehicle->m_pVehicle) continue;

            const CVector pos = vehicle->m_pVehicle->GetPosition();
            const float dx = pos.x - local.x;
            const float dy = pos.y - local.y;
            if (dx * dx + dy * dy > maxDistanceSq) continue;

            RadarBlip& blip = m_vehicleBlips[m_vehicleBlipCount++];
            blip.position = pos;
            blip.color = IM_COL32(235, 235, 235, 210);
            blip.radius = 2.6f * scale;
        }
    }

    CPlayerPool* players = pNetGame->GetPlayerPool();
    if (!players) {
        return;
    }

    for (PLAYERID id = 0; id < MAX_PLAYERS && m_playerBlipCount < kMaxCachedPlayers; ++id) {
        if (!players->GetSlotState(id)) continue;
        CRemotePlayer* remote = players->GetAt(id);
        if (!remote || !remote->IsActive() || remote->IsNPC()) continue;
        CPlayerPed* ped = remote->GetPlayerPed();
        if (!ped || !ped->m_pPed) continue;

        const CVector pos = ped->m_pPed->GetPosition();
        const float dx = pos.x - local.x;
        const float dy = pos.y - local.y;
        if (dx * dx + dy * dy > maxDistanceSq) continue;

        RadarBlip& blip = m_playerBlips[m_playerBlipCount++];
        blip.position = pos;
        blip.color = IM_COL32(60, 205, 95, 255);
        blip.radius = 3.4f * scale;
    }
}

void NativeRadarCrosshair::RenderRadar(ImDrawList* draw, const ImVec2& displaySize) {
    if (!g_bHudMapEnabled || !m_replaceRadar || !draw || !pGame || !pNetGame) return;

    CPlayerPed* localPed = pGame->FindPlayerPed();
    if (!localPed || !localPed->m_pPed) return;

    const CVector local = localPed->m_pPed->GetPosition();
    const RwMatrix localMatrix = localPed->m_pPed->GetMatrix().ToRwMatrix();
    const float scale = std::max(0.75f, displaySize.y / 1080.0f);
    const uint32_t now = GetTickCount();
    if (m_lastRadarCacheMs == 0 || now - m_lastRadarCacheMs >= kRadarCacheIntervalMs) {
        RefreshRadarCache(local, scale);
        m_lastRadarCacheMs = now;
    }

    float size = 176.0f * scale;
    ImVec2 origin = ImVec2(28.0f * scale, displaySize.y - size - 30.0f * scale);

    const ImVec2 center(origin.x + size * 0.5f, origin.y + size * 0.5f);
    const float radius = size * 0.5f;
    draw->AddCircleFilled(center, radius, IM_COL32(9, 17, 22, 190), 48);
    draw->AddCircle(center, radius, IM_COL32(255, 255, 255, 55), 48, 2.0f * scale);
    draw->AddLine(ImVec2(center.x - radius, center.y), ImVec2(center.x + radius, center.y), IM_COL32(255, 255, 255, 26), 1.0f);
    draw->AddLine(ImVec2(center.x, center.y - radius), ImVec2(center.x, center.y + radius), IM_COL32(255, 255, 255, 26), 1.0f);

    for (int i = 0; i < m_vehicleBlipCount; ++i) {
        const RadarBlip& blip = m_vehicleBlips[i];
        DrawBlip(draw, WorldToRadar(local, localMatrix, blip.position, center, radius), blip.color, blip.radius);
    }

    for (int i = 0; i < m_playerBlipCount; ++i) {
        const RadarBlip& blip = m_playerBlips[i];
        DrawBlip(draw, WorldToRadar(local, localMatrix, blip.position, center, radius), blip.color, blip.radius);
    }

    ImVec2 arrow[3] = {
            ImVec2(center.x, center.y - 9.0f * scale),
            ImVec2(center.x - 6.0f * scale, center.y + 7.0f * scale),
            ImVec2(center.x + 6.0f * scale, center.y + 7.0f * scale)
    };
    draw->AddTriangleFilled(arrow[0], arrow[1], arrow[2], IM_COL32(55, 210, 220, 255));
}

void NativeRadarCrosshair::RenderCrosshair(ImDrawList* draw, const ImVec2& displaySize) {
    if (!m_replaceCrosshair || !draw || !pGame || !pNetGame) return;

    CPlayerPed* ped = pGame->FindPlayerPed();
    if (!ped || !ped->m_pPed) return;

    const int camMode = GameGetLocalPlayerCameraMode();
    const int normalized = camMode & 0xFFFD;
    if (!(camMode == 53 || camMode == 39 || normalized == 40)) return;

    const float scale = std::max(0.75f, displaySize.y / 1080.0f);
    float radius = 18.0f * scale;
    if (ped->m_pPed) {
        radius += std::clamp(GTASAEngineApi::GetWeaponRadiusOnScreen(ped->m_pPed), 0.0f, 2.0f) * 11.0f * scale;
    }

    const ImVec2 center(displaySize.x * 0.5f, displaySize.y * 0.5f);
    const ImU32 color = IM_COL32(255, 255, 255, 230);
    const ImU32 shadow = IM_COL32(0, 0, 0, 180);
    const float gap = 7.0f * scale;
    const float len = radius;
    const float thick = 2.2f * scale;

    draw->AddLine(ImVec2(center.x - gap - len + 1.0f, center.y + 1.0f), ImVec2(center.x - gap + 1.0f, center.y + 1.0f), shadow, thick + 1.0f);
    draw->AddLine(ImVec2(center.x + gap + 1.0f, center.y + 1.0f), ImVec2(center.x + gap + len + 1.0f, center.y + 1.0f), shadow, thick + 1.0f);
    draw->AddLine(ImVec2(center.x + 1.0f, center.y - gap - len + 1.0f), ImVec2(center.x + 1.0f, center.y - gap + 1.0f), shadow, thick + 1.0f);
    draw->AddLine(ImVec2(center.x + 1.0f, center.y + gap + 1.0f), ImVec2(center.x + 1.0f, center.y + gap + len + 1.0f), shadow, thick + 1.0f);

    draw->AddLine(ImVec2(center.x - gap - len, center.y), ImVec2(center.x - gap, center.y), color, thick);
    draw->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + len, center.y), color, thick);
    draw->AddLine(ImVec2(center.x, center.y - gap - len), ImVec2(center.x, center.y - gap), color, thick);
    draw->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + len), color, thick);
    draw->AddCircle(center, 2.0f * scale, IM_COL32(65, 220, 230, 240), 12, 1.6f * scale);
}
