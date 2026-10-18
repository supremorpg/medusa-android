#include "../main.h"
#include "../settings.h"

#include <algorithm>
#include <cmath>

#include "Plugin.h"

#include "include/util/Timer.h"

#include "Record.h"
#include "Playback.h"
#include "Network.h"
//#include "BlackList.h"
#include "PluginConfig.h"
#include "MicroIcon.h"
#include "SpeakerList.h"
//#include "PluginMenu.h"
#include "GlobalStream.h"
#include "StreamAtPoint.h"
#include "StreamAtVehicle.h"
#include "StreamAtPlayer.h"
#include "StreamAtObject.h"

#include "../audiostream.h"
extern CAudioStream* pAudioStream;

extern CSettings* pSettings;

namespace {
    constexpr uint8_t kVoiceActivationKey = 0x42;
    constexpr bool kEnableImGuiVoiceUi = true;
    constexpr float kVoiceLayoutDesignWidth = 1920.0f;
    constexpr float kVoiceLayoutDesignHeight = 1080.0f;
    constexpr float kVoiceDefaultSettingSize = 30.0f;
    bool g_hasRuntimeVoiceLayout = false;
    float g_runtimeVoicePosX = 0.0f;
    float g_runtimeVoicePosY = 0.0f;
    float g_runtimeVoiceSizeScale = 1.0f;

    float ClampVoiceFloat(float value, float minValue, float maxValue)
    {
        if (!std::isfinite(value))
        {
            return minValue;
        }
        return std::max(minValue, std::min(value, maxValue));
    }

    ImVec2 ResolveVoiceButtonPosition(const ImGuiIO& io, const ImVec2& buttonSize)
    {
        ImVec2 fallback(io.DisplaySize.x / 1.3f - buttonSize.x, io.DisplaySize.y / 1.5f - buttonSize.y * 3.0f);
        float posX = fallback.x;
        float posY = fallback.y;

        if (g_hasRuntimeVoiceLayout)
        {
            posX = g_runtimeVoicePosX * std::max(0.0f, io.DisplaySize.x - buttonSize.x);
            posY = g_runtimeVoicePosY * std::max(0.0f, io.DisplaySize.y - buttonSize.y);
        }
        else if (pSettings)
        {
            const float settingX = pSettings->Get().fVoiceChatPosX;
            const float settingY = pSettings->Get().fVoiceChatPosY;
            if (std::isfinite(settingX) && std::isfinite(settingY))
            {
                if (settingX >= 0.0f && settingX <= 1.0f && settingY >= 0.0f && settingY <= 1.0f)
                {
                    posX = settingX * std::max(0.0f, io.DisplaySize.x - buttonSize.x);
                    posY = settingY * std::max(0.0f, io.DisplaySize.y - buttonSize.y);
                }
                else
                {
                    posX = settingX * (io.DisplaySize.x / kVoiceLayoutDesignWidth);
                    posY = settingY * (io.DisplaySize.y / kVoiceLayoutDesignHeight);
                }
            }
        }

        posX = ClampVoiceFloat(posX, 0.0f, std::max(0.0f, io.DisplaySize.x - buttonSize.x));
        posY = ClampVoiceFloat(posY, 0.0f, std::max(0.0f, io.DisplaySize.y - buttonSize.y));
        return ImVec2(posX, posY);
    }

    float ResolveVoiceButtonSizeScale()
    {
        if (g_hasRuntimeVoiceLayout)
        {
            return ClampVoiceFloat(g_runtimeVoiceSizeScale, 0.65f, 1.85f);
        }
        if (!pSettings)
        {
            return 1.0f;
        }
        return ClampVoiceFloat(pSettings->Get().fVoiceChatSize / kVoiceDefaultSettingSize, 0.65f, 1.85f);
    }

