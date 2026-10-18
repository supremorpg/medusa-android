#include <jni.h>
#include <pthread.h>
#include <cmath>
#include <syscall.h>

#include "main.h"
#include "game/game.h"
#include "game/GTASAEngineApi.h"
#include "game/Timer.h"
#include "net/netgame.h"
#include "gui/gui.h"
#include "gui/uisettings.h"
#include "playertags.h"
#include "audiostream.h"
#include "java/jniutil.h"
#include <dlfcn.h>
#include "StackTrace.h"

// voice
#include "voice_new/Plugin.h"

#include "vendor/armhook/patch.h"
#include "vendor/str_obfuscator/str_obfuscator.hpp"

#include "settings.h"

#include "crashlytics.h"
#include "core/SampCoreDOD.h"
#include "core/SampRenderHook.h"
#include "perf/PerformanceProfiler.h"

/*
Peerapol Unarak
*/

JavaVM* javaVM;

char* g_pszStorage = nullptr;

UI* pUI = nullptr;
CGame *pGame = nullptr;

CNetGame *pNetGame = nullptr;
CPlayerTags* pPlayerTags = nullptr;
CSnapShotHelper* pSnapShotHelper = nullptr;
CAudioStream* pAudioStream = nullptr;
CJavaWrapper* pJavaWrapper = nullptr;
CSettings* pSettings = nullptr;

bool g_bHudVisible = false;
bool g_bDialogVisible = false;
bool g_bHudMapEnabled = true;

MaterialTextGenerator* pMaterialTextGenerator = nullptr;

bool bDebug = false;
bool bGameInited = false;
bool bNetworkInited = false;

uintptr_t g_libGTASA = 0x00;
uintptr_t g_libSAMP = 0x00;

void ApplyGlobalPatches();
void ApplyPatches_level0();
void ApplyMultiTouchPatches();
void InstallGlobalHooks();
void InstallSpecialHooks();
void InitRenderWareFunctions();
void FLog(const char* fmt, ...);
//void MyLog(const char* fmt, ...);

int work = 0;

bool Mchat = false;

extern "C" void XyronNativeFrameTick();
extern "C" void XyronApplyStreamingRuntimeProfile(int renderDistance, bool effectsEnabled);
extern "C" void XyronRequestVehicleRenderRefresh64();

static bool IsWeaponIdValidForWheel(uint32_t weaponId)
{
	return weaponId <= 46;
}

static std::string BuildWeaponWheelPayload(CPlayerPed* pPlayerPed)
{
	if (!pPlayerPed || !pPlayerPed->m_pPed)
	{
		return "[{\"id\":0,\"ammo\":0,\"current\":true}]";
	}

	uint8_t currentWeapon = pPlayerPed->GetCurrentWeapon();
	bool seen[47] = {};
	std::ostringstream stream;
	stream << "[";

	auto appendWeapon = [&](uint32_t weaponId, uint32_t ammo, bool current) {
		if (!IsWeaponIdValidForWheel(weaponId) || seen[weaponId])
		{
			return;
		}
		if (seen[0] || weaponId != 0)
		{
			stream << ",";
		}
		stream << "{\"id\":" << weaponId
			   << ",\"ammo\":" << ammo
			   << ",\"current\":" << (current ? "true" : "false")
			   << "}";
		seen[weaponId] = true;
	};

	appendWeapon(0, 0, currentWeapon == 0);

	for (int i = 0; i < 13; ++i)
	{
		CWeapon& slot = pPlayerPed->m_pPed->m_aWeapons[i];
		uint32_t weaponId = slot.dwType;
		if (weaponId == 0 || !IsWeaponIdValidForWheel(weaponId))
		{
			continue;
		}
		appendWeapon(weaponId, slot.dwAmmo, currentWeapon == weaponId);
	}

	stream << "]";
	return stream.str();
}

static void UpdateWeaponWheelSnapshot(CPlayerPed* pPlayerPed)
{
	if (!pJavaWrapper)
	{
		return;
	}

	static std::string s_lastWeaponWheelPayload;
	std::string payload = BuildWeaponWheelPayload(pPlayerPed);
	if (payload == s_lastWeaponWheelPayload)
	{
		return;
	}

	s_lastWeaponWheelPayload = payload;
	pJavaWrapper->UpdateWeaponWheel(s_lastWeaponWheelPayload.c_str());
}

static CLocalPlayer* GetLocalPlayerForHud()
{
	if (!pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED)
	{
		return nullptr;
	}

	CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
	return pPlayerPool ? pPlayerPool->GetLocalPlayer() : nullptr;
}

static bool IsLocalPlayerHudReady()
{
	CLocalPlayer* pLocalPlayer = GetLocalPlayerForHud();
	if (!pLocalPlayer || !pLocalPlayer->m_bIsActive || pLocalPlayer->IsSpectating() ||
		pLocalPlayer->m_bWaitingForSpawnRequestReply)
	{
		return false;
	}

	CPlayerPed* pPlayerPed = pLocalPlayer->GetPlayerPed();
	return pPlayerPed && pPlayerPed->m_pPed && !pPlayerPed->IsDead();
}

struct SampChatCfgPtrs
{
	float* posX;
	float* posY;
	float* sizeX;
	float* sizeY;
	int* maxMsgs;
};

struct SampChatScaleInfo
{
	float scaleX;
	float scaleY;
	bool originBottom;
};

static SampChatCfgPtrs g_sampChatCfg = { nullptr, nullptr, nullptr, nullptr, nullptr };
static bool g_sampChatCfgReady = false;
static bool g_sampChatCfgTried = false;
static SampChatScaleInfo g_sampChatScale = { 1.0f, 1.0f, false };
static pthread_mutex_t g_sampChatAreaMutex = PTHREAD_MUTEX_INITIALIZER;

struct SampChatAreaRuntime
{
	bool enabled;
	int left;
	int top;
	int width;
	int height;
	int screenWidth;
	int screenHeight;
};

