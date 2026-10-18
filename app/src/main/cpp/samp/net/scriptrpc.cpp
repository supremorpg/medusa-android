#include "../main.h"
#include "../game/game.h"
#include "../game/SampCollisionApi.h"
#include "../game/SampStreamingApi.h"
#include "../game/GTASAEngineApi.h"
#include "netgame.h"
#include "../audiostream.h"
#include <cmath>
#include <cstring>

extern CGame *pGame;
extern CNetGame *pNetGame;
extern CAudioStream* pAudioStream;
extern "C" void RequestArm64WorldCollisionForPosition(const CVector& pos, uint8_t area, bool immediate);
extern "C" bool ProbeArm64SpawnSurfaceForPosition(const CVector& pos, uint8_t area, float* outSurfaceZ, int32* outModelId);

#if !VER_x32
static uint8_t ResolveScriptedPlayerArea64(CPlayerPed* playerPed)
{
	int32 area = CGame::currArea;
	if (playerPed && playerPed->m_pPed) {
		area = playerPed->m_pPed->m_nAreaCode;
		return Xyron::Streaming::ResolveEffectiveWorldArea(playerPed->m_pPed->GetPosition(), area);
	}
	if (area < 0 || area > 255) {
		return static_cast<uint8_t>(AREA_CODE_NORMAL_WORLD);
	}
	return static_cast<uint8_t>(area);
}

static void RestoreScriptedPlayerCollision64(CPlayerPed* playerPed)
{
	if (!playerPed || !playerPed->m_pPed) {
		return;
	}

	playerPed->m_pPed->m_bUsesCollision = true;
	playerPed->m_pPed->physicalFlags.bCollidable = true;
	playerPed->m_pPed->physicalFlags.bCanBeCollidedWith = true;
	playerPed->m_pPed->physicalFlags.bDisableSimpleCollision = false;
	playerPed->m_pPed->physicalFlags.bSkipLineCol = false;
	playerPed->m_pPed->physicalFlags.bApplyGravity = true;
}

static CVector WarmupScriptedPlayerPosition64(CPlayerPed* playerPed, const CVector& pos, const char* reason)
{
	if (!pGame || !playerPed || !playerPed->m_pPed ||
	    !std::isfinite(pos.x) ||
	    !std::isfinite(pos.y) ||
	    !std::isfinite(pos.z)) {
		return pos;
	}

	const uint32_t startTick = GetTickCount();
	CVector outPos = pos;
	const uint8_t area = ResolveScriptedPlayerArea64(playerPed);
	const CVector zeroVelocity(0.0f, 0.0f, 0.0f);
	Xyron::Streaming::ForceStreamingEnabled(reason ? reason : "scripted-pos-warmup64");
	Xyron::Streaming::RequestSceneObjectsAroundPlayer(pos, zeroVelocity);

	int samples = 0;
	auto requestAt = [&](const CVector& sample, bool loadScene) {
		++samples;
		Xyron::Streaming::WorldStreamRequest request{};
		request.areaCode = area;
		request.streamingFlags = STREAMING_DEFAULT;
		request.loadScene = loadScene;
		request.refreshGame = true;
		request.reason = reason ? reason : "scripted-pos-warmup64";
		Xyron::Streaming::PrepareWorldAtPoint(sample, request);
	};

	auto requestCross = [&](float radius, bool loadScene) {
		const float diag = radius * 0.70710678f;
		requestAt(pos, loadScene);
		requestAt(CVector(pos.x + radius, pos.y, pos.z), false);
		requestAt(CVector(pos.x - radius, pos.y, pos.z), false);
		requestAt(CVector(pos.x, pos.y + radius, pos.z), false);
		requestAt(CVector(pos.x, pos.y - radius, pos.z), false);
		requestAt(CVector(pos.x + diag, pos.y + diag, pos.z), false);
		requestAt(CVector(pos.x + diag, pos.y - diag, pos.z), false);
		requestAt(CVector(pos.x - diag, pos.y + diag, pos.z), false);
		requestAt(CVector(pos.x - diag, pos.y - diag, pos.z), false);
	};

	const float beforeGround = pGame->FindGroundZForCoord(pos.x, pos.y, pos.z + 4.0f);
	const float radii[] = { 12.0f, 28.0f, 56.0f };
	for (int i = 0; i < 3; ++i) {
		requestCross(radii[i], i == 0);
		Xyron::Streaming::LoadAllRequestedModels(false);
	}
	Xyron::Collision::LoadCollision(pos, false);
	Xyron::Collision::EnsureCollisionInMemory(pos);
	RequestArm64WorldCollisionForPosition(pos, area, true);
	Xyron::Streaming::LoadAllRequestedModels(false);
	Xyron::Collision::LoadCollision(pos, false);
	Xyron::Collision::EnsureCollisionInMemory(pos);
	RequestArm64WorldCollisionForPosition(pos, area, true);

	const float afterGround = pGame->FindGroundZForCoord(pos.x, pos.y, pos.z + 4.0f);
	float spawnSurfaceZ = 0.0f;
	int32 spawnSurfaceModel = -1;
	bool nativeGroundLift = false;
	const bool spawnSurfaceOk =
		ProbeArm64SpawnSurfaceForPosition(pos, area, &spawnSurfaceZ, &spawnSurfaceModel);
	if (spawnSurfaceOk) {
		float desiredZ = spawnSurfaceZ + 0.95f;
		const float maxSpawnZ = pos.z + 3.0f;
		if (desiredZ > maxSpawnZ) {
			desiredZ = maxSpawnZ;
		}
		if (desiredZ > outPos.z) {
			outPos.z = desiredZ;
		}
	}
	const bool findZRequest = reason && std::strcmp(reason, "set-pos-findz") == 0;
	const bool nativeGroundIsHigher =
		findZRequest &&
		std::isfinite(afterGround) &&
		afterGround > pos.z + 12.0f &&
		afterGround > outPos.z + 1.0f &&
		afterGround < 3000.0f;
	const bool spawnSurfaceTooLow =
		!spawnSurfaceOk ||
		spawnSurfaceZ < afterGround - 8.0f;
	if (nativeGroundIsHigher && spawnSurfaceTooLow) {
		outPos.z = afterGround + 1.0f;
		nativeGroundLift = true;
	}
	RestoreScriptedPlayerCollision64(playerPed);

	static uint32_t sWarmupLogCount = 0;
	if (sWarmupLogCount < 80) {
		++sWarmupLogCount;
		FLog("[SET_POS_WARM64] reason=%s #%u pos=%.2f %.2f %.2f out=%.2f %.2f %.2f area=%u samples=%d ground=%.2f->%.2f dz=%.2f spawnSurface=%d,%.2f,%d nativeLift=%d elapsed=%u streamingDisabled=%d",
		     reason ? reason : "?",
		     sWarmupLogCount,
		     pos.x,
		     pos.y,
		     pos.z,
		     outPos.x,
		     outPos.y,
		     outPos.z,
		     static_cast<unsigned>(area),
		     samples,
		     beforeGround,
		     afterGround,
		     pos.z - afterGround,
		     spawnSurfaceOk ? 1 : 0,
		     spawnSurfaceOk ? spawnSurfaceZ : 0.0f,
		     spawnSurfaceOk ? spawnSurfaceModel : -1,
		     nativeGroundLift ? 1 : 0,
		     GetTickCount() - startTick,
		     Xyron::Streaming::IsStreamingDisabled() ? 1 : 0);
	}

	return outPos;
}
#endif

// 0.3.7
void ScrSetGravity(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fGravity;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(fGravity);
	
	pGame->SetGravity(fGravity);

	return;
}
// 0.3.7
void ScrSetCameraPos(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fX, fY, fZ;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(fX);
	bsData.Read(fY);
	bsData.Read(fZ);

    FLog("[CAM_SCRIPT64] set-pos %.2f %.2f %.2f", fX, fY, fZ);
    CCamera::SetPosition(fX, fY, fZ, 0.0f, 0.0f, 0.0f);

	return;
}
// 0.3.7
void ScrSetCameraLookAt(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fX, fY, fZ;
	uint8_t byteType;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(fX);
	bsData.Read(fY);
	bsData.Read(fZ);
	bsData.Read(byteType);
	if (byteType < 1 || byteType > 2) {
		byteType = 2;
	}

    FLog("[CAM_SCRIPT64] look-at %.2f %.2f %.2f type=%u", fX, fY, fZ, static_cast<unsigned>(byteType));
	CCamera::LookAtPoint(fX, fY, fZ, byteType);

	return;
}
// 0.3.7
void ScrInterpolateCamera(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	bool mode;
	CVector vecFrom;
	CVector vecTo;
	int iTime;
	uint8_t byteMode;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(mode);
	bsData.Read(vecFrom.x);
	bsData.Read(vecFrom.y);
	bsData.Read(vecFrom.z);
	bsData.Read(vecTo.x);
	bsData.Read(vecTo.y);
	bsData.Read(vecTo.z);
	bsData.Read(iTime);
	bsData.Read(byteMode);

	if (byteMode < 1 || byteMode > 2) {
		byteMode = 2;
	}

	if (iTime > 0) {
		pNetGame->GetPlayerPool()->GetLocalPlayer()->m_bSpectateProcessed = true;
		if (mode) {
            CCamera::InterpolateCameraPos(&vecFrom, &vecTo, iTime, byteMode);
		}
		else {
            CCamera::InterpolateCameraLookAt(&vecFrom, &vecTo, iTime, byteMode);
		}
	}

	return;
}
// 0.3.7
void ScrTogglePlayerSpectating(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint32_t dwToggle; 
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(dwToggle);

	FLog("TogglePlayerSpectating: %d", dwToggle);

	pNetGame->GetPlayerPool()->GetLocalPlayer()->ToggleSpectating(dwToggle);

	return;
}
// 0.3.7
void ScrSetSpawnInfo(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYER_SPAWN_INFO spawnInfo;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read((char*)&spawnInfo, sizeof(PLAYER_SPAWN_INFO));

    static uint32_t sSpawnInfoDiagCount = 0;
    if (sSpawnInfoDiagCount < 24) {
        ++sSpawnInfoDiagCount;
        FLog("[SPAWN_DIAG64] spawninfo #%u team=%u skin=%d pos=%.1f %.1f %.1f rot=%.2f",
             sSpawnInfoDiagCount,
             spawnInfo.byteTeam,
             spawnInfo.iSkin,
             spawnInfo.vecPos.x,
             spawnInfo.vecPos.y,
             spawnInfo.vecPos.z,
             spawnInfo.fRotation);
    }

	pNetGame->GetPlayerPool()->GetLocalPlayer()->SetSpawnInfo(&spawnInfo);

	return;
}
// 0.3.7
void ScrAddGangZone(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint16_t wZoneID;
	float minX, minY, maxX, maxY;
	uint32_t dwColor;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);

	CGangZonePool *pGangZonePool = pNetGame->GetGangZonePool();
	if (pGangZonePool)
	{
		bsData.Read(wZoneID);
		bsData.Read(minX);
		bsData.Read(minY);
		bsData.Read(maxX);
		bsData.Read(maxY);
		bsData.Read(dwColor);
		pGangZonePool->New(wZoneID, minX, minY, maxX, maxY, dwColor);
	}
}
// 0.3.7
void ScrGangZoneDestroy(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	CGangZonePool *pGangZonePool = pNetGame->GetGangZonePool();
	if (pGangZonePool)
	{
		uint16_t wZoneID;
		bsData.Read(wZoneID);
		pGangZonePool->Delete(wZoneID);
	}
}
// 0.3.7
void ScrGangZoneFlash(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	CGangZonePool *pGangZonePool = pNetGame->GetGangZonePool();
	if (pGangZonePool)
	{
		uint16_t wZoneID;
		uint32_t dwColor;
		bsData.Read(wZoneID);
		bsData.Read(dwColor);
		pGangZonePool->Flash(wZoneID, dwColor);
	}
}
// 0.3.7
void ScrGangZoneStopFlash(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	CGangZonePool *pGangZonePool = pNetGame->GetGangZonePool();
	if (pGangZonePool)
	{
		uint16_t wZoneID;
		bsData.Read(wZoneID);
		pGangZonePool->StopFlash(wZoneID);
	}
}

