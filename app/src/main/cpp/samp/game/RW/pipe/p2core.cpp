//
// Created by x1y2z on 02.05.2024.
//

#include "p2core.h"
#include "../../GTASAEngineApi.h"
#include "../../GTASAEngineRwBindings.h"

RxPipeline* RxPipelineCreate() {
    return GTASAEngineApi::RxPipelineCreateNative();
}