static SampChatAreaRuntime g_sampChatAreaRuntime = { false, 0, 0, 0, 0, 0, 0 };

static int ClampIntValue(int value, int minValue, int maxValue)
{
	if (value < minValue) {
		return minValue;
	}
	if (value > maxValue) {
		return maxValue;
	}
	return value;
}

static float ClampFloatValue(float value, float minValue, float maxValue)
{
	if (value < minValue) {
		return minValue;
	}
	if (value > maxValue) {
		return maxValue;
	}
	return value;
}

static int GetRuntimeChatMaxMessages()
{
	return ClampIntValue(pSettings ? pSettings->Get().iChatMaxMessages : 6, 1, 30);
}

static bool FloatEqRel(float a, float b, float absEps = 1.0f, float relEps = 0.02f)
{
	const float diff = fabsf(a - b);
	const float maxAbs = fmaxf(fabsf(a), fabsf(b));
	return diff <= fmaxf(absEps, relEps * maxAbs);
}

static bool IsPlausiblePos(float x, float y, float dispW, float dispH)
{
	if (!std::isfinite(x) || !std::isfinite(y)) {
		return false;
	}
	return x >= 0.0f && x <= dispW && y >= 0.0f && y <= dispH;
}

static bool ScanRangeForChatBySize(uintptr_t start, uintptr_t end, float sizeX, float sizeY,
	float dispW, float dispH, SampChatCfgPtrs* out)
{
	for (uintptr_t addr = start; addr + sizeof(float) * 2 <= end; addr += 4)
	{
		float* f = reinterpret_cast<float*>(addr);
		if (!FloatEqRel(f[0], sizeX, 6.0f, 0.08f) || !FloatEqRel(f[1], sizeY, 6.0f, 0.08f)) {
			continue;
		}

		for (int off = -32; off <= 32; off += 4)
		{
			uintptr_t paddr = addr + off;
			if (paddr < start || paddr + sizeof(float) * 2 > end) {
				continue;
			}

			float* p = reinterpret_cast<float*>(paddr);
			float px = p[0];
			float py = p[1];
			if (!IsPlausiblePos(px, py, dispW, dispH)) {
				continue;
			}

			out->posX = &p[0];
			out->posY = &p[1];
			out->sizeX = &f[0];
			out->sizeY = &f[1];
			out->maxMsgs = nullptr;
			return true;
		}
	}

	return false;
}

static bool FindSampChatConfig()
{
	const float dispW = RsGlobal ? RsGlobal->maximumWidth : 1920.0f;
	const float dispH = RsGlobal ? RsGlobal->maximumHeight : 1080.0f;
	const float baseW = 640.0f;
	const float baseH = 480.0f;
	const float sx = dispW / baseW;
	const float sy = dispH / baseH;

	const float sizeXBase = pSettings ? pSettings->Get().fChatSizeX : 1150.0f;
	const float sizeYBase = pSettings ? pSettings->Get().fChatSizeY : 220.0f;
	const float sizeXScaled = sizeXBase * sx;
	const float sizeYScaled = sizeYBase * sy;

	const float sizeCandidates[4][2] = {
		{ sizeXBase, sizeYBase },
		{ sizeXScaled, sizeYScaled },
		{ sizeYBase, sizeXBase },
		{ sizeYScaled, sizeXScaled }
	};

	FILE* fp = fopen("/proc/self/maps", "rt");
	if (!fp) {
		return false;
	}

	char line[512];
	while (fgets(line, sizeof(line), fp))
	{
		uintptr_t start = 0;
		uintptr_t end = 0;
		char perms[5] = { 0 };

		if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) != 3) {
			continue;
		}

		if (!(perms[0] == 'r' && perms[1] == 'w')) {
			continue;
		}

		if (!strstr(line, "libsamp.so") &&
			!strstr(line, "libGTASA.so") &&
			!strstr(line, "[anon]") &&
			!strstr(line, "[heap]"))
		{
			continue;
		}

		if (end <= start || (end - start) > (64 * 1024 * 1024)) {
			continue;
		}

		for (int i = 0; i < 4; ++i)
		{
			if (ScanRangeForChatBySize(start, end, sizeCandidates[i][0], sizeCandidates[i][1],
				dispW, dispH, &g_sampChatCfg))
			{
				const float curSizeX = *g_sampChatCfg.sizeX;
				const float curSizeY = *g_sampChatCfg.sizeY;

				g_sampChatScale.scaleX = (fabsf(curSizeX - sizeXScaled) < fabsf(curSizeX - sizeXBase)) ? sx : 1.0f;
				g_sampChatScale.scaleY = (fabsf(curSizeY - sizeYScaled) < fabsf(curSizeY - sizeYBase)) ? sy : 1.0f;

				const float refH = (g_sampChatScale.scaleY > 1.1f) ? dispH : baseH;
				const float margin = 10.0f * ((g_sampChatScale.scaleY > 1.1f) ? g_sampChatScale.scaleY : 1.0f);
				const float topY = margin;
				const float bottomY = refH - curSizeY - margin;
				const float curY = *g_sampChatCfg.posY;

				g_sampChatScale.originBottom = fabsf(curY - bottomY) < fabsf(curY - topY);

				fclose(fp);
				FLog("SAMP chat config found at %p (scaleX=%.3f scaleY=%.3f bottom=%d)",
					g_sampChatCfg.posX, g_sampChatScale.scaleX, g_sampChatScale.scaleY, g_sampChatScale.originBottom ? 1 : 0);
				return true;
			}
		}
	}

	fclose(fp);
	return false;
}

static bool IsRadarVisible()
{
	if (!g_bHudVisible) {
		return false;
	}
	if (!g_bHudMapEnabled) {
		return false;
	}
	if (g_bDialogVisible) {
		return false;
	}
	if (pNetGame) {
		CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();
		if (pTextDrawPool && pTextDrawPool->GetState()) {
			return false;
		}
	}
	return true;
}

