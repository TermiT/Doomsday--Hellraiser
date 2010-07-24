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

// Only one game can be set up at a time on a given wireless segment, although
// several independent games can be played.
// If a valid setupPacket has arrived in the last second, that will be the
// displayed game, otherwise the local system starts sending out setupPackets.
packetSetup_t	setupPacket;
int				setupPacketFrameNum;
int				localGameID;	// change every time we take over as the sender of setupPackets

boolean	levelHasBeenLoaded;	// determines if "resume game" does a loadGame and exiting does a saveGame

menuState_t menuState;
color4_t highlightColor = { 128, 128, 128, 255 };
color4_t colorPressed = { 128, 128, 0, 255 };

void SetupEmptyNetGame();

void R_Draw_Blend( int x, int y, int w, int h, color4_t c ) {
	glDisable( GL_TEXTURE_2D );	
	glColor4ubv( c );
	
	glBegin( GL_QUADS );
	
	glVertex2i( x, y );
	glVertex2i( x+w, y );
	glVertex2i( x+w, y+h );
	glVertex2i( x, y+h );
	
	glEnd();
	
	glColor3f( 1, 1, 1 );
	glEnable( GL_TEXTURE_2D );
}


void R_Draw_Fill( int x, int y, int w, int h, color3_t c ) {
	// as of 2.2 OS, doing a clear with a small scissor rect is MUCH slower
	// than drawing geometry
	color4_t	c4;
	c4[0] = c[0];
	c4[1] = c[1];
	c4[2] = c[2];
	c4[3] = 255;
	R_Draw_Blend( x, y, w, h, c4 );
}

/*
 ==================
 iphoneSlider
 
 Returns true if modified
 ==================
 */
#define SF_DISABLED		1		// grey out, don't respond to touches
#define SF_INTEGER		2		// don't add percent
boolean iphoneSlider( int x, int y, int w, int h, const char *title, cvar_t *cvar, 
				  float min, float max, int sliderFlags ) {
	float value = cvar->value;
	char	str[80];
	float	f = ( value - min ) / ( max - min );
	
	if ( f < 0 ) {
		f = 0;
	}
	if ( f > 1 ) {
		f = 1;
	}
	
	// draw the background
	PK_StretchTexture( PK_FindTexture( "iphone/slider_shadow.tga" ), x, y, w, h );
	
	// draw the current range
	PK_BindTexture( PK_FindTexture( "iphone/slider_bar.tga" ) );
#ifdef IPAD
	// proportional
	glBegin( GL_QUADS );
	
	glTexCoord2f( 0.0f, 0.0f );	glVertex2i( x, y );
	glTexCoord2f( f, 0.0f );	glVertex2i( x+w*f, y );
	glTexCoord2f( f, 1.0f );	glVertex2i( x+w*f, y+h );
	glTexCoord2f( 0.0f, 1.0f );	glVertex2i( x, y+h );
	
	glEnd();
#else
	// dragging thumb
	glBegin( GL_QUADS );
	
	glTexCoord2f( 0.0f, 0.0f );	glVertex2i( x+w*f-8, y );
	glTexCoord2f( 1.0f, 0.0f );	glVertex2i( x+w*f+8, y );
	glTexCoord2f( 1.0f, 1.0f );	glVertex2i( x+w*f+8, y+h );
	glTexCoord2f( 0.0f, 1.0f );	glVertex2i( x+w*f-8, y+h );
	
	glEnd();	
#endif
	
	// draw the title and fraction
	if ( sliderFlags & SF_INTEGER ) {
		sprintf( str, "%s : %i", title, (int)value );
	} else {
		sprintf( str, "%s : %i%%", title, (int)(f*100+0.5) );
	}
	iphoneCenterText( x+ w/2, y+h-10, 0.75, str );
	
	// check for touches
	if ( numTouches > 0 && touches[0][0] >= x && touches[0][0] < x + w
		&& touches[0][1] >= y && touches[0][1] < y+ h ) {
		float newValue;
		float	delta;
		
		f = (float)( touches[0][0] - x ) / w;
		
		if ( sliderFlags & SF_INTEGER ) {
			newValue = rint( min + f * ( max - min ) );
		} else {
			// round to tenths
			f = (int)( ( f + 0.05 ) * 10 ) * 0.1f;
			if ( f < 0 ) {
				f = 0;
			}
			if ( f > 1.0 ) {
				f = 1.0;
			}			
			newValue = min + f * ( max - min );
		}
		
		delta = fabs( newValue - cvar->value );
		if ( f == 0 && cvar->value == 0 ) {
			// special case of disable-at-0
		} else if ( delta > 0.01 ) {
			Cvar_SetValue( cvar->name, newValue );
			Sound_StartLocalSound( "iphone/slide_01.wav" );
			return true;
		}
		return false;
	}
	
	return false;
}

/*
 ==================
 BackButton
 
 ==================
 */
ibutton_t	btnBack;
int BackButton() {
	if ( !btnBack.texture ) {
		#ifdef IPAD
		SetButtonPicsAndSizes( &btnBack, "iphone/back_button.tga", "", 5, 5, 48, 48 );
		#else
		SetButtonPicsAndSizes( &btnBack, "iphone/back_button.tga", "", 0, 0, 48, 48 );
		#endif
	}
	return HandleButton( &btnBack );
}	

void GetMoreLevels( int x, int y ) {
	// directly to the app store for more levels
	SysIPhoneOpenURL( "http://phobos.apple.com/WebObjects/MZStore.woa/wa/viewSoftware?id=304694876" );
}


