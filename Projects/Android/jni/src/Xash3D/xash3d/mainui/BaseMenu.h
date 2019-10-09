/*
basemenu.h - menu basic header 
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef BASEMENU_H
#define BASEMENU_H
#ifdef CS16CLIENT
#include "../public/cl_dll/IGameClientExports.h"
#endif
#include "enginecallback_menu.h"
#include "keydefs.h"
#include "Primitive.h"
#include "EventSystem.h"
#include "Utils.h"
#include "FontManager.h"
#include "BtnsBMPTable.h"

#define UI_MAX_MENUDEPTH		64
#define UI_MAX_MENUITEMS		64

#define UI_PULSE_DIVISOR		75.0f

#define UI_OUTLINE_WIDTH		uiStatic.outlineWidth	// outline thickness

#define UI_MAXGAMES			1024	// slots for savegame/demos
#define UI_MAX_BGMAPS		32

#define MAX_HINT_TEXT		512

// menu buttons dims
#define UI_BUTTONS_WIDTH		240
#define UI_BUTTONS_HEIGHT		40

#define UI_DESCEND			"gfx/shell/down"
#define UI_ASCEND			"gfx/shell/up"
#define UI_LEFTARROW		"gfx/shell/larrowdefault"
#define UI_LEFTARROWFOCUS		"gfx/shell/larrowflyover"
#define UI_LEFTARROWPRESSED		"gfx/shell/larrowpressed"
#define UI_RIGHTARROW		"gfx/shell/rarrowdefault"
#define UI_RIGHTARROWFOCUS		"gfx/shell/rarrowflyover"
#define UI_RIGHTARROWPRESSED		"gfx/shell/rarrowpressed"
#define UI_UPARROW			"gfx/shell/uparrowd"
#define UI_UPARROWFOCUS		"gfx/shell/uparrowf"
#define UI_UPARROWPRESSED		"gfx/shell/uparrowp"
#define UI_DOWNARROW		"gfx/shell/dnarrowd"
#define UI_DOWNARROWFOCUS		"gfx/shell/dnarrowf"
#define UI_DOWNARROWPRESSED		"gfx/shell/dnarrowp"


// =====================================================================
// Main menu interface

#ifdef CS16CLIENT
extern IGameClientExports *g_pClient;
#endif
extern cvar_t	*ui_precache;
extern cvar_t	*ui_showmodels;
extern cvar_t   *ui_show_window_stack;
extern cvar_t	*ui_borderclip;

class CMenuBaseWindow;

class windowStack_t
{
public:
	CMenuBaseWindow *rootActive; // current active fullscreen holder(menu framework)
	CMenuBaseWindow *menuActive; // current active window
	CMenuBaseWindow *prevMenu;   // previous active window
	CMenuBaseWindow *menuStack[UI_MAX_MENUDEPTH];
	int menuDepth;
	int rootPosition;

	bool IsActive( void ) { return menuDepth > 0; }
	void VidInit( bool firstTime );
	void Update( void );
	void KeyEvent( int key, int down );
	void CharEvent( int ch );
	void MouseEvent( int x, int y );

	void InputMethodResized( void );

	void Close( void )
	{
		menuActive = NULL;
		menuDepth = 0;
		rootPosition = 0;
	}
};

typedef struct
{
	windowStack_t menu;
	windowStack_t client; // separate window stack for client windows
	char	bgmaps[UI_MAX_BGMAPS][80];
	int		bgmapcount;

	HIMAGE	hFont;		// legacy qfont

	HFont hDefaultFont;
	HFont hSmallFont;
	HFont hBigFont;
	HFont hConsoleFont;
	HFont hBoldFont;
#ifdef MAINUI_RENDER_PICBUTTON_TEXT
	HFont hLightBlur;
	HFont hHeavyBlur;
#endif
	bool	m_fDemosPlayed;
	bool	m_fNoOldBackground;
	int 	m_iOldMenuDepth;

	float	scaleX;
	float	scaleY;
	int		outlineWidth;

	int		cursorX;
	int		cursorY;
	int		realTime;
	int		firstDraw;
	float	enterSound;
	int		mouseInRect;
	int		hideCursor;
	int		framecount;	// how many frames menu visible
	int		initialized;

	// btns_main.bmp stuff
	HIMAGE	buttonsPics[PC_BUTTONCOUNT];

	int		buttons_width;	// btns_main.bmp global width
	int		buttons_height;	// per one button with all states (inactive, focus, pressed)

	int		buttons_draw_width;	// scaled image what we drawing
	int		buttons_draw_height;
	int		width;
	bool	textInput;
	bool	enableAlphaFactor;
	float	alphaFactor;
	int xOffset, yOffset;

	bool isForkedEngine;
	bool needMapListUpdate;
} uiStatic_t;

extern float	cursorDY;			// use for touch scroll
extern bool g_bCursorDown;
extern uiStatic_t		uiStatic;

#define DLG_X ((uiStatic.width - 640) / 2 - 192) // Dialogs are 640px in width

extern const char		*uiSoundIn;
extern const char		*uiSoundRollOver;
extern const char		*uiSoundOut;
extern const char		*uiSoundKey;
extern const char		*uiSoundRemoveKey;
extern const char		*uiSoundLaunch;
extern const char		*uiSoundBuzz;
extern const char		*uiSoundGlow;
extern const char		*uiSoundMove;
extern const char		*uiSoundNull;

extern unsigned int	uiColorHelp;
extern unsigned int	uiPromptBgColor;
extern unsigned int	uiPromptTextColor;
extern unsigned int	uiPromptFocusColor;
extern unsigned int	uiInputTextColor;
extern unsigned int	uiInputBgColor;
extern unsigned int	uiInputFgColor;
extern unsigned int	uiColorWhite;
extern unsigned int	uiColorDkGrey;
extern unsigned int	uiColorBlack;

// TODO: Move it under namespace?

bool UI_IsXashFWGS( void );

void UI_ScaleCoords( int *x, int *y, int *w, int *h );
void UI_ScaleCoords( int &x, int &y, int &w, int &h );
void UI_ScaleCoords( int &x, int &y );

bool UI_CursorInRect( int x, int y, int w, int h );

// temporarily override alpha by multiplying given alpha and factor
void UI_EnableAlphaFactor( float a );
void UI_DisableAlphaFactor();

enum ETextFlags
{
	ETF_FORCECOL    = BIT( 0 ),
	ETF_SHADOW      = BIT( 1 ),
	ETF_NOSIZELIMIT = BIT( 2 ),
	ETF_ADDITIVE    = BIT( 3 )
};

int  UI_DrawString( HFont font, int x, int y, int w, int h, const char *str, const unsigned int col, int charH, uint justify, uint flags = 0 );
inline int UI_DrawString( HFont font, Point pos, Size size, const char *str, const unsigned int col, int charH, uint justify, uint flags = 0 )
{
	return UI_DrawString( font, pos.x, pos.y, size.w, size.h, str, col, charH, justify, flags );
}

void UI_DrawPic( int x, int y, int w, int h, const unsigned int color, const char *pic );
void UI_DrawPicAdditive( int x, int y, int w, int h, const unsigned int color, const char *pic );
void UI_DrawPicTrans( int x, int y, int width, int height, const unsigned int color, const char *pic );
void UI_DrawPicHoles( int x, int y, int width, int height, const unsigned int color, const char *pic );
void UI_FillRect( int x, int y, int w, int h, const unsigned int color );
void UI_DrawRectangleExt(int in_x, int in_y, int in_w, int in_h, const unsigned int color, int outlineWidth, int flag  = QM_TOP | QM_BOTTOM | QM_LEFT | QM_RIGHT);

inline void UI_DrawRectangle( int x, int y, int w, int h, const unsigned int color, int flag  = QM_TOP | QM_BOTTOM | QM_LEFT | QM_RIGHT)
{
	UI_DrawRectangleExt( x, y, w, h, color, uiStatic.outlineWidth, flag );
}

inline bool UI_CursorInRect( Point pos, Size size )
{
	return UI_CursorInRect( pos.x, pos.y, size.w, size.h );
}
inline void UI_DrawPic( Point pos, Size size, const unsigned int color, const char *pic )
{
	UI_DrawPic( pos.x, pos.y, size.w, size.h, color, pic );
}
inline void UI_DrawPicAdditive( Point pos, Size size, const unsigned int color, const char *pic )
{
	UI_DrawPicAdditive( pos.x, pos.y, size.w, size.h, color, pic );
}
inline void UI_DrawPicTrans( Point pos, Size size, const unsigned int color, const char *pic )
{
	UI_DrawPicTrans( pos.x, pos.y, size.w, size.h, color, pic );
}
inline void UI_DrawPicHoles( Point pos, Size size, const unsigned int color, const char *pic )
{
	UI_DrawPicHoles( pos.x, pos.y, size.w, size.h, color, pic );
}
inline void UI_FillRect( Point pos, Size size, const unsigned int color )
{
	UI_FillRect( pos.x, pos.y, size.w, size.h, color );
}
inline void UI_DrawRectangle( Point pos, Size size, const unsigned int color, int flag = QM_TOP | QM_BOTTOM | QM_LEFT | QM_RIGHT )
{
	UI_DrawRectangleExt( pos.x, pos.y, size.w, size.h, color, uiStatic.outlineWidth, flag );
}
inline void UI_DrawRectangleExt( Point pos, Size size, const unsigned int color, int outlineWidth, int flag  = QM_TOP | QM_BOTTOM | QM_LEFT | QM_RIGHT )
{
	UI_DrawRectangleExt( pos.x, pos.y, size.w, size.h, color, outlineWidth, flag );
}

void UI_StartSound( const char *sound );
void UI_LoadBmpButtons( void );

int UI_CreditsActive( void );
void UI_DrawFinalCredits( void );

void UI_CloseMenu( void );

// SCR support
void UI_LoadScriptConfig( void );

class CMenuEntry
{
public:
	CMenuEntry( const char *cmd, void (*pfnPrecache)( void ), void (*pfnShow)( void ));
	const char *m_szCommand;
	void (*m_pfnPrecache)( void );
	void (*m_pfnShow)( void );
	CMenuEntry *m_pNext;
};

#define ADD_MENU( cmd, precachefunc, showfunc ) \
	static CMenuEntry cmd( #cmd, precachefunc, showfunc )

#define ADD_COMMAND( cmd, showfunc ) \
	static CMenuEntry cmd( #cmd, NULL, showfunc )

// Menus
void UI_Main_Menu( void );
void UI_NewGame_Menu( void );
void UI_LoadGame_Menu( void );
void UI_SaveGame_Menu( void );
void UI_SaveLoad_Menu( void );
void UI_MultiPlayer_Menu( void );
void UI_Options_Menu( void );
void UI_InternetGames_Menu( void );
void UI_LanGame_Menu( void );
void UI_ServerBrowser_Menu( void );
void UI_PlayerSetup_Menu( void );
void UI_Controls_Menu( void );
void UI_AdvControls_Menu( void );
void UI_GameOptions_Menu( void );
void UI_CreateGame_Menu( void );
void UI_Audio_Menu( void );
void UI_Video_Menu( void );
void UI_VidOptions_Menu( void );
void UI_VidModes_Menu( void );
void UI_NoClip_Btn( void );
void UI_God_Btn( void );
void UI_GiveSuit_Btn( void );
void UI_GiveCrowbar_Btn( void );
void UI_CustomGame_Menu( void );
void UI_Credits_Menu( void );
void UI_Touch_Menu( void );
void UI_TouchOptions_Menu( void );
void UI_TouchButtons_Menu( void );
void UI_TouchEdit_Menu( void );
void UI_FileDialog_Menu( void );
void UI_TouchButtons_AddButtonToList( const char *name, const char *texture, const char *command, unsigned char *color, int flags );
void UI_TouchButtons_GetButtonList();
void UI_GamePad_Menu( void );
void UI_Zoo_Menu( void );

bool UI_AdvUserOptions_IsAvailable( void );
void UI_AdvUserOptions_Menu( void );
bool UI_AdvServerOptions_IsAvailable( void );
void UI_AdvServerOptions_Menu( void );
void UI_InputDevices_Menu( void );

void UI_OpenUpdatePage(bool engine , bool preferstore);

// time
double Sys_DoubleTime( void );

//
//-----------------------------------------------------
//
class CMenu
{
public:
	// Game information
	GAMEINFO		m_gameinfo;
};

typedef struct
{
	char patterns[32][256];
	int npatterns;
	char result[256];
	bool valid;
	void ( * callback )( bool success );
	bool preview;
} uiFileDialogGlobal_t;

extern uiFileDialogGlobal_t uiFileDialogGlobal;

extern CMenu gMenu;

#endif // BASEMENU_H
