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
// vid_null.c -- null video driver to aid porting efforts

#include "quakedef.h"
#include "d_local.h"
#include "SDL3\SDL.h"

viddef_t	vid;				// global video state


int SCREENWIDTH = 1080, SCREENHEIGHT = 720;

// fix unresolved externals
int (*LittleLong)(int l);
byte* host_colormap;
short* d_pzbuffer;

byte* vid_buffer;//[SCREENWIDTH*SCREENHEIGHT];
short* zbuffer;//[SCREENWIDTH*SCREENHEIGHT];
byte	surfcache[256*1024];

unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];

SDL_Texture *texture;
SDL_Renderer *renderer;
SDL_Window *trueWindow;

byte* quake_palette;
byte active_palette[768];

unsigned char	d_15to8table[65536];

void VID_ShiftPalette(unsigned char* p)
{
		VID_SetPalette(p);
}

void VID_SetPalette(unsigned char* palette)
{
	memcpy(active_palette, palette, 768);

	byte* pal;
	unsigned r, g, b;
	unsigned v;
	int     r1, g1, b1;
	int		j, k, l, m;
	unsigned short i;
	unsigned* table;
	FILE* f;
	char s[255];
	int dist, bestdist;

	//
	// 8 8 8 encoding
	//
	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 256; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		*table++ = v;
	}
	d_8to24table[255] &= 0xffffff;	// 255 is transparent

	for (i = 0; i < (1 << 15); i++) {
		/* Maps
		000000000000000
		000000000011111 = Red  = 0x1F
		000001111100000 = Blue = 0x03E0
		111110000000000 = Grn  = 0x7C00
		*/
		r = ((i & 0x1F) << 3) + 4;
		g = ((i & 0x03E0) >> 2) + 4;
		b = ((i & 0x7C00) >> 7) + 4;
		pal = (unsigned char*)d_8to24table;
		for (v = 0, k = 0, bestdist = 10000 * 10000; v < 256; v++, pal += 4) {
			r1 = (int)r - (int)pal[0];
			g1 = (int)g - (int)pal[1];
			b1 = (int)b - (int)pal[2];
			dist = (r1 * r1) + (g1 * g1) + (b1 * b1);
			if (dist < bestdist) {
				k = v;
				bestdist = dist;
			}
		}
		d_15to8table[i] = k;
	}
}

void	VID_Init (unsigned char *palette)
{
    vid_buffer = malloc(SCREENWIDTH * SCREENHEIGHT);
    zbuffer = malloc(SCREENWIDTH * SCREENHEIGHT * sizeof(short));

	quake_palette = palette;
	memcpy(active_palette, palette, 768);
	vid.maxwarpwidth = vid.width = vid.conwidth = SCREENWIDTH;
	vid.maxwarpheight = vid.height = vid.conheight = SCREENHEIGHT;
	vid.aspect = 1.0;
	vid.numpages = 1;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid.buffer = vid.conbuffer = vid_buffer;
	vid.rowbytes = vid.conrowbytes = SCREENWIDTH;
	
	d_pzbuffer = zbuffer;
	D_InitCaches (surfcache, sizeof(surfcache));

	SDL_Init(SDL_INIT_VIDEO);
	SDL_CreateWindowAndRenderer("Quake", 1080, 720, SDL_WINDOW_RESIZABLE, &trueWindow, &renderer);
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREENWIDTH, SCREENHEIGHT);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
}

void	VID_Shutdown (void)
{
}

void VID_Update(vrect_t* rects)
{
    void* pixels;
    int pitch;

    SDL_LockTexture(texture, NULL, &pixels, &pitch);

    for (int y = 0; y < vid.height; y++)
    {
        byte* src = vid.buffer + y * vid.rowbytes;
        Uint32* dst = (Uint32*)((byte*)pixels + y * pitch);

        for (int x = 0; x < vid.width; x++)
        {
            byte p = src[x];

			byte r = active_palette[p * 3 + 0];
			byte g = active_palette[p * 3 + 1];
			byte b = active_palette[p * 3 + 2];

            dst[x] = 0xFF000000 |
                ((Uint32)r << 16) |
                ((Uint32)g << 8) |
                b;
        }
    }

    SDL_UnlockTexture(texture);

    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}




/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}

