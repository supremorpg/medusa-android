#pragma once

#include "common.h"

namespace GTASAEngineApi {

int32 TxdStoreGetNumRefsNative(int32 index);
void TxdStoreRemoveTxdNative(int32 index);
int32 TxdStoreFindSlotNative(const char* name);
int32 TxdStoreAddSlotNative(const char* name, const char* dbName, bool keepCPU);
void TxdStoreInitialiseNative();
void TxdStorePushCurrentNative();
void TxdStorePopCurrentNative();
void TxdStoreSetCurrentNative(int32 index);

}
