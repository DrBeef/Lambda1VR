/*
BaseWindow.h -- base client menu window
Copyright (C) 2018 a1batross

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#include "BaseMenu.h"
#include "BaseClientWindow.h"

CMenuBaseClientWindow::CMenuBaseClientWindow( const  char *name ) :
	BaseClass( name )
{
	m_pStack = &uiStatic.client;
}

const char *CMenuBaseClientWindow::Key(int key, int down)
{
	if( down )
	{
		if( UI::Key::IsEscape( key ))
		{
			EngFuncs::KEY_SetDest( KEY_GAME ); // set engine states before "escape"
			EngFuncs::ClientCmd( FALSE, "escape\n" );
			return uiSoundNull;
		}
		else if( key == '`' )
		{
			EngFuncs::KEY_SetDest( KEY_CONSOLE );
		}
	}

	return BaseClass::Key( key, down );

}