    void DrawFallbackSampVoiceButton(const ImVec2& size, bool active, bool muted)
    {
        ImGui::InvisibleButton("##SampVoiceButtonFallback", size);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        const float radius = ImMin(size.x, size.y) * 0.42f;

        const ImU32 bgColor = muted
            ? IM_COL32(255, 255, 255, 58)
            : (active ? IM_COL32(255, 255, 255, 104) : IM_COL32(255, 255, 255, 72));
        const ImU32 micColor = muted
            ? IM_COL32(230, 234, 240, 205)
            : IM_COL32(255, 255, 255, 240);

        drawList->AddCircleFilled(center, radius, bgColor, 32);
        drawList->AddCircle(center, radius, IM_COL32(255, 255, 255, active ? 210 : 155), 32, 2.0f);

        const float micW = radius * 0.34f;
        const float micH = radius * 0.58f;
        const ImVec2 micMin(center.x - micW * 0.5f, center.y - micH * 0.62f);
        const ImVec2 micMax(center.x + micW * 0.5f, center.y + micH * 0.18f);
        drawList->AddRectFilled(micMin, micMax, micColor, micW * 0.45f);
        drawList->AddLine(ImVec2(center.x - micW * 0.82f, center.y + micH * 0.02f),
                          ImVec2(center.x - micW * 0.82f, center.y + micH * 0.30f), micColor, 3.0f);
        drawList->AddLine(ImVec2(center.x + micW * 0.82f, center.y + micH * 0.02f),
                          ImVec2(center.x + micW * 0.82f, center.y + micH * 0.30f), micColor, 3.0f);
        drawList->AddLine(ImVec2(center.x - micW * 0.82f, center.y + micH * 0.30f),
                          ImVec2(center.x + micW * 0.82f, center.y + micH * 0.30f), micColor, 3.0f);
        drawList->AddLine(ImVec2(center.x, center.y + micH * 0.30f),
                          ImVec2(center.x, center.y + micH * 0.58f), micColor, 3.0f);
        drawList->AddLine(ImVec2(center.x - micW * 0.70f, center.y + micH * 0.58f),
                          ImVec2(center.x + micW * 0.70f, center.y + micH * 0.58f), micColor, 3.0f);
    }
}

bool Plugin::OnPluginLoad() noexcept
{
    if(!Render::Init())
    {
        LogVoice("[sv:err:plugin] : failed to init render module");
        return false;
    }

    Render::AddDeviceInitCallback(Plugin::OnDeviceInit);
    Render::AddRenderCallback(Plugin::OnRender);
    Render::AddDeviceFreeCallback(Plugin::OnDeviceFree);

    return true;
}

bool Plugin::OnSampLoad() noexcept
{
    if(!Samp::Init())
    {
        LogVoice("[sv:err:plugin] : failed to init samp");
        Render::Free();
        return false;
    }

    Samp::AddLoadCallback(Plugin::OnInitGame);
    Samp::AddExitCallback(Plugin::OnExitGame);

    if(!Network::Init())
    {
        LogVoice("[sv:err:plugin] : failed to init network");
        Render::Free();
        Samp::Free();
        return false;
    }

    Network::AddConnectCallback(Plugin::ConnectHandler);
    Network::AddSvConnectCallback(Plugin::PluginConnectHandler);
    Network::AddSvInitCallback(Plugin::PluginInitHandler);
    Network::AddDisconnectCallback(Plugin::DisconnectHandler);

    if(!Playback::Init())
    {
        LogVoice("[sv:err:plugin] : failed to init playback");
        Render::Free();
        Samp::Free();
        Network::Free();
        return false;
    }

    return true;
}

void Plugin::OnInitGame() noexcept
{
    // ~ none
}

void Plugin::OnExitGame() noexcept
{
    Network::Free();

    Plugin::streamTable.clear();

    Record::Free();
    Playback::Free();
}

void Plugin::SetInputRecordStatus(bool enabled) noexcept
{
    if(!Samp::IsLoaded()) return;

    if(Plugin::muteStatus || !PluginConfig::GetMicroEnable())
    {
        enabled = false;
    }

    if(enabled)
    {
        if(pAudioStream)
        {
            pAudioStream->Stop(true);
        }

        if(!Plugin::recordBusy && !Plugin::recordStatus)
        {
            Plugin::recordStatus = true;
            Record::StartRecording();
        }

        SV::PressKeyPacket pressKeyPacket{};
        pressKeyPacket.keyId = kVoiceActivationKey;
        if(!Network::SendControlPacket(SV::ControlPacketType::pressKey, &pressKeyPacket, sizeof(pressKeyPacket)))
            LogVoice("[sv:err:plugin:setinputrecord] : failed to send PressKey packet");
        return;
    }

    if(!Plugin::recordBusy && Plugin::recordStatus)
    {
        Plugin::recordStatus = false;
    }

    SV::ReleaseKeyPacket releaseKeyPacket{};
    releaseKeyPacket.keyId = kVoiceActivationKey;
    if(!Network::SendControlPacket(SV::ControlPacketType::releaseKey, &releaseKeyPacket, sizeof(releaseKeyPacket)))
        LogVoice("[sv:err:plugin:setinputrecord] : failed to send ReleaseKey packet");
}