static bool ReadSampChatAreaRuntime(SampChatAreaRuntime* out)
{
	if (!out) {
		return false;
	}

	pthread_mutex_lock(&g_sampChatAreaMutex);
	*out = g_sampChatAreaRuntime;
	pthread_mutex_unlock(&g_sampChatAreaMutex);

	return out->enabled;
}

static void WriteSampChatFloat(float* target, float value)
{
	if (!target) {
		return;
	}

	CHook::UnFuck(reinterpret_cast<uintptr_t>(target), sizeof(float));
	*target = value;
}

static void ApplySampChatWidgetArea(float x, float y, float width, float height)
{
	if (!pUI || !pUI->chat()) {
		return;
	}

	const float itemHeight = fmaxf(1.0f, UISettings::chatItemSize().y);
	const ImVec2 size(width, height);
	pUI->chat()->setFixedSize(size);
	pUI->chat()->setItemSize(ImVec2(width, itemHeight));
	pUI->chat()->setPosition(ImVec2(x, y));
	pUI->chat()->performLayout();
}

static bool ApplySampChatAreaOverride()
{
	SampChatAreaRuntime runtime;
	if (!ReadSampChatAreaRuntime(&runtime)) {
		return false;
	}

	const float fallbackW = runtime.screenWidth > 0 ? static_cast<float>(runtime.screenWidth) : 640.0f;
	const float fallbackH = runtime.screenHeight > 0 ? static_cast<float>(runtime.screenHeight) : 480.0f;
	const float dispW = RsGlobal ? RsGlobal->maximumWidth : fallbackW;
	const float dispH = RsGlobal ? RsGlobal->maximumHeight : fallbackH;
	const float scaleX = runtime.screenWidth > 0 ? dispW / static_cast<float>(runtime.screenWidth) : 1.0f;
	const float scaleY = runtime.screenHeight > 0 ? dispH / static_cast<float>(runtime.screenHeight) : 1.0f;
	const float minWidth = fmaxf(48.0f * scaleX, 1.0f);
	const float minHeight = fmaxf(UISettings::chatItemSize().y, 1.0f);

	float width = fmaxf(static_cast<float>(runtime.width) * scaleX, minWidth);
	float height = fmaxf(static_cast<float>(runtime.height) * scaleY, minHeight);
	width = ClampFloatValue(width, minWidth, dispW);
	height = ClampFloatValue(height, minHeight, dispH);

	float x = static_cast<float>(runtime.left) * scaleX;
	float y = static_cast<float>(runtime.top) * scaleY;
	x = ClampFloatValue(x, 0.0f, fmaxf(0.0f, dispW - width));
	y = ClampFloatValue(y, 0.0f, fmaxf(0.0f, dispH - height));

	if (pSettings) {
		pSettings->Get().fChatPosX = x;
		pSettings->Get().fChatPosY = y;
		pSettings->Get().fChatSizeX = width;
		pSettings->Get().fChatSizeY = height;
	}

	ApplySampChatWidgetArea(x, y, width, height);

	if (g_sampChatCfgReady) {
		WriteSampChatFloat(g_sampChatCfg.posX, x);
		WriteSampChatFloat(g_sampChatCfg.posY, y);
		WriteSampChatFloat(g_sampChatCfg.sizeX, width);
		WriteSampChatFloat(g_sampChatCfg.sizeY, height);
	}

	return true;
}

static void ApplySampChatLineRuntime()
{
	if (!pUI || !pUI->chat()) {
		return;
	}

	SampChatAreaRuntime runtime;
	if (ReadSampChatAreaRuntime(&runtime)) {
		ApplySampChatAreaOverride();
		return;
	}

	const float itemHeight = fmaxf(1.0f, UISettings::chatItemSize().y);
	const float width = fmaxf(pUI->chat()->width(), UISettings::chatSize().x);
	const float height = itemHeight * static_cast<float>(GetRuntimeChatMaxMessages());
	ApplySampChatWidgetArea(pUI->chat()->position().x, pUI->chat()->position().y, width, height);
	if (g_sampChatCfgReady) {
		WriteSampChatFloat(g_sampChatCfg.sizeY, height);
	}
}

static void UpdateSampChatPos(bool radarVisible)
{
	if (!g_sampChatCfgReady) {
		return;
	}

	if (ApplySampChatAreaOverride()) {
		return;
	}

	const float baseW = 640.0f;
	const float baseH = 480.0f;
	const float dispW = RsGlobal ? RsGlobal->maximumWidth : baseW * g_sampChatScale.scaleX;
	const float dispH = RsGlobal ? RsGlobal->maximumHeight : baseH * g_sampChatScale.scaleY;
	const bool useDisplayCoords = (g_sampChatScale.scaleX < 1.2f && g_sampChatScale.scaleY < 1.2f);
	const float refW = useDisplayCoords ? dispW : baseW;
	const float refH = useDisplayCoords ? dispH : baseH;
	const float scaleX = useDisplayCoords ? 1.0f : g_sampChatScale.scaleX;
	const float scaleY = useDisplayCoords ? 1.0f : g_sampChatScale.scaleY;
	const float sizeY = g_sampChatCfg.sizeY ? *g_sampChatCfg.sizeY
											: (pSettings ? pSettings->Get().fChatSizeY : 220.0f);
	const float radarSize = refW * 0.19f;
	const float marginY = 10.0f;
	const float marginXNoRadar = 10.0f;
	const float radarOffset = -refW * 0.03f;

	float posXBase = radarVisible ? (radarSize + radarOffset) : marginXNoRadar;
	float posYBase = marginY;
	if (g_sampChatScale.originBottom) {
		posYBase = refH - sizeY - marginY;
	}

	const float posX = posXBase * scaleX;
	const float posY = posYBase * scaleY;

	CHook::UnFuck(reinterpret_cast<uintptr_t>(g_sampChatCfg.posX), sizeof(float) * 2);
	*g_sampChatCfg.posX = posX;
	*g_sampChatCfg.posY = posY;
}

