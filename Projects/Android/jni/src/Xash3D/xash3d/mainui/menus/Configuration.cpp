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

	AddButton( "Audio",    "Change sound volume and quality",
		PC_AUDIO, UI_Audio_Menu, QMF_NOTIFY );
	AddButton( "Video",    "Change video settings",
		PC_VIDEO, UI_Video_Menu, QMF_NOTIFY );
	AddButton( "Controls", "Change game Control settings", PC_CONTROLS, UI_Controls_Menu, QMF_NOTIFY );
	AddButton( "Console Commands",    "Console Commands Menu",
		PC_CONSOLE, UI_ConsoleCommands_Menu, QMF_NOTIFY );

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

void UI_Options_Menu( void )
{
	uiOptions.Show();
}

ADD_MENU( menu_options, UI_Options_Precache, UI_Options_Menu );
