/*
BaseWindow.h -- base menu window
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
#pragma once
#ifndef BASEWINDOW_H
#define BASEWINDOW_H

#include "ItemsHolder.h"
#include "BackgroundBitmap.h"

// Base class for windows.
// Should be used for message boxes, dialogs, root menus(e.g. frameworks)
class CMenuBaseWindow : public CMenuItemsHolder
{
public:
	typedef CMenuItemsHolder BaseClass;
	CMenuBaseWindow( const char *name = "Unnamed Window" );

	// Overloaded functions
	// Window visibility is switched through window stack
	void Hide() override;
	void Show() override;
	bool IsVisible() const override;

	const char *Key( int key, int down ) override;
	void Draw() override;

	bool KeyValueData(const char *key, const char *data) override;

	enum EAnimation
	{
		ANIM_IN = 0,
		ANIM_OUT
	};

	// Override this method to draw custom animations
	// For example, during transitions
	// Return false when animation is still going
	// Otherwise return true, so window will be marked as "no animation"
	// and this method will not be called anymore(until next menu transition)

	// Window animation draw life during transition
	// 1. ANIM_IN
	// 2. ANIM_OUT
	virtual bool DrawAnimation( EAnimation anim );

	// Check current window is a root
	virtual bool IsRoot() { return false; }

	// Hide current window and save changes
	virtual void SaveAndPopMenu();

	bool IsWindow() override { return true; }

	void EnableTransition();

	// set parent of window
	void Link( CMenuItemsHolder *h )
	{
		m_pParent = h;
	}

	bool bAllowDrag;
	bool bInTransition;
	EAnimation eTransitionType; // valid only when in transition
	CMenuBackgroundBitmap background;

	const windowStack_t *WindowStack() const
	{
		return m_pStack;
	}

protected:
	int m_iTransitionStartTime;

	void PushMenu( windowStack_t &stack );
	void PopMenu( windowStack_t &stack );
	windowStack_t *m_pStack;

private:
	CMenuBaseWindow(); // remove

	friend void UI_DrawMouseCursor( void ); // HACKHACK: Cursor should be set by menu item
	friend void UI_UpdateMenu( float flTime );

	bool IsAbsolutePositioned( void ) const override { return true; }

	bool m_bHolding;
	Point m_bHoldOffset;
};

#endif // BASEWINDOW_H
