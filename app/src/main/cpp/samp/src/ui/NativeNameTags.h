#pragma once

#include <array>
#include <cstdint>

#include "imgui.h"
#include "game/Core/Vector.h"

class NativeNameTags {
public:
    void Render(ImDrawList* draw, const ImVec2& displaySize);

private:
    static bool WorldToScreen(const CVector& world, ImVec2& screen, float* depth);
    static ImU32 SampColorToImU32(uint32_t color);
    static void DrawBar(ImDrawList* draw, const ImVec2& center, float width, float height,
                        float value, ImU32 fill, ImU32 back);
    static void DrawTextOutlined(ImDrawList* draw, const ImVec2& pos, ImU32 color,
                                 const char* text, float size);
    void RefreshCache(const ImVec2& displaySize, float fontSize);

    struct CachedTag {
        ImVec2 screen{};
        ImU32 color = 0;
        float health = 0.0f;
        float armour = 0.0f;
        float labelWidth = 0.0f;
        bool afk = false;
        char label[64]{};
    };

    static constexpr int kMaxCachedTags = 64;
    static constexpr uint32_t kCacheIntervalMs = 50;

    uint32_t m_lastCacheMs = 0;
    int m_tagCount = 0;
    std::array<CachedTag, kMaxCachedTags> m_tags{};
};
