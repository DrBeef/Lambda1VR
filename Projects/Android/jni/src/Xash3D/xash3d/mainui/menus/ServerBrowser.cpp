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
#include "YesNoMessageBox.h"
#include "Table.h"
#include "keydefs.h"
#include "Switch.h"
#include "Field.h"
#include "utlvector.h"

#define ART_BANNER_INET		"gfx/shell/head_inetgames"
#define ART_BANNER_LAN		"gfx/shell/head_lan"
#define ART_BANNER_LOCK		"gfx/shell/lock"

struct server_t
{
	netadr_t adr;
	char info[256];
	float ping;
	char name[64];
	char mapname[64];
	char clientsstr[64];
	char pingstr[64];
	bool havePassword;

	static int NameCmpAscend( const void *_a, const void *_b )
	{
		const server_t *a = (const server_t*)_a;
		const server_t *b = (const server_t*)_b;
		return colorstricmp( a->name, b->name );
	}
	static int NameCmpDescend( const void *a, const void *b )
	{
		return NameCmpAscend( b, a );
	}

	static int MapCmpAscend( const void *_a, const void *_b )
	{
		const server_t *a = (const server_t*)_a;
		const server_t *b = (const server_t*)_b;
		return stricmp( a->mapname, b->mapname );
	}
	static int MapCmpDescend( const void *a, const void *b )
	{
		return MapCmpAscend( b, a );
	}

	static int ClientCmpAscend( const void *_a, const void *_b )
	{
		const server_t *a = (const server_t*)_a;
		const server_t *b = (const server_t*)_b;

		int a_cl = atoi( Info_ValueForKey( a->info, "numcl" ));
		int b_cl = atoi( Info_ValueForKey( b->info, "numcl" ));

		if( a_cl > b_cl ) return 1;
		else if( a_cl < b_cl ) return -1;
		return 0;
	}
	static int ClientCmpDescend( const void *a, const void *b )
	{
		return ClientCmpAscend( b, a );
	}

	static int PingCmpAscend( const void *_a, const void *_b )
	{
		const server_t *a = (const server_t*)_a;
		const server_t *b = (const server_t*)_b;

		if( a->ping > b->ping ) return 1;
		else if( a->ping < b->ping ) return -1;
		return 0;
	}
	static int PingCmpDescend( const void *a, const void *b )
	{
		return PingCmpAscend( b, a );
	}
};

class CMenuGameListModel : public CMenuBaseModel
{
public:
	CMenuGameListModel() : CMenuBaseModel(), m_iSortingColumn(-1) {}

	void Update() override;
	int GetColumns() const override
	{
		return 5; // havePassword, game, mapname, maxcl, ping
	}
	int GetRows() const override
	{
		return servers.Count();
	}
	ECellType GetCellType( int line, int column ) override
	{
		if( column == 0 )
			return CELL_IMAGE_ADDITIVE;
		return CELL_TEXT;
	}
	const char *GetCellText( int line, int column ) override
	{
		switch( column )
		{
		case 0: return servers[line].havePassword ? ART_BANNER_LOCK : NULL;
		case 1: return servers[line].name;
		case 2: return servers[line].mapname;
		case 3: return servers[line].clientsstr;
		case 4: return servers[line].pingstr;
		default: return NULL;
		}
	}
	void OnActivateEntry(int line) override;

	void Flush()
	{
		servers.RemoveAll();
		serversRefreshTime = gpGlobals->time;
	}

	bool IsHavePassword( int line )
	{
		return servers[line].havePassword;
	}

	void AddServerToList( netadr_t adr, const char *info );

	bool Sort(int column, bool ascend) override;

	float serversRefreshTime;
	CUtlVector<server_t> servers;
private:
	int m_iSortingColumn;
	bool m_bAscend;
};

class CMenuServerBrowser: public CMenuFramework
{
public:
	CMenuServerBrowser() : CMenuFramework( "CMenuServerBrowser" ) { }
	void Draw() override;
	void Show() override;

	void SetLANOnly( bool lanOnly )
	{
		m_bLanOnly = lanOnly;
	}
	void GetGamesList( void );
	void ClearList( void );
	void RefreshList( void );
	void JoinGame( void );
	void ResetPing( void )
	{
		gameListModel.serversRefreshTime = Sys_DoubleTime();
	}

	void AddServerToList( netadr_t adr, const char *info );

	static void Connect( server_t &server );

	CMenuPicButton *joinGame;
	CMenuPicButton *createGame;
	CMenuPicButton *refresh;
	CMenuSwitch natOrDirect;

	CMenuYesNoMessageBox msgBox;
	CMenuTable	gameList;
	CMenuGameListModel gameListModel;

