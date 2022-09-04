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
#include "Slider.h"
#include "CheckBox.h"
#include "SpinControl.h"
#include "StringArrayModel.h"

#define ART_BANNER	  	"gfx/shell/head_vidoptions"
#define ART_GAMMA		"gfx/shell/gamma"

static const char *refreshStr[] =
{
		"60hz", "72hz", "80hz", "90hz", "120hz"
};


class CMenuVidOptions : public CMenuFramework
{
private:
	void _Init() override;
	void _VidInit() override;

public:
	CMenuVidOptions() : CMenuFramework( "CMenuVidOptions" ) { }
	void SaveAndPopMenu() override;
	void GammaUpdate();
	void GammaGet();
	void GetConfig();
	void RefreshChanged( void );

	int		outlineWidth;

	class CMenuVidPreview : public CMenuBitmap
	{
		void Draw() override;
	} testImage;

	CMenuPicButton	done;

	CMenuSlider	gammaIntensity;
	CMenuSlider	glareReduction;
	CMenuSpinControl	refresh;
	//CMenuCheckBox	fastSky;
	CMenuCheckBox	hiTextures;
	CMenuCheckBox   vbo;
	CMenuCheckBox   actionables;
	CMenuCheckBox   fps;
	CMenuSlider		height;
	CMenuSlider		vignette;

	HIMAGE		hTestImage;
} uiVidOptions;


/*
=================
CMenuVidOptions::GammaUpdate
=================
*/
void CMenuVidOptions::GammaUpdate( void )
{
	float val = RemapVal( uiVidOptions.gammaIntensity.GetCurrentValue(), 0.0, 1.0, 1.8, 7.0 );
	EngFuncs::CvarSetValue( "gamma", val );
	EngFuncs::ProcessImage( uiVidOptions.hTestImage, val );
}

void CMenuVidOptions::GammaGet( void )
{
	float val = EngFuncs::GetCvarFloat( "gamma" );

	uiVidOptions.gammaIntensity.SetCurrentValue( RemapVal( val, 1.8f, 7.0f, 0.0f, 1.0f ) );
	EngFuncs::ProcessImage( uiVidOptions.hTestImage, val );

	uiVidOptions.gammaIntensity.SetOriginalValue( val );
}

void CMenuVidOptions::SaveAndPopMenu( void )
{
	glareReduction.WriteCvar();
	//refresh.WriteCvar();
	//fastSky.WriteCvar();
	hiTextures.WriteCvar();
	vbo.WriteCvar();
	actionables.WriteCvar();
	fps.WriteCvar();
	height.WriteCvar();
	vignette.WriteCvar();
	// gamma is already written

	//write refresh
	RefreshChanged();

	CMenuFramework::SaveAndPopMenu();
}

/*
=================
CMenuVidOptions::Ownerdraw
=================
*/
void CMenuVidOptions::CMenuVidPreview::Draw( )
{
	int		color = 0xFFFF0000; // 255, 0, 0, 255
	int		viewport[4];
	int		viewsize, size, sb_lines;

	viewsize = EngFuncs::GetCvarFloat( "viewsize" );

	if( viewsize >= 120 )
		sb_lines = 0;	// no status bar at all
	else if( viewsize >= 110 )
		sb_lines = 24;	// no inventory
	else sb_lines = 48;

	size = Q_min( viewsize, 100 );

	viewport[2] = m_scSize.w * size / 100;
	viewport[3] = m_scSize.h * size / 100;

	if( viewport[3] > m_scSize.h - sb_lines )
		viewport[3] = m_scSize.h - sb_lines;
	if( viewport[3] > m_scSize.h )
		viewport[3] = m_scSize.h;

	viewport[2] &= ~7;
	viewport[3] &= ~1;

	viewport[0] = (m_scSize.w - viewport[2]) / 2;
	viewport[1] = (m_scSize.h - sb_lines - viewport[3]) / 2;

	UI_DrawPic( m_scPos.x + viewport[0], m_scPos.y + viewport[1], viewport[2], viewport[3], uiColorWhite, szPic );
	UI_DrawRectangleExt( m_scPos, m_scSize, color, ((CMenuVidOptions*)Parent())->outlineWidth );
}

