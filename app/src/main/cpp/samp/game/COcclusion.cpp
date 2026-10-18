//
// Created by Gor on 1/4/2025.
//

#include "COcclusion.h"
#include "GTASAEngineApi.h"

void COcclusion::InjectHooks()
{
    GTASAEngineApi::BindOcclusionGlobals(&aOccluders, &NumOccludersOnMap);
}
