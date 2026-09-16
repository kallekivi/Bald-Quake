/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// sys_null.h -- null system driver to aid porting efforts

#include "quakedef.h"
#include "errno.h"
#include "hellishInput.h"
#include "SDL3/SDL.h"
#include "menu.h"


/*
===============================================================================

FILE IO

===============================================================================
*/

#define MAX_HANDLES             10
FILE    *sys_handles[MAX_HANDLES];

extern SDL_Window* trueWindow;

int movedMouseX, movedMouseY;
int screenWidth = 1080, screenHeight = 720;

int             findhandle (void)
{
	int             i;
	
	for (i=1 ; i<MAX_HANDLES ; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

/*
================
filelength
================
*/
int filelength (FILE *f)
{
	int             pos;
	int             end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (char *path, int *hndl)
{
	FILE    *f;
	int             i;
	
	i = findhandle ();

	f = fopen(path, "rb");
	if (!f)
	{
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	*hndl = i;
	
	return filelength(f);
}

int Sys_FileOpenWrite (char *path)
{
	FILE    *f;
	int             i;
	
	i = findhandle ();

	f = fopen(path, "wb");
	if (!f)
		Sys_Error ("Error opening %s: %s", path,strerror(errno));
	sys_handles[i] = f;
	
	return i;
}

void Sys_FileClose (int handle)
{
	fclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	return fread (dest, 1, count, sys_handles[handle]);
}

int Sys_FileWrite (int handle, void *data, int count)
{
	return fwrite (data, 1, count, sys_handles[handle]);
}

int     Sys_FileTime (char *path)
{
	FILE    *f;
	
	f = fopen(path, "rb");
	if (f)
	{
		fclose(f);
		return 1;
	}
	
	return -1;
}

void Sys_mkdir (char *path)
{
}


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
}


void Sys_Error (char *error, ...)
{
	va_list         argptr;

	printf ("Sys_Error: ");   
	va_start (argptr,error);
	vprintf (error,argptr);
	va_end (argptr);
	printf ("\n");

	exit (1);
}

void Sys_Printf (char *fmt, ...)
{
	va_list         argptr;
	
	va_start (argptr,fmt);
	vprintf (fmt,argptr);
	va_end (argptr);
}

void Sys_Quit (void)
{
	Host_Shutdown();
	exit(0);
}


double Sys_FloatTime(void) {
	static Uint64 start;
	static Uint64 frequency;

	if (!frequency) {
		frequency = SDL_GetPerformanceFrequency();
		start = SDL_GetPerformanceCounter();
	}

	Uint64 now = SDL_GetPerformanceCounter();
	return (double)(now - start) / (double)frequency;
}


char *Sys_ConsoleInput (void)
{
	return NULL;
}

void Sys_Sleep (void)
{
}

void Sys_SendKeyEvents(void)
{
	HellishInput();
}



void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}

//=============================================================================

void PollStuff(void) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_EVENT_KEY_DOWN:
			if (event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT))
				SDL_SetWindowFullscreen(trueWindow, (SDL_GetWindowFlags(trueWindow) & SDL_WINDOW_FULLSCREEN) == 0);
			break;

		case SDL_EVENT_MOUSE_MOTION:
			movedMouseX = event.motion.xrel;
			movedMouseY = event.motion.yrel;
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			switch (event.button.button) {
			case SDL_BUTTON_LEFT:
				Key_Event(K_MOUSE1, true);
				break;
			case SDL_BUTTON_RIGHT:
				Key_Event(K_MOUSE2, true);
				break;
			case SDL_BUTTON_MIDDLE:
				Key_Event(K_MOUSE3, true);
				break;
			}
			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			switch (event.button.button) {
			case SDL_BUTTON_LEFT:
				Key_Event(K_MOUSE1, false);
				break;
			case SDL_BUTTON_RIGHT:
				Key_Event(K_MOUSE2, false);
				break;
			case SDL_BUTTON_MIDDLE:
				Key_Event(K_MOUSE3, false);
				break;
			}
			break;

		case SDL_EVENT_WINDOW_RESIZED:
			screenWidth = event.window.data1;
			screenHeight = event.window.data2;
			break;

		case SDL_EVENT_QUIT:
			printf("QUIT!\n");
			Sys_Quit();
			break;

		default:
			break;
		}
	}
}


qboolean		isDedicated = false;

void main (int argc, char **argv)
{
	static quakeparms_t    parms;

	parms.memsize = 8*1024*1024;
	parms.membase = malloc (parms.memsize);
	parms.basedir = ".";

	COM_InitArgv (argc, argv);

	parms.argc = argc;
	parms.argv = argv;

	if (COM_CheckParm("-lowres")) {
		SCREENWIDTH = 320;
		SCREENHEIGHT = 200;
	}

	printf ("Host_Init\n");
	Host_Init (&parms);

	double time, oldtime, newtime;

	oldtime = Sys_FloatTime();

	while ("main window message loop")
	{
		in_mlook.state |= 1; // mouse look forced on (might make a setting for this in future)

		PollStuff();
		static bool last = true;
		if (m_state == 0 && !last) SDL_SetWindowRelativeMouseMode(trueWindow, true), last = true;
		else if (m_state != 0 && last) SDL_SetWindowRelativeMouseMode(trueWindow, false), last = false;

		if (isDedicated)
		{
			newtime = Sys_FloatTime();
			time = newtime - oldtime;

			while (time < sys_ticrate.value)
			{
				Sys_Sleep();
				newtime = Sys_FloatTime();
				time = newtime - oldtime;
			}
		}
		else
		{
			newtime = Sys_FloatTime();
			time = newtime - oldtime;
		}

		Host_Frame(time);
		oldtime = newtime;
	}
}
