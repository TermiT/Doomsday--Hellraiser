/*
 *  iphone_sound.c
 *  doom
 *
 *  Created by John Carmack on 4/16/09.
 *  Copyright 2009 Id Software. All rights reserved.
 *
 */
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
#import <AudioToolbox/AudioServices.h>


typedef struct  {
	unsigned			sourceName;		// OpenAL sourceName
	pkWav_t				*sfx;			// NULL if unused
	float				volume;			// stored for showSound display
} channel_t;

static ALCcontext *Context;
static ALCdevice *Device;

#define MAX_CHANNELS		16
static channel_t	s_channels[ MAX_CHANNELS ];

cvar_t	*s_sfxVolume;

void Sound_StartLocalSound( const char *filename ) {
	Sound_StartLocalSoundAtVolume( filename, 1.0f );
}

void Sound_StartLocalSoundAtVolume( const char *filename, float volume ) {
	pkWav_t	*sfx;
	
	sfx = PK_FindWav( filename );
	if( ! sfx ) {
		Com_Printf( "Sound_StartLocalSound: could not cache (%s)\n", filename );
		return;
	}
//	printf( "sound:%s\n", filename );	
	// channel 0 is reserved for UI sounds, the other channels
	// are for DOOM sounds
	channel_t *ch = &s_channels[ 0 ];
	
	ch->sfx = sfx;
	ch->volume = s_sfxVolume->value * volume;
	
	alSourceStop( ch->sourceName );
	alSourcef( ch->sourceName, AL_GAIN, ch->volume );
	alSourcei( ch->sourceName, AL_BUFFER, sfx->alBufferNum );
	alSourcef( ch->sourceName, AL_PITCH, 1.0f );
	alSourcePlay( ch->sourceName );
}


static void Sound_Play_f( void ) {
	if( Cmd_Argc() == 1 ) {
		Com_Printf( "Usage: play <soundfile>\n" );
		return;
	}
	Sound_StartLocalSound( Cmd_Argv( 1 ) );
}

// we won't allow music to be toggled on or off in the menu when this is true
int otherAudioIsPlaying;

int SysIPhoneOtherAudioIsPlaying() {
	return otherAudioIsPlaying;
}

void interruptionListener( void *inUserData, UInt32 inInterruption)
{
	printf("Session interrupted! --- %s ---\n", inInterruption == kAudioSessionBeginInterruption ? "Begin Interruption" : "End Interruption");
	
	if ( inInterruption == kAudioSessionBeginInterruption ) {
		printf("Audio interrupted.\n" );
		iphonePauseMusic();			
		alcMakeContextCurrent( NULL );
		AudioSessionSetActive( false );
	} else if ( inInterruption == kAudioSessionEndInterruption ) {
		printf("Audio restored.\n" );
		
		OSStatus r = AudioSessionSetActive( true );
		if ( r != kAudioSessionNoError ) {
			printf( "AudioSessionSetActive( true ) failed: 0x%x\n", r );
		} else {
			printf( "AudioSessionSetActive( true ) succeeded.\n" );
		}
		alcMakeContextCurrent( Context );
		if( alcGetError( Device ) != ALC_NO_ERROR ) {
			Com_Error( "Failed to alcMakeContextCurrent\n" );
		}
		iphoneResumeMusic();
	}
}

#ifndef USE_STATIC_NONPUBLIC_API
typedef ALvoid	AL_APIENTRY	(*alcMacOSXMixerOutputRateProcPtr) (const ALdouble value);
static ALvoid  alcMacOSXMixerOutputRateProc(const ALdouble value)
{
	static	alcMacOSXMixerOutputRateProcPtr	proc = NULL;
    
    if (proc == NULL) {
        proc = (alcMacOSXMixerOutputRateProcPtr) alcGetProcAddress(NULL, (const ALCchar*) "alcMacOSXMixerOutputRate");
    }
    
    if (proc)
        proc(value);

    return;
}
#endif

void Sound_Init( void ) {

	Com_Printf( "\n------- Sound Initialization -------\n" );
	
	s_sfxVolume		= Cvar_Get( "s_sfxVolume", "1.0", 0 );
	
	Cmd_AddCommand( "play", Sound_Play_f );
	
	// make sure background ipod music mixes with our sound effects
	Com_Printf( "...Initializing AudioSession\n" );
	OSStatus status = 0;
	status = AudioSessionInitialize(NULL, NULL, interruptionListener, NULL);	// else "couldn't initialize audio session"
	
	// if there is iPod music playing in the background, we want to use
	// the AmbientSound catagory, otherwise we will leave it at the default.
	// If we always set it to AmbientSound, then the mp3 background music
	// playback goes to software on 3.0 for a huge slowdown.
	UInt32  propOtherAudioIsPlaying = 'othr'; // kAudioSessionProperty_OtherAudioIsPlaying
	UInt32  size = sizeof( otherAudioIsPlaying );
	AudioSessionGetProperty( propOtherAudioIsPlaying, &size, &otherAudioIsPlaying );
	Com_Printf("OtherAudioIsPlaying = %d\n", otherAudioIsPlaying );
	
	if ( otherAudioIsPlaying ) {
		UInt32 audioCategory = kAudioSessionCategory_AmbientSound;
		status = AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(audioCategory), &audioCategory);
	}
	
	status = AudioSessionSetActive(true);                                       // else "couldn't set audio session active\n"	
	
	Com_Printf( "...Initializing OpenAL subsystem\n" );
	
	// get the OpenAL device
	Device = alcOpenDevice( NULL );
	if( Device == NULL ) {
		Com_Printf( "Failed to alcOpenDevice\n" );
	}
	
	// set the mixer output rate lower, so we don't waste time doing 44khz
	// must be done before the context is created!
