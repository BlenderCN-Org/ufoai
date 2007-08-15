/**
 * @file qgl_osx.c
 * @brief This file implements the MacOS X system bindings of OpenGL to QGL function pointers
 * Written by:	awe				[mailto:awe@fruitz-of-dojo.de]
 * 2001-2002 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de]
 */

/*

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma mark =Defines=

#define QGL

#pragma mark -


#pragma mark =Includes=
#import <mach-o/dyld.h>
#include "../../ref_gl/gl_local.h"
#pragma mark -

#pragma mark =Macros=
#define SIG(x) 		fprintf (gQGLLogFile, x "\n")
#define GPA(A)		qglGetProcAddress (A, QR_SAFE_SYMBOL)
#pragma mark -


#pragma mark =Enums=
enum qglGetAdrMode {
    QR_LAZY_SYMBOL = 0,
    QR_SAFE_SYMBOL
};
#pragma mark -

void ( APIENTRY * qglPNTrianglesiATIX) (GLenum pname, GLint param);
void ( APIENTRY * qglPNTrianglesfATIX) (GLenum pname, GLfloat param);

/**
 * @brief
 */
void* qglGetProcAddress (const char *theName, enum qglGetAdrMode theMode)
{
	NSSymbol mySymbol = NULL;
	char *mySymbolName = malloc(strlen(theName) + 2);

	if (mySymbolName != NULL) {
		Com_sprintf(mySymbolName, strlen(theName) + 2, "_%s", theName);

		mySymbol = NULL;
		if (NSIsSymbolNameDefined(mySymbolName))
			mySymbol = NSLookupAndBindSymbol(mySymbolName);

		free(mySymbolName);
	}

	if (theMode == QR_SAFE_SYMBOL && mySymbol == NULL) {
		ri.Sys_Error(ERR_FATAL, "Failed to import a required OpenGL function!\n");
	}

	return ((mySymbol != NULL) ? NSAddressOfSymbol(mySymbol) : NULL);
}

/**
 * @brief
 */
void* qwglGetProcAddress (const char *theSymbol)
{
	return (qglGetProcAddress(theSymbol, QR_LAZY_SYMBOL));
}

/**
 * @brief
 * @sa QR_UnLink
 */
void QR_Shutdown (void)
{
	/* general pointers */
	QR_UnLink();
	/* macos specific */
	qglPNTrianglesiATIX          = NULL;
	qglPNTrianglesfATIX          = NULL;
}

/**
 * @brief This is responsible for binding our qgl function pointers to the appropriate GL stuff
 * @sa QR_Init
 */
qboolean QR_Init (const char *dllname)
{
	/* general qgl bindings */
	QR_Link();
	/* mac specific ones */
	qglPNTrianglesiATIX          = NULL;
	qglPNTrianglesfATIX          = NULL;
	return (qtrue);
}
