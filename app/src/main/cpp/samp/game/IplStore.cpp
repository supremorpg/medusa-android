//
// Created by x1y2z on 15.11.2023.
//

#include "IplStore.h"
#include "GTASAEngineApi.h"

void CIplStore::LoadIpls(CVector posn, bool bAvoidLoadInPlayerVehicleMovingDirection) {
    GTASAEngineApi::LoadIpls(posn, bAvoidLoadInPlayerVehicleMovingDirection);
}

void CIplStore::EnsureIplsAreInMemory(const CVector *posn) {
    if (!posn) {
        return;
    }
    GTASAEngineApi::EnsureIplsAreInMemory(*posn);
}

void CIplStore::AddIplsNeededAtPosn(const CVector *posn) {
    if (!posn) {
        return;
    }
    GTASAEngineApi::AddIplsNeededAtPosn(*posn);
}

void CIplStore::RemoveIpl(int32 iplSlotIndex) {
    GTASAEngineApi::RemoveIpl(iplSlotIndex);
}
