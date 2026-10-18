#include "../main.h"
#include "../game/game.h"
#include "netgame.h"
#include "../gui/gui.h"
#include "../audiostream.h"
#include "../game/util.h"
#include "../settings.h"
#include "../util/CUtil.h"
#include "../perf/PerformanceProfiler.h"
#include "nettransport.h"

// voice
#include "../voice_new/MicroIcon.h"
#include "../voice_new/SpeakerList.h"
#include "../voice_new/Network.h"
#include "java/jniutil.h"

#include <cstring>

//#define AUTH_BS "39FB2DEEDB49ACFB8D4EECE6953D2507988CCCF4410"//main
#define AUTH_BS "15121F6F18550C00AC4B4F8A167D0379BB0ACA99043"

extern UI* pUI;
extern CGame* pGame;
extern CAudioStream* pAudioStream;
extern CSettings* pSettings;

namespace {
constexpr size_t kSampNickMinLen = 3;
constexpr size_t kSampNickMaxLen = 20;

bool IsSampNickChar(char value)
{
	return (value >= 'A' && value <= 'Z') ||
		(value >= 'a' && value <= 'z') ||
		(value >= '0' && value <= '9') ||
		value == '_';
}

void BuildSafeSampNick(const char* input, char* output, size_t outputSize)
{
	if (!output || outputSize == 0) {
		return;
	}

	size_t out = 0;
	if (input) {
		for (size_t in = 0; input[in] && out < kSampNickMaxLen; ++in) {
			if (IsSampNickChar(input[in])) {
				output[out++] = input[in];
			}
		}
	}
	output[out] = '\0';

	if (out < kSampNickMinLen) {
		strncpy(output, "Player123", outputSize - 1);
		output[outputSize - 1] = '\0';
	}

	if (!input || strcmp(input, output) != 0) {
		FLog("[NICK_FIX64] sanitized connect nick '%s' -> '%s'", input ? input : "", output);
	}
}
}

int iNetModeNormalOnFootSendRate = 30;
int iNetModeNormalInCarSendRate = 30;
int iNetModeFiringSendRate = 30;
int iNetModeSendMultiplier = 2;

void RegisterRPCs(RakClientInterface *pRakClient);
void UnregisterRPCs(RakClientInterface *pRakClient);
void RegisterScriptRPCs(RakClientInterface *pRakClient);
void UnregisterScriptRPCs(RakClientInterface *pRakClient);
void ScrCreateObject(RPCParameters* rpcParams);
void ScrSetObjectPos(RPCParameters* rpcParams);
void ScrSetObjectRotation(RPCParameters* rpcParams);
void ScrDestroyObject(RPCParameters* rpcParams);
void ScrMoveObject(RPCParameters* rpcParams);
void ScrStopObject(RPCParameters* rpcParams);
void ScrSetObjectMaterial(RPCParameters* rpcParams);

void InstallVehicleEngineLightPatches();

extern CJavaWrapper *pJavaWrapper;

CNetGame::CNetGame(const char* szHostOrIp, int iPort, const char *szPlayerName, const char* szPass)
{
	FLog("CNetGame initializing..");
    ResetRemovedBuildings();
	char safePlayerName[kSampNickMaxLen + 1] = {};
	BuildSafeSampNick(szPlayerName, safePlayerName, sizeof(safePlayerName));

	// voice
	Network::OnRaknetConnect(szHostOrIp, iPort);

	//MyLog2("Voice connect %s:%d", szHostOrIp, iPort);
	//MyLog2("Voice connect %s:%d", szHostOrIp, iPort);
	//MyLog2("Voice connect %s:%d", szHostOrIp, iPort);

	m_pNetSet = new NET_SETTINGS;
	memset(m_szHostName, 0, 256);
	memset(m_szHostOrIp, 0, 256);

	strcpy(m_szHostName, "SA-MP");
	strncpy(m_szHostOrIp, szHostOrIp, sizeof(m_szHostOrIp));
	m_iPort = iPort;

	m_pRakClient = RakNetworkFactory::GetRakClientInterface();
	InitializePools();

	GetPlayerPool()->SetLocalPlayerName(safePlayerName);

	RegisterRPCs(m_pRakClient);
	RegisterScriptRPCs(m_pRakClient);
	m_pRakClient->SetPassword(szPass);

	memset(m_dwMapIcon, 0, sizeof(m_dwMapIcon));

	pGame->EnableClock(false);
	pGame->EnableZoneNames(false);

	m_pNetSet->iDeathDropMoney = 0;
	m_pNetSet->iSpawnsAvailable = 0;
	m_pNetSet->bNameTagLOS = 0;
	m_pNetSet->byteHoldTime = true;
	m_pNetSet->bUseCJWalk = 0;
	m_pNetSet->bDisableInteriorEnterExits = 0;
	m_pNetSet->bZoneNames = false;
	m_pNetSet->bInstagib = false;
	m_pNetSet->fNameTagDrawDistance = 70.0f;
	m_pNetSet->bFriendlyFire = true;
	m_pNetSet->byteWorldTime_Hour = 12;
	m_pNetSet->byteWorldTime_Minute = 0;
	m_pNetSet->byteWeather = 1;
	m_pNetSet->fGravity = 0.008f;
	m_bNameTagStatus = true;

	m_dwLastConnectAttempt = GetTickCount();
	SetGameState(GAMESTATE_WAIT_CONNECT);
	m_bLanMode = false;

    pJavaWrapper->HideLoadingScreen();

    const char* sampVer = SAMP_VERSION;
    if(pSettings)
        sampVer = pSettings->Get().szVersion;

	if (pUI) pUI->chat()->addDebugMessage("{FFFFFF}SA-MP {B9C9BF}%s {FFFFFF}Started", sampVer);
}
// 0.3.7
CNetGame::~CNetGame()
{
	// voice
	//Network::OnRaknetDisconnect();

	NetTransport::Disconnect(m_pRakClient, 0);
	UnregisterRPCs(m_pRakClient);
	UnregisterScriptRPCs(m_pRakClient);
	RakNetworkFactory::DestroyRakClientInterface(m_pRakClient);

	UninitializePools();

	if (m_pNetSet) {
		delete m_pNetSet;
		m_pNetSet = nullptr;
	}
}

void CNetGame::InitializePools()
{
	m_pPools = new NET_POOLS;
	m_pPools->pPlayerPool = new CPlayerPool();
	m_pPools->pVehiclePool = new CVehiclePool();
	m_pPools->pGangZonePool = new CGangZonePool();
	m_pPools->pPickupPool = new CPickupPool();
	m_pPools->pObjectPool = new CObjectPool();
	m_pPools->pTextLabelPool = new C3DTextLabelPool();
	m_pPools->pTextDrawPool = new CTextDrawPool();
	m_pPools->pActorPool = new CActorPool();
	m_pPools->pMenuPool = new CMenuPool();
	m_pPools->pPlayerBubblePool = new CPlayerBubblePool();
}