int iTotalObjects = 0;
static uint32_t gCreateObjectRpcCount = 0;
static uint32_t gDestroyObjectRpcCount = 0;
static uint32_t gRemoveBuildingRpcCount = 0;
// Black Russia / custom 64-bit clients can deliver object RPCs on remapped IDs.
// Register these aliases in addition to stock SAMP IDs so object stream still works.
static int RPC_BR_ScrSetObjectRotation = 0x144;
static int RPC_BR_ScrCreateObject = 0x14A;
static int RPC_BR_ScrDestroyObject = 0x14B;
static int RPC_BR_ScrSetObjectMaterial = 0x16C;
static int RPC_BR_ScrStopObject = 0x17A;
static int RPC_BR_ScrSetObjectPos = 0x197;
static int RPC_BR_ScrMoveObject = 0x1A7;

static bool ReadFixedString(RakNet::BitStream& bsData, char* out, size_t outSize, uint8_t byteLength, const char* fieldName, OBJECTID objectId)
{
    if (!out || outSize == 0) {
        return false;
    }

    out[0] = '\0';

    size_t bytesToRead = static_cast<size_t>(byteLength);
    size_t bytesToCopy = bytesToRead;
    if (bytesToCopy >= outSize) {
        FLog("[OBJ_FIX64] %s overflow len=%u object=%u (max=%zu), truncating",
             fieldName ? fieldName : "string", byteLength, objectId, outSize - 1);
        bytesToCopy = outSize - 1;
    }

    if (bytesToCopy > 0) {
        if (!bsData.Read(out, static_cast<int>(bytesToCopy))) {
            FLog("[OBJ_FIX64] failed to read %s (%u bytes) for object=%u",
                 fieldName ? fieldName : "string", byteLength, objectId);
            out[0] = '\0';
            return false;
        }
    }

    out[bytesToCopy] = '\0';

    if (bytesToRead > bytesToCopy) {
        bsData.IgnoreBits(static_cast<int>((bytesToRead - bytesToCopy) * 8));
    }

    return true;
}

void ScrCreateObject(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	int iModel;
	CVector vecPos;
	CVector vecRot;
	float fDrawDistance;
	uint8_t byteNoCameraCol;
	OBJECTID AttachedObjectID;
	VEHICLEID AttachedVehicleID;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	if (!bsData.Read(ObjectID) ||
		!bsData.Read(iModel) ||
		!bsData.Read(vecPos.x) ||
		!bsData.Read(vecPos.y) ||
		!bsData.Read(vecPos.z) ||
		!bsData.Read(vecRot.x) ||
		!bsData.Read(vecRot.y) ||
		!bsData.Read(vecRot.z) ||
		!bsData.Read(fDrawDistance) ||
		!bsData.Read(byteNoCameraCol) ||
		!bsData.Read(AttachedVehicleID) ||
		!bsData.Read(AttachedObjectID)) {
		FLog("[OBJ_FIX64] ScrCreateObject malformed header bits=%d", iBitLength);
		return;
	}

	CVector vecAttachOffset;
	CVector vecAttachRot;
	uint8_t bSyncRotation;

	if (AttachedObjectID != INVALID_OBJECT_ID || AttachedVehicleID != INVALID_VEHICLE_ID)
	{
		if (!bsData.Read(vecAttachOffset.x) ||
			!bsData.Read(vecAttachOffset.y) ||
			!bsData.Read(vecAttachOffset.z) ||
			!bsData.Read(vecAttachRot.x) ||
			!bsData.Read(vecAttachRot.y) ||
			!bsData.Read(vecAttachRot.z) ||
			!bsData.Read(bSyncRotation)) {
			FLog("[OBJ_FIX64] ScrCreateObject malformed attach id=%u model=%d bits=%d",
				 ObjectID, iModel, iBitLength);
			return;
		}
	}

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
    if (!pObjectPool) {
        FLog("[OBJ_FIX64] ScrCreateObject object pool unavailable");
        return;
    }
	const bool wasObjectSlotActive = pObjectPool->GetAt(ObjectID) != nullptr;
	const bool createdObject = pObjectPool->New(ObjectID, iModel, vecPos, vecRot, fDrawDistance);
	if (!createdObject) {
        FLog("[OBJ_FIX64] ScrCreateObject failed id=%u model=%d", ObjectID, iModel);
    }

	CObject* pObject = pObjectPool->GetAt(ObjectID);
    if (!pObject) {
        FLog("[OBJ_FIX64] ScrCreateObject target object missing id=%u", ObjectID);
    }
	if (AttachedObjectID != INVALID_OBJECT_ID)
	{
		if (pObject) {
			pObject->SetAttachedObject(AttachedObjectID, &vecAttachOffset, &vecAttachRot, bSyncRotation);
		}
	}
	else if (AttachedVehicleID != INVALID_VEHICLE_ID)
	{
		if (pObject) {
			pObject->SetAttachedVehicle(AttachedVehicleID, &vecAttachOffset, &vecAttachRot);
		}
	}

	uint8_t byteMaterialsCount;
	if (!bsData.Read(byteMaterialsCount)) {
		FLog("[OBJ_FIX64] ScrCreateObject missing material count id=%u model=%d bits=%d",
			 ObjectID, iModel, iBitLength);
		byteMaterialsCount = 0;
	}
    FLog("[OBJ_DIAG64] create id=%u model=%d pos=%.1f %.1f %.1f rot=%.1f %.1f %.1f draw=%.1f noCamCol=%u mats=%u attachObj=%u attachVeh=%u",
         ObjectID,
         iModel,
         vecPos.x,
         vecPos.y,
         vecPos.z,
         vecRot.x,
         vecRot.y,
         vecRot.z,
         fDrawDistance,
         byteNoCameraCol,
         byteMaterialsCount,
         AttachedObjectID,
         AttachedVehicleID);
    for (uint8_t materialEntry = 0; materialEntry < byteMaterialsCount; ++materialEntry)
    {
		char txdname[256];
		char texturename[256];
        char szFontName[32];
        char szText[2048];
		uint8_t byteType = 0;
		uint8_t byteMaterialIndex = 0;
		uint16_t MaterialModel = 0;
		uint8_t byteLength = 0;
		uint32_t dwColor = 0;

		// Material Text
		uint8_t byteMaterialSize = 0;
		uint8_t byteFontNameLength = 0;
		uint8_t byteFontSize = 0;
		uint8_t byteFontBold = 0;
		uint32_t dwFontColor = 0;
		uint32_t dwBackgroundColor = 0;
		uint8_t byteAlign = 0;

        if (!bsData.Read(byteType)) {
            FLog("[OBJ_FIX64] material entry read failed object=%u entry=%u/%u", ObjectID, materialEntry + 1, byteMaterialsCount);
            break;
        }

		if (byteType == 1) // material
		{
			if (!bsData.Read(byteMaterialIndex) ||
				!bsData.Read(MaterialModel) ||
				!bsData.Read(byteLength)) {
				FLog("[OBJ_FIX64] material header read failed object=%u entry=%u/%u",
					 ObjectID, materialEntry + 1, byteMaterialsCount);
				break;
			}
            if (!ReadFixedString(bsData, txdname, sizeof(txdname), byteLength, "txdname", ObjectID)) {
                break;
            }
			if (!bsData.Read(byteLength)) {
				FLog("[OBJ_FIX64] material texture length read failed object=%u entry=%u/%u",
					 ObjectID, materialEntry + 1, byteMaterialsCount);
				break;
			}
            if (!ReadFixedString(bsData, texturename, sizeof(texturename), byteLength, "texturename", ObjectID)) {
                break;
            }
			if (!bsData.Read(dwColor)) {
				FLog("[OBJ_FIX64] material color read failed object=%u entry=%u/%u",
					 ObjectID, materialEntry + 1, byteMaterialsCount);
				break;
			}

			if (strlen(txdname) < 32 && strlen(texturename) < 32)
			{
				if (MaterialModel == 0xFFFF || MaterialModel > 20000)
					MaterialModel = 0xFFFF;

				CObject* pMaterialObject = pObjectPool->GetAt(ObjectID);
				if (pMaterialObject) {
                    pMaterialObject->SetMaterial(MaterialModel, byteMaterialIndex, txdname, texturename, dwColor);
                } else {
                    FLog("[OBJ_FIX64] missing object for SetMaterial id=%u entry=%u", ObjectID, materialEntry);
                }
			}
		}
		else if (byteType == 2) // material text
		{
			if (!bsData.Read(byteMaterialIndex) ||
				!bsData.Read(byteMaterialSize) ||
				!bsData.Read(byteFontNameLength)) {
				FLog("[OBJ_FIX64] material text header read failed object=%u entry=%u/%u",
					 ObjectID, materialEntry + 1, byteMaterialsCount);
				break;
			}
            if (!ReadFixedString(bsData, szFontName, sizeof(szFontName), byteFontNameLength, "fontname", ObjectID)) {
                break;
            }
			if (!bsData.Read(byteFontSize) ||
				!bsData.Read(byteFontBold) ||
				!bsData.Read(dwFontColor) ||
				!bsData.Read(dwBackgroundColor) ||
				!bsData.Read(byteAlign)) {
				FLog("[OBJ_FIX64] material text style read failed object=%u entry=%u/%u",
					 ObjectID, materialEntry + 1, byteMaterialsCount);
				break;
			}
			stringCompressor->DecodeString(szText, 2048, &bsData);
			szText[sizeof(szText) - 1] = '\0';

			if(strlen(szFontName) <= 32)
			{
				if(pObject)
				{
					pObject->SetMaterialText(byteMaterialIndex, szText, byteMaterialSize, szFontName, byteFontSize, byteFontBold, dwFontColor, dwBackgroundColor, byteAlign);
				}
                else {
                    FLog("[OBJ_FIX64] missing object for SetMaterialText id=%u entry=%u", ObjectID, materialEntry);
                }
			}
		}
        else
        {
            FLog("[OBJ_FIX64] unknown material type=%u for object=%u entry=%u/%u",
                 byteType, ObjectID, materialEntry + 1, byteMaterialsCount);
            break;
        }
	}


	if (createdObject && !wasObjectSlotActive) {
		iTotalObjects++;
	}
    ++gCreateObjectRpcCount;
    if (gCreateObjectRpcCount <= 30 || (gCreateObjectRpcCount % 100) == 0) {
        FLog("[OBJ_DIAG64] create-count rpc=%u live=%d lastId=%u model=%d",
             gCreateObjectRpcCount, iTotalObjects, ObjectID, iModel);
    }
	//LOGI("CreateObject: model %d; Total objects: %d", iModel, iTotalObjects);
	//MyLog2("CreateObject: model %d; Total objects: %d", iModel, iTotalObjects);
	//MyLog2("CreateObject: id: %d model: %d x: %f y: %f z: %f", iTotalObjects, iModel, vecPos.x, vecPos.y, vecPos.z);
}

