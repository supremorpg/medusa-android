//
// Created by x1y2z on 15.04.2023.
//

#include "FileLoader.h"
#include "../main.h"
#include "CFileMgr.h"
#include "GTASAEngineStreamingBindings.h"
#include "game/Models/ModelInfo.h"
#include "game/Enums/eItemDefinitionFlags.h"
#include "game/Models/AtomicModelInfo.h"
#include "SampStreamingApi.h"
#include "util.h"

namespace {
int32 (*g_LoadObjectNative)(const char*) = nullptr;
CEntityGTA* (*g_LoadObjectInstanceLineNative)(const char*) = nullptr;

const char* CurrentAbiLabel()
{
    return VER_x32 ? "32" : "64";
}

bool IsSampCustomObjectModel(int32 modelId)
{
    return (modelId >= 11682 && modelId <= 12799) ||
           (modelId >= 15065 && modelId <= 15999) ||
           (modelId >= 18631 && modelId <= 19999);
}

bool IsWatchedIplModel(int32 modelId)
{
    switch (modelId) {
        case 3438:
        case 8393:
        case 8485:
        case 8622:
        case 8697:
        case 8979:
        case 8980:
            return true;
        default:
            return false;
    }
}

void LogFileObjectInstanceLayoutOnce()
{
    static bool s_logged = false;
    if (s_logged) {
        return;
    }
    s_logged = true;

    FLog("[IPL_LAYOUT%s] sizeof=%zu pos=%zu rot=%zu rotXYZW=%zu/%zu/%zu/%zu model=%zu flags=%zu lod=%zu",
         CurrentAbiLabel(),
         sizeof(CFileObjectInstance),
         offsetof(CFileObjectInstance, m_vecPosition),
         offsetof(CFileObjectInstance, m_qRotation),
         offsetof(CFileObjectInstance, m_qRotation) + offsetof(CFileObjectRotation, x),
         offsetof(CFileObjectInstance, m_qRotation) + offsetof(CFileObjectRotation, y),
         offsetof(CFileObjectInstance, m_qRotation) + offsetof(CFileObjectRotation, z),
         offsetof(CFileObjectInstance, m_qRotation) + offsetof(CFileObjectRotation, w),
         offsetof(CFileObjectInstance, m_nModelId),
         offsetof(CFileObjectInstance, m_nInstanceType),
         offsetof(CFileObjectInstance, m_nLodInstanceIndex));
}

void LogWatchedIplObject(const CFileObjectInstance& instance, const char* modelName, CEntityGTA* entity)
{
    if (!IsWatchedIplModel(instance.m_nModelId)) {
        return;
    }

    static int s_logged = 0;
    if (s_logged >= 80) {
        return;
    }
    ++s_logged;

    CVector matForward{};
    CVector matUp{};
    bool hasMatrix = false;
    if (entity && entity->m_matrix) {
        hasMatrix = true;
        matForward = entity->m_matrix->GetForward();
        matUp = entity->m_matrix->GetUp();
    }

    FLog("[IPL_LOAD%s] model=%d name=%s pos=%.3f %.3f %.3f rot=%.6f %.6f %.6f %.6f area=%u lod=%d entity=%p mat=%d matF=%.3f %.3f %.3f matU=%.3f %.3f %.3f",
         CurrentAbiLabel(),
         instance.m_nModelId,
         modelName ? modelName : "",
         instance.m_vecPosition.x,
         instance.m_vecPosition.y,
         instance.m_vecPosition.z,
         instance.m_qRotation.x,
         instance.m_qRotation.y,
         instance.m_qRotation.z,
         instance.m_qRotation.w,
         static_cast<unsigned>(instance.m_nAreaCode),
         instance.m_nLodInstanceIndex,
         entity,
         hasMatrix ? 1 : 0,
         matForward.x, matForward.y, matForward.z,
         matUp.x, matUp.y, matUp.z);
}
}

// Load line into static buffer (`ms_line`)
char* CFileLoader::LoadLine(FILE* file) {
   // if(!fgets(ms_line, sizeof(ms_line), file))
     //   return nullptr;

  //  return ms_line;

    if (!CFileMgr::ReadLine(file, ms_line, sizeof(ms_line)))
        return nullptr;

    // Sanitize it (otherwise random crashes appear)
    for (char* it = ms_line; *it; it++) {
        // Have to cast to uint8, because signed ASCII is retarded
        if ((uint8)*it < (uint8)' ' || *it == ',')
            *it = ' ';
    }

    return FindFirstNonNullOrWS(ms_line);
}

bool CFileMgr::ReadLine(FILESTREAM file, char *str, int32 num)
{
    return fgets(str, num, file) != nullptr;
}

/*!
* Load line from a text buffer with sanitization (replaces chars < 32 (space) with a space)
* @param bufferIt Iterator into buffer. It is modified by this function to point after the last character of this line
* @param buffSize Size of buffer. It is modified to represent the size of the buffer remaining after the end of this line
* @returns The beginning of the line - Note, this isn't a pointer into the passed in buffer!
* @addr 0x536FE0
*/
char* CFileLoader::LoadLine(char*& bufferIt, int32& buffSize) {
    return GTASAEngineApi::FileLoaderLoadLineFromBufferNative(bufferIt, buffSize);
}

char* CFileLoader::FindFirstNonNullOrWS(char* it) {
    // Have to cast to uint8, because signed ASCII is retarded
    for (; *it && (uint8)*it <= (uint8)' '; it++);
    return it;
}