/*
 ==================
 iphoneMainMenu
 
 ==================
 */
ibutton_t	btnResumeGame;
ibutton_t	btnNewGame;
ibutton_t	btnControls;
ibutton_t	btnMultiplayer;
ibutton_t	btnWWW;
#ifndef IPAD
ibutton_t	btnDemo;
#endif
#ifdef IPAD
ibutton_t   btnLabel1;
ibutton_t   btnLabel2;
ibutton_t   btnLabel3;
ibutton_t   btnLabel4;
#endif

void iphoneMainMenu() {	
	if ( !btnResumeGame.texture ) {
		// initial setup
		#ifdef IPAD
		SetButtonPics( &btnResumeGame, "iphone/resume_game.tga", "Resume Game", 216, 338 );
		SetButtonPics( &btnNewGame, "iphone/new_game.tga", "New Game", 446, 338 );
		SetButtonPics( &btnWWW, "iphone/website.tga",  "Website", 676, 338 );
		SetButtonPics( &btnMultiplayer, "iphone/multiplay.tga",  "Multiplayer", 347-17, 568 );
		SetButtonPics( &btnControls, "iphone/controls.tga", "Options", 545+17, 568 );	
		
		SetButtonPics( &btnLabel1, "iphone/label1.tga", "", 0,   20 );
		SetButtonPics( &btnLabel2, "iphone/label2.tga", "", 256, 20 );
		SetButtonPics( &btnLabel3, "iphone/label3.tga", "", 512, 20 );
		SetButtonPics( &btnLabel4, "iphone/label4.tga", "", 768, 20 );

		btnLabel1.buttonFlags = BF_INACTIVE;
		btnLabel2.buttonFlags = BF_INACTIVE;
		btnLabel3.buttonFlags = BF_INACTIVE;
		btnLabel4.buttonFlags = BF_INACTIVE;
		
		#else
		SetButtonPics( &btnResumeGame, "iphone/resume_game.tga", "Resume Game", 16, 4 );
		SetButtonPics( &btnNewGame, "iphone/new_game.tga", "New Game", 176, 4 );
		SetButtonPics( &btnDemo, "iphone/demo.tga", "Demos", 336, 4 );
		SetButtonPics( &btnMultiplayer, "iphone/multiplay.tga",  "Multiplayer", 16, 168 );
		SetButtonPics( &btnWWW, "iphone/website.tga",  "Website", 176, 168 );
		SetButtonPics( &btnControls, "iphone/controls.tga", "Options", 336, 168 );
		#endif
	}
		
		
	#ifdef IPAD
	HandleButton(&btnLabel1);
	HandleButton(&btnLabel2);
	HandleButton(&btnLabel3);
	HandleButton(&btnLabel4);
	#endif
	
	
	if ( netgame ) {
		// disable buttons if we are already in a netgame
		btnNewGame.buttonFlags = BF_INACTIVE | BF_TRANSPARENT;
		btnMultiplayer.buttonFlags = BF_INACTIVE | BF_TRANSPARENT;
		btnWWW.buttonFlags = BF_INACTIVE | BF_TRANSPARENT;
		#ifndef IPAD
		btnDemo.buttonFlags = BF_INACTIVE | BF_TRANSPARENT;
		#endif
	}

	if ( HandleButton( &btnResumeGame ) ) {
		ResumeGame();
	}

	if ( HandleButton( &btnNewGame ) ) {
		menuState = IPM_MAPS;
	}
		
	if ( HandleButton( &btnControls ) ) {
		menuState = IPM_CONTROLS;
	}
	
	if ( !NetworkAvailable() ) {
		// disable multiplayer if we don't have a good device
		//btnMultiplayer.buttonFlags = BF_INACTIVE | BF_TRANSPARENT;
		btnMultiplayer.buttonFlags = BF_TRANSPARENT;
	} else if ( netgame ) {
		// disable multiplayer if we are already in a netgame
		btnMultiplayer.buttonFlags = BF_INACTIVE | BF_TRANSPARENT;
	} else if ( NetworkServerAvailable() ) {
		// blink the multiplayer button if a local server is available
		btnMultiplayer.buttonFlags = BF_GLOW;
	} else {
		btnMultiplayer.buttonFlags = 0;
	}
	
	if ( HandleButton( &btnMultiplayer )) {
		if (NetworkAvailable()) {
			// get the address for the local service, which may
			// start up a bluetooth personal area network
			boolean serverResolved = ResolveNetworkServer( &netServer.address );
		
			// open our socket now that the network interfaces have been configured
			// Explicitly open on interface 1, which is en0.  If bluetooth ever starts
			// working better, we can handle multiple interfaces.
			if ( gameSocket <= 0 ) {
				gameSocket = UDPSocket( "en0", DOOM_PORT );
			}
		
			// get the address for the local service
			if ( !serverResolved ) {
				// nobody else is acting as a server, so start one here
				RegisterGameService();
				SetupEmptyNetGame();
			}		
			menuState = IPM_MULTIPLAYER;
		} else {
			SysShowAlert("Attention:", "Multiplayer requires a WiFi connection that doesn't block UDP port 14666");
		}
	}
	// draw the available interfaces over the blinking net button
	if ( NetworkServerAvailable() ) {
 		iphoneCenterText( btnMultiplayer.x + btnMultiplayer.drawWidth / 2, 
						 btnMultiplayer.y + btnMultiplayer.drawHeight/2, 0.75,
								 NetworkServerTransport() );
	}
	
	if ( HandleButton( &btnWWW ) ) {
//		menuState = IPM_PACKET_TEST;	// !@# debug
		SysIPhoneOpenURL( "http://doomsday.generalarcade.com/" );
		
	}
	#ifndef IPAD
	if ( HandleButton( &btnDemo ) ) {
		StartDemoGame( btnDemo.twoFingerPress );
	}
	
	if ( btnDemo.twoFingerPress ) {
		strcpy( timeDemoResultString, "TIMEDEMO" );
	}
	// draw the timedemo results on top of the button
	if ( timeDemoResultString[0] ) {
		iphoneCenterText( btnDemo.x + btnDemo.drawWidth / 2, btnDemo.y + btnDemo.drawHeight/2, 0.75,
						 timeDemoResultString );
	}
	#endif
}


