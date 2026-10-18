//
// Created by x1y2z on 25.04.2024.
//

#include "CustomBuildingRenderer.h"
#include "../../GTASAEngineApi.h"

bool CCustomBuildingRenderer::Initialise() {
    return GTASAEngineApi::InitialiseCustomBuildingRendererNative();
}

void CCustomBuildingRenderer::Update() {
    GTASAEngineApi::UpdateCustomBuildingRenderer();
}