int32 CFileLoader::LoadObject(const char* line) {
    int32  modelId{ -1};
    char   modelName[24] = {};
    char   texName[24] = {};
    float  fDrawDist = 0.0f;
    uint32 nFlags = 0;

    if (sscanf(line, "%d", &modelId) != 1) {
        return LoadObjectNative(line);
    }

    if (!IsSampCustomObjectModel(modelId)) {
        return LoadObjectNative(line);
    }

    auto iNumRead = sscanf(line, "%d %23s %23s %f %u", &modelId, modelName, texName, &fDrawDist, &nFlags);
    if (iNumRead != 5 || fDrawDist < 4.0f)
    {
        int32 objType = 0;
        float fDrawDist2_unused = 0.0f, fDrawDist3_unused = 0.0f;
        iNumRead = sscanf(line, "%d %23s %23s %d", &modelId, modelName, texName, &objType);
        if (iNumRead != 4)
            return LoadObjectNative(line);

        switch (objType)
        {
            case 1:
                if (sscanf(line, "%d %23s %23s %d %f %u", &modelId, (modelName), (texName), &objType, &fDrawDist, &nFlags) < 5) {
                    return LoadObjectNative(line);
                }
                break;
            case 2:
                if (sscanf(line, "%d %23s %23s %d %f %f %u", &modelId, (modelName), (texName), &objType, &fDrawDist, &fDrawDist2_unused, &nFlags) != 7) {
                    return LoadObjectNative(line);
                }
                break;
            case 3:
                if (sscanf(line, "%d %23s %23s %d %f %f %f %u", &modelId, (modelName), (texName), &objType, &fDrawDist, &fDrawDist2_unused, &fDrawDist3_unused, &nFlags) != 8) {
                    return LoadObjectNative(line);
                }
                break;
            default:
                return LoadObjectNative(line);
        }
    }

    const uint32 parsedFlags = nFlags;
    sItemDefinitionFlags flags(parsedFlags);
    const auto mi = flags.bIsDamageable ? reinterpret_cast<CAtomicModelInfo *>(CModelInfo::AddDamageAtomicModel(modelId)) : CModelInfo::AddAtomicModel(modelId);
    mi->m_fDrawDistance = fDrawDist;

    mi->SetModelName(modelName);

    const char* db = IsSampCustomObjectModel(modelId) ? "samp" : Xyron::Streaming::GetModelCDName(modelId);
    mi->SetTexDictionary(texName, db);

    SetAtomicModelInfoFlags(mi, parsedFlags);

    static int s_sampObjectDefs = 0;
    ++s_sampObjectDefs;
    if (s_sampObjectDefs <= 8 || (s_sampObjectDefs % 200) == 0) {
        FLog("[OBJ_LOAD64] samp-def count=%d model=%d name=%s txd=%s db=%s flags=%u",
             s_sampObjectDefs,
             modelId,
             modelName,
             texName,
             db ? db : "",
             parsedFlags);
    }

    return modelId;
}

int32 CFileLoader::LoadObjectNative(const char* line) {
    if (g_LoadObjectNative) {
        return g_LoadObjectNative(line);
    }
    return GTASAEngineApi::FileLoaderLoadObjectNative(line);
}

extern int iBuildingToRemoveCount;
extern REMOVEBUILDING_DATA BuildingToRemove[1000];

CEntityGTA* CFileLoader::LoadObjectInstance1(const char* line) {
    char modelName[24] = {};
    CFileObjectInstance instance{};
    LogFileObjectInstanceLayoutOnce();
    if (sscanf(
            line,
            "%d %23s %d %f %f %f %f %f %f %f %d",
            &instance.m_nModelId,
            &modelName,
            &instance.m_nInstanceType,
            &instance.m_vecPosition.x,
            &instance.m_vecPosition.y,
            &instance.m_vecPosition.z,
            &instance.m_qRotation.x,
            &instance.m_qRotation.y,
            &instance.m_qRotation.z,
            &instance.m_qRotation.w,
            &instance.m_nLodInstanceIndex
    ) != 11) {
        if (g_LoadObjectInstanceLineNative) {
            return g_LoadObjectInstanceLineNative(line);
        }
        return nullptr;
    }

    if (iBuildingToRemoveCount >= 1) {
        for (int i = 0; i < iBuildingToRemoveCount; i++)
        {
            float fDistance = GetDistance(BuildingToRemove[i].vecPos, instance.m_vecPosition);
            if (fDistance <= BuildingToRemove[i].fRange) {
                if (BuildingToRemove[i].dwModel == -1 || instance.m_nModelId == (uint16_t) BuildingToRemove[i].dwModel) {
                    instance.m_nModelId = 19300;
                    break;
                }
            }
        }
    }

    CEntityGTA* entity = LoadObjectInstance(&instance, modelName);
    LogWatchedIplObject(instance, modelName, entity);
    return entity;
}

void CFileLoader::InjectHooks() {
    // Keep the native parser for base GTA objects, but force SA-MP/custom object defs
    // through the local path so their texture DB is resolved correctly on arm64.
    GTASAEngineApi::InstallFileLoaderHooksNative(
        reinterpret_cast<uintptr_t>(&CFileLoader::LoadObject),
        reinterpret_cast<void**>(&g_LoadObjectNative),
        reinterpret_cast<uintptr_t>(&CFileLoader::LoadObjectInstance1),
        reinterpret_cast<void**>(&g_LoadObjectInstanceLineNative));
}

CEntityGTA *CFileLoader::LoadObjectInstance(CFileObjectInstance *objInstance, const char *modelName) {
    return GTASAEngineApi::FileLoaderLoadObjectInstanceNative(objInstance, modelName);
}