void ScrDestroyObject(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	if (!bsData.Read(ObjectID)) {
		FLog("[OBJ_FIX64] ScrDestroyObject malformed bits=%d", iBitLength);
		return;
	}

	CObjectPool *pObjectPool = pNetGame->GetObjectPool();
	const bool hadObject = pObjectPool && pObjectPool->GetAt(ObjectID) != nullptr;
	if (pObjectPool) {
		pObjectPool->Delete(ObjectID);
	}
	if (hadObject && iTotalObjects > 0) {
		iTotalObjects--;
	}
    ++gDestroyObjectRpcCount;
    if (gDestroyObjectRpcCount <= 30 || (gDestroyObjectRpcCount % 100) == 0) {
        FLog("[OBJ_DIAG64] destroy-count rpc=%u live=%d id=%u had=%d",
             gDestroyObjectRpcCount, iTotalObjects, ObjectID, hadObject ? 1 : 0);
    }
	//LOGI("DestroyObject; Total objects: %d", iTotalObjects);
}

void ScrSetObjectMaterial(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
    if (!pObjectPool) {
        FLog("[OBJ_FIX64] ScrSetObjectMaterial object pool unavailable");
        return;
    }
	OBJECTID ObjectID;
	uint8_t byteMaterialType;
	uint8_t byteMaterialIndex;
	uint16_t wModelID;
	uint8_t byteLength;
	char txdname[256], texname[256], fontname[256];
	uint32_t dwColor;
	uint8_t byteMaterialSize;
	uint8_t byteFontSize;
	uint8_t byteBold;
	uint32_t dwFontColor;
	uint32_t dwBackColor;
	uint8_t byteTextAlignment;
	char text[2048];

	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	if (!bsData.Read(ObjectID)) {
		FLog("[OBJ_FIX64] ScrSetObjectMaterial malformed object id bits=%d", iBitLength);
		return;
	}

	CObject* pObject = pObjectPool->GetAt(ObjectID);
    if (!pObject) {
        FLog("[OBJ_FIX64] ScrSetObjectMaterial target object missing id=%u", ObjectID);
    }

	if (!bsData.Read(byteMaterialType)) {
		FLog("[OBJ_FIX64] ScrSetObjectMaterial missing type id=%u bits=%d", ObjectID, iBitLength);
		return;
	}
	if (byteMaterialType == 1)
	{
		if (!bsData.Read(byteMaterialIndex) ||
			!bsData.Read(wModelID) ||
			!bsData.Read(byteLength)) {
			FLog("[OBJ_FIX64] ScrSetObjectMaterial material header failed id=%u bits=%d",
				 ObjectID, iBitLength);
			return;
		}
        if (!ReadFixedString(bsData, txdname, sizeof(txdname), byteLength, "txdname", ObjectID)) {
            return;
        }
		if (!bsData.Read(byteLength)) {
			FLog("[OBJ_FIX64] ScrSetObjectMaterial texture length failed id=%u bits=%d",
				 ObjectID, iBitLength);
			return;
		}
        if (!ReadFixedString(bsData, texname, sizeof(texname), byteLength, "texturename", ObjectID)) {
            return;
        }
		if (!bsData.Read(dwColor)) {
			FLog("[OBJ_FIX64] ScrSetObjectMaterial color failed id=%u bits=%d",
				 ObjectID, iBitLength);
			return;
		}
		if (strlen(txdname) < 32 && strlen(texname) < 32)
		{
			if (pObject)
				pObject->SetMaterial(wModelID, byteMaterialIndex, txdname, texname, dwColor);
		}
	}
	else if (byteMaterialType == 2)
	{
		if (!bsData.Read(byteMaterialIndex) ||
			!bsData.Read(byteMaterialSize) ||
			!bsData.Read(byteLength)) {
			FLog("[OBJ_FIX64] ScrSetObjectMaterial text header failed id=%u bits=%d",
				 ObjectID, iBitLength);
			return;
		}
        if (!ReadFixedString(bsData, fontname, sizeof(fontname), byteLength, "fontname", ObjectID)) {
            return;
        }
		if (!bsData.Read(byteFontSize) ||
			!bsData.Read(byteBold) ||
			!bsData.Read(dwFontColor) ||
			!bsData.Read(dwBackColor) ||
			!bsData.Read(byteTextAlignment)) {
			FLog("[OBJ_FIX64] ScrSetObjectMaterial text style failed id=%u bits=%d",
				 ObjectID, iBitLength);
			return;
		}

		stringCompressor->DecodeString(text, 2048, &bsData);
		text[sizeof(text) - 1] = '\0';


		if (strlen(fontname) > 0 && strlen(fontname) < 32)
		{
			if (pObject) {
				pObject->SetMaterialText(
					byteMaterialIndex,
					text,
					byteMaterialSize,
					fontname,
					byteFontSize,
					byteBold,
					dwFontColor,
					dwBackColor,
					byteTextAlignment
				);
			}
		}
	}
    else
    {
        FLog("[OBJ_FIX64] ScrSetObjectMaterial unknown material type=%u id=%u", byteMaterialType, ObjectID);
    }
}

