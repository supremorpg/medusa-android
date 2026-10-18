//
// Created by roman on 11/24/2024.
//

#include "CPedGTA.h"
#include "game/General.h"
#include "game/SampStreamingApi.h"
#include "game/util.h"
#include "game/GTASAEngineApi.h"

#if !VER_x32
extern "C" void XyronRememberLocalVehicleForRender64(CVehicleGTA* vehicle);
#endif

void CPedGTA::GetBonePosition(RwV3d *posn, uint32 boneTag, bool bCalledFromCamera) {
    GTASAEngineApi::GetPedBonePosition(this, posn, boneTag, bCalledFromCamera);
}

bool CPedGTA::IsEnteringCar() {
    if ( GetTaskManager().CTaskManager::FindActiveTaskByType(TASK_COMPLEX_ENTER_CAR_AS_DRIVER) )
        return true;

    return GetTaskManager().CTaskManager::FindActiveTaskByType(TASK_COMPLEX_ENTER_CAR_AS_PASSENGER) != nullptr;
}

bool CPedGTA::IsExitingVehicle() {
    if ( GetTaskManager().CTaskManager::FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR) )
        return true;

    return false;
}

CPedGTA::CPedGTA(ePedType pedType) : CPhysical(), m_pedIK{CPedIK(this)} {
    m_vecAnimMovingShiftLocal = CVector2D();

    m_fHealth = 100.0f;
    m_fMaxHealth = 100.0f;
    m_fArmour = 0.0f;

    m_nPedType = pedType;
    m_nType = ENTITY_TYPE_PED;

    // 0x5E8196
    physicalFlags.bCanBeCollidedWith = true;
    physicalFlags.bDisableTurnForce = true;

    m_nCreatedBy = PED_GAME;
    pVehicle = nullptr;
    m_nAntiSpazTimer = 0;
    m_nUnconsciousTimer = 0;
    m_nAttackTimer = 0;
    m_nLookTime = 0;
    m_nDeathTimeMS = 0;

    m_vecAnimMovingShift = CVector2D();
//    field_56C = CVector();
//    field_578 = CVector(0.0f, 0.0f, 1.0f);

    m_nPedState = PEDSTATE_IDLE;
    m_nMoveState = PEDMOVE_STILL;
    m_fCurrentRotation = 0.0f;
    m_fHeadingChangeRate = 15.0f;
    m_fMoveAnim = 0.1f;
    m_fAimingRotation = 0.0f;
    //  m_standingOnEntity = nullptr;
    m_nWeaponShootingRate = 40;
    //   field_594 = 0;
    // m_pEntityIgnoredCollision = nullptr;
    // m_nSwimmingMoveState = 0;
    m_pFire = nullptr;
    m_fireDmgMult = 1.0f;
    // m_pTargetedObject = nullptr;
    m_pLookTarget = nullptr;
    m_fLookDirection = 0.0f;
    //  m_pContactEntity = nullptr;
    // field_588 = 99999.992f;
    m_fMass = 70.0f;
    m_fTurnMass = 100.0f;
    m_fAirResistance = 1.f / 175.f;
    m_fElasticity = 0.05f;
//    m_nBodypartToRemove = -1;
    bHasACamera = CGeneral::GetRandomNumber() % 4 != 0;

    // m_weaponAudio.Initialise(this);
    // m_pedAudio.Initialise(this);

    //  m_acquaintance = CPedType::GetPedTypeAcquaintances(m_nPedType);
    m_nSavedWeapon = WEAPON_UNIDENTIFIED;
    m_nDelayedWeapon = WEAPON_UNIDENTIFIED;
    m_nActiveWeaponSlot = 0;

    for (auto& weapon : m_aWeapons ) {
        weapon.dwType = WEAPON_UNARMED;
        weapon.dwState = WEAPONSTATE_READY;
        weapon.dwAmmoInClip = 0;
        weapon.dwAmmo = 0;
        //  weapon.m_nTimeForNextShot = 0;
    }

    m_nWeaponSkill = eWeaponSkill::STD;
    m_nFightingStyle = STYLE_STANDARD;
    m_nAllowedAttackMoves = 0;

    // GiveWeapon(WEAPON_UNARMED, 0, true);

    m_nWeaponAccuracy = 60;
    //  m_nLastWeaponDamage = -1;
    m_pLastEntityDamage = nullptr;
    //  field_768 = 0;
    m_pAttachedTo = nullptr;
    //  m_nTurretAmmo = 0;
    //  field_460 = nullptr;
    // field_468 = 0;
    m_nWeaponModelId = -1;
    //   m_nMoneyCount = 0;
    field_72F = 0;
//    m_nTimeTillWeNeedThisPed = 0;
    m_VehDeadInFrontOf = nullptr;

    m_pWeaponObject = nullptr;
    m_pGunflashObject = nullptr;
    m_pGogglesObject = nullptr;
    m_pGogglesState = nullptr;

    m_nWeaponGunflashAlphaMP1 = 0;
    m_nWeaponGunFlashAlphaProgMP1 = 0;
    m_nWeaponGunflashAlphaMP2 = 0;
    m_nWeaponGunFlashAlphaProgMP2 = 0;

    m_pCoverPoint = nullptr;
    //   m_pEnex = nullptr;
    //  field_798 = -1;

//    m_pIntelligence = new CPedIntelligence(this);
    m_pPlayerData = nullptr;

//    if (!IsPlayer()) {
//        GetTaskManager().SetTaskSecondary(new CTaskComplexFacial{}, TASK_SECONDARY_FACIAL_COMPLEX);
//    }
    //  GetTaskManager().SetTask(new CTaskSimpleStandStill{ 0, true, false, 8.0 }, TASK_PRIMARY_DEFAULT, false);

    // field_758 = 0;
    m_fRemovalDistMultiplier = 1.0f;
    m_nSpecialModelIndex = -1;

    //   CPopulation::UpdatePedCount(this, 0);

//    if (CCheat::IsActive(CHEAT_HAVE_ABOUNTY_ON_YOUR_HEAD)) {
//        if (!IsPlayer()) {
//            GetAcquaintance().SetAsAcquaintance(ACQUAINTANCE_HATE, CPedType::GetPedFlag(ePedType::PED_TYPE_PLAYER1));
//
//            CEventAcquaintancePedHate event(FindPlayerPed());
//            GetEventGroup().Add(&event);
//        }
//    }
}

