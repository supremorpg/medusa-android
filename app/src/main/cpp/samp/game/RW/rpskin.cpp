//
// Created by x1y2z on 20.04.2023.
//

#include "RenderWare.h"
#include "../GTASAEngineApi.h"
#include "../GTASAEngineRwBindings.h"

RpSkin* RpSkinGeometryGetSkin(RpGeometry* geometry) {
    return GTASAEngineApi::RpSkinGeometryGetSkinNative(geometry);
}

RpHAnimHierarchy* RpSkinAtomicGetHAnimHierarchy(const RpAtomic* atomic) {
    return GTASAEngineApi::RpSkinAtomicGetHAnimHierarchyNative(atomic);
}