// 0.3.7
void ScrRemoveBuilding(RPCParameters *rpcParams)
{
	unsigned char * Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	int iModel;
	float fX, fY, fZ;
	float fRadius;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(iModel);
	bsData.Read(fX);
	bsData.Read(fY);
	bsData.Read(fZ);
	bsData.Read(fRadius);
    ++gRemoveBuildingRpcCount;
    if (gRemoveBuildingRpcCount <= 40 || (gRemoveBuildingRpcCount % 200) == 0 || iModel == -1 || fRadius >= 150.0f) {
        FLog("[MAP_DIAG64] remove-building-rpc #%u model=%d pos=%.1f %.1f %.1f r=%.1f",
             gRemoveBuildingRpcCount, iModel, fX, fY, fZ, fRadius);
    }
	RemoveBuilding(iModel, CVector(fX, fY, fZ), fRadius);
}
// 0.3.7
void ScrSetPlayerSkin(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	int iPlayerID;
	int iModel;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(iPlayerID);
	bsData.Read(iModel);

	if (IsValidPedModel(iModel))
	{
		if (pPlayerPool->GetLocalPlayerID() == iPlayerID)
		{
			pPlayerPool->GetLocalPlayer()->GetPlayerPed()->SetModelIndex(iModel);
		}
		else
		{
			if (pPlayerPool->GetSlotState(iPlayerID) == true)
			{
				CRemotePlayer* pPlayer = pPlayerPool->GetAt(iPlayerID);
				if (!pPlayer || pPlayer->GetState() == PLAYER_STATE_NONE) return;
				CPlayerPed* pPlayerPed = pPlayer->GetPlayerPed();
				if (!pPlayerPed) return;
				pPlayerPed->SetModelIndex(iModel);
			}
		}
	}
	else
	{
		//if (gui) gui->chat()->addDebugMessage("Warning: SetPlayerSkin %d isn't a valid ped model.", iModel);
	}
}
// 0.3.7
void ScrSetPlayerMapIcon(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteIconID;
	CVector vecPos;
	uint8_t byteType;
	uint32_t dwColor;
	uint8_t byteStyle;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(byteIconID);
	bsData.Read(vecPos.x);
	bsData.Read(vecPos.y);
	bsData.Read(vecPos.z);
	bsData.Read(byteType);
	bsData.Read(dwColor);
	bsData.Read(byteStyle);

	// fix crash (invalid GTASA map icons)
	if (byteIconID == 1 or
		byteIconID == 2 or
		byteIconID == 4 or
		byteIconID == 56) byteIconID = 52;

	pNetGame->SetMapIcon(byteIconID, vecPos.x, vecPos.y, vecPos.z, byteType, dwColor, byteStyle);
}
// 0.3.7
void ScrRemovePlayerMapIcon(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteIconID;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(byteIconID);

	pNetGame->DisableMapIcon(byteIconID);
}
// 0.3.7
void ScrShowNameTag(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint8_t byteShowNameTag;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(PlayerID);
	bsData.Read(byteShowNameTag);

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (pPlayerPool->GetSlotState(PlayerID))
	{
		CRemotePlayer* pPlayer = pPlayerPool->GetAt(PlayerID);
		if (pPlayer) {
			pPlayer->m_bShowNameTag = byteShowNameTag;
		}
	}
}
// 0.3.7
void ScrApplyPlayerAnimation(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	char szAnimLib[256];
	char szAnimName[256];
	memset(szAnimLib, 0, sizeof(szAnimLib));
	memset(szAnimName, 0, sizeof(szAnimName));

	PLAYERID PlayerID;
	uint8_t byteLength;
	float fDelta;
	bool bLoop, bLockX, bLockY, bFreeze;
	uint32_t dwTime;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(PlayerID);
	
	bsData.Read(byteLength);
	bsData.Read(szAnimLib, byteLength);
	szAnimLib[byteLength] = '\0';

	bsData.Read(byteLength);
	bsData.Read(szAnimName, byteLength);
	szAnimName[byteLength] = '\0';

	bsData.Read(fDelta);
	bsData.Read(bLoop);
	bsData.Read(bLockX);
	bsData.Read(bLockY);
	bsData.Read(bFreeze);
	bsData.Read(dwTime);

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	CPlayerPed* pPlayerPed = nullptr;

	if (pPlayerPool)
	{
		if (PlayerID == pPlayerPool->GetLocalPlayerID()) {
			pPlayerPed = pPlayerPool->GetLocalPlayer()->GetPlayerPed();
		}
		else {
			if (pPlayerPool->GetSlotState(PlayerID)) {
				pPlayerPed = pPlayerPool->GetAt(PlayerID)->GetPlayerPed();
				pPlayerPool->GetAt(PlayerID)->m_bAppliedAnimation = true;
			}
		}

		if (pPlayerPed) {
			FLog("ApplyAnimation: %s:%s", szAnimLib, szAnimName);
			pPlayerPed->ApplyAnimation(szAnimName, szAnimLib, fDelta, bLoop, bLockX, bLockY, bFreeze, dwTime);
		}
	}
}
// 0.3.7
void ScrClearPlayerAnimations(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

	FLog("ClearAnimation");

	PLAYERID playerId;
	bsData.Read(playerId);
	RwMatrix mat;

	CPlayerPool * pPlayerPool = NULL;
	CPlayerPed * pPlayerPed = NULL;

	pPlayerPool = pNetGame->GetPlayerPool();

	if (pPlayerPool)
	{
		if (playerId == pPlayerPool->GetLocalPlayerID()) {
			pPlayerPed = pPlayerPool->GetLocalPlayer()->GetPlayerPed();
		}
		else {
			if (pPlayerPool->GetSlotState(playerId)) {
				pPlayerPed = pPlayerPool->GetAt(playerId)->GetPlayerPed();
				pPlayerPool->GetAt(playerId)->m_bAppliedAnimation = false;
			}
		}
		if (pPlayerPed) {
            mat = pPlayerPed->m_pPed->GetMatrix().ToRwMatrix();
			pPlayerPed->TeleportTo(mat.pos.x, mat.pos.y, mat.pos.z);
		}
	}
}
// 0.3.7
void ScrSetPlayerHealth(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fHealth;
	RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);
	bsData.Read(fHealth);

	pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->SetHealth(fHealth);
}
// 0.3.7
void ScrGivePlayerWeapon(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	int iWeapon;
	int iAmmo;
	RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);
	bsData.Read(iWeapon);
	bsData.Read(iAmmo);
	pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->GiveWeapon(iWeapon, iAmmo);
}
// 0.3.7
void ScrSetPlayerInterior(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteInteriorId;
	RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);
	bsData.Read(byteInteriorId);

    static uint32_t sInteriorDiagCount = 0;
    if (sInteriorDiagCount < 32) {
        ++sInteriorDiagCount;
        FLog("[SPAWN_DIAG64] set-interior #%u id=%u", sInteriorDiagCount, byteInteriorId);
    }

	pGame->FindPlayerPed()->SetInterior(byteInteriorId, true);
}
// 0.3.7
extern UI *pUI;
void ScrShowTextDraw(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

    FLog("ScrShowTextDraw");

	CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();
	if (pTextDrawPool == nullptr) {
        FLog("no textdraw pool");
        return;
    }

	uint16_t wTextDrawID;
	TEXT_DRAW_TRANSMIT textDrawTransmit;
	uint16_t wTextLength;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(wTextDrawID);
	bsData.Read((char*)& textDrawTransmit, sizeof(TEXT_DRAW_TRANSMIT));
	bsData.Read(wTextLength);

	if (wTextDrawID >= MAX_TEXT_DRAWS) {
        FLog("[TEXTDRAW_GUARD64] reject-show invalid id=%u max=%u", wTextDrawID, MAX_TEXT_DRAWS);
		return;
	}
	if (wTextLength > MAX_TEXT_DRAW_LINE) {
        FLog("[TEXTDRAW_GUARD64] reject-show text too long id=%u len=%u max=%u",
             wTextDrawID,
             wTextLength,
             MAX_TEXT_DRAW_LINE);
		return;
	}

	char szText[1024 + 1];

	bsData.Read(szText, wTextLength);
    szText[wTextLength] = 0;

    pTextDrawPool->New(wTextDrawID, &textDrawTransmit, szText);
}
// 0.3.7
void ScrHideTextDraw(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();
	if (pTextDrawPool == nullptr) return;

	uint16_t wTextDrawID;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(wTextDrawID);

	if (wTextDrawID >= MAX_TEXT_DRAWS) {
        FLog("[TEXTDRAW_GUARD64] reject-hide invalid id=%u max=%u", wTextDrawID, MAX_TEXT_DRAWS);
		return;
	}

	pTextDrawPool->Delete(wTextDrawID);
}
// 0.3.7
void ScrTextDrawSetString(RPCParameters * rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();
	if (pTextDrawPool == nullptr) return;

	uint16_t wTextDrawID;
	uint16_t wTextLength;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(wTextDrawID);
	bsData.Read(wTextLength);

	if (wTextDrawID >= MAX_TEXT_DRAWS) {
        FLog("[TEXTDRAW_GUARD64] reject-setstring invalid id=%u max=%u", wTextDrawID, MAX_TEXT_DRAWS);
		return;
	}
	if (wTextLength > MAX_TEXT_DRAW_LINE) {
        FLog("[TEXTDRAW_GUARD64] reject-setstring text too long id=%u len=%u max=%u",
             wTextDrawID,
             wTextLength,
             MAX_TEXT_DRAW_LINE);
		return;
	}

	if (wTextLength < 1024)
	{
		char szText[1024 + 1];
		bsData.Read(szText, wTextLength);
		szText[wTextLength] = '\0';

		CTextDraw* pTextDraw = pTextDrawPool->GetAt(wTextDrawID);
		if (pTextDraw) pTextDraw->SetText(szText);
	}

}

void ScrSelectTextDraw(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	bool bEnable = false;
	uint32_t dwColor = 0;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(bEnable);
	bsData.Read(dwColor);

	pNetGame->GetTextDrawPool()->SetSelectState(bEnable ? true : false, dwColor);
}

// 0.3.7
void ScrSetPlayerAmmo(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	
	CLocalPlayer* pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();

	uint8_t byteWeapon;
	uint16_t wAmmo;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(byteWeapon);
	bsData.Read(wAmmo);

	pLocalPlayer->GetPlayerPed()->SetAmmo(byteWeapon, wAmmo);
}
// 0.3.7
void ScrSetVehicleHealth(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	float fHealth;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(VehicleID);
	bsData.Read(fHealth);

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (pVehiclePool)
	{
		CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
		if (pVehicle) {
			pVehicle->SetHealth(fHealth);
		}
	}
}
// 0.3.7
void ScrAttachTrailerToVehicle(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID TrailerID;
	VEHICLEID VehicleID;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(TrailerID);
	bsData.Read(VehicleID);

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CVehicle* pTrailer = pVehiclePool->GetAt(TrailerID);
	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);

	if (pTrailer && pVehicle)
	{
		pVehicle->SetTrailer(pTrailer);
		pVehicle->AttachTrailer();
	}
}
// 0.3.7
void ScrDetachTrailerFromVehicle(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	VEHICLEID VehicleID;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(VehicleID);

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (pVehicle)
	{
		pVehicle->DetachTrailer();
		pVehicle->SetTrailer(nullptr);
	}
}
// 0.3.7
void ScrSetObjectPos(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	float fX;
	float fY;
	float fZ;
	float fUnused;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	if (!bsData.Read(ObjectID) ||
		!bsData.Read(fX) ||
		!bsData.Read(fY) ||
		!bsData.Read(fZ) ||
		!bsData.Read(fUnused)) {
		FLog("[OBJ_FIX64] ScrSetObjectPos malformed bits=%d", iBitLength);
		return;
	}

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;

	CObject* pObject = pObjectPool->GetAt(ObjectID);
	if (pObject) {
		pObject->SetPos(fX, fY, fZ);
	}
}
// 0.3.7
void ScrSetObjectRotation(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	CVector vecRot;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	if (!bsData.Read(ObjectID) ||
		!bsData.Read(vecRot.x) ||
		!bsData.Read(vecRot.y) ||
		!bsData.Read(vecRot.z)) {
		FLog("[OBJ_FIX64] ScrSetObjectRotation malformed bits=%d", iBitLength);
		return;
	}

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;

	CObject* pObject = pObjectPool->GetAt(ObjectID);
	if (!pObject) return;

	pObject->InstantRotate(vecRot.x, vecRot.y, vecRot.z);
}
// 0.3.7
void ScrCreateExplosion(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fX, fY, fZ;
	uint32_t dwType;
	float fRadius;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(fX);
	bsData.Read(fY);
	bsData.Read(fZ);
	bsData.Read(dwType);
	bsData.Read(fRadius);

	ScriptCommand(&create_explosion_with_radius, fX, fY, fZ, dwType, fRadius);
}
// 0.3.7
void ScrSetVehicleNumberPlate(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteLength;
	char szPlateName[32+1];
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(VehicleID);
	bsData.Read(byteLength);
	if (byteLength > 32) return;

	bsData.Read(szPlateName, byteLength);
	szPlateName[byteLength] = '\0';

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	pVehicle->SetPlateText(szPlateName);
}

