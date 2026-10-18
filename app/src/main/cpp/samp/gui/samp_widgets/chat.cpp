#include "../gui.h"
#include "../../main.h"
#include "../../game/game.h"
#include "../../net/netgame.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sys/stat.h>
#include "../settings.h"
#include "java/jniutil.h"
#include "src/ui/SampUiController.h"

extern UI* pUI;
extern CGame* pGame;
extern CNetGame* pNetGame;
extern CSettings* pSettings;
extern CJavaWrapper *pJavaWrapper;

namespace {
bool IsLocalHdMapCommand(std::string command)
{
	if (!command.empty() && command[0] == '/') {
		command.erase(0, 1);
	}
	const size_t space = command.find_first_of(" \t\r\n");
	if (space != std::string::npos) {
		command.resize(space);
	}
	std::transform(command.begin(), command.end(), command.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return command == "hdmap" || command == "mapahd" || command == "gpshd";
}

bool QueueRuntimeCommand(const char* command)
{
	if (!g_pszStorage || !g_pszStorage[0] || !command || !command[0]) {
		return false;
	}

	char dir[512]{};
	std::snprintf(dir, sizeof(dir), "%sSAMP", g_pszStorage);
	mkdir(dir, 0775);
	std::snprintf(dir, sizeof(dir), "%sSAMP/xyron_monitor", g_pszStorage);
	mkdir(dir, 0775);

	char path[640]{};
	std::snprintf(path, sizeof(path), "%sSAMP/xyron_monitor/runtime_command.txt", g_pszStorage);
	FILE* file = std::fopen(path, "wb");
	if (!file) {
		return false;
	}
	std::fputs(command, file);
	std::fputc('\n', file);
	std::fclose(file);
	return true;
}
}

Chat::Chat()
	: ListBox()
{

}

void Chat::addChatMessage(const std::string& message, const std::string& nick, const ImColor& nick_color)
{
	if (SampUiController::IsEnabled()) {
		SampUiController::AddPlayerMessage(nick.c_str(), message.c_str(), static_cast<ImU32>(nick_color));
	}
	addPlayerMessage(message, nick, nick_color);
}

void Chat::addInfoMessage(const std::string& format, ...)
{
	char tmp_buf[512];

	va_list args;
	va_start(args, format);
	vsprintf(tmp_buf, format.c_str(), args);
	va_end(args);

	addMessage(std::string(tmp_buf), ImColor(0x00, 0xc8, 0xc8));
}

void Chat::addDebugMessage(const std::string& format, ...)
{
	char tmp_buf[512];

	va_list args;
	va_start(args, format);
	vsprintf(tmp_buf, format.c_str(), args);
	va_end(args);

	addMessage(std::string(tmp_buf), ImColor(0xbe, 0xbe, 0xbe));
}

void Chat::addClientMessage(const std::string& message, const ImColor& color)
{
	addMessage(message, color);
}

void Chat::addMessage(const std::string& message, const ImColor& color)
{
	if (SampUiController::IsEnabled()) {
		SampUiController::AddClientMessage(message.c_str(), static_cast<ImU32>(color));
	}

	if (this->itemsCount() > UISettings::chatMaxMessages())
	{
		this->removeItem(0);
	}

	MessageItem* item = new MessageItem(message, color);
	this->addItem(item);
	/*if(!active())*/ this->setScrollY(1.0f);
}

void Chat::addPlayerMessage(const std::string& message, const std::string& nick, const ImColor& nick_color)
{
	if (this->itemsCount() > UISettings::chatMaxMessages())
	{
		this->removeItem(0);
	}

	PlayerMessageItem* item = new PlayerMessageItem(message, nick, nick_color);
	this->addItem(item);
	/*if(!active())*/ this->setScrollY(1.0f);
}

void Chat::draw(ImGuiRenderer* renderer)
{
	if (SampUiController::IsEnabled()) return;
	ListBox::draw(renderer);
}

void Chat::activateEvent(bool active)
{
	if (active)
	{
		this->setScrollable(true);
	}
	else
	{
		this->setScrollable(false);
	}
}

void Chat::touchPopEvent()
{
	if (pUI->playertablist()->visible()) return;
	if (SampUiController::IsScoreboardVisible()) return;

	SampUiController::SetChatInputActive(true);
	pUI->keyboard()->show(this);
}

void Chat::keyboardEvent(const std::string& input)
{
	SampUiController::SetChatInputActive(false);
	if (input.length() > 0 && pNetGame)
	{
		if (input[0] == '/') {
			if (!commandClient(input)) pNetGame->SendChatCommand(input.c_str());
		}
		else pNetGame->SendChatMessage(input.c_str());
	}
}

bool Chat::commandClient(const std::string& command)
{
	if (command == "/tab")
	{
		if (pNetGame) pNetGame->UpdatePlayerScoresAndPings();
		SampUiController::ToggleScoreboard();
		return true;
	}

	if (command == "/hidechat")
	{
		SampUiController::ToggleChatVisible();
		return true;
	}

	if (IsLocalHdMapCommand(command))
	{
		const bool queued = QueueRuntimeCommand("hdmap");
		SampUiController::AddInfoMessage(queued ? "Abrindo mapa HD..." : "Mapa HD indisponivel neste momento.");
		return true;
	}

	return false;
}
