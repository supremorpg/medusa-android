#include "../main.h"
#include "../game/game.h"
#include "netgame.h"
#include <cmath>

extern CGame *pGame;
extern CNetGame *pNetGame;

// 0.3.7
CObjectPool::CObjectPool()
{
	for (OBJECTID ObjectID = 0; ObjectID < MAX_OBJECTS; ObjectID++)
	{
		m_bObjectSlotState[ObjectID] = false;
		m_pObjects[ObjectID] = nullptr;
	}

	m_iObjectCount = 0;
}
// 0.3.7
CObjectPool::~CObjectPool()
{
	for (OBJECTID ObjectID = 0; ObjectID < MAX_OBJECTS; ObjectID++)
	{
		Delete(ObjectID);
	}
}

bool CObjectPool::New(OBJECTID ObjectID, int iModel, CVector vecPos, CVector vecRot, float fDrawDistance)
{
    if (ObjectID >= MAX_OBJECTS) {
        FLog("[OBJ_FIX64] CObjectPool::New invalid id=%u model=%d", ObjectID, iModel);
        return false;
    }

	if (m_pObjects[ObjectID] != nullptr) {
		Delete(ObjectID);
	}

	m_pObjects[ObjectID] = pGame->NewObject(iModel, vecPos, vecRot, fDrawDistance);
	if (!m_pObjects[ObjectID]) {
        FLog("[OBJ_FIX64] CObjectPool::New failed to allocate id=%u model=%d", ObjectID, iModel);
        return false;
    }

	m_bObjectSlotState[ObjectID] = true;
    if (!m_pObjects[ObjectID]->EnsureEntityAvailable("pool-new", true)) {
        FLog("[OBJ_FIX64] CObjectPool::New pending entity id=%u model=%d (will retry in Process)", ObjectID, iModel);
    }
	m_pObjects[ObjectID]->EnsureCollisionState(ObjectID, "create", true);
	return true;
}

bool CObjectPool::Delete(OBJECTID ObjectID)
{
	if (ObjectID < MAX_OBJECTS && m_bObjectSlotState[ObjectID])
	{
		CObject* pObject = m_pObjects[ObjectID];
		if (pObject)
		{
			delete m_pObjects[ObjectID];
		}

        m_pObjects[ObjectID] = nullptr;
        m_bObjectSlotState[ObjectID] = false;
	}

	return true;
}

void CObjectPool::Process()
{
	static uint32_t s_dwLastTick = 0;
    static uint32_t s_lastDiagTick = 0;

	if (s_dwLastTick == 0) {
		s_dwLastTick = GetTickCount();
	}

	uint32_t dwThisTick = GetTickCount();
	float fElapsedTime = (dwThisTick - s_dwLastTick) / 1000.0f;

	for (OBJECTID i = 0; i < MAX_OBJECTS; i++)
	{
		if (m_bObjectSlotState[i]) {
            CObject* object = m_pObjects[i];
            if (!object) {
                continue;
            }

            object->EnsureEntityAvailable("pool-process");
			object->Process(fElapsedTime);
			object->EnsureCollisionState(i, "pool-scan");
		}
	}

    if (dwThisTick - s_lastDiagTick >= 2000 && pNetGame) {
        s_lastDiagTick = dwThisTick;

        CPlayerPool* playerPool = pNetGame->GetPlayerPool();
        CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
        CPlayerPed* localPed = localPlayer ? localPlayer->GetPlayerPed() : nullptr;

        if (localPed && localPed->m_pPed) {
            const CVector playerPos = localPed->m_pPed->GetPosition();
            const float nearRadiusSq = 180.0f * 180.0f;

            int activeObjects = 0;
            int nearbyObjects = 0;
            int nearbyNoEntity = 0;
            int nearbyNoCollision = 0;
            int nearbyDisabledSimple = 0;
            int logged = 0;

            for (OBJECTID i = 0; i < MAX_OBJECTS; i++) {
                if (!m_bObjectSlotState[i]) {
                    continue;
                }
                activeObjects++;

                CObject* obj = m_pObjects[i];
                if (!obj) {
                    continue;
                }

                obj->EnsureEntityAvailable("diag");

                if (!obj->m_pEntity) {
                    nearbyNoEntity++;
                    continue;
                }

                const CVector objPos = obj->m_pEntity->GetPosition();
                const float dx = objPos.x - playerPos.x;
                const float dy = objPos.y - playerPos.y;
                const float dz = objPos.z - playerPos.z;
                const float distSq = dx * dx + dy * dy + dz * dz;
                if (distSq > nearRadiusSq) {
                    continue;
                }

                nearbyObjects++;

                const bool collUse = obj->m_pEntity->m_bUsesCollision;
                const bool collFlag = obj->m_pEntity->physicalFlags.bCollidable;
                const bool hitFlag = obj->m_pEntity->physicalFlags.bCanBeCollidedWith;
                const bool simpleEnabled = !obj->m_pEntity->physicalFlags.bDisableSimpleCollision;

                if (!collUse || !collFlag || !hitFlag || !simpleEnabled) {
                    nearbyNoCollision++;
                    if (!simpleEnabled) {
                        nearbyDisabledSimple++;
                    }
                    obj->EnsureCollisionState(i, "diag-nearby", true);
                    if (logged < 6) {
                        logged++;
                        FLog("[OBJ_DIAG64] near id=%u model=%d dist=%.1f pos=%.1f %.1f %.1f coll={use:%d,col:%d,hit:%d,simple:%d}",
                             i,
                             obj->m_pEntity->m_nModelIndex,
                             sqrtf(distSq),
                             objPos.x,
                             objPos.y,
                             objPos.z,
                             collUse ? 1 : 0,
                             collFlag ? 1 : 0,
                             hitFlag ? 1 : 0,
                             simpleEnabled ? 1 : 0);
                    }
                }
            }

            FLog("[OBJ_DIAG64] summary active=%d near=%d noEntity=%d nearNoCol=%d nearSimpleOff=%d player=%.1f %.1f %.1f",
                 activeObjects,
                 nearbyObjects,
                 nearbyNoEntity,
                 nearbyNoCollision,
                 nearbyDisabledSimple,
                 playerPos.x,
                 playerPos.y,
                 playerPos.z);
        }
    }

	s_dwLastTick = dwThisTick;
}

CObject* CObjectPool::FindObjectFromGtaPtr(CPhysical* pGtaObject)
{
	for (OBJECTID ObjectID = 0; ObjectID < MAX_OBJECTS; ObjectID++)
	{
		if (m_pObjects[ObjectID] && m_pObjects[ObjectID]->m_pEntity && m_pObjects[ObjectID]->m_pEntity == pGtaObject)
			return m_pObjects[ObjectID];
	}

	return nullptr;
}

OBJECTID CObjectPool::FindIDFromGtaPtr(CPhysical* pGtaObject)
{
	for (OBJECTID ObjectID = 0; ObjectID < MAX_OBJECTS; ObjectID++)
	{
		if (m_pObjects[ObjectID] && m_pObjects[ObjectID]->m_pEntity && m_pObjects[ObjectID]->m_pEntity == pGtaObject)
			return ObjectID;
	}

	return INVALID_OBJECT_ID;
}

void CObjectPool::ProcessMaterialText()
{
	for (OBJECTID ObjectID = 0; ObjectID < MAX_OBJECTS; ObjectID++)
	{
		if (m_pObjects[ObjectID] && m_bObjectSlotState[ObjectID] == true)
		{
			m_pObjects[ObjectID]->ProcessMaterialText();
		}
	}
} 
