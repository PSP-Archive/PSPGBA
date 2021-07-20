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
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psptypes.h>
#include <pspge.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "main.h"


extern u8 msx[];

static int X = 0, Y = 0;
static int MX=67, MY=34;
static u16 bg_col = 0, fg_col = 0xFFFF;
static u16* g_vram_base = (u16*) (0x44000000);
static int init = 0;
static unsigned int __attribute__((aligned(16))) list[262144];
static int dispBufferNumber;
static int displayPixelFormat = PSP_DISPLAY_PIXEL_FORMAT_565;

static unsigned int *GEcmd = (unsigned int *)0x441FC000;
static short *ScreenVertex = (short *)0x441FC100;
static int gecbid = -1;


u16* getVramDrawBuffer()
{
	u16* vram = g_vram_base;
	if (dispBufferNumber == 0) 
		vram += (PSP_LINE_SIZE * PSP_SCREEN_HEIGHT);
	return vram;
}

static void clear_screen(u16 color)
{
	u16 *vram = getVramDrawBuffer();
	for (int i=0;i<PSP_LINE_SIZE * PSP_SCREEN_HEIGHT;i++)
	 	*(vram+i)=0;
}

void myScreenFlip()
{
	u16 *vram=getVramDrawBuffer();
	sceGuSwapBuffers();
	sceDisplaySetFrameBuf(vram, PSP_LINE_SIZE, displayPixelFormat, PSP_DISPLAY_SETBUF_NEXTFRAME);
	dispBufferNumber ^= 1;
}



void myScreenSetBackColor(u16 colour)
{
	bg_col = colour;
}

void myScreenSetTextColor(u16 colour)
{
	fg_col = colour;
}



void myScreenPutChar( int x, int y, u16 color, u8 ch)
{
	
	int 	i,j, l;
	u8	*font;
	u16  pixel;
	u16 *vram_ptr;
	u16 *vram;

	if(!init)
	{
		return;
	}

	vram = (u16*)getVramDrawBuffer() + x;
	vram += (y * PSP_LINE_SIZE);

	font = &msx[ (int)ch * 8];
	for (i=l=0; i < 8; i++, l+= 8, font++)
	{
		vram_ptr  = vram;
		for (j=0; j < 8; j++)
		{
			if ((*font & (128 >> j)))
			{
				pixel = color;
				*vram_ptr++ = pixel;
			}
			else
			{
				vram_ptr++;
			}
		}
		vram += PSP_LINE_SIZE;
	}
}

static void  clear_line( int Y)
{
	int i;
	for (i=0; i < MX; i++)
	myScreenPutChar( i*7 , Y * 8, bg_col, 219);
}

int myScreenPrintData(const char *buff, int size)
{
	int i;
	int j;
	char c;

	for (i = 0; i < size; i++)
	{
		c = buff[i];
		switch (c)
		{
			case '\n':
			X = 0;
			Y ++;
			if (Y == MY)
			Y = 0;
			clear_line(Y);
			break;
			case '\t':
			for (j = 0; j < 5; j++) {
				myScreenPutChar( X*7 , Y * 8, fg_col, ' ');
				X++;
			}
			break;
			default:
			myScreenPutChar( X*7 , Y * 8, fg_col, c);
			X++;
			if (X == MX)
			{
				return i;
				X = 0;
				Y++;
				if (Y == MY)
				Y = 0;
				clear_line(Y);
			}
		}
	}

	return i;
}

void myPrintfXY(u16 x,u16 y,u16 color,const char *str, ...)
{
	va_list ap;
	char szBuf[512];

	va_start(ap, str);
	vsprintf(szBuf, str, ap);
	va_end(ap);

	myScreenSetXY(x,y);
	myScreenSetTextColor(color);
	myScreenPrintf(szBuf);
}


void myScreenPrintf(const char *format, ...)
{
	va_list	opt;
	char     buff[2048];
	int		bufsz;

	if(!init)
	{
		return;
	}

	va_start(opt, format);
	bufsz = vsnprintf( buff, (size_t) sizeof(buff), format, opt);
	(void) myScreenPrintData(buff, bufsz);
}


void myScreenSetXY(int x, int y)
{
	if( x<MX && x>=0 ) X=x;
	if( y<MY && y>=0 ) Y=y;
}

