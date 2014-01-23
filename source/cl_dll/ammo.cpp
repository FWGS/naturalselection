/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// Ammo.cpp
//
// implementation of CHudAmmo class
//

#include "hud.h"
#include "cl_util.h"

#include <string.h>
#include <stdio.h>

#include "ammohistory.h"
#include "vgui_TeamFortressViewport.h"
#include "mod/AvHClientVariables.h"
#include "mod/AvHSharedUtil.h"
#include "mod/AvHScrollHandler.h"
#include "mod/AvHNetworkMessages.h"

WEAPON *gpActiveSel;	// NULL means off, 1 means just the menu bar, otherwise
						// this points to the active weapon menu item
WEAPON *gpLastSel;		// Last weapon menu selection 

bool HUD_GetWeaponEnabled(int inID);
client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

WeaponsResource gWR;

int g_weaponselect = 0;

extern bool gCanMove;

void IN_AttackDownForced(void);
void IN_AttackUpForced(void);
void IN_Attack2Down(void);
void IN_Attack2Up(void);
void IN_ReloadDown();
void IN_ReloadUp();
bool CheckInAttack(void);

//Equivalent to DECLARE_COMMAND(lastinv,LastInv) except we use gWR instead of gHud
void __CmdFunc_LastInv(void)
{ gWR.UserCmd_LastInv(); }

// +movement
void __CmdFunc_MovementOn(void)
{ gWR.UserCmd_MovementOn(); }
void __CmdFunc_MovementOff(void)
{ gWR.UserCmd_MovementOff(); }

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