#define SPECTATE_TYPE_NORMAL	1
#define SPECTATE_TYPE_FIXED		2
#define SPECTATE_TYPE_SIDE		3

// 0.3.7
void ScrSpectatePlayer(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint8_t byteMode;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(PlayerID);
	bsData.Read(byteMode);

	switch (byteMode) 
	{
	case SPECTATE_TYPE_FIXED:
		byteMode = 15;
		break;
	case SPECTATE_TYPE_SIDE:
		byteMode = 14;
		break;
	default:
		byteMode = 4;
	}

	CLocalPlayer* pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();
	pLocalPlayer->m_byteSpectateMode = byteMode;
	pLocalPlayer->SpectatePlayer(PlayerID);
}
// 0.3.7
void ScrSpectateVehicle(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteMode;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(VehicleID);
	bsData.Read(byteMode);

	switch (byteMode)
	{
	case SPECTATE_TYPE_FIXED:
		byteMode = 15;
		break;
	case SPECTATE_TYPE_SIDE:
		byteMode = 14;
		break;
	default:
		byteMode = 3;
	}

	CLocalPlayer* pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();
	pLocalPlayer->m_byteSpectateMode = byteMode;
	pLocalPlayer->SpectateVehicle(VehicleID);
}
// 0.3.7
void ScrRemoveVehicleComponent(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	VEHICLEID VehicleID;
	uint16_t wComponent;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(VehicleID);
	bsData.Read(wComponent);

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	pVehicle->RemoveComponent(wComponent);
}
// 0.3.7
void ScrAttachObjectToPlayer(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	PLAYERID PlayerID;
	float offsetX, offsetY, offsetZ;
	float rX, rY, rZ;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	if (!bsData.Read(ObjectID) ||
		!bsData.Read(PlayerID) ||
		!bsData.Read(offsetX) ||
		!bsData.Read(offsetY) ||
		!bsData.Read(offsetZ) ||
		!bsData.Read(rX) ||
		!bsData.Read(rY) ||
		!bsData.Read(rZ)) {
		FLog("[OBJ_FIX64] ScrAttachObjectToPlayer malformed bits=%d", iBitLength);
		return;
	}

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) return;
	CObject* pObject = pObjectPool->GetAt(ObjectID);
	if (!pObject) return;

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	if (pPlayerPool->GetLocalPlayerID() == PlayerID)
	{
		CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
		ScriptCommand(&attach_object_to_actor,
			pObject->m_dwGTAId,
			pLocalPlayer->GetPlayerPed()->m_dwGTAId,
			offsetX, offsetY, offsetZ,
			rX, rY, rZ);
	}
	else
	{
		CRemotePlayer* pRemotePlayer = pPlayerPool->GetAt(PlayerID);
		ScriptCommand(&attach_object_to_actor,
			pObject->m_dwGTAId,
			pRemotePlayer->GetPlayerPed()->m_dwGTAId,
			offsetX, offsetY, offsetZ,
			rX, rY, rZ);
	}
}
// 0.3.7
void ScrSetPlayerWantedLevel(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteWantedLevel;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(byteWantedLevel);

	if (pGame) pGame->SetWantedLevel(byteWantedLevel);
}
// 0.3.7
void ScrSetPlayerSpecialAction(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteSpecialAction;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(byteSpecialAction);

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	pPlayerPool->GetLocalPlayer()->ApplySpecialAction(byteSpecialAction);
}
// 0.3.7
void ScrEnableStuntBonus(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	bool bEnable;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(bEnable);

	pGame->EnableStuntBonus(bEnable);
}
// 0.3.7
void ScrSetPlayerFightingStyle(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint8_t byteStyle;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(PlayerID);
	bsData.Read(byteStyle);

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	CPlayerPed* pPlayerPed = nullptr;

	if (PlayerID == pPlayerPool->GetLocalPlayerID()) {
		pPlayerPed = pPlayerPool->GetLocalPlayer()->GetPlayerPed();
	}
	else {
		CRemotePlayer *pRemotePlayer = pPlayerPool->GetAt(PlayerID);
		if (pRemotePlayer) {
			pPlayerPed = pRemotePlayer->GetPlayerPed();
		}
	}

	if (pPlayerPed) {
		pPlayerPed->SetFightingStyle(byteStyle);
	}
}

