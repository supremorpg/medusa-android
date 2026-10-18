//
// Created by x1y2z on 27.04.2023.
//
#include "BaseModelInfo.h"
#include "game/TxdStore.h"
#include "../GTASAEngineApi.h"

void CBaseModelInfo::SetTexDictionary(const char* txdName, const char *dbName) {
    m_nTxdIndex = CTxdStore::FindOrAddTxdSlot(txdName, dbName);
}

ModelInfoType CBaseModelInfo::GetModelType() {
    return static_cast<ModelInfoType>(GTASAEngineApi::BaseModelInfoGetModelTypeNative(this));
}

int32 CBaseModelInfo::GetAnimFileIndex() {
    return GTASAEngineApi::BaseModelInfoGetAnimFileIndexNative(this);
}

void CBaseModelInfo::DeleteRwObject() {
    GTASAEngineApi::BaseModelInfoDeleteRwObjectNative(this);
}
