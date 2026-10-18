#include "NativeScoreboard.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "main.h"
#include "game/game.h"

extern CGame* pGame;
extern CNetGame* pNetGame;

void NativeScoreboard::SetVisible(bool visible) {
    if (m_visible == visible) return;
    m_visible = visible;
    m_selected = -1;
    m_lastRefresh = 0;
    if (pGame) pGame->EnableGameInput(!visible);
}

void NativeScoreboard::Toggle() {
    SetVisible(!m_visible);
}

void NativeScoreboard::Refresh() {
    m_rowCount = 0;
    if (!pNetGame) return;

    CPlayerPool* pool = pNetGame->GetPlayerPool();
    if (!pool) return;

    pNetGame->UpdatePlayerScoresAndPings();

    Row& local = m_rows[m_rowCount++];
    local.id = pool->GetLocalPlayerID();
    std::strncpy(local.name, pool->GetLocalPlayerName() ? pool->GetLocalPlayerName() : "Player", MAX_PLAYER_NAME);
    local.name[MAX_PLAYER_NAME] = '\0';
    local.score = pool->GetLocalPlayerScore();
    local.ping = pool->GetLocalPlayerPing();
    local.color = 0xFFFFFFFF;

    for (PLAYERID id = 0; id < MAX_PLAYERS && m_rowCount < static_cast<int>(m_rows.size()); ++id) {
        if (!pool->GetSlotState(id)) continue;
        Row& row = m_rows[m_rowCount++];
        row.id = id;
        std::strncpy(row.name, pool->GetPlayerName(id) ? pool->GetPlayerName(id) : "Player", MAX_PLAYER_NAME);
        row.name[MAX_PLAYER_NAME] = '\0';
        row.score = pool->GetPlayerScore(id);
        row.ping = pool->GetPlayerPing(id);
        CRemotePlayer* remote = pool->GetAt(id);
        row.color = remote ? remote->GetPlayerColor() : 0xFFFFFFFF;
    }
}

void NativeScoreboard::DrawText(ImDrawList* draw, const ImVec2& pos, ImU32 color, const char* text, float size) {
    ImFont* font = ImGui::GetFont();
    draw->AddText(font, size, ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 210), text);
    draw->AddText(font, size, pos, color, text);
}

void NativeScoreboard::HandleClick(const ImVec2& min, const ImVec2& max, int visibleRows, float rowH) {
    ImGuiIO& io = ImGui::GetIO();
    const bool pressed = io.MouseDown[0] && !m_prevMouseDown;
    m_prevMouseDown = io.MouseDown[0];
    if (!pressed) return;

    const bool inside =
            io.MousePos.x >= min.x && io.MousePos.x <= max.x &&
            io.MousePos.y >= min.y && io.MousePos.y <= max.y;
    if (!inside) return;

    const float closeSize = 44.0f;
    if (io.MousePos.x >= max.x - closeSize && io.MousePos.y <= min.y + closeSize) {
        SetVisible(false);
        return;
    }

    const float firstRowY = min.y + 92.0f;
    const int row = static_cast<int>((io.MousePos.y - firstRowY) / rowH);
    if (row >= 0 && row < visibleRows && row < m_rowCount) {
        m_selected = row;
    }
}

