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
#include "Slider.h"
#include "Bitmap.h"
#include "PicButton.h"
#include "CheckBox.h"
#include "SpinControl.h"
#include "StringArrayModel.h"

#define ART_BANNER			"gfx/shell/head_advoptions"

class CMenuConsoleCommands : public CMenuFramework
{
public:
	typedef CMenuFramework BaseClass;

	CMenuConsoleCommands() : CMenuFramework("CMenuConsoleCommands") { }

private:
	void _Init() override;

	void ExecuteCommand( void *pExtra );
};

static CMenuConsoleCommands		uiConsoleCommands;


static int numCommands = 0;
static char commandTitles[128][128];
static char commands[128][128];

void UI_LoadCustomConsoleCommands( void )
{
	numCommands = 0;
	char *afile = (char *)EngFuncs::COM_LoadFile( "commands.lst", NULL );
	char *pfile = afile;
	char token[128];

	if( !afile )
		return;

	while(( pfile = EngFuncs::COM_ParseFile( pfile, token )) != NULL )
	{
		strcpy(commandTitles[numCommands], token);
		pfile = EngFuncs::COM_ParseFile( pfile, token );

		if (pfile == NULL)
			break;

		strcpy(commands[numCommands++], token);
	}

	EngFuncs::COM_FreeFile( afile );
}

/*
=================
CMenuConsoleCommands::Init
=================
*/
void CMenuConsoleCommands::_Init( void )
{
	banner.SetPicture( ART_BANNER );

    AddItem( background );
    AddItem( banner );

	UI_LoadCustomConsoleCommands();
	for (int i = 0; i < numCommands; ++i)
	{
		CMenuPicButton *button = AddButton(commandTitles[i], commandTitles[i], PC_CONSOLE, MenuCb( &CMenuConsoleCommands::ExecuteCommand ));
		button->onActivated.pExtra = commands[i];
	}

	AddButton( "Done",     "Go back to the Main menu",
			   PC_DONE, VoidCb( &CMenuConsoleCommands::Hide ), QMF_NOTIFY );
}

void CMenuConsoleCommands::ExecuteCommand( void *pExtra )
{
    const char *command = (const char *)pExtra;
    if( command[0] )
    {
        EngFuncs::ClientCmd(TRUE, command);
    }
}


/*
=================
UI_ConsoleCommands_Precache
=================
*/
void UI_ConsoleCommands_Precache( void )
{
	EngFuncs::PIC_Load( ART_BANNER );
}

/*
=================
UI_ConsoleCommands_Menu
=================
*/
void UI_ConsoleCommands_Menu( void )
{
	uiConsoleCommands.Show();
}
ADD_MENU( menu_ConsoleCommands, UI_ConsoleCommands_Precache, UI_ConsoleCommands_Menu );
