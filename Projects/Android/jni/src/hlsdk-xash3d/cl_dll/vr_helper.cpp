
#include "Matrices.h"
#include "hud.h"
#include "cl_util.h"
#include "r_studioint.h"
#include "ref_params.h"
#include "vr_helper.h"

#ifndef MAX_COMMAND_SIZE
#define MAX_COMMAND_SIZE 256
#endif

const Vector3 HL_TO_VR(1.44f / 10.f, 2.0f / 10.f, 1.44f / 10.f);
const Vector3 VR_TO_HL(1.f / HL_TO_VR.x, 1.f / HL_TO_VR.y, 1.f / HL_TO_VR.z);


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
/*
Matrix4 VRHelper::GetHMDMatrixPoseEye(vr::EVREye nEye)
{
	return ConvertSteamVRMatrixToMatrix4(vrSystem->GetEyeToHeadTransform(nEye)).invert();
}

Matrix4 VRHelper::ConvertSteamVRMatrixToMatrix4(const vr::HmdMatrix34_t &mat)
{
	return Matrix4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], 0.0f,
		mat.m[0][1], mat.m[1][1], mat.m[2][1], 0.0f,
		mat.m[0][2], mat.m[1][2], mat.m[2][2], 0.0f,
		mat.m[0][3], mat.m[1][3], mat.m[2][3], 0.1f
	);
}

Matrix4 VRHelper::ConvertSteamVRMatrixToMatrix4(const vr::HmdMatrix44_t &mat)
{
	return Matrix4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
		mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
		mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
	);
}

extern void VectorAngles(const float *forward, float *angles);

Vector VRHelper::GetHLViewAnglesFromVRMatrix(const Matrix4 &mat)
{
	Vector4 v1 = mat * Vector4(1, 0, 0, 0);
	Vector4 v2 = mat * Vector4(0, 1, 0, 0);
	Vector4 v3 = mat * Vector4(0, 0, 1, 0);
	v1.normalize();
	v2.normalize();
	v3.normalize();
	Vector angles;
	VectorAngles(Vector(-v1.z, -v2.z, -v3.z), angles);
	angles.x = 360.f - angles.x;	// viewangles pitch is inverted
	return angles;
}

Vector VRHelper::GetHLAnglesFromVRMatrix(const Matrix4 &mat)
{
	Vector4 forwardInVRSpace = mat * Vector4(0, 0, -1, 0);
	Vector4 rightInVRSpace = mat * Vector4(1, 0, 0, 0);
	Vector4 upInVRSpace = mat * Vector4(0, 1, 0, 0);

	Vector forward(forwardInVRSpace.x, -forwardInVRSpace.z, forwardInVRSpace.y);
	Vector right(rightInVRSpace.x, -rightInVRSpace.z, rightInVRSpace.y);
	Vector up(upInVRSpace.x, -upInVRSpace.z, upInVRSpace.y);

	forward.Normalize();
	right.Normalize();
	up.Normalize();

	Vector angles;
	GetAnglesFromVectors(forward, right, up, angles);
	angles.x = 360.f - angles.x;
	return angles;
}

Matrix4 GetModelViewMatrixFromAbsoluteTrackingMatrix(Matrix4 &absoluteTrackingMatrix, Vector translate)
{
	Matrix4 hlToVRScaleMatrix;
	hlToVRScaleMatrix[0] = HL_TO_VR.x;
	hlToVRScaleMatrix[5] = HL_TO_VR.y;
	hlToVRScaleMatrix[10] = HL_TO_VR.z;

	Matrix4 hlToVRTranslateMatrix;
	hlToVRTranslateMatrix[12] = translate.x;
	hlToVRTranslateMatrix[13] = translate.y;
	hlToVRTranslateMatrix[14] = translate.z;

	Matrix4 switchYAndZTransitionMatrix;
	switchYAndZTransitionMatrix[5] = 0;
	switchYAndZTransitionMatrix[6] = -1;
	switchYAndZTransitionMatrix[9] = 1;
	switchYAndZTransitionMatrix[10] = 0;

	Matrix4 modelViewMatrix = absoluteTrackingMatrix * hlToVRScaleMatrix * switchYAndZTransitionMatrix * hlToVRTranslateMatrix;
	modelViewMatrix.scale(10);
	return modelViewMatrix;
}
*/

bool VRHelper::UpdatePositions(struct ref_params_s* pparams)
{
	positions.vieworg = pparams->vieworg;
	positions.viewangles = pparams->viewangles;
	//positions.flashlight = pparams->flashlight;
	//positions.weapon = pparams->weapon;

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
        //Vector clientPositionInRelativeHLSpace(clientPosition.x * vr_worldscale->value, clientPosition.y * vr_worldscale->value, , clientPosition.z * vr_worldscale->value);

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
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	cl_entity_t *viewent = gEngfuncs.GetViewModel();
	Vector hmdOffset; // TODO
	Vector weaponOrigin = viewent ? viewent->curstate.origin : Vector();
	Vector weaponOffset = weaponOrigin - localPlayer->curstate.origin;
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