/*
 ==================
 iphoneControlMenu
 
 ==================
 */
ibutton_t	btnSchemes[4];	
ibutton_t	btnSettings;
ibutton_t	btnMove;
#ifdef IPAD
static ibutton_t	optionButtons[2][6];
static ibutton_t	defaultsButton;
boolean OptionButton( int col, int row, const char *title ) {
	assert( col >= 0 && col < 2 && row >= 0 && row < 6 );
	return NewTextButton( &optionButtons[col][row], title, 512/2 + 18 + 242 * col, 420 + 66 * row, 225, 48 );
}
#endif


void iphoneControlMenu() {
	int		i;
	
//	iphoneCenterText( 240, 16, 0.75, __DATE__" "__TIME__ );
	
	// 112 units between
	
	if ( BackButton() ) {
		menuState = IPM_MAIN;
	}
	#ifndef IPAD
	if ( NewTextButton( &btnSettings, "Settings",  480-128, 0, 128, 48 ) ) {
		menuState = IPM_OPTIONS;
	}
	#endif
	
	#ifdef IPAD
	if ( NewTextButton( &btnMove, "Move Controls",  514, 354, 225, 48 ) ) {
		menuState = IPM_HUDEDIT;
	}
	#else
	if ( NewTextButton( &btnMove, "Move Controls",  48 + (480-(128+160+48))/2, 0, 160, 48 ) ) {
		menuState = IPM_HUDEDIT;
	}	
	#endif
	
	if ( !btnSchemes[0].texture ) {
		for ( int i = 0 ; i < 3 ; i++ ) {
			#ifdef IPAD
			SetButtonPicsAndSizes( &btnSchemes[i], va("iphone/config_%i.tga",i+1), "", 
								512/2 + 42 + (96+64) * i, 10, 96, 96 );
			#else
			SetButtonPicsAndSizes( &btnSchemes[i], va("iphone/config_%i.tga",i+1), "", 
								32 + (96+64) * i, 48 + ( 112 - 64 )*0.5, 96, 64 );
			
			#endif
		}
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		char	str[128];
		sprintf( str, "iphone/config_%i.tga", i+1 );
		if ( i != controlScheme->value ) {
			btnSchemes[i].buttonFlags = BF_DIMMED;
		} else {
			btnSchemes[i].buttonFlags = 0;
		}
		if ( HandleButton( &btnSchemes[i] ) ) {
			Cvar_SetValue( controlScheme->name, i );
			HudSetForScheme( i );
		}
	}
	
	#ifdef IPAD
	iphoneSlider( 512/2 + 32, 135, 440, 40, "Move stick size", stickMove, 64, 128, 0 );
	iphoneSlider( 512/2 + 32, 135 + 55, 440, 40, "Turn stick size", stickTurn, 64, 128, 0 );
	iphoneSlider( 512/2 + 32, 135 + 55 * 2, 440, 40, "Tilt move speed", tiltMove, 100, 300, 0 );	
	#else
	iphoneSlider( 20, 160, 440, 40, "Move stick size", stickMove, 64, 128, 0 );
	iphoneSlider( 20, 210, 440, 40, "Turn stick size", stickTurn, 64, 128, 0 );
	iphoneSlider( 20, 260, 440, 40, "Tilt move speed", tiltMove, 100, 300, 0 );	
	#endif
	
	if ( tiltMove->value == 100 ) {
		Cvar_SetValue( tiltMove->name, 0 );
	}
	if ( tiltMove->value ) {
		Cvar_SetValue( tiltTurn->name, 0 );
	}
#ifdef IPAD	
	iphoneSlider( 512/2+32, 135 + 55 * 3, 440, 40, "Tilt turn speed", tiltTurn, 1500, 3500, 0 );
	if ( tiltTurn->value == 1500 ) {
		Cvar_SetValue( tiltTurn->name, 0 );
	}
	if ( tiltTurn->value ) {
		Cvar_SetValue( tiltMove->name, 0 );
	}
#endif
	
#ifdef IPAD

		boolean musicState = music->value;
		if ( SysIPhoneOtherAudioIsPlaying() ) {
			// music always off when ipod music is playing
			musicState = false;
		}
		
		if ( NewTextButton( &defaultsButton, "Defaults", 274, 354, 225, 48 ) ) {
			// reset all cvars except the reverse-landscape mode value
			float value = revLand->value;
			Cvar_Reset_f();
			Cvar_SetValue( revLand->name, value );
			HudSetForScheme(0);
			iphoneStartMusic();
		}
		
		if ( OptionButton( 0, 0, autoUse->value ? "Auto use: ON" : "Auto use: OFF" ) ) {
			Cvar_SetValue( autoUse->name, !autoUse->value );
		}
		if ( OptionButton( 0, 1, statusBar->value ? "Status bar: ON" : "Status bar: OFF" ) ) {
			Cvar_SetValue( statusBar->name, !statusBar->value );
		}
		if ( OptionButton( 0, 2, touchClick->value ? "Touch click: ON" : "Touch click: OFF" ) ) {
			Cvar_SetValue( touchClick->name, !touchClick->value );
		}
		if ( OptionButton( 0, 3, messages->value ? "Text messages: ON" : "Text messages: OFF" ) ) {
			Cvar_SetValue( messages->name, !messages->value );
		}
		if ( OptionButton( 1, 0, drawControls->value ? "Draw controls: ON" : "Draw controls: OFF" ) ) {
			Cvar_SetValue( drawControls->name, !drawControls->value );
		}
		if ( OptionButton( 1, 1, musicState ? "Music: ON" : "Music: OFF" ) ) {
			if ( !SysIPhoneOtherAudioIsPlaying() ) {
				Cvar_SetValue( music->name, !music->value );
				if ( music->value ) {
					iphoneStartMusic();
				} else {
					iphoneStopMusic();
				}
			}
		}
		if ( OptionButton( 1, 2, centerSticks->value ? "Center sticks: ON" : "Center sticks: OFF" ) ) {
			Cvar_SetValue( centerSticks->name, !centerSticks->value );
		}
		if ( OptionButton( 1, 3, rampTurn->value ? "Ramp turn: ON" : "Ramp turn: OFF" ) ) {
			Cvar_SetValue( rampTurn->name, !rampTurn->value );
		}	
#endif
}


