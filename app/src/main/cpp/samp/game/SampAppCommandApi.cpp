#include "SampAppCommandApi.h"

#include "../main.h"
#include "../net/netgame.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

extern CNetGame* pNetGame;

namespace Xyron::AppCommand {
namespace {

constexpr std::size_t kMaxAppCommandBytes = 512;

bool IsAsciiSpace(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

char ToLowerAscii(char value)
{
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }
    return value;
}

const char* SkipSpaces(const char* value)
{
    if (!value) {
        return "";
    }
    while (IsAsciiSpace(*value)) {
        ++value;
    }
    return value;
}

bool IsAppCommandPrefix(const char* value, const char** argsOut)
{
    const char* cursor = SkipSpaces(value);
    if (cursor[0] != '/' ||
        ToLowerAscii(cursor[1]) != 'a' ||
        ToLowerAscii(cursor[2]) != 'p' ||
        ToLowerAscii(cursor[3]) != 'p') {
        return false;
    }

    const char tail = cursor[4];
    if (tail != '\0' && !IsAsciiSpace(tail)) {
        return false;
    }

    if (argsOut) {
        *argsOut = SkipSpaces(cursor + 4);
    }
    return true;
}

const char* ArgsForKnownCommand(KnownCommand command)
{
    switch (command) {
        case KnownCommand::Sync:
            return "sync";
        case KnownCommand::Inventory:
            return "inv";
        case KnownCommand::VehicleLock:
            return "lock";
        case KnownCommand::WeaponTest:
            return "ak";
        default:
            return "";
    }
}

const char* NameForKnownCommand(KnownCommand command)
{
    switch (command) {
        case KnownCommand::Sync:
            return "sync";
        case KnownCommand::Inventory:
            return "inventory";
        case KnownCommand::VehicleLock:
            return "vehicle-lock";
        case KnownCommand::WeaponTest:
            return "weapon-test";
        default:
            return "unknown";
    }
}

bool SendAppArgsInternal(const char* args, const char* tag)
{
    if (!IsTransportReady()) {
        FLog("[APP_CMD64] skip tag=%s reason=not-connected", tag ? tag : "raw");
        return false;
    }

    const char* safeArgs = SkipSpaces(args);
    char command[kMaxAppCommandBytes];
    const int written = (safeArgs && safeArgs[0])
        ? std::snprintf(command, sizeof(command), "/app %s", safeArgs)
        : std::snprintf(command, sizeof(command), "/app");

    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(command)) {
        FLog("[APP_CMD64] skip tag=%s reason=too-long", tag ? tag : "raw");
        return false;
    }

    pNetGame->SendChatCommand(command);
    FLog("[APP_CMD64] sent tag=%s transport=server-command", tag ? tag : "raw");
    return true;
}

} // namespace

bool IsTransportReady()
{
    return pNetGame && pNetGame->GetGameState() == GAMESTATE_CONNECTED;
}

bool SendKnown(KnownCommand command)
{
    return SendAppArgsInternal(ArgsForKnownCommand(command), NameForKnownCommand(command));
}

bool SendSyncRequest()
{
    return SendKnown(KnownCommand::Sync);
}

bool SendInventoryRequest()
{
    return SendKnown(KnownCommand::Inventory);
}

bool SendVehicleLockToggle()
{
    return SendKnown(KnownCommand::VehicleLock);
}

bool SendWeaponTestRequest()
{
    return SendKnown(KnownCommand::WeaponTest);
}

bool SendRawAppArgs(const char* args)
{
    return SendAppArgsInternal(args, "raw");
}

bool TrySendAppCommandLine(const char* commandLine)
{
    const char* args = nullptr;
    if (!IsAppCommandPrefix(commandLine, &args)) {
        return false;
    }
    (void)SendAppArgsInternal(args, "line");
    return true;
}

}