/*
=================
CMenuVidOptions::Init
=================
*/
void CMenuVidOptions::_Init( void )
{
#ifdef PIC_KEEP_RGBDATA
	hTestImage = EngFuncs::PIC_Load( ART_GAMMA, PIC_KEEP_RGBDATA );
#else
	hTestImage = EngFuncs::PIC_Load( ART_GAMMA, PIC_KEEP_SOURCE );
#endif

	banner.SetPicture(ART_BANNER);

	testImage.iFlags = QMF_INACTIVE;
	testImage.SetRect( 390, 225, 480, 450 );
	testImage.SetPicture( ART_GAMMA );

	gammaIntensity.SetNameAndStatus( "Gamma", "Set gamma value (0.5 - 2.3)" );
	gammaIntensity.SetCoord( 72, 280 );
	gammaIntensity.Setup( 0.0, 1.0, 0.025 );
	gammaIntensity.onChanged = VoidCb( &CMenuVidOptions::GammaUpdate );
	gammaIntensity.onCvarGet = VoidCb( &CMenuVidOptions::GammaGet );
	gammaIntensity.LinkCvar( "gamma" );

	glareReduction.SetCoord( 72, 340 );
	if( UI_IsXashFWGS() )
	{
		glareReduction.SetNameAndStatus( "Glare reduction", "Set glare reduction level" );
		glareReduction.Setup( 100, 300, 15 );
		glareReduction.LinkCvar( "r_flaresize" );
	}
	else
	{
		glareReduction.SetNameAndStatus( "Brightness", "Set brightness level" );
		glareReduction.Setup( 0, 3, 0.1 );
		glareReduction.LinkCvar( "brightness" );
	}


	static CStringArrayModel model( refreshStr, ARRAYSIZE( refreshStr ));
	refresh.Setup( &model );
	refresh.font = QM_SMALLFONT;
	refresh.SetRect( 72, 400, 300, 32 );
	refresh.SetNameAndStatus( "Refresh", "Set Refresh Rate" );
	refresh.Setup( 0, 4, 1 );
	//refresh.LinkCvar( "vr_refresh", CMenuEditable::CVAR_VALUE );
	refresh.onChanged = VoidCb( &CMenuVidOptions::RefreshChanged );


	height.SetCoord( 72, 470 );
	height.SetNameAndStatus( "Height Offset: %.1fm", "Set Player Height Adjustment" );
	height.Setup( -0.5, 1.0, 0.1 );
	height.SetDrawValue(true);
	height.LinkCvar( "vr_height_adjust" );

	vignette.SetCoord( 72, 520 );
	vignette.SetNameAndStatus( "Comfort Vignette", "Set Comfort Vignette" );
	vignette.Setup( 0.0, 0.7, 0.05 );
	vignette.SetDrawValue(true);
	vignette.LinkCvar( "vr_comfort_mask" );

	vbo.SetNameAndStatus( "Use VBO", "Use new world renderer. Faster, but causes issues with flashlight" );
	vbo.SetCoord( 72, 555 );
	vbo.LinkCvar( "r_vbo" );
//	vbo.onChanged = CMenuCheckBox::BitMaskCb;
//	vbo.onChanged.pExtra = &bump.iFlags;
	vbo.bInvertMask = true;
	vbo.iMask = QMF_GRAYED;

	actionables.SetNameAndStatus( "Highlight Usable Objects", "Enables highlighting of objects when they can be \"used\"" );
	actionables.SetCoord( 72, 605 );
	actionables.LinkCvar( "vr_highlight_actionables" );

	//fastSky.SetNameAndStatus( "Draw simple sky", "enable/disable fast sky rendering (for old computers)" );
	//fastSky.SetCoord( 72, 545 );
	//fastSky.LinkCvar( "r_fastsky" );

	hiTextures.SetNameAndStatus( "Allow materials", "let engine replace 8-bit textures with full color hi-res prototypes (if present)" );
	hiTextures.SetCoord( 72, 655 );
	hiTextures.LinkCvar( "host_allow_materials" );

	fps.SetNameAndStatus( "Show FPS", "Show FPS Counter" );
	fps.SetCoord( 72, 705 );
	fps.LinkCvar( "cl_showfps" );

	done.SetNameAndStatus( "Done", "Go back to the Video Menu" );
	done.SetCoord( 72, 755 );
	done.SetPicture( PC_DONE );
	done.onActivated = VoidCb( &CMenuVidOptions::SaveAndPopMenu );

	AddItem( background );
	AddItem( banner );
	AddItem( done );
	AddItem( gammaIntensity );
	AddItem( glareReduction );
	AddItem( refresh );
	AddItem( actionables );
	AddItem( fps );
	AddItem( vbo );
	AddItem( height );
	AddItem( vignette );
	//AddItem( fastSky );
	AddItem( hiTextures );
	AddItem( testImage );
}

void CMenuVidOptions::RefreshChanged( void )
{
	if( refresh.GetCurrentValue() == 0)
	{
	EngFuncs::CvarSetValue("vr_refresh", 60.0);
	}
	else if( refresh.GetCurrentValue() == 1)
	{
	EngFuncs::CvarSetValue("vr_refresh", 72);
	}
	else if( refresh.GetCurrentValue() == 2)
	{
	EngFuncs::CvarSetValue("vr_refresh", 80);
	}
	else if( refresh.GetCurrentValue() == 3)
	{
	EngFuncs::CvarSetValue("vr_refresh", 90);
	}
	else if( refresh.GetCurrentValue() == 4)
	{
	EngFuncs::CvarSetValue("vr_refresh", 120);
	}
}
/*
=================
CMenuVidOptions::GetConfig
=================
*/
void CMenuVidOptions::GetConfig( void )
{
	int vr_refresh = (int)EngFuncs::GetCvarFloat("vr_refresh");
	int val = 0;
	switch (vr_refresh)
	{
		case 72:
		{
			val = 1;
		}
		break;
		case 80:
		{
			val = 2;
		}
		break;
		case 90:
		{
			val = 3;
		}
		break;
		case 120:
		{
			val = 4;
		}
		break;
		default:
			break;
	}

	refresh.ForceDisplayString( refreshStr[val] );
	refresh.SetCurrentValue( (float)val );
}

void CMenuVidOptions::_VidInit()
{
	outlineWidth = 2;
	UI_ScaleCoords( NULL, NULL, &outlineWidth, NULL );

	GetConfig();
}

/*
=================
CMenuVidOptions::Precache
=================
*/
void UI_VidOptions_Precache( void )
{
	EngFuncs::PIC_Load( ART_BANNER );
}

/*
=================
CMenuVidOptions::Menu
=================
*/
void UI_VidOptions_Menu( void )
{
	uiVidOptions.Show();
}
ADD_MENU( menu_vidoptions, UI_VidOptions_Precache, UI_VidOptions_Menu );
