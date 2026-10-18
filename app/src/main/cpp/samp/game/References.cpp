#include "common.h"
#include "References.h"
#include "GTASAEngineApi.h"

void CReferences::InjectHooks() {
    GTASAEngineApi::BindReferencesGlobals(&CReferences::pEmptyList, &CReferences::aRefs);
}

// 0x5719B0
void CReferences::Init() {
    pEmptyList = aRefs;

    // todo: do better
    for (int32 i = 0; i < MAX_NUM_REFERENCES - 1; ++i) {
        aRefs[i].m_pNext = &aRefs[i + 1];
        aRefs[i].m_ppEntity = nullptr;
    }

    aRefs[MAX_NUM_REFERENCES - 1].m_pNext = nullptr;
}

uint32 CReferences::ListSize(CReference* ref) {
    uint32 count = 0;
    while (ref) {
        ref = ref->m_pNext;
        ++count;
    }

    return count;
}

void CReferences::RemoveReferencesToPlayer() {

}

void CReferences::PruneAllReferencesInWorld() {

}
