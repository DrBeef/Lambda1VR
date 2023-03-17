#include <common/common.h>
#include <common/cvardef.h>
#include <common/xash3d_types.h>

extern convar_t	*vr_snapturn_angle;
extern convar_t	*vr_reloadtimeoutms;
extern convar_t	*vr_positional_factor;
extern convar_t	*vr_walkdirection;
extern convar_t	*vr_weapon_pitchadjust;
extern convar_t	*vr_crowbar_pitchadjust;
extern convar_t	*vr_weapon_recoil;
extern convar_t	*vr_weapon_stabilised;
extern convar_t	*vr_lasersight;
extern convar_t	*vr_hmd_fov_x;
extern convar_t	*vr_hud_yoffset;
extern convar_t	*vr_refresh;
extern convar_t	*vr_control_scheme;
extern convar_t	*vr_enable_crouching;
extern convar_t	*vr_height_adjust;
extern convar_t	*vr_flashlight_model;
extern convar_t	*vr_hand_model;
extern convar_t	*vr_mirror_weapons;
extern convar_t	*vr_weapon_backface_culling;
extern convar_t	*vr_comfort_mask;
extern convar_t	*vr_controller_ladders;
extern convar_t	*vr_controller_tracking_haptic;
extern convar_t	*vr_highlight_actionables;
extern convar_t	*vr_headtorch;
extern convar_t	*vr_quick_crouchjump;
extern convar_t *vr_gesture_triggered_use;
extern convar_t *vr_use_gesture_boundary;

//Used and updated continuously during rendering
extern convar_t	*vr_stereo_side;