void NativeScoreboard::Render(ImDrawList* draw, const ImVec2& displaySize) {
    if (!m_visible || !draw) return;

    const uint32_t now = GetTickCount();
    if (m_lastRefresh == 0 || now - m_lastRefresh > 500) {
        Refresh();
        m_lastRefresh = now;
    }

    const float scale = std::max(0.78f, displaySize.y / 1080.0f);
    const ImVec2 size(std::min(displaySize.x * 0.72f, 980.0f * scale), std::min(displaySize.y * 0.72f, 720.0f * scale));
    const ImVec2 min((displaySize.x - size.x) * 0.5f, (displaySize.y - size.y) * 0.5f);
    const ImVec2 max(min.x + size.x, min.y + size.y);

    draw->AddRectFilled(ImVec2(0.0f, 0.0f), displaySize, IM_COL32(0, 0, 0, 82));
    draw->AddRectFilled(min, max, IM_COL32(7, 13, 18, 232), 8.0f * scale);
    draw->AddRect(min, max, IM_COL32(255, 255, 255, 45), 8.0f * scale, 0, 1.0f);

    const float titleSize = 32.0f * scale;
    DrawText(draw, ImVec2(min.x + 24.0f * scale, min.y + 22.0f * scale),
             IM_COL32(255, 255, 255, 255), "Scoreboard", titleSize);

    char total[48]{};
    std::snprintf(total, sizeof(total), "%d jogadores", m_rowCount);
    DrawText(draw, ImVec2(max.x - 220.0f * scale, min.y + 26.0f * scale),
             IM_COL32(130, 235, 235, 255), total, 23.0f * scale);

    draw->AddRectFilled(ImVec2(max.x - 48.0f * scale, min.y + 14.0f * scale),
                        ImVec2(max.x - 14.0f * scale, min.y + 48.0f * scale),
                        IM_COL32(180, 40, 50, 225), 6.0f * scale);
    DrawText(draw, ImVec2(max.x - 39.0f * scale, min.y + 16.0f * scale),
             IM_COL32(255, 255, 255, 255), "x", 26.0f * scale);

    const float headerY = min.y + 74.0f * scale;
    const float rowH = 34.0f * scale;
    DrawText(draw, ImVec2(min.x + 28.0f * scale, headerY), IM_COL32(160, 185, 195, 255), "ID", 20.0f * scale);
    DrawText(draw, ImVec2(min.x + 105.0f * scale, headerY), IM_COL32(160, 185, 195, 255), "Nome", 20.0f * scale);
    DrawText(draw, ImVec2(max.x - 230.0f * scale, headerY), IM_COL32(160, 185, 195, 255), "Score", 20.0f * scale);
    DrawText(draw, ImVec2(max.x - 105.0f * scale, headerY), IM_COL32(160, 185, 195, 255), "Ping", 20.0f * scale);

    const int visibleRows = std::min(m_rowCount, static_cast<int>((size.y - 112.0f * scale) / rowH));
    float y = min.y + 102.0f * scale;
    for (int i = 0; i < visibleRows; ++i) {
        const Row& row = m_rows[i];
        const ImU32 bg = (i == m_selected) ? IM_COL32(35, 120, 128, 175) :
                         (i & 1) ? IM_COL32(255, 255, 255, 14) : IM_COL32(255, 255, 255, 7);
        draw->AddRectFilled(ImVec2(min.x + 18.0f * scale, y - 3.0f * scale),
                            ImVec2(max.x - 18.0f * scale, y + rowH - 5.0f * scale), bg, 5.0f * scale);

        char id[12]{};
        char score[16]{};
        char ping[16]{};
        std::snprintf(id, sizeof(id), "%u", row.id);
        std::snprintf(score, sizeof(score), "%d", row.score);
        std::snprintf(ping, sizeof(ping), "%d", row.ping);

        DrawText(draw, ImVec2(min.x + 28.0f * scale, y), IM_COL32(255, 255, 255, 240), id, 21.0f * scale);
        DrawText(draw, ImVec2(min.x + 105.0f * scale, y), IM_COL32(255, 255, 255, 255), row.name, 21.0f * scale);
        DrawText(draw, ImVec2(max.x - 230.0f * scale, y), IM_COL32(230, 230, 230, 255), score, 21.0f * scale);
        DrawText(draw, ImVec2(max.x - 105.0f * scale, y), IM_COL32(130, 235, 235, 255), ping, 21.0f * scale);
        y += rowH;
    }

    HandleClick(min, max, visibleRows, rowH);
}
