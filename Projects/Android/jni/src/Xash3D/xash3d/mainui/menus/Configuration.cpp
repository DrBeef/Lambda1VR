/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "Framework.h"
#include "Bitmap.h"
#include "PicButton.h"
#include "YesNoMessageBox.h"
#include "keydefs.h"
#include "TabView.h"

#define ART_BANNER	     	"gfx/shell/head_config"

class CMenuOptions: public CMenuFramework
{
private:
	void _Init( void ) override;

public:
	typedef CMenuFramework BaseClass;
	CMenuOptions() : CMenuFramework("CMenuOptions") { }

	// update dialog
	CMenuYesNoMessageBox msgBox;
};

static CMenuOptions	uiOptions;

/*
=================
CMenuOptions::Init
=================
*/
void CMenuOptions::_Init( void )
{
	banner.SetPicture( ART_BANNER );

	msgBox.SetMessage( "Check the Internet for updates?" );
	SET_EVENT( msgBox.onPositive, UI_OpenUpdatePage( false, true ) );

	msgBox.Link( this );

	AddItem( background );
	AddItem( banner );
#ifndef VR
	AddButton( "Controls", "Change keyboard and mouse settings",
		PC_CONTROLS, UI_Controls_Menu, QMF_NOTIFY );
#endif
	AddButton( "Audio",    "Change sound volume and quality",
		PC_AUDIO, UI_Audio_Menu, QMF_NOTIFY );
	AddButton( "Video",    "Change screen size, video mode and gamma",
		PC_VIDEO, UI_Video_Menu, QMF_NOTIFY );

	// Cheats
	AddButton( "No Clip", "No Clip", PC_CONSOLE, UI_NoClip_Btn, QMF_NOTIFY );
	AddButton( "God", "God", PC_CONSOLE, UI_God_Btn, QMF_NOTIFY );
	AddButton( "Give Suit", "Give Suit", PC_CONSOLE, UI_GiveSuit_Btn, QMF_NOTIFY );
	AddButton( "Give Crowbar", "Give Crowbar", PC_CONSOLE, UI_GiveCrowbar_Btn, QMF_NOTIFY );
	
#ifndef VR
	AddButton( "Touch",    "Change touch settings and buttons",
		PC_TOUCH, UI_Touch_Menu, QMF_NOTIFY );
	AddButton( "Gamepad",  "Change gamepad axis and button settings",
		PC_GAMEPAD, UI_GamePad_Menu, QMF_NOTIFY );
	AddButton( "Update",   "Check for updates",
		PC_UPDATE, msgBox.MakeOpenEvent(), QMF_NOTIFY );
#endif
	AddButton( "Done",     "Go back to the Main menu",
		PC_DONE, VoidCb( &CMenuOptions::Hide ), QMF_NOTIFY );
}

/*
=================
CMenuOptions::Precache
=================
*/
void UI_Options_Precache( void )
{
	EngFuncs::PIC_Load( ART_BANNER );
}

/*
=================
CMenuOptions::Menu
=================
*/

void UI_NoClip_Btn( void )
{
	EngFuncs::ClientCmd(TRUE, "noclip");
}

void UI_God_Btn( void )
{
	EngFuncs::ClientCmd(TRUE, "god");
}

void UI_GiveSuit_Btn( void )
{
	EngFuncs::ClientCmd(TRUE, "give item_suit");
}

void UI_GiveCrowbar_Btn( void )
{
	EngFuncs::ClientCmd(TRUE, "give weapon_crowbar");
}

void UI_Options_Menu( void )
{
	uiOptions.Show();
}
ADD_MENU( menu_options, UI_Options_Precache, UI_Options_Menu );
