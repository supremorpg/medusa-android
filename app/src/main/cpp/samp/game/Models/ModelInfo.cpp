//
// Created by x1y2z on 07.03.2023.
//

#include "ModelInfo.h"
#include "../GTASAEngineApi.h"
#include "log.h"

#include <cstring>

CVehicleModelInfo* CModelInfo::AddVehicleModel(int index)
{
    auto& pInfo = CModelInfo::ms_vehicleModelInfoStore.AddItem();

    GTASAEngineApi::ConstructVehicleModelInfoNative(&pInfo);

    CModelInfo::SetModelInfo(index, &pInfo);
    return &pInfo;
}

CPedModelInfo* CModelInfo::AddPedModel(int index)
{

    auto& pInfo = CModelInfo::ms_pedModelInfoStore.AddItem();

    GTASAEngineApi::ConstructPedModelInfoNative(&pInfo);

    CModelInfo::SetModelInfo(index, &pInfo);
    return &pInfo;
}

CDamageAtomicModelInfo* CModelInfo::AddDamageAtomicModel(int32 index)
{
    return GTASAEngineApi::AddDamageAtomicModelNative(index);
}

CAtomicModelInfo* CModelInfo::AddAtomicModel(int index)
{
    auto& pInfo = ms_atomicModelInfoStore.AddItem();

    GTASAEngineApi::ConstructAtomicModelInfoNative(&pInfo);

    CModelInfo::SetModelInfo(index, &pInfo);
    return &pInfo;
}

void CModelInfo::Initialise() {
    memset(ms_modelInfoPtrs, 0, sizeof(ms_modelInfoPtrs));

}

void CModelInfo::injectHooks()
{
    GTASAEngineApi::InstallModelInfoHooksNative(
        &CModelInfo::ms_atomicModelInfoStore,
        &CModelInfo::ms_pedModelInfoStore,
        &CModelInfo::ms_vehicleModelInfoStore,
        &CModelInfo::ms_modelInfoPtrs,
        reinterpret_cast<uintptr_t>(&CModelInfo::AddPedModel),
        reinterpret_cast<uintptr_t>(&CModelInfo::AddVehicleModel),
        reinterpret_cast<uintptr_t>(&CModelInfo::AddAtomicModel));
}
