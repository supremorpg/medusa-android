#include "../main.h"
#include "game.h"
#include "../net/netgame.h"

#include "RW/RenderWare.h"
#include "SampStreamingApi.h"
#include "GTASAEngineApi.h"
#include "game/Models/ModelInfo.h"
#include "Timer.h"

#include <cmath>
#include <cstring>

extern CGame* pGame;
extern CNetGame* pNetGame;
extern MaterialTextGenerator* pMaterialTextGenerator;

namespace {
char LowerAscii(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
}

bool IsAsciiSpace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

bool IsAsciiHex(char ch)
{
    return (ch >= '0' && ch <= '9') ||
           (ch >= 'a' && ch <= 'f') ||
           (ch >= 'A' && ch <= 'F');
}

bool IsLoadingPlaceholderText(const char* text)
{
    if (!text) {
        return false;
    }

    char compact[64]{};
    size_t compactLen = 0;

    for (const char* cursor = text; *cursor && compactLen < sizeof(compact) - 1; ++cursor) {
        if (*cursor == '{' &&
            IsAsciiHex(cursor[1]) &&
            IsAsciiHex(cursor[2]) &&
            IsAsciiHex(cursor[3]) &&
            IsAsciiHex(cursor[4]) &&
            IsAsciiHex(cursor[5]) &&
            IsAsciiHex(cursor[6]) &&
            cursor[7] == '}') {
            cursor += 7;
            continue;
        }

        const char ch = LowerAscii(*cursor);
        if (ch >= 'a' && ch <= 'z') {
            compact[compactLen++] = ch;
        }
    }

    if (compactLen == 0 || compactLen > 24) {
        return false;
    }

    static constexpr char kLoading[] = "loading";
    for (size_t i = 0; i + sizeof(kLoading) - 1 <= compactLen; ++i) {
        bool matches = true;
        for (size_t j = 0; j < sizeof(kLoading) - 1; ++j) {
            if (compact[i + j] != kLoading[j]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }

    return false;
}

bool ShouldForceRenderObject(const CVector& pos)
{
    return pos.x < -3000.0f || pos.x > 3000.0f ||
           pos.y < -3000.0f || pos.y > 3000.0f;
}

void EnsureObjectModelLoaded(int modelId)
{
    if (!Xyron::Streaming::IsValidResourceId(modelId) || !CModelInfo::GetModelInfo(modelId)) {
        return;
    }

    if (!Xyron::Streaming::IsModelLoaded(modelId)) {
        FLog("[OBJ_FIX64] requesting model %d before object create", modelId);
        if (!Xyron::Streaming::TryLoadModel(modelId)) {
            FLog("[OBJ_FIX64] failed to stream model %d (object may miss texture/collision)", modelId);
        } else if (!Xyron::Streaming::IsModelLoaded(modelId)) {
            FLog("[OBJ_FIX64] model %d still not loaded after stream try", modelId);
        }
    }
}

bool HasSafeCollisionData(const CColModel* colModel)
{
    if (!colModel || !colModel->m_bHasCollisionVolumes || !colModel->m_pColData) {
        return false;
    }

    const CCollisionData* data = colModel->m_pColData;
    const uint32_t volumeCount =
        static_cast<uint32_t>(data->m_nNumSpheres) +
        static_cast<uint32_t>(data->m_nNumBoxes) +
        static_cast<uint32_t>(data->m_nNumLines) +
        static_cast<uint32_t>(data->m_nNumTriangles);
    if (volumeCount == 0) {
        return false;
    }

    if (data->m_nNumSpheres && !data->m_pSpheres) {
        return false;
    }
    if (data->m_nNumBoxes && !data->m_pBoxes) {
        return false;
    }
    if (data->m_nNumLines) {
        if (data->bUsesDisks) {
            if (!data->m_pDisks) {
                return false;
            }
        } else if (!data->m_pLines) {
            return false;
        }
    }
    if (data->m_nNumTriangles && (!data->m_pVertices || !data->m_pTriangles)) {
        return false;
    }

    return true;
}

bool HasSafeObjectCollision(const CPhysical* entity)
{
    if (!entity) {
        return false;
    }

    const int modelId = entity->m_nModelIndex;
    if (!Xyron::Streaming::IsValidResourceId(modelId)) {
        return false;
    }

    CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
    return modelInfo && HasSafeCollisionData(modelInfo->m_pColModel);
}

void SetObjectCollisionParticipation(CPhysical* entity, bool enabled)
{
    if (!entity) {
        return;
    }

    entity->SetCollisionChecking(enabled);
    entity->m_bCollisionProcessed = false;
    entity->m_bHasContacted = false;
    entity->m_bIsStuck = false;
    entity->m_bIsInSafePosition = false;
    entity->physicalFlags.bCollidable = enabled;
    entity->physicalFlags.bCanBeCollidedWith = enabled;
    entity->physicalFlags.bDisableSimpleCollision = !enabled;
    entity->physicalFlags.bProcessCollisionEvenIfStationary = enabled;
}

void RefreshObjectCollision(CPhysical* entity, const CVector& pos, bool forceStreaming = false)
{
    if (!entity) {
        return;
    }

    if (forceStreaming) {
        const int modelId = entity->m_nModelIndex;
        if (Xyron::Streaming::IsValidResourceId(modelId)) {
            EnsureObjectModelLoaded(modelId);
        }
        if (!entity->m_pRwObject) {
            entity->CreateRwObject();
        }
    }

    entity->m_bIsStatic = false;
    entity->m_bIsStaticWaitingForCollision = false;
    entity->m_bIsVisible = true;
    entity->m_bStreamingDontDelete = true;
    entity->m_bRemoveFromWorld = false;
    entity->m_bDontStream = true;

    Xyron::Streaming::WorldStreamRequest streamRequest{};
    streamRequest.areaCode = entity->m_nAreaCode;
    streamRequest.streamingFlags = STREAMING_DEFAULT;
    streamRequest.addModels = forceStreaming;
    streamRequest.addLods = forceStreaming;
    streamRequest.addIpls = forceStreaming;
    streamRequest.loadIpls = forceStreaming;
    streamRequest.ensureIpls = forceStreaming;
    streamRequest.loadSceneCollision = forceStreaming;
    streamRequest.reason = forceStreaming ? "object-refresh-force" : "object-refresh";
    Xyron::Streaming::PrepareWorldAtPoint(pos, streamRequest);

    const bool safeCollision = HasSafeObjectCollision(entity);
    SetObjectCollisionParticipation(entity, safeCollision);
    if (!safeCollision && forceStreaming) {
        static uint32_t s_lastUnsafeCollisionLogTick = 0;
        const uint32_t now = GetTickCount();
        if (now - s_lastUnsafeCollisionLogTick >= 750) {
            s_lastUnsafeCollisionLogTick = now;
            CBaseModelInfo* modelInfo = nullptr;
            CColModel* colModel = nullptr;
            CCollisionData* colData = nullptr;
            if (Xyron::Streaming::IsValidResourceId(entity->m_nModelIndex)) {
                modelInfo = CModelInfo::GetModelInfo(entity->m_nModelIndex);
                colModel = modelInfo ? modelInfo->m_pColModel : nullptr;
                colData = colModel ? colModel->m_pColData : nullptr;
            }
            FLog("[OBJ_FIX64] visual-only unsafe collision model=%d col=%d data=%d sph=%u box=%u line=%u tri=%u pos=%.1f %.1f %.1f",
                 entity->m_nModelIndex,
                 colModel ? 1 : 0,
                 colData ? 1 : 0,
                 colData ? static_cast<unsigned>(colData->m_nNumSpheres) : 0,
                 colData ? static_cast<unsigned>(colData->m_nNumBoxes) : 0,
                 colData ? static_cast<unsigned>(colData->m_nNumLines) : 0,
                 colData ? static_cast<unsigned>(colData->m_nNumTriangles) : 0,
                 pos.x,
                 pos.y,
                 pos.z);
        }
    }

    entity->UpdateRW();
    entity->UpdateRwFrame();
}

float DistanceSquared(const CVector& a, const CVector& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

bool IsReliablePlayerPosition(const CVector& pos)
{
    return std::isfinite(pos.x) &&
           std::isfinite(pos.y) &&
           std::isfinite(pos.z) &&
           !(std::fabs(pos.x) < 0.025f &&
             std::fabs(pos.y) < 0.025f &&
             std::fabs(pos.z) < 0.025f) &&
           pos.z > -1000.0f &&
           pos.z < 3000.0f;
}

bool GetLocalPlayerPosition(CVector& out)
{
    if (!pNetGame) {
        return false;
    }

    CPlayerPool* playerPool = pNetGame->GetPlayerPool();
    if (!playerPool) {
        return false;
    }

    CLocalPlayer* localPlayer = playerPool->GetLocalPlayer();
    if (!localPlayer || !localPlayer->GetPlayerPed() || !localPlayer->GetPlayerPed()->m_pPed) {
        return false;
    }

    static bool s_hasLastGoodPlayerPos = false;
    static CVector s_lastGoodPlayerPos{};
    static uint32_t s_lastGoodPlayerPosTick = 0;
    static uint32_t s_lastObjectPosFixLogTick = 0;

    CPlayerPed* playerPed = localPlayer->GetPlayerPed();
    CPedGTA* ped = playerPed->m_pPed;
    CVector pos = ped->m_matrix ? ped->m_matrix->GetPosition() : ped->GetPosition();
    if (!IsReliablePlayerPosition(pos)) {
        CVector fallback = ped->GetPosition();
        if (!IsReliablePlayerPosition(fallback) && playerPed->IsInVehicle()) {
            CVehicleGTA* vehicle = playerPed->GetGtaVehicle();
            if (vehicle) {
                fallback = vehicle->m_matrix ? vehicle->m_matrix->GetPosition() : vehicle->GetPosition();
            }
        }

        if (IsReliablePlayerPosition(fallback)) {
            pos = fallback;
        } else if (s_hasLastGoodPlayerPos && GetTickCount() - s_lastGoodPlayerPosTick < 15000) {
            pos = s_lastGoodPlayerPos;
        }

        const uint32_t now = GetTickCount();
        if (IsReliablePlayerPosition(pos) && now - s_lastObjectPosFixLogTick > 1200) {
            s_lastObjectPosFixLogTick = now;
            FLog("[OBJ_POS_FIX64] playerPos fallback %.3f %.3f %.3f", pos.x, pos.y, pos.z);
        }
    }

    if (IsReliablePlayerPosition(pos)) {
        s_hasLastGoodPlayerPos = true;
        s_lastGoodPlayerPos = pos;
        s_lastGoodPlayerPosTick = GetTickCount();
    }

    out = pos;
    return true;
}

size_t BoundedStringLength(const char* value, size_t limit)
{
    if (!value) {
        return 0;
    }

    size_t length = 0;
    while (length < limit && value[length] != '\0') {
        ++length;
    }
    return length;
}
}

CObject::CObject(int iModel, CVector vecPos, CVector vecRot, float fDrawDistance, uint8_t bAttached)
{
    if(!CModelInfo::GetModelInfo(iModel)) {
        FLog("[OBJ_FIX64] invalid model %d, fallback to 18631", iModel);
        iModel = 18631; // ????????
    }
    EnsureObjectModelLoaded(iModel);

	m_AttachedVehicleID = INVALID_VEHICLE_ID;
	m_AttachedObjectID = INVALID_OBJECT_ID;
	m_bAttachedToPed = bAttached;

	m_pEntity = 0;
	m_dwGTAId = 0;
    m_iModel = iModel;
    m_fDrawDistance = fDrawDistance;
    m_vecLastKnownPos = vecPos;
    m_vecLastKnownRot = vecRot;
    m_byteMoving = 0;
    m_fMoveSpeed = 0.0f;
    m_bNeedRotate = false;
    m_dwMoveTick = 0;
    m_dwLastCollisionRefreshTick = 0;
    m_dwLastEntityRecoverTick = 0;
    m_dwLastHeavyRecoverTick = 0;
    m_byteEntityRecoverAttempts = 0;

	m_vecAttachedPos.x = 0.0f;
	m_vecAttachedPos.y = 0.0f;
	m_vecAttachedPos.z = 0.0f;
	m_vecAttachedRot.x = 0.0f;
	m_vecAttachedRot.y = 0.0f;
	m_vecAttachedRot.z = 0.0f;
	m_bSyncRotation = true;
    for (int i = 0; i < 16; i++)
    {
        m_MaterialTexture[i] = 0;
        m_MaterialTextTexture[i] = 0;
        m_dwMaterialColor[i] = 0;
        m_iMaterialType[i] = 0;
        m_szMaterialText[i] = nullptr;
        m_iMaterialSize[i] = 0;
        m_iMaterialFontSize[i] = 0;
        m_dwMaterialFontColor[i] = 0;
        m_dwMaterialBackColor[i] = 0;
        m_iMaterialTextAlign[i] = 0;
    }
    m_bHasMaterial = false;
    m_bHasMaterialText = false;
    m_bForceRender = ShouldForceRenderObject(vecPos);

	ScriptCommand(&create_object, iModel, vecPos.x, vecPos.y, vecPos.z, &m_dwGTAId);
    if(!m_dwGTAId) {
        FLog("[OBJ_FIX64] create_object failed model=%d pos=%.2f %.2f %.2f",
             iModel, vecPos.x, vecPos.y, vecPos.z);
        return;
    }

    ScriptCommand(&put_object_at, m_dwGTAId, vecPos.x, vecPos.y, vecPos.z);

	m_pEntity = GamePool_Object_GetAt(m_dwGTAId);

    if(!m_pEntity) {
        FLog("[OBJ_FIX64] GamePool_Object_GetAt failed gtaId=%u model=%d", m_dwGTAId, iModel);
        return;
    }
    RefreshObjectCollision(m_pEntity, vecPos, true);

    /*m_Matrix = m_pEntity->GetMatrix().ToRwMatrix();
    m_Matrix.pos.x = vecPos.x;
    m_Matrix.pos.y = vecPos.y;
    m_Matrix.pos.z = vecPos.z;
    m_pEntity->SetMatrix((CMatrix&)m_Matrix);*/
    InstantRotate(vecRot.x, vecRot.y ,vecRot.z);

	for (int i = 0; i < 16; i++)
	{
		m_MaterialTexture[i] = 0;
		m_MaterialTextTexture[i] = 0;
		m_dwMaterialColor[i] = 0;
		m_iMaterialType[i] = 0;

		/* material text */
		m_szMaterialText[i] = nullptr;
		m_iMaterialSize[i] = 0;
		m_iMaterialFontSize[i] = 0;
		m_dwMaterialFontColor[i] = 0;
		m_dwMaterialBackColor[i] = 0;
		m_iMaterialTextAlign[i] = 0;
	}
	m_bHasMaterial = false;
	m_bHasMaterialText = false;

	m_bAttachedToPed = bAttached;

	m_bForceRender = ShouldForceRenderObject(vecPos);
}

CObject::~CObject()
{
    if(m_pEntity) {
        const int modelId = m_pEntity->m_nModelIndex;
        ScriptCommand(&destroy_object, m_dwGTAId);
        Xyron::Streaming::RemoveModelIfNoRefs(modelId);
    }

	for (int i = 0; i < 16; i++)
	{
		if (m_szMaterialText[i] != nullptr) {
			delete[] m_szMaterialText[i];
			m_szMaterialText[i] = nullptr;
		}
	}
}

bool CObject::ShouldSkipCollisionRepair() const
{
    return m_bAttachedToPed ||
           m_AttachedVehicleID != INVALID_VEHICLE_ID ||
           m_AttachedObjectID != INVALID_OBJECT_ID;
}

bool CObject::EnsureEntityAvailable(const char* reason, bool forceRecreate)
{
    CPhysical* pooledEntity = nullptr;
    if (m_dwGTAId != 0) {
        pooledEntity = GamePool_Object_GetAt(m_dwGTAId);
    }

    if (pooledEntity) {
        const bool wasMissing = (m_pEntity == nullptr);
        m_pEntity = pooledEntity;
        m_byteEntityRecoverAttempts = 0;

        if (wasMissing || forceRecreate) {
            RefreshObjectCollision(m_pEntity, m_vecLastKnownPos);
            if (m_vecLastKnownRot.x != 0.0f || m_vecLastKnownRot.y != 0.0f || m_vecLastKnownRot.z != 0.0f) {
                InstantRotate(m_vecLastKnownRot.x, m_vecLastKnownRot.y, m_vecLastKnownRot.z);
            }
            FLog("[OBJ_FIX64] entity recovered reason=%s gta=%u model=%d pos=%.1f %.1f %.1f",
                 reason ? reason : "unknown",
                 m_dwGTAId,
                 m_iModel,
                 m_vecLastKnownPos.x,
                 m_vecLastKnownPos.y,
                 m_vecLastKnownPos.z);
        }

        return true;
    }

    m_pEntity = nullptr;

    const uint32_t now = GetTickCount();
    if (!forceRecreate && now - m_dwLastEntityRecoverTick < 700) {
        return false;
    }
    m_dwLastEntityRecoverTick = now;
    ++m_byteEntityRecoverAttempts;

    EnsureObjectModelLoaded(m_iModel);
    Xyron::Streaming::WorldStreamRequest recoverRequest{};
    recoverRequest.areaCode = AREA_CODE_NORMAL_WORLD;
    recoverRequest.streamingFlags = STREAMING_DEFAULT;
    recoverRequest.refreshGame = true;
    recoverRequest.reason = "object-entity-recover";
    Xyron::Streaming::PrepareWorldAtPoint(m_vecLastKnownPos, recoverRequest);

    const bool mustRecreate = forceRecreate || m_dwGTAId == 0 || m_byteEntityRecoverAttempts >= 3;
    if (!mustRecreate) {
        return false;
    }

    if (m_dwGTAId != 0) {
        ScriptCommand(&destroy_object, m_dwGTAId);
    }

    uint32_t newGtaId = 0;
    ScriptCommand(&create_object, m_iModel, m_vecLastKnownPos.x, m_vecLastKnownPos.y, m_vecLastKnownPos.z, &newGtaId);
    if (!newGtaId) {
        FLog("[OBJ_FIX64] entity recreate failed reason=%s model=%d pos=%.1f %.1f %.1f attempts=%u",
             reason ? reason : "unknown",
             m_iModel,
             m_vecLastKnownPos.x,
             m_vecLastKnownPos.y,
             m_vecLastKnownPos.z,
             m_byteEntityRecoverAttempts);
        return false;
    }

    m_dwGTAId = newGtaId;
    ScriptCommand(&put_object_at, m_dwGTAId, m_vecLastKnownPos.x, m_vecLastKnownPos.y, m_vecLastKnownPos.z);
    m_pEntity = GamePool_Object_GetAt(m_dwGTAId);
    if (!m_pEntity) {
        FLog("[OBJ_FIX64] entity recreate unresolved reason=%s gta=%u model=%d",
             reason ? reason : "unknown",
             m_dwGTAId,
             m_iModel);
        return false;
    }

    RefreshObjectCollision(m_pEntity, m_vecLastKnownPos);
    InstantRotate(m_vecLastKnownRot.x, m_vecLastKnownRot.y, m_vecLastKnownRot.z);
    m_byteEntityRecoverAttempts = 0;
    FLog("[OBJ_FIX64] entity recreated reason=%s gta=%u model=%d pos=%.1f %.1f %.1f",
         reason ? reason : "unknown",
         m_dwGTAId,
         m_iModel,
         m_vecLastKnownPos.x,
         m_vecLastKnownPos.y,
         m_vecLastKnownPos.z);
    return true;
}

void CObject::EnsureCollisionState(uint16_t objectId, const char* reason, bool forceLog)
{
    (void)objectId;

    if (!EnsureEntityAvailable(reason, false) || !m_pEntity || ShouldSkipCollisionRepair()) {
        return;
    }

    const uint32_t now = GetTickCount();
    if (!forceLog && now - m_dwLastCollisionRefreshTick < 1500) {
        return;
    }
    m_dwLastCollisionRefreshTick = now;

    const CVector pos = m_pEntity->GetPosition();
    m_vecLastKnownPos = pos;
    const bool hadUsesCollision = m_pEntity->m_bUsesCollision;
    const bool hadCollidable = m_pEntity->physicalFlags.bCollidable;
    const bool hadCanBeCollidedWith = m_pEntity->physicalFlags.bCanBeCollidedWith;
    const bool hadSimpleCollision = !m_pEntity->physicalFlags.bDisableSimpleCollision;
    const bool needsRepair = !hadUsesCollision || !hadCollidable || !hadCanBeCollidedWith || !hadSimpleCollision;

    CVector playerPos;
    const bool hasPlayerPos = GetLocalPlayerPosition(playerPos);
    const bool nearPlayer = !hasPlayerPos || DistanceSquared(pos, playerPos) <= 80.0f * 80.0f;

    if (!forceLog && !nearPlayer && !needsRepair) {
        return;
    }

    if (needsRepair) {
        RefreshObjectCollision(m_pEntity, pos);
    } else {
        Xyron::Streaming::WorldStreamRequest streamRequest{};
        streamRequest.areaCode = m_pEntity->m_nAreaCode;
        streamRequest.streamingFlags = STREAMING_DEFAULT;
        streamRequest.addModels = false;
        streamRequest.addLods = false;
        streamRequest.addIpls = false;
        streamRequest.loadIpls = false;
        streamRequest.ensureIpls = false;
        streamRequest.loadSceneCollision = false;
        streamRequest.reason = "object-collision-keepalive";
        Xyron::Streaming::PrepareWorldAtPoint(pos, streamRequest);
    }

    if ((forceLog || needsRepair) && now - m_dwLastHeavyRecoverTick >= 1800) {
        m_dwLastHeavyRecoverTick = now;
        Xyron::Streaming::WorldStreamRequest streamRequest{};
        streamRequest.areaCode = m_pEntity->m_nAreaCode;
        streamRequest.streamingFlags = STREAMING_DEFAULT;
        streamRequest.refreshGame = true;
        streamRequest.reason = "object-collision-heavy";
        Xyron::Streaming::PrepareWorldAtPoint(pos, streamRequest);
    }

    if (forceLog || needsRepair) {
        FLog("[OBJ_FIX64] coll-heal reason=%s model=%d gta=%u pos=%.1f %.1f %.1f repaired=%d flagsBefore={use:%d,col:%d,hit:%d,simple:%d}",
             reason ? reason : "unknown",
             m_iModel,
             m_dwGTAId,
             pos.x,
             pos.y,
             pos.z,
             needsRepair ? 1 : 0,
             hadUsesCollision ? 1 : 0,
             hadCollidable ? 1 : 0,
             hadCanBeCollidedWith ? 1 : 0,
             hadSimpleCollision ? 1 : 0);
    }
}

void CObject::Process(float fElapsedTime)
{
    if (!m_pEntity || !GamePool_Object_GetAt(m_dwGTAId)) {
        EnsureEntityAvailable("process", false);
    }

	if (m_AttachedVehicleID != INVALID_VEHICLE_ID)
	{
		if (pNetGame)
		{
			CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
			if (pVehiclePool)
			{
				CVehicle* pVehicle = pVehiclePool->GetAt(m_AttachedVehicleID);
				if (pVehicle)
				{
					if (pVehicle->m_pVehicle->IsAdded()) {
						this->AttachToVehicle(pVehicle);
					}
				}
			}
		}
		return;
	}

	if (m_AttachedObjectID != INVALID_OBJECT_ID)
	{
		if (pNetGame)
		{
			CObjectPool* pObjectPool = pNetGame->GetObjectPool();
			if (pObjectPool)
			{
				CObject* pObject = pObjectPool->GetAt(m_AttachedObjectID);
				if (pObject) {
					this->AttachToObject(pObject);
				}
			}
		}

		return;
	}

	if (m_byteMoving & 1)
	{
        if (!m_pEntity) {
            return;
        }

		CVector vecSpeed = { 0.0f, 0.0f, 0.0f };
		RwMatrix matEnt;
        matEnt = m_pEntity->GetMatrix().ToRwMatrix();
		float distance = fElapsedTime * m_fMoveSpeed;
		float remaining = DistanceRemaining(&matEnt);
		uint32_t dwThisTick = GetTickCount();

		float posX = matEnt.pos.x;
		float posY = matEnt.pos.y;
		float posZ = matEnt.pos.z;

		float f1 = ((float)(dwThisTick - m_dwMoveTick)) * 0.001f * m_fMoveSpeed;
		float f2 = m_fDistanceToTargetPoint - remaining;

		if (distance >= remaining)
		{
            m_pEntity->SetVelocity(vecSpeed);
            m_pEntity->SetTurnSpeed(vecSpeed);
			matEnt.pos.x = m_matTarget.pos.x;
			matEnt.pos.y = m_matTarget.pos.y;
			matEnt.pos.z = m_matTarget.pos.z;
			if (m_bNeedRotate) {
				m_quatTarget.GetMatrix(reinterpret_cast<RwMatrix *>(&matEnt));
			}
			m_pEntity->SetMatrix((CMatrix&)matEnt);
            m_pEntity->UpdateRW();
            m_pEntity->UpdateRwFrame();

            m_pEntity->Add();
            m_vecLastKnownPos = CVector(matEnt.pos.x, matEnt.pos.y, matEnt.pos.z);
            EnsureCollisionState(INVALID_OBJECT_ID, "move-end", true);
			StopMoving();
			return;
		}

		if (fElapsedTime <= 0.0f)
			return;

		float delta = 1.0f / (remaining / distance);
		matEnt.pos.x += ((m_matTarget.pos.x - matEnt.pos.x) * delta);
		matEnt.pos.y += ((m_matTarget.pos.y - matEnt.pos.y) * delta);
		matEnt.pos.z += ((m_matTarget.pos.z - matEnt.pos.z) * delta);

		distance = remaining / m_fDistanceToTargetPoint;
		float slerpDelta = 1.0f - distance;

		delta = 1.0f / fElapsedTime;
		vecSpeed.x = (matEnt.pos.x - posX) * delta * 0.02f;
		vecSpeed.y = (matEnt.pos.y - posY) * delta * 0.02f;
		vecSpeed.z = (matEnt.pos.z - posZ) * delta * 0.02f;

		if (FloatOffset(f1, f2) > 0.1f)
		{
			if (f1 > f2)
			{
				delta = (f1 - f2) * 0.1f + 1.0f;
				vecSpeed.x *= delta;
				vecSpeed.y *= delta;
				vecSpeed.z *= delta;
			}

			if (f2 > f1)
			{
				delta = 1.0f - (f2 - f1) * 0.1f;
				vecSpeed.x *= delta;
				vecSpeed.y *= delta;
				vecSpeed.z *= delta;
			}
		}

		m_pEntity->SetVelocity(vecSpeed);
        m_pEntity->ApplyMoveSpeed();

		if (m_bNeedRotate)
		{
			float fx, fy, fz;
			GetRotation(&fx, &fy, &fz);
			distance = m_vecRotationTarget.z - distance * m_vecSubRotationTarget.z;
			vecSpeed.x = 0.0f;
			vecSpeed.y = 0.0f;
			vecSpeed.z = subAngle(remaining, distance) * 0.01f;
			if (vecSpeed.z <= 0.001f)
			{
				if (vecSpeed.z < -0.001f)
					vecSpeed.z = -0.001f;
			}
			else
			{
				vecSpeed.z = 0.001f;
			}

            m_pEntity->SetTurnSpeed(vecSpeed);
            matEnt = m_pEntity->GetMatrix().ToRwMatrix();
			CQuaternion quat;
			quat.Slerp(&m_quatStart, &m_quatTarget, slerpDelta);
			quat.Normalize();
			quat.GetMatrix(reinterpret_cast<RwMatrix *>(&matEnt));
		}
		else
		{
            matEnt = m_pEntity->GetMatrix().ToRwMatrix();
		}

        m_pEntity->Remove();

		m_pEntity->SetMatrix((CMatrix&)matEnt);
        m_pEntity->UpdateRW();
        m_pEntity->UpdateRwFrame();

        m_pEntity->Add();
        m_vecLastKnownPos = CVector(matEnt.pos.x, matEnt.pos.y, matEnt.pos.z);
        EnsureCollisionState(INVALID_OBJECT_ID, "move-process");
	}
}

// 0.3.7
void CObject::SetRotation(CVector * vecRotation)
{
	if (m_pEntity && GamePool_Object_GetAt(m_dwGTAId))
	{
		ScriptCommand(&set_object_rotation, m_dwGTAId, vecRotation->x, vecRotation->y, vecRotation->z);
		m_vecRotation.x = vecRotation->x;
		m_vecRotation.y = vecRotation->y;
		m_vecRotation.z = vecRotation->z;
        m_vecLastKnownRot = *vecRotation;
	}
}

// Converts degrees to radians
// keywords: 0.017453292 flt_8595EC
constexpr float DegreesToRadians(float angleInDegrees) {
    return angleInDegrees * PI / 180.0F;
}

void CObject::InstantRotate(float x, float y, float z)
{
    if(!m_pEntity)return;
    m_vecLastKnownRot = CVector(x, y, z);
    x = DegreesToRadians(x);
    y = DegreesToRadians(y);
    z = DegreesToRadians(z);

    m_pEntity->Remove();

    m_pEntity->SetOrientation(x, y, z);

    m_pEntity->UpdateRW();
    m_pEntity->UpdateRwFrame();

    m_pEntity->Add();
    EnsureCollisionState(INVALID_OBJECT_ID, "rotate", true);
}

// 0.3.7
void CObject::GetRotation(float* pfX, float* pfY, float* pfZ)
{
    if (!m_pEntity) return;

    m_pEntity->m_matrix->ConvertToEulerAngles(pfX, pfY, pfZ, 21);

    *pfX = *pfX * 57.295776 * -1.0;
    *pfY = *pfY * 57.295776 * -1.0;
    *pfZ = *pfZ * 57.295776 * -1.0;
}
// 0.3.7
void CObject::RotateMatrix(CVector vecRot)
{
	m_vecRotation = vecRot;

	vecRot.x *= 0.017453292f; // x * pi/180
	vecRot.y *= 0.017453292f; // y * pi/180
	vecRot.z *= 0.017453292f; // z * pi/180

	float cosx = cos(vecRot.x);
	float sinx = sin(vecRot.x);

	float cosy = cos(vecRot.y);
	float siny = sin(vecRot.y);

	float cosz = cos(vecRot.z);
	float sinz = sin(vecRot.z);

	float sinzx = sinz * sinx;
	float coszx = cosz * sinx;

	m_matTarget.right.x = cosz * cosy - sinzx * siny;
	m_matTarget.right.y = coszx * siny + sinz * cosy;
	m_matTarget.right.z = -(siny * cosx);
	m_matTarget.up.x = -(sinz * cosx);
	m_matTarget.up.y = cosz * cosx;
	m_matTarget.up.z = sinx;
	m_matTarget.at.x = sinzx * cosy + cosz * siny;
	m_matTarget.at.y = sinz * siny - coszx * cosy;
	m_matTarget.at.z = cosy * cosx;
}
// 0.3.7
void CObject::ApplyMoveSpeed()
{
	if (m_pEntity)
	{
		float fTimeStep = CTimer::GetTimeStep();

		RwMatrix mat;
        mat = m_pEntity->GetMatrix().ToRwMatrix();
		mat.pos.x += fTimeStep * m_pEntity->GetMoveSpeed().x;
		mat.pos.y += fTimeStep * m_pEntity->GetMoveSpeed().y;
		mat.pos.z += fTimeStep * m_pEntity->GetMoveSpeed().z;
		m_pEntity->SetMatrix((CMatrix&)mat);
	}
}
// 0.3.7
float CObject::DistanceRemaining(RwMatrix* matPos)
{
	float	fSX, fSY, fSZ;
	fSX = (matPos->pos.x - m_matTarget.pos.x) * (matPos->pos.x - m_matTarget.pos.x);
	fSY = (matPos->pos.y - m_matTarget.pos.y) * (matPos->pos.y - m_matTarget.pos.y);
	fSZ = (matPos->pos.z - m_matTarget.pos.z) * (matPos->pos.z - m_matTarget.pos.z);
	return (float)sqrt(fSX + fSY + fSZ);
}

void CObject::SetMaterial(int iModel, int iMaterialIndex, char* txdname, char* texturename, uint32_t dwColor)
{
    const char* safeTxd = txdname ? txdname : "<null>";
    const char* safeTexture = texturename ? texturename : "<null>";
	FLog("SetMaterial: model: %d, %s, %s", iModel, safeTxd, safeTexture);

	if (iMaterialIndex < 0 || iMaterialIndex >= 16) {
        FLog("[OBJ_FIX64] invalid material index %d for model %d", iMaterialIndex, iModel);
        return;
    }

    if (!texturename || !texturename[0] ||
        !strcmp(safeTexture, "INVALID") || !strcmp(safeTexture, "invalid")) {
        FLog("[OBJ_FIX64] skip invalid material payload: model=%d index=%d txd=%s tex=%s",
             iModel, iMaterialIndex, safeTxd, safeTexture);
        return;
    }

	if (m_MaterialTexture[iMaterialIndex]) {
		RwTextureDestroy(reinterpret_cast<RwTexture *>(m_MaterialTexture[iMaterialIndex]));
		m_MaterialTexture[iMaterialIndex] = 0;
	}

    RwTexture* resolvedTexture = nullptr;
    if (txdname && txdname[0] && texturename && texturename[0]) {
        resolvedTexture = LoadTextureFromTxd(txdname, texturename);
        if (resolvedTexture) {
            FLog("[OBJ_FIX64] material resolved via txd: model=%d index=%d txd=%s tex=%s",
                 iModel, iMaterialIndex, txdname, texturename);
        }
    }

    if (!resolvedTexture && texturename && texturename[0]) {
        resolvedTexture = reinterpret_cast<RwTexture*>(LoadTexture(texturename));
        if (resolvedTexture) {
            FLog("[OBJ_FIX64] material resolved via fallback db search: model=%d index=%d tex=%s",
                 iModel, iMaterialIndex, texturename);
        }
    }

    if (!resolvedTexture) {
        FLog("[OBJ_FIX64] material texture missing: model=%d index=%d txd=%s tex=%s",
             iModel, iMaterialIndex, safeTxd, safeTexture);
    }

	m_MaterialTexture[iMaterialIndex] = reinterpret_cast<uintptr_t>(resolvedTexture);
	m_dwMaterialColor[iMaterialIndex] = dwColor;
	m_iMaterialType[iMaterialIndex] = MATERIAL_TYPE_MATERIAL;
	m_bHasMaterial = true;
}

void CObject::SetMaterialText(int index, char* text, int materialSize, char* fontname, int fontSize, bool bold,
	uint32_t dwFontColor, uint32_t dwBackColor, int textAlignment)
{
    (void)bold;

	if (index < 0 || index >= 16) {
        FLog("[OBJ_FIX64] invalid material text index %d model=%d", index, m_iModel);
        return;
    }

    const char* safeText = text ? text : "";
    const size_t kMaxMaterialText = 2047;
    const size_t textLength = BoundedStringLength(safeText, kMaxMaterialText);
    if (!text) {
        FLog("[OBJ_FIX64] null material text model=%d index=%d", m_iModel, index);
    } else if (safeText[textLength] != '\0') {
        FLog("[OBJ_FIX64] material text truncated model=%d index=%d len>=%zu",
             m_iModel, index, kMaxMaterialText);
    }

	if (m_MaterialTextTexture[index]) {
        RwTextureDestroy(reinterpret_cast<RwTexture *>(m_MaterialTextTexture[index]));
		m_MaterialTextTexture[index] = 0;
	}

    m_MaterialTextIndex = index;

	m_dwMaterialColor[index] = 0;
	m_iMaterialType[index] = MATERIAL_TYPE_TEXT;

	if (m_szMaterialText[index] != nullptr) {
		delete[] m_szMaterialText[index];
		m_szMaterialText[index] = nullptr;
	}

	m_szMaterialText[index] = new char[textLength + 1];
    memcpy(m_szMaterialText[index], safeText, textLength);
    m_szMaterialText[index][textLength] = '\0';

	m_iMaterialSize[index] = materialSize;
	m_iMaterialFontSize[index] = fontSize;
	m_dwMaterialFontColor[index] = dwFontColor;
	m_dwMaterialBackColor[index] = dwBackColor;
	m_iMaterialTextAlign[index] = textAlignment;

    if (IsLoadingPlaceholderText(m_szMaterialText[index])) {
        FLog("[OBJ_PLACEHOLDER64] registered Loading material-text model=%d index=%d size=%d font=%d",
             m_iModel,
             index,
             materialSize,
             fontSize);
    }
}

void CObject::ProcessMaterialText()
{
	for (int i = 0; i < 16; i++)
	{
		if (m_iMaterialType[i] == MATERIAL_TYPE_TEXT && m_MaterialTextTexture[i] == 0)
		{
            if (IsLoadingPlaceholderText(m_szMaterialText[i])) {
                m_bHasMaterialText = true;
                continue;
            }

			m_iMaterialFontSize[i]*=0.75f;
			m_MaterialTextTexture[i] = reinterpret_cast<uintptr_t>(pMaterialTextGenerator->Generate(
                    m_szMaterialText[i], m_iMaterialSize[i], m_iMaterialFontSize[i],
                    false, m_dwMaterialFontColor[i], m_dwMaterialBackColor[i],
                    m_iMaterialTextAlign[i]));
			m_bHasMaterialText = true;
		}
	}
}

bool CObject::HasVisiblePlaceholderMaterialText() const
{
    for (int i = 0; i < 16; ++i) {
        if (m_iMaterialType[i] == MATERIAL_TYPE_TEXT && IsLoadingPlaceholderText(m_szMaterialText[i])) {
            return true;
        }
    }

    return false;
}

// 0.3.7
void CObject::MoveTo(float fX, float fY, float fZ, float fSpeed, float fRotX, float fRotY, float fRotZ)
{
    if (!EnsureEntityAvailable("move-start", false) || !m_pEntity) {
        FLog("[OBJ_FIX64] MoveTo skipped (entity missing) model=%d gta=%u target=%.1f %.1f %.1f",
             m_iModel, m_dwGTAId, fX, fY, fZ);
        return;
    }

	RwMatrix mat;
    mat = m_pEntity->GetMatrix().ToRwMatrix();

	if (m_byteMoving & 1) {
		this->StopMoving();
		mat.pos.x = m_matTarget.pos.x;
		mat.pos.y = m_matTarget.pos.y;
		mat.pos.z = m_matTarget.pos.z;

		if (m_bNeedRotate) {
			m_quatTarget.GetMatrix(reinterpret_cast<RwMatrix *>(&mat));
		}

        m_pEntity->Remove();

		m_pEntity->SetMatrix((CMatrix&)mat);
        m_pEntity->UpdateRW();
        m_pEntity->UpdateRwFrame();

        m_pEntity->Add();
        RefreshObjectCollision(m_pEntity, CVector(mat.pos.x, mat.pos.y, mat.pos.z));
	}

	m_dwMoveTick = GetTickCount();
	m_fMoveSpeed = fSpeed;
	m_matTarget.pos.x = fX;
	m_matTarget.pos.y = fY;
	m_matTarget.pos.z = fZ;
    m_vecLastKnownPos = CVector(fX, fY, fZ);
	m_byteMoving |= 1;

	if (fRotX <= -999.0f || fRotY <= -999.0f || fRotZ <= -999.0f) {
		m_bNeedRotate = false;
	}
	else
	{
		m_bNeedRotate = true;

		CVector vecRot;
		RwMatrix matrix;
		this->GetRotation(&vecRot.x, &vecRot.y, &vecRot.z);
		m_vecRotationTarget.x = fixAngle(fRotX);
		m_vecRotationTarget.y = fixAngle(fRotY);
		m_vecRotationTarget.z = fixAngle(fRotZ);

		m_vecSubRotationTarget.x = subAngle(vecRot.x, fRotX);
		m_vecSubRotationTarget.y = subAngle(vecRot.y, fRotY);
		m_vecSubRotationTarget.z = subAngle(vecRot.z, fRotZ);

		this->RotateMatrix(CVector{ fRotX, fRotY, fRotZ });
        matrix = m_pEntity->GetMatrix().ToRwMatrix();
		m_quatStart.SetFromMatrix(&matrix);
		m_quatTarget.SetFromMatrix(&m_matTarget);
		m_quatStart.Normalize();
		m_quatTarget.Normalize();
	}

	m_fDistanceToTargetPoint = m_pEntity->GetDistanceFromPoint(m_matTarget.pos.x, m_matTarget.pos.y,
                                                               m_matTarget.pos.z);
    m_pEntity->m_bHasContacted = false;
    EnsureCollisionState(INVALID_OBJECT_ID, "move-start", true);

	if (pNetGame) {
		CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
		if (pPlayerPool) {
			//pPlayerPool->GetLocalPlayer()->UpdateSurfing();
		}
	}

	// sub_1009F070
}
// 0.3.7
void CObject::StopMoving()
{
	CVector vec = { 0.0f, 0.0f, 0.0f };
    if (m_pEntity) {
	    m_pEntity->SetVelocity(vec);
	    m_pEntity->SetTurnSpeed(vec);
    }

	m_byteMoving &= ~1;
}

// 0.3.7
void CObject::SetAttachedObject(uint16_t ObjectID, CVector* vecPos, CVector* vecRot, bool bSyncRotation)
{
	if (ObjectID == INVALID_OBJECT_ID)
	{
		m_AttachedObjectID = INVALID_OBJECT_ID;
		m_vecAttachedPos.x = 0.0f;
		m_vecAttachedPos.y = 0.0f;
		m_vecAttachedPos.z = 0.0f;
		m_vecAttachedRot.x = 0.0f;
		m_vecAttachedRot.y = 0.0f;
		m_vecAttachedRot.z = 0.0f;
		m_bSyncRotation = false;
	}
	else
	{
		m_AttachedObjectID = ObjectID;
		m_vecAttachedPos.x = vecPos->x;
		m_vecAttachedPos.y = vecPos->y;
		m_vecAttachedPos.z = vecPos->z;
		m_vecAttachedRot.x = vecRot->x;
		m_vecAttachedRot.y = vecRot->y;
		m_vecAttachedRot.z = vecRot->z;
		m_bSyncRotation = bSyncRotation;
	}
}
// 0.3.7
void CObject::SetAttachedVehicle(uint16_t VehicleID, CVector* vecPos, CVector* vecRot)
{
	if (VehicleID == INVALID_VEHICLE_ID)
	{
		m_AttachedVehicleID = INVALID_VEHICLE_ID;
		m_vecAttachedPos.x = 0.0f;
		m_vecAttachedPos.y = 0.0f;
		m_vecAttachedPos.z = 0.0f;
		m_vecAttachedRot.x = 0.0f;
		m_vecAttachedRot.y = 0.0f;
		m_vecAttachedRot.z = 0.0f;
	}
	else
	{
		m_AttachedVehicleID = VehicleID;
		m_vecAttachedPos.x = vecPos->x;
		m_vecAttachedPos.y = vecPos->y;
		m_vecAttachedPos.z = vecPos->z;
		m_vecAttachedRot.x = vecRot->x;
		m_vecAttachedRot.y = vecRot->y;
		m_vecAttachedRot.z = vecRot->z;
	}
} 
// 0.3.7
void CObject::AttachToVehicle(CVehicle* pVehicle)
{
    if (GamePool_Object_GetAt(m_dwGTAId)) {
        if (!ScriptCommand(&is_object_attached, m_dwGTAId)) {
            ScriptCommand(&attach_object_to_car,
                          m_dwGTAId,
                          pVehicle->m_dwGTAId,
                          m_vecAttachedPos.x,
                          m_vecAttachedPos.y,
                          m_vecAttachedPos.z,
                          m_vecAttachedRot.x,
                          m_vecAttachedRot.y,
                          m_vecAttachedRot.z);
        }
    }
}
// 0.3.7
void CObject::AttachToObject(CObject* pObject)
{
    if (GamePool_Object_GetAt(m_dwGTAId)) {
        if (!ScriptCommand(&is_object_attached, m_dwGTAId)) {
            ScriptCommand(&attach_object_to_object,
                          m_dwGTAId,
                          pObject->m_dwGTAId,
                          m_vecAttachedPos.x,
                          m_vecAttachedPos.y,
                          m_vecAttachedPos.z,
                          m_vecAttachedRot.x,
                          m_vecAttachedRot.y,
                          m_vecAttachedRot.z);
        }
    }
} 

bool CObject::AttachedToMovingEntity()
{
	if(m_AttachedObjectID == INVALID_OBJECT_ID)
	{
		if(m_AttachedVehicleID != INVALID_VEHICLE_ID)
			return true;

		return (m_byteMoving & 1);
	}
	else
	{
		if(m_AttachedObjectID >= 0 && m_AttachedObjectID < MAX_OBJECTS)
		{
			if(pNetGame)
			{
				CObjectPool *pObjectPool = pNetGame->GetObjectPool();
				if(pObjectPool)
				{
					CObject *pObject = pObjectPool->GetAt(m_AttachedObjectID);
					if(pObject) return (pObject->m_byteMoving & 1);
				}
			}
		}
	}

	return false;
}

void CObject::TeleportTo(float x, float y, float z)
{
    CVector pos(x, y, z);
    m_vecLastKnownPos = pos;
    m_bForceRender = ShouldForceRenderObject(pos);

    if (!EnsureEntityAvailable("teleport", true)) {
        FLog("[OBJ_FIX64] TeleportTo failed to recover entity model=%d gta=%u pos=%.1f %.1f %.1f",
             m_iModel, m_dwGTAId, pos.x, pos.y, pos.z);
        return;
    }

    if (pGame) {
        pGame->RefreshStreamingAt(x, y);
    }

    ScriptCommand(&put_object_at, m_dwGTAId, x, y, z);

    m_pEntity->SetPosn(pos);
    m_pEntity->ResetMoveSpeed();
    m_pEntity->ResetTurnSpeed();
    EnsureCollisionState(INVALID_OBJECT_ID, "teleport", true);
}

void CObject::SetPos(float x, float y, float z)
{
    TeleportTo(x, y, z);
}