int myScreenGetX()
{
	return X;
}

int myScreenGetY()
{
	return Y;
}

void myScreenClear()
{
	if(!init)
	{
		return;
	}
	clear_screen(bg_col);
}

void Ge_Finish_Callback(int id, void *arg)
{
}


void myScreenInit(int pixelFormat, int texFormat)
{
	displayPixelFormat=pixelFormat;
	
	sceDisplaySetMode(0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	
	dispBufferNumber = 0;
	sceDisplayWaitVblankStart();
	sceDisplaySetFrameBuf((void*) g_vram_base, PSP_LINE_SIZE, pixelFormat, PSP_DISPLAY_SETBUF_NEXTFRAME);

	sceGuInit();
	
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(texFormat , (void*)0, PSP_LINE_SIZE);
	sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, (void*)0, PSP_LINE_SIZE);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

	sceGuDepthBuffer((void*) 0x110000, PSP_LINE_SIZE);
	sceGuOffset(2048 - (PSP_SCREEN_WIDTH / 2), 2048 - (PSP_SCREEN_HEIGHT / 2));
	sceGuViewport(2048, 2048, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	sceGuDepthRange(0xc350, 0x2710);

	sceGuScissor(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuTexMode(texFormat , 0, 0, GU_FALSE);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	
	sceGuAlphaFunc(GU_GREATER, 0, 0xff);
	sceGuEnable(GU_ALPHA_TEST);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_CULL_FACE);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuAmbientColor(0xffffffff);
	sceGuDisable(GU_BLEND);

	sceGuFinish();
	sceGuSync(0, 0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);

	PspGeCallbackData gecb;
	gecb.signal_func = NULL;
	gecb.signal_arg = NULL;
	gecb.finish_func = Ge_Finish_Callback;
	gecb.finish_arg = NULL;
	gecbid = sceGeSetCallback(&gecb);

	X = Y = 0;
	init = 1;
	
}



void myBitBltGe(u16* src, bool sync)
{
	u16* dest = getVramDrawBuffer();
	static int qid = -1;
	ScreenVertex[0] = 0;
	ScreenVertex[1] = 0;
	ScreenVertex[2] = 0;
	ScreenVertex[3] = 0;
	ScreenVertex[4] = 0;
	ScreenVertex[5] = GBA_SCREEN_WIDTH;
	ScreenVertex[6] = GBA_SCREEN_HEIGHT;
	ScreenVertex[7] = PSP_SCREEN_WIDTH;
	ScreenVertex[8] = PSP_SCREEN_HEIGHT;
	ScreenVertex[9] = 0;

	// Set Draw Buffer
	GEcmd[ 0] = 0x9C000000UL | ((u32)(unsigned char *)dest & 0x00FFFFFF);
	GEcmd[ 1] = 0x9D000000UL | (((u32)(unsigned char *)dest & 0xFF000000) >> 8) | 512;
	// Set Tex Buffer
	GEcmd[ 2] = 0xA0000000UL | ((u32)(unsigned char *)src & 0x00FFFFFF);
	GEcmd[ 3] = 0xA8000000UL | (((u32)(unsigned char *)src & 0xFF000000) >> 8) | GBA_SCREEN_WIDTH;
	// Tex size
	GEcmd[ 4] = 0xB8000000UL | (8 << 8) | 8;
	// Tex Flush
	GEcmd[ 5] = 0xCB000000UL;
	// Set Vertex
	GEcmd[ 6] = 0x12000000UL | (1 << 23) | (0 << 11) | (0 << 9) | (2 << 7) | (0 << 5) | (0 << 2) | 2;
	GEcmd[ 7] = 0x10000000UL;
	GEcmd[ 8] = 0x02000000UL;
	GEcmd[ 9] = 0x10000000UL | (((u32)(void *)ScreenVertex & 0xFF000000) >> 8);
	GEcmd[10] = 0x01000000UL | ((u32)(void *)ScreenVertex & 0x00FFFFFF);
	// Draw Vertex
	GEcmd[11] = 0x04000000UL | (6 << 16) | 2;
	// List End
	GEcmd[12] = 0x0F000000UL;
	GEcmd[13] = 0x0C000000UL;
	GEcmd[14] = 0;
	GEcmd[15] = 0;


	sceKernelDcacheWritebackAll();
	qid = sceGeListEnQueue(&GEcmd[0], &GEcmd[14], gecbid, NULL);
	if (sync && qid >= 0) sceGeListSync(qid, 0);
}

void myBitBlt(u16 x,u16 y,u16 w,u16 h,u16 *src)
{
	u16* dd;
	u16* d= src;
	u16* vptr0 = getVramDrawBuffer();
	u16* vptr;
	int xx,yy;
	for (yy=0; yy<h; yy++) {
			vptr=vptr0+x+y*PSP_LINE_SIZE;
			dd=d;
			for (xx=0; xx<w; xx++) {
				*vptr=*dd;
				vptr++;
				dd++;
			}
			vptr0+=PSP_LINE_SIZE;
		d+=w;
	}
}

void myBitBltGe3_2(u16* src, bool sync)
{
	u16* dest = getVramDrawBuffer();
	static int qid = -1;
	ScreenVertex[0] = 0;
	ScreenVertex[1] = 0;
	ScreenVertex[2] = 36;
	ScreenVertex[3] = 0;
	ScreenVertex[4] = 0;
	ScreenVertex[5] = GBA_SCREEN_WIDTH;
	ScreenVertex[6] = GBA_SCREEN_HEIGHT;
	ScreenVertex[7] = 408+36;
	ScreenVertex[8] = PSP_SCREEN_HEIGHT;
	ScreenVertex[9] = 0;

	// Set Draw Buffer
	GEcmd[ 0] = 0x9C000000UL | ((u32)(unsigned char *)dest & 0x00FFFFFF);
	GEcmd[ 1] = 0x9D000000UL | (((u32)(unsigned char *)dest & 0xFF000000) >> 8) | 512;
	// Set Tex Buffer
	GEcmd[ 2] = 0xA0000000UL | ((u32)(unsigned char *)src & 0x00FFFFFF);
	GEcmd[ 3] = 0xA8000000UL | (((u32)(unsigned char *)src & 0xFF000000) >> 8) | GBA_SCREEN_WIDTH;
	// Tex size
	GEcmd[ 4] = 0xB8000000UL | (8 << 8) | 8;
	// Tex Flush
	GEcmd[ 5] = 0xCB000000UL;
	// Set Vertex
	GEcmd[ 6] = 0x12000000UL | (1 << 23) | (0 << 11) | (0 << 9) | (2 << 7) | (0 << 5) | (0 << 2) | 2;
	GEcmd[ 7] = 0x10000000UL;
	GEcmd[ 8] = 0x02000000UL;
	GEcmd[ 9] = 0x10000000UL | (((u32)(void *)ScreenVertex & 0xFF000000) >> 8);
	GEcmd[10] = 0x01000000UL | ((u32)(void *)ScreenVertex & 0x00FFFFFF);
	// Draw Vertex
	GEcmd[11] = 0x04000000UL | (6 << 16) | 2;
	// List End
	GEcmd[12] = 0x0F000000UL;
	GEcmd[13] = 0x0C000000UL;
	GEcmd[14] = 0;
	GEcmd[15] = 0;


	sceKernelDcacheWritebackAll();
	qid = sceGeListEnQueue(&GEcmd[0], &GEcmd[14], gecbid, NULL);
	if (sync && qid >= 0) sceGeListSync(qid, 0);
}

void myDrawFrame(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
	unsigned long i;

	u16* vptr0=getVramDrawBuffer();
	for(i=x1; i<=x2; i++){
		vptr0[i+y1*PSP_LINE_SIZE] = 0xffff;
		vptr0[i+y2*PSP_LINE_SIZE] = 0xffff;
	}
	for(i=y1; i<=y2; i++){
		vptr0[x1+i*PSP_LINE_SIZE] = 0xffff;
		vptr0[x2+i*PSP_LINE_SIZE] = 0xffff;
	}
}

void myDebug(bool reset,const char *str, ...)
{
	va_list ap;
	
	static int y=10;
	char szBuf[512];
	if (reset) y=10;
	va_start(ap, str);
	vsprintf(szBuf, str, ap);
	va_end(ap);
	
	myScreenSetXY(0,y++);
	if (y>30) {
		clear_screen(0);
		y=10;
	}		 
		 
	myScreenSetTextColor(0xFF00);
	myScreenPrintf(szBuf);
}

