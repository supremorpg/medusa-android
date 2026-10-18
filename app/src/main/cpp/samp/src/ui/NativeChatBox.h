#pragma once

#include <array>
#include <cstdarg>
#include <cstdint>

#include "imgui.h"

class NativeChatBox {
public:
    static constexpr int kMaxLines = 128;
    static constexpr int kMaxText = 192;

    void AddClientMessage(const char* text, ImU32 color);
    void AddPlayerMessage(const char* nick, const char* text, ImU32 nickColor);
    void AddInfoMessage(const char* fmt, ...);
    void AddDebugMessage(const char* fmt, ...);

    void SetVisible(bool visible) { m_visible = visible; }
    bool IsVisible() const { return m_visible; }
    void ToggleVisible() { m_visible = !m_visible; }

    void SetInputActive(bool active) { m_inputActive = active; }
    bool IsInputActive() const { return m_inputActive; }

    void Render(ImDrawList* draw, const ImVec2& displaySize);

private:
    struct Line {
        char text[kMaxText]{};
        ImU32 color{};
    };

    void AddLine(const char* text, ImU32 color);
    void AddFormatted(ImU32 color, const char* fmt, va_list args);
    int PhysicalIndex(int logicalIndex) const;
    void HandleScrollInput(const ImVec2& origin, const ImVec2& size);
    void ResolveLayout(const ImVec2& displaySize, ImVec2& origin, ImVec2& size,
                       float& fontSize, int& maxMessages) const;
    static void CopyText(char* dst, int dstSize, const char* src);

    std::array<Line, kMaxLines> m_lines{};
    int m_start = 0;
    int m_count = 0;
    int m_scrollOffsetLines = 0;
    bool m_visible = true;
    bool m_inputActive = false;
    bool m_dragging = false;
    float m_lastDragY = 0.0f;
};