	CMenuYesNoMessageBox askPassword;
	CMenuField password;

	int	  refreshTime;
	int   refreshTime2;

	bool m_bLanOnly;
private:
	void _Init() override;
	void _VidInit() override;
};

static server_t staticServerSelect;
static bool staticWaitingPassword = false;

static CMenuServerBrowser	uiServerBrowser;

bool CMenuGameListModel::Sort(int column, bool ascend)
{
	m_iSortingColumn = column;
	if( column == -1 )
		return false; // disabled

	m_bAscend = ascend;
	switch( column )
	{
	case 0: return false;
	case 1:
		qsort( servers.Base(), servers.Count(), sizeof( server_t ),
			ascend ? server_t::NameCmpAscend : server_t::NameCmpDescend );
		return true;
	case 2:
		qsort( servers.Base(), servers.Count(), sizeof( server_t ),
			ascend ? server_t::MapCmpAscend : server_t::MapCmpDescend );
		return true;
	case 3:
		qsort( servers.Base(), servers.Count(), sizeof( server_t ),
			ascend ? server_t::ClientCmpAscend : server_t::ClientCmpDescend );
		return true;
	case 4:
		qsort( servers.Base(), servers.Count(), sizeof( server_t ),
			ascend ? server_t::PingCmpAscend : server_t::PingCmpDescend );
		return true;
	}

	return false;
}

/*
=================
CMenuServerBrowser::GetGamesList
=================
*/
void CMenuGameListModel::Update( void )
{
	int		i;
	const char	*info;

	// regenerate table data
	for( i = 0; i < servers.Count(); i++ )
	{
		info = servers[i].info;

		Q_strncpy( servers[i].name, Info_ValueForKey( info, "host" ), 64 );
		Q_strncpy( servers[i].mapname, Info_ValueForKey( info, "map" ), 64 );
		snprintf( servers[i].clientsstr, 64, "%s\\%s", Info_ValueForKey( info, "numcl" ), Info_ValueForKey( info, "maxcl" ) );
		snprintf( servers[i].pingstr, 64, "%.f ms", servers[i].ping * 1000 );

		const char *passwd = Info_ValueForKey( info, "password" );
		if( passwd[0] && !stricmp( passwd, "1") )
		{
			servers[i].havePassword = true;
		}
		else
		{
			servers[i].havePassword = false;
		}
	}

	if( servers.Count() )
	{
		uiServerBrowser.joinGame->SetGrayed( false );
		if( m_iSortingColumn != -1 )
			Sort( m_iSortingColumn, m_bAscend );
	}
}

void CMenuGameListModel::OnActivateEntry( int line )
{
	CMenuServerBrowser::Connect( servers[line] );
}

void CMenuGameListModel::AddServerToList(netadr_t adr, const char *info)
{
	int i;

	// ignore if duplicated
	for( i = 0; i < servers.Count(); i++ )
	{
		if( !stricmp( servers[i].info, info ))
			return;
	}

	server_t server;

	server.adr = adr;
	server.ping = Sys_DoubleTime() - serversRefreshTime;
	server.ping = bound( 0, server.ping, 9.999 );
	Q_strncpy( server.info, info, sizeof( server.info ));


	Q_strncpy( server.name, Info_ValueForKey( info, "host" ), 64 );
	Q_strncpy( server.mapname, Info_ValueForKey( info, "map" ), 64 );
	snprintf( server.clientsstr, 64, "%s\\%s", Info_ValueForKey( info, "numcl" ), Info_ValueForKey( info, "maxcl" ) );
	snprintf( server.pingstr, 64, "%.f ms", server.ping * 1000 );

	const char *passwd = Info_ValueForKey( info, "password" );
	server.havePassword = passwd[0] && !stricmp( passwd, "1");

	servers.AddToTail( server );

	if( m_iSortingColumn != -1 )
		Sort( m_iSortingColumn, m_bAscend );
}

void CMenuServerBrowser::Connect( server_t &server )
{
	// prevent refresh during connect
	uiServerBrowser.refreshTime = uiStatic.realTime + 999999;

	// ask user for password
	if( server.havePassword )
	{
		// if dialog window is still open, then user have entered the password
		if( !staticWaitingPassword )
		{
			// save current select
			staticServerSelect = server;
			staticWaitingPassword = true;

			// show password request window
			uiServerBrowser.askPassword.Show();

			return;
		}
	}
	else
	{
		// remove password, as server don't require it
		EngFuncs::CvarSetString( "password", "" );
	}

	staticWaitingPassword = false;

	//BUGBUG: ClientJoin not guaranted to return, need use ClientCmd instead!!!
	//BUGBUG: But server addres is known only as netadr_t here!!!
	EngFuncs::ClientJoin( server.adr );
	EngFuncs::ClientCmd( false, "menu_connectionprogress menu server\n" );
}