void CNetGame::UninitializePools()
{
	if (m_pPools->pPlayerPool) {
		delete m_pPools->pPlayerPool;
		m_pPools->pPlayerPool = nullptr;
	}

	if (m_pPools->pVehiclePool) {
		delete m_pPools->pVehiclePool;
		m_pPools->pVehiclePool = nullptr;
	}

	if (m_pPools->pGangZonePool) {
		delete m_pPools->pGangZonePool;
		m_pPools->pGangZonePool = nullptr;
	}

	if (m_pPools->pPickupPool) {
		delete m_pPools->pPickupPool;
		m_pPools->pPickupPool = nullptr;
	}

	if (m_pPools->pObjectPool) {
		delete m_pPools->pObjectPool;
		m_pPools->pObjectPool = nullptr;
	}

	if (m_pPools->pTextLabelPool) {
		delete m_pPools->pTextLabelPool;
		m_pPools->pTextLabelPool = nullptr;
	}

	if (m_pPools->pTextDrawPool) {
		delete m_pPools->pTextDrawPool;
		m_pPools->pTextDrawPool = nullptr;
	}

	if (m_pPools->pActorPool) {
		delete m_pPools->pActorPool;
		m_pPools->pActorPool = nullptr;
	}

	if (m_pPools->pMenuPool) {
		delete m_pPools->pMenuPool;
		m_pPools->pMenuPool = nullptr;
	}

	if (m_pPools->pPlayerBubblePool)
	{
		delete m_pPools->pPlayerBubblePool;
		m_pPools->pPlayerBubblePool = nullptr;
	}

	if (m_pPools) {
		delete m_pPools;
		m_pPools = nullptr;
	}

	if (m_pNetSet) {
		delete m_pNetSet;
		m_pNetSet = nullptr;
	}
}

void CNetGame::Process()
{
	Xyron::Perf::ScopedZone perfNetGame(Xyron::Perf::Zone::NetGame);
	static uint32_t time = GetTickCount();
	static int s_lastDiagState = 0;
	static uint32_t s_stateDiagStarted = 0;
	static uint32_t s_lastStateDiagLog = 0;
	bool bProcess = false;
	const uint32_t now = GetTickCount();
	const int currentState = GetGameState();
	if (currentState != s_lastDiagState) {
		s_lastDiagState = currentState;
		s_stateDiagStarted = now;
		s_lastStateDiagLog = 0;
		FLog("[NET_STATE64] state=%d host=%s:%d", currentState, m_szHostOrIp, m_iPort);
	}
	if ((currentState == GAMESTATE_CONNECTING || currentState == GAMESTATE_AWAIT_JOIN) &&
		(now - s_lastStateDiagLog) > 5000)
	{
		s_lastStateDiagLog = now;
		FLog("[NET_STATE64] waiting state=%d elapsed=%u host=%s:%d",
			 currentState,
			 now - s_stateDiagStarted,
			 m_szHostOrIp,
			 m_iPort);
	}
	if (currentState == GAMESTATE_CONNECTING && (now - s_stateDiagStarted) > 15000) {
		FLog("[NET_STATE64] connect-timeout elapsed=%u host=%s:%d reconnecting",
			 now - s_stateDiagStarted,
			 m_szHostOrIp,
			 m_iPort);
		if (m_pRakClient) {
			NetTransport::Disconnect(m_pRakClient, 0);
		}
		SetGameState(GAMESTATE_WAIT_CONNECT);
		m_dwLastConnectAttempt = 0;
		return;
	}
	if (currentState == GAMESTATE_AWAIT_JOIN && (now - s_stateDiagStarted) > 20000) {
		FLog("[NET_STATE64] await-join-timeout elapsed=%u host=%s:%d reconnecting",
			 now - s_stateDiagStarted,
			 m_szHostOrIp,
			 m_iPort);
		if (m_pRakClient) {
			NetTransport::Disconnect(m_pRakClient, 0);
		}
		SetGameState(GAMESTATE_WAIT_CONNECT);
		m_dwLastConnectAttempt = 0;
		return;
	}
	if (GetTickCount() - time >= 1000 / 30)
	{
		UpdateNetwork();
		time = GetTickCount();
		bProcess = true;
	}
	if (m_pNetSet->byteHoldTime) {
		pGame->SetWorldTime(m_pNetSet->byteWorldTime_Hour, m_pNetSet->byteWorldTime_Minute);
	}

	//pGame->PreloadObjectsAnims();

	if (GetGameState() == GAMESTATE_CONNECTED) {
		ProcessPools();
	}
	else {
		ProcessLoadingScreen();
	}

	if (GetGameState() == GAMESTATE_WAIT_CONNECT) {
		ProcessConnecting();
	}
}

void CNetGame::UpdateNetwork()
{
	Xyron::Perf::ScopedZone perfNetworkReceive(Xyron::Perf::Zone::NetworkReceive);
	bool breakStatus = false;
	Packet *pkt = nullptr;
	unsigned char packetIdentifier;

	while ((pkt = NetTransport::Receive(m_pRakClient)))
	{
		packetIdentifier = NetTransport::PacketId(pkt);
        static uint32_t sPktDiagCount = 0;
        ++sPktDiagCount;
        if (sPktDiagCount <= 400 || (sPktDiagCount % 200) == 0) {
            const uint32_t pktLen = pkt ? pkt->length : 0;
            FLog("[PKT64] n=%u id=%u len=%u", sPktDiagCount, packetIdentifier, pktLen);
        }

		switch (packetIdentifier) {
            case ID_AUTH_KEY:
                Packet_AuthKey(pkt);
                break;

            case ID_CONNECTION_ATTEMPT_FAILED:
                Packet_ConnectAttemptFailed(pkt);
                break;

            case ID_NO_FREE_INCOMING_CONNECTIONS:
                Packet_NoFreeIncomingConnections(pkt);
                break;

            case ID_DISCONNECTION_NOTIFICATION:
                Packet_DisconnectionNotification(pkt);
                //SetGameState(GAMESTATE_WAIT_CONNECT);
                break;

            case ID_CONNECTION_LOST:
                Packet_ConnectionLost(pkt);
                break;

            case ID_CONNECTION_REQUEST_ACCEPTED:
                Packet_ConnectionSucceeded(pkt);
                break;

            case ID_FAILED_INITIALIZE_ENCRIPTION:
                Packet_FailedInitializeEncription(pkt);
                break;

            case ID_CONNECTION_BANNED:
                Packet_ConnectionBanned(pkt);
                SetGameState(GAMESTATE_WAIT_CONNECT);
                break;

            case ID_INVALID_PASSWORD:
                Packet_InvalidPassword(pkt);
                break;

            case ID_VEHICLE_SYNC:
                Packet_VehicleSync(pkt);
                break;

            case ID_AIM_SYNC:
                Packet_AimSync(pkt);
                break;

            case ID_BULLET_SYNC:
                Packet_BulletSync(pkt);
                break;

            case ID_PLAYER_SYNC:
                Packet_PlayerSync(pkt);
                break;

            case ID_MARKERS_SYNC:
                Packet_MarkerSync(pkt);
                break;

            case ID_UNOCCUPIED_SYNC:
                Packet_UnoccupiedSync(pkt);
                break;

			case ID_TRAILER_SYNC:
				Packet_TrailerSync(pkt);
				break;

			case ID_PASSENGER_SYNC:
				Packet_PassengerSync(pkt);
				break;

			case Network::kRaknetPacketId: {
				Network::OnRaknetReceive(pkt);
				break;
			}

            case ID_CUSTOM_SYNC:
            case 251:
				Packet_CustomRPC(pkt);
				break;

            case ID_MODIFIED_PACKET:
                FLog("[PKT_WARN64] modified-packet len=%u", pkt ? pkt->length : 0);
                break;

            default:
            {
                static uint32_t sUnhandledPktCount = 0;
                ++sUnhandledPktCount;
                if (sUnhandledPktCount <= 160 || (sUnhandledPktCount % 80) == 0) {
                    const unsigned int pktLen = pkt ? pkt->length : 0;
                    const unsigned int head = (pkt && pkt->data && pkt->length > 0) ? (unsigned int)(uint8_t)pkt->data[0] : 255u;
                    FLog("[PKT_UNH64] n=%u id=%u head=%u len=%u", sUnhandledPktCount, packetIdentifier, head, pktLen);
                }
                break;
            }

        }

		// voice
		/*if (!Network::OnRaknetReceive(*pkt)) {
			return;
		}*/

		NetTransport::Deallocate(m_pRakClient, pkt);
	}
}

