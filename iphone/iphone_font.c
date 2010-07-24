/*
 *  iphone_font.c
 *  Doom
 *
 *  Created by pcwiz on 01/02/10.
 *  Copyright 2010 leowiz. All rights reserved.
 *
 */

#include "../doomiphone.h"

typedef struct {
	unsigned short	x0, y0, x1, y1;
	float	xoff, yoff, xadvance;
} GlyphRect;

#ifdef USE_ID_FONT
#include "arialGlyphRects.h"	// precalculated offsets in the font image
#else
#include "droidGlyphRects.h"
#endif

float getKerning( const char *str ){
#ifndef USE_ID_FONT
	int	i, j;
	i = str[0];
	j = str[1];
	if ( i >= ' ' && i < 128 && j >= ' ' && j < 128 ) {
		return ((float)(glyphKernings[i*(128-32) + j]))/256.0;
	}
#endif
	return 0;

}

float	StringFontWidth( const char *str ) {
	float	len = 0;
	float	kerning = 0;
	while ( *str ) {
		int i = *str;
		if ( i >= ' ' && i < 128 ) {
			len += glyphRects[i-32].xadvance + kerning;
			kerning = getKerning( str );
		}
		str++;
	}
	return len;
}

/*
 ==================
 iphoneDrawText
 
 Returns the width in pixels
 ==================
 */
float iphoneDrawText( float x, float y, float scale, const char *str ) {
	float	fx = x;
	float	fy = y;
	float	kerning = 0;
	
	PK_BindTexture( arialFontTexture );
	glBegin( GL_QUADS );
	
	while ( *str ) {
		int i = *str;
		if ( i >= ' ' && i < 128 ) {
			GlyphRect *glyph = &glyphRects[i-32];
			
			// the glyphRects don't include the shadow outline
			float	x0 = ( glyph->x0 - 1 ) / 256.0;
			float	y0 = ( glyph->y0 - 1 ) / 256.0;
			float	x1 = ( glyph->x1 + 2 ) / 256.0;
			float	y1 = ( glyph->y1 + 2 ) / 256.0;
			
			float	width = ( x1 - x0 ) * 256 * scale;
			float	height = ( y1 - y0 ) * 256 * scale;
			
			float	xoff = ( glyph->xoff - 1 ) * scale + kerning;
			float	yoff = ( glyph->yoff - 1 ) * scale;
			
			glTexCoord2f( x0, y0 );
			glVertex2f( fx + xoff, fy + yoff );
			
			glTexCoord2f( x1, y0 );
			glVertex2f( fx + xoff + width, fy + yoff );
			
			glTexCoord2f( x1, y1 );
			glVertex2f( fx + xoff + width, fy + yoff + height );
			
			glTexCoord2f( x0, y1 );
			glVertex2f( fx + xoff, fy + yoff + height );
      
			fx += glyph->xadvance * scale + kerning;
      //			fx += ceil(glyph->xadvance);	// with the outline, ceil is probably the right thing
			kerning = getKerning( str );
		}
		str++;
	}
	
	glEnd();
	
	return fx - x;
}

/*
 ==================
 iphoneCenterText
 
 Returns the width in pixels
 ==================
 */
float iphoneCenterText( float x, float y, float scale, const char *str ) {
	float l = StringFontWidth( str );
	x -= l * scale * 0.5;
	return iphoneDrawText( x, y, scale, str );
}
