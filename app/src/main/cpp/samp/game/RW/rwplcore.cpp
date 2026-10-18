//
// Created by x1y2z on 19.04.2023.
//
#include "rwplcore.h"
#include "../../main.h"
#include "../GTASAEngineApi.h"
#include "../GTASAEngineRwBindings.h"

RwMatrix* RwMatrixUpdate(RwMatrix* matrix) {
    matrix->flags &= 0xFFFDFFFC;
    return matrix;
}

RwBool RwMatrixDestroy(RwMatrix* mpMat) {
    return GTASAEngineApi::RwMatrixDestroyNative(mpMat);
}

RwV3d* RwV3dTransformPoint(RwV3d* pointOut, const RwV3d* pointIn, const RwMatrix* matrix) {
    return GTASAEngineApi::RwV3dTransformPointNative(pointOut, pointIn, matrix);
}

RwV3d* RwV3dTransformPoints(RwV3d* pointsOut, const RwV3d* pointsIn, RwInt32 numPoints, const RwMatrix* matrix) {
    return GTASAEngineApi::RwV3dTransformPointsNative(pointsOut, pointsIn, numPoints, matrix);
}

RwMatrix* RwMatrixOrthoNormalize(RwMatrix* matrixOut, const RwMatrix* matrixIn) {
    return GTASAEngineApi::RwMatrixOrthoNormalizeNative(matrixOut, matrixIn);
}

RwUInt32 RwStreamRead(RwStream* stream, void* buffer, RwUInt32 length) {
    return GTASAEngineApi::RwStreamReadNative(stream, buffer, length);
}

RwStream* RwStreamOpen(RwStreamType type, RwStreamAccessType accessType, const void* data) {
    return GTASAEngineApi::RwStreamOpenNative(type, accessType, data);
}

RwBool RwStreamClose(RwStream* stream, void* data) {
    return GTASAEngineApi::RwStreamCloseNative(stream, data);
}

RwMatrix* RwMatrixTransform(RwMatrix* matrix, const RwMatrix* transform, RwOpCombineType combineOp) {
    return GTASAEngineApi::RwMatrixTransformNative(matrix, transform, combineOp);
}

RwMatrix* RwMatrixCreate() {
    return GTASAEngineApi::RwMatrixCreateNative();
}

RwMatrix* RwMatrixRotate(RwMatrix* pMat, CVector* axis, float angle)
{
    return GTASAEngineApi::RwMatrixRotateNative(pMat, axis, angle, rwCOMBINEPRECONCAT);
}

RwMatrix* RwMatrixTranslate(RwMatrix *matrix, const RwV3d *translation, RwOpCombineType combineOp)
{
    return GTASAEngineApi::RwMatrixTranslateNative(matrix, translation, combineOp);
}
