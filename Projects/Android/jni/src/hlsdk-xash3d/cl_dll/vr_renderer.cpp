
#include "Matrices.h"

#include "hud.h"
#include "cl_util.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"

#include "entity_state.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "in_defs.h" // PITCH YAW ROLL
#include "pm_movevars.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "event_api.h"
#include "pmtrace.h"
#include "screenfade.h"
#include "shake.h"
#include "hltv.h"
#include "keydefs.h"

#include "vr_renderer.h"
#include "vr_helper.h"


VRRenderer gVRRenderer;



VRRenderer::VRRenderer()
{
	vrHelper = new VRHelper();
}

VRRenderer::~VRRenderer()
{
	delete vrHelper;
	vrHelper = nullptr;
}

void VRRenderer::Init()
{
	vrHelper->Init();
}

void VRRenderer::VidInit()
{
}

void VRRenderer::Frame(double time)
{
	// make sure these are always properly set
	gEngfuncs.pfnClientCmd("fps_max 144");
	gEngfuncs.pfnClientCmd("crosshair 0");

	float fov = gEngfuncs.pfnGetCvarFloat("vr_fov");
	char buffer[256];
	sprintf(buffer, "default_fov %f", fov);
    gEngfuncs.pfnClientCmd(buffer);

    //Force set these for now
    gEngfuncs.pfnClientCmd("cl_upspeed 150");
    gEngfuncs.pfnClientCmd("cl_forwardspeed 150");
    gEngfuncs.pfnClientCmd("cl_backspeed 150");
    gEngfuncs.pfnClientCmd("cl_sidespeed 150");
    gEngfuncs.pfnClientCmd("cl_movespeedkey 3.0");

    //Force client weapons
	gEngfuncs.pfnClientCmd("cl_lw 1");
}


void VRRenderer::CalcRefdef(struct ref_params_s* pparams)
{
	vrHelper->UpdatePositions(pparams);
	// Update player viewangles from HMD pose
	gEngfuncs.SetViewAngles(pparams->viewangles);
}

void VRRenderer::InterceptHUDRedraw(float time, int intermission)
{
	gHUD.Redraw(time, intermission);
}

void VRRenderer::InterceptHUDWeaponsPostThink( local_state_s *from, local_state_s *to )
{
	vrHelper->UpdateCurrentWeapon(to->client.m_iId);
}

void VRRenderer::GetViewAngles(float * angles)
{
	vrHelper->GetViewAngles(angles);
}
