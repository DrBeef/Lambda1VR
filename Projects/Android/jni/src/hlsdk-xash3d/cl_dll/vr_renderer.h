#pragma once

class VRHelper;

class VRRenderer
{
public:
	VRRenderer();
	~VRRenderer();

	void Init();
	void VidInit();

	void Frame(double time);
	void CalcRefdef(struct ref_params_s* pparams);


	void InterceptHUDRedraw(float time, int intermission);

	void GetViewAngles(float * angles);

private:
	VRHelper *vrHelper = nullptr;
};

extern VRRenderer gVRRenderer;
