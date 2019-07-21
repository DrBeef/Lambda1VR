
#include "Matrices.h"
#include "hud.h"
#include "cl_util.h"
#include "r_studioint.h"
#include "ref_params.h"
#include "vr_helper.h"

#ifndef MAX_COMMAND_SIZE
#define MAX_COMMAND_SIZE 256
#endif


VRHelper::VRHelper()
{

}

VRHelper::~VRHelper()
{

}

void VRHelper::Init()
{

}

void VRHelper::Exit(const char* lpErrorMessage)
{
	gEngfuncs.pfnClientCmd("quit");
}

bool VRHelper::UpdatePositions(struct ref_params_s* pparams)
{
	positions.vieworg = pparams->vieworg;
	positions.viewangles = pparams->viewangles;

	UpdateGunPosition(pparams);

	SendPositionUpdateToServer();

	return true;
}

void VRHelper::GetViewAngles(float * angles)
{
	angles[0] = positions.viewangles[0];
	angles[1] = positions.viewangles[1];
	angles[2] = positions.viewangles[2];
}

void VRHelper::UpdateGunPosition(struct ref_params_s* pparams)
{
	cl_entity_t *viewent = gEngfuncs.GetViewModel();
	if (viewent != nullptr)
	{
		cvar_s	*vr_worldscale = gEngfuncs.pfnGetCvarPointer( "vr_worldscale" );

		Vector3 originInVRSpace = pparams->weapon.org;
		Vector originInRelativeHLSpace(-originInVRSpace.z * vr_worldscale->value, -originInVRSpace.x * vr_worldscale->value, originInVRSpace.y * vr_worldscale->value);

		cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
		Vector clientPosition = pparams->vieworg;
		Vector originInHLSpace = clientPosition + originInRelativeHLSpace;

		VectorCopy(originInHLSpace, viewent->origin);
		VectorCopy(originInHLSpace, viewent->curstate.origin);
		VectorCopy(originInHLSpace, viewent->latched.prevorigin);


		viewent->angles = pparams->weapon.angles;
		VectorCopy(viewent->angles, viewent->curstate.angles);
		VectorCopy(viewent->angles, viewent->latched.prevangles);


		Vector velocityInVRSpace = pparams->weapon.velocity;
		Vector velocityInHLSpace(-velocityInVRSpace.z * vr_worldscale->value, -velocityInVRSpace.x * vr_worldscale->value, velocityInVRSpace.y * vr_worldscale->value);
		viewent->curstate.velocity = velocityInHLSpace;
	}
}

void VRHelper::SendPositionUpdateToServer()
{
	cl_entity_t *viewent = gEngfuncs.GetViewModel();

	Vector hmdOffset; // TODO
	Vector weaponOrigin = viewent ? viewent->curstate.origin : Vector();
	Vector weaponOffset = weaponOrigin - positions.vieworg;
	Vector weaponAngles = viewent ? viewent->curstate.angles : Vector();
	Vector weaponVelocity = viewent ? viewent->curstate.velocity : Vector();

	// void CBasePlayer::UpdateVRRelatedPositions(const Vector & hmdOffset, const Vector & weaponoffset, const Vector & weaponAngles, const Vector & weaponVelocity)
	char cmd[MAX_COMMAND_SIZE] = { 0 };
	sprintf(cmd, "updatevr %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f",
		hmdOffset.x, hmdOffset.y, hmdOffset.z,
		weaponOffset.x, weaponOffset.y, weaponOffset.z,
		weaponAngles.x, weaponAngles.y, weaponAngles.z,
		weaponVelocity.x, weaponVelocity.y, weaponVelocity.z
	);

	gEngfuncs.pfnClientCmd(cmd);
}
