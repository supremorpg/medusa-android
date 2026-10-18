//
// Created by x1y2z on 20.04.2023.
//
#include "rwcore.h"
#include "rpworld.h"
#include "../GTASAEngineApi.h"
#include "../GTASAEngineRwBindings.h"
#include <assert.h>

RwFrame* RwFrameUpdateObjects(RwFrame* frame) {
    return GTASAEngineApi::RwFrameUpdateObjectsNative(frame);
}

RwTexture* RwTextureCreate(RwRaster* raster) {
    return GTASAEngineApi::RwTextureCreateNative(raster);
}

RwCamera* RwCameraCreate() {
    return GTASAEngineApi::RwCameraCreateNative();
}

RwFrame* RwFrameCreate() {
    return GTASAEngineApi::RwFrameCreateNative();
}

RwCamera* RwCameraClear(RwCamera* camera, RwRGBA* colour, RwInt32 clearMode) {
    return GTASAEngineApi::RwCameraClearNative(camera, colour, clearMode);
}

RwCamera* RwCameraSetNearClipPlane(RwCamera* camera, RwReal nearClip) {
    return GTASAEngineApi::RwCameraSetNearClipPlaneNative(camera, nearClip);
}

RwCamera* RwCameraSetFarClipPlane(RwCamera* camera, RwReal farClip) {
    return GTASAEngineApi::RwCameraSetFarClipPlaneNative(camera, farClip);
}

RwFrame* RwFrameTranslate(RwFrame* frame, const RwV3d* v, RwOpCombineType combine) {
    return GTASAEngineApi::RwFrameTranslateNative(frame, v, combine);
}

RwFrame* RwFrameRotate(RwFrame* frame, const RwV3d* axis, RwReal angle, RwOpCombineType combine) {
    return GTASAEngineApi::RwFrameRotateNative(frame, axis, angle, combine);
}

RwCamera* RwCameraSetViewWindow(RwCamera* camera, const RwV2d* viewWindow) {
    return GTASAEngineApi::RwCameraSetViewWindowNative(camera, viewWindow);
}

RwCamera* RwCameraSetProjection(RwCamera* camera, RwCameraProjection projection) {
    return GTASAEngineApi::RwCameraSetProjectionNative(camera, projection);
}

void _rwObjectHasFrameSetFrame(void *object, RwFrame *frame) {
    GTASAEngineApi::RwObjectHasFrameSetFrameNative(object, frame);
}

RwMatrix* RwFrameGetLTM(RwFrame* frame) {
    return GTASAEngineApi::RwFrameGetLTMNative(frame);
}

RwCamera* RwCameraEndUpdate(RwCamera* camera) {
    return GTASAEngineApi::RwCameraEndUpdateNative(camera);
}

RwBool RwIm3DEnd() {
    return GTASAEngineApi::RwIm3DEndNative();
}

RwBool RwIm3DRenderPrimitive(RwPrimitiveType primType) {
    return GTASAEngineApi::RwIm3DRenderPrimitiveNative(primType);
}

RwBool RwIm3DRenderIndexedPrimitive(RwPrimitiveType primType, RwImVertexIndex* indices, RwInt32 numIndices) {
    return GTASAEngineApi::RwIm3DRenderIndexedPrimitiveNative(primType, indices, numIndices);
}

void* RwIm3DTransform(RwIm3DVertex* pVerts, RwUInt32 numVerts, RwMatrix* ltm, RwUInt32 flags) {
    return GTASAEngineApi::RwIm3DTransformNative(pVerts, numVerts, ltm, flags);
}

RwTexture* RwTextureRead(const char* name, const char* maskName) {
    return GTASAEngineApi::RwTextureReadNative(name, maskName);
}

RwFrame* RwFrameForAllObjects(RwFrame* frame, RwObjectCallBack callBack, void* data) {
    return GTASAEngineApi::RwFrameForAllObjectsNative(frame, callBack, data);
}

RwBool RwFrameDestroy(RwFrame* frame) {
    assert(frame);
    return GTASAEngineApi::RwFrameDestroyNative(frame);
}

RwTexture* RwTextureSetRaster(RwTexture* texture, RwRaster* raster) {
    return GTASAEngineApi::RwTextureSetRasterNative(texture, raster);
}

RwBool RwCameraDestroy(RwCamera* camera) {
    return GTASAEngineApi::RwCameraDestroyNative(camera);
}

RwBool RwIm3DRenderLine(RwInt32 vert1, RwInt32 vert2) {
    return GTASAEngineApi::RwIm3DRenderLineNative(vert1, vert2);
}

RwTexture* RwTextureSetName(RwTexture* texture, const RwChar* name) {
    return GTASAEngineApi::RwTextureSetNameNative(texture, name);
}

RwFrame* RwFrameOrthoNormalize(RwFrame* frame) {
    return GTASAEngineApi::RwFrameOrthoNormalizeNative(frame);
}

RwBool RwTextureSetFindCallBack(RwTextureCallBackFind callBack) {
    return GTASAEngineApi::RwTextureSetFindCallBackNative(callBack);
}

RwBool RwTextureSetReadCallBack(RwTextureCallBackRead callBack) {
    return GTASAEngineApi::RwTextureSetReadCallBackNative(callBack);
}
