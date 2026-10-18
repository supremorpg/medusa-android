//
// Created by x1y2z on 28.04.2024.
//

#include "RxPipeline.h"
#include "../GTASAEngineApi.h"
#include "../GTASAEngineRwBindings.h"
#include "pipe/p2core.h"

RxNodeDefinition* RxNodeDefinitionGetOpenGLAtomicAllInOne() {
    return GTASAEngineApi::RxNodeDefinitionGetOpenGLAtomicAllInOneNative();
}

void RxOpenGLAllInOneSetInstanceCallBack(RxPipelineNode *node, RxOpenGLAllInOneInstanceCallBack instanceCB) {
    GTASAEngineApi::RxOpenGLAllInOneSetInstanceCallBackNative(node, reinterpret_cast<uintptr_t>(instanceCB));
}

void RxOpenGLAllInOneSetRenderCallBack(RxPipelineNode* node, RxOpenGLAllInOneRenderCallBack renderCB) {
    GTASAEngineApi::RxOpenGLAllInOneSetRenderCallBackNative(node, reinterpret_cast<uintptr_t>(renderCB));
}

void _rxPipelineDestroy(RxPipeline *Pipeline) {
    GTASAEngineApi::RxPipelineDestroyNative(Pipeline);
}
