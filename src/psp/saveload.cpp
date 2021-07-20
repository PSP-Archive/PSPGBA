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
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include "main.h"
#include "png.h"
#include "../System.h"
#include "../GBA.h"

extern struct EmulatedSystem emulator;

uLong sram_crc;

int read_png(char *path, u16 *out, size_t outlen)
{
	FILE *fp = fopen(path,"rb");
	if(!fp)
		return 0;

	u16 nSigSize = 8;
	u8 signature[nSigSize];
	if (sceIoRead(fileno(fp), signature, sizeof(u8)*nSigSize) != nSigSize){
		fclose(fp);
		return 0;
	}

	if (!png_check_sig( signature, nSigSize )){
		fclose(fp);
		return 0;
	}

	png_struct *pPngStruct = png_create_read_struct( PNG_LIBPNG_VER_STRING,
	NULL, NULL, NULL );
	if(!pPngStruct){
		fclose(fp);
		return 0;
	}

	png_info *pPngInfo = png_create_info_struct(pPngStruct);
	if(!pPngInfo){
		png_destroy_read_struct( &pPngStruct, NULL, NULL );
		fclose(fp);
		return 0;
	}

	if (setjmp( pPngStruct->jmpbuf )){
		png_destroy_read_struct( &pPngStruct, NULL, NULL );
		fclose(fp);
		return 0;
	}

	png_init_io( pPngStruct, fp );
	png_set_sig_bytes( pPngStruct, nSigSize );
	png_read_png( pPngStruct, pPngInfo,
	PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING |
	PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_BGR , NULL);

	png_uint_32 width = pPngInfo->width;
	png_uint_32 height = pPngInfo->height;
	int color_type = pPngInfo->color_type;

	if (outlen < width * height * sizeof(unsigned short)){
		png_destroy_read_struct( &pPngStruct, &pPngInfo, NULL );
		fclose(fp);
		return 0;
	}

	png_byte **pRowTable = pPngInfo->row_pointers;
	unsigned int x, y;
	u8 r, g, b;
	for (y=0; y<height; y++){
		png_byte *pRow = pRowTable[y];
		for (x=0; x<width; x++){
			switch(color_type){
				case PNG_COLOR_TYPE_GRAY:
				r = g = b = *pRow++;
				break;
				case PNG_COLOR_TYPE_GRAY_ALPHA:
				r = g = b = *pRow++;
				pRow++;
				break;
				case PNG_COLOR_TYPE_RGB:
				b = *pRow++;
				g = *pRow++;
				r = *pRow++;
				break;
				case PNG_COLOR_TYPE_RGB_ALPHA:
				b = *pRow++;
				g = *pRow++;
				r = *pRow++;
				pRow++;
				break;
				default:
				r = g = b = 0;
				break;
			}
			*out++ = RGB(r,g,b);
		}
	}

	png_destroy_read_struct( &pPngStruct, &pPngInfo, NULL );
	fclose(fp);

	return 1;
}


int remove_file(const char *fullpath)
{
	sceIoRemove(fullpath);
	return 0;

}

void save_config(const char* gbaPath)
{
	if (!memcmp(&setting, &tmpsetting, sizeof(SETTING)))
		return;
	
	char path[MAX_PATH];
	strcpy(path,gbaPath);
	
	char *p = strrchr(path, '/');
	strcpy(p+1, "GBA.CFG");

	int fd = sceIoOpen(path, PSP_O_WRONLY|PSP_O_TRUNC|PSP_O_CREAT, 644);
	if(fd<0)
		return;
	sceIoWrite(fd, &setting, sizeof(setting));
	sceIoClose(fd);

	tmpsetting = setting;
}

