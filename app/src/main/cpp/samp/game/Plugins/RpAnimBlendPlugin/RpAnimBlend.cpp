#include "RpAnimBlend.h"
#include "../../GTASAEngineApi.h"
#include "../../GTASAEngineAnimationBindings.h"

// 0x4D6150
bool RpAnimBlendPluginAttach() {
    return GTASAEngineApi::RpAnimBlendPluginAttachNative();
}

// 0x4D5FA0
void* RtAnimBlendKeyFrameApply(void* result, void* frame) {
    return GTASAEngineApi::RtAnimBlendKeyFrameApplyNative(result, frame);
}

// 0x4D5F50
CAnimBlendClumpData* RpAnimBlendAllocateData(RpClump* clump) {
    return GTASAEngineApi::RpAnimBlendAllocateDataNative(clump);
}

// 0x4D6790
CAnimBlendAssociation* RpAnimBlendClumpAddAssociation(RpClump* clump, CAnimBlendAssociation* association, uint32 flags, float startTime, float blendAmount) {
    return GTASAEngineApi::RpAnimBlendClumpAddAssociationNative(clump, association, flags, startTime, blendAmount);
}

// 0x4D6BE0
CAnimBlendAssociation* RpAnimBlendClumpExtractAssociations(RpClump* clump) {
    int *v1; // r1
    int v2; // r3
    int result; // r0

    v1 = *(int **)(clump + ClumpOffset);
    v2 = *v1;
    *v1 = 0;
    result = v2 - 4;
    *(uint32_t *)(v2 + 4) = 0;
    return reinterpret_cast<CAnimBlendAssociation *>(result);
}

// 0x4D64A0
void RpAnimBlendClumpFillFrameArray(RpClump* clump, AnimBlendFrameData** frameData) {
    GTASAEngineApi::RpAnimBlendClumpFillFrameArrayNative(clump, frameData);
}

// 0x4D6400
AnimBlendFrameData* RpAnimBlendClumpFindBone(RpClump* clump, uint32 id) {
    return GTASAEngineApi::RpAnimBlendClumpFindBoneNative(clump, id);
}

// 0x4D62A0
AnimBlendFrameData* RpAnimBlendClumpFindFrame(RpClump* clump, const char* name) {
    return GTASAEngineApi::RpAnimBlendClumpFindFrameNative(clump, name);
}

// 0x4D6370
AnimBlendFrameData* RpAnimBlendClumpFindFrameFromHashKey(RpClump* clump, uint32 key) {
    return GTASAEngineApi::RpAnimBlendClumpFindFrameFromHashKeyNative(clump, key);
}

// 0x4D68E0
CAnimBlendAssociation* RpAnimBlendClumpGetAssociation(RpClump* clump, bool bStopFunctionConfusion, CAnimBlendHierarchy* hierarchy) {
    return GTASAEngineApi::RpAnimBlendClumpGetAssociationByHierarchyNative(clump, bStopFunctionConfusion, hierarchy);
}

// 0x4D6870
CAnimBlendAssociation* RpAnimBlendClumpGetAssociation(RpClump* clump, const char* name) {
    return GTASAEngineApi::RpAnimBlendClumpGetAssociationByNameNative(clump, name);
}

// AnimationId animId
// 0x4D68B0
CAnimBlendAssociation* RpAnimBlendClumpGetAssociation(RpClump* clump, uint32 animId) {
    return GTASAEngineApi::RpAnimBlendClumpGetAssociationByAnimIdNative(clump, animId);
}

// 0x4D15E0
CAnimBlendAssociation* RpAnimBlendClumpGetFirstAssociation(RpClump* clump) {
    int *v4; // r4
    int result; // r0

    v4 = *(int **)(clump + ClumpOffset);
    result = RpAnimBlendClumpIsInitialized(clump);
    if ( result )
    {
        result = *v4;
        if ( *v4 )
            result -= 4;
    }
    return reinterpret_cast<CAnimBlendAssociation *>(result);
}

// 0x4D6A70
CAnimBlendAssociation* RpAnimBlendClumpGetFirstAssociation(RpClump* clump, uint32 flags) {
    return GTASAEngineApi::RpAnimBlendClumpGetFirstAssociationByFlagsNative(clump, flags);
}

// 0x4D6910
CAnimBlendAssociation* RpAnimBlendClumpGetMainAssociation(RpClump* clump, CAnimBlendAssociation** pp2ndAnim, float* pBlendVal2nd) {
    return GTASAEngineApi::RpAnimBlendClumpGetMainAssociationNative(clump, pp2ndAnim, pBlendVal2nd);
}

// 0x4D6A30
CAnimBlendAssociation* RpAnimBlendClumpGetMainAssociation_N(RpClump* clump, int32 n) {
    return GTASAEngineApi::RpAnimBlendClumpGetMainAssociationNNative(clump, n);
}