extern "C" void XyronApplyNativeChatArea(
	int left,
	int top,
	int width,
	int height,
	int screenWidth,
	int screenHeight)
{
	if (width <= 0 || height <= 0 || screenWidth <= 0 || screenHeight <= 0) {
		pthread_mutex_lock(&g_sampChatAreaMutex);
		g_sampChatAreaRuntime.enabled = false;
		pthread_mutex_unlock(&g_sampChatAreaMutex);
		ApplySampChatLineRuntime();
		return;
	}

	SampChatAreaRuntime runtime;
	runtime.enabled = true;
	runtime.left = ClampIntValue(left, 0, screenWidth);
	runtime.top = ClampIntValue(top, 0, screenHeight);
	runtime.width = ClampIntValue(width, 1, screenWidth);
	runtime.height = ClampIntValue(height, 1, screenHeight);
	runtime.screenWidth = screenWidth;
	runtime.screenHeight = screenHeight;

	pthread_mutex_lock(&g_sampChatAreaMutex);
	g_sampChatAreaRuntime = runtime;
	pthread_mutex_unlock(&g_sampChatAreaMutex);

	if (!g_sampChatCfgReady && !g_sampChatCfgTried) {
		g_sampChatCfgReady = FindSampChatConfig();
		g_sampChatCfgTried = true;
	}
	ApplySampChatAreaOverride();
}

extern "C" void XyronApplyNativeMenuRuntimeSettings(int chatMaxMessages)
{
	if (pSettings) {
		pSettings->Get().iChatMaxMessages = ClampIntValue(chatMaxMessages, 1, 30);
	}
	ApplySampChatLineRuntime();
}

extern "C" void XyronApplyNativeRenderRuntimeSettings(
	int renderDistance,
	bool,
	bool effectsEnabled)
{
	const float safeRenderDistance = static_cast<float>(ClampIntValue(renderDistance, 20, 300));
	if (pNetGame && pNetGame->m_pNetSet) {
		pNetGame->m_pNetSet->fNameTagDrawDistance = safeRenderDistance;
	}
	XyronApplyStreamingRuntimeProfile(ClampIntValue(renderDistance, 35, 220), effectsEnabled);
	XyronRequestVehicleRenderRefresh64();
}

void ReadSettingFile()
{
	/*char path[255] = { 0 };
	//sprintf(path, "%ssamp.set", g_pszStorage);
	sprintf(path, "%sNickName.ini", g_pszStorage);

	FILE* fp = fopen(path, "r");
	if (fp == NULL) return;

	char buf[1024];

	// nickname
	if (fgets(buf, 1024, fp) != NULL) {
		buf[strcspn(buf, "\n\r")] = 0;
		strcpy(g_nick, buf);
	}

	fclose(fp);*/

	if (pSettings) {
		return;
	}
	if (!g_pszStorage) {
		FLog("Storage path unavailable; settings load deferred.");
		return;
	}

	pSettings = new CSettings();
	firebase::crashlytics::SetUserId(pSettings->Get().szNickName);
}

int hashing(const char* str) {
	int hashing = 5381;
	int c;
	while (c = *str++) {
		hashing = ((hashing << 5) + hashing) + c; /* hash * 33 + c */
		if (hashing < 0) hashing = 100;
	}
	if (hashing < 0) hashing = 100;
	return hashing;
}

void DoDebugLoop()
{
	// ...
}

void DoDebugStuff()
{
	// ...

	RwMatrix mat = pGame->FindPlayerPed()->m_pPed->GetMatrix().ToRwMatrix();

	for (int i = 0; i < 100; i++)
	{
		CPlayerPed* ped = pGame->NewPlayer(i, mat.pos.x + i, mat.pos.y, mat.pos.z, 0.0f, false, false);
		//ped->SetCollisionChecking(false);
		//ped->SetGravityProcessing(false);
	}
}
struct sigaction act_old;
struct sigaction act1_old;
struct sigaction act2_old;
struct sigaction act3_old;

extern int g_iLastProcessedSkinCollision, g_iLastProcessedEntityCollision, g_iLastRenderedObject;
extern uintptr_t g_dwLastRetAddrCrash;
void handler(int signum, siginfo_t *info, void* contextPtr)
{
	ucontext* context = (ucontext_t*)contextPtr;

	if (act_old.sa_sigaction)
	{
		act_old.sa_sigaction(signum, info, contextPtr);
	}

	if(info->si_signo == SIGSEGV)
	{
		FLog("SIGSEGV | Fault address: 0x%x", info->si_addr);

		PRINT_CRASH_STATES(context);

		CStackTrace::printBacktrace();
	}

	return;
}

void handler1(int signum, siginfo_t *info, void* contextPtr)
{
	ucontext* context = (ucontext_t*)contextPtr;

	if (act1_old.sa_sigaction)
	{
		act1_old.sa_sigaction(signum, info, contextPtr);
	}

	if(info->si_signo == SIGABRT)
	{
		FLog("SIGABRT | Fault address: 0x%x", info->si_addr);

		PRINT_CRASH_STATES(context);

		CStackTrace::printBacktrace();
	}

	return;
}

void handler2(int signum, siginfo_t *info, void* contextPtr)
{
	ucontext* context = (ucontext_t*)contextPtr;

	if (act2_old.sa_sigaction)
	{
		act2_old.sa_sigaction(signum, info, contextPtr);
	}

	if(info->si_signo == SIGFPE)
	{
		FLog("SIGFPE | Fault address: 0x%x", info->si_addr);

		PRINT_CRASH_STATES(context);

		CStackTrace::printBacktrace();
	}

	return;
}

