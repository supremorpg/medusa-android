//
// Created by x1y2z on 27.04.2024.
//

#include "CustomCarEnvMapPipeline.h"
#include "../../GTASAEngineApi.h"
#include "../../GTASAEngineRwBindings.h"
#include "pipe/p2core.h"
#include "pipe/p2define.h"
#include "opengl/openglpipepriv.h"

// Android - CreateCustomOpenGLObjPipe
RxPipeline* CCustomCarEnvMapPipeline::CreateCustomObjPipe() {
    return GTASAEngineApi::CreateCustomCarOpenGLObjPipeNative();
//    auto pipeline = RxPipelineCreate();
//    if (!pipeline)
//        return nullptr;
//
//    auto lock = RxPipelineLock(pipeline);
//    if (lock) {
//        auto nodeDefinition = RxNodeDefinitionGetOpenGLAtomicAllInOne();
//        auto lockedPipe = RxLockedPipeAddFragment(lock, 0, nodeDefinition);
//        if (RxLockedPipeUnlock(lockedPipe)) {
//            auto node = RxPipelineFindNodeByName(pipeline, nodeDefinition->name, nullptr, nullptr);
//            RxOpenGLAllInOneSetInstanceCallBack(node, CCustomCarEnvMapPipeline::CustomPipeInstanceCB);
//           // RxD3D9AllInOneSetReinstanceCallBack(node, CustomPipeInstanceCB);
//            RxOpenGLAllInOneSetRenderCallBack(node, (RxOpenGLAllInOneRenderCallBack)CCustomCarEnvMapPipeline::CustomPipeRenderCB);
//            pipeline->pluginId = CUSTOM_CAR_ENV_MAP_PIPELINE_PLUGIN_ID;
//            pipeline->pluginData = CUSTOM_CAR_ENV_MAP_PIPELINE_PLUGIN_ID;
//            return pipeline;
//        }
//    }
//    _rxPipelineDestroy(pipeline);
//    return nullptr;
}


bool CCustomCarEnvMapPipeline::CreatePipe() {
    ObjPipeline = CreateCustomObjPipe();
    if (!ObjPipeline /*|| !IsEnvironmentMappingSupported()*/) {
        return false;
    }
  //  memset(&g_GameLight, 0, sizeof(g_GameLight));
    m_gEnvMapPipeMatDataPool = new CustomEnvMapPipeMaterialDataPool(4096, "CustomEnvMapPipeMatDataPool");
    m_gEnvMapPipeAtmDataPool = new CustomEnvMapPipeAtomicDataPool(1024, "CustomEnvMapPipeAtmDataPool");
    m_gSpecMapPipeMatDataPool = new CustomSpecMapPipeMaterialDataPool(4096, "CustomSpecMapPipeMaterialDataPool");
    return true;
}


// x64 fixes
RwBool (*openglAtomicAllInOnePipelineInit)(RxPipelineNode *node);
RwBool openglAtomicAllInOnePipelineInit_hooked(RxPipelineNode *node)
{
    node->privateData = new _rxOpenGLAllInOnePrivateData;

    return openglAtomicAllInOnePipelineInit(node);
}

RwBool (*openglWorldSectorAllInOnePipelineInit)(RxPipelineNode *node);
RwBool openglWorldSectorAllInOnePipelineInit_hooked(RxPipelineNode *node)
{
    node->privateData = new _rxOpenGLAllInOnePrivateData;

    return openglWorldSectorAllInOnePipelineInit(node);
}

RpMaterial* CCustomCarEnvMapPipeline__CustomPipeMaterialSetup(RpMaterial* material, void* data)
{
    auto& flags = reinterpret_cast<uint32_t&>(material->surfaceProps.specular);

    flags = 0u;

    if (GTASAEngineApi::RpMatFXMaterialGetEffectsNative(material) == rpMATFXEFFECTENVMAP)
    {
        GTASAEngineApi::CustomBuildingDNSetFxEnvTextureNative(material, nullptr);
    }

    if (GTASAEngineApi::CustomCarEnvMapGetFxEnvShininessNative(material) != 0.0f)
    {
        if (const auto tex = GTASAEngineApi::CustomCarEnvMapGetFxEnvTextureNative(material))
        {
            flags |= RwTextureGetName(tex)[0] == 'x' ? 0b10 : 0b01;
        }
    }

    auto getFxSpecSpecularity = GTASAEngineApi::CustomCarEnvMapGetFxSpecSpecularityNative(material);
    auto* getFxSpecTexture = GTASAEngineApi::CustomCarEnvMapGetFxSpecTextureNative(material);
    if (getFxSpecSpecularity != 0.0f && getFxSpecTexture)
    {
        flags |= 0b100;
    }

    return material;
}

void CCustomCarEnvMapPipeline::InjectHooks() {
    GTASAEngineApi::InstallCustomCarEnvMapPipelineHooksNative(
        reinterpret_cast<uintptr_t>(&openglWorldSectorAllInOnePipelineInit_hooked),
        reinterpret_cast<void**>(&openglWorldSectorAllInOnePipelineInit),
        reinterpret_cast<uintptr_t>(&openglAtomicAllInOnePipelineInit_hooked),
        reinterpret_cast<void**>(&openglAtomicAllInOnePipelineInit),
        &ObjPipeline,
        &m_gEnvMapPipeMatDataPool,
        &m_gEnvMapPipeAtmDataPool,
        &m_gSpecMapPipeMatDataPool);
}

RwBool CCustomCarEnvMapPipeline::CustomPipeInstanceCB(void *object, RxOpenGLMeshInstanceData *instanceData, const RwBool instanceDLandVA, const RwBool reinstance) {
    return GTASAEngineApi::CustomCarEnvMapPipeInstanceCBNative(object, instanceData, instanceDLandVA, reinstance);
}

void CCustomCarEnvMapPipeline::CustomPipeRenderCB(RwResEntry *repEntry, void *object, RwUInt8 type, RwUInt32 flags) {
    GTASAEngineApi::CustomCarEnvMapPipeRenderCBNative(repEntry, object, type, flags);
}
