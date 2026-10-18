
#include "rphanim.h"
#include "game/common.h"
#include "../GTASAEngineApi.h"
#include "../GTASAEngineAnimationBindings.h"

void RpHAnimHierarchySetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    GTASAEngineApi::RpHAnimHierarchySetFreeListCreateParamsNative(blockSize, numBlocksToPrealloc);
}

RpHAnimHierarchy* RpHAnimHierarchyCreate(RwInt32 numNodes, RwUInt32* nodeFlags, RwInt32* nodeIDs, RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize) {
    return GTASAEngineApi::RpHAnimHierarchyCreateNative(numNodes, nodeFlags, nodeIDs, flags, maxInterpKeyFrameSize);
}

RpHAnimHierarchy* RpHAnimHierarchyCreateFromHierarchy(RpHAnimHierarchy* hierarchy, RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize) {
    return GTASAEngineApi::RpHAnimHierarchyCreateFromHierarchyNative(hierarchy, flags, maxInterpKeyFrameSize);
}

RpHAnimHierarchy* RpHAnimHierarchyDestroy(RpHAnimHierarchy* hierarchy) {
    return GTASAEngineApi::RpHAnimHierarchyDestroyNative(hierarchy);
}

RpHAnimHierarchy* RpHAnimHierarchyCreateSubHierarchy(RpHAnimHierarchy* parentHierarchy, RwInt32 startNode, RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize) {
    return GTASAEngineApi::RpHAnimHierarchyCreateSubHierarchyNative(parentHierarchy, startNode, flags, maxInterpKeyFrameSize);
}

RpHAnimHierarchy* RpHAnimHierarchyAttach(RpHAnimHierarchy* hierarchy) {
    return GTASAEngineApi::RpHAnimHierarchyAttachNative(hierarchy);
}

RpHAnimHierarchy* RpHAnimHierarchyDetach(RpHAnimHierarchy* hierarchy) {
    return GTASAEngineApi::RpHAnimHierarchyDetachNative(hierarchy);
}

RpHAnimHierarchy* RpHAnimHierarchyAttachFrameIndex(RpHAnimHierarchy* hierarchy, RwInt32 nodeIndex) {
    return GTASAEngineApi::RpHAnimHierarchyAttachFrameIndexNative(hierarchy, nodeIndex);
}

RpHAnimHierarchy* RpHAnimHierarchyDetachFrameIndex(RpHAnimHierarchy* hierarchy, RwInt32 nodeIndex) {
    return GTASAEngineApi::RpHAnimHierarchyDetachFrameIndexNative(hierarchy, nodeIndex);
}

RwBool RpHAnimFrameSetHierarchy(RwFrame* frame, RpHAnimHierarchy* hierarchy) {
    return GTASAEngineApi::RpHAnimFrameSetHierarchyNative(frame, hierarchy);
}

RpHAnimHierarchy* RpHAnimFrameGetHierarchy(RwFrame* frame) {
    return GTASAEngineApi::RpHAnimFrameGetHierarchyNative(frame);
}

RwMatrix* RpHAnimHierarchyGetMatrixArray(RpHAnimHierarchy* hierarchy) {
    return GTASAEngineApi::RpHAnimHierarchyGetMatrixArrayNative(hierarchy);
}

RwBool RpHAnimHierarchyUpdateMatrices(RpHAnimHierarchy* hierarchy) {
    return GTASAEngineApi::RpHAnimHierarchyUpdateMatricesNative(hierarchy);
}

RwInt32 RpHAnimIDGetIndex(RpHAnimHierarchy* hierarchy, RwInt32 ID) {
    return GTASAEngineApi::RpHAnimIDGetIndexNative(hierarchy, ID);
}

RwBool RpHAnimPluginAttach() {
    return GTASAEngineApi::RpHAnimPluginAttachNative();
}

void RpHAnimKeyFrameApply(void* matrix, void* voidIFrame) {
    GTASAEngineApi::RpHAnimKeyFrameApplyNative(matrix, voidIFrame);
}

void RpHAnimKeyFrameBlend(void* voidOut, void* voidIn1, void* voidIn2, RwReal alpha) {
    GTASAEngineApi::RpHAnimKeyFrameBlendNative(voidOut, voidIn1, voidIn2, alpha);
}

void RpHAnimKeyFrameInterpolate(void* voidOut, void* voidIn1, void* voidIn2, RwReal time, void* customData) {
    GTASAEngineApi::RpHAnimKeyFrameInterpolateNative(voidOut, voidIn1, voidIn2, time, customData);
}

void RpHAnimKeyFrameAdd(void* voidOut, void* voidIn1, void* voidIn2) {
    GTASAEngineApi::RpHAnimKeyFrameAddNative(voidOut, voidIn1, voidIn2);
}

void RpHAnimKeyFrameMulRecip(void* voidFrame, void* voidStart) {
    GTASAEngineApi::RpHAnimKeyFrameMulRecipNative(voidFrame, voidStart);
}

RtAnimAnimation* RpHAnimKeyFrameStreamRead(RwStream* stream, RtAnimAnimation* animation) {
    return GTASAEngineApi::RpHAnimKeyFrameStreamReadNative(stream, animation);
}

RwBool RpHAnimKeyFrameStreamWrite(const RtAnimAnimation* animation, RwStream* stream) {
    return GTASAEngineApi::RpHAnimKeyFrameStreamWriteNative(animation, stream);
}

RwInt32 RpHAnimKeyFrameStreamGetSize(const RtAnimAnimation* animation) {
    return GTASAEngineApi::RpHAnimKeyFrameStreamGetSizeNative(animation);
}

RwBool RpHAnimFrameSetID(RwFrame* frame, RwInt32 id) {
    return GTASAEngineApi::RpHAnimFrameSetIDNative(frame, id);
}

RwInt32 RpHAnimFrameGetID(RwFrame* frame) {
    return GTASAEngineApi::RpHAnimFrameGetIDNative(frame);
}
