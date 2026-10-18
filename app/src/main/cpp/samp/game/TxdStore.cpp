//
// Created by x1y2z on 01.07.2023.
//

#include "TxdStore.h"
#include "GTASAEngineTxdBindings.h"

int32 CTxdStore::GetNumRefs(int32 index){
    return GTASAEngineApi::TxdStoreGetNumRefsNative(index);
}

void CTxdStore::RemoveTxd(int32 index) {
    GTASAEngineApi::TxdStoreRemoveTxdNative(index);
}

void CTxdStore::InjectHooks() {
}

int32 CTxdStore::FindTxdSlot(const char *name) {
    return GTASAEngineApi::TxdStoreFindSlotNative(name);
}

int32 CTxdStore::FindTxdSlot(uint32 hash) {
    assert("NO x64 call");
}

int32 CTxdStore::AddTxdSlot(const char *name, const char *dbName, bool keepCPU) {
    return GTASAEngineApi::TxdStoreAddSlotNative(name, dbName, keepCPU);
}

void CTxdStore::Initialise() {
    GTASAEngineApi::TxdStoreInitialiseNative();
}

void CTxdStore::PushCurrentTxd() {

    GTASAEngineApi::TxdStorePushCurrentNative();
}

void CTxdStore::PopCurrentTxd() {
    GTASAEngineApi::TxdStorePopCurrentNative();
}

void CTxdStore::SetCurrentTxd(int32 index) {
    GTASAEngineApi::TxdStoreSetCurrentNative(index);
}
