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

/* Handle the event stream, converting HelenOS events into SDL events */

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <libc/io/kbd_event.h>
#include <libc/io/pos_event.h>

#include "SDL.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
#include "SDL_helenos_events.h"
#include "SDL_helenos_video.h"

/* The translation tables from a HelenOS keycode to a SDL keysym */
static SDLKey keymap[128];

static int canvas_pop_keyboard_event(kbd_event_t *evt);
static int canvas_pop_position_event(pos_event_t *evt);
static SDL_keysym *HelenOS_TranslateKey(kbd_event_t *ev, SDL_keysym *keysym);

#define ROT_BUFFER_LEN 50
static kbd_event_t rot_kbd_evt_buffer[ROT_BUFFER_LEN];
static pos_event_t rot_pos_evt_buffer[ROT_BUFFER_LEN];

static int rot_kbd_evt_start = 0;
static int rot_kbd_evt_end = 0;
static int rot_pos_evt_start = 0;
static int rot_pos_evt_end = 0;



void HELENOS_PumpEvents(_THIS){
	kbd_event_t kbd_event;
	pos_event_t pos_event;
	SDL_keysym keysym;
	int posted=0;
	while(canvas_pop_keyboard_event(&kbd_event)){
		if(kbd_event.type == KEY_PRESS) {
			posted += SDL_PrivateKeyboard(SDL_PRESSED, HelenOS_TranslateKey(&kbd_event, &keysym));
		} else if(kbd_event.type == KEY_RELEASE){
			posted += SDL_PrivateKeyboard(SDL_RELEASED, HelenOS_TranslateKey(&kbd_event, &keysym));
		}
	}

	while(canvas_pop_position_event(&pos_event)){
		static int mouse_x = 0, mouse_y = 0;

		if(pos_event.type == POS_PRESS){
			posted += SDL_PrivateMouseButton(SDL_PRESSED, (uint8_t)pos_event.btn_num, 0, 0);
		} else if(pos_event.type == POS_RELEASE){
			posted += SDL_PrivateMouseButton(SDL_RELEASED, (uint8_t)pos_event.btn_num, 0, 0);
		} else if(pos_event.type == POS_UPDATE){
			if (mouse_x != pos_event.hpos || mouse_y != pos_event.vpos)
			{
				mouse_x = pos_event.hpos;
				mouse_y = pos_event.vpos;
//				int x = pos_event.hpos - mouse_x;
//				int y = pos_event.vpos - mouse_y;
				posted += SDL_PrivateMouseMotion(0, 0, pos_event.hpos,  pos_event.vpos);
			}
		}
	}
}

void canvas_push_keyboard_event(widget_t *widget, void *data){
	rot_kbd_evt_buffer[rot_kbd_evt_end] = *(kbd_event_t*)data;
	rot_kbd_evt_end = (rot_kbd_evt_end+1) % ROT_BUFFER_LEN;
}

void canvas_push_position_event(widget_t *widget, void *data){
	rot_pos_evt_buffer[rot_pos_evt_end] = *(pos_event_t*)data;
	rot_pos_evt_end = (rot_pos_evt_end+1) % ROT_BUFFER_LEN;
}


int canvas_pop_keyboard_event(kbd_event_t *evt){
	if(rot_kbd_evt_start == rot_kbd_evt_end){
		return 0;
	} else {
		memcpy(evt,&rot_kbd_evt_buffer[rot_kbd_evt_start],sizeof(kbd_event_t));
		rot_kbd_evt_start =  (rot_kbd_evt_start+1) % ROT_BUFFER_LEN;
		return 1;
	}
}

int canvas_pop_position_event(pos_event_t *evt){
	if(rot_pos_evt_start == rot_pos_evt_end){
		return 0;
	} else {
		memcpy(evt,&rot_pos_evt_buffer[rot_pos_evt_start],sizeof(pos_event_t));
		rot_pos_evt_start =  (rot_pos_evt_start+1) % ROT_BUFFER_LEN;
		return 1;
	}
}

static SDL_keysym *HelenOS_TranslateKey(kbd_event_t *ev, SDL_keysym *keysym)
{
	/* Set the keysym information */
	keysym->scancode = ev->key;
	keysym->sym = keymap[ev->key];
	keysym->mod = KMOD_NONE;

	/* If UNICODE is on, get the UNICODE value for the key */
	keysym->unicode = 0;
	if (SDL_TranslateUNICODE)
	{
		keysym->unicode = (uint16_t)ev->c;
	}

	return keysym;
}

