//
// Created by x1y2z on 31.07.2023.
//

#include "AtomicModelInfo.h"
#include "../GTASAEngineApi.h"

void SetAtomicModelInfoFlags(CAtomicModelInfo* modelInfo, uint32 dwFlags) {
    GTASAEngineApi::SetAtomicModelInfoFlagsNative(modelInfo, dwFlags);
}
