//
// Created by x1y2z on 19.04.2023.
//

#include "CdStreamInfo.h"
#include "GTASAEngineStreamingBindings.h"

void CdStreamInit(int32 streamCount) {
    GTASAEngineApi::CdStreamInitNative(streamCount);
}

int32 CdStreamOpen(const char* lpFileName, bool bNotPlayerImg) {
    return GTASAEngineApi::CdStreamOpenNative(lpFileName, bNotPlayerImg);
}

// This function halts the caller thread if CdStreamThread is still reading the file to "sync" it.
// CdStreamSync is always called from the main thread. It's called when entering or exiting
// interiors. It's used when the game wants a model to be loaded immediately,
// like when spawning after exiting interiors.
// This function was responsible for causing the deadlock since it was not thread-safe.
// The deadlock problem only exists on PC version of SA. R* fixed it on android.
// So it's fixed here because we used a critical section for it.
// Workflow:
// 1. CdStreamRead is called on the main thread and signals gStreamSemaphore to
//    read game asset in a secondary thread a.k.a. CdStreamThread.
// 2. CdStreamThread starts reading the file on the secondary thread, and the main thread
//    immediately calls CdStreamSync to stop the main thread from execution until the model data has been read from the file.
// 3. When CdStreamThread is done reading the file, it signals `stream.sync.hSemaphore`, so the main thread can
//    continue executing code and continue the gameplay.
eCdStreamStatus CdStreamSync(int32 streamId) {
    return static_cast<eCdStreamStatus>(GTASAEngineApi::CdStreamSyncNative(streamId));
}

eCdStreamStatus CdStreamGetStatus(int32 streamId) {
    return static_cast<eCdStreamStatus>(GTASAEngineApi::CdStreamGetStatusNative(streamId));
}

// When CdStreamRead is called, it will update CdStream information for the channel and
// signal gStreamSemaphore, so the secondary thread `CdStreamThread` can start reading the models.
// If this function is called with the same channelId/streamId whilst CdStreamThread is still reading the previous model
// for the channel, then it will return false.
// When CdStreamThread is done reading the model, then CdStreamThread will set `stream.nSectorsToRead` and `stream.bInUse` to 0,
// so the main thread can call CdStreamRead again to read more models.
bool CdStreamRead(int32 streamId, void* lpBuffer, uint32 offsetAndHandle, int32 sectorCount) {
    return GTASAEngineApi::CdStreamReadNative(streamId, lpBuffer, offsetAndHandle, sectorCount);
}

uint32 CdStreamGetLastPosn() {
    return GTASAEngineApi::CdStreamGetLastPositionNative();
}
