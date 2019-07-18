
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
	//GB - Changing to 72 to see if it fixes physics
	gEngfuncs.pfnClientCmd("fps_max 72");
	//gEngfuncs.pfnClientCmd("fps_override 1");
	gEngfuncs.pfnClientCmd("gl_vsync 0");
	gEngfuncs.pfnClientCmd("default_fov 110");
	//gEngfuncs.pfnClientCmd("firstperson");

	if (isInMenu)
	{
		//vrHelper->CaptureMenuTexture();
		//CaptureCurrentScreenToTexture(vrGLMenuTexture);
	}
	else
	{
		//CaptureCurrentScreenToTexture(vrGLHUDTexture);
		isInMenu = true;
	}

}


void VRRenderer::CalcRefdef(struct ref_params_s* pparams)
{
	vrHelper->UpdatePositions(pparams);
	// Update player viewangles from HMD pose
	gEngfuncs.SetViewAngles(pparams->viewangles);
}

void VRRenderer::DrawNormal()
{
}

void VRRenderer::DrawTransparent()
{
}

void VRRenderer::InterceptHUDRedraw(float time, int intermission)
{
	isInMenu = false;
	gHUD.Redraw(time, intermission);
}

void VRRenderer::GetViewAngles(float * angles)
{
	vrHelper->GetViewAngles(angles);
}
