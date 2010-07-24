typedef struct {
	unsigned short	x0, y0, x1, y1;
	float	xoff, yoff, xadvance;
} GlyphRect;

#ifdef USE_DROID
#include "droidGlyphRects.h"
#else
#include "arialGlyphRects.h"	// precalculated offsets in the font image
#endif

float	StringFontWidth( const char *str ) {
	float	len = 0;
	while ( *str ) {
		int i = *str;
		if ( i >= ' ' && i < 128 ) {
			len += glyphRects[i-32].xadvance;
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
	float	kerning = 0.0f;
	
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
			
			float	xoff = ( glyph->xoff - 1 ) * scale;
			float	yoff = ( glyph->yoff - 1 ) * scale;
			
			glTexCoord2f( x0, y0 );
			glVertex2f( fx + xoff, fy + yoff );
			
			glTexCoord2f( x1, y0 );
			glVertex2f( fx + xoff + width, fy + yoff );
			
			glTexCoord2f( x1, y1 );
			glVertex2f( fx + xoff + width, fy + yoff + height );
			
			glTexCoord2f( x0, y1 );
			glVertex2f( fx + xoff, fy + yoff + height );

			// with our default texture, the difference is negligable
			fx += glyph->xadvance * scale;
//			fx += ceil(glyph->xadvance);	// with the outline, ceil is probably the right thing
			if ( str[1] > ' ' && str[1] < 128 ){
				kerning = (float)glyphKernings[(str[1]-' ')*96 + (str[1]-' ')];
			} else {
				kerning = 0.0f;
			}				
		str++;
	}
	
	glEnd();
	
	return fx - x;
}
