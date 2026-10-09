// ----------------------------------------------------------------------- //
//
// MODULE  : HUDItem.h
//
// PURPOSE : Definition of CHUDItem base class
//
// (c) 2001 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#ifndef __HUD_ITEM_H
#define __HUD_ITEM_H

#include "ltbasedefs.h"


enum eHUDRenderLevel
{
	kHUDRenderNone = 0,		// always render
	kHUDRenderText,			// render when text is allowed (i.e. cinematics, etc.)
	kHUDRenderDead,			// render when text is allowed (i.e. cinematics, etc.)
	kHUDRenderFull			// render only when full HUD is rendered
};

// VR: HUD pieces shown somewhere in the headset other than the HUD panel
// (see CVRMgr::RenderHudItem).
enum eHUDVRSpot
{
	kHUDVRNone = 0,			// on the HUD panel with everything else
	kHUDVRWrist,			// on the left wrist
	kHUDVRGun				// on the gun
};

// VR: the part of the screen a HUD item draws in, built up piece by piece
// as the item lays itself out (pixels).
class CHUDVRRect
{
public:
	CHUDVRRect() { Reset(); }
	void Reset() { m_bValid = false; m_fL = m_fT = m_fR = m_fB = 0.0f; }
	void Add(float x, float y, float w, float h)
	{
		if (!(w > 0.0f && h > 0.0f))
			return;
		if (!m_bValid || x < m_fL)		m_fL = x;
		if (!m_bValid || y < m_fT)		m_fT = y;
		if (!m_bValid || x + w > m_fR)	m_fR = x + w;
		if (!m_bValid || y + h > m_fB)	m_fB = y + h;
		m_bValid = true;
	}
	bool Get(float rect[4]) const
	{
		if (!m_bValid)
			return false;
		rect[0] = m_fL; rect[1] = m_fT; rect[2] = m_fR; rect[3] = m_fB;
		return true;
	}
private:
	bool	m_bValid;
	float	m_fL, m_fT, m_fR, m_fB;
};


//******************************************************************************************
//** Base HUD item
//******************************************************************************************
class CHUDItem
{
public:
	CHUDItem() {m_UpdateFlags = 0; m_eLevel = kHUDRenderFull;}
	virtual ~CHUDItem() {Term();}
	

    virtual LTBOOL      Init() {return LTTRUE;}
	virtual void		Term() {}

    virtual void        Render() = 0;
    virtual void        Update() = 0;

    virtual void        UpdateLayout() {}

	virtual uint32		GetUpdateFlags() { return m_UpdateFlags; }

	virtual	eHUDRenderLevel	GetRenderLevel() { return m_eLevel; }

	virtual void		SetRenderState();

	// VR: where this item goes instead of the HUD panel, and the part of
	// the screen it draws in (left, top, right, bottom in pixels; false =
	// nothing to show right now).
	virtual eHUDVRSpot	GetVRSpot() { return kHUDVRNone; }
	virtual bool		GetVRRect(float rect[4]) { return false; }

protected:
	uint32				m_UpdateFlags;
	eHUDRenderLevel		m_eLevel;

};


#endif