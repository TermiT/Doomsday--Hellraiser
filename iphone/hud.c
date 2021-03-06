/*
 
 Copyright (C) 2009 Id Software, Inc.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
 */

#include "../doomiphone.h"

hud_t	huds;

void HudDraw();

void HudWrite();

void HudRead();

ibutton_t	*dragHud;
int			dragX, dragY;

void SetHudPic( ibutton_t *hp, const char *image ) {
	pkTexture_t *gl;
	gl = PK_FindTexture( image );
	assert( gl );
	hp->texture = gl;
	hp->touch = NULL;	// in case one was down when it was saved
}

void SetHudSpot( ibutton_t *hp, int x, int y, int dw, int dh ) {
	hp->touch = NULL;	// in case one was down when it was saved	
	hp->x = x - dw/2;
	hp->y = y - dh/2;
	hp->drawWidth = dw;
	hp->drawHeight = dh;
	hp->buttonFlags = 0;
	hp->scale = 1.0f;
}

void HudSetTexnums() {
	SetHudPic( &huds.forwardStick, "iphone/up_down.tga" );
	SetHudPic( &huds.sideStick, "iphone/side_2_side.tga" );
	SetHudPic( &huds.turnStick, "iphone/directional_2.tga" );
	SetHudPic( &huds.turnRotor, "iphone/rotate.tga" );
	SetHudPic( &huds.fire, "iphone/fire.tga" );
	SetHudPic( &huds.menu, "iphone/menu_button.tga" );
	SetHudPic( &huds.map, "iphone/map_button.tga" );
	
	#ifdef IPAD
	SetHudSpot( &huds.weaponSelect, 512, 708, 60, 60 );	
	#else
	SetHudSpot( &huds.weaponSelect, 240, 280, 40, 40 );	
	#endif
}

void HudSetForScheme( int schemeNum ) {
	for ( ibutton_t *hud = (ibutton_t *)&huds ; hud != (ibutton_t *)(&huds+1) ; hud++ ) {
		hud->buttonFlags = BF_IGNORE;
	}
	static const int STICK_SIZE = 128;
	static const int HALF_STICK = 128/2;
	
	#ifdef IPAD
	static const int BOTTOM = 768 - 130;// above the status bar
	#else
	static const int BOTTOM = 320 - 44;	// above the status bar
	#endif
	
	
	#ifdef IPAD
	SetHudSpot( &huds.weaponSelect, 512, 708, 60, 60 );	
	#else
	SetHudSpot( &huds.weaponSelect, 240, 280, 40, 40 );	// the touch area is doubled
	#endif

	
	// make the forward / back sticks touch taller than they draw
	switch ( schemeNum ) {
		default:
		case 0:		// turn stick
			SetHudSpot( &huds.forwardStick, HALF_STICK, BOTTOM-HALF_STICK, STICK_SIZE, STICK_SIZE );
			SetHudSpot( &huds.turnStick, HALF_STICK, BOTTOM-HALF_STICK, STICK_SIZE, STICK_SIZE );
			#ifdef IPAD
			SetHudSpot( &huds.fire, 1024-60, BOTTOM-HALF_STICK, 80, 80 );
			SetHudSpot( &huds.menu, 1024-29, 29, 48, 48 );
			SetHudSpot( &huds.map, 29, 29, 48, 48 );
			#else
			SetHudSpot( &huds.fire, 480-40, BOTTOM-HALF_STICK, 80, 80 );
			SetHudSpot( &huds.menu, 480-24, 24, 48, 48 );
			SetHudSpot( &huds.map, 24, 24, 48, 48 );
			#endif
			break;
		case 1:		// dual stick
			SetHudSpot( &huds.forwardStick, HALF_STICK, BOTTOM-HALF_STICK, STICK_SIZE, STICK_SIZE );
			SetHudSpot( &huds.sideStick, HALF_STICK, BOTTOM-HALF_STICK, STICK_SIZE, STICK_SIZE );
			#ifdef IPAD
			SetHudSpot( &huds.turnStick, 1024-HALF_STICK, BOTTOM-HALF_STICK, STICK_SIZE, STICK_SIZE );
			SetHudSpot( &huds.fire, 1024-60, 60, 80, 80 );	
			SetHudSpot( &huds.menu, 58+29, 29, 48, 48 );
			SetHudSpot( &huds.map, 29, 29, 48, 48 );
			#else
			SetHudSpot( &huds.turnStick, 1024-HALF_STICK, BOTTOM-HALF_STICK, STICK_SIZE, STICK_SIZE );
			SetHudSpot( &huds.fire, 480-40, 40, 80, 80 );	
			SetHudSpot( &huds.menu, 48+24, 24, 48, 48 );
			SetHudSpot( &huds.map, 24, 24, 48, 48 );			
			#endif
			break;
		case 2:		// rotor
			SetHudSpot( &huds.forwardStick, HALF_STICK, BOTTOM-HALF_STICK, STICK_SIZE, STICK_SIZE );
			SetHudSpot( &huds.sideStick, HALF_STICK, BOTTOM-HALF_STICK, STICK_SIZE, STICK_SIZE );
			#ifdef IPAD
			SetHudSpot( &huds.turnRotor, 1024-HALF_STICK, BOTTOM-HALF_STICK, STICK_SIZE, STICK_SIZE );
			SetHudSpot( &huds.fire, 1024-60, 60, 80, 80 );	
			SetHudSpot( &huds.menu, 58+29, 29, 48, 48 );
			SetHudSpot( &huds.map, 29, 29, 48, 48 );			
			#else
			SetHudSpot( &huds.turnRotor, 480-HALF_STICK, BOTTOM-HALF_STICK, STICK_SIZE, STICK_SIZE );
			SetHudSpot( &huds.fire, 480-40, 40, 80, 80 );	
			SetHudSpot( &huds.menu, 48+24, 24, 48, 48 );
			SetHudSpot( &huds.map, 24, 24, 48, 48 );			
			#endif

			break;
	}
	
	// don't process these in the update hud touch loop, because they will be
	// handled with normal button calls
	huds.menu.buttonFlags |= BF_HUDBUTTON;
	huds.map.buttonFlags |= BF_HUDBUTTON;
	
	// don't make the big button click sound for the fire button
	huds.fire.buttonFlags |= BF_SMALL_CLICK;
	
}

