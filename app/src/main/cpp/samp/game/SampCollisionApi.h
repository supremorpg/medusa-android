#pragma once

#include "common.h"

namespace Xyron::Collision {

struct ColSlotInfo {
    bool valid = false;
    bool empty = true;
    int32 slot = -1;
    int32 poolSize = -1;
    float areaLeft = 0.0f;
    float areaBottom = 0.0f;
    float areaRight = 0.0f;
    float areaTop = 0.0f;
    uint32 field10 = 0;
    uint32 field14 = 0;
    uint32 field18 = 0;
    uint32 field1C = 0;
    uint16 field20 = 0;
    int16 modelIdStart = -1;
    int16 modelIdEnd = -1;
    uint16 refCount = 0;
    bool active = false;
    bool required = false;
    bool procedural = false;
    bool interior = false;
    char name[19] = {};
};

int32 PoolSize();
bool ReadSlotInfo(int32 slot, ColSlotInfo& outInfo);
bool IsValidSlot(int32 slot);
bool IsSlotActive(int32 slot);
bool IsSlotRequired(int32 slot);
bool OnlyBoundingBoxes();

int32 FindSlot(const char* name);
bool HasCollisionLoaded(const CVector& pos, int32 areaCode);
void AddRef(int32 slot);
void RemoveRef(int32 slot);
void IncludeModelIndex(int32 slot, int32 modelId);
void LoadCol(int32 slot, const char* name);
void LoadAllCollision();
void LoadAllBoundingBoxes();

void AddCollisionNeededAtPosn(const CVector& pos);
void SetCollisionRequired(const CVector& pos, int32 areaCode);
void RequestCollision(const CVector& pos, int32 areaCode);
void LoadCollision(const CVector& pos, bool ignorePlayerVehicle);
void EnsureCollisionInMemory(const CVector& pos);

}