void CNetGame::Packet_CustomRPC(Packet *p) {
    if (!p || !p->data || p->length <= 1) {
        return;
    }
    uint8_t packetId = static_cast<uint8_t>(p->data[0]);
    int headerSkipBytes = 1;
    if (packetId == ID_TIMESTAMP) {
        const int idOff32 = 1 + 4;
        const int idOff64 = 1 + 8;
        const uint8_t id32 = (p->length > (unsigned int)idOff32) ? static_cast<uint8_t>(p->data[idOff32]) : 255;
        const uint8_t id64 = (p->length > (unsigned int)idOff64) ? static_cast<uint8_t>(p->data[idOff64]) : 255;

        if (id64 == ID_CUSTOM_SYNC || id64 == 251) {
            packetId = id64;
            headerSkipBytes = idOff64 + 1;
        } else if (id32 == ID_CUSTOM_SYNC || id32 == 251) {
            packetId = id32;
            headerSkipBytes = idOff32 + 1;
        } else {
            // Fallback to 32-bit timestamp layout.
            headerSkipBytes = idOff32 + 1;
        }
    }

    static uint32_t sCustomPktLogCount = 0;
    ++sCustomPktLogCount;
    if (sCustomPktLogCount <= 180 || (sCustomPktLogCount % 80) == 0) {
        char preview[80];
        int previewOffset = 0;
        preview[0] = '\0';
        const int previewBytes = (p->length < 12) ? p->length : 12;
        for (int i = 0; i < previewBytes && previewOffset < (int)sizeof(preview) - 4; ++i) {
            previewOffset += snprintf(preview + previewOffset,
                                      sizeof(preview) - previewOffset,
                                      "%02X%s",
                                      static_cast<unsigned char>(p->data[i]),
                                      (i + 1 < previewBytes) ? " " : "");
        }
        FLog("[RPCX64] n=%u pkt=%u len=%u head=%s", sCustomPktLogCount, packetId, p->length, preview);
    }

    auto dispatchCustomPayload = [&](RakNet::BitStream& bsData,
                                     uint32_t rpcID,
                                     const char* tag,
                                     void (*handler)(RPCParameters*),
                                     const char* layoutTag) -> bool {
        if (!handler) {
            return false;
        }

        const int payloadBitOffset = bsData.GetReadOffset();
        if (payloadBitOffset < 0 || (payloadBitOffset % 8) != 0) {
            FLog("[OBJ_ROOT64] custom-rpc skip unaligned pkt=%u id=%u tag=%s layout=%s bitOffset=%d",
                 packetId,
                 rpcID,
                 tag ? tag : "unknown",
                 layoutTag ? layoutTag : "?",
                 payloadBitOffset);
            return false;
        }

        const unsigned int payloadBits = static_cast<unsigned int>(bsData.GetNumberOfUnreadBits());
        if (payloadBits == 0) {
            FLog("[OBJ_ROOT64] custom-rpc empty payload pkt=%u id=%u tag=%s layout=%s",
                 packetId,
                 rpcID,
                 tag ? tag : "unknown",
                 layoutTag ? layoutTag : "?");
            return false;
        }

        unsigned char* payloadData = bsData.GetData() + (payloadBitOffset / 8);
        if (!payloadData) {
            return false;
        }

        RPCParameters params{};
        params.input = payloadData;
        params.numberOfBitsOfData = payloadBits;
        params.sender = p->playerId;
        params.recipient = nullptr;
        params.replyToSender = nullptr;

        FLog("[OBJ_ROOT64] custom-rpc dispatch pkt=%u id=%u tag=%s layout=%s bits=%u",
             packetId,
             rpcID,
             tag ? tag : "unknown",
             layoutTag ? layoutTag : "?",
             payloadBits);
        handler(&params);
        return true;
    };

    auto handleCustomRpc = [&](RakNet::BitStream& bsData, uint32_t rpcID, const char* layoutTag) -> bool {
        switch (rpcID) {
            case 99: {
                FLog("RPC_CHECK_CASH");
                uint8_t bLen, bLen1;
                uint16_t bVersion;
                char szText[30];
                char szText1[30];

                memset(szText, 0, 30);
                memset(szText1, 0, 30);

                bsData.Read(bLen);
                if (bLen >= sizeof(szText) - 1)
                    return true;

                bsData.Read(&szText[0], bLen);

                bsData.Read(bLen1);
                if (bLen1 >= sizeof(szText1) - 1)
                    return true;

                bsData.Read(&szText1[0], bLen1);

                bsData.Read(bVersion);

                RwTexture *pCashTexture = nullptr;
                pCashTexture = (RwTexture *) CUtil::LoadTextureFromDB(szText1, szText);

                int iVersion;
                if (pCashTexture) {
                    iVersion = bVersion;
                    RwTextureDestroy(pCashTexture);
                } else iVersion = 0;

                RakNet::BitStream bsParams;
                bsParams.Write(packetId);
                bsParams.Write((uint32_t)99);
                bsParams.Write(iVersion);
                NetTransport::Send(m_pRakClient, &bsParams, SYSTEM_PRIORITY, RELIABLE, 0, "custom-rpc-reply");
                return true;
            }

            // Black Russia/custom object RPC ids routed through custom packet.
            case 0x144: return dispatchCustomPayload(bsData, rpcID, "ScrSetObjectRotation(0x144)", ScrSetObjectRotation, layoutTag);
            case 0x14A: return dispatchCustomPayload(bsData, rpcID, "ScrCreateObject(0x14A)", ScrCreateObject, layoutTag);
            case 0x14B: return dispatchCustomPayload(bsData, rpcID, "ScrDestroyObject(0x14B)", ScrDestroyObject, layoutTag);
            case 0x16C: return dispatchCustomPayload(bsData, rpcID, "ScrSetObjectMaterial(0x16C)", ScrSetObjectMaterial, layoutTag);
            case 0x17A: return dispatchCustomPayload(bsData, rpcID, "ScrStopObject(0x17A)", ScrStopObject, layoutTag);
            case 0x197: return dispatchCustomPayload(bsData, rpcID, "ScrSetObjectPos(0x197)", ScrSetObjectPos, layoutTag);
            case 0x1A7: return dispatchCustomPayload(bsData, rpcID, "ScrMoveObject(0x1A7)", ScrMoveObject, layoutTag);
            default:
                return false;
        }
    };

    auto tryLayout = [&](int extraSkipBytes, int rpcIdBytes, const char* layoutTag) -> bool {
        RakNet::BitStream bsData((unsigned char*)p->data, p->length, false);
        const int baseSkipBits = headerSkipBytes * 8;
        if (bsData.GetNumberOfUnreadBits() < baseSkipBits) {
            return false;
        }
        bsData.IgnoreBits(baseSkipBits);
        if (extraSkipBytes > 0) {
            const int skipBits = extraSkipBytes * 8;
            if (bsData.GetNumberOfUnreadBits() < skipBits) {
                return false;
            }
            bsData.IgnoreBits(skipBits);
        }

        uint32_t rpcID = 0;
        if (rpcIdBytes == 4) {
            if (bsData.GetNumberOfUnreadBits() < 32 || !bsData.Read(rpcID)) {
                return false;
            }
        } else if (rpcIdBytes == 2) {
            uint16_t rpc16 = 0;
            if (bsData.GetNumberOfUnreadBits() < 16 || !bsData.Read(rpc16)) {
                return false;
            }
            rpcID = rpc16;
        } else if (rpcIdBytes == 1) {
            uint8_t rpc8 = 0;
            if (bsData.GetNumberOfUnreadBits() < 8 || !bsData.Read(rpc8)) {
                return false;
            }
            rpcID = rpc8;
        } else {
            return false;
        }

        return handleCustomRpc(bsData, rpcID, layoutTag);
    };

    if (tryLayout(0, 4, "id32@0")) return;
    if (tryLayout(1, 4, "id32@1")) return;
    if (tryLayout(0, 2, "id16@0")) return;
    if (tryLayout(1, 2, "id16@1")) return;
    (void)tryLayout(0, 1, "id8@0");
}