void HELENOS_InitOSKeymap(_THIS){
	keymap[KC_1] = SDLK_1;
	keymap[KC_2] = SDLK_2;
	keymap[KC_3] = SDLK_3;
	keymap[KC_4] = SDLK_4;
	keymap[KC_5] = SDLK_5;
	keymap[KC_6] = SDLK_6;
	keymap[KC_7] = SDLK_7;
	keymap[KC_8] = SDLK_8;
	keymap[KC_9] = SDLK_9;
	keymap[KC_0] = SDLK_0;


	keymap[KC_MINUS] = SDLK_MINUS;
	keymap[KC_EQUALS] = SDLK_EQUALS;
	keymap[KC_BACKSPACE] = SDLK_BACKSPACE;
	keymap[KC_TAB] = SDLK_TAB;
	keymap[KC_Q] = SDLK_q;
	keymap[KC_W] = SDLK_w;
	keymap[KC_E] = SDLK_e;
	keymap[KC_R] = SDLK_r;
	keymap[KC_T] = SDLK_t;
	keymap[KC_Y] = SDLK_y;
	keymap[KC_U] = SDLK_u;
	keymap[KC_I] = SDLK_i;
	keymap[KC_O] = SDLK_o;
	keymap[KC_P] = SDLK_p;
	keymap[KC_LBRACKET] = SDLK_LEFTBRACKET;
	keymap[KC_RBRACKET] = SDLK_RIGHTBRACKET;
	keymap[KC_ENTER] = SDLK_RETURN;
	keymap[KC_LCTRL] = SDLK_LCTRL;
	keymap[KC_A] = SDLK_a;
	keymap[KC_S] = SDLK_s;
	keymap[KC_D] = SDLK_d;
	keymap[KC_F] = SDLK_f;
	keymap[KC_G] = SDLK_g;
	keymap[KC_H] = SDLK_h;
	keymap[KC_J] = SDLK_j;
	keymap[KC_K] = SDLK_k;
	keymap[KC_L] = SDLK_l;
	keymap[KC_SEMICOLON] = SDLK_SEMICOLON;
	keymap[KC_QUOTE] = SDLK_QUOTE;
	keymap[KC_HASH] = SDLK_BACKQUOTE;
	keymap[KC_LSHIFT] = SDLK_LSHIFT;
	keymap[KC_BACKSLASH] = SDLK_BACKSLASH;
	keymap[KC_Z] = SDLK_z;
	keymap[KC_X] = SDLK_x;
	keymap[KC_C] = SDLK_c;
	keymap[KC_V] = SDLK_v;
	keymap[KC_B] = SDLK_b;
	keymap[KC_N] = SDLK_n;
	keymap[KC_M] = SDLK_m;
	keymap[KC_COMMA] = SDLK_COMMA;
	keymap[KC_PERIOD] = SDLK_PERIOD;
	keymap[KC_SLASH] = SDLK_SLASH;
	keymap[KC_RSHIFT] = SDLK_RSHIFT;
	keymap[KC_NTIMES] = SDLK_KP_MULTIPLY;
	keymap[KC_LALT] = SDLK_LALT;
	keymap[KC_SPACE] = SDLK_SPACE;
	keymap[KC_CAPS_LOCK] = SDLK_CAPSLOCK;
	keymap[KC_F1] = SDLK_F1;
	keymap[KC_F2] = SDLK_F2;
	keymap[KC_F3] = SDLK_F3;
	keymap[KC_F4] = SDLK_F4;
	keymap[KC_F5] = SDLK_F5;
	keymap[KC_F6] = SDLK_F6;
	keymap[KC_F7] = SDLK_F7;
	keymap[KC_F8] = SDLK_F8;
	keymap[KC_F9] = SDLK_F9;
	keymap[KC_F10] = SDLK_F10;
	keymap[KC_NUM_LOCK] = SDLK_NUMLOCK;
	keymap[KC_SCROLL_LOCK] = SDLK_SCROLLOCK;
	keymap[KC_NMINUS] = SDLK_KP_MINUS;
	keymap[KC_NPLUS] = SDLK_KP_PLUS;
	keymap[KC_N1] = SDLK_KP1;
	keymap[KC_N2] = SDLK_KP2;
	keymap[KC_N3] = SDLK_KP3;
	keymap[KC_N4] = SDLK_KP4;
	keymap[KC_N5] = SDLK_KP5;
	keymap[KC_N6] = SDLK_KP6;
	keymap[KC_N7] = SDLK_KP7;
	keymap[KC_N8] = SDLK_KP8;
	keymap[KC_N9] = SDLK_KP9;
	keymap[KC_N0] = SDLK_KP0;
	keymap[KC_NPERIOD] = SDLK_KP_PERIOD;
	keymap[KC_F11] = SDLK_F11;
	keymap[KC_F12] = SDLK_F12;
	keymap[KC_NENTER] = SDLK_KP_ENTER;
	keymap[KC_RCTRL] = SDLK_RCTRL;
	keymap[KC_NSLASH] = SDLK_KP_DIVIDE;
	keymap[KC_PRTSCR] = SDLK_PRINT;
	keymap[KC_RALT] = SDLK_RALT;
	keymap[KC_BREAK] = SDLK_BREAK;
	keymap[KC_HOME] = SDLK_HOME;
	keymap[KC_UP] = SDLK_UP;
	keymap[KC_PAGE_UP] = SDLK_PAGEUP;
	keymap[KC_LEFT] = SDLK_LEFT;
	keymap[KC_RIGHT] = SDLK_RIGHT;
	keymap[KC_END] = SDLK_END;
	keymap[KC_DOWN] = SDLK_DOWN;
	keymap[KC_PAGE_DOWN] = SDLK_PAGEDOWN;
	keymap[KC_INSERT] = SDLK_INSERT;
	keymap[KC_DELETE] = SDLK_DELETE;
}