#pragma once

#include "Matrices.h"
#include "util_vector.h"

class Positions
{
public:
	// output
	Vector		vieworg;
	Vector		viewangles;

	//controllers
	struct controller_t {
		Vector offset;
		Vector velocity;
		Vector angles;
	} flashlight, weapon;

	//Feels hacky
	long currentWeapon;
};

class VRHelper
{
public:
	VRHelper();
	~VRHelper();

	void Init();

	bool UpdatePositions(struct ref_params_s* pparams);
	void UpdateCurrentWeapon( long currentWeaponID );

	void GetViewAngles(float * angles);

private:

	void Exit(const char* lpErrorMessage = nullptr);

	void UpdateGunPosition(struct ref_params_s* pparams);
	void SendPositionUpdateToServer();

/*
	Matrix4 GetHMDMatrixProjectionEye(vr::EVREye eEye);
	Matrix4 GetHMDMatrixPoseEye(vr::EVREye eEye);
	Matrix4 ConvertSteamVRMatrixToMatrix4(const vr::HmdMatrix34_t &matPose);
	Matrix4 ConvertSteamVRMatrixToMatrix4(const vr::HmdMatrix44_t &matPose);

	Vector GetHLViewAnglesFromVRMatrix(const Matrix4 &mat);
	Vector GetHLAnglesFromVRMatrix(const Matrix4 &mat);
*/
	Positions positions;

	//bool isVRRoomScale = true;
};