// 0.3.7
void CNetGame::ShutdownForGameModeRestart()
{
    ResetRemovedBuildings();
	// voice
	SpeakerList::Hide();
	MicroIcon::Hide();
	Network::OnRaknetDisconnect();

	for (PLAYERID playerId = 0; playerId < MAX_PLAYERS; playerId++)
	{
		CRemotePlayer* pRemotePlayer = GetPlayerPool()->GetAt(playerId);
		if (pRemotePlayer) {
			pRemotePlayer->SetTeam(NO_TEAM);
			pRemotePlayer->ResetAllSyncAttributes();
		}
	}

	GetPlayerPool()->GetLocalPlayer()->ResetAllSyncAttributes();
	GetPlayerPool()->GetLocalPlayer()->ToggleSpectating(false);
	GameResetStats();

	if (pAudioStream) { //add new
		pAudioStream->Stop(true);
	}

	SetGameState(GAMESTATE_RESTARTING);
	GetPlayerPool()->DeactivateAll();
	GetPlayerPool()->Process();
	ResetVehiclePool();
	ResetActorPool();
	ResetTextDrawPool();
	ResetGangZonePool();
	Reset3DTextLabelPool();
	ResetPickupPool();
	ResetObjectPool();
	ResetMenuPool();

	m_pNetSet->bDisableInteriorEnterExits = false;
	m_pNetSet->fNameTagDrawDistance = 70.0f;
	m_pNetSet->byteWorldTime_Hour = 12;
	m_pNetSet->byteWorldTime_Minute = 0;
	m_pNetSet->byteWeather = 1;
	m_pNetSet->byteHoldTime = 1;
	m_pNetSet->bNameTagLOS = true;
	m_pNetSet->bUseCJWalk = false;
	pGame->ToggleCJWalk(false);
	m_pNetSet->fGravity = 0.008f;
	m_pNetSet->iDeathDropMoney = 0;

	pGame->m_bCheckpointsEnabled = false;
	pGame->m_bRaceCheckpointsEnabled = false;

	pGame->FindPlayerPed()->m_pPed->SetInterior(0, true);
	pGame->ResetLocalMoney();
	pGame->FindPlayerPed()->SetDead();
	pGame->FindPlayerPed()->SetArmour(0.0f);
	pGame->EnableZoneNames(false);
	m_pNetSet->bZoneNames = false;
	GameResetRadarColors();
	pGame->SetGravity(m_pNetSet->fGravity);
	pGame->SetWantedLevel(0);
	pGame->EnableClock(false);

	// voice
	//SpeakerList::Hide();
	//MicroIcon::Hide();
	//Network::OnRaknetDisconnect();

	if (pUI) pUI->chat()->addInfoMessage("The server is restarting..");
}

