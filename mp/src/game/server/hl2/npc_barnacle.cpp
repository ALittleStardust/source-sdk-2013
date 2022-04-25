//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		barnacle - stationary ceiling mounted 'fishing' monster	
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "physics_prop_ragdoll.h"
#include "npc_barnacle.h"
#include "npcevent.h"
#include "gib.h"
#include "ai_default.h"
#include "activitylist.h"
#include "hl2_player.h"
#include "vstdlib/random.h"
#include "physics_saverestore.h"
#include "vcollide_parse.h"
#include "vphysics/constraints.h"
#include "studio.h"
#include "bone_setup.h"
#include "iservervehicle.h"
#include "collisionutils.h"
#include "combine_mine.h"
#include "explode.h"
#include "npc_BaseZombie.h"
#include "modelentities.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Private activities.
//-----------------------------------------------------------------------------
int ACT_BARNACLE_SLURP;			// Pulling the tongue up with prey on the end
int ACT_BARNACLE_BITE_HUMAN;	// Biting the head of a humanoid
int ACT_BARNACLE_BITE_PLAYER;	// Biting the head of the player
int ACT_BARNACLE_CHEW_HUMAN;	// Slowly swallowing the humanoid
int ACT_BARNACLE_BARF_HUMAN;	// Spitting out human legs & gibs
int ACT_BARNACLE_TONGUE_WRAP;	// Wrapping the tongue around a target
int ACT_BARNACLE_TASTE_SPIT;	// Yuck! Me no like that!
int ACT_BARNACLE_BITE_SMALL_THINGS;	// Eats small things
int ACT_BARNACLE_CHEW_SMALL_THINGS;	// Chews small things


//-----------------------------------------------------------------------------
// Interactions
//-----------------------------------------------------------------------------
int	g_interactionBarnacleVictimDangle = 0;
int	g_interactionBarnacleVictimReleased = 0;
int	g_interactionBarnacleVictimGrab = 0;
int g_interactionBarnacleVictimBite = 0;

LINK_ENTITY_TO_CLASS(npc_barnacle, CNPC_Barnacle);

// Tongue Spring constants
#define BARNACLE_TONGUE_SPRING_CONSTANT_HANGING			10000
#define BARNACLE_TONGUE_SPRING_CONSTANT_LIFTING			10000
#define BARNACLE_TONGUE_SPRING_CONSTANT_LOWERING		7000
#define BARNACLE_TONGUE_SPRING_DAMPING					20
#define BARNACLE_TONGUE_TIP_MASS						100
#define BARNACLE_TONGUE_MAX_LIFT_MASS					70

#define BARNACLE_BITE_DAMAGE_TO_PLAYER					15
#define BARNACLE_DEAD_TONGUE_ALTITUDE					164
#define BARNACLE_MIN_DEAD_TONGUE_CLEARANCE				78

CNPC_Barnacle::CNPC_Barnacle(void) {}
CNPC_Barnacle::~CNPC_Barnacle(void) {}

BEGIN_DATADESC(CNPC_Barnacle)

DEFINE_FIELD(m_flAltitude, FIELD_FLOAT),
DEFINE_FIELD(m_cGibs, FIELD_INTEGER),// barnacle loads up on gibs each time it kills something.
DEFINE_FIELD(m_bLiftingPrey, FIELD_BOOLEAN),
DEFINE_FIELD(m_bSwallowingPrey, FIELD_BOOLEAN),
DEFINE_FIELD(m_flDigestFinish, FIELD_TIME),
DEFINE_FIELD(m_bPlayedPullSound, FIELD_BOOLEAN),
DEFINE_FIELD(m_bPlayerWasStanding, FIELD_BOOLEAN),
DEFINE_FIELD(m_flVictimHeight, FIELD_FLOAT),
DEFINE_FIELD(m_iGrabbedBoneIndex, FIELD_INTEGER),

