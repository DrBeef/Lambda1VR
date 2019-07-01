#include "ScrollView.h"
#include "Scissor.h"

CMenuScrollView::CMenuScrollView() : CMenuItemsHolder (),
	m_bHoldingMouse1( false )
{
}

void CMenuScrollView::VidInit()
{
	iStrokeColor.SetDefault( uiPromptTextColor );

	BaseClass::VidInit();

	m_iMax = 0;
	m_iPos = 0;

	for( int i = 0; i < m_numItems; i++ )
	{
		Point pt = m_pItems[i]->pos;
		Size sz = m_pItems[i]->size;

		m_iMax += pt.y + sz.h;
	}
	m_bDisableScrolling = (m_iMax < size.h);

	m_iMax *= uiStatic.scaleX;
}

const char *CMenuScrollView::Key( int key, int down )
{
	// act when key is pressed or repeated
	if( down )
	{
		if( !m_bDisableScrolling )
		{
			int newPos = m_iPos;
			switch( key )
			{
			case K_MWHEELUP:
			case K_UPARROW:
				newPos -= 20;
				break;
			case K_MWHEELDOWN:
			case K_DOWNARROW:
				newPos += 20;
				break;

			case K_PGUP:
				newPos -= 100;
				break;
			case K_PGDN:
				newPos += 100;
				break;
			case K_MOUSE1:
				// m_bHoldingMouse1 = down != 0;
				// m_HoldingPoint = Point( uiStatic.cursorX, uiStatic.cursorY );
				// drag & drop
				// scrollbar
				break;
			}
			// TODO: overscrolling
			newPos = bound( 0, newPos, m_iMax - m_scSize.h );

			// recalc
			if( newPos != m_iPos )
			{
				m_iPos = newPos;
				for( int i = 0; i < m_numItems; i++ )
				{
					CMenuBaseItem *pItem = m_pItems[i];

					pItem->VidInit();
				}
				CMenuItemsHolder::MouseMove( uiStatic.cursorX, uiStatic.cursorY );
			}
		}
	}

	return CMenuItemsHolder::Key( key, down );
}

Point CMenuScrollView::GetPositionOffset() const
{
	return Point( 0, -m_iPos ) + BaseClass::GetPositionOffset();
}

bool CMenuScrollView::MouseMove( int x, int y )
{
	return CMenuItemsHolder::MouseMove( x, y );
}

bool CMenuScrollView::IsRectVisible(Point pt, Size sz)
{
	bool x = isrange( m_scPos.x, pt.x, m_scPos.x + m_scSize.w ) ||
			 isrange( pt.x, m_scPos.x, pt.x + sz.w );

	bool y = isrange( m_scPos.y, pt.y, m_scPos.y + m_scSize.h ) ||
			 isrange( pt.y, m_scPos.y, pt.y + sz.h );

	return x && y;
}

void CMenuScrollView::Draw()
{
	if( EngFuncs::KEY_IsDown( K_MOUSE1 ) )
	{
		if( !m_bHoldingMouse1 )
		{
			m_bHoldingMouse1 = true;
			m_HoldingPoint = Point( uiStatic.cursorX, uiStatic.cursorY );
		}
	}
	else
	{
		if( m_bHoldingMouse1 ) m_bHoldingMouse1 = false;
	}

	if( m_bHoldingMouse1 && !m_bDisableScrolling )
	{
		int newPos = m_iPos;

		newPos -= ( uiStatic.cursorY - m_HoldingPoint.y ) / 2;

		// TODO: overscrolling
		newPos = bound( 0, newPos, m_iMax - m_scSize.h );

		// recalc
		if( newPos != m_iPos )
		{
			m_iPos = newPos;
			for( int i = 0; i < m_numItems; i++ )
			{
				CMenuBaseItem *pItem = m_pItems[i];

				pItem->VidInit();
			}
		}
		m_HoldingPoint = Point( uiStatic.cursorX, uiStatic.cursorY );
	}

	if( bDrawStroke )
	{
		UI_DrawRectangleExt( m_scPos, m_scSize, iStrokeColor, 1 );
	}

	int drawn = 0, skipped = 0;
	for( int i = 0; i < m_numItems; i++ )
	{
		if( !IsRectVisible( m_pItems[i]->GetRenderPosition(), m_pItems[i]->GetRenderSize() ) )
		{
			m_pItems[i]->iFlags |= QMF_HIDDENBYPARENT;
			skipped++;
		}
		else
		{
			m_pItems[i]->iFlags &= ~QMF_HIDDENBYPARENT;
			drawn++;
		}
	}

	Con_NPrintf( 0, "Drawn: %i Skipped: %i", drawn, skipped );

	UI::Scissor::PushScissor( m_scPos, m_scSize );
		CMenuItemsHolder::Draw();
	UI::Scissor::PopScissor();
}
