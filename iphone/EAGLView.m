//
//  EAGLView.m
//  Doom
//
//  Created by Cass Everitt on 2/20/09.
//  Copyright Id Software 2009. All rights reserved.
//
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




#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGLDrawable.h>

#import "EAGLView.h"
#import "doomAppDelegate.h"

#include "doomiphone.h"

#include <pthread.h>

EAGLView *eaglview;
EAGLContext *context;

@implementation EAGLView

// You must implement this method
+ (Class)layerClass {
    return [CAEAGLLayer class];
}

CAEAGLLayer *eaglLayer;

//The GL view is stored in the nib file. When it's unarchived it's sent -initWithCoder:
- (id)initWithCoder:(NSCoder*)coder {
    self = [super initWithCoder:coder];
	
	eaglview = self;
	
	// allow multiple touch events
	self.multipleTouchEnabled = true;
	
	// Get the layer
	eaglLayer = (CAEAGLLayer *)self.layer;
	
	// set opaque so UIKit doesn't try to blend it over other layers
	eaglLayer.opaque = YES;
	
	// set it to no-backing-retained so it can do ast pageflips instead
	// of update copies, and go to 565 bit depth for higher performance.
	eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
									
									[NSNumber numberWithBool:NO], 
									kEAGLDrawablePropertyRetainedBacking, 
									
									kEAGLColorFormatRGB565,
									/* kEAGLColorFormatRGBA8, */
									kEAGLDrawablePropertyColorFormat, 
									
									nil];
	
	context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
	assert( context );
	
	if ( ![EAGLContext setCurrentContext:context]) {
		[self release];
		return nil;
	}        
	
    glGenFramebuffersOES(1, &viewFramebuffer);
    glGenRenderbuffersOES(1, &viewRenderbuffer);
    
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, viewRenderbuffer);
    
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);
    
	glGenRenderbuffersOES(1, &depthRenderbuffer);
	glBindRenderbufferOES(GL_RENDERBUFFER_OES, depthRenderbuffer);
	glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_DEPTH_COMPONENT16_OES, backingWidth, backingHeight);
	glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES, GL_RENDERBUFFER_OES, depthRenderbuffer);
    
    if(glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) != GL_FRAMEBUFFER_COMPLETE_OES) {
        NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES));
    }
	
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
	
    return self;
}

- (void) handleTouches:(UIEvent*)event {
	int touchCount = 0;
	static int previousTouchCount;
	static int touchRover;
	int	touchThisSequence[MAX_TOUCHES];
	
	memset( touchThisSequence, 0, sizeof( touchThisSequence ) );
	
	NSSet *touches = [event allTouches];
//	printf( "count: %i\n", [touches count] );

	// lock the game out temporarily
	pthread_mutex_lock( &eventMutex );
	
    for (UITouch *myTouch in touches)
    {
        CGPoint touchLocation = [myTouch locationInView:nil];

		// handle landscape mode and flipping
		int		x, y;
		if ( revLand->value ) {
			x = touchLocation.y;
#ifdef IPAD
			y = 767 - touchLocation.x;
#else
			y = 319 - touchLocation.x;
#endif
		} else {
#ifdef IPAD
			x = 1023 - touchLocation.y;
#else
			x = 479 - touchLocation.y;
#endif
			y = touchLocation.x;
		}
		//printf( "Touch: %i, %i\n", x, y );		
		touchCount++;
		
		touch_t	*t2;

		// find which one it is closest to
		int		minDist = 64 * 64;	// allow up to 64 unit moves to be drags
		int		minIndex = -1;
		for ( int i = 0 ; i < MAX_TOUCHES ; i++ ) {
			t2 = &sysTouches[i];
			if ( !t2->down ) {
				continue;
			}
			int dist = ( t2->x - x ) * ( t2->x - x ) + ( t2->y - y ) * ( t2->y - y );
			if ( dist < minDist ) {
				minDist = dist;
				minIndex = i;
			}
		}
		if ( minIndex != -1 ) {
			// reuse a touch
			sysTouches[minIndex].x = x;
			sysTouches[minIndex].y = y;
			if (myTouch.phase == UITouchPhaseEnded) {
				// if this was released before the game got to see it,
				// make it a special case
				if ( sysTouches[minIndex].stateCount == 1 ) {
					// leave it in the down state with a special count
					sysTouches[minIndex].stateCount = -1;
//					printf( "Tap release touch on a reuse\n" );
				} else {
					sysTouches[minIndex].down = false;
					sysTouches[minIndex].stateCount = 1;
//					printf( "Release touch on a reuse\n" );
				}
			} else {
				if (myTouch.phase == UITouchPhaseBegan) {
					sysTouches[minIndex].stateCount = 1;
					sysTouches[minIndex].controlOwner = NULL;
//					printf( "Begin touch on a reuse\n" );
				} else {
//					printf( "Drag touch on a reuse\n" );
				}
				sysTouches[minIndex].down = true;
			}
			touchThisSequence[minIndex] = true;
		} else {
			if ( myTouch.phase != UITouchPhaseBegan ) {
				printf( "Non-local touch wasn't a begin\n" );
			} else {
				// allocate a new one
				// grab the next rover spot
				// don't just use first-not-down, because that might
				// cause the release to be missed by the game code.
				int i, j;
				for ( j = 0 ; j < MAX_TOUCHES ; j++ ) {
					i = touchRover;
					t2 = &sysTouches[i];
					touchRover = ( touchRover + 1 ) % MAX_TOUCHES;				
					if ( !t2->down ) {
						break;
					}
				}
				if ( j == MAX_TOUCHES ) {
					printf( "MAX_TOUCHES, clearing everything!\n" );
					memset( sysTouches, 0, sizeof( sysTouches ) );
					continue;
				}
//				printf( "new touch down\n" );
				t2->x = x;
				t2->y = y;
				t2->down = true;
				t2->controlOwner = NULL;
				t2->stateCount = 1;
				
				touchThisSequence[i] = true;
			}
		}
    }
	// Change any active touches to released if they weren't
	// in the touch set.  This will happen if we forced a break because
	// a "moved" event was so large that it was very likely a release and
	// press of a different finger that happened to be in the same frame.
	for ( int i = 0 ; i < MAX_TOUCHES ; i++ ) {
		if ( sysTouches[i].down && !touchThisSequence[i] ) {
			printf( "clearing touch %i\n", i );
			sysTouches[i].down = false;
			sysTouches[i].stateCount = 0;
			touchCount--;
		}
	}
	
	// toggle the console with four touches
	if ( touchCount == 4 && previousTouchCount != 4 ) {	
		touchCount = 0;	// won't get the releases, because the text field will eat them

		if ( textField == nil ) {
			// do this before starting the textField, which
			// takes a long time
			// iphoneActivateConsole();
			
			textField = [UITextField alloc];
			[textField initWithFrame:CGRectMake( 0, 0, 20, 20 ) ];
			[self addSubview:textField];
			[textField release];
			textField.hidden = true;
			textField.delegate = self;
			textField.autocapitalizationType = UITextAutocapitalizationTypeNone;
			textField.autocorrectionType = UITextAutocorrectionTypeNo;
			[textField becomeFirstResponder];
		} else {
		}		
	}

	// the game is free to copy the touches now
	pthread_mutex_unlock( &eventMutex );
	
	previousTouchCount = touchCount;	
}