int iVehiclePoolProcessFlag = 0;
void CNetGame::ProcessPools()
{
	Xyron::Perf::ScopedZone perfPools(Xyron::Perf::Zone::Pools);
	if (GetPlayerPool()) {
		GetPlayerPool()->Process();
	}

	if(GetVehiclePool()) {
		GetVehiclePool()->Process();
	}

	if (GetPickupPool()) {
		GetPickupPool()->Process();
	}
}	
// 0.3.7
void CNetGame::ProcessLoadingScreen()
{
	CPlayerPed* pPlayerPed = pGame->FindPlayerPed();
#if VER_x64
	static uint32_t s_lastLoadingTeleportTick = 0;
	const uint32_t now = GetTickCount();
	const bool shouldRefreshLoadingPose = (now - s_lastLoadingTeleportTick) > 10000;
	if (shouldRefreshLoadingPose) {
		s_lastLoadingTeleportTick = now;
		FLog("[LOAD64] refreshing loading pose while waiting for connection");
#endif
	if (pPlayerPed->IsInVehicle()) {
		pPlayerPed->RemoveFromVehicleAndPutAt(1093.4, -2036.5, 82.710602);
	}
	else {
		pPlayerPed->TeleportTo(1133.0504, -2038.4034, 69.099998);
	}
#if VER_x64
	}
#endif

	CCamera::SetPosition(1093.0, -2036.0, 90.0, 0.0, 0.0, 0.0);
	CCamera::LookAtPoint(384.0, -1557.0, 20.0, 2);
	pGame->SetWorldWeather(1);
	pGame->DisplayHUD(false);
}
// 0.3.7
void CNetGame::ProcessConnecting()
{
	if (GetTickCount() - m_dwLastConnectAttempt > 1000/*3000*/)
	{
		//if (pUI) pUI->chat()->addDebugMessage("Connecting to %s:%d...", m_szHostOrIp, m_iPort);
		if (pUI) pUI->chat()->addDebugMessage("Connecting to SA-MP Server...");

		m_pRakClient->Connect(m_szHostOrIp, m_iPort, 0, 0, 2);
		
		// voice fix voice not connect when restart
		Network::OnRaknetConnect(m_szHostOrIp, m_iPort);

		m_dwLastConnectAttempt = GetTickCount();
		SetGameState(GAMESTATE_CONNECTING);
	}
}
// 0.3.7
void gen_auth_key(char buf[260], char* auth_in);
void CNetGame::Packet_AuthKey(Packet *pkt)
{
	uint8_t byteAuthLen;
	char szAuth[260], szAuthKey[269];

	RakNet::BitStream bsAuth((unsigned char*)pkt->data, pkt->length, false);

	if (GetGameState() < GAMESTATE_WAIT_CONNECT || GetGameState() > GAMESTATE_AWAIT_JOIN) return;

	bsAuth.IgnoreBits(8);
	if (!bsAuth.Read(byteAuthLen) || byteAuthLen == 0 || byteAuthLen >= sizeof(szAuth)) {
		FLog("[NET_STATE64] malformed-auth-key len=%u pktLen=%u host=%s:%d",
			 (unsigned)byteAuthLen,
			 pkt ? pkt->length : 0,
			 m_szHostOrIp,
			 m_iPort);
		if (m_pRakClient) {
			NetTransport::Disconnect(m_pRakClient, 0);
		}
		SetGameState(GAMESTATE_WAIT_CONNECT);
		m_dwLastConnectAttempt = 0;
		return;
	}
	if (!bsAuth.Read(szAuth, byteAuthLen)) {
		FLog("[NET_STATE64] truncated-auth-key len=%u pktLen=%u host=%s:%d",
			 (unsigned)byteAuthLen,
			 pkt ? pkt->length : 0,
			 m_szHostOrIp,
			 m_iPort);
		if (m_pRakClient) {
			NetTransport::Disconnect(m_pRakClient, 0);
		}
		SetGameState(GAMESTATE_WAIT_CONNECT);
		m_dwLastConnectAttempt = 0;
		return;
	}
	szAuth[byteAuthLen] = '\0';

	gen_auth_key(szAuthKey, szAuth);
	uint8_t byteAuthKeyLen = strlen(szAuthKey);

	RakNet::BitStream bsKey;
	bsKey.Write((uint8_t)ID_AUTH_KEY);
	bsKey.Write(byteAuthKeyLen);
	bsKey.Write(szAuthKey, byteAuthKeyLen);
	NetTransport::Send(m_pRakClient, &bsKey, SYSTEM_PRIORITY, RELIABLE, 0, "auth-key");
}
// 0.3.7
void CNetGame::Packet_ConnectAttemptFailed(Packet *pkt)
{
	if (pUI) pUI->chat()->addDebugMessage("The server didn't respond. Retrying..");
	if (pAudioStream) { //add new
		pAudioStream->Stop(true);
	}
	SpeakerList::Hide(); //add new
	MicroIcon::Hide();
	SetGameState(GAMESTATE_WAIT_CONNECT);

	//SpeakerList::Hide();
	//MicroIcon::Hide();
}
// 0.3.7
void CNetGame::Packet_NoFreeIncomingConnections(Packet *pkt)
{
	if(pUI) pUI->chat()->addDebugMessage("The server is full. Retrying...");
	SpeakerList::Hide(); //addnew
	MicroIcon::Hide();
	SetGameState(GAMESTATE_WAIT_CONNECT);

	//SpeakerList::Hide();
	//MicroIcon::Hide();
}
// 0.3.7
void CNetGame::Packet_DisconnectionNotification(Packet *pkt)
{
	if (pUI) pUI->chat()->addDebugMessage("Server closed the connection.");
	if (pAudioStream) {
		pAudioStream->Stop(true);
	}
    ResetRemovedBuildings();
	NetTransport::Disconnect(m_pRakClient, 2000);

	SpeakerList::Hide();
	MicroIcon::Hide();
}
// 0.3.7
void CNetGame::Packet_ConnectionSucceeded(Packet *pkt)
{
    ResetRemovedBuildings();
	RakNet::BitStream bsSuccAuth(pkt->data, pkt->length, true);
	PLAYERID MyPlayerID;
	unsigned int uiChallenge;
	int iVersion = 4057;
	uint8_t byteMod = 1;

	bsSuccAuth.IgnoreBits(8);	// packetId
	bsSuccAuth.IgnoreBits(32);	// binaryAddress
	bsSuccAuth.IgnoreBits(16);	// port

	if (!bsSuccAuth.Read(MyPlayerID) || !bsSuccAuth.Read(uiChallenge)) {
		FLog("[NET_STATE64] malformed-connection-accepted pktLen=%u host=%s:%d",
			 pkt ? pkt->length : 0,
			 m_szHostOrIp,
			 m_iPort);
		if (m_pRakClient) {
			NetTransport::Disconnect(m_pRakClient, 0);
		}
		SetGameState(GAMESTATE_WAIT_CONNECT);
		m_dwLastConnectAttempt = 0;
		return;
	}
	uiChallenge ^= iVersion;

	if (pUI) pUI->chat()->addDebugMessage("Connected. Joining the game...");
	FLog("[NET_JOIN64] accepted playerId=%u challenge=%u proto=%d mod=%u clientVersion=%s authLen=%u",
		 (unsigned)MyPlayerID,
		 uiChallenge,
		 iVersion,
		 byteMod,
		 (pSettings ? pSettings->Get().szVersion : SAMP_VERSION),
		 (unsigned)strlen(AUTH_BS));

	SetGameState(GAMESTATE_AWAIT_JOIN);

	uint8_t byteNameLen = strlen(GetPlayerPool()->GetLocalPlayerName());
	uint8_t byteAuthBSLen = strlen(AUTH_BS);
	uint8_t byteClientVerLen = strlen(SAMP_VERSION);
    if(pSettings)
        byteClientVerLen = strlen(pSettings->Get().szVersion);


	RakNet::BitStream bsSend;
	bsSend.Write(iVersion);
	bsSend.Write(byteMod);
	bsSend.Write(byteNameLen);
	bsSend.Write(GetPlayerPool()->GetLocalPlayerName(), byteNameLen);
	bsSend.Write(uiChallenge);
	bsSend.Write(byteAuthBSLen);
	bsSend.Write(AUTH_BS, byteAuthBSLen);
	bsSend.Write(byteClientVerLen);
	//bsSend.Write(SAMP_VERSION, byteClientVerLen);
    if(pSettings)
        bsSend.Write(pSettings->Get().szVersion, byteClientVerLen);
    else
        bsSend.Write(SAMP_VERSION, byteClientVerLen);

	Network::OnRaknetRpc(RPC_ClientJoin, bsSend);

	NetTransport::Rpc(m_pRakClient, &RPC_ClientJoin, &bsSend, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "client-join");

	// voice
	SpeakerList::Hide();
	MicroIcon::Hide();
}
// 0.3.7
void CNetGame::Packet_FailedInitializeEncription(Packet *pkt)
{
	if (pUI) pUI->chat()->addDebugMessage("Failed to initialize encryption.");
	FLog("[NET_STATE64] encryption-failed state=%d host=%s:%d",
		 GetGameState(),
		 m_szHostOrIp,
		 m_iPort);
	if (m_pRakClient) {
		NetTransport::Disconnect(m_pRakClient, 0);
	}
	SetGameState(GAMESTATE_WAIT_CONNECT);
	m_dwLastConnectAttempt = 0;
}
// 0.3.7
void CNetGame::Packet_ConnectionBanned(Packet *pkt)
{
	if (pUI) pUI->chat()->addDebugMessage("You are banned from this server.");
}
// 0.3.7
void CNetGame::Packet_InvalidPassword(Packet *pkt)
{
	if (pUI) pUI->chat()->addDebugMessage("Wrong server password.");
	FLog("[NET_STATE64] invalid-password state=%d host=%s:%d",
		 GetGameState(),
		 m_szHostOrIp,
		 m_iPort);
	if (m_pRakClient) {
		NetTransport::Disconnect(m_pRakClient, 0);
	}
	SetGameState(GAMESTATE_WAIT_CONNECT);
	m_dwLastConnectAttempt = 0;
}
// 0.3.7
void CNetGame::Packet_ConnectionLost(Packet *pkt)
{
	if (m_pRakClient) {
		NetTransport::Disconnect(m_pRakClient, 0);
	}

	if (pUI) pUI->chat()->addDebugMessage("Lost connection to the server. Reconnecting..");
	ShutdownForGameModeRestart();

	CPlayerPool *pPlayerPool = GetPlayerPool();
	if (pPlayerPool) {
		for (PLAYERID playerId = 0; playerId < MAX_PLAYERS; playerId++) {
			if (pPlayerPool->GetSlotState(playerId) == true) {
				pPlayerPool->Delete(playerId, 0);
			}
		}
	}

	if (pAudioStream) {
		pAudioStream->Stop(true);
	}

	SetGameState(GAMESTATE_WAIT_CONNECT);

	SpeakerList::Hide();
	MicroIcon::Hide();
}
// 0.3.7
void CNetGame::Packet_PlayerSync(Packet *pkt)
{
	RakNet::BitStream bsData(pkt->data, pkt->length, false);
	ONFOOT_SYNC_DATA ofSync;
	uint32_t dwTime = 0;
	uint8_t bytePacketId;
	PLAYERID playerId;

	bool bHasLR, bHasUD;

	if (GetGameState() != GAMESTATE_CONNECTED) return;

	memset(&ofSync, 0, sizeof(ONFOOT_SYNC_DATA));

	bsData.Read(bytePacketId);
	bsData.Read(playerId);

	bsData.Read(bHasLR);
	if (bHasLR) {
		bsData.Read(ofSync.lrAnalog);
	}

	bsData.Read(bHasUD);
	if (bHasUD) {
		bsData.Read(ofSync.udAnalog);
	}

	bsData.Read(ofSync.wKeys);
	bsData.Read((char*)&ofSync.vecPos, sizeof(CVector));
	float w, x, y, z;
	bsData.ReadNormQuat(w, x, y, z);
	ofSync.quat.Set(x, y, z, w);

	uint8_t byteHealthArmour;
	uint8_t byteArmTemp = 0, byteHlTemp = 0;
	
	bsData.Read(byteHealthArmour);
	byteArmTemp = (byteHealthArmour & 0x0F);
	byteHlTemp = (byteHealthArmour >> 4);

	if (byteArmTemp == 0xF) ofSync.byteArmour = 100;
	else if (byteArmTemp == 0) ofSync.byteArmour = 0;
	else ofSync.byteArmour = byteArmTemp * 7;

	if (byteHlTemp == 0xF) ofSync.byteHealth = 100;
	else if (byteHlTemp == 0) ofSync.byteHealth = 0;
	else ofSync.byteHealth = byteHlTemp * 7;

	uint8_t byteCurrentWeapon = 0;
	bsData.Read(byteCurrentWeapon);
	ofSync.byteCurrentWeapon ^= (byteCurrentWeapon ^ ofSync.byteCurrentWeapon) & 0x3F;

	bsData.Read(ofSync.byteSpecialAction);
	bsData.ReadVector(ofSync.vecMoveSpeed.x, ofSync.vecMoveSpeed.y, ofSync.vecMoveSpeed.z);

	bool bHasVehicleSurfingInfo;
	bsData.Read(bHasVehicleSurfingInfo);
	if (bHasVehicleSurfingInfo)
	{
		bsData.Read(ofSync.wSurfID);
		bsData.Read(ofSync.vecSurfOffsets.x);
		bsData.Read(ofSync.vecSurfOffsets.y);
		bsData.Read(ofSync.vecSurfOffsets.z);
	}
	else
		ofSync.wSurfID = INVALID_VEHICLE_ID;

	bool bHasAnimation;
	bsData.Read(bHasAnimation);
	if (bHasAnimation) {
		bsData.Read(ofSync.dwAnimation);
	}
	else {
		ofSync.dwAnimation = 0x80000000;
	}

	CRemotePlayer *pRemotePlayer = GetPlayerPool()->GetAt(playerId);
	if (pRemotePlayer) {
		pRemotePlayer->StoreOnFootFullSyncData(&ofSync, 0);
	}
}
// 0.3.7
void CNetGame::Packet_VehicleSync(Packet* pkt)
{
	RakNet::BitStream bsData(pkt->data, pkt->length, false);
	uint32_t dwTime = 0;
	uint8_t bytePacketId;
	INCAR_SYNC_DATA icSync;
	PLAYERID PlayerID;

	if (GetGameState() != GAMESTATE_CONNECTED) return;

	memset(&icSync, 0, sizeof(INCAR_SYNC_DATA));

	bsData.Read(bytePacketId);
	bsData.Read(PlayerID);
	bsData.Read(icSync.VehicleID);
	bsData.Read(icSync.lrAnalog);
	bsData.Read(icSync.udAnalog);
	bsData.Read(icSync.wKeys);

	float w, x, y, z;
	bsData.ReadNormQuat(w, x, y, z);
	icSync.quat.Set(x, y, z, w);
	
	bsData.Read((char*)& icSync.vecPos, sizeof(CVector));
	bsData.ReadVector(icSync.vecMoveSpeed.x, icSync.vecMoveSpeed.y, icSync.vecMoveSpeed.z);

	// car health
	uint16_t wTempVehicleHealth;
	bsData.Read(wTempVehicleHealth);
	icSync.fCarHealth = (float)wTempVehicleHealth;

	// health/armour
	uint8_t byteHealthArmour;
	uint8_t byteArmTemp = 0, byteHlTemp = 0;

	bsData.Read(byteHealthArmour);
	byteArmTemp = (byteHealthArmour & 0x0F);
	byteHlTemp = (byteHealthArmour >> 4);

	if (byteArmTemp == 0xF) icSync.bytePlayerArmour = 100;
	else if (byteArmTemp == 0) icSync.bytePlayerArmour = 0;
	else icSync.bytePlayerArmour = byteArmTemp * 7;

	if (byteHlTemp == 0xF) icSync.bytePlayerHealth = 100;
	else if (byteHlTemp == 0) icSync.bytePlayerHealth = 0;
	else icSync.bytePlayerHealth = byteHlTemp * 7;

	// current weapon
	uint8_t byteTempWeapon;
	bsData.Read(byteTempWeapon);
	icSync.byteCurrentWeapon ^= (byteTempWeapon ^ icSync.byteCurrentWeapon) & 0x3F;

	bool bCheck;

	// siren
	bsData.Read(bCheck);
	if (bCheck) icSync.byteSirenOn = 1;
	// landinggear
	bsData.Read(bCheck);
	if (bCheck) icSync.byteLandingGearState = 1;
	// train speed
	bsData.Read(bCheck);
	if (bCheck) bsData.Read(icSync.fTrainSpeed);
	// triler id
	bsData.Read(bCheck);
	if (bCheck) bsData.Read(icSync.TrailerID);

	CRemotePlayer* pRemotePlayer = GetPlayerPool()->GetAt(PlayerID);
	if (pRemotePlayer) {
		pRemotePlayer->StoreInCarFullSyncData(&icSync, 0);
	}
}
// 0.3.7
void CNetGame::Packet_AimSync(Packet* pkt)
{
	RakNet::BitStream bsData(pkt->data, pkt->length, false);
	
	uint8_t bytePacketId;
	PLAYERID PlayerId;
	AIM_SYNC_DATA aimSync;
	bsData.Read(bytePacketId);
	bsData.Read(PlayerId);
	bsData.Read((char*)& aimSync, sizeof(AIM_SYNC_DATA));

	CRemotePlayer* pPlayer = m_pPools->pPlayerPool->GetAt(PlayerId);
	if (pPlayer)
		pPlayer->StoreAimFullSyncData(&aimSync);
}
// 0.3.7
void CNetGame::Packet_BulletSync(Packet* pkt)
{
	RakNet::BitStream bsData(pkt->data, pkt->length, false);

	if (GetGameState() != GAMESTATE_CONNECTED) return;

	BULLET_SYNC_DATA btSync;
	uint8_t bytePacketId = 0;
	PLAYERID PlayerID = 0;

	bsData.Read(bytePacketId);
	bsData.Read(PlayerID);
	bsData.Read((char*)&btSync, sizeof(BULLET_SYNC_DATA));

	CPlayerPool* pPlayerPool = GetPlayerPool();
	CRemotePlayer* pRemotePlayer = pPlayerPool->GetAt(PlayerID);
	if (pRemotePlayer)
		pRemotePlayer->StoreBulletFullSyncData(&btSync);

}
// 0.3.7
void CNetGame::Packet_PassengerSync(Packet* pkt)
{
	RakNet::BitStream bsData(pkt->data, pkt->length, false);

	if (GetGameState() == GAMESTATE_CONNECTED)
	{
		uint8_t bytePacketId;
		PLAYERID PlayerID;
		PASSENGER_SYNC_DATA psSync;

		bsData.Read(bytePacketId);
		bsData.Read(PlayerID);
		bsData.Read((char*)& psSync, sizeof(PASSENGER_SYNC_DATA));

		CRemotePlayer* pRemotePlayer = GetPlayerPool()->GetAt(PlayerID);
		if (pRemotePlayer) {
			pRemotePlayer->StorePassengerFullSyncData(&psSync);
		}
	}
}

