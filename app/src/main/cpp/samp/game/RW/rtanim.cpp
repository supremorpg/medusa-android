#include "RenderWare.h"
#include "game/common.h"
#include "../GTASAEngineApi.h"
#include "../GTASAEngineAnimationBindings.h"

void RtAnimAnimationFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    GTASAEngineApi::RtAnimAnimationFreeListCreateParamsNative(blockSize, numBlocksToPrealloc);
}

RwBool RtAnimInitialize() {
    return GTASAEngineApi::RtAnimInitializeNative();
}

RwBool RtAnimRegisterInterpolationScheme(RtAnimInterpolatorInfo* interpolatorInfo) {
    return GTASAEngineApi::RtAnimRegisterInterpolationSchemeNative(interpolatorInfo);
}

RtAnimInterpolatorInfo* RtAnimGetInterpolatorInfo(RwInt32 typeID) {
    return GTASAEngineApi::RtAnimGetInterpolatorInfoNative(typeID);
}

RtAnimAnimation* RtAnimAnimationCreate(RwInt32 typeID, RwInt32 numFrames, RwInt32 flags, RwReal duration) {
    return GTASAEngineApi::RtAnimAnimationCreateNative(typeID, numFrames, flags, duration);
}

RwBool RtAnimAnimationDestroy(RtAnimAnimation* animation) {
    return GTASAEngineApi::RtAnimAnimationDestroyNative(animation);
}

RtAnimAnimation* RtAnimAnimationRead(const RwChar* filename) {
    return GTASAEngineApi::RtAnimAnimationReadNative(filename);
}

RwBool RtAnimAnimationWrite(const RtAnimAnimation* animation, const RwChar* filename) {
    return GTASAEngineApi::RtAnimAnimationWriteNative(animation, filename);
}

RtAnimAnimation* RtAnimAnimationStreamRead(RwStream* stream) {
    return GTASAEngineApi::RtAnimAnimationStreamReadNative(stream);
}

RwBool RtAnimAnimationStreamWrite(const RtAnimAnimation* animation, RwStream* stream) {
    return GTASAEngineApi::RtAnimAnimationStreamWriteNative(animation, stream);
}

RwInt32 RtAnimAnimationStreamGetSize(const RtAnimAnimation* animation) {
    return GTASAEngineApi::RtAnimAnimationStreamGetSizeNative(animation);
}

RwUInt32 RtAnimAnimationGetNumNodes(const RtAnimAnimation* animation) {
    return GTASAEngineApi::RtAnimAnimationGetNumNodesNative(animation);
}

RtAnimInterpolator* RtAnimInterpolatorCreate(RwInt32 numNodes, RwInt32 maxInterpKeyFrameSize) {
    return GTASAEngineApi::RtAnimInterpolatorCreateNative(numNodes, maxInterpKeyFrameSize);
}

void RtAnimInterpolatorDestroy(RtAnimInterpolator* anim) {
    GTASAEngineApi::RtAnimInterpolatorDestroyNative(anim);
}

RwBool RtAnimInterpolatorSetCurrentAnim(RtAnimInterpolator* animI, RtAnimAnimation* anim) {
    return GTASAEngineApi::RtAnimInterpolatorSetCurrentAnimNative(animI, anim);
}

RwBool RtAnimInterpolatorSetKeyFrameCallBacks(RtAnimInterpolator* anim, RwInt32 keyFrameTypeID) {
    return GTASAEngineApi::RtAnimInterpolatorSetKeyFrameCallBacksNative(anim, keyFrameTypeID);
}

void RtAnimInterpolatorSetAnimLoopCallBack(RtAnimInterpolator* anim, RtAnimCallBack callBack, void* data) {
    GTASAEngineApi::RtAnimInterpolatorSetAnimLoopCallBackNative(anim, callBack, data);
}

void RtAnimInterpolatorSetAnimCallBack(RtAnimInterpolator* anim, RtAnimCallBack callBack, RwReal time, void* data) {
    GTASAEngineApi::RtAnimInterpolatorSetAnimCallBackNative(anim, callBack, time, data);
}

RwBool RtAnimInterpolatorCopy(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim) {
    return GTASAEngineApi::RtAnimInterpolatorCopyNative(outAnim, inAnim);
}

RwBool RtAnimInterpolatorSubAnimTime(RtAnimInterpolator* anim, RwReal time) {
    return GTASAEngineApi::RtAnimInterpolatorSubAnimTimeNative(anim, time);
}

RwBool RtAnimInterpolatorAddAnimTime(RtAnimInterpolator* anim, RwReal time) {
    return GTASAEngineApi::RtAnimInterpolatorAddAnimTimeNative(anim, time);
}

RwBool RtAnimInterpolatorSetCurrentTime(RtAnimInterpolator* anim, RwReal time) {
    return GTASAEngineApi::RtAnimInterpolatorSetCurrentTimeNative(anim, time);
}

RwBool RtAnimAnimationMakeDelta(RtAnimAnimation* animation, RwInt32 numNodes, RwReal time) {
    return GTASAEngineApi::RtAnimAnimationMakeDeltaNative(animation, numNodes, time);
}

RwBool RtAnimInterpolatorBlend(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1, RtAnimInterpolator* inAnim2, RwReal alpha) {
    return GTASAEngineApi::RtAnimInterpolatorBlendNative(outAnim, inAnim1, inAnim2, alpha);
}

RwBool RtAnimInterpolatorAddTogether(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1, RtAnimInterpolator* inAnim2) {
    return GTASAEngineApi::RtAnimInterpolatorAddTogetherNative(outAnim, inAnim1, inAnim2);
}

RtAnimInterpolator* RtAnimInterpolatorCreateSubInterpolator(RtAnimInterpolator* parentAnim, RwInt32 startNode, RwInt32 numNodes, RwInt32 maxInterpKeyFrameSize) {
    return GTASAEngineApi::RtAnimInterpolatorCreateSubInterpolatorNative(parentAnim, startNode, numNodes, maxInterpKeyFrameSize);
}

RwBool RtAnimInterpolatorBlendSubInterpolator(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1, RtAnimInterpolator* inAnim2, RwReal alpha) {
    return GTASAEngineApi::RtAnimInterpolatorBlendSubInterpolatorNative(outAnim, inAnim1, inAnim2, alpha);
}

RwBool RtAnimInterpolatorAddSubInterpolator(RtAnimInterpolator* outAnim, RtAnimInterpolator* mainAnim, RtAnimInterpolator* subAnim) {
    return GTASAEngineApi::RtAnimInterpolatorAddSubInterpolatorNative(outAnim, mainAnim, subAnim);
}