void Plugin::SetVoiceButtonLayout(float posX, float posY, float sizeScale) noexcept
{
    g_hasRuntimeVoiceLayout = true;
    g_runtimeVoicePosX = ClampVoiceFloat(posX, 0.0f, 1.0f);
    g_runtimeVoicePosY = ClampVoiceFloat(posY, 0.0f, 1.0f);
    g_runtimeVoiceSizeScale = ClampVoiceFloat(sizeScale, 0.65f, 1.85f);

    if (pSettings)
    {
        pSettings->Get().fVoiceChatPosX = g_runtimeVoicePosX;
        pSettings->Get().fVoiceChatPosY = g_runtimeVoicePosY;
        pSettings->Get().fVoiceChatSize = kVoiceDefaultSettingSize * g_runtimeVoiceSizeScale;
    }
}

void Plugin::ResetVoiceButtonLayout() noexcept
{
    g_hasRuntimeVoiceLayout = false;
    g_runtimeVoicePosX = 0.0f;
    g_runtimeVoicePosY = 0.0f;
    g_runtimeVoiceSizeScale = 1.0f;

    if (pSettings)
    {
        pSettings->Get().fVoiceChatPosX = 1520.0f;
        pSettings->Get().fVoiceChatPosY = 480.0f;
        pSettings->Get().fVoiceChatSize = kVoiceDefaultSettingSize;
    }
}

void Plugin::MainLoop()
{
    if(!Samp::IsLoaded()) return;

    while(const auto controlPacket = Network::ReceiveControlPacket())
    {
        Plugin::ControlPacketHandler(*&*controlPacket);
    }

    while(const auto voicePacket = Network::ReceiveVoicePacket())
    {
        const auto& voicePacketRef = *voicePacket;

        const auto iter = Plugin::streamTable.find(voicePacketRef->stream);
        if(iter == Plugin::streamTable.end()) continue;

        iter->second->Push(*&voicePacketRef);
    }
    
    for(const auto& stream : Plugin::streamTable)
        stream.second->Tick();

    Playback::Tick();
    Record::Tick();

    static bool imguiVoiceHeld = false;
    auto setImguiVoiceHeld = [&](bool held)
    {
        if (held && pSettings && !pSettings->Get().bVoiceChatEnable)
        {
            held = false;
        }
        if (imguiVoiceHeld == held)
        {
            return;
        }
        imguiVoiceHeld = held;
        Plugin::SetInputRecordStatus(held);
    };

    const bool voiceChatEnabled = !pSettings || pSettings->Get().bVoiceChatEnable;
    if(kEnableImGuiVoiceUi && voiceChatEnabled && MicroIcon::IsShowed())
    {
        ImGuiIO& io = ImGui::GetIO();

        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(0x00, 0x00, 0x00, 0x00).Value);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(0x00, 0x00, 0x00, 0x00).Value);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(0x00, 0x00, 0x00, 0x00).Value);

        ImGuiStyle style;
        style.FrameBorderSize = ImGui::GetStyle().FrameBorderSize;
        ImGui::GetStyle().FrameBorderSize = 0.0f;

        const float voiceSizeScale = ResolveVoiceButtonSizeScale();
        ImVec2 vecButSize = ImVec2(
            (MicroIcon::kBaseIconSize * 4 + 3.0f) * voiceSizeScale,
            (MicroIcon::kBaseIconSize * 4) * voiceSizeScale);
        ImVec2 vecWindowPos = ResolveVoiceButtonPosition(io, vecButSize);

        ImGui::SetNextWindowPos(vecWindowPos);
        ImGui::Begin("SampVoiceButton", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

        if (!Plugin::muteStatus)
        {
            bool wantsRecording = false;
            DrawFallbackSampVoiceButton(vecButSize, Plugin::recordStatus, false);
            wantsRecording = ImGui::IsItemActive() && PluginConfig::GetMicroEnable();
            setImguiVoiceHeld(wantsRecording);
        }
        else
        {
            setImguiVoiceHeld(false);
            DrawFallbackSampVoiceButton(vecButSize, false, true);
        }

        ImGui::SetWindowSize(ImVec2(-1, -1));
        ImGui::End();

        ImGui::PopStyleColor(3);
        ImGui::GetStyle().FrameBorderSize = style.FrameBorderSize;
    }
    else if (imguiVoiceHeld)
    {
        setImguiVoiceHeld(false);
    }

    uint8_t frameBuffer[Network::kMaxVoiceDataSize];
    if(const auto frameSize = Record::GetFrame(frameBuffer, sizeof(frameBuffer)))
    {
        if(!Network::SendVoicePacket(frameBuffer, frameSize))
            LogVoice("[sv:err:plugin] : failed to send voice packet");

        if(!Plugin::recordStatus)
        {
            Record::StopRecording();
            Network::EndSequence();
        }
    }
}

