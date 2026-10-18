//
// Created by x1y2z on 20.04.2023.
//
#include "ClumpModelInfo.h"
#include "../GTASAEngineApi.h"


void CClumpModelInfo::CClumpModelInfo__SetClump(RpClump* clump)
{
    GTASAEngineApi::ClumpModelInfoSetClumpNative(this, clump);
}

void CClumpModelInfo::SetFrameIds(RwObjectNameIdAssocation* data) {
    GTASAEngineApi::ClumpModelInfoSetFrameIdsNative(this, data);
}

void CClumpModelInfo::CClumpModelInfo__DeleteRwObject()
{
   GTASAEngineApi::ClumpModelInfoDeleteRwObjectNative(this);
}

RwFrame* CClumpModelInfo::GetFrameFromName(RpClump* clump, const char* name)
{
    return GTASAEngineApi::ClumpModelInfoGetFrameFromNameNative(clump, name);
//    auto searchInfo = tCompSearchStructByName(name, nullptr);
//    RwFrameForAllChildren(RpClumpGetFrame(clump), FindFrameFromNameCB, &searchInfo);
//    return searchInfo.m_pFrame;
}