void ScrSetPlayerVelocity(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVector vecVelocity;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(vecVelocity.x);
	bsData.Read(vecVelocity.y);
	bsData.Read(vecVelocity.z);

	CPlayerPed* pPlayerPed = pGame->FindPlayerPed();
	if (!pPlayerPed) return;

	if (pPlayerPed->IsOnGround()) {
		//uint32_t dwState = pPlayerPed->GetStateFlags();
		//pPlayerPed->SetStateFlags(dwState ^ 3);
	}

	pPlayerPed->m_pPed->SetVelocity(vecVelocity);
}
// 0.3.7
void ScrSetVehicleVelocity(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteType;
	CVector vecVelocity;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(byteType);
	bsData.Read(vecVelocity.x);
	bsData.Read(vecVelocity.y);
	bsData.Read(vecVelocity.z);

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CPlayerPed* pPlayerPed = pGame->FindPlayerPed();
	if (!pPlayerPed) return;

	if (pPlayerPed->IsInVehicle())
	{
        CVehicleGTA* pGtaVehicle = pPlayerPed->GetGtaVehicle();
		VEHICLEID VehicleID = pVehiclePool->FindIDFromGtaPtr(pGtaVehicle);
		CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
		if (!pVehicle) return;

		if (byteType)
		{
			if (byteType == 1)
			{
				pVehicle->m_pVehicle->SetTurnSpeed(vecVelocity);
			}
		}
		else
		{
			pVehicle->m_pVehicle->SetVelocity(vecVelocity);
		}
	}

}
// 0.3.7
void ScrToggleWidescreen(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteToggle;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(byteToggle);
	//if (gui) gui->chat()->addDebugMessage("Widescreen = %d", byteToggle);
	ScriptCommand(&toggle_widescreen, byteToggle);
}
// 0.3.7
void ScrSetVehicleTireDamageStatus(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteTireDamageStatus;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(VehicleID);
	bsData.Read(byteTireDamageStatus);

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	pVehicle->SetTireDamageStatus(byteTireDamageStatus);
}
// 0.3.7
void ScrSetPlayerTeam(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint8_t byteTeam;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(PlayerID);
	bsData.Read(byteTeam);

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
	if (!pLocalPlayer) return;

	if (PlayerID == pPlayerPool->GetLocalPlayerID())
	{
		pLocalPlayer->SetTeam(byteTeam);
	}
	else
	{
		CRemotePlayer* pRemotePlayer = pPlayerPool->GetAt(PlayerID);
		if (pRemotePlayer) {
			pRemotePlayer->SetTeam(byteTeam);
		}
	}
}
// 0.3.7
void ScrSetPlayerName(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint8_t byteLen;
	char szName[28];
	uint8_t byteSuccess;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(PlayerID);
	bsData.Read(byteLen);
	if (byteLen > MAX_PLAYER_NAME) return;

	bsData.Read(szName, byteLen);
	szName[byteLen] = '\0';
	bsData.Read(byteSuccess);

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	if (byteSuccess == 1) {
		pPlayerPool->SetPlayerName(PlayerID, szName);
	}

	if (pPlayerPool->GetLocalPlayerID() == PlayerID) {
		pPlayerPool->SetLocalPlayerName(szName);
	}
}
// 0.3.7
void ScrSetPlayerPos(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVector vecPos;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(vecPos.x);
	bsData.Read(vecPos.y);
	bsData.Read(vecPos.z);

    static uint32_t sSetPosDiagCount = 0;
    if (sSetPosDiagCount < 60) {
        ++sSetPosDiagCount;
        const float ground = pGame ? pGame->FindGroundZForCoord(vecPos.x, vecPos.y, vecPos.z + 4.0f) : 0.0f;
        FLog("[SPAWN_DIAG64] set-pos #%u pos=%.2f %.2f %.2f ground=%.2f dz=%.2f",
             sSetPosDiagCount,
             vecPos.x,
             vecPos.y,
             vecPos.z,
             ground,
             vecPos.z - ground);
    }

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;
	CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
	if (!pLocalPlayer) return;
    CPlayerPed* localPed = pLocalPlayer->GetPlayerPed();
    if (!localPed || !localPed->m_pPed) return;

    pLocalPlayer->DisableSurf();

#if !VER_x32
    vecPos = WarmupScriptedPlayerPosition64(localPed, vecPos, "set-pos");
#else
    // Prime stream/collision around server authoritative position instead
    // of using post-spawn teleport corrections.
    Xyron::Streaming::WorldStreamRequest request{};
    request.areaCode = localPed->m_pPed->m_nAreaCode;
    request.addModels = false;
    request.addLods = false;
    request.refreshGame = true;
    request.reason = "set-pos-32";
    Xyron::Streaming::PrepareWorldAtPoint(vecPos, request);
    Xyron::Streaming::LoadAllRequestedModels(false);
#endif

    if(localPed->m_pPed->IsInVehicle())
        localPed->RemoveFromVehicleAndPutAt(vecPos.x, vecPos.y, vecPos.z);
    else
        localPed->TeleportTo(vecPos.x, vecPos.y, vecPos.z);
#if !VER_x32
    RestoreScriptedPlayerCollision64(localPed);
#endif
}
// 0.3.7
void ScrSetPlayerPosFindZ(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CVector vecPos;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(vecPos.x);
	bsData.Read(vecPos.y);
	bsData.Read(vecPos.z);

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
	if (!pLocalPlayer) return;

    const float ground = pGame->FindGroundZForCoord(vecPos.x, vecPos.y, vecPos.z);
    constexpr float kArm64PedGroundOffset = 1.0f;
	vecPos.z = ground + kArm64PedGroundOffset;
    static uint32_t sSetPosFindZDiagCount = 0;
    if (sSetPosFindZDiagCount < 60) {
        ++sSetPosFindZDiagCount;
        FLog("[SPAWN_DIAG64] set-pos-findz #%u pos=%.2f %.2f %.2f ground=%.2f outZ=%.2f",
             sSetPosFindZDiagCount,
             vecPos.x,
             vecPos.y,
             vecPos.z - kArm64PedGroundOffset,
             ground,
             vecPos.z);
    }
    pLocalPlayer->DisableSurf();
    CPlayerPed* localPed = pLocalPlayer->GetPlayerPed();
    if (!localPed || !localPed->m_pPed) return;
#if !VER_x32
    vecPos = WarmupScriptedPlayerPosition64(localPed, vecPos, "set-pos-findz");
#endif
	localPed->TeleportTo(vecPos.x, vecPos.y, vecPos.z);
#if !VER_x32
    RestoreScriptedPlayerCollision64(localPed);
#endif
}
// 0.3.7
void ScrPutPlayerInVehicle(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteSeatID;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(VehicleID);
	bsData.Read(byteSeatID);

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

    CPlayerPed *pPed = pGame->FindPlayerPed();
    if(!pPed)return;

    //if(vehicleid == pPed->GetCurrentSampVehicleID()) return;

    if(pPed->m_pPed->IsInVehicle()) {
        pPed->m_pPed->RemoveFromVehicle();
    }
    CVehicle *pVehicle = pVehiclePool->GetAt(VehicleID);
    if(!pVehicle)return;
    if (pVehicle->m_pVehicle) {
        pVehicle->m_pVehicle->ResetMoveSpeed();
        pVehicle->m_pVehicle->ResetTurnSpeed();
        pVehicle->m_pVehicle->m_bUsesCollision = true;
        pVehicle->m_pVehicle->m_bIsVisible = true;
        pVehicle->m_pVehicle->m_bRemoveFromWorld = false;
        pVehicle->m_pVehicle->physicalFlags.bCollidable = true;
        pVehicle->m_pVehicle->physicalFlags.bCanBeCollidedWith = true;
        pVehicle->m_pVehicle->physicalFlags.bApplyGravity = true;
        pVehicle->m_pVehicle->physicalFlags.bDisableCollisionForce = false;
        pVehicle->m_pVehicle->physicalFlags.bDisableMoveForce = false;
        pVehicle->m_pVehicle->physicalFlags.bDontApplySpeed = false;
        pVehicle->m_pVehicle->m_nVehicleFlags.bEngineOn = 1;
        pVehicle->m_pVehicle->m_nVehicleFlags.bEngineBroken = 0;
        pVehicle->m_pVehicle->m_nVehicleFlags.bIsHandbrakeOn = 0;
        pVehicle->m_pVehicle->m_nVehicleFlags.bComedyControls = 0;
        pVehicle->m_pVehicle->m_nVehicleFlags.bIsBeingCarJacked = 0;
        pVehicle->m_pVehicle->m_nVehicleFlags.bIsDrowning = 0;
        pVehicle->m_pVehicle->m_nVehicleFlags.bParking = 0;
        pVehicle->m_pVehicle->UpdateRW();
        pVehicle->m_pVehicle->UpdateRwFrame();
    }

    pPed->PutDirectlyInVehicle(pVehicle->m_dwGTAId, byteSeatID);
    if (pVehicle->m_pVehicle) {
        pVehicle->m_pVehicle->m_bIsVisible = true;
        pVehicle->m_pVehicle->m_bRemoveFromWorld = false;
        pVehicle->m_pVehicle->UpdateRW();
        pVehicle->m_pVehicle->UpdateRwFrame();
        if (pPed->m_pPed && pPed->m_pPed->IsInVehicle()) {
            pPed->SetCameraMode(static_cast<uint8_t>(MODE_BEHINDCAR));
            CCamera& camera = GTASAEngineApi::Camera();
            camera.Restore();
            CCamera::SetBehindPlayer();
        }
        FLog("[VEH_VIS64] put-player vehicleId=%u seat=%u gta=%u model=%u rw=%p visible=%u inVeh=%u flags=0x%08x cam=native-behind",
             VehicleID,
             byteSeatID,
             pVehicle->m_dwGTAId,
             pVehicle->m_pVehicle->m_nModelIndex,
             pVehicle->m_pVehicle->m_pRwObject,
             pVehicle->m_pVehicle->m_bIsVisible ? 1 : 0,
             (pPed->m_pPed && pPed->m_pPed->IsInVehicle()) ? 1 : 0,
             pVehicle->m_pVehicle->m_nFlags);
    }
}
// 0.3.7
void ScrRemovePlayerFromVehicle(RPCParameters* rpcParams)
{
	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
	if (!pLocalPlayer) return;

	pLocalPlayer->GetPlayerPed()->ExitCurrentVehicle();
}
// 0.3.7
void ScrSetPlayerColor(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	PLAYERID PlayerID;
	uint32_t dwColor;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(PlayerID);
	bsData.Read(dwColor);

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	if (!pPlayerPool) return;

	if (pPlayerPool->GetLocalPlayerID() == PlayerID)
	{
		CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
		if (pLocalPlayer) {
			pLocalPlayer->SetPlayerColor(dwColor);
		}
	}
	else
	{
		CRemotePlayer* pRemotePlayer = pPlayerPool->GetAt(PlayerID);
		if (pRemotePlayer) {
			pRemotePlayer->SetPlayerColor(dwColor);
		}
	}
}
// 0.3.7
void ScrShowGameText(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	char szMsg[256];
	memset(szMsg, 0, sizeof(szMsg));

	int iSize;
	int iTime;
	int iLen;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(iSize);
	bsData.Read(iTime);
	bsData.Read(iLen);

	if (iLen < 0 || iLen > 200) return;

	bsData.Read(szMsg, iLen);
	szMsg[iLen] = '\0';
	pGame->DisplayGameText(szMsg, iTime, iSize);
}
// 0.3.7
void ScrSetVehiclePos(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	float fX;
	float fY;
	float fZ;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(VehicleID);
	bsData.Read(fX);
	bsData.Read(fY);
	bsData.Read(fZ);

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	pVehicle->TeleportTo(fX, fY, fZ);
}
// 0.3.7
void ScrSetVehicleZAngle(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	float fAngle;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(VehicleID);
	bsData.Read(fAngle);

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
	if (!pVehicle) return;

	pVehicle->SetZAngle(fAngle);
}
// 0.3.7
void ScrSetVehicleParams(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteObjective;
	uint8_t byteDoorsLocked;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(VehicleID);
	bsData.Read(byteObjective);
	bsData.Read(byteDoorsLocked);

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	pVehiclePool->AssignSpecialParamsToVehicle(VehicleID, byteObjective, byteDoorsLocked);
}
// 0.3.7
void ScrSetPlayerCameraBehindPlayer(RPCParameters* rpcParams)
{
    CCamera::SetBehindPlayer();
}
// 0.3.7
void ScrTogglePlayerControllable(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteControllable;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(byteControllable);

	pGame->FindPlayerPed()->TogglePlayerControllable(byteControllable);
}

void ScrPlayerPlaySound(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	int iSound;
	float fX, fY, fZ;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(iSound);
	bsData.Read(fX);
	bsData.Read(fY);
	bsData.Read(fZ);

	// sub_100A1B90(pGame->field_0, a2, a3, a4, a5);
}
// 0.3.7
void ScrSetWorldBounds(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float bounds[4];
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(bounds[0]);
	bsData.Read(bounds[1]);
	bsData.Read(bounds[2]);
	bsData.Read(bounds[3]);

	pNetGame->m_pNetSet->fWorldBounds[0] = bounds[0];
	pNetGame->m_pNetSet->fWorldBounds[1] = bounds[1];
	pNetGame->m_pNetSet->fWorldBounds[2] = bounds[2];
	pNetGame->m_pNetSet->fWorldBounds[3] = bounds[3];
}
// 0.3.7
void ScrGivePlayerMoney(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	int iMoney;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(iMoney);

	pGame->AddToLocalMoney(iMoney);
}
// 0.3.7
void ScrSetPlayerFacingAngle(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	float fRotation;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(fRotation);

	pGame->FindPlayerPed()->SetTargetRotation(fRotation);
}
// 0.3.7
void ScrResetPlayerMoney(RPCParameters* rpcParams)
{
	pGame->ResetLocalMoney();
}
// 0.3.7
void ScrResetPlayerWeapons(RPCParameters* rpcParams)
{
	pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed()->ClearWeapons();
}
// 0.3.7
void ScrLinkVehicleToInterior(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	VEHICLEID VehicleID;
	uint8_t byteInterior;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(VehicleID);
	bsData.Read(byteInterior);

	CVehiclePool* pVehiclePool = pNetGame->GetVehiclePool();
	if (!pVehiclePool) return;

	pVehiclePool->LinkToInterior(VehicleID, byteInterior);
}
// 0.3.7
void ScrSetPlayerArmour(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CLocalPlayer* pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();

	float fArmour;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(fArmour);
	pLocalPlayer->GetPlayerPed()->SetArmour(fArmour);
}
// 0.3.7
void ScrSetArmedWeapon(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint32_t dwWeapon;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(dwWeapon);
	if (dwWeapon >= 0 && dwWeapon <= 46) {
		CPlayerPed* pPlayerPed = pGame->FindPlayerPed();
		if (pPlayerPed) {
			pPlayerPed->SetArmedWeapon(dwWeapon, false);
		}
	}
}

