//
// Created by x1y2z on 17.04.2023.
//

#include "Pools.h"
#include "GTASAEngineApi.h"
#include "GTASAEnginePoolBindings.h"
#include "IplDef.h"
#include "game/Entity/CVehicleGTA.h"
#include "game/Entity/Building.h"
#include "game/Entity/CopPed.h"
#include "game/Entity/Heli.h"
#include "game/Entity/Dummy.h"
#include "game/Entity/Object.h"
#include "game/Entity/CutsceneObject.h"

namespace {
constexpr int kPtrNodeSingleLinkCapacity = VER_x32 ? 100000 : 120000;
constexpr int kPtrNodeDoubleLinkCapacity = VER_x32 ? 60000 : 90000;
constexpr int kObjectPoolCapacity = VER_x32 ? 3000 : 20000;
constexpr int kEntryInfoNodeCapacity = VER_x32 ? 20000 : 80000;
constexpr int kDummyPoolCapacity = VER_x32 ? 40000 : 50000;
constexpr int kBuildingPoolCapacity = VER_x32 ? 21000 : 26000;
} // namespace

PoolAllocator::Pool*      CPools::ms_pColModelPool;
PoolAllocator::Pool*      CPools::ms_pEventPool;
PoolAllocator::Pool*      CPools::ms_pPointRoutePool;
PoolAllocator::Pool*      CPools::ms_pPatrolRoutePool;
PoolAllocator::Pool*      CPools::ms_pNodeRoutePool;
PoolAllocator::Pool*      CPools::ms_pTaskAllocatorPool;
PoolAllocator::Pool*      CPools::ms_pPedIntelligencePool;
PoolAllocator::Pool*      CPools::ms_pPedAttractorPool;


void CPools::Initialise()
{
    FLog("CPools::Initialise");

    CPools::ms_pPtrNodeSingleLinkPool   = new CPool<CPtrNodeSingleLink>(kPtrNodeSingleLinkCapacity, "PtrNode Single");
    CPools::ms_pPtrNodeDoubleLinkPool   = new CPool<CPtrNodeDoubleLink>(kPtrNodeDoubleLinkCapacity, "PtrNode Double");
    CPools::ms_pPedPool                 = new CPool<CPedGTA, CCopPed>(240, "Peds");
    CPools::ms_pVehiclePool             = new CPool<CVehicleGTA, CHeli>(1000, "Vehicles");
    CPools::ms_pObjectPool              = new CPool<CObjectGta, CCutsceneObject>(kObjectPoolCapacity, "Objects");
    CPools::ms_pTaskPool                = new CPool<CTask, CTaskSimpleSlideToCoord>(6000, "Task");
    CPools::ms_pEntryInfoNodePool       = new CPool<CEntryInfoNode>(kEntryInfoNodeCapacity, "EntryInfoNodePool");
    CPools::ms_pDummyPool               = new CPool<CDummy>(kDummyPoolCapacity, "Dummies");
    CPools::ms_pBuildingPool            = new CPool<CBuilding>(kBuildingPoolCapacity, "Buildings");

    FLog("[POOL_FIX64] capacities ptr1=%d ptr2=%d obj=%d entry=%d dummy=%d building=%d objStride=%zu cutsceneStride=%zu",
         kPtrNodeSingleLinkCapacity,
         kPtrNodeDoubleLinkCapacity,
         kObjectPoolCapacity,
         kEntryInfoNodeCapacity,
         kDummyPoolCapacity,
         kBuildingPoolCapacity,
         sizeof(CObjectGta),
         sizeof(CCutsceneObject));

    CPools::ms_pColModelPool = PoolAllocator::Allocate(50000, (VER_x32 ? 0x30 : 0x38));
    // 13600 / 200 = 68
    CPools::ms_pEventPool = PoolAllocator::Allocate(1000, (VER_x32 ? 0x44 : 0x58));
    // 6400 / 64 = 100
    CPools::ms_pPointRoutePool = PoolAllocator::Allocate(200, (VER_x32 ? 0x64 : 0x64));
    // 13440 / 32 = 420
    CPools::ms_pPatrolRoutePool = PoolAllocator::Allocate(200, 420);	// 32
    // 2304 / 64 = 36
    CPools::ms_pNodeRoutePool = PoolAllocator::Allocate(200, (VER_x32 ? 0x24 : 0x24));
    // 512 / 16 = 32
    CPools::ms_pTaskAllocatorPool = PoolAllocator::Allocate(3000, (VER_x32 ? 0x20 : 0x30));
    // 92960 / 140 = 664
    CPools::ms_pPedIntelligencePool = PoolAllocator::Allocate(240, (VER_x32 ? 0x298 : 0x440));
    // 15104 / 64 = 236
    CPools::ms_pPedAttractorPool = PoolAllocator::Allocate(200, (VER_x32 ? 0xEC : 0xEC));

    GTASAEngineApi::BindAuxiliaryPoolsNative(CPools::ms_pColModelPool,
                                             CPools::ms_pEventPool,
                                             CPools::ms_pPointRoutePool,
                                             CPools::ms_pPatrolRoutePool,
                                             CPools::ms_pNodeRoutePool,
                                             CPools::ms_pTaskAllocatorPool,
                                             CPools::ms_pPedIntelligencePool,
                                             CPools::ms_pPedAttractorPool);
}

void CPools::InjectHooks() {
    GTASAEngineApi::InstallPoolsHooksNative(reinterpret_cast<uintptr_t>(&CPools::Initialise),
                                            &CPools::ms_pBuildingPool,
                                            &CPools::ms_pDummyPool,
                                            &CPools::ms_pEntryInfoNodePool,
                                            &CPools::ms_pPtrNodeSingleLinkPool,
                                            &CPools::ms_pPtrNodeDoubleLinkPool,
                                            &CPools::ms_pPedPool,
                                            &CPools::ms_pVehiclePool,
                                            &CPools::ms_pObjectPool,
                                            &CPools::ms_pTaskPool);
}