// 0x4D69A0
CAnimBlendAssociation* RpAnimBlendClumpGetMainPartialAssociation(RpClump* clump) {
    return GTASAEngineApi::RpAnimBlendClumpGetMainPartialAssociationNative(clump);
}

// 0x4D69F0
CAnimBlendAssociation* RpAnimBlendClumpGetMainPartialAssociation_N(RpClump* clump, int32 n) {
    return GTASAEngineApi::RpAnimBlendClumpGetMainPartialAssociationNNative(clump, n);
}

// 0x4D6B60
uint32 RpAnimBlendClumpGetNumAssociations(RpClump* clump) {
    return GTASAEngineApi::RpAnimBlendClumpGetNumAssociationsNative(clump);
}

// 0x4D6BB0
uint32 RpAnimBlendClumpGetNumNonPartialAssociations(RpClump* clump) {
    return GTASAEngineApi::RpAnimBlendClumpGetNumNonPartialAssociationsNative(clump);
}

// 0x4D6B80
uint32 RpAnimBlendClumpGetNumPartialAssociations(RpClump* clump) {
    return GTASAEngineApi::RpAnimBlendClumpGetNumPartialAssociationsNative(clump);
}

// 0x4D6C30
void RpAnimBlendClumpGiveAssociations(RpClump* clump, CAnimBlendAssociation* association) {
    GTASAEngineApi::RpAnimBlendClumpGiveAssociationsNative(clump, association);
}

// 0x4D6720
void RpAnimBlendClumpInit(RpClump* clump) {
    GTASAEngineApi::RpAnimBlendClumpInitNative(clump);
}

// 0x4D6760
bool RpAnimBlendClumpIsInitialized(RpClump* clump) {
//    int result; // r0
//
//    result = *(uint32 *)(clump + ClumpOffset);
//    if ( result )
//        return *(uint32 *)(result + 8) != 0;
//    return result;

   return GTASAEngineApi::RpAnimBlendClumpIsInitializedNative(clump);
}

// 0x4D6B00
void RpAnimBlendClumpPauseAllAnimations(RpClump* clump) {
    GTASAEngineApi::RpAnimBlendClumpPauseAllAnimationsNative(clump);
}

// 0x4D6C00
void RpAnimBlendClumpRemoveAllAssociations(RpClump* clump) {
    RpAnimBlendClumpRemoveAssociations(clump, 0);
}

// 0x4D6820
void RpAnimBlendClumpRemoveAssociations(RpClump* clump, uint32 flags) {
    GTASAEngineApi::RpAnimBlendClumpRemoveAssociationsNative(clump, flags);
}

// 0x4D67E0
void RpAnimBlendClumpSetBlendDeltas(RpClump* clump, uint32 flags, float delta) {
    GTASAEngineApi::RpAnimBlendClumpSetBlendDeltasNative(clump, flags, delta);
}

// 0x4D6B30
void RpAnimBlendClumpUnPauseAllAnimations(RpClump* clump) {
    GTASAEngineApi::RpAnimBlendClumpUnPauseAllAnimationsNative(clump);
}

// 0x4D34F0
void RpAnimBlendClumpUpdateAnimations(RpClump* clump, float step, bool onScreen) {
    GTASAEngineApi::RpAnimBlendClumpUpdateAnimationsNative(clump, step, onScreen);
}

// 0x4D60E0
RtAnimAnimation* RpAnimBlendCreateAnimationForHierarchy(RpHAnimHierarchy* hierarchy) {
    return GTASAEngineApi::RpAnimBlendCreateAnimationForHierarchyNative(hierarchy);
}

// 0x4D5EF0
char* RpAnimBlendFrameGetName(RwFrame* frame) {
    return GTASAEngineApi::RpAnimBlendFrameGetNameNative(frame);
}

// 0x4D5F00
void RpAnimBlendFrameSetName(RwFrame* frame, char* name) {
    GTASAEngineApi::RpAnimBlendFrameSetNameNative(frame, name);
}

// 0x4D6AB0
CAnimBlendAssociation* RpAnimBlendGetNextAssociation(CAnimBlendAssociation* association) {
    return GTASAEngineApi::RpAnimBlendGetNextAssociationNative(association);
}

// 0x4D6AD0
CAnimBlendAssociation* RpAnimBlendGetNextAssociation(CAnimBlendAssociation* association, uint32 flags) {
    return GTASAEngineApi::RpAnimBlendGetNextAssociationByFlagsNative(association, flags);
}

// 0x4D60C0
void RpAnimBlendKeyFrameInterpolate(void *pVoidOut, void *pVoidIn1, void *pVoidIn2, RwReal time, void *customData) {
    GTASAEngineApi::RpAnimBlendKeyFrameInterpolateNative(pVoidOut, pVoidIn1, pVoidIn2, time, customData);
}
