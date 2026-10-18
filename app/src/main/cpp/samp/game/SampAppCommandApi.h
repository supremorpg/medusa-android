#pragma once

#include <cstdint>

namespace Xyron::AppCommand {

enum class KnownCommand : uint8_t {
    Sync = 0,
    Inventory,
    VehicleLock,
    WeaponTest
};

bool IsTransportReady();
bool SendKnown(KnownCommand command);
bool SendSyncRequest();
bool SendInventoryRequest();
bool SendVehicleLockToggle();
bool SendWeaponTestRequest();
bool SendRawAppArgs(const char* args);
bool TrySendAppCommandLine(const char* commandLine);

}