DEFINE_FIELD(m_vecRoot, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_vecTip, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_hTongueRoot, FIELD_EHANDLE),
DEFINE_FIELD(m_hTongueTip, FIELD_EHANDLE),
DEFINE_FIELD(m_hRagdoll, FIELD_EHANDLE),
DEFINE_AUTO_ARRAY(m_pRagdollBones, FIELD_MATRIX3X4_WORLDSPACE),
DEFINE_PHYSPTR(m_pConstraint),
DEFINE_KEYFIELD(m_flRestUnitsAboveGround, FIELD_FLOAT, "RestDist"),
DEFINE_FIELD(m_nSpitAttachment, FIELD_INTEGER),
DEFINE_FIELD(m_hLastSpitEnemy, FIELD_EHANDLE),
DEFINE_FIELD(m_nShakeCount, FIELD_INTEGER),
DEFINE_FIELD(m_flNextBloodTime, FIELD_TIME),
#ifndef _XBOX
DEFINE_FIELD(m_nBloodColor, FIELD_INTEGER),
#endif
DEFINE_FIELD(m_vecBloodPos, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_flBarnaclePullSpeed, FIELD_FLOAT),
DEFINE_FIELD(m_flLocalTimer, FIELD_TIME),
DEFINE_FIELD(m_vLastEnemyPos, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_flLastPull, FIELD_FLOAT),
DEFINE_EMBEDDED(m_StuckTimer),

DEFINE_INPUTFUNC(FIELD_VOID, "DropTongue", InputDropTongue),
DEFINE_INPUTFUNC(FIELD_INTEGER, "SetDropTongueSpeed", InputSetDropTongueSpeed),

#ifdef HL2_EPISODIC
DEFINE_INPUTFUNC(FIELD_VOID, "LetGo", InputLetGo),
DEFINE_OUTPUT(m_OnGrab, "OnGrab"),
DEFINE_OUTPUT(m_OnRelease, "OnRelease"),
#endif

// Function pointers
DEFINE_THINKFUNC(BarnacleThink),
DEFINE_THINKFUNC(WaitTillDead),

DEFINE_FIELD(m_bSwallowingBomb, FIELD_BOOLEAN),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CNPC_Barnacle, DT_Barnacle)
SendPropFloat(SENDINFO(m_flAltitude), 0, SPROP_NOSCALE),
SendPropVector(SENDINFO(m_vecRoot), 0, SPROP_COORD),
SendPropVector(SENDINFO(m_vecTip), 0, SPROP_COORD),
SendPropVector(SENDINFO(m_vecTipDrawOffset), 0, SPROP_NOSCALE),
END_SEND_TABLE()