void handler3(int signum, siginfo_t *info, void* contextPtr)
{
	ucontext* context = (ucontext_t*)contextPtr;

	if (act3_old.sa_sigaction)
	{
		act3_old.sa_sigaction(signum, info, contextPtr);
	}

	if(info->si_signo == SIGBUS)
	{
		FLog("SIGBUS | Fault address: 0x%x", info->si_addr);

		PRINT_CRASH_STATES(context);

		CStackTrace::printBacktrace();
	}

	return;
}

void DoInitStuff()
{
	if (bGameInited == false)
	{
		pPlayerTags = new CPlayerTags();
		pSnapShotHelper = new CSnapShotHelper();
		pMaterialTextGenerator = new MaterialTextGenerator();
		pAudioStream = new CAudioStream();
		pAudioStream->Initialize();

		if (pJavaWrapper) {
			pJavaWrapper->HideLoadingScreen();
		}
		pUI->chat()->setVisible(true);
		Mchat = true;

		pGame->Initialize();
		pGame->SetMaxStats();
		pGame->ToggleThePassingOfTime(false);

		if (!g_sampChatCfgTried) {
			g_sampChatCfgReady = FindSampChatConfig();
			g_sampChatCfgTried = true;
			UpdateSampChatPos(IsRadarVisible());
		}

		LogVoice("[dbg:samp:load] : module loading...");

		for (const auto& loadCallback : Samp::loadCallbacks) {
			if (loadCallback != nullptr) {
				loadCallback();
			}
		}

		Samp::loadStatus = true;

		LogVoice("[dbg:samp:load] : module loaded");

		if (bDebug)
		{
            CCamera& TheCamera = GTASAEngineApi::Camera();
            (void)TheCamera;
            //TheCamera.Restore();
            CCamera::SetBehindPlayer();
			pGame->DisplayHUD(true);
			pGame->EnableClock(false);

			DoDebugStuff();
		}

		bGameInited = true;
	}

	if (!bNetworkInited && !bDebug)
	{
		if (!pSettings) {
			FLog("Settings were not initialized before network startup; loading now.");
			if (g_pszStorage) {
				ReadSettingFile();
			}
		}

		if (!pSettings) {
			FLog("Settings are unavailable; network startup skipped.");
			return;
		}

		const char* host = pSettings->Get().szHost;
		int port = pSettings->Get().iPort;

		if (host == nullptr || host[0] == '\0') {
			host = cryptor::create("15.228.76.174").decrypt();
		}

		if (port <= 0) {
			port = 7777;
		}

		pNetGame = new CNetGame(host, port, pSettings->Get().szNickName, pSettings->Get().szPassword);
		bNetworkInited = true;

        FLog("DoInitStuff end");
	}
}

