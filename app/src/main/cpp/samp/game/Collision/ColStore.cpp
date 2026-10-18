//
// Created by x1y2z on 01.07.2023.
//

#include "ColStore.h"
#include "../GTASAEngineApi.h"
#include "game/constants.h"

namespace GTASAEngineApi {
CColPool* ColStorePool();
}

void CColStore::RequestCollision(const CVector *pos, int32 areaCode) {
   if (!pos) {
      return;
   }

   GTASAEngineApi::RequestCollision(*pos, areaCode);
}

int32 CColStore::FindColSlot(const char* name)
{
   if (!name || !name[0]) {
      return -1;
   }

   return GTASAEngineApi::FindColSlot(name);
}

void CColStore::AddRef(int32 colSlot)
{
   GTASAEngineApi::AddColRef(colSlot);
}

void CColStore::RemoveRef(int32 colSlot)
{
   GTASAEngineApi::RemoveColRef(colSlot);
}

void CColStore::SetCollisionRequired(const CVector* pos, int32 areaCode)
{
   if (!pos) {
      return;
   }

   GTASAEngineApi::SetCollisionRequired(*pos, areaCode);
}

bool CColStore::HasCollisionLoaded(const CVector* pos, int32 areaCode)
{
   if (!pos) {
      return false;
   }

   return GTASAEngineApi::HasCollisionLoaded(*pos, areaCode);
}

void CColStore::IncludeModelIndex(int32 colSlot, int32 modelId)
{
   GTASAEngineApi::IncludeColModelIndex(colSlot, modelId);
}

void CColStore::RemoveCol(int32 colSlot)
{
   GTASAEngineApi::RemoveCol(colSlot);
}

void CColStore::LoadCol(int32 colSlot, const char* name)
{
   if (!name || !name[0]) {
      return;
   }

   GTASAEngineApi::LoadCol(colSlot, name);
}

void CColStore::AddCollisionNeededAtPosn(const CVector *pos) {
   if (!pos) {
      return;
   }
   GTASAEngineApi::AddCollisionNeededAtPosn(*pos);
}

void CColStore::LoadCollision(CVector pos, bool bIgnorePlayerVeh)
{
   GTASAEngineApi::LoadCollision(pos, bIgnorePlayerVeh);
}

void CColStore::LoadAllCollision()
{
   GTASAEngineApi::LoadAllCollision();
}

void CColStore::LoadAllBoundingBoxes()
{
   GTASAEngineApi::LoadAllBoundingBoxes();
}

void CColStore::EnsureCollisionIsInMemory(const CVector* pos)
{
   if (!pos) {
      return;
   }
   GTASAEngineApi::EnsureCollisionIsInMemory(*pos);
}

void CColStore::Initialise()
{
    GTASAEngineApi::InitialiseColStore();
}

CColPool* CColStore::GetPool()
{
   return GTASAEngineApi::ColStorePool();
}

bool CColStore::GetOnlyBB()
{
   return GTASAEngineApi::ColStoreOnlyBB();
}
