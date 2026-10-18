//
// Created by x1y2z on 13.05.2023.
//

#include "World.h"
#include "GTASAEngineApi.h"
#include "main.h"
#include "game/game.h"
#include "net/netgame.h"

//CRepeatSector CWorld::ms_aRepeatSectors[MAX_REPEAT_SECTORS_Y][MAX_REPEAT_SECTORS_X];
//CPtrListDoubleLink CWorld::ms_listMovingEntityPtrs;

CSector* GetSector(int32 x, int32 y) {
    return GTASAEngineApi::Sector(x, y);
}

CRepeatSector* GetRepeatSector(int32 x, int32 y) {
    return GTASAEngineApi::RepeatSector(x, y);
}

void CWorld::InjectHooks() {

    GTASAEngineApi::InstallWorldProcessPedsAfterPreRenderHook(&ProcessPedsAfterPreRender);
}

bool CWorld::ProcessLineOfSight(const CVector* origin, const CVector* target, CColPoint* outColPoint, CEntityGTA** outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, bool doCameraIgnoreCheck, bool doShootThroughCheck) {
   // assert(!origin.HasNanOrInf() && !target.HasNanOrInf()); // We're getting random nan/inf's from somewhere, so let's try to root cause it...
    return GTASAEngineApi::ProcessLineOfSight(origin, target, outColPoint, outEntity, buildings, vehicles, peds, objects, dummies, doSeeThroughCheck, doCameraIgnoreCheck, doShootThroughCheck);
}

bool CWorld::ProcessVerticalLine(const CVector* origin, float targetZ, CColPoint* outColPoint, CEntityGTA** outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, CStoredCollPoly* storedPoly) {
    return GTASAEngineApi::ProcessVerticalLine(origin, targetZ, outColPoint, outEntity, buildings, vehicles, peds, objects, dummies, doSeeThroughCheck, storedPoly);
}

float CWorld::FindGroundZForCoord(float x, float y) {
    return GTASAEngineApi::FindGroundZForCoord(x, y);
}

bool CWorld::GetIsLineOfSightClear(const CVector& origin, const CVector& target, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, bool doCameraIgnoreCheck) {
    return GTASAEngineApi::GetIsLineOfSightClear(origin, target, buildings, vehicles, peds, objects, dummies, doSeeThroughCheck, doCameraIgnoreCheck);
}

void CWorld::Add(CEntityGTA *entity) {
    GTASAEngineApi::AddWorldEntity(entity);
}

void CWorld::Remove(CEntityGTA *entity) {
    GTASAEngineApi::RemoveWorldEntity(entity);
}

extern CNetGame *pNetGame;
void CWorld::ProcessPedsAfterPreRender() {
    if (CTimer::bSkipProcessThisFrame)
        return;

    if (pNetGame)
    {
        CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
        if (pPlayerPool)
        {
            pPlayerPool->ProcessAttachedObjects();
        }
    }
}
