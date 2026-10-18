#include "../main.h"
#include "game.h"
#include "GTASAEngineApi.h"
#include "SampCollisionApi.h"
#include "SampStreamingApi.h"

extern CGame* pGame;

// 0.3.7
CActor::CActor(int iSkin, float fX, float fY, float fZ, float fAngle)
{
    if (!Xyron::Streaming::TryLoadModel(iSkin))
        throw std::runtime_error("Model not loaded");

    if (!IsValidPedModel(iSkin))
    {
        iSkin = 0;
    }

    ScriptCommand(&create_actor, 5, iSkin, fX, fY, fZ, &m_dwGTAId);

    m_pPed = GamePool_Ped_GetAt(m_dwGTAId);

    ForceTargetRotation(fAngle);
    CVector pos(fX, fY, fZ);
    Xyron::Collision::RequestCollision(pos, m_pPed->m_nAreaCode);
    m_pPed->SetPosn(pos);
    m_pPed->bDontRender = false;
    m_pPed->m_bIsVisible = true;
    m_pPed->UpdateRW();
    m_pPed->UpdateRwFrame();

    ScriptCommand(&set_actor_can_be_decapitated, m_dwGTAId, 0);
}
// 0.3.7
CActor::~CActor()
{
    auto modelId = m_pPed->m_nModelIndex;

    if (IsValidGamePed(m_pPed))
    {
        GTASAEngineApi::DestroyPlayerPed(m_pPed);
    }

    m_pPed = nullptr;
    m_dwGTAId = 0;

    Xyron::Streaming::RemoveModelIfNoRefs(modelId);
}
void CActor::ForceTargetRotation(float fRotation)
{

    if (!m_pPed) return;
    if (!GamePool_Ped_GetAt(m_dwGTAId)) return;

    if (!IsValidGamePed(m_pPed))
    {
        return;
    }

    m_pPed->m_fCurrentRotation = DegToRad(fRotation);
    m_pPed->m_fAimingRotation = DegToRad(fRotation);

    ScriptCommand(&set_actor_z_angle, m_dwGTAId, fRotation);
}
// 0.3.7
void CActor::SetHealth(float fHealth)
{
	if (m_pPed) {
		m_pPed->m_fHealth = fHealth;
	}
}
// 0.3.7
void CActor::SetInvulnerable(bool bInvulnerable)
{
	m_bInvulnerable = bInvulnerable;

	if (bInvulnerable) {
		ScriptCommand(&set_actor_immunities, m_dwGTAId, 1, 1, 1, 1, 1);
	}
	else {
		ScriptCommand(&set_actor_immunities, m_dwGTAId, 0, 0, 0, 0, 0);
	}
}
// 0.3.7 (adapted)
void CActor::ApplyAnimation(const char* szAnimName, const char* szAnimLib, float fDelta,
	int bLoop, int bLockX, int bLockY, int bFreeze, int iTime)
{
	if (!m_pPed) return;
	if (!GamePool_Ped_GetAt(m_dwGTAId)) return;

	if (!strcasecmp(szAnimLib, "SEX")) return;

	if (!pGame->IsAnimationLoaded(szAnimLib)) {
		pGame->RequestAnimation(szAnimLib);

        ScriptCommand(&apply_animation, m_dwGTAId, szAnimName, szAnimLib, fDelta, bLoop, bLockX, bLockY, bFreeze, iTime);
		return;
	}

	ScriptCommand(&apply_animation, m_dwGTAId, szAnimName, szAnimLib, fDelta, bLoop, bLockX, bLockY, bFreeze, iTime);
}
// 0.3.7
void CActor::ClearAnimation()
{
	if (m_pPed) {
	}
}
// 0.3.7
void CActor::SetFacingAngle(float fAngle)
{
	if (m_pPed && GamePool_Ped_GetAt(m_dwGTAId)) {
		m_pPed->m_fAimingRotation = DegToRad(fAngle);
	}
}