- (void) touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
//	printf( "touchesBegan\n" );
	[self handleTouches:event];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event {
//	printf( "touchesMoved\n" );
	[self handleTouches:event];
}

- (void) touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
//	printf( "touchesEnded\n" );
	[self handleTouches:event];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event {
//	printf( "touchesCancelled\n" );
	[self handleTouches:event];
}



@end


@implementation EAGLView (UITextFieldDelegate)

char	consoleCommand[1024];

- (BOOL)textFieldShouldReturn:(UITextField *)_textField 
{
	if ( eaglview->textField == nil ) {
		return YES;
	}

	// we can't just execute this, because we are running in another
	// thread, so fetch the line and the game will catch it next time
	// around

	// lock the game out temporarily
	pthread_mutex_lock( &eventMutex );

	const char *line = [ eaglview->textField.text UTF8String ];
	strncpy( consoleCommand, line, sizeof(consoleCommand)-1 );
	eaglview->textField.text = [ NSString stringWithUTF8String: "" ];
	
	// put it away
	[textField resignFirstResponder];
	[textField removeFromSuperview];
	textField = nil;
	
	// lock the game out temporarily
	pthread_mutex_unlock( &eventMutex );
	
	return YES;
}

@end


const char * SysIPhoneGetConsoleTextField() {
	if ( eaglview->textField == nil ) {
		return "";
	}
	return [ eaglview->textField.text UTF8String ];
}

void SysIPhoneSetConsoleTextField( const char * str) {
	assert( eaglview->textField != nil );	
	eaglview->textField.text = [ NSString stringWithUTF8String: str ];
}

void SysIPhoneSwapBuffers() {
    [context presentRenderbuffer:GL_RENDERBUFFER_OES];
}

void SysIPhoneOpenURL( const char *url ) {
	Com_Printf( "OpenURL char *: %s\n", url );
	
	NSString *nss = [NSString stringWithCString: url encoding: NSASCIIStringEncoding];
	[[UIApplication sharedApplication] openURL:[NSURL URLWithString: nss]];
}

void SysShowAlert(const char *title, const char *message) {
	NSString *nss_title = [NSString stringWithCString: title encoding: NSASCIIStringEncoding];
	NSString *nss_message = [NSString stringWithCString: message encoding: NSASCIIStringEncoding];
	UIAlertView *Alert = [[UIAlertView alloc] initWithTitle: nss_title message: nss_message delegate:nil cancelButtonTitle: @"Ok" otherButtonTitles: nil];	
	
	[Alert show];
	[Alert release];
}

void SysIPhoneSetUIKitOrientation( int isLandscapeRight ) {
	if ( isLandscapeRight ) {
		[UIApplication sharedApplication].statusBarOrientation = UIInterfaceOrientationLandscapeRight;
	} else {
		[UIApplication sharedApplication].statusBarOrientation = UIInterfaceOrientationLandscapeLeft;
	}
}



