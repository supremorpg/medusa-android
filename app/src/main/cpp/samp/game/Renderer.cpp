//
// Created by x1y2z on 15.11.2023.
//

#include "Renderer.h"
#include "../main.h"
#include "GTASAEngineApi.h"

void CRenderer::InjectHooks() {
    GTASAEngineApi::BindRendererGlobals(&ms_bRenderOutsideTunnels,
                                         &m_loadingPriority,
                                         &ms_aVisibleEntityPtrs,
                                         &ms_nNoOfVisibleEntities,
                                         &ms_aVisibleLodPtrs,
                                         &ms_nNoOfVisibleLods,
                                         &ms_aVisibleSuperLodPtrs,
                                         &ms_nNoOfVisibleSuperLods,
                                         &ms_aInVisibleEntityPtrs,
                                         &ms_nNoOfInVisibleEntities);
#if !VER_x32
    FLog("[RENDER64] redirected visible/lod/superlod/invisible render lists");
#endif
}
