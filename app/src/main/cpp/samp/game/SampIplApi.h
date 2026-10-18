#pragma once

#include "common.h"

namespace Xyron::Ipl {

void AddNeededAtPosition(const CVector& pos);
void LoadAtPosition(const CVector& pos, bool avoidPlayerVehicleMovingDirection);
void EnsureInMemory(const CVector& pos);
void RemoveSlot(int32 iplSlotIndex);

}