#define ATTACH_BONE_SPINE	1
#define ATTACH_BONE_HEAD	2
#define ATTACH_BONE_LUPPER	3
#define ATTACH_BONE_RUPPER	4
#define ATTACH_BONE_LHAND	5
#define ATTACH_BONE_RHAND	6
#define ATTACH_BONE_LTHIGH	7
#define ATTACH_BONE_RTHIGH	8
#define ATTACH_BONE_LFOOT	9
#define ATTACH_BONE_RFOOT	10
#define ATTACH_BONE_RCALF	11
#define ATTACH_BONE_LCALF	12
#define ATTACH_BONE_LFARM	13
#define ATTACH_BONE_RFARM	14
#define ATTACH_BONE_LSHOULDER	15
#define ATTACH_BONE_RSHOULDER	16
#define ATTACH_BONE_NECK	17
#define ATTACH_BONE_JAW		18

int GetInternalBoneIDFromSampID(int sampid)
{
    switch (sampid)
    {
        case ATTACH_BONE_SPINE: // 3 or 2
            return 3;
        case ATTACH_BONE_HEAD: // ?
            return 5;
        case ATTACH_BONE_LUPPER: // left upper arm
            return 22;
        case ATTACH_BONE_RUPPER: // right upper arm
            return 32;
        case ATTACH_BONE_LHAND: // left hand
            return 34;
        case ATTACH_BONE_RHAND: // right hand
            return 24;
        case ATTACH_BONE_LTHIGH: // left thigh
            return 41;
        case ATTACH_BONE_RTHIGH: // right thigh
            return 51;
        case ATTACH_BONE_LFOOT: // left foot
            return 43;
        case ATTACH_BONE_RFOOT: // right foot
            return 53;
        case ATTACH_BONE_RCALF: // right calf
            return 52;
        case ATTACH_BONE_LCALF: // left calf
            return 42;
        case ATTACH_BONE_LFARM: // left forearm
            return 33;
        case ATTACH_BONE_RFARM: // right forearm
            return 23;
        case ATTACH_BONE_LSHOULDER: // left shoulder (claviacle)
            return 31;
        case ATTACH_BONE_RSHOULDER: // right shoulder (claviacle)
            return 21;
        case ATTACH_BONE_NECK: // neck
            return 4;
        case ATTACH_BONE_JAW: // jaw ???
            return 8; // i dont know
    }
    return 0;
}

void ScrSetPlayerAttachedObject(RPCParameters* rpcParams)
{
    FLog("ScrSetPlayerAttachedObject");

    unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
    int iBitLength = rpcParams->numberOfBitsOfData;
    RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);

    uint16_t playerId;
    uint32_t slot;
    bool create;
    int32_t model;
    uint32_t bone;
    float offsetX, offsetY, offsetZ;
    float rotX, rotY, rotZ;
    float scaleX, scaleY, scaleZ;
    uint32_t materialColor1, materialColor2;

    bsData.Read(playerId);
    bsData.Read(slot);
    bsData.Read(create);
    bsData.Read(model);
    bsData.Read(bone);
    bsData.Read(offsetX);
    bsData.Read(offsetY);
    bsData.Read(offsetZ);
    bsData.Read(rotX);
    bsData.Read(rotY);
    bsData.Read(rotZ);
    bsData.Read(scaleX);
    bsData.Read(scaleY);
    bsData.Read(scaleZ);
    bsData.Read(materialColor1);
    bsData.Read(materialColor2);

    CPlayerPed* pPed = nullptr;
    if (playerId == pNetGame->GetPlayerPool()->GetLocalPlayerID())
    {
        pPed = pNetGame->GetPlayerPool()->GetLocalPlayer()->GetPlayerPed();
    }
    else if (pNetGame->GetPlayerPool()->GetAt(playerId))
    {
        pPed = pNetGame->GetPlayerPool()->GetAt(playerId)->GetPlayerPed();
    }
    if (!pPed) return;

    if (!create)
    {
        pPed->DeattachObject(slot);
        return;
    }
    FLog("SetPlayerAttachedObject: pid=%u slot=%u create=%d model=%d bone=%u "
         "offset(%.3f, %.3f, %.3f) rot(%.3f, %.3f, %.3f) scale(%.3f, %.3f, %.3f) "
         "color1=0x%08X color2=0x%08X",
         (unsigned)playerId, slot, (int)create, model, bone,
         offsetX, offsetY, offsetZ,
         rotX, rotY, rotZ,
         scaleX, scaleY, scaleZ,
         materialColor1, materialColor2);


    ATTACHED_OBJECT_INFO info{};
    info.dwModelId = model;
    info.dwBoneId_MP = bone;
    info.vecOffset = CVector(offsetX, offsetY, offsetZ);
    info.vecRotation = CVector(rotX, rotY, rotZ);
    info.vecScale = CVector(scaleX, scaleY, scaleZ);
    info.dwColor[0] = materialColor1;
    info.dwColor[1] = materialColor2;

    pPed->AttachObject(&info, slot);
}
// 0.3.7
void ScrApplyActorAnimation(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CActorPool* pActorPool = pNetGame->GetActorPool();
	if (!pActorPool) return;

	char szAnimLib[256];
	char szAnimName[256];
	memset(szAnimLib, 0, 256);
	memset(szAnimName, 0, 256);

	PLAYERID ActorID;
	uint8_t byteAnimLibLen;
	uint8_t byteAnimNameLen;
	float fDelta;
	bool bLoop;
	bool bLockX;
	bool bLockY;
	bool bFreeze;
	int iTime;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(ActorID);
	bsData.Read(byteAnimLibLen);
	bsData.Read(szAnimLib, byteAnimLibLen);
	bsData.Read(byteAnimNameLen);
	bsData.Read(szAnimName, byteAnimNameLen);
	bsData.Read(fDelta);
	bsData.Read(bLoop);
	bsData.Read(bLockX);
	bsData.Read(bLockY);
	bsData.Read(bFreeze);
	bsData.Read(iTime);

	szAnimLib[byteAnimLibLen] = '\0';
	szAnimName[byteAnimNameLen] = '\0';

	CActor* pActor = pActorPool->GetAt(ActorID);
	if (pActor) {
		pActor->ApplyAnimation(szAnimName, szAnimLib, fDelta, bLoop, bLockX, bLockY, bFreeze, iTime);
	}
}
// 0.3.7
void ScrClearActorAnimation(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CActorPool* pActorPool = pNetGame->GetActorPool();
	if (!pActorPool) return;

	PLAYERID ActorID;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(ActorID);

	CActor* pActor = pActorPool->GetAt(ActorID);
	if (pActor) {
		pActor->ClearAnimation();
	}
}
// 0.3.7
void ScrSetActorFacingAngle(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CActorPool* pActorPool = pNetGame->GetActorPool();
	if (!pActorPool) return;

	PLAYERID ActorID;
	float fAngle;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(ActorID);
	bsData.Read(fAngle);

	CActor* pActor = pActorPool->GetAt(ActorID);
	if (pActor) {
		pActor->SetFacingAngle(fAngle);
	}
}
// 0.3.7
void ScrSetActorPos(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CActorPool* pActorPool = pNetGame->GetActorPool();
	if (!pActorPool) return;

	PLAYERID ActorID;
	CVector vecPos;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(ActorID);
	bsData.Read(vecPos.x);
	bsData.Read(vecPos.y);
	bsData.Read(vecPos.z);

	CActor* pActor = pActorPool->GetAt(ActorID);
	if (pActor) {
		Xyron::Collision::RequestCollision(vecPos, pActor->m_pPed->m_nAreaCode);
		pActor->m_pPed->SetPosn(vecPos.x, vecPos.y, vecPos.z);
		pActor->m_pPed->UpdateRW();
		pActor->m_pPed->UpdateRwFrame();
	}
}
// 0.3.7
void ScrSetActorHealth(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CActorPool* pActorPool = pNetGame->GetActorPool();
	if (!pActorPool) return;

	PLAYERID ActorID;
	float fHealth;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	bsData.Read(ActorID);
	bsData.Read(fHealth);

	CActor* pActor = pActorPool->GetAt(ActorID);
	if (pActor) {
		pActor->SetHealth(fHealth);
	}
}

void ScrPlayAudioStream(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	uint8_t byteUrlLen;
	char szUrl[256];
	float fX, fY, fZ;
	float fRadius;
	uint8_t byteUsePos;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	memset(szUrl, 0, sizeof(szUrl));

	bsData.Read(byteUrlLen);
	bsData.Read(szUrl, byteUrlLen);
	bsData.Read(fX);
	bsData.Read(fY);
	bsData.Read(fZ);
	bsData.Read(fRadius);
	bsData.Read(byteUsePos);

	if (pAudioStream)
		pAudioStream->Play(szUrl, fX, fY, fZ, fRadius, byteUsePos);
}

void ScrStopAudioStream(RPCParameters* rpcParams)
{
	if (pAudioStream)
		pAudioStream->Stop(false);
		//pAudioStream->Stop(true);
}

void ScrMoveObject(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	float fPad0, fPad1, fPad2;
	float fPosX, fPosY, fPosZ;
	float fSpeed;
	float fRotX, fRotY, fRotZ;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	if (!bsData.Read(ObjectID) ||
		!bsData.Read(fPad0) ||
		!bsData.Read(fPad1) ||
		!bsData.Read(fPad2) ||
		!bsData.Read(fPosX) ||
		!bsData.Read(fPosY) ||
		!bsData.Read(fPosZ) ||
		!bsData.Read(fSpeed) ||
		!bsData.Read(fRotX) ||
		!bsData.Read(fRotY) ||
		!bsData.Read(fRotZ)) {
		FLog("[OBJ_FIX64] ScrMoveObject malformed bits=%d", iBitLength);
		return;
	}

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) {
		FLog("[OBJ_FIX64] ScrMoveObject object pool unavailable");
		return;
	}
	CObject* pObject = pObjectPool->GetAt(ObjectID);
	if (pObject) {
		pObject->MoveTo(fPosX, fPosY, fPosZ, fSpeed, fRotX, fRotY, fRotZ);
	}
}
// 0.3.7
void ScrStopObject(RPCParameters* rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char*>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	OBJECTID ObjectID;
	RakNet::BitStream bsData(Data, (iBitLength / 8) + 1, false);
	if (!bsData.Read(ObjectID)) {
		FLog("[OBJ_FIX64] ScrStopObject malformed bits=%d", iBitLength);
		return;
	}

	CObjectPool* pObjectPool = pNetGame->GetObjectPool();
	if (!pObjectPool) {
		FLog("[OBJ_FIX64] ScrStopObject object pool unavailable");
		return;
	}
	CObject* pObject = pObjectPool->GetAt(ObjectID);

	if (pObject) {
		pObject->StopMoving();
	}
}

