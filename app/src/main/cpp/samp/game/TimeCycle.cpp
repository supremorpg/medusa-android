//
// Created by x1y2z on 20.09.2023.
//

#include "TimeCycle.h"
#include "GTASAEngineApi.h"

void CTimeCycle::InjectHooks() {
    GTASAEngineApi::BindTimeCycleGlobals(&CTimeCycle::m_CurrentColours, &CTimeCycle::m_BelowHorizonGrey);
}
