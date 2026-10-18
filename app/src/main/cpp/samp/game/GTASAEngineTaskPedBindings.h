#pragma once

#include <cstdint>

#include "common.h"

struct CPedGTA;

namespace GTASAEngineApi {

uintptr_t CreateTask();
void ConstructEnterCarAsDriverTask(uintptr_t task, uintptr_t vehicle);
void SetTaskManagerTask(uintptr_t taskManager, uintptr_t task, int priority, int argument);
uint32_t GetPedWeaponSkill(CPedGTA* ped, uint32_t weaponType);

}
