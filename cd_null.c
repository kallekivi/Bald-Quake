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
#include "quakedef.h"
#include <SDL.h>

char *cdPath[12];

void CDAudio_Play(byte track, qboolean looping) {
	
	printf("track is: %d\n", track);
	char* path = cdPath[track];
	int size, handle;
	void* audio;

	size = Sys_FileOpenRead(path, &handle);
	if (handle == -1) {
		printf("Can't find %s track\n", path);
		return;
	}

	audio = malloc(size);
	Sys_FileRead(handle, audio, size);
	Sys_FileClose(handle);
	CDHelpMe(audio, size);
	
}



void CDAudio_Stop(void)
{
}


void CDAudio_Pause(void)
{
}


void CDAudio_Resume(void)
{
}


void CDAudio_Update(void)
{
}


int CDAudio_Init(void)
{
	cdPath[0] = ".\\id1\\music\\01.wav";
	cdPath[1] = ".\\id1\\music\\02.wav";
	cdPath[2] = ".\\id1\\music\\03.wav";
	cdPath[3] = ".\\id1\\music\\04.wav";
	cdPath[4] = ".\\id1\\music\\05.wav";
	cdPath[5] = ".\\id1\\music\\06.wav";
	cdPath[6] = ".\\id1\\music\\07.wav";
	cdPath[7] = ".\\id1\\music\\01.wav";
	cdPath[11] = ".\\id1\\music\\08.wav";
	cdPath[8] = ".\\id1\\music\\09.wav";
	cdPath[9] = ".\\id1\\music\\10.wav";

	return 0;
}


void CDAudio_Shutdown(void)
{
}