WeaponsResource::WeaponsResource(void) : lastWeapon(NULL), iOldWeaponBits(0) {}
WeaponsResource::~WeaponsResource(void) {}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::Init( void )
{
	memset( rgWeapons, 0, sizeof(WEAPON)*MAX_WEAPONS );
	Reset();
	HOOK_COMMAND("lastinv", LastInv);
	// +movement
	HOOK_COMMAND("+movement", MovementOn);
	HOOK_COMMAND("-movement", MovementOff);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::Reset( void )
{
	lastWeapon = NULL;
	iOldWeaponBits = 0;
	memset( rgSlots, 0, sizeof(WEAPON*)*MAX_WEAPON_SLOTS*MAX_WEAPON_POSITIONS );
	memset( riAmmo, 0, sizeof(int)*MAX_AMMO_TYPES );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource :: LoadAllWeaponSprites( void )
{
	int customCrosshairs=CVAR_GET_FLOAT(kvCustomCrosshair);
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if ( rgWeapons[i].iId )
			LoadWeaponSprites( &rgWeapons[i], customCrosshairs );
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

inline void LoadWeaponSprite( client_sprite_t* ptr, HSPRITE& sprite, wrect_t& bounds )
{
	if( ptr )
	{
		string name( "sprites/" );
		name.append( ptr->szSprite );
		name.append( ".spr" );
		sprite = Safe_SPR_Load(name.c_str());
		bounds = ptr->rc;
	}
	else
	{
		sprite = NULL;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource :: LoadWeaponSprites( WEAPON *pWeapon, int custom )
{
	if ( custom < 0 || custom > 4 ) 
		custom=1;

	int resolutions[6] = { 320, 640, 800, 1024, 1280, 1600};
	const int numRes=6;
	int i=0, j=0, iRes=320;
	int screenWidth=ScreenWidth();

	for ( j=0; j < numRes; j++ ) {
		if ( screenWidth == resolutions[j] ) {
			iRes=resolutions[j];
			break;
		}
		if ( j > 0 && screenWidth > resolutions[j-1] && screenWidth < resolutions[j] ) {
			iRes=resolutions[j-1];
			break;
		}
	}

	char sz[128];

	if ( !pWeapon )
		return;

	memset( &pWeapon->rcCrosshair, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcActive, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcInactive, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcAmmo, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcAmmo2, 0, sizeof(wrect_t) );
	pWeapon->hCrosshair =0;
	pWeapon->hInactive = 0;
	pWeapon->hActive = 0;
	pWeapon->hAmmo = 0;
	pWeapon->hAmmo2 = 0;

	sprintf(sz, "sprites/%s.txt", pWeapon->szName);
	client_sprite_t *pList = SPR_GetList(sz, &i);

	if (!pList)
	{
		ASSERT(pList);
		return;
	}

	char crosshairName[32];
	sprintf(crosshairName, "crosshair_%d", custom);
	for ( j=numRes-1; j>=0; j-- ) {
		if ( resolutions[j] <= iRes ) {
			if( pWeapon->hCrosshair == NULL )
				LoadWeaponSprite( GetSpriteList( pList, crosshairName, resolutions[j], i ), pWeapon->hCrosshair, pWeapon->rcCrosshair );
			if( pWeapon->hCrosshair == NULL && custom != 0 )
				LoadWeaponSprite( GetSpriteList( pList, "crosshair_0", resolutions[j], i ), pWeapon->hCrosshair, pWeapon->rcCrosshair );


			if( pWeapon->hInactive == NULL )
				LoadWeaponSprite( GetSpriteList( pList, "weapon", resolutions[j], i ), pWeapon->hInactive, pWeapon->rcInactive );

			if( pWeapon->hActive == NULL )
				LoadWeaponSprite( GetSpriteList( pList, "weapon_s", resolutions[j], i ), pWeapon->hActive, pWeapon->rcActive );

			if( pWeapon->hAmmo == NULL )
				LoadWeaponSprite( GetSpriteList( pList, "ammo", resolutions[j], i ), pWeapon->hAmmo, pWeapon->rcAmmo );

			if( pWeapon->hAmmo2 == NULL )
				LoadWeaponSprite( GetSpriteList( pList, "ammo2", resolutions[j], i ), pWeapon->hAmmo2, pWeapon->rcAmmo2 );
		}
	}
	
	if( pWeapon->hActive || pWeapon->hInactive || pWeapon->hAmmo || pWeapon->hAmmo2 )
	{ gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top ); }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

WEAPON* WeaponsResource::GetWeapon( int iId ) 
{ 
	if( iId < 0 || iId >= MAX_WEAPONS ) { return NULL; }
	return rgWeapons[iId].iId ? &rgWeapons[iId] : NULL; 
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

WEAPON* WeaponsResource::GetWeaponSlot( int slot, int pos ) { return rgSlots[slot][pos]; }

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

WEAPON* WeaponsResource::GetFirstPos( int iSlot )
{
	WEAPON *returnVal = NULL;
	ASSERT( iSlot < MAX_WEAPON_SLOTS && iSlot >= 0 );

	for( int counter = 0; counter < MAX_WEAPON_POSITIONS; ++counter )
	{
		if( this->IsSelectable(rgSlots[iSlot][counter]) )
		{
			returnVal = rgSlots[iSlot][counter];
			break;
		}
	}

	return returnVal;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

WEAPON* WeaponsResource::GetNextActivePos( int iSlot, int iSlotPos )
{
	WEAPON* returnVal = NULL;
	ASSERT( iSlot < MAX_WEAPON_SLOTS && iSlot >= 0 );
	ASSERT( iSlotPos >= 0 );

	for( int counter = iSlotPos+1; counter < MAX_WEAPON_POSITIONS; ++counter )
	{
		if( this->IsSelectable(rgSlots[iSlot][counter]) )
		{
			returnVal = rgSlots[iSlot][counter];
			break;
		}
	}

	return returnVal;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool WeaponsResource::IsEnabled(WEAPON* p)
{
	if( p == NULL ) { return false; }
	return HUD_GetWeaponEnabled(p->iId) && this->HasAmmo(p);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool WeaponsResource::IsSelectable(WEAPON* p)
{
	if( p == NULL ) { return false; }
	return HUD_GetWeaponEnabled(p->iId);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool WeaponsResource::HasAmmo( WEAPON* p )
{
	if( p == NULL) { return false; }
	//note : if max ammo capacity is -1, this has always returned true in spite of not
	// having actual ammo -- KGP
	return (p->iAmmoType == -1) || (p->iMax1 == -1) || p->iClip > 0 || CountAmmo(p->iAmmoType)
		|| CountAmmo(p->iAmmo2Type) || (p->iFlags & WEAPON_FLAGS_SELECTIONEMPTY );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int WeaponsResource::CountAmmo( int iId )
{
	ASSERT( iId < MAX_AMMO_TYPES );
	if( iId < 0 ) return 0;
	return riAmmo[iId];
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int WeaponsResource::GetAmmo( int iId )
{
	ASSERT( iId < MAX_AMMO_TYPES && iId > -1 );
	return riAmmo[iId];
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::SetAmmo( int iId, int iCount )
{
	ASSERT( iId < MAX_AMMO_TYPES && iId > -1 );
	riAmmo[iId] = iCount;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

HSPRITE* WeaponsResource::GetAmmoPicFromWeapon( int iAmmoId, wrect_t& rect )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( rgWeapons[i].iAmmoType == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo;
			return &rgWeapons[i].hAmmo;
		}
		else if ( rgWeapons[i].iAmmo2Type == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo2;
			return &rgWeapons[i].hAmmo2;
		}
	}

	return NULL;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::AddWeapon( WEAPON *wp ) 
{ 
	int customCrosshairs=CVAR_GET_FLOAT(kvCustomCrosshair);
	rgWeapons[ wp->iId ] = *wp;	
	LoadWeaponSprites( &rgWeapons[ wp->iId ], customCrosshairs);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::PickupWeapon( WEAPON *wp )
{
	rgSlots[ wp->iSlot ][ wp->iSlotPos ] = wp;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::DropWeapon( WEAPON *wp )
{
	rgSlots[ wp->iSlot ][ wp->iSlotPos ] = NULL;
	if(lastWeapon == wp) //dropped last weapon, remove it from the list
	{ lastWeapon = NULL; }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::DropAllWeapons( void )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( rgWeapons[i].iId )
			DropWeapon( &rgWeapons[i] );
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::UserCmd_LastInv(void)
{
	if(this->IsSelectable(this->lastWeapon))
	{ 
		this->SetCurrentWeapon(lastWeapon);
		// : 764
		//const char* theSound = AvHSHUGetCommonSoundName(gHUD.GetIsAlien(), WEAPON_SOUND_HUD_ON);
		//gHUD.PlayHUDSound(theSound, kHUDSoundVolume);
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::UserCmd_MovementOn()
{
	// Find out which weapon we want to trigger
	AvHUser3 theUser3 = gHUD.GetHUDUser3();
	int wID = -1;
	switch(theUser3)
	{
	case AVH_USER3_ALIEN_PLAYER1:
		wID = AVH_ABILITY_LEAP;
		break;
	case AVH_USER3_ALIEN_PLAYER3:
		// TODO: Add flap
		break;
	case AVH_USER3_ALIEN_PLAYER4:
		wID = AVH_WEAPON_BLINK;
		break;
	case AVH_USER3_ALIEN_PLAYER5:
		wID = AVH_ABILITY_CHARGE;
		break;
	default:
		IN_ReloadDown();
		return;
	}	

	if (wID > -1)
	{
		// Fetch the needed movement weapon
		WEAPON *p = this->GetWeapon(wID);
		if (p != NULL && this->IsSelectable(p))
		{
			// Send activation of ability asap
			IN_Attack2Down();
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::UserCmd_MovementOff()
{
	// Ensure that we're not activating any weapons when selected
	IN_Attack2Up();
	IN_ReloadUp();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::SetValidWeapon(void)
{
	WEAPON* p = this->GetFirstPos(0); //alien attack 1 or primary marine weapon
	if(gHUD.GetIsAlien())
	{
		this->SetCurrentWeapon(p);
	}
	else
	{
		if(this->IsSelectable(p) && this->HasAmmo(p))
		{
			this->SetCurrentWeapon(p);
		}
		else
		{
			p = this->GetFirstPos(1); //pistol slot
			if(this->IsSelectable(p) && this->HasAmmo(p))
			{
				this->SetCurrentWeapon(p);
			}
			else
			{
				p = this->GetFirstPos(2); //knife slot
				this->SetCurrentWeapon(p);
			}
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource::SetCurrentWeapon(WEAPON* newWeapon)
{
	WEAPON* currentWeapon = this->GetWeapon(gHUD.GetCurrentWeaponID());
	// : 497 - Because weapon state can get out of sync, we should allow this even if the weapons are the same 
	// && newWeapon != currentWeapon 
	if( newWeapon != NULL )
	{ 
		if( newWeapon != currentWeapon )
		{ lastWeapon = currentWeapon; }
		ServerCmd(newWeapon->szName);
		g_weaponselect = newWeapon->iId;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void WeaponsResource :: SelectSlot( int iSlot, int fAdvance, int iDirection )
{
	if ( gHUD.m_Menu.m_fMenuDisplayed && (fAdvance == FALSE) && (iDirection == 1) )	
	{ // menu is overriding slot use commands
		gHUD.m_Menu.SelectMenuItem( iSlot + 1 );  // slots are one off the key numbers
		return;
	}

	if ( iSlot >= MAX_WEAPON_SLOTS )
		return;
	
	if ( gHUD.m_fPlayerDead || gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) )
		return;
	
	if (!(gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT)) )) //require suit
		return;
	
	if ( ! ( gHUD.m_iWeaponBits & ~(1<<(WEAPON_SUIT)) )) //require something besides suit
		return;
	
	WEAPON *p = NULL;
	bool fastSwitch = CVAR_GET_FLOAT( "hud_fastswitch" ) != 0;
	if ((gpActiveSel == NULL) || (gpActiveSel == (WEAPON *)1) || (iSlot != gpActiveSel->iSlot))
	{
		p = GetFirstPos(iSlot);
		const char* theSound = AvHSHUGetCommonSoundName(gHUD.GetIsAlien(), WEAPON_SOUND_HUD_ON);
		gHUD.PlayHUDSound(theSound, kHUDSoundVolume);

		if (this->IsSelectable(p) && fastSwitch) //check to see if we can use fastSwitch
		{
			WEAPON *p2 = GetNextActivePos( p->iSlot, p->iSlotPos );
			if (!this->IsSelectable(p2)) //only one target in the bucket
			{
				this->SetCurrentWeapon(p);
				return;
			}
		}
	}
	else
	{
		const char* theSound = AvHSHUGetCommonSoundName(gHUD.GetIsAlien(), WEAPON_SOUND_MOVE_SELECT);
		gHUD.PlayHUDSound(theSound, kHUDSoundVolume);

		if ( gpActiveSel )
			p = GetNextActivePos( gpActiveSel->iSlot, gpActiveSel->iSlotPos );
		if ( !p )
			p = GetFirstPos( iSlot );
	}
	
	if (!this->IsSelectable(p))  // no valid selection found
	{
		// if fastSwitch is on, ignore, else turn on the menu
		if ( !fastSwitch ) {
			gpActiveSel = (WEAPON *)1;
		}
		else {
			gpActiveSel = NULL;
		}
	}
	else 
	{
		gpActiveSel = p;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int giBucketHeight, giBucketWidth, giABHeight, giABWidth; // Ammo Bar width and height

HSPRITE ghsprBuckets;					// Sprite for top row of weapons menu

DECLARE_MESSAGE(m_Ammo, CurWeapon );	// Current weapon and clip
DECLARE_MESSAGE(m_Ammo, WeaponList);	// new weapon type
DECLARE_MESSAGE(m_Ammo, AmmoX);			// update known ammo type's count
DECLARE_MESSAGE(m_Ammo, AmmoPickup);	// flashes an ammo pickup record
DECLARE_MESSAGE(m_Ammo, WeapPickup);    // flashes a weapon pickup record
DECLARE_MESSAGE(m_Ammo, HideWeapon);	// hides the weapon, ammo, and crosshair displays temporarily
DECLARE_MESSAGE(m_Ammo, ItemPickup);

DECLARE_COMMAND(m_Ammo, Slot1);
DECLARE_COMMAND(m_Ammo, Slot2);
DECLARE_COMMAND(m_Ammo, Slot3);
DECLARE_COMMAND(m_Ammo, Slot4);
DECLARE_COMMAND(m_Ammo, Slot5);
DECLARE_COMMAND(m_Ammo, Slot6);
DECLARE_COMMAND(m_Ammo, Slot7);
DECLARE_COMMAND(m_Ammo, Slot8);
DECLARE_COMMAND(m_Ammo, Slot9);
DECLARE_COMMAND(m_Ammo, Slot10);
DECLARE_COMMAND(m_Ammo, Close);
DECLARE_COMMAND(m_Ammo, NextWeapon);
DECLARE_COMMAND(m_Ammo, PrevWeapon);

// width of ammo fonts
#define AMMO_SMALL_WIDTH 10
#define AMMO_LARGE_WIDTH 20

#define HISTORY_DRAW_TIME	"5"

int CHudAmmo::Init(void)
{
	gHUD.AddHudElem(this);

	HOOK_MESSAGE(CurWeapon);
	HOOK_MESSAGE(WeaponList);
	HOOK_MESSAGE(AmmoPickup);
	HOOK_MESSAGE(WeapPickup);
	HOOK_MESSAGE(ItemPickup);
	HOOK_MESSAGE(HideWeapon);
	HOOK_MESSAGE(AmmoX);

	HOOK_COMMAND("slot1", Slot1);
	HOOK_COMMAND("slot2", Slot2);
	HOOK_COMMAND("slot3", Slot3);
	HOOK_COMMAND("slot4", Slot4);
	HOOK_COMMAND("slot5", Slot5);
	HOOK_COMMAND("slot6", Slot6);
	HOOK_COMMAND("slot7", Slot7);
	HOOK_COMMAND("slot8", Slot8);
	HOOK_COMMAND("slot9", Slot9);
	HOOK_COMMAND("slot10", Slot10);
	HOOK_COMMAND("cancelselect", Close);
	HOOK_COMMAND("invnext", NextWeapon);
	HOOK_COMMAND("invprev", PrevWeapon);

	Reset();

	CVAR_CREATE( "hud_drawhistory_time", HISTORY_DRAW_TIME, 0 );
	CVAR_CREATE( "hud_fastswitch", "0", FCVAR_ARCHIVE );		// controls whether or not weapons can be selected in one keypress

	m_iFlags |= HUD_ACTIVE; //!!!

	gWR.Init();
	gHR.Init();

	return 1;
};

void CHudAmmo::Reset(void)
{
	m_fFade = 0;
	m_iFlags |= HUD_ACTIVE; //!!!

	gpActiveSel = NULL;
	gHUD.m_iHideHUDDisplay = 0;

	gWR.Reset();
	gHR.Reset();

	m_customCrosshair=0;
	//	VidInit();

}

int CHudAmmo::VidInit(void)
{
	// Load sprites for buckets (top row of weapon menu)
	m_HUD_bucket0 = gHUD.GetSpriteIndex( "bucket1" );
	m_HUD_selection = gHUD.GetSpriteIndex( "selection" );

	ghsprBuckets = gHUD.GetSprite(m_HUD_bucket0);
	giBucketWidth = gHUD.GetSpriteRect(m_HUD_bucket0).right - gHUD.GetSpriteRect(m_HUD_bucket0).left;
	giBucketHeight = gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top;

	gHR.iHistoryGap = max( gHR.iHistoryGap, gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top);

	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	if (ScreenWidth() >= 640)
	{
		giABWidth = 20;
		giABHeight = 4;
	}
	else
	{
		giABWidth = 10;
		giABHeight = 2;
	}

	return 1;
}

//
// Think:
//  Used for selection of weapon menu item.
//

void CHudAmmo::Think(void)
{
	if ( gHUD.m_fPlayerDead )
		return;

	if ( gHUD.m_iWeaponBits != gWR.iOldWeaponBits )
	{
		gWR.iOldWeaponBits = gHUD.m_iWeaponBits;
		bool droppedCurrent = false;

		for (int i = MAX_WEAPONS-1; i > 0; i-- )
		{
			WEAPON *p = gWR.GetWeapon(i);

			if ( p )
			{
				if ( gHUD.m_iWeaponBits & ( 1 << p->iId ) )
					gWR.PickupWeapon( p );
				else
					gWR.DropWeapon( p );
			}
		}
	}
	if ( (int)CVAR_GET_FLOAT(kvCustomCrosshair) != m_customCrosshair ) {
		m_customCrosshair=(int)CVAR_GET_FLOAT(kvCustomCrosshair);
		for ( int i=0; i < MAX_WEAPONS; i++ ) {
			WEAPON *weapon = gWR.GetWeapon(i);
			if ( weapon ) {
				gWR.LoadWeaponSprites(weapon, m_customCrosshair);
				if ( gHUD.GetHUDPlayMode() != PLAYMODE_READYROOM && gHUD.GetCurrentWeaponID() == weapon->iId ) {
					gHUD.SetCurrentCrosshair(weapon->hCrosshair, weapon->rcCrosshair, 255, 255, 255);
				}
			}
		}

	}

	if(gHUD.GetIsAlien()) //check for hive death causing loss of current weapon
	{
		WEAPON* currentWeapon = gWR.GetWeapon(gHUD.GetCurrentWeaponID());
		if(!gWR.IsSelectable(currentWeapon)) //current weapon isn't valid
		{
			gWR.SetValidWeapon(); //get best option
		}
	}

	if (!gpActiveSel)
		return;

	// has the player selected one?
	if (gHUD.m_iKeyBits & IN_ATTACK)
	{
		if (gpActiveSel != (WEAPON *)1)
		{
			gWR.SetCurrentWeapon(gpActiveSel);
		}

		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		gHUD.m_iKeyBits &= ~IN_ATTACK;

		const char* theSound = AvHSHUGetCommonSoundName(gHUD.GetIsAlien(), WEAPON_SOUND_SELECT);
		gHUD.PlayHUDSound(theSound, kHUDSoundVolume);
	}
}


//------------------------------------------------------------------------
// Message Handlers
//------------------------------------------------------------------------

//
// AmmoX  -- Update the count of a known type of ammo
// 
int CHudAmmo::MsgFunc_AmmoX(const char *pszName, int iSize, void *pbuf)
{
	int iIndex, iCount;
	NetMsg_AmmoX( pbuf, iSize, iIndex, iCount );
	gWR.SetAmmo( iIndex, abs(iCount) );

	return 1;
}

int CHudAmmo::MsgFunc_AmmoPickup( const char *pszName, int iSize, void *pbuf )
{
	int iIndex, iCount;
	NetMsg_AmmoPickup( pbuf, iSize, iIndex, iCount );

	// Add ammo to the history
	gHR.AddToHistory( HISTSLOT_AMMO, iIndex, abs(iCount) );

	return 1;
}

int CHudAmmo::MsgFunc_WeapPickup( const char *pszName, int iSize, void *pbuf )
{
	int iIndex;
	NetMsg_WeapPickup( pbuf, iSize, iIndex );

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_WEAP, iIndex );

	return 1;
}

int CHudAmmo::MsgFunc_ItemPickup( const char *pszName, int iSize, void *pbuf )
{
	string szName;
	NetMsg_ItemPickup( pbuf, iSize, szName );

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_ITEM, szName.c_str() );

	return 1;
}


int CHudAmmo::MsgFunc_HideWeapon( const char *pszName, int iSize, void *pbuf )
{
	NetMsg_HideWeapon( pbuf, iSize, gHUD.m_iHideHUDDisplay );

	if (gEngfuncs.IsSpectateOnly())
		return 1;
	if ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) )
	{
		static wrect_t nullrc;
		gpActiveSel = NULL;
		gHUD.SetCurrentCrosshair( 0, nullrc, 0, 0, 0 );
	}
	else
	{
		if ( m_pWeapon )
			gHUD.SetCurrentCrosshair( m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255 );
	}

	return 1;
}

// 
//  CurWeapon: Update hud state with the current weapon and clip count. Ammo
//  counts are updated with AmmoX. Server assures that the Weapon ammo type 
//  numbers match a real ammo type.
//
int CHudAmmo::MsgFunc_CurWeapon(const char *pszName, int iSize, void *pbuf )
{
	static wrect_t nullrc;

	int iState, iId, iClip;
	NetMsg_CurWeapon( pbuf, iSize, iState, iId, iClip );

	if ( iId < 1 ) //signal kills crosshairs if this condition is met...
	{
		gHUD.SetCurrentCrosshair(0, nullrc, 0, 0, 0);
		return 0;
	}

	if ( g_iUser1 != OBS_IN_EYE )
	{
		if ( iId == -1 && iClip == -1 ) //this conditional is never true due to iId < 1 check above!
		{
			gHUD.m_fPlayerDead = TRUE;
			gpActiveSel = NULL;
			return 1;
		}

		gHUD.m_fPlayerDead = FALSE; 
	}

	WEAPON *pWeapon = gWR.GetWeapon( iId );
	if( pWeapon == NULL ) //don't have the weapon described in our resource list
	{ return 0; }

	bool bOnTarget = (iState & WEAPON_ON_TARGET) != 0;	//used to track autoaim state
	bool bIsCurrent = (iState & WEAPON_IS_CURRENT) != 0;
	pWeapon->iEnabled = (iState & WEAPON_IS_ENABLED) != 0 ? TRUE : FALSE;
	pWeapon->iClip = abs(iClip);

	// Ensure that movement is enabled/disabled according to weapons
	if (iId == 22 || iId == 11 || iId == 21)
	{
		gCanMove = pWeapon->iEnabled;
	}

	if( !bIsCurrent )
	{ return 1; }

	m_pWeapon = pWeapon;

	if ( !(gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL )) )
	{
		gHUD.SetCurrentCrosshair(m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255);

/*		if ( gHUD.m_iFOV >= 90 )
		{ // normal crosshairs
			if (bOnTarget && m_pWeapon->hAutoaim)
				gHUD.SetCurrentCrosshair(m_pWeapon->hAutoaim, m_pWeapon->rcAutoaim, 255, 255, 255);
			else
				gHUD.SetCurrentCrosshair(m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255);
		}
		else
		{ // zoomed crosshairs
			if (bOnTarget && m_pWeapon->hZoomedAutoaim)
				gHUD.SetCurrentCrosshair(m_pWeapon->hZoomedAutoaim, m_pWeapon->rcZoomedAutoaim, 255, 255, 255);
			else
				gHUD.SetCurrentCrosshair(m_pWeapon->hZoomedCrosshair, m_pWeapon->rcZoomedCrosshair, 255, 255, 255);
		}*/
	}

	m_fFade = 200.0f; //!!!
	m_iFlags |= HUD_ACTIVE;
	
	return 1;
}

//
// WeaponList -- Tells the hud about a new weapon type.
//
int CHudAmmo::MsgFunc_WeaponList(const char *pszName, int iSize, void *pbuf )
{
	WeaponList weapon_data;
	NetMsg_WeaponList( pbuf, iSize, weapon_data );

	WEAPON Weapon;
	memset( &Weapon, 0, sizeof(WEAPON) );

	strcpy( Weapon.szName, weapon_data.weapon_name.c_str() );
	Weapon.iAmmoType = weapon_data.ammo1_type;	
	Weapon.iMax1 = weapon_data.ammo1_max_amnt == 255 ? -1 : weapon_data.ammo1_max_amnt;
    Weapon.iAmmo2Type = weapon_data.ammo2_type;
	Weapon.iMax2 = weapon_data.ammo2_max_amnt == 255 ? -1 : weapon_data.ammo2_max_amnt;
	Weapon.iSlot = weapon_data.bucket;
	Weapon.iSlotPos = weapon_data.bucket_pos;
	Weapon.iId = weapon_data.bit_index;
	Weapon.iFlags = weapon_data.flags;
	Weapon.iClip = 0;
	// : 497 - default value for enable state
	Weapon.iEnabled = 0;

	gWR.AddWeapon( &Weapon );
	return 1;
}

//------------------------------------------------------------------------
// Command Handlers
//------------------------------------------------------------------------
// Slot button pressed
void CHudAmmo::SlotInput( int iSlot )
{
	// Let the Viewport use it first, for menus
	if ( gViewPort && gViewPort->SlotInput( iSlot ) )
		return;

	gWR.SelectSlot( iSlot, FALSE, 1 );
}

void CHudAmmo::UserCmd_Slot1(void)
{
	SlotInput( 0 );
}

void CHudAmmo::UserCmd_Slot2(void)
{
	SlotInput( 1 );
}

void CHudAmmo::UserCmd_Slot3(void)
{
	SlotInput( 2 );
}

void CHudAmmo::UserCmd_Slot4(void)
{
	SlotInput( 3 );
}

void CHudAmmo::UserCmd_Slot5(void)
{
	SlotInput( 4 );
}

void CHudAmmo::UserCmd_Slot6(void)
{
	SlotInput( 5 );
}

void CHudAmmo::UserCmd_Slot7(void)
{
	SlotInput( 6 );
}

void CHudAmmo::UserCmd_Slot8(void)
{
	SlotInput( 7 );
}

void CHudAmmo::UserCmd_Slot9(void)
{
	SlotInput( 8 );
}

void CHudAmmo::UserCmd_Slot10(void)
{
	SlotInput( 9 );
}

void CHudAmmo::UserCmd_Close(void)
{
	if (gpActiveSel)
	{
		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;

		const char* theSound = AvHSHUGetCommonSoundName(gHUD.GetIsAlien(), WEAPON_SOUND_HUD_OFF);
		gHUD.PlayHUDSound(theSound, kHUDSoundVolume);
	}
}


// Selects the next item in the weapon menu
void CHudAmmo::UserCmd_NextWeapon(void)
{
	if(gHUD.GetInTopDownMode())
	{
		AvHScrollHandler::ScrollHeightUp();
	}

	if ( gHUD.m_fPlayerDead || (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)) )
		return;

	if ( !gpActiveSel || gpActiveSel == (WEAPON*)1 )
		gpActiveSel = m_pWeapon;

	int pos = 0;
	int slot = 0;
	if ( gpActiveSel )
	{
		pos = gpActiveSel->iSlotPos + 1;
		slot = gpActiveSel->iSlot;
	}

	for ( int loop = 0; loop <= 1; loop++ )
	{
		for ( ; slot < MAX_WEAPON_SLOTS; slot++ )
		{
			for ( ; pos < MAX_WEAPON_POSITIONS; pos++ )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if (gWR.IsSelectable(wsp))
				{
					gpActiveSel = wsp;
					return;
				}
			}

			pos = 0;
		}

		slot = 0;  // start looking from the first slot again
	}

	gpActiveSel = NULL;
}

// Selects the previous item in the menu
void CHudAmmo::UserCmd_PrevWeapon(void)
{
	if(gHUD.GetInTopDownMode())
	{
		AvHScrollHandler::ScrollHeightDown();
	}
	
	if ( gHUD.m_fPlayerDead || (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)) )
		return;

	if ( !gpActiveSel || gpActiveSel == (WEAPON*)1 )
		gpActiveSel = m_pWeapon;

	int pos = MAX_WEAPON_POSITIONS-1;
	int slot = MAX_WEAPON_SLOTS-1;
	if ( gpActiveSel )
	{
		pos = gpActiveSel->iSlotPos - 1;
		slot = gpActiveSel->iSlot;
	}
	
	for ( int loop = 0; loop <= 1; loop++ )
	{
		for ( ; slot >= 0; slot-- )
		{
			for ( ; pos >= 0; pos-- )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if (gWR.IsSelectable(wsp))
				{
					gpActiveSel = wsp;
					return;
				}
			}

			pos = MAX_WEAPON_POSITIONS-1;
		}
		
		slot = MAX_WEAPON_SLOTS-1;
	}

	gpActiveSel = NULL;
}

void CHudAmmo::SetCurrentClip(int inClip)
{
	if(this->m_pWeapon)
	{
		this->m_pWeapon->iClip = inClip;
	}
}

//-------------------------------------------------------------------------
// Drawing code
//-------------------------------------------------------------------------

int CHudAmmo::Draw(float flTime)
{
	int a, x, y, r, g, b;
	int AmmoWidth;

	if (!(gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT)) ))
		return 1;

	if (/*!gHUD.GetIsAlive() ||*/ (gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL )) )
		return 1;

	// Draw Weapon Menu
	DrawWList(flTime);

	// Draw ammo pickup history
	gHR.DrawAmmoHistory( flTime );

	if (!(m_iFlags & HUD_ACTIVE))
		return 0;

	if (!m_pWeapon)
		return 0;

	WEAPON *pw = m_pWeapon; // shorthand

	// SPR_Draw Ammo
	if ((pw->iAmmoType < 0) && (pw->iAmmo2Type < 0))
		return 0;


	int iFlags = DHN_DRAWZERO; // draw 0 values

	AmmoWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;

	a = (int) max( MIN_ALPHA, m_fFade );

	if (m_fFade > 0)
		m_fFade -= (gHUD.m_flTimeDelta * 20);

	gHUD.GetPrimaryHudColor(r, g, b);
	
	ScaleColors(r, g, b, a );

    int theViewport[4];
    gHUD.GetViewport(theViewport);

	// Does this weapon have a clip?
	y = theViewport[1] + theViewport[3] - gHUD.m_iFontHeight - gHUD.m_iFontHeight/2;

	// Does weapon have any ammo at all?
	if (m_pWeapon->iAmmoType > 0)
	{
		int iIconWidth = m_pWeapon->rcAmmo.right - m_pWeapon->rcAmmo.left;
		
		if (pw->iClip >= 0)
		{
			// room for the number and the '|' and the current ammo
			
			x = theViewport[0] + theViewport[2] - (8 * AmmoWidth) - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, pw->iClip, r, g, b);

			wrect_t rc;
			rc.top = 0;
			rc.left = 0;
			rc.right = AmmoWidth;
			rc.bottom = 100;

			int iBarWidth =  AmmoWidth/10;

			x += AmmoWidth/2;

			gHUD.GetPrimaryHudColor(r, g, b);
			
			// draw the | bar
			FillRGBA(x, y, iBarWidth, gHUD.m_iFontHeight, r, g, b, a);

			x += iBarWidth + AmmoWidth/2;;

			// GL Seems to need this
			ScaleColors(r, g, b, a );
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);		


		}
		else
		{
			// SPR_Draw a bullets only line
			x = theViewport[0] + theViewport[2]  - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);
		}

		// Draw the ammo Icon
		int iOffset = (m_pWeapon->rcAmmo.bottom - m_pWeapon->rcAmmo.top)/8;
		SPR_Set(m_pWeapon->hAmmo, r, g, b);
		SPR_DrawAdditive(0, x, y - iOffset, &m_pWeapon->rcAmmo);
	}

	// Does weapon have seconday ammo?
	if (pw->iAmmo2Type > 0) 
	{
		int iIconWidth = m_pWeapon->rcAmmo2.right - m_pWeapon->rcAmmo2.left;

		// Do we have secondary ammo?
		if ((pw->iAmmo2Type != 0) && (gWR.CountAmmo(pw->iAmmo2Type) > 0))
		{
			y -= gHUD.m_iFontHeight + gHUD.m_iFontHeight/4;
			x = theViewport[0] + theViewport[2] - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags|DHN_3DIGITS, gWR.CountAmmo(pw->iAmmo2Type), r, g, b);

			// Draw the ammo Icon
			SPR_Set(m_pWeapon->hAmmo2, r, g, b);
			int iOffset = (m_pWeapon->rcAmmo2.bottom - m_pWeapon->rcAmmo2.top)/8;
			SPR_DrawAdditive(0, x, y - iOffset, &m_pWeapon->rcAmmo2);
		}
	}
	return 1;
}


//
// Draws the ammo bar on the hud
//
int DrawBar(int x, int y, int width, int height, float f)
{
	int r, g, b;

	if (f < 0)
		f = 0;
	if (f > 1)
		f = 1;

	if (f)
	{
		int w = f * width;

		// Always show at least one pixel if we have ammo.
		if (w <= 0)
			w = 1;
		UnpackRGB(r, g, b, RGB_GREENISH);
		FillRGBA(x, y, w, height, r, g, b, 255);
		x += w;
		width -= w;
	}

	gHUD.GetPrimaryHudColor(r, g, b);
	FillRGBA(x, y, width, height, r, g, b, 128);

	return (x + width);
}



void DrawAmmoBar(WEAPON *p, int x, int y, int width, int height)
{
	if ( !p )
		return;
	
	if (p->iAmmoType != -1)
	{
		if (!gWR.CountAmmo(p->iAmmoType))
			return;

		float f = (float)gWR.CountAmmo(p->iAmmoType)/(float)p->iMax1;
		
		x = DrawBar(x, y, width, height, f);


		// Do we have secondary ammo too?

		if (p->iAmmo2Type != -1)
		{
			f = (float)gWR.CountAmmo(p->iAmmo2Type)/(float)p->iMax2;

			x += 5; //!!!

			DrawBar(x, y, width, height, f);
		}
	}
}




//
// Draw Weapon Menu
//
int CHudAmmo::DrawWList(float flTime)
{
	int r,g,b,x,y,a,i;

	if ( !gpActiveSel )
	{
		gHUD.SetSelectingWeaponID(-1);
		return 0;
	}

	int iActiveSlot;

	if ( gpActiveSel == (WEAPON *)1 )
		iActiveSlot = -1;	// current slot has no weapons
	else 
		iActiveSlot = gpActiveSel->iSlot;

	x = 10; //!!!
	y = 10; //!!!
	

	// Ensure that there are available choices in the active slot
	if ( iActiveSlot > 0 )
	{
		if ( !gWR.GetFirstPos( iActiveSlot ) )
		{
			gpActiveSel = (WEAPON *)1;
			iActiveSlot = -1;
		}
	}
		
	// Draw top line
	for ( i = 0; i < MAX_WEAPON_SLOTS; i++ )
	{
		int iWidth;

		gHUD.GetPrimaryHudColor(r, g, b);

		if ( iActiveSlot == i )
			a = 255;
		else
			a = 192;

		ScaleColors(r, g, b, 255);
		SPR_Set(gHUD.GetSprite(m_HUD_bucket0 + i), r, g, b );

		// make active slot wide enough to accomodate gun pictures
		if ( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos(iActiveSlot);
			if ( p )
				iWidth = p->rcActive.right - p->rcActive.left;
			else
				iWidth = giBucketWidth;
		}
		else
			iWidth = giBucketWidth;

		SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_bucket0 + i));
		
		x += iWidth + 5;
	}


	a = 128; //!!!
	x = 10;

	// Draw all of the buckets
	for (i = 0; i < MAX_WEAPON_SLOTS; i++)
	{
		y = giBucketHeight + 10;

		// If this is the active slot, draw the bigger pictures,
		// otherwise just draw boxes
		if ( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos( i );
			int iWidth = giBucketWidth;
			if ( p )
				iWidth = p->rcActive.right - p->rcActive.left;

			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				p = gWR.GetWeaponSlot( i, iPos );

				if ( !p || !p->iId )
					continue;

				// Preserve red/yellow depending on whether it has ammo or not
				if(!gWR.IsEnabled(p))
				{
					UnpackRGB(r,g,b, RGB_REDISH);
					ScaleColors(r, g, b, 128);
				}
				else
				{
					gHUD.GetPrimaryHudColor(r, g, b);
					ScaleColors(r, g, b, 192);
				}

				if ( gpActiveSel == p )
				{
					SPR_Set(p->hActive, r, g, b );
					SPR_DrawAdditive(0, x, y, &p->rcActive);

					SPR_Set(gHUD.GetSprite(m_HUD_selection), r, g, b );
					SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_selection));

					// Lookup iID for helptext
					gHUD.SetSelectingWeaponID(p->iId, r, g, b);
				}
				else
				{
					// Draw Weapon if Red if no ammo

					//if (gWR.IsSelectable(p))
					//	ScaleColors(r, g, b, 192);
					//else
					//{
					//	UnpackRGB(r,g,b, RGB_REDISH);
					//	ScaleColors(r, g, b, 128);
					//}

					SPR_Set( p->hInactive, r, g, b );
					SPR_DrawAdditive( 0, x, y, &p->rcInactive );
				}

				// Draw Ammo Bar

				DrawAmmoBar(p, x + giABWidth/2, y, giABWidth, giABHeight);
				
				y += p->rcActive.bottom - p->rcActive.top + 5;
			}

			x += iWidth + 5;

		}
		else
		{
			// Draw Row of weapons.
			gHUD.GetPrimaryHudColor(r, g, b);
			
			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				WEAPON *p = gWR.GetWeaponSlot( i, iPos );
				
				if ( !p || !p->iId )
					continue;

				if ( gWR.IsEnabled(p) )
				{
					gHUD.GetPrimaryHudColor(r, g, b);
					a = 128;
				}
				else
				{
					UnpackRGB(r,g,b, RGB_REDISH);
					a = 96;
				}

				FillRGBA( x, y, giBucketWidth, giBucketHeight, r, g, b, a );

				y += giBucketHeight + 5;
			}

			x += giBucketWidth + 5;
		}
	}	

	return 1;

}


/* =================================
	GetSpriteList

Finds and returns the matching 
sprite name 'psz' and resolution 'iRes'
in the given sprite list 'pList'
iCount is the number of items in the pList
================================= */
client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount)
{
	if (!pList)
		return NULL;

	int i = iCount;
	client_sprite_t *p = pList;

	while(i--)
	{
		if ((!strcmp(psz, p->szName)) && (p->iRes == iRes))
			return p;
		p++;
	}

	return NULL;
}
