/*
BaseWindow.cpp -- base menu window
Copyright (C) 2017 a1batross

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#include "extdll_menu.h"
#include "BaseMenu.h"
#include "Utils.h"
#include "PicButton.h"
#include "ItemsHolder.h"
#include "BaseWindow.h"

CMenuBaseWindow::CMenuBaseWindow(const char *name) : BaseClass()
{
	bAllowDrag = false; // UNDONE
	m_bHolding = false;
	bInTransition = false;
	szName = name;
	m_pStack = &uiStatic.menu;
}

void CMenuBaseWindow::Show()
{
	Init();
	VidInit();
	Reload(); // take a chance to reload info for items
	PushMenu( *m_pStack );
	EnableTransition();
}

void CMenuBaseWindow::Hide()
{
	PopMenu( *m_pStack );
	EnableTransition();
}

bool CMenuBaseWindow::IsVisible() const
{
	// slow!
	for( int i = m_pStack->rootPosition; i < m_pStack->menuDepth; i++  )
	{
		if( m_pStack->menuStack[i] == this )
			return true;
	}
	return false;
}

void CMenuBaseWindow::PushMenu( windowStack_t &stack )
{
	int		i;
	CMenuBaseItem	*item;

	// if this menu is already present, drop back to that level to avoid stacking menus by hotkeys
	for( i = 0; i < stack.menuDepth; i++ )
	{
		if( stack.menuStack[i] == this )
		{
			if( IsRoot() )
				stack.menuDepth = i;
			else
			{
				if( i != stack.menuDepth - 1 )
				{
					// swap windows
					stack.menuStack[i] = stack.menuActive;
					stack.menuStack[stack.menuDepth] = this;
				}
			}
			break;
		}
	}

	if( i == stack.menuDepth )
	{
		if( stack.menuDepth >= UI_MAX_MENUDEPTH )
			Host_Error( "UI_PushMenu: menu stack overflow\n" );
		stack.menuStack[stack.menuDepth++] = this;
	}

	stack.prevMenu = stack.menuActive;
	if( this->IsRoot() && stack.prevMenu && stack.prevMenu->IsRoot() )
		stack.prevMenu->EnableTransition();
	stack.menuActive = this;

	if( &stack == &uiStatic.menu ) // hack!
	{
		uiStatic.firstDraw = true;
		uiStatic.enterSound = gpGlobals->time + 0.15f;	// make some delay

		EngFuncs::KEY_SetDest ( KEY_MENU );
	}

	m_iCursor = 0;

	// Probably not a best way
	// but we need to inform new window about cursor position,
	// otherwise we will have an invalid cursor until first mouse move event
#if 1
	m_iCursorPrev = -1;
	MouseMove( uiStatic.cursorX, uiStatic.cursorY );
#else
	m_iCursor = 0;
	m_iCursorPrev = 0;
	// force first available item to have focus
	for( i = 0; i < m_numItems; i++ )
	{
		item = m_pItems[i];

		if( !item->IsVisible() || item->iFlags & (QMF_GRAYED|QMF_INACTIVE|QMF_MOUSEONLY))
		continue;

		m_iCursorPrev = -1;
		SetCursor( i );
		break;
	}
#endif
}

void CMenuBaseWindow::PopMenu( windowStack_t &stack )
{
	if( &stack == &uiStatic.menu ) // hack!
	{
		EngFuncs::PlayLocalSound( uiSoundOut );
	}

	stack.menuDepth--;

	if( stack.menuDepth < 0 )
		Host_Error( "UI_PopMenu: menu stack underflow\n" );

	if( stack.menuDepth )
	{
		stack.prevMenu = this;
		stack.menuActive = stack.menuStack[stack.menuDepth-1];
		if( this->IsRoot() && stack.menuActive->IsRoot() )
			stack.menuActive->EnableTransition();

		uiStatic.firstDraw = true;
	}
	else if( &stack == &uiStatic.menu ) // hack!
	{
		if ( CL_IsActive( ))
		{
			UI_CloseMenu();
		}
		else
		{
			// a1ba: not needed anymore?

			// never trying the close menu when client isn't connected
			EngFuncs::KEY_SetDest( KEY_MENU );
			UI_Main_Menu();
		}
	}

	if( &stack == &uiStatic.menu && uiStatic.m_fDemosPlayed && uiStatic.m_iOldMenuDepth == stack.menuDepth )
	{
		EngFuncs::ClientCmd( FALSE, "demos\n" );
		uiStatic.m_fDemosPlayed = false;
		uiStatic.m_iOldMenuDepth = 0;
	}
}

void CMenuBaseWindow::SaveAndPopMenu()
{
	EngFuncs::ClientCmd( FALSE, "trysaveconfig\n" );
	Hide();
}

const char *CMenuBaseWindow::Key(int key, int down)
{
	if( key == K_MOUSE1 && bAllowDrag )
	{
		m_bHolding = down;
		m_bHoldOffset.x = uiStatic.cursorX;
		m_bHoldOffset.y = uiStatic.cursorY;
	}

	if( down && UI::Key::IsEscape( key ) )
	{
		Hide( );
		return uiSoundOut;
	}

	return CMenuItemsHolder::Key( key, down );
}

void CMenuBaseWindow::Draw()
{
	if( !IsRoot() && m_bHolding && bAllowDrag )
	{
		m_scPos.x += uiStatic.cursorX - m_bHoldOffset.x;
		m_scPos.y += uiStatic.cursorY - m_bHoldOffset.y;

		m_bHoldOffset.x = uiStatic.cursorX;
		m_bHoldOffset.y = uiStatic.cursorY;
		// CalcPosition();
		CalcItemsPositions();
	}
	CMenuItemsHolder::Draw();
}


bool CMenuBaseWindow::DrawAnimation(EAnimation anim)
{
	float alpha;

	if( anim == ANIM_IN )
	{
		alpha = ( uiStatic.realTime - m_iTransitionStartTime ) / TTT_PERIOD;
	}
	else if( anim == ANIM_OUT )
	{
		alpha = 1.0f - ( uiStatic.realTime - m_iTransitionStartTime ) / TTT_PERIOD;
	}

	if(	( anim == ANIM_IN  && alpha < 1.0f )
		|| ( anim == ANIM_OUT && alpha > 0.0f ) )
	{
		UI_EnableAlphaFactor( alpha );

		Draw();

		UI_DisableAlphaFactor();

		return false;
	}

	return true;
}

bool CMenuBaseWindow::KeyValueData(const char *key, const char *data)
{
	if( !strcmp( key, "enabled" ) || !strcmp( key, "visible" ) )
	{

	}
	else
	{
		if( !strcmp( key, "xpos" ) ||
		!strcmp( key, "ypos" ) ||
		!strcmp( key, "wide" ) ||
		!strcmp( key, "tall" ) )
		{
			background.KeyValueData( key, data );
		}

		return CMenuBaseItem::KeyValueData(key, data);
	}

	return true;
}

void CMenuBaseWindow::EnableTransition()
{
	if( m_pStack->prevMenu )
	{
		bInTransition = true;
		m_iTransitionStartTime = uiStatic.realTime;
	}
}