// 0.3.7
void CNetGame::Packet_MarkerSync(Packet *pkt)
{
	if(m_iGameState != GAMESTATE_CONNECTED) return;

	uint8_t bytePacketId;
	int	iNumberOfPlayers;
	PLAYERID playerId;
	bool bIsPlayerActive;
	short sPos[3];

	RakNet::BitStream bsMarkersSync((unsigned char *)pkt->data, pkt->length, false);
	bsMarkersSync.Read(bytePacketId);
	bsMarkersSync.Read(iNumberOfPlayers);
	if(iNumberOfPlayers)
	{
		for(int i = 0; i < iNumberOfPlayers; i++)
		{
			bsMarkersSync.Read(playerId);
			bsMarkersSync.ReadCompressed(bIsPlayerActive);

			if(bIsPlayerActive)
			{
				bsMarkersSync.Read(sPos[0]);
				bsMarkersSync.Read(sPos[1]);
				bsMarkersSync.Read(sPos[2]);
			}

			if(playerId >= 0 && playerId < MAX_PLAYERS && GetPlayerPool()->GetSlotState(playerId))
			{
				CRemotePlayer *pPlayer = GetPlayerPool()->GetAt(playerId);
				if(pPlayer)
				{
					if(bIsPlayerActive)
						pPlayer->ShowGlobalMarker(sPos[0], sPos[1], sPos[2]);
					else
						pPlayer->HideGlobalMarker();
				}
			}
		}
	}
}

