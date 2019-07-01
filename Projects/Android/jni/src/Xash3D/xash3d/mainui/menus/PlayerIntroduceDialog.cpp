/*
Copyright (C) 2017 a1batross.
PlayerIntroduceDialog.cpp -- dialog intended to let player introduce themselves: enter nickname

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

#include "BaseWindow.h"
#include "PicButton.h"
#include "YesNoMessageBox.h"
#include "MessageBox.h"
#include "Field.h"
#include "PlayerIntroduceDialog.h"

static class CMenuPlayerIntroduceDialog : public CMenuYesNoMessageBox
{
public:
	CMenuPlayerIntroduceDialog() : CMenuYesNoMessageBox( false ), msgBox( true )
	{
	}

	void WriteOrDiscard();
	void _Init() override;
	const char *Key( int key, int down ) override;

	CMenuBaseWindow *pCaller;

private:
	CMenuField name;
	CMenuYesNoMessageBox msgBox;
} uiIntroduceDialog;

void CMenuPlayerIntroduceDialog::WriteOrDiscard()
{
	if( !UI::Names::CheckIsNameValid( name.GetBuffer() ) )
	{
		msgBox.Show();
	}
	else
	{
		name.WriteCvar();
		SaveAndPopMenu();
	}
}

const char *CMenuPlayerIntroduceDialog::Key( int key, int down )
{
	if( down && UI::Key::IsEscape( key ) )
	{
		return uiSoundNull; // handled
	}

	if( down && UI::Key::IsEnter( key ) && ItemAtCursor() == &name )
	{
		WriteOrDiscard();
	}

	return CMenuYesNoMessageBox::Key( key, down );
}

void CMenuPlayerIntroduceDialog::_Init()
{
	onPositive = VoidCb( &CMenuPlayerIntroduceDialog::WriteOrDiscard );
	SET_EVENT_MULTI( onNegative,
	{
		CMenuPlayerIntroduceDialog *self = (CMenuPlayerIntroduceDialog*)pSelf;
		self->Hide(); // hide ourselves first
		self->pCaller->Hide(); // hide our parent
	});

	SetMessage( "Enter your name:");

	name.bAllowColorstrings = true;
	name.SetRect( 188, 140, 270, 32 );
	name.LinkCvar( "name" );
	name.iMaxLength = MAX_SCOREBOARDNAME;

	msgBox.SetMessage( "Please, choose another player name" );
	msgBox.Link( this );

	// don't close automatically
	bAutoHide = false;
	Link( this ); // i am my own son

	CMenuYesNoMessageBox::_Init();

	AddItem( name );
}

void UI_PlayerIntroduceDialog_Show( CMenuBaseWindow *pCaller )
{
	uiIntroduceDialog.pCaller = pCaller;
	uiIntroduceDialog.Show();
}
