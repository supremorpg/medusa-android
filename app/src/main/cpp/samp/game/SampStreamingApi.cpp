#include "SampStreamingApi.h"
#include "Streaming.h"
#include "GTASAEngineStreamingBindings.h"
#include "Enums/eAreaCodes.h"

#include <cstring>
#include <cmath>
#include <sys/stat.h>

namespace Xyron::Streaming {
namespace {

void CopyArchiveName(char* dst, size_t dstSize, const char* src)
{
    if (!dst || dstSize == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    std::strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

const char* ExpectedImageArchiveNameForSlot(size_t slot)
{
#if VER_SAMP
    static constexpr const char* kExpected[] = {
        "TEXDB\\SAMPCOL.IMG",
        "TEXDB\\GTA3.IMG",
        "TEXDB\\GTA_INT.IMG",
        "TEXDB\\SAMP.IMG",
    };
#else
    static constexpr const char* kExpected[] = {
        "TEXDB\\GTA3.IMG",
        "TEXDB\\GTA_INT.IMG",
        "TEXDB\\SAMP.IMG",
        "TEXDB\\SAMPCOL.IMG",
    };
#endif

    if (slot < sizeof(kExpected) / sizeof(kExpected[0])) {
        return kExpected[slot];
    }
    return "";
}

}

bool IsModelLoaded(int32 modelId)
{
    if (!IsValidResourceId(modelId)) {
        return false;
    }
    return CStreaming::IsModelLoaded(modelId);
}

bool IsValidResourceId(int32 modelId)
{
    return modelId >= 0 && modelId < RESOURCE_ID_TOTAL;
}

int32 IfpModelIdFromBlock(int32 blockIndex)
{
    return IFPToModelId(blockIndex);
}

int32 ColModelIdFromSlot(int32 slot)
{
    return COLToModelId(slot);
}

void InitialiseCdStreams()
{
    GTASAEngineApi::CdStreamInitNative(TOTAL_IMG_ARCHIVES);
}

void InstallStreamingHooks()
{
    CStreaming::InjectHooks();
}

void InstallStreamingInfoHooks()
{
    CStreamingInfo::InjectHooks();
}

bool IsStreamingDisabled()
{
    return CStreaming::ms_disableStreaming;
}

bool EnableStreamingIfDisabled()
{
    if (!CStreaming::ms_disableStreaming) {
        return false;
    }

    CStreaming::ms_disableStreaming = false;
    return true;
}

void ForceStreamingEnabled(const char* reason)
{
    GTASAEngineApi::ForceStreamingEnabled(reason);
}

bool TryLoadModel(int32 modelId)
{
    return CStreaming::TryLoadModel(modelId);
}

void RequestModel(int32 modelId, int32 streamingFlags)
{
    CStreaming::RequestModel(modelId, streamingFlags);
}

void MarkModelDontRemoveInLoadScene(int32 modelId)
{
    if (modelId < 0 || modelId >= RESOURCE_ID_TOTAL) {
        return;
    }
    CStreaming::GetInfo(modelId).SetFlags(STREAMING_DONTREMOVE_IN_LOADSCENE);
}

void LoadRequestedModels()
{
    CStreaming::LoadRequestedModels();
}

void LoadAllRequestedModels(bool priorityRequestsOnly)
{
    CStreaming::LoadAllRequestedModels(priorityRequestsOnly);
}

void LoadSceneCollision(const CVector& point)
{
    CStreaming::LoadSceneCollision(point);
}

void RemoveModel(int32 modelId)
{
    CStreaming::RemoveModel(modelId);
}

void RemoveStreamingModel(int32 modelId)
{
    GTASAEngineApi::RemoveStreamingModel(modelId);
}

void RemoveModelFromNativeStreaming(int32 modelId)
{
    if (modelId < 0 || modelId >= RESOURCE_ID_TOTAL) {
        return;
    }

#if !VER_x32
    GTASAEngineApi::RemoveStreamingModelNative(modelId);
#else
    CStreaming::RemoveModel(modelId);
#endif
}

void RemoveModelIfNoRefs(int32 modelId)
{
    CStreaming::RemoveModelIfNoRefs(modelId);
}

char* GetModelCDName(int32 modelId)
{
    return CStreaming::GetModelCDName(modelId);
}

void RequestSceneObjectsAroundPlayer(const CVector& playerPos, const CVector& velocity)
{
#if !VER_x32
    CStreaming::RequestSceneObjectsAroundPlayer64(playerPos, velocity);
#else
    (void)playerPos;
    (void)velocity;
#endif
}

WorldStreamStatus PrepareWorldAtPoint(const CVector& point, const WorldStreamRequest& request)
{
    return GTASAEngineApi::PrepareWorldAtPoint(point, request);
}

void UpdateStreamingFrame()
{
    CStreaming::Update();
}

uint8_t ResolveEffectiveWorldArea(const CVector& point, int32 areaCode)
{
    if (areaCode < 0 || areaCode > 255) {
        return static_cast<uint8_t>(AREA_CODE_NORMAL_WORLD);
    }

    const bool finitePoint =
        std::isfinite(point.x) &&
        std::isfinite(point.y) &&
        std::isfinite(point.z);
    const bool exteriorWorldPoint =
        finitePoint &&
        point.x > -3200.0f && point.x < 3200.0f &&
        point.y > -3200.0f && point.y < 3200.0f &&
        point.z > -120.0f && point.z < 180.0f;

    if (areaCode == static_cast<int32>(AREA_CODE_13) && exteriorWorldPoint) {
        return static_cast<uint8_t>(AREA_CODE_NORMAL_WORLD);
    }

    return static_cast<uint8_t>(areaCode);
}

int32 ResolveEffectiveWorldAreaCode(const CVector& point, int32 areaCode)
{
    return static_cast<int32>(ResolveEffectiveWorldArea(point, areaCode));
}

ModelStreamDiagnostic ReadModelDiagnostic(int32 modelId)
{
    ModelStreamDiagnostic diagnostic{};
    if (modelId < 0 || modelId >= RESOURCE_ID_TOTAL) {
        return diagnostic;
    }

    CStreamingInfo& info = CStreaming::GetInfo(modelId);
    diagnostic.valid = true;
    diagnostic.loaded = info.IsLoaded();
    diagnostic.loadState = static_cast<uint8>(info.m_nLoadState);
    diagnostic.flags = info.m_nFlags;
    diagnostic.imgId = info.m_nImgId;
    diagnostic.cdPosn = info.m_nCdPosn;
    diagnostic.cdSize = info.m_nCdSize;
    diagnostic.nextIndexOnCd = info.m_nNextIndexOnCd;
    return diagnostic;
}

size_t MemoryUsed()
{
    return CStreaming::ms_memoryUsed;
}

size_t MemoryAvailable()
{
    return CStreaming::ms_memoryAvailable;
}

size_t CollectMountedImageArchives(MountedImageArchiveMetadata* outArchives, size_t capacity)
{
    size_t mountedCount = 0;
    for (size_t slot = 0; slot < TOTAL_IMG_ARCHIVES; ++slot) {
        const tStreamingFileDesc& desc = CStreaming::ms_files[slot];
        if (!desc.IsInUse()) {
            continue;
        }

        MountedImageArchiveMetadata metadata{};
        metadata.occupied = true;
        metadata.notPlayerImg = desc.m_bNotPlayerImg;
        metadata.slot = static_cast<int32>(slot);
        metadata.streamHandle = desc.m_StreamHandle;
        CopyArchiveName(metadata.expectedName, sizeof(metadata.expectedName), ExpectedImageArchiveNameForSlot(slot));
        CopyArchiveName(metadata.openedName, sizeof(metadata.openedName), desc.m_szName);

        struct stat st {};
        if (metadata.openedName[0] != '\0' && stat(metadata.openedName, &st) == 0 && st.st_size >= 0) {
            metadata.sizeKnown = true;
            metadata.sizeBytes = static_cast<uint64>(st.st_size);
        }

        if (outArchives && mountedCount < capacity) {
            outArchives[mountedCount] = metadata;
        }
        ++mountedCount;
    }
    return mountedCount;
}

void LogMountedImageArchives(const char* reason)
{
    MountedImageArchiveMetadata archives[TOTAL_IMG_ARCHIVES]{};
    const size_t mountedCount = CollectMountedImageArchives(
        archives,
        sizeof(archives) / sizeof(archives[0]));

    FLog("[IMG_META64] reason=%s mounted=%zu capacity=%d",
         reason ? reason : "?",
         mountedCount,
         TOTAL_IMG_ARCHIVES);
    for (size_t i = 0; i < mountedCount && i < sizeof(archives) / sizeof(archives[0]); ++i) {
        const MountedImageArchiveMetadata& archive = archives[i];
        FLog("[IMG_META64] slot=%d handle=%d notPlayer=%d expected=%s opened=%s sizeKnown=%d size=%llu",
             archive.slot,
             archive.streamHandle,
             archive.notPlayerImg ? 1 : 0,
             archive.expectedName[0] ? archive.expectedName : "?",
             archive.openedName[0] ? archive.openedName : "?",
             archive.sizeKnown ? 1 : 0,
             static_cast<unsigned long long>(archive.sizeBytes));
    }
}

StreamingMemoryBudgetResult ApplyMemoryBudget(uint32 minAvailableBytes, uint32 maxAvailableBytes)
{
    StreamingMemoryBudgetResult result{};
    if (maxAvailableBytes < minAvailableBytes) {
        maxAvailableBytes = minAvailableBytes;
    }

    uint32* nativeMemoryAvailable = GTASAEngineApi::StreamingMemoryAvailableNative();
    result.hasNativeMemoryAvailable = nativeMemoryAvailable != nullptr;
    result.wrapperBefore = CStreaming::ms_memoryAvailable;
    result.nativeBefore = nativeMemoryAvailable ? *nativeMemoryAvailable : 0;

    if (CStreaming::ms_memoryAvailable < minAvailableBytes) {
        CStreaming::ms_memoryAvailable = minAvailableBytes;
        result.changed = true;
    } else if (CStreaming::ms_memoryAvailable > maxAvailableBytes) {
        CStreaming::ms_memoryAvailable = maxAvailableBytes;
        result.changed = true;
    }

    if (nativeMemoryAvailable && *nativeMemoryAvailable < minAvailableBytes) {
        *nativeMemoryAvailable = minAvailableBytes;
        result.changed = true;
    } else if (nativeMemoryAvailable && *nativeMemoryAvailable > maxAvailableBytes) {
        *nativeMemoryAvailable = maxAvailableBytes;
        result.changed = true;
    }

    result.wrapperAfter = CStreaming::ms_memoryAvailable;
    result.nativeAfter = nativeMemoryAvailable ? *nativeMemoryAvailable : 0;
    result.memoryUsed = CStreaming::ms_memoryUsed;
    return result;
}

}