void AttachCameraToObject(RPCParameters *rpcParams)
{
	unsigned char* Data = reinterpret_cast<unsigned char *>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData((unsigned char*)Data, (iBitLength / 8) + 1, false);

	CObjectPool *pObjectPool = pNetGame->GetObjectPool();
	if(pObjectPool)
	{
		OBJECTID objectId;

		if (!bsData.Read(objectId)) {
			FLog("[OBJ_FIX64] AttachCameraToObject malformed bits=%d", iBitLength);
			return;
		}
		if(objectId >= MAX_OBJECTS)
			return;

		CObject *pObject = pObjectPool->GetAt(objectId);
		if(pObject)
		{
			//if(pGameCamera)
				//pGameCamera->AttachToEntity(pObject);
		}
	}
}

static uint32_t gBrObjectAliasHitCount = 0;

static void LogBrObjectAliasHit(const char* aliasName, RPCParameters* rpcParams)
{
    ++gBrObjectAliasHitCount;
    if (gBrObjectAliasHitCount <= 50 || (gBrObjectAliasHitCount % 100) == 0) {
        const int bitLength = rpcParams ? rpcParams->numberOfBitsOfData : -1;
        FLog("[OBJ_BR64] alias=%s hit=%u bits=%d",
             aliasName ? aliasName : "unknown",
             gBrObjectAliasHitCount,
             bitLength);
    }
}

static void ScrCreateObject_BR(RPCParameters* rpcParams)
{
    LogBrObjectAliasHit("ScrCreateObject(0x14A)", rpcParams);
    ScrCreateObject(rpcParams);
}

static void ScrSetObjectPos_BR(RPCParameters* rpcParams)
{
    LogBrObjectAliasHit("ScrSetObjectPos(0x197)", rpcParams);
    ScrSetObjectPos(rpcParams);
}

static void ScrSetObjectRotation_BR(RPCParameters* rpcParams)
{
    LogBrObjectAliasHit("ScrSetObjectRotation(0x144)", rpcParams);
    ScrSetObjectRotation(rpcParams);
}

static void ScrDestroyObject_BR(RPCParameters* rpcParams)
{
    LogBrObjectAliasHit("ScrDestroyObject(0x14B)", rpcParams);
    ScrDestroyObject(rpcParams);
}

static void ScrMoveObject_BR(RPCParameters* rpcParams)
{
    LogBrObjectAliasHit("ScrMoveObject(0x1A7)", rpcParams);
    ScrMoveObject(rpcParams);
}

static void ScrStopObject_BR(RPCParameters* rpcParams)
{
    LogBrObjectAliasHit("ScrStopObject(0x17A)", rpcParams);
    ScrStopObject(rpcParams);
}

static void ScrSetObjectMaterial_BR(RPCParameters* rpcParams)
{
    LogBrObjectAliasHit("ScrSetObjectMaterial(0x16C)", rpcParams);
    ScrSetObjectMaterial(rpcParams);
}

void RegisterScriptRPCs(RakClientInterface *pRakClient)
{
	FLog("Registering script RPC's..");

	// RPC_ScrDisableVehicleCollision
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetMapIcon, ScrSetPlayerMapIcon);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDisableMapIcon, ScrRemovePlayerMapIcon);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetWeaponAmmo, ScrSetPlayerAmmo);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetGravity, ScrSetGravity);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehicleHealth, ScrSetVehicleHealth);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrAttachTrailerToVehicle, ScrAttachTrailerToVehicle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDetachTrailerFromVehicle, ScrDetachTrailerFromVehicle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreateObject, ScrCreateObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetObjectPos, ScrSetObjectPos);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetObjectRotation, ScrSetObjectRotation);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDestroyObject, ScrDestroyObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreateExplosion, ScrCreateExplosion);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrShowNameTag, ScrShowNameTag);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrMoveObject, ScrMoveObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrStopObject, ScrStopObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrNumberPlate, ScrSetVehicleNumberPlate);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating, ScrTogglePlayerSpectating);
	// RPC_null - unused
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPlayerSpectatePlayer, ScrSpectatePlayer);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPlayerSpectateVehicle, ScrSpectateVehicle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrRemoveComponent, ScrRemoveVehicleComponent);
	// RPC_ScrForceClassSelection - useless
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrAttachObjectToPlayer, ScrAttachObjectToPlayer);
	// RPC_ScrInitMenu
	// RPC_ScrShowMenu
	// RPC_ScrHideMenu
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerWantedLevel, ScrSetPlayerWantedLevel);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrShowTextDraw, ScrShowTextDraw);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHideTextDraw, ScrHideTextDraw);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrTextDrawSetString, ScrTextDrawSetString);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrAddGangZone, ScrAddGangZone);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrRemoveGangZone, ScrGangZoneDestroy);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrFlashGangZone, ScrGangZoneFlash);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrStopFlashGangZone, ScrGangZoneStopFlash);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrApplyPlayerAnimation, ScrApplyPlayerAnimation);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrClearPlayerAnimations, ScrClearPlayerAnimations);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetSpecialAction, ScrSetPlayerSpecialAction);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrEnableStuntBonus, ScrEnableStuntBonus);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetFightingStyle, ScrSetPlayerFightingStyle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerVelocity, ScrSetPlayerVelocity);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehicleVelocity, ScrSetVehicleVelocity);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrToggleWidescreen, ScrToggleWidescreen);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehicleTireStatus, ScrSetVehicleTireDamageStatus);
	// RPC_150 ???
	// RPC_92 ???
	// RPC_ScrPlayCrimeReport
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo, ScrSetSpawnInfo);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerTeam, ScrSetPlayerTeam);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerSkin, ScrSetPlayerSkin);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerName, ScrSetPlayerName);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos, ScrSetPlayerPos);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPosFindZ, ScrSetPlayerPosFindZ);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerHealth, ScrSetPlayerHealth);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPutPlayerInVehicle, ScrPutPlayerInVehicle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrRemovePlayerFromVehicle, ScrRemovePlayerFromVehicle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerColor, ScrSetPlayerColor);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDisplayGameText, ScrShowGameText);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetInterior, ScrSetPlayerInterior);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetCameraPos, ScrSetCameraPos);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetCameraLookAt, ScrSetCameraLookAt);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehiclePos, ScrSetVehiclePos);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetVehicleZAngle, ScrSetVehicleZAngle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrVehicleParams, ScrSetVehicleParams);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetCameraBehindPlayer, ScrSetPlayerCameraBehindPlayer);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerControllable, ScrTogglePlayerControllable);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPlaySound, ScrPlayerPlaySound);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetWorldBounds, ScrSetWorldBounds);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHaveSomeMoney, ScrGivePlayerMoney);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerFacingAngle, ScrSetPlayerFacingAngle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrResetMoney, ScrResetPlayerMoney);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrResetPlayerWeapons, ScrResetPlayerWeapons);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrGivePlayerWeapon, ScrGivePlayerWeapon);
	// RPC_64 - unused
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrLinkVehicle, ScrLinkVehicleToInterior);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerArmour, ScrSetPlayerArmour);
	// RPC_ScrSendDeathMessage
	// RPC_ScrSetShopName
	// RPC_ScrSetPlayerDrunkLevel
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetArmedWeapon, ScrSetArmedWeapon);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerAttachedObject, ScrSetPlayerAttachedObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrPlayAudioStream, ScrPlayAudioStream);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrStopAudioStream, ScrStopAudioStream);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrRemoveBuilding, ScrRemoveBuilding);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrAttachCameraToObject, AttachCameraToObject);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrInterpolateCamera, ScrInterpolateCamera);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ClickTextDraw, ScrSelectTextDraw);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetObjectMaterial, ScrSetObjectMaterial);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_BR_ScrCreateObject, ScrCreateObject_BR);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_BR_ScrSetObjectPos, ScrSetObjectPos_BR);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_BR_ScrSetObjectRotation, ScrSetObjectRotation_BR);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_BR_ScrDestroyObject, ScrDestroyObject_BR);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_BR_ScrMoveObject, ScrMoveObject_BR);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_BR_ScrStopObject, ScrStopObject_BR);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_BR_ScrSetObjectMaterial, ScrSetObjectMaterial_BR);
	// RPC_ScrObjectNoCameraCol
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrApplyActorAnimation, ScrApplyActorAnimation);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrClearActorAnimations, ScrClearActorAnimation);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetActorFacingAngle, ScrSetActorFacingAngle);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetActorPos, ScrSetActorPos);
	pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetActorHealth, ScrSetActorHealth);
	// RPC_SetPlayerVirtualWorld - 03DL only
}

void UnregisterScriptRPCs(RakClientInterface *pRakClient)
{
	LOGI("Unregistering script RPC's..");

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetMapIcon);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDisableMapIcon);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetGravity);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreateObject);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_BR_ScrCreateObject);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrAddGangZone);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrRemoveGangZone);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrFlashGangZone);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrStopFlashGangZone);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetCameraPos);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetCameraLookAt);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrRemoveBuilding);

	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrInterpolateCamera);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_BR_ScrSetObjectPos);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_BR_ScrSetObjectRotation);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_BR_ScrDestroyObject);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_BR_ScrMoveObject);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_BR_ScrStopObject);
	pRakClient->UnregisterAsRemoteProcedureCall(&RPC_BR_ScrSetObjectMaterial);
}
