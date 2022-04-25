//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "c_ai_basenpc.h"
#include "engine/ivmodelinfo.h"
#include "rope_physics.h"
#include "materialsystem/imaterialsystem.h"
#include "fx_line.h"
#include "engine/ivdebugoverlay.h"
#include "bone_setup.h"
#include "model_types.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_NPC_Barnacle : public C_AI_BaseNPC
{
public:

	DECLARE_CLASS(C_NPC_Barnacle, C_AI_BaseNPC);
	DECLARE_CLIENTCLASS();

	C_NPC_Barnacle(void) {};

	virtual void GetRenderBounds(Vector &theMins, Vector &theMaxs)
	{
		theMins = theMaxs = Vector();
	}

	// Purpose: Initialize absmin & absmax to the appropriate box
	virtual void ComputeWorldSpaceSurroundingBox(Vector *pVecWorldMins, Vector *pVecWorldMaxs)
	{
		*pVecWorldMins = *pVecWorldMaxs = Vector();
	}

	void	OnDataChanged(DataUpdateType_t updateType) {};

protected:
	Vector	m_vecTipPrevious;
	Vector	m_vecRoot;
	Vector	m_vecTip;
	Vector  m_vecTipDrawOffset;

private:
	// Tongue points
	float	m_flAltitude;

private:
	C_NPC_Barnacle(const C_NPC_Barnacle &); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_NPC_Barnacle, DT_Barnacle, CNPC_Barnacle)
RecvPropFloat(RECVINFO(m_flAltitude)),
RecvPropVector(RECVINFO(m_vecRoot)),
RecvPropVector(RECVINFO(m_vecTip), 0, [] (const CRecvProxyData *pData, void *pStruct, void *pOut) -> void {}),
RecvPropVector(RECVINFO(m_vecTipDrawOffset)),
END_RECV_TABLE()
