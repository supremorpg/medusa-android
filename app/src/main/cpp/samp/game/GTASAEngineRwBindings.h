#pragma once

#include <cstdint>

#include "common.h"

class CRect;
struct RpAtomic;
struct RpClump;
struct RpGeometry;
struct RpHAnimHierarchy;
struct RpLight;
struct RpMaterial;
struct RpSkin;
struct RpWorld;
struct RsGlobalType;
struct RtQuat;
struct RwBBox;
struct RwCamera;
struct RwFrame;
struct RwMatrixTag;
struct RwObject;
struct RwRaster;
struct RwRGBA;
struct RwRGBAReal;
struct RwStream;
struct RwTexture;
struct RwV2d;
struct RwV3d;
struct RxNodeDefinition;
struct RxPipeline;
struct RxPipelineNode;

typedef RwMatrixTag RwMatrix;
typedef float RwReal;

namespace GTASAEngineApi {

using RpAtomicCallbackFn = RpAtomic* (*)(RpAtomic*, void*);
using RpMaterialCallbackFn = RpMaterial* (*)(RpMaterial*, void*);
using RwObjectCallbackFn = RwObject* (*)(RwObject*, void*);
using RwTextureFindCallbackFn = RwTexture* (*)(const char*);
using RwTextureReadCallbackFn = RwTexture* (*)(const char*, const char*);

RpSkin* RpSkinGeometryGetSkinNative(RpGeometry* geometry);
RpHAnimHierarchy* RpSkinAtomicGetHAnimHierarchyNative(const RpAtomic* atomic);
int32 RtQuatConvertFromMatrixNative(RtQuat* quat, const RwMatrix* matrix);
RtQuat* RtQuatRotateNative(RtQuat* quat, const RwV3d* axis, RwReal angle, int32 combineOp);
const RtQuat* RtQuatQueryRotateNative(const RtQuat* quat, RwV3d* unitAxis, RwReal* angle);
RwV3d* RtQuatTransformVectorsNative(RwV3d* vectorsOut,
                                    const RwV3d* vectorsIn,
                                    int32 numPoints,
                                    const RtQuat* quat);
RwReal RtQuatModulusNative(RtQuat* quat);
void DrawRaster(RwRaster* raster, const CRect& rect);
void DrawTexture(uintptr_t texture, CRect* rect, uint32_t color);
void DrawTextureUV(uintptr_t texture, CRect* rect, uint32_t color, const float* uv);
void RenderRwClump(uintptr_t rwObject);
int32 RwMatrixDestroyNative(RwMatrix* matrix);
RwV3d* RwV3dTransformPointNative(RwV3d* pointOut, const RwV3d* pointIn, const RwMatrix* matrix);
RwV3d* RwV3dTransformPointsNative(RwV3d* pointsOut,
                                  const RwV3d* pointsIn,
                                  int32 numPoints,
                                  const RwMatrix* matrix);
RwMatrix* RwMatrixOrthoNormalizeNative(RwMatrix* matrixOut, const RwMatrix* matrixIn);
uint32 RwStreamReadNative(RwStream* stream, void* buffer, uint32 length);
RwStream* RwStreamOpenNative(int32 type, int32 accessType, const void* data);
int32 RwStreamCloseNative(RwStream* stream, void* data);
RwMatrix* RwMatrixTransformNative(RwMatrix* matrix, const RwMatrix* transform, int32 combineOp);
RwMatrix* RwMatrixCreateNative();
RwMatrix* RwMatrixRotateNative(RwMatrix* matrix, const RwV3d* axis, RwReal angle, int32 combineOp);
RwMatrix* RwMatrixTranslateNative(RwMatrix* matrix, const RwV3d* translation, int32 combineOp);
RpClump* RpClumpForAllAtomicsNative(RpClump* clump, RpAtomicCallbackFn callback, void* data);
RpGeometry* RpGeometryForAllMaterialsNative(RpGeometry* geometry, RpMaterialCallbackFn callback, void* data);
int32 RpClumpDestroyNative(RpClump* clump);
RpClump* RpClumpRenderNative(RpClump* clump);
RpLight* RpLightCreateNative(int32 type);
int32 RpLightDestroyNative(RpLight* light);
RpWorld* RpWorldCreateNative(RwBBox* boundingBox);
RpWorld* RpWorldAddCameraNative(RpWorld* world, RwCamera* camera);
RpLight* RpLightSetColorNative(RpLight* light, const RwRGBAReal* color);
RpAtomic* AtomicDefaultRenderCallBackNative(RpAtomic* atomic);
RpWorld* RpWorldAddLightNative(RpWorld* world, RpLight* light);
RpWorld* RpWorldRemoveLightNative(RpWorld* world, RpLight* light);
int32 RpAtomicDestroyNative(RpAtomic* atomic);
void RpClumpGtaCancelStreamNative();
RsGlobalType* GetRsGlobalNative();
RwFrame* RwFrameUpdateObjectsNative(RwFrame* frame);
RwTexture* RwTextureCreateNative(RwRaster* raster);
RwCamera* RwCameraCreateNative();
RwFrame* RwFrameCreateNative();
RwCamera* RwCameraClearNative(RwCamera* camera, RwRGBA* colour, int32 clearMode);
RwCamera* RwCameraSetNearClipPlaneNative(RwCamera* camera, RwReal nearClip);
RwCamera* RwCameraSetFarClipPlaneNative(RwCamera* camera, RwReal farClip);
RwFrame* RwFrameTranslateNative(RwFrame* frame, const RwV3d* v, int32 combine);
RwFrame* RwFrameRotateNative(RwFrame* frame, const RwV3d* axis, RwReal angle, int32 combine);
RwCamera* RwCameraSetViewWindowNative(RwCamera* camera, const RwV2d* viewWindow);
RwCamera* RwCameraSetProjectionNative(RwCamera* camera, int32 projection);
void RwObjectHasFrameSetFrameNative(void* object, RwFrame* frame);
RwMatrix* RwFrameGetLTMNative(RwFrame* frame);
RwCamera* RwCameraEndUpdateNative(RwCamera* camera);
int32 RwIm3DEndNative();
int32 RwIm3DRenderPrimitiveNative(int32 primType);
int32 RwIm3DRenderIndexedPrimitiveNative(int32 primType, uint32* indices, int32 numIndices);
void* RwIm3DTransformNative(void* pVerts, uint32 numVerts, RwMatrix* ltm, uint32 flags);
RwTexture* RwTextureReadNative(const char* name, const char* maskName);
RwFrame* RwFrameForAllObjectsNative(RwFrame* frame, RwObjectCallbackFn callBack, void* data);
int32 RwFrameDestroyNative(RwFrame* frame);
RwTexture* RwTextureSetRasterNative(RwTexture* texture, RwRaster* raster);
int32 RwCameraDestroyNative(RwCamera* camera);
int32 RwIm3DRenderLineNative(int32 vert1, int32 vert2);
RwTexture* RwTextureSetNameNative(RwTexture* texture, const char* name);
RwFrame* RwFrameOrthoNormalizeNative(RwFrame* frame);
int32 RwTextureSetFindCallBackNative(RwTextureFindCallbackFn callBack);
int32 RwTextureSetReadCallBackNative(RwTextureReadCallbackFn callBack);
void*& RwEngineInstanceRefNative();
int32& RxPipelineGlobalsOffsetRefNative();
RxPipeline* RxPipelineCreateNative();
uint32 RxChaseDependenciesNative(RxPipeline* pipeline);
RxPipeline* RxPipelineLockNative(RxPipeline* pipeline);
RxPipelineNode* RxPipelineFindNodeByNameNative(RxPipeline* pipeline,
                                               const char* name,
                                               RxPipelineNode* start,
                                               int32* nodeIndex);
RxPipeline* RxLockedPipeAddFragmentNative(RxPipeline* pipeline,
                                          uint32* firstIndex,
                                          RxNodeDefinition* nodeDef0,
                                          uint32 nodeUnk);
RxNodeDefinition* RxNodeDefinitionGetOpenGLAtomicAllInOneNative();
void RxOpenGLAllInOneSetInstanceCallBackNative(RxPipelineNode* node, uintptr_t instanceCB);
void RxOpenGLAllInOneSetRenderCallBackNative(RxPipelineNode* node, uintptr_t renderCB);
void RxPipelineDestroyNative(RxPipeline* pipeline);
const char* GetFrameNodeNameNative(RwFrame* frame);
int32 RpMatFXMaterialGetEffectsNative(RpMaterial* material);

}
