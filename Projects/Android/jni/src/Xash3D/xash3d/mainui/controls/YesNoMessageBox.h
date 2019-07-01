/*
YesNoMessageBox.h - simple generic yes/no message box
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
#ifndef MENU_GENERICMSGBOX_H
#define MENU_GENERICMSGBOX_H

#include "PicButton.h"
#include "Action.h"
#include "ItemsHolder.h"
#include "BaseWindow.h"

class CMenuYesNoMessageBox : public CMenuBaseWindow
{
public:
	typedef CMenuBaseWindow BaseClass;
	CMenuYesNoMessageBox( bool alert = false );

	void _Init() override;
	void _VidInit() override;
	void Draw() override;
	const char *Key( int key, int down ) override;
	void SetMessage( const char *msg );
	void SetPositiveButton( const char *msg, EDefaultBtns buttonPic, int extrawidth = 0 );
	void SetNegativeButton( const char *msg, EDefaultBtns buttonPic, int extrawidth = 0 );
	enum EHighlight
	{
		NO_HIGHLIGHT = 0,
		HIGHLIGHT_YES,
		HIGHLIGHT_NO
	};
	void HighlightChoice( EHighlight ch );

	// Pass pointer to messagebox to extra of calling object
	CEventCallback MakeOpenEvent();

	CEventCallback onPositive;
	CEventCallback onNegative;

	static void UI_ShowMessageBox( void );

	bool bAutoHide;
private:
	static void OpenCb( CMenuBaseItem *, void *pExtra );

	CMenuAction		dlgMessage1;
	CMenuPicButton	yes;
	CMenuPicButton	no;
	bool m_bSetYes, m_bSetNo;
	bool m_bIsAlert;
};

#endif // MENU_GENERICMSGBOX_H
