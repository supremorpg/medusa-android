//
// Created by x1y2z on 01.07.2023.
//

#pragma once

#include "../Core/Pool.h"
#include "../common.h"
#include "../Core/Rect.h"

struct ColDef {
    CRect  m_Area;
    uint32 field_10;
    uint32 field_14;
    uint32 field_18;
    uint32 field_1C;
    uint16 field_20;
    int16  m_nModelIdStart;
    int16  m_nModelIdEnd;
    uint16 m_nRefCount;
    bool   m_bActive;
    bool   m_bCollisionIsRequired;
    bool   m_bProcedural;
    bool   m_bInterior;

    static void* operator new(size_t size);
    static void  operator delete(void* data);
};
VALIDATE_SIZE(ColDef, 0x2C);

typedef CPool<ColDef> CColPool;

class CColStore {
public:
    static void Initialise();
    static int32 AddColSlot(const char* name);
    static int32 FindColSlot(const char* name);
    static void AddRef(int32 colSlot);
    static void RemoveRef(int32 colSlot);
    static void RequestCollision(const CVector* pos, int32 areaCode);
    static void SetCollisionRequired(const CVector* pos, int32 areaCode);
    static bool HasCollisionLoaded(const CVector* pos, int32 areaCode);
    static void AddCollisionNeededAtPosn(const CVector* pos);
    static void IncludeModelIndex(int32 colSlot, int32 modelId);
    static void RemoveCol(int32 colSlot);
    static void LoadCol(int32 colSlot, const char* name);

    static void LoadCollision(CVector pos, bool bIgnorePlayerVeh);
    static void LoadAllCollision();
    static void LoadAllBoundingBoxes();

    static void EnsureCollisionIsInMemory(const CVector *pos);
    static CColPool* GetPool();
    static bool GetOnlyBB();
};
