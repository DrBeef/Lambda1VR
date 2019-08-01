

#include "Matrices.h"
#include "hud.h"
#include "cl_util.h"
#include "r_studioint.h"
#include "ref_params.h"
#include "vr_helper.h"


#define WEAPON_NONE				0
#define WEAPON_CROWBAR			1
#define	WEAPON_GLOCK			2
#define WEAPON_PYTHON			3
#define WEAPON_MP5				4
#define WEAPON_CHAINGUN			5
#define WEAPON_CROSSBOW			6
#define WEAPON_SHOTGUN			7
#define WEAPON_RPG				8
#define WEAPON_GAUSS			9
#define WEAPON_EGON				10
#define WEAPON_HORNETGUN		11
#define WEAPON_HANDGRENADE		12
#define WEAPON_TRIPMINE			13
#define	WEAPON_SATCHEL			14
#define	WEAPON_SNARK			15

#ifndef MAX_COMMAND_SIZE
#define MAX_COMMAND_SIZE 256
#endif

bool bIsMultiplayer( void );

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

void VRHelper::UpdateCurrentWeapon( long currentWeaponID )
{
	positions.currentWeapon = currentWeaponID;
}

void VRHelper::UpdateGunPosition(struct ref_params_s* pparams)
{
	cl_entity_t *viewent = gEngfuncs.GetViewModel();
	if (viewent != nullptr)
	{
		cvar_s	*vr_worldscale = gEngfuncs.pfnGetCvarPointer( "vr_worldscale" );
		cvar_s	*vr_weapon_pitchadjust = gEngfuncs.pfnGetCvarPointer( "vr_weapon_pitchadjust" );

		Vector3 weaponOriginInVRSpace = pparams->weapon.org;
		//
		//(left/right, forward/backward, up/down)
		Vector weaponOriginInRelativeHLSpace(-weaponOriginInVRSpace.z * vr_worldscale->value, -weaponOriginInVRSpace.x * vr_worldscale->value, weaponOriginInVRSpace.y * vr_worldscale->value);

		cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
		Vector clientPosition = pparams->vieworg;
		positions.weapon.offset =  Vector(weaponOriginInRelativeHLSpace.x, weaponOriginInRelativeHLSpace.y, clientPosition.z + weaponOriginInRelativeHLSpace.z);

		Vector weaponOrigin = clientPosition + weaponOriginInRelativeHLSpace;
		VectorCopy(weaponOrigin, viewent->origin);
		VectorCopy(weaponOrigin, viewent->curstate.origin);
		VectorCopy(weaponOrigin, viewent->latched.prevorigin);


		positions.weapon.angles = pparams->weapon.angles;
		viewent->angles = pparams->weapon.angles;
		switch (positions.currentWeapon)
		{
			//Incline crowbar a bit closer to natural hand location (not too far, that causes weirdness)
			case WEAPON_CROWBAR:
				viewent->angles[0] -= (vr_weapon_pitchadjust->value - 20.0f);
				break;
				//These just need the adjustment removing, they aren't aimed weapons
			case WEAPON_HANDGRENADE:
			case WEAPON_TRIPMINE:
			case WEAPON_SATCHEL:
			case WEAPON_SNARK:
				viewent->angles[0] -= vr_weapon_pitchadjust->value;
				break;
			default:
				//Everything else is adjusted correctly
				break;
		}


		VectorCopy(viewent->angles, viewent->curstate.angles);
		VectorCopy(viewent->angles, viewent->latched.prevangles);


		Vector velocityInVRSpace = pparams->weapon.velocity;
		Vector velocityInHLSpace(-velocityInVRSpace.z * vr_worldscale->value, -velocityInVRSpace.x * vr_worldscale->value, velocityInVRSpace.y * vr_worldscale->value);
		viewent->curstate.velocity = velocityInHLSpace;
		positions.weapon.velocity = velocityInHLSpace;
	}
}

void VRHelper::SendPositionUpdateToServer()
{
	Vector hmdOffset; // Not required
	Vector weaponOffset = positions.weapon.offset;
	Vector weaponAngles = positions.weapon.angles;
	Vector weaponVelocity = positions.weapon.velocity;

	if (!bIsMultiplayer()) {
        // void CBasePlayer::UpdateVRRelatedPositions(const Vector & hmdOffset, const Vector & weaponoffset, const Vector & weaponAngles, const Vector & weaponVelocity)
        char cmd[MAX_COMMAND_SIZE] = {0};
        sprintf(cmd, "updatevr %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f",
                hmdOffset.x, hmdOffset.y, hmdOffset.z,
                weaponOffset.x, weaponOffset.y, weaponOffset.z,
                weaponAngles.x, weaponAngles.y, weaponAngles.z,
                weaponVelocity.x, weaponVelocity.y, weaponVelocity.z
        );

        gEngfuncs.pfnClientCmd(cmd);
    }
}