#include "NativeChatBox.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "settings.h"

extern CSettings* pSettings;

namespace {
constexpr ImU32 kInputBackground = IM_COL32(10, 140, 150, 178);
constexpr ImU32 kTextShadow = IM_COL32(0, 0, 0, 210);
constexpr float kMinChatWidth = 80.0f;
constexpr float kMinChatHeight = 32.0f;
constexpr int kFallbackVisibleMessages = 9;

void DrawTextShadowed(ImDrawList* draw, const ImVec2& pos, ImU32 color, const char* text,
                      float size, float wrapWidth) {
    ImFont* font = ImGui::GetFont();
    draw->AddText(font, size, ImVec2(pos.x + 1.0f, pos.y + 1.0f), kTextShadow, text, nullptr, wrapWidth);
    draw->AddText(font, size, pos, color, text, nullptr, wrapWidth);
}

float WrappedTextHeight(const char* text, float fontSize, float wrapWidth) {
    ImFont* font = ImGui::GetFont();
    const ImVec2 size = font->CalcTextSizeA(fontSize, wrapWidth, wrapWidth, text ? text : "");
    return std::max(fontSize, size.y);
}
}

void NativeChatBox::AddClientMessage(const char* text, ImU32 color) {
    AddLine(text, color);
}

void NativeChatBox::AddPlayerMessage(const char* nick, const char* text, ImU32 nickColor) {
    char line[kMaxText]{};
    std::snprintf(line, sizeof(line), "%s: %s", nick ? nick : "", text ? text : "");
    AddLine(line, nickColor);
}

void NativeChatBox::AddInfoMessage(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AddFormatted(IM_COL32(0, 210, 210, 255), fmt, args);
    va_end(args);
}

void NativeChatBox::AddDebugMessage(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AddFormatted(IM_COL32(195, 195, 195, 255), fmt, args);
    va_end(args);
}

void NativeChatBox::AddLine(const char* text, ImU32 color) {
    int slot = 0;
    if (m_count == kMaxLines) {
        slot = m_start;
        m_start = (m_start + 1) % kMaxLines;
    } else {
        slot = (m_start + m_count) % kMaxLines;
        ++m_count;
    }

    CopyText(m_lines[slot].text, kMaxText, text ? text : "");
    m_lines[slot].color = color;
    m_scrollOffsetLines = 0;
}

void NativeChatBox::AddFormatted(ImU32 color, const char* fmt, va_list args) {
    char line[kMaxText]{};
    std::vsnprintf(line, sizeof(line), fmt ? fmt : "", args);
    AddLine(line, color);
}

int NativeChatBox::PhysicalIndex(int logicalIndex) const {
    return (m_start + logicalIndex) % kMaxLines;
}

void NativeChatBox::CopyText(char* dst, int dstSize, const char* src) {
    if (!dst || dstSize <= 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    std::strncpy(dst, src, static_cast<size_t>(dstSize - 1));
    dst[dstSize - 1] = '\0';
}

void NativeChatBox::HandleScrollInput(const ImVec2& origin, const ImVec2& size) {
    ImGuiIO& io = ImGui::GetIO();
    const bool inside =
            io.MousePos.x >= origin.x && io.MousePos.x <= origin.x + size.x &&
            io.MousePos.y >= origin.y && io.MousePos.y <= origin.y + size.y;

    if (io.MouseDown[0] && inside && !m_dragging) {
        m_dragging = true;
        m_lastDragY = io.MousePos.y;
    } else if (!io.MouseDown[0]) {
        m_dragging = false;
    }

    if (!m_dragging) return;

    const float delta = io.MousePos.y - m_lastDragY;
    if (std::fabs(delta) < 18.0f) return;

    const int direction = delta > 0.0f ? 1 : -1;
    const int maxScroll = std::max(0, m_count - 1);
    m_scrollOffsetLines = std::clamp(m_scrollOffsetLines + direction, 0, maxScroll);
    m_lastDragY = io.MousePos.y;
}

void NativeChatBox::ResolveLayout(const ImVec2& displaySize, ImVec2& origin, ImVec2& size,
                                  float& fontSize, int& maxMessages) const {
    const float scale = std::max(0.75f, displaySize.y / 1080.0f);
    fontSize = 23.0f * scale;
    maxMessages = kFallbackVisibleMessages;

    origin = ImVec2(24.0f * scale, 56.0f * scale);
    size = ImVec2(std::min(displaySize.x * 0.48f, 760.0f * scale),
                  (fontSize + 5.0f * scale) * kFallbackVisibleMessages + 18.0f * scale);

    if (!pSettings) {
        return;
    }

    const stSettings& settings = pSettings->Get();
    maxMessages = std::clamp(settings.iChatMaxMessages, 1, 30);
    if (settings.fChatSizeX <= 0.0f || settings.fChatSizeY <= 0.0f) {
        return;
    }

    const float width = std::clamp(settings.fChatSizeX, kMinChatWidth, displaySize.x);
    const float height = std::clamp(settings.fChatSizeY, kMinChatHeight, displaySize.y);
    const float x = std::clamp(settings.fChatPosX, 0.0f, std::max(0.0f, displaySize.x - width));
    const float y = std::clamp(settings.fChatPosY, 0.0f, std::max(0.0f, displaySize.y - height));

    origin = ImVec2(x, y);
    size = ImVec2(width, height);
}

void NativeChatBox::Render(ImDrawList* draw, const ImVec2& displaySize) {
    if (!m_visible || !draw) return;

    ImVec2 origin{};
    ImVec2 size{};
    float fontSize = 0.0f;
    int maxMessages = 0;
    ResolveLayout(displaySize, origin, size, fontSize, maxMessages);

    const float scale = std::max(0.75f, displaySize.y / 1080.0f);
    const float paddingX = 12.0f * scale;
    const float paddingY = 8.0f * scale;
    const float messageGap = 3.0f * scale;
    const float wrapWidth = std::max(16.0f, size.x - paddingX * 2.0f);
    const float contentMinY = origin.y + paddingY;
    const float contentMaxY = origin.y + size.y - paddingY;

    HandleScrollInput(origin, size);

    draw->PushClipRect(ImVec2(origin.x, origin.y), ImVec2(origin.x + size.x, origin.y + size.y), true);

    const int newestLogical = m_count - 1 - m_scrollOffsetLines;
    int drawnMessages = 0;
    float y = contentMaxY;

    for (int logical = newestLogical; logical >= 0 && drawnMessages < maxMessages; --logical) {
        const Line& line = m_lines[PhysicalIndex(logical)];
        const float textHeight = WrappedTextHeight(line.text, fontSize, wrapWidth);
        const float drawY = std::max(contentMinY, y - textHeight);
        ImVec2 pos(origin.x + paddingX, drawY);
        DrawTextShadowed(draw, pos, line.color, line.text, fontSize, wrapWidth);

        y = drawY - messageGap;
        ++drawnMessages;
        if (y <= contentMinY) {
            break;
        }
    }

    draw->PopClipRect();

    if (m_inputActive) {
        const ImVec2 inputMin(origin.x, origin.y + size.y + 8.0f * scale);
        const ImVec2 inputMax(origin.x + size.x, inputMin.y + 38.0f * scale);
        draw->AddRectFilled(inputMin, inputMax, kInputBackground, 6.0f * scale);
        DrawTextShadowed(draw, ImVec2(inputMin.x + 12.0f * scale, inputMin.y + 7.0f * scale),
                         IM_COL32(255, 255, 255, 255), "Digite no teclado nativo...", fontSize * 0.9f,
                         std::max(16.0f, size.x - 24.0f * scale));
    }
}
