//
// Created by x1y2z on 11.04.2023.
//

#include "VehicleModelInfo.h"
#include "../GTASAEngineApi.h"
#include "../RW/rpworld.h"
#include "ModelInfo.h"

#include <cstring>

void CVehicleModelInfo::CVehicleModelInfo__SetClump(RpClump* clump)
{
    m_pVehicleStruct = new CVehicleStructure();
    CClumpModelInfo__SetClump(clump);
    SetAtomicRenderCallbacks();

    CClumpModelInfo::SetFrameIds(GTASAEngineApi::VehicleModelInfoDescriptorNative(m_nVehicleType));
    SetRenderPipelines();
    PreprocessHierarchy();
    ReduceMaterialsInVehicle();
//    m_nCurrentPrimaryColor = 255;
//    m_nCurrentSecondaryColor = 255;
//    m_nCurrentTertiaryColor = 255;
//    m_nCurrentQuaternaryColor = 255;
//    SetCarCustomPlate();
}

void CVehicleModelInfo::CVehicleModelInfo__DeleteRwObject()
{
    delete m_pVehicleStruct;
    m_pVehicleStruct = nullptr;
    CClumpModelInfo__DeleteRwObject();
}

void CVehicleModelInfo::SetAtomicRenderCallbacks()
{
   GTASAEngineApi::VehicleModelInfoSetAtomicRenderCallbacksNative(this);
}

CVehicleModelInfo::CVehicleStructure::CVehicleStructure() : m_aUpgrades()
{
    for (auto& vecPos : m_avDummyPos)
        vecPos.Set(0.0F, 0.0F, 0.0F);

    for (auto& upgrade : m_aUpgrades)
        upgrade.m_nParentComponentId = -1;

    memset(m_apExtras, 0, sizeof(m_apExtras));
    m_nNumExtras = 0;
    m_nMaskComponentsDamagable = 0;
}

CVehicleModelInfo::CVehicleStructure::~CVehicleStructure()
{
    for (int32 i = 0; i < m_nNumExtras; ++i) {
        auto atomic = m_apExtras[i];
        auto frame = RpAtomicGetFrame(atomic);
        RpAtomicDestroy(atomic);
        RwFrameDestroy(frame);
    }
}

void CVehicleModelInfo::ReduceMaterialsInVehicle()
{
   GTASAEngineApi::VehicleModelInfoReduceMaterialsNative(this);
}


void CVehicleModelInfo::PreprocessHierarchy()
{
   GTASAEngineApi::VehicleModelInfoPreprocessHierarchyNative(this);
}

void CVehicleModelInfo::SetRenderPipelines()
{
    GTASAEngineApi::VehicleModelInfoSetRenderPipelinesNative(this);
}

void* CVehicleModelInfo::CVehicleStructure::operator new(size_t size)
{
    return m_pInfoPool->New();
}

void CVehicleModelInfo::CVehicleStructure::operator delete(void* data)
{
    m_pInfoPool->Delete(reinterpret_cast<CVehicleStructure*>(data));
}

void CVehicleModelInfo::InjectHooks() {
}
