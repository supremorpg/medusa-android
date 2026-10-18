#pragma once

#include "common.h"

class CAnimBlendAssocGroup;
class CAnimBlendAssociation;
class CAnimBlendClumpData;
class CAnimBlendHierarchy;
class CAnimBlendNode;
class CAnimBlendStaticAssociation;
class CQuaternion;
class CVector;
class AnimBlendFrameData;
struct AnimAssocDefinition;
struct AnimDescriptor;
struct RpClump;
struct RpHAnimHierarchy;
struct RtAnimAnimation;
struct RtAnimInterpolator;
struct RtAnimInterpolatorInfo;
struct RwFrame;
struct RwMatrixTag;
struct RwStream;

typedef RwMatrixTag RwMatrix;
typedef float RwReal;

namespace GTASAEngineApi {

using RtAnimCallbackFn = RtAnimInterpolator* (*)(RtAnimInterpolator*, void*);

void AnimBlendHierarchyMoveMemoryNative(CAnimBlendHierarchy* hierarchy);
void ConstructAnimBlendStaticAssociationNative(CAnimBlendStaticAssociation* association,
                                               RpClump* clump,
                                               CAnimBlendHierarchy* hierarchy);
void InitAnimBlendStaticAssociationNative(CAnimBlendStaticAssociation* association,
                                          RpClump* clump,
                                          CAnimBlendHierarchy* hierarchy);
bool AnimBlendNodeNextKeyFrameNative(CAnimBlendNode* node);
bool AnimBlendNodeNextKeyFrameCompressedNative(CAnimBlendNode* node);
bool AnimBlendNodeNextKeyFrameNoCalcNative(CAnimBlendNode* node);
bool AnimBlendNodeUpdateNative(CAnimBlendNode* node, CVector& trans, CQuaternion& rot, float weight);
bool AnimBlendNodeUpdateCompressedNative(CAnimBlendNode* node, CVector& trans, CQuaternion& rot, float weight);
void ConstructAnimBlendAssocGroupNative(CAnimBlendAssocGroup* group);
CAnimBlendAssociation* CopyAnimBlendAssocGroupAnimationNative(CAnimBlendAssocGroup* group, const char* animName);
CAnimBlendAssociation* CopyAnimBlendAssocGroupAnimationNative(CAnimBlendAssocGroup* group, uint32 animId);
void CreateAnimBlendAssociationsNative(const char* blockName);
void CreateAnimBlendAssociationsNative(CAnimBlendAssocGroup* group,
                                       const char* animName,
                                       RpClump* clump,
                                       char** names,
                                       int32 animationCount);
void CreateAnimBlendAssociationsNative(CAnimBlendAssocGroup* group,
                                       const char* animName,
                                       const char* arg2,
                                       const char* arg3,
                                       uint32 storageSize);
void DestroyAnimBlendAssociationsNative(CAnimBlendAssocGroup* group);
CAnimBlendStaticAssociation* GetAnimBlendAssocGroupAnimationNative(CAnimBlendAssocGroup* group,
                                                                   const char* animName);
uint32 GetAnimBlendAssocGroupAnimationIdNative(CAnimBlendAssocGroup* group, const char* animName);
void InitEmptyAnimBlendAssociationsNative(CAnimBlendAssocGroup* group, RpClump* clump);
void DestroyAnimBlendAssocGroupNative(CAnimBlendAssocGroup* group);
void RtAnimAnimationFreeListCreateParamsNative(int32 blockSize, int32 numBlocksToPrealloc);
int32 RtAnimInitializeNative();
int32 RtAnimRegisterInterpolationSchemeNative(RtAnimInterpolatorInfo* interpolatorInfo);
RtAnimInterpolatorInfo* RtAnimGetInterpolatorInfoNative(int32 typeID);
RtAnimAnimation* RtAnimAnimationCreateNative(int32 typeID, int32 numFrames, int32 flags, RwReal duration);
int32 RtAnimAnimationDestroyNative(RtAnimAnimation* animation);
RtAnimAnimation* RtAnimAnimationReadNative(const char* filename);
int32 RtAnimAnimationWriteNative(const RtAnimAnimation* animation, const char* filename);
RtAnimAnimation* RtAnimAnimationStreamReadNative(RwStream* stream);
int32 RtAnimAnimationStreamWriteNative(const RtAnimAnimation* animation, RwStream* stream);
int32 RtAnimAnimationStreamGetSizeNative(const RtAnimAnimation* animation);
uint32 RtAnimAnimationGetNumNodesNative(const RtAnimAnimation* animation);
RtAnimInterpolator* RtAnimInterpolatorCreateNative(int32 numNodes, int32 maxInterpKeyFrameSize);
void RtAnimInterpolatorDestroyNative(RtAnimInterpolator* anim);
int32 RtAnimInterpolatorSetCurrentAnimNative(RtAnimInterpolator* animI, RtAnimAnimation* anim);
int32 RtAnimInterpolatorSetKeyFrameCallBacksNative(RtAnimInterpolator* anim, int32 keyFrameTypeID);
void RtAnimInterpolatorSetAnimLoopCallBackNative(RtAnimInterpolator* anim, RtAnimCallbackFn callBack, void* data);
void RtAnimInterpolatorSetAnimCallBackNative(RtAnimInterpolator* anim,
                                             RtAnimCallbackFn callBack,
                                             RwReal time,
                                             void* data);
int32 RtAnimInterpolatorCopyNative(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim);
int32 RtAnimInterpolatorSubAnimTimeNative(RtAnimInterpolator* anim, RwReal time);
int32 RtAnimInterpolatorAddAnimTimeNative(RtAnimInterpolator* anim, RwReal time);
int32 RtAnimInterpolatorSetCurrentTimeNative(RtAnimInterpolator* anim, RwReal time);
int32 RtAnimAnimationMakeDeltaNative(RtAnimAnimation* animation, int32 numNodes, RwReal time);
int32 RtAnimInterpolatorBlendNative(RtAnimInterpolator* outAnim,
                                    RtAnimInterpolator* inAnim1,
                                    RtAnimInterpolator* inAnim2,
                                    RwReal alpha);
int32 RtAnimInterpolatorAddTogetherNative(RtAnimInterpolator* outAnim,
                                          RtAnimInterpolator* inAnim1,
                                          RtAnimInterpolator* inAnim2);
RtAnimInterpolator* RtAnimInterpolatorCreateSubInterpolatorNative(RtAnimInterpolator* parentAnim,
                                                                  int32 startNode,
                                                                  int32 numNodes,
                                                                  int32 maxInterpKeyFrameSize);
int32 RtAnimInterpolatorBlendSubInterpolatorNative(RtAnimInterpolator* outAnim,
                                                   RtAnimInterpolator* inAnim1,
                                                   RtAnimInterpolator* inAnim2,
                                                   RwReal alpha);
int32 RtAnimInterpolatorAddSubInterpolatorNative(RtAnimInterpolator* outAnim,
                                                 RtAnimInterpolator* mainAnim,
                                                 RtAnimInterpolator* subAnim);
void RpHAnimHierarchySetFreeListCreateParamsNative(int32 blockSize, int32 numBlocksToPrealloc);
RpHAnimHierarchy* RpHAnimHierarchyCreateNative(int32 numNodes,
                                               uint32* nodeFlags,
                                               int32* nodeIDs,
                                               int32 flags,
                                               int32 maxInterpKeyFrameSize);
RpHAnimHierarchy* RpHAnimHierarchyCreateFromHierarchyNative(RpHAnimHierarchy* hierarchy,
                                                            int32 flags,
                                                            int32 maxInterpKeyFrameSize);
RpHAnimHierarchy* RpHAnimHierarchyDestroyNative(RpHAnimHierarchy* hierarchy);
RpHAnimHierarchy* RpHAnimHierarchyCreateSubHierarchyNative(RpHAnimHierarchy* parentHierarchy,
                                                           int32 startNode,
                                                           int32 flags,
                                                           int32 maxInterpKeyFrameSize);
RpHAnimHierarchy* RpHAnimHierarchyAttachNative(RpHAnimHierarchy* hierarchy);
RpHAnimHierarchy* RpHAnimHierarchyDetachNative(RpHAnimHierarchy* hierarchy);
RpHAnimHierarchy* RpHAnimHierarchyAttachFrameIndexNative(RpHAnimHierarchy* hierarchy, int32 nodeIndex);
RpHAnimHierarchy* RpHAnimHierarchyDetachFrameIndexNative(RpHAnimHierarchy* hierarchy, int32 nodeIndex);
int32 RpHAnimFrameSetHierarchyNative(RwFrame* frame, RpHAnimHierarchy* hierarchy);
RpHAnimHierarchy* RpHAnimFrameGetHierarchyNative(RwFrame* frame);
RwMatrix* RpHAnimHierarchyGetMatrixArrayNative(RpHAnimHierarchy* hierarchy);
int32 RpHAnimHierarchyUpdateMatricesNative(RpHAnimHierarchy* hierarchy);
int32 RpHAnimIDGetIndexNative(RpHAnimHierarchy* hierarchy, int32 id);
int32 RpHAnimPluginAttachNative();
void RpHAnimKeyFrameApplyNative(void* matrix, void* voidIFrame);
void RpHAnimKeyFrameBlendNative(void* voidOut, void* voidIn1, void* voidIn2, RwReal alpha);
void RpHAnimKeyFrameInterpolateNative(void* voidOut,
                                      void* voidIn1,
                                      void* voidIn2,
                                      RwReal time,
                                      void* customData);
void RpHAnimKeyFrameAddNative(void* voidOut, void* voidIn1, void* voidIn2);
void RpHAnimKeyFrameMulRecipNative(void* voidFrame, void* voidStart);
RtAnimAnimation* RpHAnimKeyFrameStreamReadNative(RwStream* stream, RtAnimAnimation* animation);
int32 RpHAnimKeyFrameStreamWriteNative(const RtAnimAnimation* animation, RwStream* stream);
int32 RpHAnimKeyFrameStreamGetSizeNative(const RtAnimAnimation* animation);
int32 RpHAnimFrameSetIDNative(RwFrame* frame, int32 id);
int32 RpHAnimFrameGetIDNative(RwFrame* frame);
bool RpAnimBlendPluginAttachNative();
void* RtAnimBlendKeyFrameApplyNative(void* result, void* frame);
CAnimBlendClumpData* RpAnimBlendAllocateDataNative(RpClump* clump);
CAnimBlendAssociation* RpAnimBlendClumpAddAssociationNative(RpClump* clump,
                                                            CAnimBlendAssociation* association,
                                                            uint32 flags,
                                                            RwReal startTime,
                                                            RwReal blendAmount);
void RpAnimBlendClumpFillFrameArrayNative(RpClump* clump, AnimBlendFrameData** frameData);
AnimBlendFrameData* RpAnimBlendClumpFindBoneNative(RpClump* clump, uint32 id);
AnimBlendFrameData* RpAnimBlendClumpFindFrameNative(RpClump* clump, const char* name);
AnimBlendFrameData* RpAnimBlendClumpFindFrameFromHashKeyNative(RpClump* clump, uint32 key);
CAnimBlendAssociation* RpAnimBlendClumpGetAssociationByHierarchyNative(RpClump* clump,
                                                                       bool stopFunctionConfusion,
                                                                       CAnimBlendHierarchy* hierarchy);
CAnimBlendAssociation* RpAnimBlendClumpGetAssociationByNameNative(RpClump* clump, const char* name);
CAnimBlendAssociation* RpAnimBlendClumpGetAssociationByAnimIdNative(RpClump* clump, uint32 animId);
CAnimBlendAssociation* RpAnimBlendClumpGetFirstAssociationByFlagsNative(RpClump* clump, uint32 flags);
CAnimBlendAssociation* RpAnimBlendClumpGetMainAssociationNative(RpClump* clump,
                                                                CAnimBlendAssociation** pp2ndAnim,
                                                                RwReal* pBlendVal2nd);
CAnimBlendAssociation* RpAnimBlendClumpGetMainAssociationNNative(RpClump* clump, int32 n);
CAnimBlendAssociation* RpAnimBlendClumpGetMainPartialAssociationNative(RpClump* clump);
CAnimBlendAssociation* RpAnimBlendClumpGetMainPartialAssociationNNative(RpClump* clump, int32 n);
uint32 RpAnimBlendClumpGetNumAssociationsNative(RpClump* clump);
uint32 RpAnimBlendClumpGetNumNonPartialAssociationsNative(RpClump* clump);
uint32 RpAnimBlendClumpGetNumPartialAssociationsNative(RpClump* clump);
void RpAnimBlendClumpGiveAssociationsNative(RpClump* clump, CAnimBlendAssociation* association);
void RpAnimBlendClumpInitNative(RpClump* clump);
bool RpAnimBlendClumpIsInitializedNative(RpClump* clump);
void RpAnimBlendClumpPauseAllAnimationsNative(RpClump* clump);
void RpAnimBlendClumpRemoveAssociationsNative(RpClump* clump, uint32 flags);
void RpAnimBlendClumpSetBlendDeltasNative(RpClump* clump, uint32 flags, RwReal delta);
void RpAnimBlendClumpUnPauseAllAnimationsNative(RpClump* clump);
void RpAnimBlendClumpUpdateAnimationsNative(RpClump* clump, RwReal step, bool onScreen);
RtAnimAnimation* RpAnimBlendCreateAnimationForHierarchyNative(RpHAnimHierarchy* hierarchy);
char* RpAnimBlendFrameGetNameNative(RwFrame* frame);
void RpAnimBlendFrameSetNameNative(RwFrame* frame, char* name);
CAnimBlendAssociation* RpAnimBlendGetNextAssociationNative(CAnimBlendAssociation* association);
CAnimBlendAssociation* RpAnimBlendGetNextAssociationByFlagsNative(CAnimBlendAssociation* association, uint32 flags);
void RpAnimBlendKeyFrameInterpolateNative(void* pVoidOut,
                                          void* pVoidIn1,
                                          void* pVoidIn2,
                                          RwReal time,
                                          void* customData);
void ReadAnimAssociationDefinitionsNative();
CAnimBlendAssociation* AddAnimationNative(RpClump* clump, int32 groupId, int32 animId);
CAnimBlendAssociation* AddAnimationByHierarchyNative(RpClump* clump,
                                                     CAnimBlendHierarchy* hier,
                                                     int32 clumpAssocFlag);
CAnimBlendAssociation* AddAnimationAndSyncNative(RpClump* clump,
                                                 CAnimBlendAssociation* animBlendAssoc,
                                                 int32 groupId,
                                                 int32 animId);
AnimAssocDefinition* AddAnimAssocDefinitionNative(const char* groupName,
                                                  const char* blockName,
                                                  uint32 modelIndex,
                                                  uint32 animsCount,
                                                  AnimDescriptor* descriptor);
void RemoveLastAnimFileNative();
void RemoveAnimBlockNative(int32 index);
void UncompressAnimationNative(CAnimBlendHierarchy* hier);
void RemoveFromUncompressedCacheNative(CAnimBlendHierarchy* hier);
CAnimBlendAssociation* BlendAnimationByIdNative(RpClump* clump,
                                                int32 groupId,
                                                int32 animId,
                                                float blendData);
void InstallAnimManagerHooksNative(void* numAnimAssocDefinitions,
                                   void* animBlocks,
                                   void* numAnimBlocks,
                                   void* animAssocGroups,
                                   void* animations,
                                   void* numAnimations,
                                   void* animCache,
                                   uintptr_t getAssocByIdHook,
                                   uintptr_t getAssocByNameHook,
                                   uintptr_t initialiseHook,
                                   uintptr_t loadAnimFilesHook);

}
