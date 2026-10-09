// ----------------------------------------------------------------------- //
//
// MODULE  : HUDDecision.h
//
// PURPOSE : Definition of CHUDDecision to display transmission messages
//
// (c) 2001 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#ifndef __HUD_DECISION_H
#define __HUD_DECISION_H

#include "HUDItem.h"
#include "ClientServerShared.h"

//******************************************************************************************
//** HUD Message Queue
//******************************************************************************************
class CHUDDecision : public CHUDItem
{
public:
	CHUDDecision();
	

    virtual LTBOOL      Init();
	virtual void		Term();

    virtual void        Render();
    virtual void        Update();

	virtual void        UpdateLayout();

	void	OnObjectRemove(HLOCALOBJ hObj);

	//hide or show based on messages from the server
	void	Show(ILTMessage_Read *pMsg);

	//close window and tell server
	void	Hide();

	void	SetScale(float fScale);

	LTBOOL	IsVisible() {return m_bVisible;}

	void	Choose(uint8 nChoice);

	// VR: the controllers pick an answer by moving a highlight (nDir -1 =
	// up, +1 = down) and confirming it. The number keys still work.
	void	VR_MoveSelection(int nDir);
	void	VR_ChooseSelection();

	HOBJECT GetObject() { return m_hObject; }
	float	GetRadius() { return m_fRadius; }

protected:

    LTIntPt					m_BasePos;

	CUIFont*	m_pFont;				// The font for this control
	uint8		m_nFontSize;			// The scaled font size to use.
	uint8		m_nBaseFontSize;		// The font size before scaling
	uint32		m_nTextColor;
	float		m_fScale;
	LTBOOL		m_bVisible;
	uint16		m_nWidth;
	LTIntPt		m_Offset;

	HOBJECT		m_hObject;
	LTVector	m_vObjPos;
	float		m_fRadius;

	CLTGUIWindow	    m_Dlg;
	CLTGUIColumnCtrl*	m_pText[MAX_DECISION_CHOICES];

	int		m_nVRSelection;
	bool	IsChoiceShown(int i) { return m_pText[i] && m_pText[i]->IsVisible(); }
	void	UpdateVRHighlight();

};



#endif