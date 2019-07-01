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


#include "extdll_menu.h"
#include "BaseMenu.h"
#include "Utils.h"
#include "BtnsBMPTable.h"
#include <string.h>

#define ART_BUTTONS_MAIN		"gfx/shell/btns_main.bmp"	// we support bmp only

typedef unsigned char	BYTE;
typedef short int	    WORD;
typedef unsigned int    DWORD;
typedef int				LONG;

#pragma pack(push, 1)
typedef struct tagBITMAPFILEHEADER {
  WORD  bfType;
  DWORD bfSize;
  WORD  bfReserved1;
  WORD  bfReserved2;
  DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
  DWORD biSize;
  LONG  biWidth;
  LONG  biHeight;
  WORD  biPlanes;
  WORD  biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG  biXPelsPerMeter;
  LONG  biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct tagRGBQUAD {
  BYTE rgbBlue;
  BYTE rgbGreen;
  BYTE rgbRed;
  BYTE rgbReserved;
} RGBQUAD;
#pragma pack(pop)
/*
=================
UI_LoadBmpButtons
=================
*/
void UI_LoadBmpButtons( void )
{
	memset( uiStatic.buttonsPics, 0, sizeof( uiStatic.buttonsPics ));

	int bmp_filesize, palette_sz = 0;
	byte *bmp_buffer = EngFuncs::COM_LoadFile( ART_BUTTONS_MAIN, &bmp_filesize );

	if( !bmp_buffer || !bmp_filesize )
	{
		Con_Printf( "UI_LoadBmpButtons: btns_main.bmp not found\n" );
		return;
	}

	BITMAPFILEHEADER *pFileHdr = (BITMAPFILEHEADER *)bmp_buffer;
	BITMAPINFOHEADER *pInfoHdr = (BITMAPINFOHEADER *)&bmp_buffer[sizeof( BITMAPFILEHEADER )];

	BITMAPINFOHEADER NewInfoHdr;
	BITMAPFILEHEADER NewFileHdr;

	if( pInfoHdr->biBitCount == 8 && pInfoHdr->biClrUsed == 0 )
		pInfoHdr->biClrUsed = 256; // all colors used

	memcpy( &NewFileHdr, pFileHdr, sizeof( BITMAPFILEHEADER ));
	memcpy( &NewInfoHdr, pInfoHdr, sizeof( BITMAPINFOHEADER ));

	byte *palette = bmp_buffer + sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER );
	if( pInfoHdr->biBitCount <= 8 )
	{
		// figure out how many entries are actually in the table
		if( pInfoHdr->biClrUsed == 0 )
		{
			pInfoHdr->biClrUsed = 256;
			palette_sz = (1 << pInfoHdr->biBitCount) * sizeof( RGBQUAD );
		}
		else palette_sz = pInfoHdr->biClrUsed * sizeof( RGBQUAD );
	}

	uiStatic.buttons_width = pInfoHdr->biWidth;
	uiStatic.buttons_height = 78;	// fixed height (26 * 3)

	// determine buttons count by image height...
	int pic_count = ( pInfoHdr->biHeight / uiStatic.buttons_height );

	int stride = (pInfoHdr->biWidth * pInfoHdr->biBitCount / 8);
	int cutted_img_sz = ((stride + 3 ) & ~3) * uiStatic.buttons_height;
	int CuttedBmpSize = sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER ) + palette_sz + cutted_img_sz;
	byte *img_data = &bmp_buffer[pFileHdr->bfOffBits + cutted_img_sz * ( pic_count - 1 )];

	NewFileHdr.bfSize = CuttedBmpSize;
	NewFileHdr.bfOffBits = sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER ) + palette_sz;
	NewInfoHdr.biHeight = uiStatic.buttons_height;
	NewInfoHdr.biSizeImage = cutted_img_sz;

	char fname[256];
	byte *raw_img_buff = (byte *)MALLOC( CuttedBmpSize );

	for( int i = 0; i < pic_count; i++ )
	{
		sprintf( fname, "#btns_%d.bmp", i );

		int offset = 0;
		memcpy( &raw_img_buff[offset], &NewFileHdr, sizeof( BITMAPFILEHEADER ));
		offset += sizeof( BITMAPFILEHEADER );

		memcpy( &raw_img_buff[offset], &NewInfoHdr, NewInfoHdr.biSize );
		offset += NewInfoHdr.biSize;

		if( NewInfoHdr.biBitCount <= 8 )
		{
			memcpy( &raw_img_buff[offset], palette, palette_sz );
			offset += palette_sz;
		}

		memcpy( &raw_img_buff[offset], img_data, cutted_img_sz );

		// upload image into video memory
		uiStatic.buttonsPics[i] = EngFuncs::PIC_Load( fname, raw_img_buff, CuttedBmpSize );

		img_data -= cutted_img_sz;
	}

	FREE( raw_img_buff );
	EngFuncs::COM_FreeFile( bmp_buffer );
}
