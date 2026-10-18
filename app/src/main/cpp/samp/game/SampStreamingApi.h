#pragma once

#include "GTASAEngineApi.h"
#include "StreamingInfo.h"

namespace Xyron::Streaming {

using WorldStreamRequest = GTASAEngineApi::WorldStreamRequest;
using WorldStreamStatus = GTASAEngineApi::WorldStreamStatus;

struct ModelStreamDiagnostic {
    bool valid = false;
    bool loaded = false;
    uint8 loadState = 0;
    uint8 flags = 0;
    uint8 imgId = 0;
    uint32 cdPosn = 0;
    uint32 cdSize = 0;
    int16 nextIndexOnCd = -1;
};

struct StreamingMemoryBudgetResult {
    bool changed = false;
    bool hasNativeMemoryAvailable = false;
    size_t wrapperBefore = 0;
    size_t wrapperAfter = 0;
    uint32 nativeBefore = 0;
    uint32 nativeAfter = 0;
    size_t memoryUsed = 0;
};

struct MountedImageArchiveMetadata {
    bool occupied = false;
    bool notPlayerImg = false;
    bool sizeKnown = false;
    int32 slot = -1;
    int32 streamHandle = -1;
    uint64 sizeBytes = 0;
    char expectedName[40] = {};
    char openedName[40] = {};
};

bool IsModelLoaded(int32 modelId);
bool IsValidResourceId(int32 modelId);
int32 IfpModelIdFromBlock(int32 blockIndex);
int32 ColModelIdFromSlot(int32 slot);
void InitialiseCdStreams();
void InstallStreamingHooks();
void InstallStreamingInfoHooks();
bool IsStreamingDisabled();
bool EnableStreamingIfDisabled();
void ForceStreamingEnabled(const char* reason = nullptr);
bool TryLoadModel(int32 modelId);
void RequestModel(int32 modelId, int32 streamingFlags);
void MarkModelDontRemoveInLoadScene(int32 modelId);
void LoadRequestedModels();
void LoadAllRequestedModels(bool priorityRequestsOnly);
void LoadSceneCollision(const CVector& point);
void RemoveModel(int32 modelId);
void RemoveStreamingModel(int32 modelId);
void RemoveModelFromNativeStreaming(int32 modelId);
void RemoveModelIfNoRefs(int32 modelId);
char* GetModelCDName(int32 modelId);
void RequestSceneObjectsAroundPlayer(const CVector& playerPos, const CVector& velocity);
WorldStreamStatus PrepareWorldAtPoint(const CVector& point, const WorldStreamRequest& request);
void UpdateStreamingFrame();
uint8_t ResolveEffectiveWorldArea(const CVector& point, int32 areaCode);
int32 ResolveEffectiveWorldAreaCode(const CVector& point, int32 areaCode);
ModelStreamDiagnostic ReadModelDiagnostic(int32 modelId);
size_t MemoryUsed();
size_t MemoryAvailable();
size_t CollectMountedImageArchives(MountedImageArchiveMetadata* outArchives, size_t capacity);
void LogMountedImageArchives(const char* reason = nullptr);
StreamingMemoryBudgetResult ApplyMemoryBudget(uint32 minAvailableBytes, uint32 maxAvailableBytes);

}