void Plugin::ConnectHandler(const std::string& serverIp, const uint16_t serverPort)
{
    // ~ none
}

void Plugin::PluginConnectHandler(SV::ConnectPacket& connectStruct)
{
    connectStruct.signature = SV::kSignature;
    connectStruct.version = SV::kVersion;
    connectStruct.micro = Record::HasMicro();
}

bool Plugin::PluginInitHandler(const SV::PluginInitPacket& initPacket)
{
    Plugin::muteStatus = initPacket.mute;

    if(!Record::Init(initPacket.bitrate))
    {
        LogVoice("[sv:inf:plugin:packet:init] : failed init record");
    }

    return true;
}

void Plugin::ControlPacketHandler(const ControlPacket& controlPacket)
{
    switch(controlPacket.packet)
    {
        case SV::ControlPacketType::muteEnable:
        {
            if(controlPacket.length != 0) break;

            LogVoice("[sv:dbg:plugin:muteenable]");

            Plugin::muteStatus = true;
            Plugin::recordStatus = false;
            Plugin::recordBusy = false;
        } 
        break;
        case SV::ControlPacketType::muteDisable:
        {
            if(controlPacket.length != 0) break;

            LogVoice("[sv:dbg:plugin:mutedisable]");

            Plugin::muteStatus = false;
        } 
        break;
        case SV::ControlPacketType::startRecord:
        {
            if(controlPacket.length != 0) break;

            LogVoice("[sv:dbg:plugin:startrecord]");

            if(Plugin::muteStatus) break;

            Plugin::recordBusy = true;
            Plugin::recordStatus = true;

            Record::StartRecording();
        } 
        break;
        case SV::ControlPacketType::stopRecord:
        {
            if(controlPacket.length != 0) break;

            LogVoice("[sv:dbg:plugin:stoprecord]");

            if(Plugin::muteStatus) break;

            Plugin::recordStatus = false;
            Plugin::recordBusy = false;
        } 
        break;
        case SV::ControlPacketType::addKey:
        {
            const auto& stData = *reinterpret_cast<const SV::AddKeyPacket*>(controlPacket.data);
            if(controlPacket.length != sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:addkey] : keyid(0x%hhx)", stData.keyId);
            LogVoice("[dbg:keyfilter] : adding key (0x%hhx)", stData.keyId); // xd fake

            //KeyFilter::AddKey(stData.keyId);
        } 
        break;
        case SV::ControlPacketType::removeKey:
        {
            const auto& stData = *reinterpret_cast<const SV::RemoveKeyPacket*>(controlPacket.data);
            if(controlPacket.length != sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:removekey] : keyid(0x%hhx)", stData.keyId);
            LogVoice("[dbg:keyfilter] : removing key (0x%hhx)", stData.keyId); // xd fake

            //KeyFilter::RemoveKey(stData.keyId);
        } 
        break;
        case SV::ControlPacketType::removeAllKeys:
        {
            if(controlPacket.length) break;

            LogVoice("[sv:dbg:plugin:removeallkeys]");
            LogVoice("[dbg:keyfilter] : removing all keys"); // xd fake

            //KeyFilter::RemoveAllKeys();
        } 
        break;
        case SV::ControlPacketType::createGStream:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateGStreamPacket*>(controlPacket.data);
            if(controlPacket.length < sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:creategstream] : stream(%p), color(0x%x), name(%s)",
                stData.stream, stData.color, stData.color ? stData.name : "");

            const auto& streamPtr = Plugin::streamTable[stData.stream] =
                MakeGlobalStream(stData.color, stData.name);

            streamPtr->AddPlayCallback(SpeakerList::OnSpeakerPlay);
            streamPtr->AddStopCallback(SpeakerList::OnSpeakerStop);
        } 
        break;
        case SV::ControlPacketType::createLPStream:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateLPStreamPacket*>(controlPacket.data);
            if(controlPacket.length < sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:createlpstream] : "
                "stream(%p), dist(%.2f), pos(%.2f;%.2f;%.2f), color(0x%x), name(%s)",
                stData.stream, stData.distance, stData.position.x, stData.position.y, stData.position.z,
                stData.color, stData.color ? stData.name : "");

            const auto& streamPtr = Plugin::streamTable[stData.stream] =
                MakeStreamAtPoint(stData.color, stData.name, stData.distance, stData.position);

            streamPtr->AddPlayCallback(SpeakerList::OnSpeakerPlay);
            streamPtr->AddStopCallback(SpeakerList::OnSpeakerStop);
        } 
        break;
        case SV::ControlPacketType::createLStreamAtVehicle:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateLStreamAtPacket*>(controlPacket.data);
            if(controlPacket.length < sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:createlstreamatvehicle] : "
                "stream(%p), dist(%.2f), vehicle(%hu), color(0x%x), name(%s)",
                stData.stream, stData.distance, stData.target,
                stData.color, stData.color ? stData.name : "");

            const auto& streamPtr = Plugin::streamTable[stData.stream] =
                MakeStreamAtVehicle(stData.color, stData.name, stData.distance, stData.target);

            streamPtr->AddPlayCallback(SpeakerList::OnSpeakerPlay);
            streamPtr->AddStopCallback(SpeakerList::OnSpeakerStop);
        } 
        break;
        case SV::ControlPacketType::createLStreamAtPlayer:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateLStreamAtPacket*>(controlPacket.data);
            if(controlPacket.length < sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:createlstreamatplayer] : "
                "stream(%p), dist(%.2f), player(%hu), color(0x%x), name(%s)",
                stData.stream, stData.distance, stData.target,
                stData.color, stData.color ? stData.name : "");

            const auto& streamPtr = Plugin::streamTable[stData.stream] =
                MakeStreamAtPlayer(stData.color, stData.name, stData.distance, stData.target);

            streamPtr->AddPlayCallback(SpeakerList::OnSpeakerPlay);
            streamPtr->AddStopCallback(SpeakerList::OnSpeakerStop);
        } 
        break;
        case SV::ControlPacketType::createLStreamAtObject:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateLStreamAtPacket*>(controlPacket.data);
            if(controlPacket.length < sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:createlstreamatobject] : "
                "stream(%p), dist(%.2f), object(%hu), color(0x%x), name(%s)",
                stData.stream, stData.distance, stData.target,
                stData.color, stData.color ? stData.name : "");

            const auto& streamPtr = Plugin::streamTable[stData.stream] =
                MakeStreamAtObject(stData.color, stData.name, stData.distance, stData.target);

            streamPtr->AddPlayCallback(SpeakerList::OnSpeakerPlay);
            streamPtr->AddStopCallback(SpeakerList::OnSpeakerStop);
        } 
        break;
        case SV::ControlPacketType::updateLStreamDistance:
        {
            const auto& stData = *reinterpret_cast<const SV::UpdateLStreamDistancePacket*>(controlPacket.data);
            if(controlPacket.length != sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:updatelpstreamdistance] : stream(%p), dist(%.2f)",
                stData.stream, stData.distance);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if(iter == Plugin::streamTable.end()) break;

            static_cast<LocalStream*>(iter->second.get())->SetDistance(stData.distance);
        } 
        break;
        case SV::ControlPacketType::updateLPStreamPosition:
        {
            const auto& stData = *reinterpret_cast<const SV::UpdateLPStreamPositionPacket*>(controlPacket.data);
            if(controlPacket.length != sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:updatelpstreamcoords] : stream(%p), pos(%.2f;%.2f;%.2f)",
                stData.stream, stData.position.x, stData.position.y, stData.position.z);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if(iter == Plugin::streamTable.end()) break;

            static_cast<StreamAtPoint*>(iter->second.get())->SetPosition(stData.position);
        } 
        break;
        case SV::ControlPacketType::deleteStream:
        {
            const auto& stData = *reinterpret_cast<const SV::DeleteStreamPacket*>(controlPacket.data);
            if (controlPacket.length != sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:deletestream] : stream(%p)", stData.stream);

            Plugin::streamTable.erase(stData.stream);
        } 
        break;
        case SV::ControlPacketType::setStreamParameter:
        {
            const auto& stData = *reinterpret_cast<const SV::SetStreamParameterPacket*>(controlPacket.data);
            if(controlPacket.length != sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:streamsetparameter] : stream(%p), parameter(%hhu), value(%.2f)",
                stData.stream, stData.parameter, stData.value);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if(iter == Plugin::streamTable.end()) break;

            iter->second->SetParameter(stData.parameter, stData.value);
        } 
        break;
        case SV::ControlPacketType::slideStreamParameter:
        {
            const auto& stData = *reinterpret_cast<const SV::SlideStreamParameterPacket*>(controlPacket.data);
            if(controlPacket.length != sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:streamslideparameter] : "
                "stream(%p), parameter(%hhu), startvalue(%.2f), endvalue(%.2f), time(%u)",
                stData.stream, stData.parameter, stData.startvalue, stData.endvalue, stData.time);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if(iter == Plugin::streamTable.end()) break;

            iter->second->SlideParameter(stData.parameter, stData.startvalue, stData.endvalue, stData.time);
        } 
        break;
        case SV::ControlPacketType::createEffect:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateEffectPacket*>(controlPacket.data);
            if(controlPacket.length < sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:effectcreate] : "
                "stream(%p), effect(%p), number(%hhu), priority(%d)",
                stData.stream, stData.effect, stData.number, stData.priority);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if(iter == Plugin::streamTable.end()) break;

            iter->second->EffectCreate(stData.effect, stData.number, stData.priority,
                stData.params, controlPacket.length - sizeof(stData));
        } 
        break;
        case SV::ControlPacketType::deleteEffect:
        {
            const auto& stData = *reinterpret_cast<const SV::DeleteEffectPacket*>(controlPacket.data);
            if(controlPacket.length != sizeof(stData)) break;

            LogVoice("[sv:dbg:plugin:effectdelete] : stream(%p), effect(%p)",
                stData.stream, stData.effect);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if(iter == Plugin::streamTable.end()) break;

            iter->second->EffectDelete(stData.effect);
        } 
        break;
    }
}

void Plugin::DisconnectHandler()
{
    Plugin::streamTable.clear();

    Plugin::muteStatus = false;
    Plugin::recordStatus = false;
    Plugin::recordBusy = false;

    Record::Free();
}

void Plugin::OnDeviceInit()
{
    FLog("Plugin::OnDeviceInit()");
    SpeakerList::Init();
    MicroIcon::Init();
}

void Plugin::OnRender()
{
    Timer::Tick();
    SpeakerList::Render();
    Plugin::MainLoop();
}

void Plugin::OnDeviceFree()
{
    SpeakerList::Free();
    MicroIcon::Free();
}

bool Plugin::muteStatus { false };
bool Plugin::recordStatus { false };
bool Plugin::recordBusy { false };
int Plugin::MicRecord{ 0 };
int Plugin::MicPress{ 0 };

std::map<uint32_t, StreamPtr> Plugin::streamTable;
