#pragma once

#include "Matrices.h"
#include "util_vector.h"

class Positions
{
public:
	// output
	vec3_t		vieworg;
	vec3_t		viewangles;

	//controllers
	struct controller_t {
		vec3_t org;
		vec3_t velocity;
		vec3_t angles;
	} flashlight, weapon;
};

class VRHelper
{
public:
	VRHelper();
	~VRHelper();

	void Init();

	bool UpdatePositions(struct ref_params_s* pparams);

	void GetViewAngles(float * angles);

	void TestRenderControllerPosition();

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
