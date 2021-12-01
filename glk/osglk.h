/******************************************************************************
 *                                                                            *
 * Copyright (C) 2021 by Dannii Willis.                                       *
 *                                                                            *
 * This file is part of the TADS Glk port.                                    *
 *                                                                            *
 * TADS is free software; you can redistribute it and/or modify               *
 * it under the terms version 2 of the GNU General Public License as          *
 * published by the Free Software Foundation.                                 *
 *                                                                            *
 * TADS is distributed in the hope that it will be useful,                    *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with TADS; if not, write to the Free Software                        *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#ifndef OSGLK_H
#define OSGLK_H

#ifdef __cplusplus
extern "C" {
#endif

// Patch Remglk's fileref_get_filename function
#include "glk.h"
#include "glkstart.h"
#ifdef GLKUNIX_FILEREF_GET_FILENAME

#include "remglk.h"
#define GLK_MODULE_FILEREF_GET_NAME
extern char *glkunix_fileref_get_filename(frefid_t fref);
#define garglk_fileref_get_name glkunix_fileref_get_filename

#endif /* GLKUNIX_FILEREF_GET_FILENAME */

// For measuring the default background colour of the main window
extern glui32 mainbg;
#define UNMEASURED_COLOUR 0xFFFFFFFF
#define UNSET_COLOUR 0xFF001234

#ifdef __cplusplus
}
#endif

#endif /* OSGLK_H */