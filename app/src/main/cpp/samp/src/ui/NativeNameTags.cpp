#include "NativeNameTags.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "main.h"
#include "game/game.h"
#include "game/GTASAEngineApi.h"
#include "game/World.h"
#include "net/netgame.h"

extern CGame* pGame;
extern CNetGame* pNetGame;

namespace {
constexpr float kHeadOffsetBase = 0.35f;
constexpr float kDistanceLift = 0.043f;
}

bool NativeNameTags::WorldToScreen(const CVector& world, ImVec2& screen, float* depth) {
    CVector out{};
    if (!GTASAEngineApi::WorldToScreen(world, out, depth)) return false;
    screen = ImVec2(out.x, out.y);
    return true;
}

ImU32 NativeNameTags::SampColorToImU32(uint32_t color) {
    const int r = static_cast<int>((color >> 24) & 0xFF);
    const int g = static_cast<int>((color >> 16) & 0xFF);
    const int b = static_cast<int>((color >> 8) & 0xFF);
    return IM_COL32(r, g, b, 255);
}

void NativeNameTags::DrawTextOutlined(ImDrawList* draw, const ImVec2& pos, ImU32 color,
                                      const char* text, float size) {
    ImFont* font = ImGui::GetFont();
    const ImU32 outline = IM_COL32(0, 0, 0, 230);
    draw->AddText(font, size, ImVec2(pos.x + 1.0f, pos.y), outline, text);
    draw->AddText(font, size, ImVec2(pos.x - 1.0f, pos.y), outline, text);
    draw->AddText(font, size, ImVec2(pos.x, pos.y + 1.0f), outline, text);
    draw->AddText(font, size, ImVec2(pos.x, pos.y - 1.0f), outline, text);
    draw->AddText(font, size, pos, color, text);
}

void NativeNameTags::DrawBar(ImDrawList* draw, const ImVec2& center, float width, float height,
                             float value, ImU32 fill, ImU32 back) {
    value = std::clamp(value, 0.0f, 100.0f);
    const ImVec2 borderMin(center.x - width * 0.5f - 1.0f, center.y - 1.0f);
    const ImVec2 borderMax(center.x + width * 0.5f + 1.0f, center.y + height + 1.0f);
    const ImVec2 bgMin(center.x - width * 0.5f, center.y);
    const ImVec2 bgMax(center.x + width * 0.5f, center.y + height);
    const ImVec2 fillMax(bgMin.x + width * (value / 100.0f), bgMax.y);

    draw->AddRectFilled(borderMin, borderMax, IM_COL32(0, 0, 0, 255), 2.0f);
    draw->AddRectFilled(bgMin, bgMax, back, 1.5f);
    draw->AddRectFilled(bgMin, fillMax, fill, 1.5f);
}

void NativeNameTags::RefreshCache(const ImVec2& displaySize, float fontSize) {
    m_tagCount = 0;
    if (!pNetGame || !pGame || !pNetGame->m_pNetSet || !pNetGame->m_pNetSet->bShowNameTags) return;

    CPlayerPool* pool = pNetGame->GetPlayerPool();
    CPlayerPed* local = pGame->FindPlayerPed();
    if (!pool || !local || !local->m_pPed) return;

    CCamera& camera = GTASAEngineApi::Camera();
    for (PLAYERID playerId = 0; playerId < MAX_PLAYERS && m_tagCount < kMaxCachedTags; ++playerId) {
        if (!pool->GetSlotState(playerId)) continue;

        CRemotePlayer* remote = pool->GetAt(playerId);
        if (!remote || !remote->IsActive() || !remote->m_bShowNameTag || remote->IsNPC()) continue;

        CPlayerPed* ped = remote->GetPlayerPed();
        if (!ped || !ped->m_pPed || !ped->m_pPed->IsAdded()) continue;

        const float distance = ped->m_pPed->GetDistanceFromCamera();
        if (distance > pNetGame->m_pNetSet->fNameTagDrawDistance) continue;

        CVector head{};
        ped->GetBonePosition(8, &head);
        if (pNetGame->m_pNetSet->bNameTagLOS) {
            const int clear = CWorld::GetIsLineOfSightClear(head, camera.GetPosition(),
                                                            true, false, false, true, false, false, false);
            if (!clear) continue;
        }

        head.z += kHeadOffsetBase + distance * kDistanceLift;

        ImVec2 screen{};
        float depth = 0.0f;
        if (!WorldToScreen(head, screen, &depth)) continue;
        if (screen.x < -200.0f || screen.x > displaySize.x + 200.0f ||
            screen.y < -120.0f || screen.y > displaySize.y + 120.0f) {
            continue;
        }

        CachedTag& tag = m_tags[m_tagCount++];
        tag.screen = screen;
        tag.color = SampColorToImU32(remote->GetPlayerColor());
        tag.health = remote->m_fReportedHealth;
        tag.armour = remote->m_fReportedArmour;
        tag.afk = remote->m_bIsAFK;
        std::snprintf(tag.label, sizeof(tag.label), "%s (%u)", pool->GetPlayerName(playerId), playerId);
        tag.labelWidth = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, tag.label).x;
    }
}

void NativeNameTags::Render(ImDrawList* draw, const ImVec2& displaySize) {
    if (!draw || !pNetGame || !pGame || !pNetGame->m_pNetSet || !pNetGame->m_pNetSet->bShowNameTags) return;

    const float scale = std::max(0.75f, displaySize.y / 1080.0f);
    const float fontSize = 22.0f * scale;
    const uint32_t now = GetTickCount();
    if (m_lastCacheMs == 0 || now - m_lastCacheMs >= kCacheIntervalMs) {
        RefreshCache(displaySize, fontSize);
        m_lastCacheMs = now;
    }

    const float barW = 72.0f * scale;
    const float barH = 5.0f * scale;
    for (int i = 0; i < m_tagCount; ++i) {
        const CachedTag& tag = m_tags[i];
        DrawTextOutlined(draw, ImVec2(tag.screen.x - tag.labelWidth * 0.5f, tag.screen.y), tag.color, tag.label, fontSize);

        const float barsY = tag.screen.y + fontSize + 5.0f * scale;
        if (tag.armour > 0.0f) {
            DrawBar(draw, ImVec2(tag.screen.x, barsY), barW, barH, tag.armour,
                    IM_COL32(218, 218, 218, 255), IM_COL32(48, 48, 48, 255));
            DrawBar(draw, ImVec2(tag.screen.x, barsY + barH + 3.0f * scale), barW, barH, tag.health,
                    IM_COL32(190, 34, 40, 255), IM_COL32(76, 11, 20, 255));
        } else {
            DrawBar(draw, ImVec2(tag.screen.x, barsY), barW, barH, tag.health,
                    IM_COL32(190, 34, 40, 255), IM_COL32(76, 11, 20, 255));
        }

        if (tag.afk) {
            DrawTextOutlined(draw, ImVec2(tag.screen.x + barW * 0.5f + 7.0f * scale, barsY - 4.0f * scale),
                             IM_COL32(255, 215, 80, 255), "AFK", fontSize * 0.75f);
        }
    }
}
