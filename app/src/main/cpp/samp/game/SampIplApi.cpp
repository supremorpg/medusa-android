#include "SampIplApi.h"

#include "IplStore.h"

namespace Xyron::Ipl {

void AddNeededAtPosition(const CVector& pos)
{
    CIplStore::AddIplsNeededAtPosn(&pos);
}

void LoadAtPosition(const CVector& pos, bool avoidPlayerVehicleMovingDirection)
{
    CIplStore::LoadIpls(pos, avoidPlayerVehicleMovingDirection);
}

void EnsureInMemory(const CVector& pos)
{
    CIplStore::EnsureIplsAreInMemory(&pos);
}

void RemoveSlot(int32 iplSlotIndex)
{
    CIplStore::RemoveIpl(iplSlotIndex);
}

}