#ifdef USE_STATIC_NONPUBLIC_API
	#error Apple private API
	extern ALvoid alcMacOSXMixerOutputRate(const ALdouble value);
	alcMacOSXMixerOutputRate( 22050 );
#else
	alcMacOSXMixerOutputRateProc( 22050 );
#endif
	
	// Create context(s)
	Context = alcCreateContext( Device, NULL );
	if( Context == NULL ) {
		Com_Error( "Failed to alcCreateContext\n" );
	}
	
	// Set active context
	alcGetError( Device );
	alcMakeContextCurrent( Context );
	if( alcGetError( Device ) != ALC_NO_ERROR ) {
		Com_Error( "Failed to alcMakeContextCurrent\n" );
	}
	
	// allocate all the channels we are going to use
	channel_t	*ch;
	int			i;
	for( i = 0, ch = s_channels ; i < MAX_CHANNELS ; ++i, ++ch ) {
		alGenSources( 1, &ch->sourceName );
		
		if( alGetError() != AL_NO_ERROR ) {
			Com_Error( "Allocating AL sound sources" );
		}
		alSourcei( ch->sourceName, AL_SOURCE_RELATIVE, AL_FALSE );
	}
	
	Com_Printf( "------------------------------------\n" );
}

/*
 ==================
 ShowSound
 
 Display active sound channels
 ==================
 */
void ShowSound() {

	if ( !showSound->value ) {
		return;
	}
	channel_t	*ch;
	int			i;
	for( i = 0, ch = s_channels ; i < MAX_CHANNELS ; ++i, ++ch ) {
		int state;
		alGetSourcei( ch->sourceName, AL_SOURCE_STATE, &state );
		if ( state != AL_PLAYING ) {
			continue;
		}
		
		int v = ch->volume * 255;
		if ( v > 255 ) {
			v = 255;
		}
		color4_t color = { v, v, v, 255 };
		R_Draw_Fill( i*16, 0, 12, 12, color );		
	}
}


/*
 ==================================================================

 PrBoom interface
 
 ==================================================================
*/

// Init at program start...
void I_InitSound(void) {}

// ... shut down and relase at program termination.
void I_ShutdownSound(void) {}

// Initialize channels?
void I_SetChannels(void) {}

// Get raw data lump index for sound descriptor.
int I_GetSfxLumpNum (sfxinfo_t *sfx) {
	// find the pkWav_t for this sfxinfo
	char	upper[16], *d = upper;
	for ( const char *c = sfx->name ; *c ; c++ ) {
		*d++ = toupper( *c );
	}
	*d = 0;
	pkWav_t *pkwav = PK_FindWav( va( "newsfx/DS%s.wav", upper ) );	
	
	return pkwav - pkWavs;
}

// Starts a sound in a particular sound channel.
// volume ranges 0 - 64
// seperation tanges is 128 straight ahead, 0 = all left ear, 255 = all right ear
// pitch centers around 128
int I_StartSound(int sfx_id, int channel, int vol, int sep, int pitch, int priority) {

	sfxinfo_t *dsfx = &S_sfx[sfx_id];
	
	assert( dsfx->lumpnum >= 0 && dsfx->lumpnum < pkHeader->wavs.count );
	
	pkWav_t *sfx = &pkWavs[dsfx->lumpnum];
//	printf( "sound: %s chan:%i vol:%i sep:%i pitch:%i priority:%i\n", sfx->wavData->name.name, channel, vol, sep, pitch, priority );	

	assert( channel >= 0 && channel < MAX_CHANNELS - 1 );
	channel_t *ch = &s_channels[ 1+channel ];
	
	alSourceStop( ch->sourceName );
	if ( ch->sfx == sfx ) {
		// restarting the same sound
		alSourceRewind( ch->sourceName );
	} else {
		alSourcei( ch->sourceName, AL_BUFFER, sfx->alBufferNum );
	}
	
	ch->sfx = sfx;
	ch->volume = s_sfxVolume->value * vol / 64.0;
	alSourcef( ch->sourceName, AL_GAIN, ch->volume );
	alSourcef( ch->sourceName, AL_PITCH, pitch / 128.0f );
	alSourcePlay( ch->sourceName );
	
	return (int)ch;
}

// Stops a sound channel.
void I_StopSound(int handle) {}

// Called by S_*() functions
//  to see if a channel is still playing.
// Returns 0 if no longer playing, 1 if playing.
boolean I_SoundIsPlaying(int handle) { 

	channel_t *ch = (channel_t *)handle;
	if ( !ch ) {
		return false;
	}
	int state;
	alGetSourcei( ch->sourceName, AL_SOURCE_STATE, &state );
	
	return state == AL_PLAYING;
}

// Called by m_menu.c to let the quit sound play and quit right after it stops
boolean I_AnySoundStillPlaying(void) { return false; }

// Updates the volume, separation,
//  and pitch of a sound channel.
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch) {}