/*
=================
CMenuServerBrowser::JoinGame
=================
*/
void CMenuServerBrowser::JoinGame()
{
	gameListModel.OnActivateEntry( gameList.GetCurrentIndex() );
}

void CMenuServerBrowser::ClearList()
{
	gameListModel.Flush();
	joinGame->SetGrayed( true );
}

void CMenuServerBrowser::RefreshList()
{
	ClearList();

	if( m_bLanOnly )
	{
		EngFuncs::ClientCmd( FALSE, "localservers\n" );
	}
	else
	{
		if( uiStatic.realTime > refreshTime2 )
		{
			EngFuncs::ClientCmd( FALSE, "internetservers\n" );
			refreshTime2 = uiStatic.realTime + (EngFuncs::GetCvarFloat("cl_nat") ? 4000:1000);
			refresh->SetGrayed( true );
			if( uiStatic.realTime + 20000 < refreshTime )
				refreshTime = uiStatic.realTime + 20000;
		}
	}
}

/*
=================
UI_Background_Ownerdraw
=================
*/
void CMenuServerBrowser::Draw( void )
{
	CMenuFramework::Draw();

	if( uiStatic.realTime > refreshTime )
	{
		RefreshList();
		refreshTime = uiStatic.realTime + 20000; // refresh every 10 secs
	}

	if( uiStatic.realTime > refreshTime2 )
	{
		refresh->SetGrayed( false );
	}
}

/*
=================
CMenuServerBrowser::Init
=================
*/
void CMenuServerBrowser::_Init( void )
{
	AddItem( background );
	AddItem( banner );

	joinGame = AddButton( "Join game", "Join to selected game", PC_JOIN_GAME,
		VoidCb( &CMenuServerBrowser::JoinGame ), QMF_GRAYED );
	joinGame->onActivatedClActive = msgBox.MakeOpenEvent();

	createGame = AddButton( "Create game", NULL, PC_CREATE_GAME );
	SET_EVENT_MULTI( createGame->onActivated,
	{
		if( ((CMenuServerBrowser*)pSelf->Parent())->m_bLanOnly )
			EngFuncs::CvarSetValue( "public", 0.0f );
		else EngFuncs::CvarSetValue( "public", 1.0f );

		UI_CreateGame_Menu();
	});

	// TODO: implement!
	AddButton( "View game info", "Get detail game info", PC_VIEW_GAME_INFO, CEventCallback::NoopCb, QMF_GRAYED );

	refresh = AddButton( "Refresh", "Refresh servers list", PC_REFRESH, VoidCb( &CMenuServerBrowser::RefreshList ) );

	AddButton( "Done", "Return to main menu", PC_DONE, VoidCb( &CMenuServerBrowser::Hide ) );

	msgBox.SetMessage( "Join a network game will exit any current game, OK to exit?" );
	msgBox.SetPositiveButton( "Ok", PC_OK );
	msgBox.HighlightChoice( CMenuYesNoMessageBox::HIGHLIGHT_YES );
	msgBox.onPositive = VoidCb( &CMenuServerBrowser::JoinGame );
	msgBox.Link( this );

	gameList.SetCharSize( QM_SMALLFONT );
	gameList.SetupColumn( 0, NULL, 32.0f, true );
	gameList.SetupColumn( 1, "Name", 0.40f );
	gameList.SetupColumn( 2, "Map", 0.25f );
	gameList.SetupColumn( 3, "Players", 100.0f, true );
	gameList.SetupColumn( 4, "Ping", 120.0f, true );
	gameList.SetModel( &gameListModel );
	gameList.bFramedHintText = true;
	gameList.bAllowSorting = true;

	natOrDirect.AddSwitch( "Direct" );
	natOrDirect.AddSwitch( "NAT" );
	natOrDirect.eTextAlignment = QM_CENTER;
	natOrDirect.bMouseToggle = false;
	natOrDirect.LinkCvar( "cl_nat" );
	natOrDirect.iSelectColor = uiInputFgColor;
	// bit darker
	natOrDirect.iFgTextColor = uiInputFgColor - 0x00151515;
	SET_EVENT_MULTI( natOrDirect.onChanged,
	{
		CMenuSwitch *self = (CMenuSwitch*)pSelf;
		CMenuServerBrowser *parent = (CMenuServerBrowser*)self->Parent();

		self->WriteCvar();
		parent->ClearList();
		parent->RefreshList();
	});

	// server.dll needs for reading savefiles or startup newgame
	if( !EngFuncs::CheckGameDll( ))
		createGame->SetGrayed( true );	// server.dll is missed - remote servers only

	password.bHideInput = true;
	password.bAllowColorstrings = false;
	password.bNumbersOnly = false;
	password.szName = "Password:";
	password.iMaxLength = 16;
	password.SetRect( 188, 140, 270, 32 );

	SET_EVENT_MULTI( askPassword.onPositive,
	{
		CMenuServerBrowser *parent = (CMenuServerBrowser*)pSelf->Parent();

		EngFuncs::CvarSetString( "password", parent->password.GetBuffer() );
		parent->password.Clear(); // we don't need entered password anymore
		CMenuServerBrowser::Connect( staticServerSelect );
	});

	SET_EVENT_MULTI( askPassword.onNegative,
	{
		CMenuServerBrowser *parent = (CMenuServerBrowser*)pSelf->Parent();

		EngFuncs::CvarSetString( "password", "" );
		parent->password.Clear(); // we don't need entered password anymore
		staticWaitingPassword = false;
	});

	askPassword.SetMessage( "Enter server password to continue:" );
	askPassword.Link( this );
	askPassword.Init();
	askPassword.AddItem( password );

	AddItem( gameList );
	AddItem( natOrDirect );
}

