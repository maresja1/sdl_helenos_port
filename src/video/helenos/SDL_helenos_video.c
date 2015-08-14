/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/* GGI-based SDL video driver implementation.
*/

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <gui/window.h>
#include <gui/canvas.h>
#include <draw/surface.h>
#include <libc/io/pixelmap.h>

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "SDL_helenos_video.h"
#include "SDL_helenos_mouse_c.h"
#include "SDL_helenos_events.h"


struct private_hwdata
{
	window_t *window;
	surface_t *surface;
};

/* Initialization/Query functions */
static int HELENOS_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **HELENOS_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *HELENOS_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int HELENOS_SetColors(_THIS, int firstcolor, int ncolors,
			 SDL_Color *colors);
static void HELENOS_VideoQuit(_THIS);

/* Hardware surface functions */
static int HELENOS_AllocHWSurface(_THIS, SDL_Surface *surface);
static int HELENOS_LockHWSurface(_THIS, SDL_Surface *surface);
static void HELENOS_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void HELENOS_FreeHWSurface(_THIS, SDL_Surface *surface);
/* HelenOS driver bootstrap functions */

static int HELENOS_Available(void)
{
	//TODO add proper check
	return 1;
}

static void HELENOS_DeleteDevice(SDL_VideoDevice *device)
{
	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_VideoDevice *HELENOS_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		SDL_memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				SDL_malloc((sizeof *device->hidden));
	}
	if ( (device == NULL) || (device->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( device ) {
			SDL_free(device);
		}
		return(0);
	}
	SDL_memset(device->hidden, 0, (sizeof *device->hidden));

	/* Set the function pointers */
	device->VideoInit = HELENOS_VideoInit;
	device->ListModes = HELENOS_ListModes;
	device->SetVideoMode = HELENOS_SetVideoMode;
	device->SetColors = HELENOS_SetColors;
	device->UpdateRects = NULL;
	device->VideoQuit = HELENOS_VideoQuit;
	device->AllocHWSurface = HELENOS_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = HELENOS_LockHWSurface;
	device->UnlockHWSurface = HELENOS_UnlockHWSurface;
	device->FlipHWSurface = NULL;
	device->FreeHWSurface = HELENOS_FreeHWSurface;
	device->SetCaption = NULL;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = NULL;
	device->GetWMInfo = NULL;
	device->InitOSKeymap = HELENOS_InitOSKeymap;
	device->PumpEvents = HELENOS_PumpEvents;

	device->free = HELENOS_DeleteDevice;

	return device;
}

VideoBootStrap HELENOS_bootstrap = {
	"helenos", "HelenOS GUI interface",
	HELENOS_Available, HELENOS_CreateDevice
};


static SDL_Rect video_mode;

#define FRAME_WIDTH 800
#define FRAME_HEIGHT 640
int HELENOS_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
	this->hidden->window = window_open("comp:0/winreg", WINDOW_MAIN | WINDOW_DECORATED, "SDL window");
	if (!this->hidden->window) {
		SDL_SetError("Cannot open main window.\n");
		return 1;
	}
;
	this->hidden->surface = surface_create(FRAME_WIDTH, FRAME_HEIGHT, NULL, 0);

	if(!this->hidden->surface){
		SDL_SetError("Could not create surface!");
		return NULL;
	}

	this->hidden->canvas = create_canvas(window_root(this->hidden->window),
					     FRAME_WIDTH, FRAME_HEIGHT, this->hidden->surface);

	if(!this->hidden->canvas) {
		surface_destroy(this->hidden->surface);
		window_close(this->hidden->window);
		SDL_SetError("Cannot create canvas.\n");
		return 1;
	}

	window_resize(this->hidden->window, 0, 0, FRAME_WIDTH + 8, FRAME_HEIGHT + 28,
		      WINDOW_PLACEMENT_RIGHT | WINDOW_PLACEMENT_BOTTOM);

	window_exec(this->hidden->window);

	this->info.current_w = FRAME_WIDTH;
	this->info.current_h = FRAME_HEIGHT;

	vformat->BitsPerPixel = 32;
	vformat->BytesPerPixel = 4;
	vformat->Amask = 0xff000000;
	vformat->Rmask = 0x00ff0000;
	vformat->Gmask = 0x0000ff00;
	vformat->Bmask = 0x000000ff;

	/* Fill in some window manager capabilities */
	this->info.wm_available = 1;

	return(0);
}

SDL_Rect **HELENOS_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
	return (SDL_Rect **) -1;
}
/* Various screen update functions available */
static void HELENOS_DirectUpdate(_THIS, int numrects, SDL_Rect *rects);

SDL_Surface *HELENOS_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags)
{
	/* HelenOS supports only 32 bits per pixel */
	bpp = 32;

	if(this->hidden->surface){
		surface_destroy(this->hidden->surface);
		this->hidden->surface = NULL;
	}

	this->hidden->surface = surface_create(width, height, NULL, 0);

	if(!this->hidden->surface){
		SDL_SetError("Could not create surface!");
		return NULL;
	}

	if(this->hidden->canvas){
		deinit_canvas(this->hidden->canvas);
		free(this->hidden->canvas);
		this->hidden->canvas = NULL;
	}

	this->hidden->canvas = create_canvas(window_root(this->hidden->window),
					     width, height, this->hidden->surface);

	sig_connect(&this->hidden->canvas->keyboard_event, NULL,
		    canvas_push_keyboard_event);
	sig_connect(&this->hidden->canvas->position_event, NULL,
		    canvas_push_position_event);

	if(!this->hidden->canvas) {
		SDL_SetError("Could not create canvas!");
		return NULL;
	}

	window_resize(this->hidden->window, 0, 0, width + 8, height + 28,
		      WINDOW_PLACEMENT_LEFT | WINDOW_PLACEMENT_TOP);

	
	/* Set up the new mode framebuffer */
	current->flags = 0;
	current->w = width;
	current->h = height;
	current->pitch = (uint16_t)(width*4);
	current->format->BitsPerPixel = bpp;
	current->format->BytesPerPixel = bpp/8;
	current->pixels = surface_pixmap_access(this->hidden->surface)->data;

	/* Set the blit function */
	this->UpdateRects = HELENOS_DirectUpdate;

	/* We're done */
	return(current);
}

static int HELENOS_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	return(-1);
}
static void HELENOS_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}
static int HELENOS_LockHWSurface(_THIS, SDL_Surface *surface)
{
	return(0);
}
static void HELENOS_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

static void HELENOS_DirectUpdate(_THIS, int numrects, SDL_Rect *rects)
{
	update_canvas(this->hidden->canvas,this->hidden->surface);
	return;
}

int HELENOS_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	return 0;
}
	
void HELENOS_VideoQuit(_THIS)
{
}
void HELENOS_FinalQuit(void) 
{
}