#pragma once

#include <cstdint>

#include "common.h"
#include "Core/Vector.h"

struct CEntityGTA;
class CFileObjectInstance;

namespace GTASAEngineApi {

void RemoveStreamingModelNative(int32 modelId);
uint32_t* StreamingMemoryAvailableNative();

void CdStreamInitNative(int32 streamCount);
int32 CdStreamOpenNative(const char* fileName, bool notPlayerImg);
int32 CdStreamSyncNative(int32 streamId);
int32 CdStreamGetStatusNative(int32 streamId);
bool CdStreamReadNative(int32 streamId, void* buffer, uint32 offsetAndHandle, int32 sectorCount);
uint32 CdStreamGetLastPositionNative();

char* FileLoaderLoadLineFromBufferNative(char* bufferIt, int32 buffSize);
int32 FileLoaderLoadObjectNative(const char* line);
CEntityGTA* FileLoaderLoadObjectInstanceNative(CFileObjectInstance* objInstance, const char* modelName);
void InstallFileLoaderHooksNative(uintptr_t loadObjectHook,
                                  void** loadObjectOriginal,
                                  uintptr_t loadObjectInstanceHook,
                                  void** loadObjectInstanceOriginal);

void AddModelsToRequestList(const CVector& point, int32 streamingFlags);
void AddLodsToRequestList(const CVector& point, int32 streamingFlags);
void LoadSceneCollision(const CVector& point);
void LoadScene(const CVector& point);
void LoadRequestedModels();
void FlushStreamingChannels();
void SetModelIsDeletable(int32 modelId);

}