/*
=================
CMenuServerBrowser::VidInit
=================
*/
void CMenuServerBrowser::_VidInit()
{
	if( m_bLanOnly )
	{
		banner.SetPicture( ART_BANNER_LAN );
		createGame->szStatusText = ( "Create new LAN game" );
		natOrDirect.Hide();
	}
	else
	{
		banner.SetPicture( ART_BANNER_INET );
		createGame->szStatusText = ( "Create new Internet game" );
		natOrDirect.Show();
	}

	gameList.SetRect( 360, 230, -20, 465 );
	natOrDirect.SetCoord( -20 - natOrDirect.size.w, gameList.pos.y - UI_OUTLINE_WIDTH - natOrDirect.size.h );

	refreshTime = uiStatic.realTime + 500; // delay before update 0.5 sec
	refreshTime2 = uiStatic.realTime + 500;
}

void CMenuServerBrowser::Show()
{
	CMenuFramework::Show();

	// clear out server table
	staticWaitingPassword = false;
	gameListModel.Flush();
	gameList.DisableSorting();
	joinGame->SetGrayed( true );
}

void CMenuServerBrowser::AddServerToList(netadr_t adr, const char *info)
{
	if( stricmp( gMenu.m_gameinfo.gamefolder, Info_ValueForKey( info, "gamedir" )) != 0 )
		return;

	gameListModel.AddServerToList( adr, info );

	joinGame->SetGrayed( false );
}

/*
=================
CMenuServerBrowser::Precache
=================
*/
void UI_ServerBrowser_Precache( void )
{
	EngFuncs::PIC_Load( ART_BANNER_INET );
	EngFuncs::PIC_Load( ART_BANNER_LAN );
}

/*
=================
CMenuServerBrowser::Menu
=================
*/
void UI_ServerBrowser_Menu( void )
{
	if ( gMenu.m_gameinfo.gamemode == GAME_SINGLEPLAYER_ONLY )
		return;

	// stop demos to allow network sockets to open
	if ( gpGlobals->demoplayback && EngFuncs::GetCvarFloat( "cl_background" ))
	{
		uiStatic.m_iOldMenuDepth = uiStatic.menu.menuDepth;
		EngFuncs::ClientCmd( FALSE, "stop\n" );
		uiStatic.m_fDemosPlayed = true;
	}

	uiServerBrowser.Show();
}

void UI_InternetGames_Menu( void )
{
	uiServerBrowser.SetLANOnly( false );

	UI_ServerBrowser_Menu();
}

void UI_LanGame_Menu( void )
{
	uiServerBrowser.SetLANOnly( true );

	UI_ServerBrowser_Menu();
}
ADD_MENU( menu_langame, NULL, UI_LanGame_Menu );
ADD_MENU( menu_internetgames, UI_ServerBrowser_Precache, UI_InternetGames_Menu );

/*
=================
UI_AddServerToList
=================
*/
void UI_AddServerToList( netadr_t adr, const char *info )
{
	if( !uiStatic.initialized )
		return;

	uiServerBrowser.AddServerToList( adr, info );
}

/*
=================
UI_MenuResetPing_f
=================
*/
void UI_MenuResetPing_f( void )
{
	Con_Printf("UI_MenuResetPing_f\n");
	uiServerBrowser.ResetPing();
}
ADD_COMMAND( menu_resetping, UI_MenuResetPing_f );
