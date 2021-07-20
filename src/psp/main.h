// PSPGBA (VisualBoyAdvance port for PSP)
// Copyright (C) 2005 psp298
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
#ifndef MAIN_H
#define MAIN_H

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <psppower.h>
#include <psprtc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define VERCNF "PSPGBA v1.1"
#define MAX_ENTRY 2048
#define PSP_SCREEN_WIDTH 480
#define PSP_SCREEN_HEIGHT 272
#define PSP_LINE_SIZE 512
#define PSP_PIXEL_FORMAT 3
#define GBA_SCREEN_WIDTH 240
#define GBA_SCREEN_HEIGHT 160

#define GUARD_LINE 0
#define PIXEL_SIZE 2
#define MAX_PATH 512
#define MAX_NAME 256

#define RGB(r,g,b) ((((b>>3) & 0x1F)<<11)|(((g>>3) & 0x1F)<<6)|(((r>>3) & 0x1F)<<0))

static inline void __memcpy4a(unsigned long *d, unsigned long *s, unsigned long c)
{
	unsigned long wk,counter;

	asm volatile (
		"		.set	push"				"\n"
		"		.set	noreorder"			"\n"

		"		move	%1,%4 "				"\n"
		"1:		lw		%0,0(%3) "			"\n"
		"		addiu	%1,%1,-1 "			"\n"
		"		addiu	%3,%3,4 "			"\n"
		"		sw		%0,0(%2) "			"\n"
		"		bnez	%1,1b "				"\n"
		"		addiu	%2,%2,4 "			"\n"

		"		.set	pop"				"\n"

			:	"=&r" (wk),		// %0
				"=&r" (counter)	// %1
			:	"r" (d),		// %2
				"r" (s),		// %3
				"r" (c)			// %4
	);
}


typedef struct
{
	unsigned long Buttons;
	int n;
} S_BUTTON;


typedef struct
{
	char vercnf[16];
	u8 compress;
	u8 thumb;
	u8 quickslot;
	u8 screensize;
	u8 bScreenSizes[16]; //余分に確保
	u8 bGB_Pals[32]; //余分に確保
	u8 frameskip;
	u8 vsync;
	u8 sound;
	u8 sound_buffer;
	u8 cpu_clock;
	S_BUTTON skeys[32]; //余分に確保
	u8 analog2dpad;
	u16 color[4];
	u8 bgbright;
	char lastpath[256];
} SETTING;

enum{
	SCR_FULL_3_2,
	SCR_FULL_16_9,
	SCR_GBA_240_160,
	SCR_END,
};

extern SETTING setting, tmpsetting;
extern int cartridgeType;
extern u16 soundFinalWave[1470];
extern int soundBufferIndex;
extern unsigned int soundBufferReady;
extern char GBAPath[MAX_PATH];


bool loadGame(char *fileName);
int delete_state(int slot,const char* savePath);
int load_state(int slot,const char* savePath);
int load_thumb(int slot, u16 *out, size_t outlen,const char* savePath);
int read_png(char *path, u16 *out, size_t outlen);
int save_state(int slot,const char* savePath);
int save_thumb(int slot,const char* savePath);
char* get_state_path(int slot, char *out,const char* savePath);
void get_thumb_path(int slot, char *out,const char* savePath);
void menu();
u16* getVramDrawBuffer();
void myBitBlt(u16 x,u16 y,u16 w,u16 h,u16* d);
void myBitBltGe(u16* src, bool sync);
void myBitBltGe3_2(u16* src, bool sync);
void myDrawFrame(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void myScreenClear();
void myScreenInit(int pixelFormat, int texFormat);
void myScreenPrintf(const char *format, ...);
void myPrintfXY(u16 x,u16 y,u16 color,const char *str, ...);
void myScreenFlip();
void myDebug(bool reset,const char *str, ...);
void myScreenSetTextColor(u16 colour);
void myScreenSetXY(int x, int y);
void startGame(int skip, bool vsync, bool sound, int zoom, bool reset);
void wavoutClear();
void load_config(const char* gbaPath);
void save_config(const char* gbaPath);
int wavoutInit();
void set_cpu_clock(int n);
void* valloc(int size,int len);

#endif