extern "C" {
	static int g_nativeOverlayState = 0;
	static bool g_allowNextNativePauseMenu = false;

	static bool IsNativeFrontEndMenuActive()
	{
		return GTASAEngineApi::IsNativeFrontEndMenuActive();
	}

	static bool IsNativeMobileMenuActive()
	{
		return GTASAEngineApi::IsNativeMobileMenuActive();
	}

	static void ForceCloseNativeMobileMenu()
	{
		GTASAEngineApi::ForceCloseNativeMobileMenu();
	}



	static void InitializeSAMPBridge(JNIEnv *pEnv, jobject thiz)
	{
		if (!pSettings && g_pszStorage) {
			ReadSettingFile();
		}
		if (!pJavaWrapper) {
			pJavaWrapper = new CJavaWrapper(pEnv, thiz);
		}
	}

	static void OnInputEndBridge(JNIEnv *pEnv, jobject thiz, jbyteArray str)
	{
		if(pUI)
		{
			pUI->keyboard()->sendForGB(pEnv, thiz, str);
		}
	}

	static void OnEventBackPressedBridge()
	{
		if(pSettings && pJavaWrapper && pSettings->Get().iAndroidKeyboard) {
			pJavaWrapper->HideKeyboard();
		}
	}

	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_initializeSAMP(JNIEnv *pEnv, jobject thiz)
	{
		InitializeSAMPBridge(pEnv, thiz);
	}
	JNIEXPORT void JNICALL Java_com_raiferoleplay_game_game_SAMP_initializeSAMP(JNIEnv *pEnv, jobject thiz)
	{
		InitializeSAMPBridge(pEnv, thiz);
	}
	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_onInputEnd(JNIEnv *pEnv, jobject thiz, jbyteArray str)
	{
		OnInputEndBridge(pEnv, thiz, str);
	}
	JNIEXPORT void JNICALL Java_com_raiferoleplay_game_game_SAMP_onInputEnd(JNIEnv *pEnv, jobject thiz, jbyteArray str)
	{
		OnInputEndBridge(pEnv, thiz, str);
	}
	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_onEventBackPressed(JNIEnv *pEnv, jobject thiz)
	{
		OnEventBackPressedBridge();
	}
	JNIEXPORT void JNICALL Java_com_raiferoleplay_game_game_SAMP_onEventBackPressed(JNIEnv *pEnv, jobject thiz)
	{
		OnEventBackPressedBridge();
	}
	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_setNativeOverlayState(JNIEnv *pEnv, jobject thiz, jint overlayType)
	{
		g_nativeOverlayState = overlayType;
	}
	JNIEXPORT jint JNICALL Java_com_xyron_game_main_SAMP_getNativeOverlayState(JNIEnv *pEnv, jobject thiz)
	{
		return g_nativeOverlayState;
	}
	JNIEXPORT jboolean JNICALL Java_com_xyron_game_main_SAMP_isNativeUserPauseActiveNative(JNIEnv *pEnv, jobject thiz)
	{
		const bool timerPaused = CTimer::GetIsPaused();
		const bool frontendPaused = pGame && pGame->IsGamePaused();
		const bool menuActive = IsNativeFrontEndMenuActive();
		const bool mobileMenuActive = IsNativeMobileMenuActive();
		return (timerPaused || frontendPaused || menuActive || mobileMenuActive) ? JNI_TRUE : JNI_FALSE;
	}
	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_setAllowNextNativePauseMenu(JNIEnv *pEnv, jobject thiz, jboolean allow)
	{
		g_allowNextNativePauseMenu = (allow == JNI_TRUE);
	}
	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_forceEndNativeUserPause(JNIEnv *pEnv, jobject thiz)
	{
		g_allowNextNativePauseMenu = false;
		ForceCloseNativeMobileMenu();
		if (CTimer::GetIsUserPaused()) {
			CTimer::EndUserPause();
		}
	}
	JNIEXPORT void JNICALL Java_com_xyron_game_main_SAMP_sendSyntheticNativeTouch(JNIEnv *pEnv, jobject thiz, jint x, jint y)
	{
		if (!pUI) {
			FLog("[TOUCH_SYNTH64] ignored ui-null x=%d y=%d", (int)x, (int)y);
			return;
		}

		const int touchX = (int)x;
		const int touchY = (int)y;
		FLog("[TOUCH_SYNTH64] tap x=%d y=%d", touchX, touchY);
		pUI->OnTouchEvent(2, false, touchX, touchY);
		if (pNetGame && pNetGame->GetTextDrawPool()) {
			pNetGame->GetTextDrawPool()->onTouchEvent(2, false, touchX, touchY);
		}
		pUI->OnTouchEvent(1, false, touchX, touchY);
		if (pNetGame && pNetGame->GetTextDrawPool()) {
			pNetGame->GetTextDrawPool()->onTouchEvent(1, false, touchX, touchY);
		}
	}
	JNIEXPORT jfloatArray JNICALL Java_com_xyron_game_main_SAMP_getPlayerPlacementSnapshot(JNIEnv *pEnv, jobject thiz)
	{
		return nullptr;
	}
	JNIEXPORT jboolean JNICALL Java_com_xyron_game_main_SAMP_showLocalPickupPreview(JNIEnv *pEnv, jobject thiz, jint modelId, jint pickupType, jfloat x, jfloat y, jfloat z)
	{
		(void)modelId;
		(void)pickupType;
		(void)x;
		(void)y;
		(void)z;
		return JNI_FALSE;
	}
	JNIEXPORT void JNICALL Java_com_nvidia_devtech_NvEventQueueActivity_nativeImGuiRenderFrame(JNIEnv *pEnv, jobject thiz)
	{
		XyronNativeFrameTick();
	}
	JNIEXPORT void JNICALL Java_com_nvidia_devtech_NvEventQueueActivity_nativeImGuiTouchEvent(JNIEnv *pEnv, jobject thiz, jint action, jint pointer, jint x, jint y)
	{
		if (!pUI) {
			return;
		}

		int touchType = 3; // TOUCH_MOVE
		switch (action) {
			case 0: // MotionEvent.ACTION_DOWN
			case 5: // MotionEvent.ACTION_POINTER_DOWN
				touchType = 2; // TOUCH_PUSH
				break;
			case 1: // MotionEvent.ACTION_UP
			case 3: // MotionEvent.ACTION_CANCEL
			case 6: // MotionEvent.ACTION_POINTER_UP
				touchType = 1; // TOUCH_POP
				break;
			default:
				touchType = 3; // TOUCH_MOVE
				break;
		}

		pUI->OnTouchEvent(touchType, pointer != 0, x, y);
	}
	JNIEXPORT void JNICALL Java_com_xyron_game_main_ui_dialog_DialogManager_sendDialogResponse(JNIEnv* pEnv, jobject thiz, jint i3, jint i, jint i2, jbyteArray str)
	{
		jboolean isCopy = true;

		jbyte* pMsg = pEnv->GetByteArrayElements(str, &isCopy);
		jsize length = pEnv->GetArrayLength(str);

		std::string szStr((char*)pMsg, length);

		if(pNetGame) {
			pNetGame->SendDialogResponse(i, i3, i2, (char*)szStr.c_str());
		}

		pEnv->ReleaseByteArrayElements(str, pMsg, JNI_ABORT);
	}
	JNIEXPORT void JNICALL Java_com_raiferoleplay_game_game_ui_dialog_DialogManager_sendDialogResponse(JNIEnv* pEnv, jobject thiz, jint i3, jint i, jint i2, jbyteArray str)
	{
		jboolean isCopy = true;

		jbyte* pMsg = pEnv->GetByteArrayElements(str, &isCopy);
		jsize length = pEnv->GetArrayLength(str);

		std::string szStr((char*)pMsg, length);

		if(pNetGame) {
			pNetGame->SendDialogResponse(i, i3, i2, (char*)szStr.c_str());
			//pGame->FindPlayerPed()->TogglePlayerControllableWithoutLock(true);
		}

		pEnv->ReleaseByteArrayElements(str, pMsg, JNI_ABORT);
	}
	JNIEXPORT void JNICALL Java_com_xyron_game_main_ui_dialog_DialogManager_setDialogVisible(JNIEnv* pEnv, jobject thiz, jboolean visible)
	{
		g_bDialogVisible = (visible == JNI_TRUE);
		if (!pGame) {
			return;
		}

		if (g_bDialogVisible) {
			pGame->DisplayHUD(false);
			return;
		}

		if (IsRadarVisible() && IsLocalPlayerHudReady()) {
			pGame->DisplayHUD(true);
		} else {
			pGame->DisplayHUD(false);
		}
	}
}

