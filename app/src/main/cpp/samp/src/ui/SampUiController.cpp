#include "SampUiController.h"

#include <cstdarg>
#include <cstdio>

#include "main.h"

bool SampUiController::s_enabled = true;

NativeChatBox& SampUiController::Chat() {
    static NativeChatBox chat;
    return chat;
}

Native3DTextLabels& SampUiController::TextLabels() {
    static Native3DTextLabels labels;
    return labels;
}

NativeNameTags& SampUiController::NameTags() {
    static NativeNameTags tags;
    return tags;
}

NativeScoreboard& SampUiController::Scoreboard() {
    static NativeScoreboard scoreboard;
    return scoreboard;
}

NativeRadarCrosshair& SampUiController::RadarCrosshair() {
    static NativeRadarCrosshair radarCrosshair;
    return radarCrosshair;
}

bool SampUiController::IsEnabled() {
    return s_enabled;
}

void SampUiController::SetEnabled(bool enabled) {
    s_enabled = enabled;
}

void SampUiController::Render() {
    if (!s_enabled) return;
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    RadarCrosshair().RenderRadar(draw, displaySize);
    TextLabels().Render(draw, displaySize);
    NameTags().Render(draw, displaySize);
    Chat().Render(draw, displaySize);
    RadarCrosshair().RenderCrosshair(draw, displaySize);
    Scoreboard().Render(ImGui::GetForegroundDrawList(), displaySize);
}

bool SampUiController::ShouldSuppressNativeRadar() {
    return s_enabled && RadarCrosshair().SuppressNativeRadar();
}

bool SampUiController::ShouldSuppressNativeCrosshair() {
    return s_enabled && RadarCrosshair().SuppressNativeCrosshair();
}

void SampUiController::InstallNativePatches() {
    FLog("[SAMP_UI] Native UI active: Render2dStuff shared ImGui pipeline, radar/crosshair native draws suppressed by hooks.");
}

void SampUiController::AddClientMessage(const char* text, ImU32 color) {
    Chat().AddClientMessage(text, color);
}

void SampUiController::AddPlayerMessage(const char* nick, const char* text, ImU32 nickColor) {
    Chat().AddPlayerMessage(nick, text, nickColor);
}

void SampUiController::AddInfoMessage(const char* fmt, ...) {
    char line[NativeChatBox::kMaxText]{};
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(line, sizeof(line), fmt ? fmt : "", args);
    va_end(args);
    Chat().AddClientMessage(line, IM_COL32(0, 210, 210, 255));
}

void SampUiController::AddDebugMessage(const char* fmt, ...) {
    char line[NativeChatBox::kMaxText]{};
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(line, sizeof(line), fmt ? fmt : "", args);
    va_end(args);
    Chat().AddClientMessage(line, IM_COL32(195, 195, 195, 255));
}

void SampUiController::SetChatInputActive(bool active) {
    Chat().SetInputActive(active);
}

void SampUiController::ToggleChatVisible() {
    Chat().ToggleVisible();
}

bool SampUiController::WantsInputCapture() {
    if (!s_enabled) return false;

    return Chat().IsInputActive() ||
           Scoreboard().IsVisible();
}

void SampUiController::SetScoreboardVisible(bool visible) {
    Scoreboard().SetVisible(visible);
}

void SampUiController::ToggleScoreboard() {
    Scoreboard().Toggle();
}

bool SampUiController::IsScoreboardVisible() {
    return Scoreboard().IsVisible();
}
