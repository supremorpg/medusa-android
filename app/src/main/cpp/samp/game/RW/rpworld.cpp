//
// Created by x1y2z on 11.04.2023.
//

#include "RenderWare.h"
#include "game/common.h"
#include "../GTASAEngineApi.h"
#include "../GTASAEngineRwBindings.h"

RpClump* RpClumpForAllAtomics(RpClump* clump, RpAtomicCallBack callback, void* data) {
    return GTASAEngineApi::RpClumpForAllAtomicsNative(clump, callback, data);
}

RpGeometry* RpGeometryForAllMaterials(RpGeometry* geometry, RpMaterialCallBack fpCallBack, void* data) {
    return GTASAEngineApi::RpGeometryForAllMaterialsNative(geometry, fpCallBack, data);
}

RwBool RpClumpDestroy(RpClump* clump) {
    return GTASAEngineApi::RpClumpDestroyNative(clump);
}

RpClump* RpClumpRender(RpClump* clump) {
    return GTASAEngineApi::RpClumpRenderNative(clump);
}

RpLight* RpLightCreate(RwInt32 type) {
    return GTASAEngineApi::RpLightCreateNative(type);
}

RwBool RpLightDestroy(RpLight* light) {
    return GTASAEngineApi::RpLightDestroyNative(light);
}

RpWorld* RpWorldCreate(RwBBox* boundingBox) {
    return GTASAEngineApi::RpWorldCreateNative(boundingBox);
}

RpWorld* RpWorldAddCamera(RpWorld* world, RwCamera* camera) {
    return GTASAEngineApi::RpWorldAddCameraNative(world, camera);
}

RpLight* RpLightSetColor(RpLight* light, const RwRGBAReal* color) {
    return GTASAEngineApi::RpLightSetColorNative(light, color);
}

RpAtomic* AtomicDefaultRenderCallBack(RpAtomic* atomic) {
    return GTASAEngineApi::AtomicDefaultRenderCallBackNative(atomic);
}

RpWorld* RpWorldAddLight(RpWorld* world, RpLight* light) {
    return GTASAEngineApi::RpWorldAddLightNative(world, light);
}

RpWorld* RpWorldRemoveLight(RpWorld* world, RpLight* light) {
    return GTASAEngineApi::RpWorldRemoveLightNative(world, light);
}

RwBool RpAtomicDestroy(RpAtomic* atomic) {
    return GTASAEngineApi::RpAtomicDestroyNative(atomic);
}

void RpClumpGtaCancelStream() {
    GTASAEngineApi::RpClumpGtaCancelStreamNative();
}