void MainLoop()
{
	if (pGame->bIsGameExiting) return;

	Xyron::Perf::BeginFrame();
	Xyron::Perf::ScopedZone perfMainLoop(Xyron::Perf::Zone::MainLoop);

	if (!XyronIsEglSwapBuffersHookInstalled()) {
		XyronCoreFrameTick();
	}
	DoInitStuff();

	if (bDebug) {
		DoDebugLoop();
	}

	if (pNetGame) {
		pNetGame->Process();

		CTextDrawPool* pTextDrawPool = pNetGame->GetTextDrawPool();
		if(pTextDrawPool) pTextDrawPool->Draw();
	}

	if (g_bDialogVisible && pGame) {
		pGame->DisplayHUD(false);
	}

	static uint32_t s_lastChatScan = 0;
	if (!g_sampChatCfgReady)
	{
		uint32_t now = GetTickCount();
		if (now - s_lastChatScan > 2000)
		{
			g_sampChatCfgReady = FindSampChatConfig();
			s_lastChatScan = now;
			if (g_sampChatCfgReady)
			{
				UpdateSampChatPos(IsRadarVisible());
			}
		}
	}

	static int s_lastRadarVisible = -1;
	if (pUI) {
		SampChatAreaRuntime runtime;
		const bool hasChatAreaRuntime = ReadSampChatAreaRuntime(&runtime);
		int radarVisible = IsRadarVisible() ? 1 : 0;
		if (hasChatAreaRuntime) {
			ApplySampChatAreaOverride();
		} else if (radarVisible != s_lastRadarVisible) {
			pUI->chat()->setPosition(radarVisible ? UISettings::chatPos()
												  : UISettings::chatPosNoRadar());
			s_lastRadarVisible = radarVisible;
		}
	}

	if (pNetGame && pNetGame->GetPlayerPool() && pNetGame->GetPlayerPool()->GetLocalPlayer())
	{
		CLocalPlayer* pLocalPlayer = pNetGame->GetPlayerPool()->GetLocalPlayer();
		CPlayerPed* pLocalPlayerPed = pLocalPlayer ? pLocalPlayer->GetPlayerPed() : nullptr;
		if (pLocalPlayerPed && IsLocalPlayerHudReady())
		{
			pGame->DisplayHUD(false);
			GTASAEngineApi::SetNativeHudDisabled(g_bDialogVisible);
			if (pJavaWrapper) {
				CWeapon* weaponSlot = pLocalPlayerPed->GetCurrentWeaponSlot();
				pJavaWrapper->UpdateHud(pLocalPlayerPed->GetHealth(),
										pLocalPlayerPed->GetArmour(),
										pNetGame->eatProcent,
										pGame->GetLocalMoney(),
										pLocalPlayerPed->GetCurrentWeapon(),
										weaponSlot ? weaponSlot->dwAmmo : 0);
				UpdateWeaponWheelSnapshot(pLocalPlayerPed);
			}
		}
		else
		{
			pGame->DisplayHUD(false);
			GTASAEngineApi::SetNativeHudDisabled(true);
			if (pJavaWrapper && g_bHudVisible) {
				pJavaWrapper->HideHud();
			}
			if (pUI) {
				pUI->chat()->setVisible(true);
				Mchat = true;
			}
		}
	}

	if (g_sampChatCfgReady)
	{
		UpdateSampChatPos(IsRadarVisible());
	}

	if (pAudioStream) {
		Xyron::Perf::ScopedZone perfAudio(Xyron::Perf::Zone::Audio);
		pAudioStream->Process();
	}

	Xyron::Perf::EndFrame();
}

void InitGui()
{
	// new voice
	Plugin::OnPluginLoad();
	Plugin::OnSampLoad();

	std::string font_path = string_format("%sfonts/%s", g_pszStorage, FONT_NAME);
	pUI = new UI(ImVec2(RsGlobal->maximumWidth, RsGlobal->maximumHeight), font_path.c_str());
	pUI->initialize();
	pUI->performLayout();
}

#include "game/multitouch.h"
#include "armhook/patch.h"
#include "util/CUtil.h"

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	javaVM = vm;
	LOGI("SA-MP library loaded! Build time: " __DATE__ " " __TIME__);

	g_libGTASA = CUtil::FindLib("libGTASA.so");
	if (g_libGTASA == 0x00) {
		LOGE("libGTASA.so address was not found! ");
		return JNI_VERSION_1_6;
	}

	g_libSAMP = CUtil::FindLib("libsamp.so");
	if (g_libSAMP == 0x00) {
		LOGE("libsamp.so address was not found! ");
		return JNI_VERSION_1_6;
	}

	firebase::crashlytics::Initialize();

	uintptr_t libgtasa = CUtil::FindLib("libGTASA.so");
	uintptr_t libsamp = CUtil::FindLib("libsamp.so");
	uintptr_t libc = CUtil::FindLib("libc.so");

	FLog("libGTASA.so: 0x%x", libgtasa);
	FLog("libsamp.so: 0x%x", libsamp);
	FLog("libc.so: 0x%x", libc);

	char str[100];

	sprintf(str, "0x%x", libgtasa);
	firebase::crashlytics::SetCustomKey("libGTASA.so", str);

	sprintf(str, "0x%x", libsamp);
	firebase::crashlytics::SetCustomKey("libsamp.so", str);

	sprintf(str, "0x%x", libc);
	firebase::crashlytics::SetCustomKey("libc.so", str);

	CHook::InitHookStuff();
	XyronCoreInitialize();
	XyronInstallEglSwapBuffersHook();
	InstallSpecialHooks();
	ApplyPatches_level0();
    InitRenderWareFunctions();
    MultiTouch::initialize();

	pGame = new CGame();

	//pthread_t thread;
	//pthread_create(&thread, 0, Init, 0);

	struct sigaction act;
	act.sa_sigaction = handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &act, &act_old);

	struct sigaction act1;
	act1.sa_sigaction = handler1;
	sigemptyset(&act1.sa_mask);
	act1.sa_flags = SA_SIGINFO;
	sigaction(SIGABRT, &act1, &act1_old);

	struct sigaction act2;
	act2.sa_sigaction = handler2;
	sigemptyset(&act2.sa_mask);
	act2.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &act2, &act2_old);

	struct sigaction act3;
	act3.sa_sigaction = handler3;
	sigemptyset(&act3.sa_mask);
	act3.sa_flags = SA_SIGINFO;
	sigaction(SIGBUS, &act3, &act3_old);

	return JNI_VERSION_1_6;
}

