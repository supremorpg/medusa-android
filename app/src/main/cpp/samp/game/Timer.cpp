//
// Created by x1y2z on 14.04.2023.
//

#include "Timer.h"
#include "GTASAEngineApi.h"
#include "../java/jniutil.h"
#include "../gui/gui.h"
#include "../net/netgame.h"

extern CJavaWrapper *pJavaWrapper;
extern UI *pUI;
extern CNetGame* pNetGame;

uint8_t* gTimerRunning = nullptr;

float CTimer::game_FPS = 0;

bool        CTimer::m_CodePause = false;
bool        CTimer::m_UserPause = false;
float       CTimer::ms_fTimeScale;
uint32_t    CTimer::m_FrameCounter = 0;
uint32_t    CTimer::m_snTimeInMilliseconds = 0;
bool        CTimer::bSkipProcessThisFrame = false;
float       CTimer::ms_fTimeStep = 0;
uint32_t    CTimer::m_snPPPPreviousTimeInMilliseconds;
uint32_t    CTimer::m_snPPPreviousTimeInMilliseconds;
uint32_t    CTimer::m_snPPreviousTimeInMilliseconds;
uint32_t    CTimer::m_snPreviousTimeInMilliseconds;
uint32_t    CTimer::m_snTimeInMillisecondsNonClipped;
uint32_t    CTimer::m_snPreviousTimeInMillisecondsNonClipped;

void CTimer::InjectHooks()
{
    GTASAEngineApi::TimerGlobals globals{};
    globals.codePause = &CTimer::m_CodePause;
    globals.frameCounter = &CTimer::m_FrameCounter;
    globals.gameFps = &CTimer::game_FPS;
    globals.userPause = &CTimer::m_UserPause;
    globals.timeScale = &CTimer::ms_fTimeScale;
    globals.timeInMilliseconds = &CTimer::m_snTimeInMilliseconds;
    globals.skipProcessThisFrame = &CTimer::bSkipProcessThisFrame;
    globals.timeStep = &CTimer::ms_fTimeStep;
    globals.pppPreviousTimeInMilliseconds = &CTimer::m_snPPPPreviousTimeInMilliseconds;
    globals.ppPreviousTimeInMilliseconds = &CTimer::m_snPPPreviousTimeInMilliseconds;
    globals.pPreviousTimeInMilliseconds = &CTimer::m_snPPreviousTimeInMilliseconds;
    globals.previousTimeInMilliseconds = &CTimer::m_snPreviousTimeInMilliseconds;
    globals.timeInMillisecondsNonClipped = &CTimer::m_snTimeInMillisecondsNonClipped;
    globals.previousTimeInMillisecondsNonClipped = &CTimer::m_snPreviousTimeInMillisecondsNonClipped;

    GTASAEngineApi::BindTimerGlobals(globals);
    gTimerRunning = GTASAEngineApi::TimerRunning();

    GTASAEngineApi::InstallTimerControlHooks(
        &CTimer::StartUserPause,
        &CTimer::EndUserPause,
        &CTimer::Stop,
        &CTimer::GetIsSlowMotionActive);
}


uint64_t GetMillisecondTime() {

}

// 0x5617E0
void CTimer::Initialise()
{

}

// 0x5618C0
void CTimer::Shutdown() {

}

// 0x5619D0
void CTimer::Suspend()
{
    GTASAEngineApi::TimerSuspend();
}

// 0x561A00
void CTimer::Resume()
{
    GTASAEngineApi::TimerResume();
}

// 0x561AA0
void CTimer::Stop()
{
    *gTimerRunning = 0;
    CTimer::m_snPPPPreviousTimeInMilliseconds = CTimer::m_snTimeInMilliseconds;
    CTimer::m_snPPPreviousTimeInMilliseconds = CTimer::m_snTimeInMilliseconds;
    CTimer::m_snPPreviousTimeInMilliseconds = CTimer::m_snTimeInMilliseconds;
    CTimer::m_snPreviousTimeInMilliseconds = CTimer::m_snTimeInMilliseconds;

    CTimer::m_snPreviousTimeInMillisecondsNonClipped = CTimer::m_snTimeInMillisecondsNonClipped;
}

// 0x561AF0
void CTimer::StartUserPause()
{
    if (pUI) pUI->setVisible(false);

    if(pJavaWrapper && pNetGame)
    {
        pJavaWrapper->SetPauseState(true);
    }
    m_UserPause = true;
}

// 0x561B00
void CTimer::EndUserPause()
{
    // process resume event
    if (pUI) pUI->setVisible(true);
    if(pJavaWrapper && pNetGame)
    {
        pJavaWrapper->SetPauseState(false);
    }
    m_UserPause = false;
}

// 0x561A40
uint32_t CTimer::GetCyclesPerMillisecond()
{
    return GTASAEngineApi::GetCyclesPerMillisecond();
}

// cycles per ms * 20
// 0x561A50
uint32_t CTimer::GetCyclesPerFrame()
{

}

uint64_t CTimer::GetCurrentTimeInCycles()
{
    return GTASAEngineApi::GetCurrentTimeInCycles();
}

// 0x561AD0
bool CTimer::GetIsSlowMotionActive()
{
    return CTimer::ms_fTimeScale < 1.0;
}

// 0x5618D0
void CTimer::UpdateVariables(float timeElapsed)
{

}

// 0x561B10
void CTimer::Update()
{

}

uint32_t CTimer::GetCurrentUnixTimeMoscow() {
    auto now = std::chrono::system_clock::now();

    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* ptm = std::gmtime(&now_c);
    ptm->tm_hour += 3;
    std::time_t moscow_time = std::mktime(ptm);

    auto duration = std::chrono::system_clock::from_time_t(moscow_time).time_since_epoch();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
}