// 0.3.7
void CNetGame::UpdatePlayerScoresAndPings()
{
	static uint32_t dwLastUpdateTick = 0;

	if (GetTickCount() - dwLastUpdateTick > 3000) {
		dwLastUpdateTick = GetTickCount();

		RakNet::BitStream bsSend;
		NetTransport::Rpc(m_pRakClient, &RPC_UpdateScoresPingsIPs, &bsSend, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "scores-pings");
	}
}

void CNetGame::SendDialogResponse(uint16_t wDialogID, uint8_t byteButtonID, uint16_t wListBoxItem, const char* szInput)
{
	if (GetGameState() != GAMESTATE_CONNECTED) return;

	uint8_t length = strlen(szInput);

	RakNet::BitStream bsSend;
	bsSend.Write(wDialogID);
	bsSend.Write(byteButtonID);
	bsSend.Write(wListBoxItem);
	bsSend.Write(length);
	bsSend.Write(szInput, length);
	NetTransport::Rpc(m_pRakClient, &RPC_DialogResponse, &bsSend, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "dialog-response");
}

void CNetGame::SendChatMessage(const char* szMsg)
{
	if (GetGameState() != GAMESTATE_CONNECTED) return;

	RakNet::BitStream bsSend;
	uint8_t byteTextLen = strlen(szMsg);

	bsSend.Write(byteTextLen);
	bsSend.Write(szMsg, byteTextLen);

	NetTransport::Rpc(m_pRakClient, &RPC_Chat, &bsSend, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, NULL, "chat");
}