void SnapSticks( ibutton_t *test, const ibutton_t *to ) {
	if ( abs( test->x - to->x ) < test->drawWidth && abs( test->y - to->y ) < test->drawHeight ) {
		test->x = to->x;
		test->y = to->y;
	}
}

/*
 ==================
 HudEditFrame
 
 ==================
 */
void HudEditFrame() {
	color3_t gray = { 32, 32, 32 };
		
	if ( numTouches == 0 && numPrevTouches == 1 && dragHud ) {
		Sound_StartLocalSound( "iphone/baction_01.wav" );
		dragHud = NULL;
	}
	
	if ( numTouches == 1 && numPrevTouches == 0 ) {
		// identify the hud being touched for drag
		int x = touches[0][0];
		int y = touches[0][1];
		dragHud = NULL;
		for ( ibutton_t *hud = (ibutton_t *)&huds ; hud != (ibutton_t *)(&huds+1) ; hud++ ) {
			if ( hud->buttonFlags & BF_IGNORE ) {
				continue;
			}
			if ( x >= hud->x && x - hud->x < hud->drawWidth && y >= hud->y && y - hud->y < hud->drawHeight ) {
				dragHud = hud;
				dragX = dragHud->x - x;
				dragY = dragHud->y - y;
				Sound_StartLocalSound( "iphone/bdown_01.wav" );
				break;
			}
		}
	}
	
	if ( numTouches == 1 && numPrevTouches == 1 && dragHud ) {
		// adjust the position of the dragHud
		dragHud->x = touches[0][0] + dragX;
		dragHud->y = touches[0][1] + dragY;
		if ( dragHud->x < 0 ) {
			dragHud->x = 0;
		}
		#ifdef IPAD
		if ( dragHud->x > 1024 - dragHud->drawWidth ) {
			dragHud->x = 1024 - dragHud->drawWidth;
		}		
		#else
		if ( dragHud->x > 480 - dragHud->drawWidth ) {
			dragHud->x = 480 - dragHud->drawWidth;
		}
		#endif
		
		if ( dragHud->y < 0 ) {
			dragHud->y = 0;
		}

		#ifdef IPAD
		if ( dragHud->y > 768 - dragHud->drawHeight ) {
			dragHud->y = 768 - dragHud->drawHeight;
		}
		#else
		if ( dragHud->y > 320 - dragHud->drawHeight ) {
			dragHud->y = 320 - dragHud->drawHeight;
		}		
		#endif
		
		// magnet pull a matchable axis
		if ( controlScheme->value == 0 ) {
			if ( dragHud == &huds.forwardStick ) {
				SnapSticks( &huds.turnStick, dragHud );				
			} 
		} else {
			if ( dragHud == &huds.forwardStick ) {
				SnapSticks( &huds.sideStick, dragHud );
			}
		}
	}
	
	// solid background color and some UI elements for context
	#ifdef IPAD
	R_Draw_Fill( 0, 0, 1024, 768, gray );	
	glColor4f( 1, 1, 1, 1 );
	iphoneCenterText( 512, 33, 1, "Drag the controls" );
	#else
	R_Draw_Fill( 0, 0, 480, 320, gray );	
	glColor4f( 1, 1, 1, 1 );
	iphoneCenterText( 240, 20, 0.75, "Drag the controls" );	
	#endif
	
	// draw the status bar
	extern patchnum_t stbarbg;
	if ( statusBar->value ) {
		// force doom to rebind, since we have changed the active GL_TEXTURE_2D
		last_gltexture = NULL;
		gld_DrawNumPatch(0, ST_Y, stbarbg.lumpnum, CR_DEFAULT, VPT_STRETCH);
	}
	
	// draw the active items at their current locations
	for ( ibutton_t *hud = (ibutton_t *)&huds ; hud != (ibutton_t *)(&huds+1) ; hud++ ) {
		if ( !hud->texture ) {
			continue;
		}
		if ( hud->buttonFlags & BF_IGNORE ) {
			continue;
		}
		PK_StretchTexture( hud->texture, hud->x, hud->y, hud->drawWidth, hud->drawHeight );
	}
	
	// draw the done button
	static ibutton_t btnDone;	
	if ( !btnDone.texture ) {
		// initial setup
		#ifdef IPAD
		SetButtonPicsAndSizes( &btnDone, "iphone/back_button.tga", "Done", 512 - 32, 384-32, 64, 64 );
		#else
		SetButtonPicsAndSizes( &btnDone, "iphone/back_button.tga", "Done", 240 - 32, 160-32, 64, 64 );
		#endif
	}
	if ( HandleButton( &btnDone ) ) {
		menuState = IPM_CONTROLS;
	}
	
}

