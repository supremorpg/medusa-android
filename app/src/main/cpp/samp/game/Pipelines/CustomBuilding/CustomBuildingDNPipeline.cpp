//
// Created by x1y2z on 25.11.2023.
//

#include "CustomBuildingDNPipeline.h"
#include "../../GTASAEngineApi.h"

void CCustomBuildingDNPipeline::InjectHooks() {
    GTASAEngineApi::BindCustomBuildingDNBalance(&m_fDNBalanceParam);
}