CPedGTA::~CPedGTA() {
    GTASAEngineApi::ShutdownPed(this);
}

void CPedGTA::GiveWeapon(int iWeaponID, int iAmmo)
{
    int iModelID = 0;
    iModelID = GameGetWeaponModelIDFromWeaponID(iWeaponID);
    FLog("GiveWeapon, iModelID %d, iWeaponID %d", iModelID, iWeaponID);

    if (iModelID == -1 || iModelID == 350 || iModelID == 365) return;

    if (!Xyron::Streaming::TryLoadModel(iModelID))
        return;

    GTASAEngineApi::GivePedWeapon(this, iWeaponID, iAmmo);
    GTASAEngineApi::SetPedCurrentWeaponAfterGive(this, iWeaponID);
}

void CPedGTA::RemoveFromVehicle()
{
    CVehicleGTA* oldVehicle = pVehicle;
    if(!bInVehicle && !oldVehicle)
        return;

    if (m_pIntelligence) {
        m_pIntelligence->m_TaskMgr.FlushImmediately();
    }

    auto pos = oldVehicle ? oldVehicle->GetPosition() : GetPosition();
    float ang = oldVehicle ? oldVehicle->GetHeading() : m_fCurrentRotation;

    pos.x += (1.0f * sin(ang + 4.71239f)); // 270 deg
    pos.y += (1.0f * sin(-ang + 4.71239f));

    if (oldVehicle) {
        if (oldVehicle->pDriver == this) {
            oldVehicle->pDriver = nullptr;
        }

        for (size_t i = 0; i < 8; ++i) {
            if (oldVehicle->m_apPassengers[i] == this) {
                oldVehicle->m_apPassengers[i] = nullptr;
                if (oldVehicle->m_nNumPassengers > 0) {
                    --oldVehicle->m_nNumPassengers;
                }
            }
        }

        if (oldVehicle->m_pEntityIgnoredCollision == this) {
            oldVehicle->m_pEntityIgnoredCollision = nullptr;
        }
        oldVehicle->SetInterior(static_cast<int>(m_nAreaCode), false);
        oldVehicle->m_nNumGettingIn = 0;
        oldVehicle->m_nGettingInFlags = 0;
        oldVehicle->m_nGettingOutFlags = 0;
        oldVehicle->m_bUsesCollision = true;
        oldVehicle->physicalFlags.bCollidable = true;
        oldVehicle->physicalFlags.bCanBeCollidedWith = true;
        oldVehicle->physicalFlags.bDisableSimpleCollision = false;
        oldVehicle->physicalFlags.bSkipLineCol = false;
        oldVehicle->physicalFlags.bApplyGravity = true;
        oldVehicle->physicalFlags.bDisableCollisionForce = false;
        oldVehicle->physicalFlags.bDisableMoveForce = false;
        oldVehicle->physicalFlags.bDontApplySpeed = false;
        oldVehicle->m_bIsVisible = true;
        oldVehicle->m_bRemoveFromWorld = false;
        oldVehicle->m_bDistanceFade = false;
        oldVehicle->m_nVehicleFlags.bFadeOut = false;
        oldVehicle->m_nVehicleFlags.bIsBeingCarJacked = false;
        oldVehicle->m_nVehicleFlags.bDriverLastFrame = false;
        oldVehicle->m_nVehicleFlags.bNeverUseSmallerRemovalRange = true;
        if (!oldVehicle->pDriver && oldVehicle->GetStatus() == STATUS_PLAYER) {
            oldVehicle->SetStatus(STATUS_ABANDONED);
        }
        if (!oldVehicle->m_pRwObject && Xyron::Streaming::IsModelLoaded(oldVehicle->m_nModelIndex)) {
            oldVehicle->CreateRwObject();
        }
        if (oldVehicle->m_pRwObject) {
            oldVehicle->UpdateRW();
            oldVehicle->UpdateRwFrame();
        }
#if !VER_x32
        XyronRememberLocalVehicleForRender64(oldVehicle);
#endif
    }

    bInVehicle = false;
    pVehicle = nullptr;
    m_pEntityIgnoredCollision = nullptr;
    m_pGroundPhysical = nullptr;
    m_pEntityStandingOn = nullptr;
    m_nPedState = PEDSTATE_IDLE;
    m_bUsesCollision = true;
    physicalFlags.bCollidable = true;
    physicalFlags.bCanBeCollidedWith = true;
    physicalFlags.bDisableSimpleCollision = false;
    physicalFlags.bSkipLineCol = false;
    physicalFlags.bApplyGravity = true;
    ResetMoveSpeed();
    ResetTurnSpeed();
    SetPosn(pos);
    UpdateRW();
    UpdateRwFrame();
}
