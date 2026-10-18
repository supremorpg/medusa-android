
#include "rtquat.h"
#include "rpworld.h"
#include "game/common.h"
#include "../GTASAEngineApi.h"
#include "../GTASAEngineRwBindings.h"

void RtQuatConvertToMatrix(const RtQuat* const qpQuat, RwMatrix* const mpMatrix) {
    RwReal              rS;
    RwV3d               rV;
    RwV3d               rW;
    RwV3d               square;
    RwV3d               cross;

    rS = ((RwReal)2) / (qpQuat->imag.x * qpQuat->imag.x
                        + qpQuat->imag.y * qpQuat->imag.y
                        + qpQuat->imag.z * qpQuat->imag.z
                        + qpQuat->real * qpQuat->real);

    RwV3dScale(&rV, &(qpQuat)->imag, rS);
    RwV3dScale(&rW, &rV, (qpQuat)->real);

    square.x = (qpQuat)->imag.x * rV.x;
    square.y = (qpQuat)->imag.y * rV.y;
    square.z = (qpQuat)->imag.z * rV.z;

    cross.x = (qpQuat)->imag.y * rV.z;
    cross.y = (qpQuat)->imag.z * rV.x;
    cross.z = (qpQuat)->imag.x * rV.y;

    (mpMatrix)->right.x = ((RwReal)1) - (square.y + square.z);
    (mpMatrix)->right.y = cross.z + rW.z;
    (mpMatrix)->right.z = cross.y - rW.y;

    (mpMatrix)->up.x = cross.z - rW.z;
    (mpMatrix)->up.y = ((RwReal)1) - (square.z + square.x);
    (mpMatrix)->up.z = cross.x + rW.x;

    (mpMatrix)->at.x = cross.y + rW.y;
    (mpMatrix)->at.y = cross.x - rW.x;
    (mpMatrix)->at.z = ((RwReal)1) - (square.x + square.y);

    /* Set position */
    (mpMatrix)->pos.x = ((RwReal)0);
    (mpMatrix)->pos.y = ((RwReal)0);
    (mpMatrix)->pos.z = ((RwReal)0);

    /* Matrix is orthogonal */
    mpMatrix->flags = rwMATRIXTYPEORTHONORMAL & ~rwMATRIXINTERNALIDENTITY;
}

RwBool RtQuatConvertFromMatrix(RtQuat* qpQuat, const RwMatrix* const mpMatrix) {
    return GTASAEngineApi::RtQuatConvertFromMatrixNative(qpQuat, mpMatrix);
}

RtQuat* RtQuatRotate(RtQuat* quat, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp) {
    return GTASAEngineApi::RtQuatRotateNative(quat, axis, angle, static_cast<int32>(combineOp));
}

const RtQuat* RtQuatQueryRotate(const RtQuat* quat, RwV3d* unitAxis, RwReal* angle) {
    return GTASAEngineApi::RtQuatQueryRotateNative(quat, unitAxis, angle);
}

RwV3d* RtQuatTransformVectors(RwV3d* vectorsOut, const RwV3d* vectorsIn, const RwInt32 numPoints, const RtQuat* quat) {
    return GTASAEngineApi::RtQuatTransformVectorsNative(vectorsOut, vectorsIn, numPoints, quat);
}

RwReal RtQuatModulus(RtQuat* q) {
    return GTASAEngineApi::RtQuatModulusNative(q);
}
