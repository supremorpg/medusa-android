#include "SampCollisionApi.h"
#include "Collision/ColStore.h"

namespace Xyron::Collision {
namespace {

CColPool* CollisionPool()
{
    return CColStore::GetPool();
}

ColDef* SlotDefinition(int32 slot)
{
    CColPool* pool = CollisionPool();
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || slot < 0 || slot >= pool->m_nSize ||
        pool->m_byteMap[slot].bEmpty) {
        return nullptr;
    }

    return &pool->m_pObjects[slot];
}

void CopySlotName(const ColDef& def, char out[19])
{
    if (!out) {
        return;
    }
    out[0] = '\0';

    const char* raw = reinterpret_cast<const char*>(&def.field_10);
    int i = 0;
    for (; i < 18; ++i) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        if (c == 0) {
            break;
        }
        out[i] = (c >= 32 && c <= 126) ? static_cast<char>(c) : '?';
    }
    out[i] = '\0';
}

}

int32 PoolSize()
{
    CColPool* pool = CollisionPool();
    return pool ? pool->m_nSize : -1;
}

bool ReadSlotInfo(int32 slot, ColSlotInfo& outInfo)
{
    outInfo = ColSlotInfo{};
    outInfo.slot = slot;

    CColPool* pool = CollisionPool();
    outInfo.poolSize = pool ? pool->m_nSize : -1;
    if (!pool || !pool->m_pObjects || !pool->m_byteMap || slot < 0 || slot >= pool->m_nSize) {
        return false;
    }

    outInfo.valid = true;
    outInfo.empty = pool->m_byteMap[slot].bEmpty;
    if (outInfo.empty) {
        return true;
    }

    const ColDef& def = pool->m_pObjects[slot];
    outInfo.areaLeft = def.m_Area.left;
    outInfo.areaBottom = def.m_Area.bottom;
    outInfo.areaRight = def.m_Area.right;
    outInfo.areaTop = def.m_Area.top;
    outInfo.field10 = def.field_10;
    outInfo.field14 = def.field_14;
    outInfo.field18 = def.field_18;
    outInfo.field1C = def.field_1C;
    outInfo.field20 = def.field_20;
    outInfo.modelIdStart = def.m_nModelIdStart;
    outInfo.modelIdEnd = def.m_nModelIdEnd;
    outInfo.refCount = def.m_nRefCount;
    outInfo.active = def.m_bActive;
    outInfo.required = def.m_bCollisionIsRequired;
    outInfo.procedural = def.m_bProcedural;
    outInfo.interior = def.m_bInterior;
    CopySlotName(def, outInfo.name);
    return true;
}

bool IsValidSlot(int32 slot)
{
    ColSlotInfo info{};
    return ReadSlotInfo(slot, info) && !info.empty;
}

bool IsSlotActive(int32 slot)
{
    ColSlotInfo info{};
    return ReadSlotInfo(slot, info) && !info.empty && info.active;
}

bool IsSlotRequired(int32 slot)
{
    ColSlotInfo info{};
    return ReadSlotInfo(slot, info) && !info.empty && info.required;
}

bool OnlyBoundingBoxes()
{
    return CColStore::GetOnlyBB();
}

int32 FindSlot(const char* name)
{
    return CColStore::FindColSlot(name);
}

bool HasCollisionLoaded(const CVector& pos, int32 areaCode)
{
    return CColStore::HasCollisionLoaded(&pos, areaCode);
}

void AddRef(int32 slot)
{
    if (!IsValidSlot(slot)) {
        return;
    }
    CColStore::AddRef(slot);
}

void RemoveRef(int32 slot)
{
    if (!IsValidSlot(slot)) {
        return;
    }
    CColStore::RemoveRef(slot);
}

void IncludeModelIndex(int32 slot, int32 modelId)
{
    if (!IsValidSlot(slot) || modelId < 0) {
        return;
    }
    CColStore::IncludeModelIndex(slot, modelId);
}

void LoadCol(int32 slot, const char* name)
{
    if (!IsValidSlot(slot) || !name || !name[0]) {
        return;
    }
    CColStore::LoadCol(slot, name);
}

void LoadAllCollision()
{
    CColStore::LoadAllCollision();
}

void LoadAllBoundingBoxes()
{
    CColStore::LoadAllBoundingBoxes();
}

void AddCollisionNeededAtPosn(const CVector& pos)
{
    CColStore::AddCollisionNeededAtPosn(&pos);
}

void SetCollisionRequired(const CVector& pos, int32 areaCode)
{
    CColStore::SetCollisionRequired(&pos, areaCode);
}

void RequestCollision(const CVector& pos, int32 areaCode)
{
    CColStore::RequestCollision(&pos, areaCode);
}

void LoadCollision(const CVector& pos, bool ignorePlayerVehicle)
{
    CColStore::LoadCollision(pos, ignorePlayerVehicle);
}

void EnsureCollisionInMemory(const CVector& pos)
{
    CColStore::EnsureCollisionIsInMemory(&pos);
}

}