//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
Class_T	CNPC_Barnacle::Classify(void)
{
	return	CLASS_BARNACLE;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize absmin & absmax to the appropriate box
//-----------------------------------------------------------------------------
void CNPC_Barnacle::ComputeWorldSpaceSurroundingBox(Vector *pVecWorldMins, Vector *pVecWorldMaxs)
{
	*pVecWorldMins = *pVecWorldMaxs = Vector();
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CNPC_Barnacle::HandleAnimEvent(animevent_t *pEvent)
{
}

//=========================================================
// Spawn
//=========================================================
void CNPC_Barnacle::Spawn() {}


void CNPC_Barnacle::Activate(void) {}

int	CNPC_Barnacle::OnTakeDamage_Alive(const CTakeDamageInfo &inputInfo)
{
	return BaseClass::OnTakeDamage_Alive(CTakeDamageInfo());
}

void CNPC_Barnacle::PlayerHasIlluminatedNPC(CBasePlayer *pPlayer, float flDot) {}

void CNPC_Barnacle::BarnacleThink(void) {}

//-----------------------------------------------------------------------------

void CNPC_Barnacle::InputSetDropTongueSpeed(inputdata_t &inputdata)
{
}

void CNPC_Barnacle::InputDropTongue(inputdata_t &inputdata)
{
}

void CNPC_Barnacle::LostPrey(bool bRemoveRagdoll)
{
}

void CNPC_Barnacle::Event_Killed(const CTakeDamageInfo &info)
{
}

void CNPC_Barnacle::WaitTillDead(void)
{
}

#if HL2_EPISODIC
//=========================================================
// Some creatures are poisonous to barnacles, and the barnacle
// will die after consuming them. This determines if a given 
// entity is one of those things.
// todo: could be a bit faster
//=========================================================
bool CNPC_Barnacle::IsPoisonous(CBaseEntity *pVictim)
{
	return false;
}

void CNPC_Barnacle::InputLetGo(inputdata_t &inputdata)
{
}


// Barnacle has custom impact damage tables, so it can take grave damage from sawblades.
static impactentry_t barnacleLinearTable[] =
{
	{ 150 * 150, 5 },
	{ 250 * 250, 10 },
	{ 350 * 350, 50 },
	{ 500 * 500, 100 },
	{ 1000 * 1000, 500 },
};


static impactentry_t barnacleAngularTable[] =
{
	{ 100 * 100, 35 },  // Sawblade always kills.
	{ 200 * 200, 50 },
	{ 250 * 250, 500 },
};

static impactdamagetable_t gBarnacleImpactDamageTable =
{
	barnacleLinearTable,
	barnacleAngularTable,

	ARRAYSIZE(barnacleLinearTable),
	ARRAYSIZE(barnacleAngularTable),

	24 * 24,		// minimum linear speed squared
	360 * 360,	// minimum angular speed squared (360 deg/s to cause spin/slice damage)
	2,			// can't take damage from anything under 2kg

	5,			// anything less than 5kg is "small"
	5,			// never take more than 5 pts of damage from anything under 5kg
	36 * 36,		// <5kg objects must go faster than 36 in/s to do damage

	VPHYSICS_LARGE_OBJECT_MASS,		// large mass in kg 
	4,			// large mass scale (anything over 500kg does 4X as much energy to read from damage table)
	5,			// large mass falling scale (emphasize falling/crushing damage over sideways impacts since the stress will kill you anyway)
	0.0f,		// min vel
};


const impactdamagetable_t &CNPC_Barnacle::GetPhysicsImpactDamageTable(void)
{
	return gBarnacleImpactDamageTable;
}

#endif


//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Barnacle::Precache() {}

//===============================================================================================================================
// BARNACLE TONGUE TIP
//===============================================================================================================================
// Crane tip
LINK_ENTITY_TO_CLASS(npc_barnacle_tongue_tip, CBarnacleTongueTip);

BEGIN_DATADESC(CBarnacleTongueTip)

DEFINE_FIELD(m_hBarnacle, FIELD_EHANDLE),
DEFINE_PHYSPTR(m_pSpring),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: To by usable by vphysics, this needs to have a phys model.
//-----------------------------------------------------------------------------
void CBarnacleTongueTip::Spawn(void) {}

int CBarnacleTongueTip::UpdateTransmitState(void)
{
	return SetTransmitState(FL_EDICT_PVSCHECK);
}

void CBarnacleTongueTip::Precache(void) {}
void CBarnacleTongueTip::UpdateOnRemove() {}
void CBarnacleTongueTip::VPhysicsUpdate(IPhysicsObject *pPhysics) {}

bool CBarnacleTongueTip::CreateSpring(CBaseAnimating *pTongueRoot)
{
	return true;
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC(npc_barnacle, CNPC_Barnacle)

// Register our interactions
DECLARE_INTERACTION(g_interactionBarnacleVictimDangle)
DECLARE_INTERACTION(g_interactionBarnacleVictimReleased)
DECLARE_INTERACTION(g_interactionBarnacleVictimGrab)
DECLARE_INTERACTION(g_interactionBarnacleVictimBite)

// Conditions

// Tasks

// Activities
DECLARE_ACTIVITY(ACT_BARNACLE_SLURP)			// Pulling the tongue up with prey on the end
DECLARE_ACTIVITY(ACT_BARNACLE_BITE_HUMAN)		// Biting the head of a humanoid
DECLARE_ACTIVITY(ACT_BARNACLE_BITE_PLAYER)	// Biting the head of a humanoid
DECLARE_ACTIVITY(ACT_BARNACLE_CHEW_HUMAN)		// Slowly swallowing the humanoid
DECLARE_ACTIVITY(ACT_BARNACLE_BARF_HUMAN)		// Spitting out human legs & gibs
DECLARE_ACTIVITY(ACT_BARNACLE_TONGUE_WRAP)	// Wrapping the tongue around a target
DECLARE_ACTIVITY(ACT_BARNACLE_TASTE_SPIT)		// Yuck! Me no like that!
DECLARE_ACTIVITY(ACT_BARNACLE_BITE_SMALL_THINGS)	// Biting small things, like a headcrab
DECLARE_ACTIVITY(ACT_BARNACLE_CHEW_SMALL_THINGS)	// Chewing small things, like a headcrab

// Schedules

AI_END_CUSTOM_NPC()