static void init_config()
{
	int i;

	strcpy(setting.vercnf, VERCNF);

	setting.screensize = SCR_FULL_16_9;
	setting.frameskip = 2;
	setting.vsync = 1;
	setting.sound = 0;
	setting.sound_buffer = 0;

	setting.color[0] = RGB(0xbb,0xbb,0xbb);
	setting.color[1] = RGB(0xff,0xdd,0xdd);
	setting.color[2] = RGB(0xff,0xee,0x00);
	setting.color[3] = RGB(0x88,0xff,0xcc);
	setting.bgbright=100;

	for(i=0; i<32; i++){
		setting.skeys[i].Buttons = 0;
		setting.skeys[i].n = -1;
	}
	for(i=0; i<=6; i++)
	setting.skeys[i].n = i;
	setting.skeys[0].Buttons = PSP_CTRL_CIRCLE;
	setting.skeys[1].Buttons = PSP_CTRL_CROSS;
	setting.skeys[2].Buttons = PSP_CTRL_TRIANGLE;
	setting.skeys[3].Buttons = PSP_CTRL_SQUARE;
	setting.skeys[4].Buttons = PSP_CTRL_SELECT;
	setting.skeys[5].Buttons = PSP_CTRL_START;
	setting.skeys[6].Buttons = PSP_CTRL_LTRIGGER;
	setting.skeys[7].Buttons = PSP_CTRL_RTRIGGER|PSP_CTRL_SELECT;
	setting.skeys[7].n = 11;
	setting.skeys[8].Buttons = PSP_CTRL_RTRIGGER|PSP_CTRL_START;
	setting.skeys[8].n = 12;

	setting.analog2dpad=1;
	setting.thumb = 1;
	setting.cpu_clock = 2;
	strcpy(setting.lastpath,"");
	for(i=0; i<16; i++)
	setting.bScreenSizes[i] = 1;
	for(i=0; i<32; i++)
	setting.bGB_Pals[i] = 1;
	setting.compress = 1;
	setting.quickslot = 0;
	

}


void load_config(const char* gbaPath)
{
	char *p;
	
	char path[MAX_PATH];
	strcpy(path,gbaPath);
	p = strrchr(path, '/')+1;
	strcpy(p, "GBA.CFG");

	int fd = sceIoOpen(path, PSP_O_RDONLY, 644);
	if(fd<0){
		init_config();
		return;
	}
	
	memset(&setting, 0, sizeof(setting));
	sceIoRead(fd, &setting, sizeof(setting));
	sceIoClose(fd);
	tmpsetting = setting;
	
}



char* get_state_path(int slot, char *out,const char* path)
{
	char *p, ext[]=".sv0";
	
	ext[3]=slot+'0';
	strcpy(out, path);
	p = strrchr(out, '.');
	strcpy(p, ext);
	return out;
}

void get_thumb_path(int slot, char *out,const char* path)
{
	char *p, ext[]=".tn0.png";
	
	ext[3]=slot+'0';
	strcpy(out, path);
	p = strrchr(out, '.');
	strcpy(p, ext);
}

int delete_thumb(int slot,const char* savePath)
{
	int ret;
	char path[MAX_PATH];
	
	get_thumb_path(slot,path,savePath);
	ret = remove_file(path);
	*strrchr(path, '.') = 0;
	ret = remove_file(path);
	
	return 1;
}

int delete_state(int slot,const char* savePath)
{
	int ret;
	char path[MAX_PATH];
	
	get_state_path(slot,path,savePath);
	ret = remove_file(path);
	strcat(path, ".gz");
	ret = remove_file(path);
	
	delete_thumb(slot,savePath);
	
	return ret;
}


int save_thumb(int slot,const char* savePath)
{
	char path[MAX_PATH];
	get_thumb_path(slot,path,savePath);

	emulator.emuWritePNG(path);
	return 1;
}

int load_thumb(int slot, unsigned short *out, size_t outlen, const char* savePath)
{
	char path[MAX_PATH];
	
	get_thumb_path(slot,path,savePath);
	return read_png(path, out, outlen);
}


int save_state(int slot,const char* savePath)
{
	char path[MAX_PATH];
	get_state_path(slot,path,savePath);

	if(emulator.emuWriteState)
		emulator.emuWriteState(path);

	if(!setting.thumb)
		delete_thumb(slot,savePath);
	else
		return save_thumb(slot,savePath);
	return 1;
}


int load_state(int slot,const char* savePath)
{
	char path[MAX_PATH];
	get_state_path(slot,path,savePath);

	if(emulator.emuReadState)
		emulator.emuReadState(path);

	return 1;
}

bool loadGame(char *fileName)
{
	char biosFileName[]="GBA.BIOS";

	for(int i = 0; i < 24;) {
		systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
		systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
		systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
		systemGbPalette[i++] = 0;
	}
	int size=CPULoadRom(fileName);

	if (size<=0)
		return false;
	
	
	cartridgeType = 0;
	CPUInit(biosFileName, false);
	CPUReset();
	for(int i = 0; i < 0x10000; i++) {
		systemColorMap16[i] = ((i & 0x1f)) |
		(((i & 0x3e0) >> 5) << 6) |
		(((i & 0x7c00) >> 10) << 11);  
	}

	return true;
}

