//
// Created by x1y2z on 03.08.2023.
//

#include "VisibilityPlugins.h"
#include "GTASAEngineApi.h"


void CVisibilityPlugins::Initialise() {
    GTASAEngineApi::VisibilityInitialiseNative();
}

void CVisibilityPlugins::SetRenderWareCamera(RwCamera* camera) {
    GTASAEngineApi::VisibilitySetRenderWareCameraNative(camera);
}

RpAtomic* CVisibilityPlugins::RenderPedCB(RpAtomic* atomic) {
    const float distanceSquared = GetDistanceSquaredFromCamera(RpAtomicGetFrame(atomic));
    if (distanceSquared >= ms_pedLodDist)
        return atomic;

    int32 alpha = GetClumpAlpha(RpAtomicGetClump(atomic));
    if (alpha == 255) {
        AtomicDefaultRenderCallBack(atomic);
        return atomic;
    }
    RenderAlphaAtomic(atomic, alpha);
    return atomic;
}

float CVisibilityPlugins::GetDistanceSquaredFromCamera(RwFrame* frame) {
    RwMatrix* transformMatrix = RwFrameGetLTM(frame);
    CVector distance;
    RwV3dSub(&distance, &transformMatrix->pos, ms_pCameraPosn);
    return distance.SquaredMagnitude();
}


int32 CVisibilityPlugins::GetClumpAlpha(RpClump* clump) {
    return GTASAEngineApi::VisibilityGetClumpAlphaNative(clump);
}

void CVisibilityPlugins::RenderAlphaAtomic(RpAtomic* atomic, int32 alpha) {
    GTASAEngineApi::VisibilityRenderAlphaAtomicNative(atomic, alpha);
}

void CVisibilityPlugins::InjectHooks() {
}

void CVisibilityPlugins::SetupVehicleVariables(RpClump *clump) {
    GTASAEngineApi::VisibilitySetupVehicleVariablesNative(clump);
}

void CVisibilityPlugins::RenderReallyDrawLastObjects() {
    GTASAEngineApi::VisibilityRenderReallyDrawLastObjectsNative();
}

// The function name is misleading, it returns the flags
uint16 CVisibilityPlugins::GetAtomicId(RpAtomic* atomic) {
    return GTASAEngineApi::VisibilityGetAtomicIdNative(atomic);
}