void SetupEmptyNetGame() {
	Com_Printf( "SetupEmptyNetGame()\n" );
	// no current setup packet, so initialize with this phone's default values
	localGameID = SysIphoneMicroseconds();
	memset( &setupPacket, 0, sizeof( setupPacket ) );
	setupPacket.gameID = localGameID;
	setupPacket.packetType = PACKET_VERSION_SETUP;
	setupPacket.map.dataset = mpDataset->value;
	setupPacket.map.episode = mpEpisode->value;
	setupPacket.map.map = mpMap->value;
	setupPacket.map.skill = mpSkill->value;
	setupPacket.deathmatch = mpDeathmatch->value;
	setupPacket.timelimit = timeLimit->value;
	setupPacket.fraglimit = fragLimit->value;
	setupPacket.playerID[0] = playerID;
}

/*
 ==================
 SendJoinPacket
 
 These will be sent to the server ever frame we are in the multiplayer menu.
 ==================
 */
void SendJoinPacket() {
	packetJoin_t	pj;
	
	pj.packetType = PACKET_VERSION_JOIN;
	pj.gameID = setupPacket.gameID;
	pj.playerID = playerID;
	
	int r = sendto( gameSocket, &pj, sizeof( pj ), 0, 
				   &netServer.address, sizeof( netServer.address ) );
	if ( r == -1 ) {
		struct sockaddr_in *addr = (struct sockaddr_in *)&netServer.address;
		byte *ip = (byte *)&addr->sin_addr;
		Com_Printf( "UDP sendTo %i.%i.%i.%i failed: %s\n", 
				   ip[0], ip[1], ip[2], ip[3], strerror( errno ) );
		close( gameSocket );
		gameSocket = -1;
	}
}

/*
 ==================
 iphoneMultiplayerMenu
 
 ==================
 */
ibutton_t	btnCoop;
ibutton_t	btnDeathmatch;
ibutton_t	btnMap;
ibutton_t	btnNetSettings;
typedef enum {
	NM_MAIN,
	NM_MAP_SELECT,
	NM_OPTIONS
} netMenu_t;
netMenu_t	netMenu;