uint32_t GetTickCount()
{
    return CTimer::m_snTimeInMillisecondsNonClipped;
}

static bool StartsWithLogPrefix(const char* text, const char* prefix)
{
	if (!text || !prefix) {
		return false;
	}
	return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

static bool ShouldDropVerboseRuntimeLog(const char* fmt)
{
	if (bDebug || !fmt) {
		return false;
	}

	static constexpr const char* kVerbosePrefixes[] = {
		"[COL_",
		"[WORLD_DIAG64]",
		"[WORLD_NEAR64]",
		"[WORLD_REPAIR64]",
		"[OBJ_DIAG64]",
		"[MAP_DIAG64]",
		"[MAP_PROBE64]",
		"[TEXDB64]",
		"[TEMP_VEH_PART64]",
		"[VEH_MATRIX64]",
		"[VEH_TEST64]",
		"[SHADOW_GUARD64]",
		"[RENDER_LIST64]",
		"[RENDER_GUARD64]",
		"[STREAM_MEM64]",
		"[OBJ_STREAM64]",
		"[WORLD_RENDER64]",
		"[LOD_CULL64]",
		"[RPC_IN64]",
		"[RPC_DISP64]",
		"[RPC_OBJ64]",
		"[RPC_TS64]",
		"[RPC_OUT64]",
		"[RPCX64]",
		"[PKT64]",
		"[PKT_OUT64]",
		"[PKT_LOW64]",
		"New Widget:",
		"TryLoadModel"
	};

	for (const char* prefix : kVerbosePrefixes) {
		if (StartsWithLogPrefix(fmt, prefix)) {
			return true;
		}
	}
	return false;
}

static bool IsCriticalLogLine(const char* line)
{
	return line &&
	       (std::strstr(line, "SIG") ||
	        std::strstr(line, "FATAL") ||
	        std::strstr(line, "Fatal") ||
	        std::strstr(line, "CRASH") ||
	        std::strstr(line, "Crash") ||
	        std::strstr(line, "ANR") ||
	        std::strstr(line, "Error") ||
	        std::strstr(line, "ERROR"));
}

void FLog(const char* fmt, ...)
{
	if (ShouldDropVerboseRuntimeLog(fmt)) {
		return;
	}

	char buffer[0xFF];
	static FILE* flLog = nullptr;
	static uint32_t s_lastFlushTick = 0;
	static uint32_t s_crashlyticsContextUntilTick = 0;
	const char* pszStorage = g_pszStorage;


	if (flLog == nullptr && pszStorage != nullptr)
	{
		sprintf(buffer, "%s/samp_log.txt", pszStorage);
		//LOGI("buffer: %s", buffer);
		flLog = fopen(buffer, "a");
	}

	memset(buffer, 0, sizeof(buffer));

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	LOGI("%s", buffer);

	const uint32_t now = GetTickCount();
	const bool criticalLine = IsCriticalLogLine(buffer);
	if (criticalLine) {
		s_crashlyticsContextUntilTick = now + 3000;
	}
	if (criticalLine || (s_crashlyticsContextUntilTick != 0 && now <= s_crashlyticsContextUntilTick)) {
		firebase::crashlytics::Log(buffer);
	}

	if (flLog == nullptr) return;
	fprintf(flLog, "%s\n", buffer);
	if (criticalLine || now - s_lastFlushTick >= 1000) {
		s_lastFlushTick = now;
		fflush(flLog);
	}

	return;
}

void ChatLog(const char* fmt, ...)
{
	char buffer[0xFF];
	static FILE* flLog = nullptr;
	const char* pszStorage = g_pszStorage;


	if (flLog == nullptr && pszStorage != nullptr)
	{
		sprintf(buffer, "%s/chat_log.txt", pszStorage);
		flLog = fopen(buffer, "a");
	}

	memset(buffer, 0, sizeof(buffer));

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	if (flLog == nullptr) return;
	fprintf(flLog, "%s\n", buffer);
	fflush(flLog);

	return;
}

void MyLog(const char* fmt, ...)
{
	char buffer[0xFF];
	static FILE* flLog = nullptr;
	const char* pszStorage = g_pszStorage;


	if (flLog == nullptr && pszStorage != nullptr)
	{
		sprintf(buffer, "%s/samp_log.txt", pszStorage);
		//LOGI("buffer: %s", buffer);
		flLog = fopen(buffer, "a");
	}

	memset(buffer, 0, sizeof(buffer));

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	if (flLog == nullptr) return;
	fprintf(flLog, "%s\n", buffer);
	fflush(flLog);

	return;
}

void MyLog2(const char* fmt, ...)
{
	char buffer[0xFF];
	static FILE* flLog = nullptr;
	const char* pszStorage = g_pszStorage;


	if (flLog == nullptr && pszStorage != nullptr)
	{
		sprintf(buffer, "%s/samp_log.txt", pszStorage);
		//LOGI("buffer: %s", buffer);
		flLog = fopen(buffer, "a");
	}

	memset(buffer, 0, sizeof(buffer));

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	if (pUI) pUI->chat()->addDebugMessage(buffer);

	if (flLog == nullptr) return;
	fprintf(flLog, "%s\n", buffer);
	fflush(flLog);
	return;
}

void LogVoice(const char* fmt, ...)
{
	char buffer[0xFF];
	static FILE* flLog = nullptr;
	const char* pszStorage = g_pszStorage;

	if (flLog == nullptr && pszStorage != nullptr)
	{
		sprintf(buffer, "%sSAMP/%s", pszStorage, SV::kLogFileName);
		flLog = fopen(buffer, "w");
	}

	memset(buffer, 0, sizeof(buffer));

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	__android_log_write(ANDROID_LOG_INFO, "AXL", buffer);

	if (flLog == nullptr) return;
	fprintf(flLog, "%s\n", buffer);
	fflush(flLog);

	return;
}
