#pragma once

#include <cstdint>

class CColPoint;
struct CEntityGTA;
struct CPhysical;
struct CVehicleGTA;
struct RpClump;
struct RwFrame;

namespace GTASAEngineApi {

using BikeSetupModelNodesFn = void (*)(CVehicleGTA*);

struct RendererListDebugState {
    int32_t nativeVisible = 0;
    int32_t nativeLods = 0;
    int32_t nativeInvisible = 0;
    int32_t nativeSuperLods = 0;
    uintptr_t visibleListPtr = 0;
    uintptr_t visibleCountPtr = 0;
    uintptr_t lodListPtr = 0;
    uintptr_t lodCountPtr = 0;
};

RwFrame* GetFrameFromId(RpClump* clump, int id);
bool ApplyPhysicalCollision(CPhysical* physical, CEntityGTA* entity, CColPoint& colPoint, float& damageIntensity);
bool ApplyPhysicalFriction(CPhysical* physical, float friction, CColPoint& colPoint);
void RenderStaticRoadEntity(CEntityGTA* entity);
void RenderStaticNonRoadEntity(CEntityGTA* entity);
bool GetRendererListDebugState(RendererListDebugState& out);
uint16_t WorldCurrentScanCode();
CEntityGTA* WorldIgnoreEntity();
BikeSetupModelNodesFn BikeSetupModelNodes();

}