void iphoneMultiplayerMenu() {
	if ( gameSocket <= 0 ) {
		// no socket, so no multiplayer
		TerminateGameService();		// don't advertise for any more new players
		setupPacket.gameID = 0;		// stop sending packets
		menuState = IPM_MAIN;
		return;
	}

	boolean	server = ( setupPacket.gameID == localGameID );
	
	// different screen when selecting a map to play
	if ( netMenu == NM_MAP_SELECT ) {
		mapStart_t	map;
		if ( !iphoneMapSelectMenu( &map ) ) {
			// haven't selected anything yet
			return;
		}
		netMenu = NM_MAIN;
		if ( map.map != -1 ) {
			// selected something new, didn't hit the back arrow
			setupPacket.map = map;
		}
	}
	
	#ifndef IPAD
	else if ( netMenu == NM_OPTIONS ) {
		
		Cvar_SetValue( fragLimit->name, setupPacket.fraglimit );
		if ( iphoneSlider( 104, 64, 272, 40, "frag limit", fragLimit, 0, 20, SF_INTEGER ) ) {
			if ( server ) {
				setupPacket.fraglimit = fragLimit->value;
			}
		}
		
		Cvar_SetValue( timeLimit->name, setupPacket.timelimit );
		if ( iphoneSlider( 104, 64+56, 272, 40, "time limit", timeLimit, 0, 20, SF_INTEGER ) ) {
			if ( server ) {
				setupPacket.timelimit = timeLimit->value;
			}
		}
		if ( BackButton() ) {
			netMenu = NM_MAIN;
		}
		return;
	}
	#else
	Cvar_SetValue( fragLimit->name, setupPacket.fraglimit );
	if ( iphoneSlider( 314, 410, 400, 40, "frag limit", fragLimit, 0, 20, SF_INTEGER ) ) {
		if ( server ) {
			setupPacket.fraglimit = fragLimit->value;
		}
	}
	
	Cvar_SetValue( timeLimit->name, setupPacket.timelimit );
	if ( iphoneSlider( 314, 410+58, 400, 40, "time limit", timeLimit, 0, 20, SF_INTEGER ) ) {
		if ( server ) {
			setupPacket.timelimit = timeLimit->value;
		}
	}
	
	#endif
	
	if ( !btnDeathmatch.texture ) {
		// initial setup
		#ifdef IPAD
		SetButtonPicsAndSizes( &btnDeathmatch, "iphone/deathmatch.tga", "Deathmatch", 512-128-50, 150, 128, 128 );
		SetButtonPicsAndSizes( &btnCoop, "iphone/co-op.tga", "Cooperative", 512+50, 150, 128, 128 );
		#else
		SetButtonPicsAndSizes( &btnDeathmatch, "iphone/deathmatch.tga", "Deathmatch", 4+48, 64, 96, 96 );
		SetButtonPicsAndSizes( &btnCoop, "iphone/co-op.tga", "Cooperative", 480-148, 64, 96, 96 );		
		#endif
	}

	if ( BackButton() ) {
		if ( server ) {
			TerminateGameService();		// don't advertise for any more new players
			setupPacket.gameID = 0;		// stop sending packets
		}
		menuState = IPM_MAIN;
	}
	
	if ( !server ) {
		// we aren't the server
		// send our join packet every frame
		SendJoinPacket();
		
		if ( setupPacketFrameNum < iphoneFrameNum - 30 ) {
			// haven't received a current server packet
			char	str[1024];
			struct sockaddr_in *sin = (struct sockaddr_in *)&netServer.address;
			byte *ip = (byte *)&sin->sin_addr;
			sprintf( str, "Joining server at %i.%i.%i.%i:%i\n", ip[0], ip[1], ip[2], ip[3], 
					ntohs( sin->sin_port ) );
			#ifdef IPAD
			iphoneCenterText( 512, 384, 1, str );			
			#else	
			iphoneCenterText( 240, 160, 0.75, str );
			#endif
			return;
		}
	} else {
		// cull out any players that haven't given us a packet in a couple seconds
		int	now = SysIphoneMilliseconds();
		for ( int i = 1 ; i < MAXPLAYERS ; i++ ) {
			if ( setupPacket.playerID[i] && now - netPlayers[i].peer.lastPacketTime > 1000 ) {
				printf( "Dropping player %i: last:%i now:%i\n", i, netPlayers[i].peer.lastPacketTime, now );
				setupPacket.playerID[i] = 0;
			}
		}
	}
	
	// draw the level and allow clicking to change
	Cvar_SetValue( mpDataset->name, setupPacket.map.dataset );
	Cvar_SetValue( mpEpisode->name, setupPacket.map.episode );
	Cvar_SetValue( mpMap->name, setupPacket.map.map );
	Cvar_SetValue( mpSkill->name, setupPacket.map.skill );
	
	// map select button / display
	#ifdef IPAD
	if ( NewTextButton( &btnMap,  FindMapName( mpDataset->value, mpEpisode->value, mpMap->value ), 512 - 200, 80, 400, 48 ) ) {
	#else
	if ( NewTextButton( &btnMap,  FindMapName( mpDataset->value, mpEpisode->value, mpMap->value ), 64, 0, 480-128, 48 ) ) {	
	#endif
		
		if ( server ) {
			// clients can't go into this menu
			netMenu = NM_MAP_SELECT;
		}
	}

	
	if ( setupPacket.deathmatch ) {
		btnDeathmatch.buttonFlags = 0;
		btnCoop.buttonFlags = BF_DIMMED;
	} else {
		btnDeathmatch.buttonFlags = BF_DIMMED;
		btnCoop.buttonFlags = 0;
	}
	
	if ( HandleButton( &btnDeathmatch ) ) {
		if ( server ) {
			Cvar_SetValue( mpDeathmatch->name, 3 );	// weapons stay, items respawn rules
			setupPacket.deathmatch = mpDeathmatch->value;
		}
	}
	if ( HandleButton( &btnCoop ) ) {
		if ( server ) {
			Cvar_SetValue( mpDeathmatch->name, 0 );		
			setupPacket.deathmatch = mpDeathmatch->value;
		}
	}	
#ifndef IPAD
	if ( NewTextButton( &btnNetSettings, "Settings",  240-64, 64+24, 128, 48 ) ) {
		netMenu = NM_OPTIONS;
	}
#endif
			
	for ( int i = 0 ; i < 4 ; i ++ ) {
		#ifdef IPAD
		int x = 320 + ( 64+45) * i;
		int y = 64+260;
		#else
		int x = 45 + ( 64+45) * i;
		int y = 64+128;
		#endif
		// FIXME: show proper player colors
		byte	color[4][4] = { { 0, 255, 0, 255 }, { 128, 128, 128, 255 }, { 128,64,0, 255 }, {255,0,0, 255 } };
		glColor4ubv( color[i] );
		PK_DrawTexture( PK_FindTexture( "iphone/multi_backdrop.tga" ), x, y );
		glColor4f( 1, 1, 1, 1 );
		if ( setupPacket.playerID[i] == playerID ) {
			// bigger outline for your player slot
			PK_StretchTexture( PK_FindTexture( "iphone/multi_frame.tga" ), x, y, 64, 64 );
		}
		
		
		// draw doom guy face
		if ( setupPacket.playerID[i] != 0 ) {
			PK_DrawTexture( PK_FindTexture( "iphone/multi_face.tga" ), x, y );
#if 0			
			// temp display IP address
			byte *ip = (byte *)&setupPacket.address[i].sin_addr;
			iphoneDrawText( x-16, (i&1) ? y+16 : y+48, 0.75, va("%i.%i.%i.%i", ip[0], ip[1], ip[2], ip[3] ) );
#endif
		}
		
	}
	
	if ( server ) {
		// flash a tiny pic when transmitting
		if ( iphoneFrameNum & 1 ) {
			glColor4f( 1,1,1,1 );
		} else {
			glColor4f( 0.5,0.5,0.5,1 );
		}
		#ifdef IPAD
		iphoneCenterText( 1024 - 20, 768 - 20, 1, "*" );
		#else
		iphoneCenterText( 470, 310, 0.75, "*" );
		#endif
		glColor4f( 1,1,1,1 );
	}
	if ( setupPacketFrameNum == iphoneFrameNum ) {
	#ifdef IPAD
		iphoneCenterText( 1024 - 40, 768 - 20, 1, "*" );
	#else
		iphoneCenterText( 450, 310, 0.75, "*" );
	#endif
	}
//	iphoneDrawText( 0, 310, 0.75, va("%i:%i", localGameID, setupPacket.gameID ) );

	// only draw the start button if we have at least two players in game
	int	numPlayers = 0;
	for ( int i = 0 ; i < MAXPLAYERS ; i++ ) {
		if ( setupPacket.playerID[i] != 0 ) {
			numPlayers++;
		}
	}
	
	if ( numPlayers > 1 ) {
		if ( server ) {
			static ibutton_t btnStart;
			#ifdef IPAD
			if ( NewTextButton( &btnStart, "Start game", 512-80, 768-100, 160, 48) ) {
			#else
			if ( NewTextButton( &btnStart, "Start game", 240-80, 320-48, 160, 48 ) ) {
			#endif
				setupPacket.startGame = 1;
				StartNetGame();
				TerminateGameService();		// don't advertise for any more new players
				return;
			}
		} else {
		#ifdef IPAD	
			iphoneCenterText( 512, 68-25, 1, "Waiting for server to start the game" );
		#else
			iphoneCenterText( 240, 320-10, 0.60, "Waiting for server to start the game" );
		#endif
		}
	} else {
		byte *ip = (byte *)&gameSocketAddress.sin_addr;
		#ifdef IPAD
		iphoneCenterText( 512, 768-25, 1, va("Waiting for players on %i.%i.%i.%i", 
													ip[0], ip[1], ip[2], ip[3] ) );
		iphoneCenterText( 512, 27, 0.75, "Attention: Multiplayer requires a WiFi connection");
		iphoneCenterText( 512, 50, 0.75, "that doesn't block UDP port 14666");
		#else
		iphoneCenterText( 240, 320-8, 0.60, va("Waiting for players on %i.%i.%i.%i", 
											   ip[0], ip[1], ip[2], ip[3] ) );
		iphoneCenterText( 240, 320-50, 0.60, "Attention: Multiplayer requires a WiFi connection");
		iphoneCenterText( 240, 320-30, 0.60, "that doesn't block UDP port 14666");		
		#endif
	}
	//	static ibutton_t btnStart;
	//	NewTextButton( &btnStart, "Start game", 512-80, 768-100, 160, 48);
}

#ifndef IPAD
static ibutton_t	optionButtons[2][6];
static ibutton_t	defaultsButton;
boolean OptionButton( int col, int row, const char *title ) {
	assert( col >= 0 && col < 2 && row >= 0 && row < 6 );
	return NewTextButton( &optionButtons[col][row], title, 10 + 235 * col, 64 + 50 * row, 225, 48 );
}
#endif

/*
 ==================
 iphoneOptionsMenu
 
 ==================
 */
void iphoneOptionsMenu() {	
	if ( BackButton() ) {
		menuState = IPM_CONTROLS;
	}

	boolean musicState = music->value;
	if ( SysIPhoneOtherAudioIsPlaying() ) {
		// music always off when ipod music is playing
		musicState = false;
	}

	if ( NewTextButton( &defaultsButton, "Defaults", 240-225/2, 2, 225, 48 ) ) {
		// reset all cvars except the reverse-landscape mode value
		float value = revLand->value;
		Cvar_Reset_f();
		Cvar_SetValue( revLand->name, value );
		HudSetForScheme(0);
		iphoneStartMusic();
	}
	
	if ( OptionButton( 0, 0, autoUse->value ? "Auto use: ON" : "Auto use: OFF" ) ) {
		Cvar_SetValue( autoUse->name, !autoUse->value );
	}
	if ( OptionButton( 0, 1, statusBar->value ? "Status bar: ON" : "Status bar: OFF" ) ) {
		Cvar_SetValue( statusBar->name, !statusBar->value );
	}
	if ( OptionButton( 0, 2, touchClick->value ? "Touch click: ON" : "Touch click: OFF" ) ) {
		Cvar_SetValue( touchClick->name, !touchClick->value );
	}
	if ( OptionButton( 0, 3, messages->value ? "Text messages: ON" : "Text messages: OFF" ) ) {
		Cvar_SetValue( messages->name, !messages->value );
	}
	if ( OptionButton( 1, 0, drawControls->value ? "Draw controls: ON" : "Draw controls: OFF" ) ) {
		Cvar_SetValue( drawControls->name, !drawControls->value );
	}
	if ( OptionButton( 1, 1, musicState ? "Music: ON" : "Music: OFF" ) ) {
		if ( !SysIPhoneOtherAudioIsPlaying() ) {
			Cvar_SetValue( music->name, !music->value );
			if ( music->value ) {
				iphoneStartMusic();
			} else {
				iphoneStopMusic();
			}
		}
	}
	if ( OptionButton( 1, 2, centerSticks->value ? "Center sticks: ON" : "Center sticks: OFF" ) ) {
		Cvar_SetValue( centerSticks->name, !centerSticks->value );
	}
	if ( OptionButton( 1, 3, rampTurn->value ? "Ramp turn: ON" : "Ramp turn: OFF" ) ) {
		Cvar_SetValue( rampTurn->name, !rampTurn->value );
	}
}	

/*
 ===================
 iphoneIntermission
 
 The end-of-level switch was just hit, note the state and awards
 for the map select menu
 ===================
 */
void iphoneIntermission( wbstartstruct_t* wb ) {
	if ( deathmatch || netgame ) {
		// no achievements in deathmatch mode
		return;
	}
	
	// find the current episode / map combination 
	
	// if a mapStat_t doesn't exist for this yet, create one
	
	// mark this level / skill combination as tried
	mapStats_t *cms = FindMapStats( playState.map.dataset, playState.map.episode, playState.map.map, true );
	if ( !cms ) {
		return;
	}
	
	int skill = playState.map.skill;
	cms->completionFlags[skill] |= MF_COMPLETED;
	
	// add the awards
	if ( wb->plyr[0].stime < wb->partime ) {
		cms->completionFlags[skill] |= MF_TIME;
	}
	
	int numkills = 0;
	int numsecrets = 0;
	int numitems = 0;
	for ( int i = 0 ; i < MAXPLAYERS ; i++ ) {
		if ( wb->plyr[i].in ) {
			numkills += wb->plyr[i].skills;
			numitems += wb->plyr[i].sitems;
			numsecrets += wb->plyr[i].ssecret;
		}
	}
	if ( numkills >= wb->maxkills ) {
		cms->completionFlags[skill] |= MF_KILLS;
	}
	if ( numitems >= wb->maxitems ) {
		cms->completionFlags[skill] |= MF_TREASURE;
	}
	if ( numsecrets >= wb->maxsecret ) {
		cms->completionFlags[skill] |= MF_SECRETS;
	}	
}

/*
 ===================
 iphoneStartLevel
 
 Do a savegame with the current state
 ===================
 */
void iphoneStartLevel() {
	if ( deathmatch || netgame ) {
		// no achievements in deathmatch mode
		
		// reset the levelTimer
		if ( levelTimer && setupPacket.timelimit > 0 ) {
			// 30 hz, minutes
			levelTimeCount = setupPacket.timelimit * 30 * 60;
		}
		
		return;
	}
	playState.map.map = gamemap;

	// automatic save game
	G_SaveGame( 0, "entersave" );
	G_DoSaveGame(true);

	// mark this level as tried
	mapStats_t *cms = FindMapStats( playState.map.dataset, playState.map.episode, playState.map.map, true );
	if ( cms ) {
		cms->completionFlags[playState.map.skill] |= MF_TRIED;
	}
}


/*
 ===================
 DrawLiveBackground
 
 Draw a randomish moving cloudy background
 ===================
 */
void DrawLiveBackground() {
	static float	bgVectors[2][2] = { { 0.01, 0.015 }, { -0.01, -0.02 } };
	float	fade[2];
	
	// slide and fade a couple textures around
	static float	tc[2][4][2];
	for ( int i = 0 ; i < 2 ; i++ ) {		
		int		ofs = iphoneFrameNum + i * 32;
		float	dist = ( ofs & 63 );
		for ( int j = 0 ; j < 2 ; j ++ ) {
			for ( int k = 0 ; k < 2 ; k++ ) {
				if ( rand()&1 ) {
					if ( bgVectors[j][k] < 0.03 ) {
						bgVectors[j][k] += 0.0001;
					}
				} else {
					if ( bgVectors[j][k] > -0.03 ) {
						bgVectors[j][k] -= 0.0001;
					}
				}
			}
		}
		fade[i] = sin( ( dist - 16 ) / 32.0 * M_PI ) * 0.5 + 0.5;
		fade[i] *= 0.7;
		
		for ( int j = 0 ; j < 2 ; j++ ) {
			tc[i][0][j] += bgVectors[i][j];
			tc[i][0][j] -= floor( tc[i][0][j] );
		}
		tc[i][1][0] = tc[i][0][0]+1;
		tc[i][1][1] = tc[i][0][1]+0;
		
		tc[i][2][0] = tc[i][0][0]+0;
		tc[i][2][1] = tc[i][0][1]+1;
		
		tc[i][3][0] = tc[i][0][0]+1;
		tc[i][3][1] = tc[i][0][1]+1;
	}
	
	
	
	// Fill rate performance is an issue just for two scrolling layers under
	// modest GUI objects.  Using a PVR2 texture and a single multitexture
	// pass helps.  If all the GUI objects were drawn with depth buffering,
	// the surface rejection would help out, but bumping depth after every
	// draw would be a bit of a chore.
	
#if 0	
	glClear( GL_DEPTH_BUFFER_BIT );
	glDepthMask( 1 );	// write the depth buffer
	glEnable( GL_DEPTH_TEST );	// depth test this background
#endif
	
	PK_BindTexture( PK_FindTexture( "iphone/livetile_1.tga" ) );
	
	glDisable( GL_BLEND );
	glDisable( GL_DEPTH_TEST );

	// multitexture setup
	glActiveTexture( GL_TEXTURE1 );
	glClientActiveTexture( GL_TEXTURE1 );
	glEnable( GL_TEXTURE_2D );
	PK_BindTexture( PK_FindTexture( "iphone/livetile_1.tga" ) );	
	glTexCoordPointer( 2, GL_FLOAT, 8, tc[1][0] );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	
	glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );
//	glColor4f( fade[0], fade[0], fade[0], fade[1] );
	glBegin( GL_TRIANGLE_STRIP );
	
#ifdef IPAD
	glTexCoord2f( tc[0][0][0], tc[0][0][1] );	glVertex3f( 0, 0, 0.5 );
	glTexCoord2f( tc[0][1][0], tc[0][1][1] );	glVertex3f( 1024, 0, 0.5 );
	glTexCoord2f( tc[0][2][0], tc[0][2][1]+1 );	glVertex3f( 0, 768, 0.5 );
	glTexCoord2f( tc[0][3][0], tc[0][3][1]+1 );	glVertex3f( 1024, 768, 0.5 );
#else
	glTexCoord2f( tc[0][0][0], tc[0][0][1] );	glVertex3f( 0, 0, 0.5 );
	glTexCoord2f( tc[0][1][0], tc[0][1][1] );	glVertex3f( 480, 0, 0.5 );
	glTexCoord2f( tc[0][2][0], tc[0][2][1]+1 );	glVertex3f( 0, 320, 0.5 );
	glTexCoord2f( tc[0][3][0], tc[0][3][1]+1 );	glVertex3f( 480, 320, 0.5 );
#endif
	glEnd();
	
	// unbind the second texture
	glBindTexture( GL_TEXTURE_2D, 0 );
	glDisable( GL_TEXTURE_2D );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	glActiveTexture( GL_TEXTURE0 );
	glClientActiveTexture( GL_TEXTURE0 );

	glColor4f( 1, 1, 1, 1 );
	glEnable( GL_BLEND );	
#if 0	
	// Enable depth test, but not depth writes, so the tile dorting
	// minimizes the amount of time drawing the background when it
	// is mostly covered.
	glEnable( GL_DEPTH_TEST );
	glDepthMask( 0 );
#endif	
}

#define MAX_PACKET_LOG	64
int	currentPacketLog;
int	packetLogMsec[MAX_PACKET_LOG];

void iphonePacketTester() {
	glClear( GL_COLOR_BUFFER_BIT );
	
	if ( BackButton() ) {
		menuState = IPM_MAIN;
		return;
	}
	
	struct sockaddr_in sender;
	unsigned senderLen = sizeof( sender );
	byte	buffer[1024];
	while( 1 ) {
		int r = recvfrom( gameSocket, buffer, sizeof( buffer ), 0, (struct sockaddr *)&sender, &senderLen );
		if ( r == -1 ) {
			break;
		}
		packetSetup_t *sp = (packetSetup_t *)buffer;
		if ( sp->sendCount == setupPacket.sendCount ) {
			Com_Printf( "Duplicated receive: %i\n", sp->sendCount );
		} else if ( sp->sendCount < setupPacket.sendCount ) {
			Com_Printf( "Out of order receive: %i < %i\n", sp->sendCount, setupPacket.sendCount );
		} else if ( sp->sendCount > setupPacket.sendCount + 1 ) {
			Com_Printf( "Dropped %i packets before %i\n", sp->sendCount - 1 - setupPacket.sendCount, sp->sendCount );
		}
		setupPacket = *sp;
		packetLogMsec[currentPacketLog&(MAX_PACKET_LOG-1)] = SysIphoneMilliseconds();
		currentPacketLog++;
	}
	
	color4_t activeColor = { 0, 255, 0, 255 };
	for ( int i = 1 ; i < MAX_PACKET_LOG ; i++ ) {
		int	t1 = packetLogMsec[(currentPacketLog - i)&(MAX_PACKET_LOG-1)];
		int	t2 = packetLogMsec[(currentPacketLog - i - 1)&(MAX_PACKET_LOG-1)];
		int	msec = t1 - t2;
		R_Draw_Fill( 0, i * 4, msec, 2, activeColor );
	}
}


/*
 ===================
 iphoneStartMenu
 
 ===================
 */
void iphoneStartMenu() {
	mapStart_t	map;
	if ( !iphoneMapSelectMenu( &map ) ) {
		return;
	}
	if ( map.map == -1 ) {
		// hit the back button
		menuState = IPM_MAIN;
		return;
	}
	
	StartSinglePlayerGame( map );
}

/*
 ===================
 iphoneDrawMenus
 
 ===================
 */
void iphoneDrawMenus() {
	if ( menuState == IPM_PACKET_TEST ) {
		// do this before the slow drawing background to get 60hz update rate
		iphonePacketTester();
		return;
	}
	
	// draw the slow double-cloud layer
	DrawLiveBackground();
	
	// check for game start in a received setup packet
	if ( !netgame && setupPacket.startGame ) {
		if ( StartNetGame() ) {
			setupPacket.startGame = false;
			// we aren't in this game
			return;
		}
	}
	
	// interactive menus
	switch ( menuState ) {
		case IPM_MAIN: iphoneMainMenu(); break;
		case IPM_MULTIPLAYER: iphoneMultiplayerMenu(); break;
		case IPM_MAPS: iphoneStartMenu(); break;
		case IPM_CONTROLS: iphoneControlMenu(); break;
		case IPM_OPTIONS: iphoneOptionsMenu(); break;
		case IPM_HUDEDIT: HudEditFrame(); break;
	}
}


