//
// Created by x1y2z on 23.11.2023.
//

#include "NodeName.h"
#include "../../GTASAEngineApi.h"
#include "../../GTASAEngineRwBindings.h"

const RwChar* GetFrameNodeName(RwFrame* frame) {
    return GTASAEngineApi::GetFrameNodeNameNative(frame);
}
