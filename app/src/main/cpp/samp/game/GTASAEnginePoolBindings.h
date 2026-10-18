#pragma once

#include "common.h"

namespace GTASAEngineApi {

void BindAuxiliaryPoolsNative(void* colModelPool,
                              void* eventPool,
                              void* pointRoutePool,
                              void* patrolRoutePool,
                              void* nodeRoutePool,
                              void* taskAllocatorPool,
                              void* pedIntelligencePool,
                              void* pedAttractorPool);
void InstallPoolsHooksNative(uintptr_t initialiseHook,
                             void* buildingPoolRef,
                             void* dummyPoolRef,
                             void* entryInfoNodePoolRef,
                             void* ptrNodeSingleLinkPoolRef,
                             void* ptrNodeDoubleLinkPoolRef,
                             void* pedPoolRef,
                             void* vehiclePoolRef,
                             void* objectPoolRef,
                             void* taskPoolRef);

}
