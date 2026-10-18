#pragma once

#include <array>
#include <cstdint>

#include "imgui.h"
#include "game/game.h"
#include "net/netgame.h"

class NativeScoreboard {
public:
    void SetVisible(bool visible);
    void Toggle();
    bool IsVisible() const { return m_visible; }
    void Render(ImDrawList* draw, const ImVec2& displaySize);

private:
    struct Row {
        PLAYERID id = INVALID_PLAYER_ID;
        char name[MAX_PLAYER_NAME + 1]{};
        int score = 0;
        int ping = 0;
        uint32_t color = 0xFFFFFFFF;
    };

    void Refresh();
    void DrawText(ImDrawList* draw, const ImVec2& pos, ImU32 color, const char* text, float size);
    void HandleClick(const ImVec2& min, const ImVec2& max, int rowCount, float rowH);

    std::array<Row, MAX_PLAYERS + 1> m_rows{};
    int m_rowCount = 0;
    int m_selected = -1;
    uint32_t m_lastRefresh = 0;
    bool m_visible = false;
    bool m_prevMouseDown = false;
};