void CNetGame::SendChatCommand(const char* szCommand)
{
	if (GetGameState() != GAMESTATE_CONNECTED) return;

	RakNet::BitStream bsParams;
	int iStrlen = strlen(szCommand);

	bsParams.Write(iStrlen);
	bsParams.Write(szCommand, iStrlen);
	NetTransport::Rpc(m_pRakClient, &RPC_ServerCommand, &bsParams, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, NULL, "server-command");
}
// 0.3.7
void CNetGame::SetMapIcon(uint8_t byteIconID, float fPosX, float fPosY, float fPosZ, uint8_t byteType, uint32_t dwColor, uint8_t byteStyle)
{
	if (m_dwMapIcon[byteIconID] != 0) {
		DisableMapIcon(byteIconID);
	}

	m_dwMapIcon[byteIconID] = pGame->CreateRadarMarkerIcon(byteType, fPosX, fPosY, fPosZ, dwColor, byteStyle);
}
// 0.3.7
void CNetGame::DisableMapIcon(uint8_t byteIconID)
{
	ScriptCommand(&disable_marker, m_dwMapIcon[byteIconID]);
	m_dwMapIcon[byteIconID] = 0;
}
// 0.3.7
void CNetGame::ResetVehiclePool()
{
	if (m_pPools->pVehiclePool) {
		delete m_pPools->pVehiclePool;
	}

	m_pPools->pVehiclePool = new CVehiclePool();
}
// 0.3.7
void CNetGame::ResetActorPool()
{
	if (m_pPools->pActorPool) {
		delete m_pPools->pActorPool;
	}

	m_pPools->pActorPool = new CActorPool();
}
// 0.3.7
void CNetGame::ResetTextDrawPool()
{
	if (m_pPools->pTextDrawPool) {
		delete m_pPools->pTextDrawPool;
	}

	m_pPools->pTextDrawPool = new CTextDrawPool();
}
// 0.3.7
void CNetGame::ResetGangZonePool()
{
	if (m_pPools->pGangZonePool) {
		delete m_pPools->pGangZonePool;
	}

	m_pPools->pGangZonePool = new CGangZonePool();
}
// 0.3.7
void CNetGame::Reset3DTextLabelPool()
{
	if (m_pPools->pTextLabelPool) {
		delete m_pPools->pTextLabelPool;
	}

	m_pPools->pTextLabelPool = new C3DTextLabelPool();
}
// 0.3.7
void CNetGame::ResetMapIcons()
{
	for (int i = 0; i < MAX_MAP_ICONS; i++)
	{
		if (m_dwMapIcon[i]) {
			ScriptCommand(&disable_marker, m_dwMapIcon[i]);
			m_dwMapIcon[i] = 0;
		}
	}
}
// 0.3.7
void CNetGame::ResetPickupPool()
{
	if (m_pPools->pPickupPool) {
		delete m_pPools->pPickupPool;
	}

	m_pPools->pPickupPool = new CPickupPool();
}
// 0.3.7
void CNetGame::ResetObjectPool()
{
	if (m_pPools->pObjectPool) {
		delete m_pPools->pObjectPool;
	}

	m_pPools->pObjectPool = new CObjectPool();
}
// 0.3.7
void CNetGame::ResetMenuPool()
{
	if (m_pPools->pMenuPool) {
		delete m_pPools->pMenuPool;
	}

	m_pPools->pMenuPool = new CMenuPool();
}
// 0.3.7
void CNetGame::InitGameLogic()
{
	if (m_pNetSet->bManualVehicleEngineAndLight) {
		//InstallVehicleEngineLightPatches();
	}

	m_pNetSet->fWorldBounds[0] = 20000.0f;
	m_pNetSet->fWorldBounds[1] = -20000.0f;
	m_pNetSet->fWorldBounds[2] = 20000.0f;
	m_pNetSet->fWorldBounds[3] = -20000.0f;
}

void CNetGame::SendPrevClass()
{
	CPlayerPool* pPlayerPool = GetPlayerPool();
	if (pPlayerPool)
	{
		pPlayerPool->GetLocalPlayer()->SendPrevClass();
	}
}

void CNetGame::SendSpawn()
{
	CPlayerPool* pPlayerPool = GetPlayerPool();
	if (pPlayerPool)
	{
		pPlayerPool->GetLocalPlayer()->SendSpawn();
	}
}

void CNetGame::SendNextClass()
{
	CPlayerPool* pPlayerPool = GetPlayerPool();
	if (pPlayerPool)
	{
		pPlayerPool->GetLocalPlayer()->SendNextClass();
	}
}

void CNetGame::Packet_TrailerSync(Packet *pkt)
{
	if(GetGameState() != GAMESTATE_CONNECTED) return;
	
	uint8_t bytePacketId;
	PLAYERID playerId;

	TRAILER_SYNC_DATA trSync;
	memset(&trSync, 0, sizeof(TRAILER_SYNC_DATA));

	RakNet::BitStream bsTrailerSync((unsigned char *)pkt->data, pkt->length, false);
	bsTrailerSync.Read(bytePacketId);
	bsTrailerSync.Read(playerId);
	bsTrailerSync.Read((char*)&trSync, sizeof(TRAILER_SYNC_DATA));

	if(GetPlayerPool()) 
	{
		CRemotePlayer *pPlayer = GetPlayerPool()->GetAt(playerId);
		if(pPlayer)
			pPlayer->StoreTrailerFullSyncData(&trSync);
	}
}

void CNetGame::Packet_UnoccupiedSync(Packet *pkt)
{
	if(m_iGameState != GAMESTATE_CONNECTED) return;

	uint8_t bytePacketId;
	PLAYERID playerId;

	UNOCCUPIED_SYNC_DATA unocSync;
	memset(&unocSync, 0, sizeof(UNOCCUPIED_SYNC_DATA));

	RakNet::BitStream bsUnocSync((unsigned char *)pkt->data, pkt->length, false);
	bsUnocSync.Read(bytePacketId);
	bsUnocSync.Read(playerId);
	bsUnocSync.Read((char*)&unocSync,sizeof(UNOCCUPIED_SYNC_DATA));

	if(GetPlayerPool())
	{
		CRemotePlayer *pPlayer = GetPlayerPool()->GetAt(playerId);
		if(pPlayer)
			pPlayer->StoreUnoccupiedSyncData(&unocSync);
	}
}
