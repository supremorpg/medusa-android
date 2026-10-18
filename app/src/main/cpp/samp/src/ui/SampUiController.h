#pragma once

#include "Native3DTextLabels.h"
#include "NativeChatBox.h"
#include "NativeNameTags.h"
#include "NativeRadarCrosshair.h"
#include "NativeScoreboard.h"

class SampUiController {
public:
    static bool IsEnabled();
    static void SetEnabled(bool enabled);
    static void Render();

    static bool ShouldSuppressNativeRadar();
    static bool ShouldSuppressNativeCrosshair();
    static void InstallNativePatches();

    static void AddClientMessage(const char* text, ImU32 color);
    static void AddPlayerMessage(const char* nick, const char* text, ImU32 nickColor);
    static void AddInfoMessage(const char* fmt, ...);
    static void AddDebugMessage(const char* fmt, ...);
    static void SetChatInputActive(bool active);
    static void ToggleChatVisible();
    static bool WantsInputCapture();

    static void SetScoreboardVisible(bool visible);
    static void ToggleScoreboard();
    static bool IsScoreboardVisible();

private:
    static NativeChatBox& Chat();
    static Native3DTextLabels& TextLabels();
    static NativeNameTags& NameTags();
    static NativeScoreboard& Scoreboard();
    static NativeRadarCrosshair& RadarCrosshair();

    static bool s_enabled;
};
