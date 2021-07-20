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
#include <stdarg.h>
#include "main.h"
#include "zlib.h"


SETTING setting, tmpsetting;
SceCtrlData paddata;
SceIoDirent *sortfiles[MAX_ENTRY];
SceIoDirent *zip_sortfiles[MAX_ENTRY];
SceIoDirent files[MAX_ENTRY];
SceIoDirent zip_files[MAX_ENTRY];
char GBAPath[MAX_PATH]="";
char RomName[MAX_NAME]="";
char RomPath[MAX_PATH]="";
char SaveFile[MAX_PATH]="";
char debugMsg[1024];
char filer_msg[256]={0};
char icon[128]="\x7F";
char imagePath[MAX_PATH];
char path_files[MAX_PATH]={0};
char path_inzip[MAX_PATH]={0};
int bBitmap=0;
int nfiles;
int zip_nfiles;
u16 bgBitmap[480*273];
u16 thumb_w[480*273];
u32 new_pad;
u32 now_pad;
u32 old_pad;

const char *scr_names[] = {
	"Full Screen (3:2)",
	"Full Screen (16:9)",
	"GBA orginal size (240x160)",
};

const char *cpu_clocks[] = {
	"222MHz",
	"266MHz",
	"333MHz",
};

enum {
	EXT_GB = 1,
	EXT_GZ = 2,
	EXT_ZIP = 4,
	EXT_TCH = 8,
	EXT_UNKNOWN = 16,
};

enum{
	STATE_SLOT_MAX=9,
};

// enum - Unzip Execute eXtract Return
enum {
	UZEXR_OK = 1,
	UZEXR_CANCEL = 0,
	// マイナス値はすべてエラーとすること
	UZEXR_INVALIDCALLBACK = -1,
	UZEXR_INVALIDFILE = -2,
	UZEXR_FATALERROR = -3
};

// enum - UnZip CallBack Return
enum {
	UZCBR_OK,
	UZCBR_PASS,
	UZCBR_CANCEL
};

// enum UnZip CallBack id
enum {
	UZCB_FIND_FILE,
	UZCB_EXTRACT_PROGRESS
};


	enum
	{
		LOAD_ROM,
		STATE_SAVE,
		STATE_LOAD,
		SCREEN_SIZE,
		SOUND,
		VSYNC,
		MAX_FRAME_SKIP,
		CPU_CLOCK,
		RESET,
		CONTINUE
	};


extern "C" void Unzip_setCallback(int (*pfuncCallback)(int nCallbackId, unsigned long ulExtractSize,
		      unsigned long ulCurrentPosition, const void *pData,
                      unsigned long ulDataSize, unsigned long ulUserData));

extern "C" int Unzip_execExtract(const char *pszTargetFile, unsigned long ulUserData);




const struct {
	char *szExt;
	int nExtId;
} stExtentions[] = {
	{"gba",EXT_GB},
	{"gb",EXT_GB},
	{"gbc",EXT_GB},
	{"sgb",EXT_GB},
	{"gz",EXT_GZ},
	{"zip",EXT_ZIP},
	{"tch",EXT_TCH},
	{NULL, EXT_UNKNOWN}
};


typedef struct {
	u8 *p_rom_image;			// pointer to rom image
	long rom_size;				// rom size
	char szFileName[MAX_PATH];	// extracted file name
}ROM_INFO, *LPROM_INFO;

static int getExtId(const char *szFilePath) {
	char *pszExt;

	if ((pszExt = strrchr(szFilePath, '.'))) {
		pszExt++;
		int i;
		for (i = 0; stExtentions[i].nExtId != EXT_UNKNOWN; i++) {
			if (!stricmp(stExtentions[i].szExt,pszExt)) {
				return stExtentions[i].nExtId;
			}
		}
	}

	return EXT_UNKNOWN;
}


static void SJISCopy(SceIoDirent *a, unsigned char *file)
{
	unsigned char ca;
	int i;
	int len=strlen(a->d_name);
	
	for(i=0;i<=len;i++){
		ca = a->d_name[i];
		if (((0x81 <= ca)&&(ca <= 0x9f))
		|| ((0xe0 <= ca)&&(ca <= 0xef))){
			file[i++] = ca;
			file[i] = a->d_name[i];
		}
		else{
			if(ca>='a' && ca<='z') ca-=0x20;
			file[i] = ca;
		}
	}

}

static int cmpFile(SceIoDirent *a, SceIoDirent *b)
{
    unsigned char file1[0x108];
    unsigned char file2[0x108];
	unsigned char ca, cb;
	int i, n, ret;

	if((a->d_stat.st_mode & FIO_S_IFMT)==(b->d_stat.st_mode & FIO_S_IFMT)){
		SJISCopy(a, file1);
		SJISCopy(b, file2);
		n=strlen((char*)file1);
		for(i=0; i<=n; i++){
			ca=file1[i]; cb=file2[i];
			ret = ca-cb;
			if(ret!=0) return ret;
		}
		return 0;
	}
	
	if(FIO_S_ISDIR(a->d_stat.st_mode))	return -1;
	else					return 1;
}
// AC add end

static void sort_files(SceIoDirent **a, int left, int right) {
	SceIoDirent *tmp, *pivot;
	int i, p;
	
	if (left < right) {
		pivot = a[left];
		p = left;
		for (i=left+1; i<=right; i++) {
			if (cmpFile(a[i],pivot)<0){
				p=p+1;
				tmp=a[p];
				a[p]=a[i];
				a[i]=tmp;
			}
		}
		a[left] = a[p];
		a[p] = pivot;
		sort_files(a, left, p-1);
		sort_files(a, p+1, right);
	}
}
////////////////////////////////////////////////////////////////////////

static void getDir(const char *path, u32 ext) {
	int fd, b=0;
	
	nfiles = 0;
	
	if(strcmp(path,"ms0:/")){
		strcpy(files[0].d_name,"..");
		files[0].d_stat.st_mode = FIO_S_IFDIR;
		sortfiles[0] = files;
		nfiles = 1;
		b=1;
	}
	
	strcpy(path_files, path);
	fd = sceIoDopen(path);
	while(nfiles<MAX_ENTRY){
		memset(&files[nfiles], 0x00, sizeof(SceIoDirent));
		if(sceIoDread(fd, &files[nfiles])<=0) 
			break;
		
		if(files[nfiles].d_name[0] == '.') 
			continue;
			
			
		if(FIO_S_ISDIR(files[nfiles].d_stat.st_mode))
		{
			strcat(files[nfiles].d_name, "/");
			sortfiles[nfiles] = files + nfiles;
			nfiles++;
		} else {
			if(getExtId(files[nfiles].d_name) & ext){
				sortfiles[nfiles] = files + nfiles;
				nfiles++;
			} 		
		}
	}
	sceIoDclose(fd);
	if(b)
		sort_files(sortfiles+1, 0, nfiles-2);
	else
		sort_files(sortfiles, 0, nfiles-1);
}

static void refreshScreen()
{
	myScreenFlip();
	sceDisplayWaitVblankStart();
}


static int getZipDirCallback(int nCallbackId, unsigned long ulExtractSize, unsigned long ulCurrentPosition,
                      const void *pData, unsigned long ulDataSize, unsigned long ulUserData)
{
	const char *pszFileName = (const char *)pData;
	
	switch(nCallbackId) {
	case UZCB_FIND_FILE:
		if(getExtId(pszFileName)==EXT_GB){
			strcpy(zip_files[zip_nfiles].d_name, pszFileName);
			zip_sortfiles[zip_nfiles] = zip_files + zip_nfiles;
			zip_nfiles++;
		}
		if(zip_nfiles >= MAX_ENTRY) return UZCBR_CANCEL;
		break;
	default: // unknown...
		//pgFillvram(RGB(255,0,0));
		myPrintfXY(0,0,0xFFFF,"Unzip fatal error.");
		refreshScreen();
        break;
    }
	return UZCBR_PASS;
}

static int getZipDirAll(const char *path)
{
	ROM_INFO stRomInfo;
	
	zip_nfiles = 0;
	path_files[0] = 0;
	path_inzip[0] = 0;

	Unzip_setCallback(getZipDirCallback);
	int ret = Unzip_execExtract(path, (unsigned long)&stRomInfo);
	if (ret != UZEXR_OK)
		zip_nfiles=0;

	sort_files(zip_sortfiles, 0, zip_nfiles-1);
	
	return zip_nfiles;
}

static void getZipDir(const char *path)
{
	char *p;
	int i, len;
	
	strcpy(files[0].d_name,"..");
	files[0].d_stat.st_mode = FIO_S_IFDIR;
	sortfiles[0] = files;
	nfiles = 1;
	
	len = strlen(path);
	for(i=0; i<zip_nfiles; i++){
		if(strncmp(zip_sortfiles[i]->d_name,path,len)) continue;
		strcpy(files[nfiles].d_name,zip_sortfiles[i]->d_name + len);
		p = strchr(files[nfiles].d_name, '/');
		if(p){
			*(p+1) = 0;
			if(!strcmp(files[nfiles].d_name,files[nfiles-1].d_name)) continue;
			files[nfiles].d_stat.st_mode = FIO_S_IFDIR;
		}else{
			files[nfiles].d_stat.st_mode = FIO_S_IFREG;
		}
		sortfiles[nfiles] = files + nfiles;
		nfiles++;
	}
	sort_files(sortfiles+1, 0, nfiles-2);
}

static void readpad(void)
{
	static int n=0;
	SceCtrlData paddata;

	sceCtrlReadBufferPositive(&paddata, 1);

	if (paddata.Ly>200) paddata.Buttons=PSP_CTRL_DOWN;  // DOWN
	if (paddata.Ly<100) paddata.Buttons=PSP_CTRL_UP;    // UP
	if (paddata.Lx<100) paddata.Buttons=PSP_CTRL_LEFT;  // LEFT
	if (paddata.Lx>200) paddata.Buttons=PSP_CTRL_RIGHT; // RIGHT

	now_pad = paddata.Buttons;
	new_pad = now_pad & ~old_pad;
	if(old_pad==now_pad){
		n++;
		if(n>=25){
			new_pad=now_pad;
			n = 20;
		}
	}else{
		n=0;
		old_pad = now_pad;
	}
}

static void myFrame(const char *msg0, const char *msg1)
{
	char tmp[1024];	

	if(bBitmap)
		myBitBlt(0,0,480,272,bgBitmap);

	if(msg0) 
		myPrintfXY(1,2,setting.color[0], msg0 );
	if(msg1) 
		myPrintfXY(1,32,setting.color[0], msg1);

	myDrawFrame(17,25,463,248,setting.color[1]);
	if(scePowerIsBatteryExist()){
		sprintf(tmp,"[");
		pspTime  t;
		sceRtcGetCurrentClockLocalTime(&t);
		sprintf(&tmp[strlen(tmp)-1]," %02d/%02d %02d:%02d:%02d",t.month,t.day,t.hour,t.minutes,t.seconds);
				
		strcat(tmp," [");
		int p=scePowerGetBatteryLifePercent();
		for (int i=3;i>=0;i--) {
			if (p/25>i)
				strcat(tmp,"<");
			else
				strcat(tmp," ");
		}
		strcat(tmp,"]");
		myPrintfXY(45,0, setting.color[3],tmp);
		
		sprintf(tmp, "%s (by psp298)",VERCNF);
		myPrintfXY(0,0, setting.color[0],tmp);
		
	}
}



static int myMessageBox(const char *msg, int type){
	

	for(;;){
		readpad();
		if(new_pad & PSP_CTRL_CIRCLE){
			return 1;
		}else if(new_pad & PSP_CTRL_CROSS && type){
			return 0;
		}

		if(type)
		myFrame(0,"O - OK    X - Cancel");
			else
		myFrame(0,"O - OK");
		myPrintfXY(0,1,setting.color[2],msg);

		refreshScreen();
	}
}

static int cmp_skey(S_BUTTON *a, S_BUTTON *b)
{
	int i, na=0, nb=0;

	for(i=0; i<32; i++){
		if ((a->Buttons >> i) & 1) na++;
		if ((b->Buttons >> i) & 1) nb++;
	}
	return nb-na;
}

static int getFilePath(char *fullpath, u32 ext)
{
	int sel=0, top=0, rows=21, x, y, h, i, up=0, inzip=0, oldDirType;
	char path[MAX_PATH], oldDir[MAX_NAME], tmp[MAX_PATH], *p;
	
	path_inzip[0] = 0;
	
	strcpy(path, fullpath);
	p = strrchr(path, '/');
	if (p){
		p++;
		strcpy(tmp, p);
		*p = 0;
	}
	else{
		strcpy(path,"ms0:/");
	}

	getDir(path, ext);

	if (tmp[0]){
		for(i=0; i<nfiles; i++){
			if (!stricmp(sortfiles[i]->d_name,tmp)){
				sel = i;
				top = i-3;
				break;
			}
		}
	}
	
	for(;;){
		readpad();
		if(new_pad)
			filer_msg[0]=0;
		if(new_pad & PSP_CTRL_CIRCLE){
			if(FIO_S_ISDIR(sortfiles[sel]->d_stat.st_mode)){
				if(!strncmp(sortfiles[sel]->d_name,"..",2)){
					up=1;
				}else{
					if(inzip){
						strcat(path_inzip,sortfiles[sel]->d_name);
						getZipDir(path_inzip);
					}else{
						strcat(path,sortfiles[sel]->d_name);
						getDir(path, ext);
					}
					sel=0;
				}
			}else{
				if(!inzip){
					strcpy(tmp,path);
					strcat(tmp,sortfiles[sel]->d_name);
					if (getExtId(tmp)==EXT_ZIP){
						getZipDirAll(tmp);
						if(zip_nfiles!=1){
							strcat(path,sortfiles[sel]->d_name);
							getZipDir(path_inzip);
							sel=0;
							inzip=1;
						}else
							break;
					}else
						break;
				}else
					break;
			}
		}else if(new_pad & PSP_CTRL_CROSS){
			return 0;
		}else if(new_pad & PSP_CTRL_SELECT){
			if(!inzip && FIO_S_ISREG(sortfiles[sel]->d_stat.st_mode)){
				strcpy(tmp,"\"");
				strcat(tmp,sortfiles[sel]->d_name);
				strcat(tmp,"\"\nRemove?");
				if(myMessageBox(tmp,1)){
					strcpy(tmp, path);
					strcat(tmp, sortfiles[sel]->d_name);
					if(sceIoRemove(tmp)>=0){
						strcpy(filer_msg,"Removed \"");
						strcat(filer_msg,sortfiles[sel]->d_name);
						strcat(filer_msg,"\"");
						getDir(path, ext);
					}
				}
			}
		}else if(new_pad & PSP_CTRL_TRIANGLE){
			up=1;
		}else if(new_pad & PSP_CTRL_UP){
			sel--;
		}else if(new_pad & PSP_CTRL_DOWN){
			sel++;
		}else if(new_pad & PSP_CTRL_LEFT){
			sel-=rows/2;
		}else if(new_pad & PSP_CTRL_RIGHT){
			sel+=rows/2;
		}
		
		if(up){
			oldDir[0]=0;
			oldDirType = FIO_S_IFDIR;
			if(inzip){
				if(path_inzip[0]==0){
					oldDirType = FIO_S_IFREG;
					inzip=0;
				}else{
					path_inzip[strlen(path_inzip)-1]=0;
					p = strrchr(path_inzip,'/');
					if (p)
						p++;
					else
						p = path_inzip;
					sprintf(oldDir,"%s/", p);
					*p = 0;
					getZipDir(path_inzip);
					sel=0;
				}
			}
			if(strcmp(path,"ms0:/") && !inzip){
				if(FIO_S_ISDIR(oldDirType))
					path[strlen(path)-1]=0;
				p=strrchr(path,'/')+1;
				strcpy(oldDir,p);
				if(FIO_S_ISDIR(oldDirType))
					strcat(oldDir,"/");
				*p=0;
				getDir(path, ext);
				sel=0;
			}
			for(i=0; i<nfiles; i++) {
				if(
				(((sortfiles[i]->d_stat.st_mode) & FIO_S_IFMT) == oldDirType)
				&& !strcmp(oldDir, sortfiles[i]->d_name)) {
					sel=i;
					top=sel-3;
					break;
				}
			}
			up=0;
		}
		
		if(top > nfiles-rows)	top=nfiles-rows;
		if(top < 0)				top=0;
		if(sel >= nfiles)		sel=nfiles-1;
		if(sel < 0)				sel=0;
		if(sel >= top+rows)		top=sel-rows+1;
		if(sel < top)			top=sel;
		
		if(inzip){
			sprintf(tmp,"%s:/%s",strrchr(path,'/')+1,path_inzip);
			myFrame(tmp,"O - OK     X - Cancel");
		}else
			myFrame(filer_msg[0]?filer_msg:"","O - OK      X - Cancel     SELECT - Remove");
		
		// スクロールバー
		if(nfiles > rows){
			h = 219;
			//pgDrawFrame(445,25,446,248,setting.color[1]);
			//pgFillBox(448, h*top/nfiles + 27, 460, h*(top+rows)/nfiles + 27,setting.color[1]);
		}
		
		x=4; y=5;
		myPrintfXY(x++,y++,setting.color[3],path);
		for(i=0; i<rows; i++){
			if(top+i >= nfiles) break;
			char temp[1024];
			sprintf(temp,"%s",sortfiles[top+i]->d_name);
			myPrintfXY(x, y, setting.color[top+i==sel?2:3],temp);
			y+=1;
		}

		//myPrintfXY(x-1,sel + 6,setting.color[3],icon);

		refreshScreen();
	}
	
	strcpy(fullpath, path);
	strcat(inzip?path_inzip:fullpath, sortfiles[sel]->d_name);
	return 1;
}

void set_cpu_clock(int n)
{
	if(n==0)
		scePowerSetClockFrequency(222,222,111);
	else if(n==1)
		scePowerSetClockFrequency(266,266,133);
	else if(n==2)
		scePowerSetClockFrequency(333,333,166);
}




static int get_nShortcutKey(u32 buttons)
{
	int i;
	for(i=6; i<32; i++){
		if (setting.skeys[i].Buttons==0)
		return -1;
		if ((buttons & setting.skeys[i].Buttons)==setting.skeys[i].Buttons)
		return setting.skeys[i].n;
	}
	return -1;
}


static int load_menu_bg(const char* gbaPath)
{
	char path[MAX_PATH]="";

	strcpy(path,gbaPath);
	
	char *p;
	
	p = strrchr(path, '/');
	strcpy(p+1, "PSPGBA.PNG");
	
	strcpy(imagePath,path);

	if(read_png(path,bgBitmap,sizeof(bgBitmap)))
		return 1;
	return 0;

}


static void sort_skeys(S_BUTTON *a, int left, int right) {
	S_BUTTON tmp, pivot;
	int i, p;

	if (left < right) {
		pivot = a[left];
		p = left;
		for (i=left+1; i<=right; i++) {
			if (cmp_skey(&a[i],&pivot)<0){
				p=p+1;
				tmp=a[p];
				a[p]=a[i];
				a[i]=tmp;
			}
		}
		a[left] = a[p];
		a[p] = pivot;
		sort_skeys(a, left, p-1);
		sort_skeys(a, p+1, right);
	}
}

static void keyconfig(void)
{
	enum
	{
		CONFIG_A,
		CONFIG_B,
		CONFIG_RAPIDA,
		CONFIG_RAPIDB,
		CONFIG_SELECT,
		CONFIG_START,
		CONFIG_MENU,
		CONFIG_WAIT,
		CONFIG_VSYNC,
		CONFIG_SOUND,
		CONFIG_SCREENSIZE,
		CONFIG_QUICKSAVE,
		CONFIG_QUICKLOAD,
		CONFIG_STATE_SLOT,
		CONFIG_GB_COLOR,
		CONFIG_CPU_CLOCK,
		CONFIG_ANALOG2DPAD,
		CONFIG_EXIT,
	};
	char msg[256];
	int key_config[32];
	int sel=0, x, y, i, bPad=0, crs_count=0;

	for(i=0; i<32; i++)
	key_config[i] = 0;
	for(i=0; i<32; i++){
		if(setting.skeys[i].n >= 0)
		key_config[setting.skeys[i].n] = setting.skeys[i].Buttons;
	}

	for(;;){
		readpad();
		if(now_pad & (PSP_CTRL_UP|PSP_CTRL_DOWN|PSP_CTRL_LEFT|PSP_CTRL_RIGHT))
		crs_count=0;
		if(now_pad==PSP_CTRL_LEFT || now_pad==PSP_CTRL_RIGHT){
			if(sel!=CONFIG_EXIT && sel!=CONFIG_MENU && sel!=CONFIG_ANALOG2DPAD)
			key_config[sel] = 0;
		}else if(now_pad==PSP_CTRL_UP){
			if(bPad==0){
				if(sel!=0)	sel--;
				else		sel=CONFIG_EXIT;
				bPad++;
			}else if(bPad >= 25){
				if(sel!=0)	sel--;
				else		sel=CONFIG_EXIT;
				bPad=20;
			}else
			bPad++;
		}else if(now_pad==PSP_CTRL_DOWN){
			if(bPad==0){
				if(sel!=CONFIG_EXIT)sel++;
				else				sel=0;
				bPad++;
			}else if(bPad >= 25){
				if(sel!=CONFIG_EXIT)sel++;
				else				sel=0;
				bPad=20;
			}else
				bPad++;
			}else if(new_pad != 0){
				if(sel==CONFIG_EXIT && new_pad&PSP_CTRL_CIRCLE)
				break;
				else if(sel==CONFIG_ANALOG2DPAD && new_pad&PSP_CTRL_CIRCLE)
				setting.analog2dpad = !setting.analog2dpad;
				else
				key_config[sel] = now_pad;
			}else{
				bPad=0;
			}
		
			if (crs_count++>=30) crs_count=0;
		
			if(sel>=CONFIG_ANALOG2DPAD)
			strcpy(msg,"O - OK");
			else
			strcpy(msg,"←→：Clear");
		
			myFrame(0, msg);
		
			x=2; y=5;
			myPrintfXY(x,y++,setting.color[3],"  A BUTTON       :");
			myPrintfXY(x,y++,setting.color[3],"  B BUTTON       :");
			myPrintfXY(x,y++,setting.color[3],"  A BUTTON(RAPID):");
			myPrintfXY(x,y++,setting.color[3],"  B BUTTON(RAPID):");
			myPrintfXY(x,y++,setting.color[3],"  SELECT BUTTON  :");
			myPrintfXY(x,y++,setting.color[3],"  START BUTTON   :");
			myPrintfXY(x,y++,setting.color[3],"  MENU BUTTON    :");
			myPrintfXY(x,y++,setting.color[3],"  TURBO ON/OFF   :");
			myPrintfXY(x,y++,setting.color[3],"  VSYNC ON/OFF   :");
			myPrintfXY(x,y++,setting.color[3],"  SOUND ON/OFF   :");
			myPrintfXY(x,y++,setting.color[3],"  SCREEN SIZE    :");
			myPrintfXY(x,y++,setting.color[3],"  QUICK SAVE     :");
			myPrintfXY(x,y++,setting.color[3],"  QUICK LOAD     :");
			myPrintfXY(x,y++,setting.color[3],"  QUICK SLOT     :");
			myPrintfXY(x,y++,setting.color[3],"  GB PALETTE     :");
			myPrintfXY(x,y++,setting.color[3],"  CPU CLOCK      :");
			y++;
			if(setting.analog2dpad)
			myPrintfXY(x,y++,setting.color[3],"  AnalogPad to D-Pad: ON");
			else
			myPrintfXY(x,y++,setting.color[3],"  AnalogPad to D-Pad: OFF");
			y++;
			myPrintfXY(x,y++,setting.color[3],"  Return to Main Menu");
		
			for (i=0; i<CONFIG_ANALOG2DPAD; i++){
				y = i + 5;
				int j = 0;
				msg[0]=0;
				if(key_config[i] == 0){
					strcpy(msg,"UNDEFINED");
				}else{
				if (key_config[i] & PSP_CTRL_LTRIGGER){
					msg[j++]='L'; msg[j++]='+'; msg[j]=0;
				}
				if (key_config[i] & PSP_CTRL_RTRIGGER){
					msg[j++]='R'; msg[j++]='+'; msg[j]=0;
				}
				if (key_config[i] & PSP_CTRL_CIRCLE){
					msg[j++]=1; msg[j++]='+'; msg[j]=0;
				}
				if (key_config[i] & PSP_CTRL_CROSS){
					msg[j++]=2; msg[j++]='+'; msg[j]=0;
				}
				if (key_config[i] & PSP_CTRL_SQUARE){
					msg[j++]=3; msg[j++]='+'; msg[j]=0;
				}
				if (key_config[i] & PSP_CTRL_TRIANGLE){
					msg[j++]=4; msg[j++]='+'; msg[j]=0;
				}
				if (key_config[i] & PSP_CTRL_START){
					strcat(msg,"START+"); j+=6;
				}
				if (key_config[i] & PSP_CTRL_SELECT){
					strcat(msg,"SELECT+"); j+=7;
				}
				if (key_config[i] & PSP_CTRL_UP){
					msg[j++]=5; msg[j++]='+'; msg[j]=0;
				}
				if (key_config[i] & PSP_CTRL_RIGHT){
					msg[j++]=6; msg[j++]='+'; msg[j]=0;
				}
				if (key_config[i] & PSP_CTRL_DOWN){
					msg[j++]=7; msg[j++]='+'; msg[j]=0;
				}
				if (key_config[i] & PSP_CTRL_LEFT){
					msg[j++]=8; msg[j++]='+'; msg[j]=0;
				}
				msg[strlen(msg)-1]=0;
			}
			myPrintfXY(21,y,setting.color[3],msg);
		}
		
		if (crs_count < 15){
			x = 2;
			y = sel + 5;
			if(sel >= CONFIG_ANALOG2DPAD) y++;
			if(sel >= CONFIG_EXIT)        y++;
			myPrintfXY((x+1)*8,y*8,setting.color[3],icon);
		}
	
	}
	
	for(i=0; i<32; i++){
		if (i!=6 && key_config[i] == key_config[6])
			key_config[i] = 0;
		if(key_config[i]){
			setting.skeys[i].Buttons = key_config[i];
			setting.skeys[i].n = i;
		}else{
			setting.skeys[i].Buttons = 0;
			setting.skeys[i].n = -1;
		}
	}
	sort_skeys(&setting.skeys[6],0,25);
}


static int screensize(int n)
{
	int x,y,i,sel=n;

	for(;;){
		readpad();
		if(new_pad & PSP_CTRL_CIRCLE)
			return sel;
		else if(new_pad & PSP_CTRL_CROSS)
			return -1;
		else if(new_pad & PSP_CTRL_SELECT){
			setting.bScreenSizes[sel] = !setting.bScreenSizes[sel];
		for(i=0; i<SCR_END; i++)
			if(setting.bScreenSizes[i]) break;
		if(i>=SCR_END)
			setting.bScreenSizes[sel] = 1;
		}else if(new_pad & PSP_CTRL_DOWN){
			sel++;
			if(sel>=SCR_END) sel=0;
		}else if(new_pad & PSP_CTRL_UP){
			sel--;
			if(sel<0) sel=SCR_END-1;
		}else if(new_pad & PSP_CTRL_RIGHT){
			sel+=SCR_END/2;
			if(sel>=SCR_END) sel=SCR_END-1;
		}else if(new_pad & PSP_CTRL_LEFT){
			sel-=SCR_END/2;
			if(sel<0) sel=0;
		}

		if(setting.bScreenSizes[sel])
			myFrame("Select Screen Size", "O - OK     X - Cancel     SELECT - Disable");
		else
			myFrame("Select Screen Size", "O - OK     X - Cancel     SELECT - Enable");

		x=4, y=5;
		myPrintfXY(x++,y++,setting.color[3],"SCREEN SIZE:");
		for(i=0; i<SCR_END; i++){
			myPrintfXY(x,y++,setting.color[i==sel?2:3],scr_names[i]);
		}
		//myPrintfXY(x-2,sel+6,setting.color[3],icon);

		refreshScreen();
	}
}


static int myFrameskip(int sel)
{
	char tmp[8];
	int x,y,i;

	strcpy(tmp,"0");

	for(;;){
		readpad();
		if(new_pad & PSP_CTRL_CIRCLE)
		return sel;
		else if(new_pad & PSP_CTRL_CROSS)
		return -1;
		else if(new_pad & PSP_CTRL_DOWN){
			sel++;
			if(sel>9) sel=0;
		}else if(new_pad & PSP_CTRL_UP){
			sel--;
			if(sel<0) sel=9;
		}else if(new_pad & PSP_CTRL_RIGHT){
			sel+=5;
			if(sel>9) sel=9;
		}else if(new_pad & PSP_CTRL_LEFT){
			sel-=5;
			if(sel<0) sel=0;
		}

		myFrame("Select Max Frame Skip", "O - OK     X - Cancel");

		x=4, y=5;
		myPrintfXY(x++,y++,setting.color[3],"MAX FRAME SKIP:");
		for(i=0; i<=9; i++){
			tmp[0] = i + '0';
			myPrintfXY(x,y++,setting.color[i==sel?2:3],tmp);
		}
		//myPrintfXY(x-2,sel+6,setting.color[3],icon);

		refreshScreen();
	}
}


static int cpuclock(int sel)
{
	int x,y,i;

	for(;;){
		readpad();
		if(new_pad & PSP_CTRL_CIRCLE)
		return sel;
		else if(new_pad & PSP_CTRL_CROSS)
		return -1;
		else if(new_pad & PSP_CTRL_DOWN){
			sel++;
			if(sel>2) sel=0;
		}else if(new_pad & PSP_CTRL_UP){
			sel--;
			if(sel<0) sel=2;
		}

		myFrame("Select CPU Clock", "O - OK     X - Cancel");

		x=4, y=5;
		myPrintfXY(x++,y++,setting.color[3],"CPU CLOCK:");
		for(i=0; i<3; i++)
			myPrintfXY(x,y++,setting.color[i==sel?2:3],cpu_clocks[i]);

		//myPrintfXY(x-2,sel+6,setting.color[3],icon);

		refreshScreen();
	}
}

static void findState(int nState[], int nThumb[])
{
	unsigned int i, j;
	unsigned int nfiles = 0;
	char tmp[MAX_PATH];
	char *p;
	
	strcpy(tmp,SaveFile);	
	*(strrchr(tmp,'/')+1) = 0;
		
	int fd = sceIoDopen(tmp);
	while(nfiles<MAX_ENTRY){
		memset(&files[nfiles], 0x00, sizeof(SceIoDirent));
		if(sceIoDread(fd, &files[nfiles])<=0) 
			break;
		nfiles++;
	}
	sceIoDclose(fd);

	myPrintfXY(1,28,setting.color[3], "nfle %i,",nfiles);

	for(i=0; i<=STATE_SLOT_MAX; i++){
		get_state_path(i,tmp,SaveFile);
		nState[i]=-1;
		for(j=0; j<nfiles; j++){
			
			myPrintfXY(1,20+j,setting.color[3], files[j].d_name);
			p=strrchr(tmp,'/')+1;
			if(!stricmp(p,files[j].d_name)){
				nState[i] = j;
				break;
			}
		}

		get_thumb_path(i,tmp,SaveFile);
		nThumb[i]=-1;
		for(j=0; j<nfiles; j++){
			p=strrchr(tmp,'/')+1;
			if(!stricmp(p,files[j].d_name)){
				nThumb[i] = j;
				break;
			}
		}
	}
}


static int stateslot(int type)
{
	const int MAX_ITEM = STATE_SLOT_MAX;
	char msg[256], *p;
	static int ex_sel=0;
	int nState[STATE_SLOT_MAX+1], nThumb[STATE_SLOT_MAX+1];
	unsigned int x, y;
	int i, ret=0;
	int sel=ex_sel, sel_bak=9999;

	findState(nState, nThumb);

	for(;;){
		readpad();
		if(new_pad & PSP_CTRL_CIRCLE){
			if (type != STATE_LOAD)
			{
				break;
			}else{
				if (nState[sel]>=0)
					break;
			}
		}else if(new_pad & PSP_CTRL_CROSS){
			return -1;
		}else if((new_pad & PSP_CTRL_SELECT)){
			if (nState[sel]>=0){
				if (delete_state(sel,SaveFile)>=0)
					nState[sel] = nThumb[sel] = -1;
			}
		}else if(new_pad & PSP_CTRL_DOWN){
			sel++;
			if(sel>=MAX_ITEM) sel=0;
		}else if(new_pad & PSP_CTRL_UP){
			sel--;
			if(sel<0) sel=MAX_ITEM-1;
		}else if(new_pad & PSP_CTRL_RIGHT){
			sel+=(MAX_ITEM+1)/2;
			if(sel>=MAX_ITEM) sel=MAX_ITEM-1;
		}else if(new_pad & PSP_CTRL_LEFT){
			sel-=(MAX_ITEM+1)/2;
			if(sel<0) sel=0;
		}
		
		if (sel!=sel_bak){
			sel_bak = sel;
			if (nState[sel]>=0 && nThumb[sel]>=0){
				p = strrchr(files[nThumb[sel]].d_name, '.');
				if(!stricmp(p, ".png"))
					ret = load_thumb(sel,thumb_w,sizeof(thumb_w),SaveFile);
				if(!ret)
					nThumb[sel] = -1;
			}
		}
	
		switch(type)
		{
			case STATE_LOAD:
				p = "Select State Load Slot";
				break;
			case STATE_SAVE:
				p = "Select State Save Slot";
				break;
			default:
				p = NULL;
		}
		myFrame(p,"O - OK     X - Cancel     SELECT - Remove");
	
		if (sel<STATE_SLOT_MAX && nState[sel]>=0 && nThumb[sel]>=0){
			myBitBlt(208,35,240,160,thumb_w);
			//myDrawFrame(200,30,433,195,setting.color[1]);
			//myDrawFrame(201,31,432,194,setting.color[1]);
		}
	
		switch(type)
		{
			case STATE_LOAD:
			p = "STATE LOAD:";
			break;
			case STATE_SAVE:
			p = "STATE SAVE:";
			break;
			break;
		}
		x=4, y=5;
		myPrintfXY(x++,y++,setting.color[3],p);
		
		for(i=0; i<STATE_SLOT_MAX; i++){
			if(nState[i] < 0){
				sprintf(msg,"%d - None", i);
			}else{
				sprintf(msg, "%d - %04d/%02d/%02d %02d:%02d:%02d", i,
				files[nState[i]].d_stat.st_mtime.year,
				files[nState[i]].d_stat.st_mtime.month,
				files[nState[i]].d_stat.st_mtime.day,
				files[nState[i]].d_stat.st_mtime.hour,
				files[nState[i]].d_stat.st_mtime.minute,
				files[nState[i]].d_stat.st_mtime.second);
			}
			myPrintfXY(x,y++,setting.color[i==sel?2:3],msg);
		}

		myPrintfXY(x-2,sel+6,setting.color[3],icon);
		
		refreshScreen();
	}
	
	ex_sel = sel;
	return sel;
}

static void runGame(bool reset)
{
	refreshScreen();
	save_config(GBAPath);
	myScreenInit(PSP_DISPLAY_PIXEL_FORMAT_565,GU_PSM_5650);
	set_cpu_clock(setting.cpu_clock);
	startGame(setting.frameskip,setting.vsync,setting.sound,setting.screensize,reset);
	set_cpu_clock(0);
	myScreenInit(PSP_DISPLAY_PIXEL_FORMAT_565,GU_PSM_5650);
}

	
void menu()
{


	char path[MAX_PATH]="";

	
	load_config(GBAPath);
	
	char tmp[MAX_PATH], msg[256]={0};
	static int sel=0;
	int x=0, y=0, crs_count=0, bLoop=1;
	int ret;
	
	bBitmap=load_menu_bg(GBAPath);

	if (strlen(setting.lastpath)>0);
		strcpy(RomPath,setting.lastpath);
	
	for(;;){
		if (!getFilePath(RomPath,EXT_GB|EXT_GZ|EXT_ZIP))
			return;


		if (loadGame(RomPath)) {
			strcpy(tmp, RomPath);
			*(strrchr(tmp,'/')+1) = 0;
			strcpy(setting.lastpath, tmp);
	
			strcpy(RomName, strrchr(RomPath, '/')+1);
			*(strrchr(RomName,'.')) = 0;
			
			sprintf(SaveFile, "%sSAVE/%s.sav", GBAPath, RomName);
			runGame(true);
		} else {
			continue;
		}

		break;
	}

	readpad();
	old_pad = paddata.Buttons;

	for(;;){
		readpad();
		if(now_pad & (PSP_CTRL_UP|PSP_CTRL_DOWN|PSP_CTRL_LEFT|PSP_CTRL_RIGHT))
			crs_count=0;
		if(new_pad)
			msg[0]=0;
		if(new_pad & PSP_CTRL_CIRCLE){
			switch(sel)
			{
				case LOAD_ROM:
					for (;;){
						if (!getFilePath(RomPath,EXT_GB|EXT_GZ|EXT_ZIP)){
							if (bLoop)
								break;
							else
								continue;
						}
						if (loadGame(RomPath)) {
							strcpy(tmp, RomPath);
							*(strrchr(tmp,'/')+1) = 0;
							strcpy(setting.lastpath, tmp);
					
							strcpy(RomName, strrchr(RomPath, '/')+1);
							*(strrchr(RomName,'.')) = 0;
							
							sprintf(SaveFile, "%sSAVE/%s.sav", GBAPath, RomName);
							runGame(true);
						} else {
							continue;
						}
						break;
					}
					crs_count=0;
					break;
				case STATE_SAVE:
					ret = stateslot(STATE_SAVE);
					if(ret>=0){
						strcpy(msg, "State Save Failed");
						get_state_path(ret,path,SaveFile);

						if(save_state(ret,SaveFile))
							strcpy(msg, "State Saved Successfully");
							
					}
					crs_count=0;
					break;
				case STATE_LOAD:
					ret = stateslot(STATE_LOAD);
					if(ret>=0){
						strcpy(msg, "State Load Failed");
						if(load_state(ret,SaveFile))
							strcpy(msg, "State Loaded Successfully");
					}
					crs_count=0;
					break;
				case SCREEN_SIZE:
					ret = screensize(setting.screensize);
					if(ret>=0)
					setting.screensize = ret;
					crs_count=0;
					break;
				case MAX_FRAME_SKIP:
					ret = myFrameskip(setting.frameskip);
					if(ret>=0)
					setting.frameskip = ret;
					crs_count=0;
					break;
				case SOUND:
					setting.sound = !setting.sound;
					break;
				case VSYNC:
					setting.vsync = !setting.vsync;
					break;
				case CPU_CLOCK:
					ret = cpuclock(setting.cpu_clock);
					if (ret>=0)
						setting.cpu_clock = ret;
					break;
				case RESET:
					runGame(true);
					break;
				case CONTINUE:
					runGame(false);
					break;
			}
		}else if(new_pad & PSP_CTRL_SQUARE){
		}else if(new_pad & PSP_CTRL_CROSS){
			//bLoop = 0;
			sel=CONTINUE;
		}else if(new_pad & PSP_CTRL_UP){
			if(sel!=0) 
				sel--;
			else       
				sel=CONTINUE;
		}else if(new_pad & PSP_CTRL_DOWN){
			if(sel!=CONTINUE)
				sel++;
			else
				sel=0;
		}else if(new_pad & PSP_CTRL_LEFT){
				sel=0;
		}else if(new_pad & PSP_CTRL_RIGHT){
				sel=LOAD_ROM;
		}else if(get_nShortcutKey(new_pad)==6){
			bLoop = 0;
			break;
		}
	
		if(!bLoop) break;
		if (crs_count++>=30) crs_count=0;
		
		myFrame(msg, "O - OK     Home - Quit");
		
		x = 4;
		y = 5;
			
		myPrintfXY(x++,y++,setting.color[3],"AVAILABLE OPTIONS:");
		myPrintfXY(x,y++,setting.color[y-7==sel?2:3],"SELECT GAME");
		myPrintfXY(x,y++,setting.color[y-7==sel?2:3],"STATE SAVE");
		myPrintfXY(x,y++,setting.color[y-7==sel?2:3],"STATE LOAD");
		myPrintfXY(x,y++,setting.color[y-7==sel?2:3],"SCREEN SIZE   : %s",scr_names[setting.screensize]);
		myPrintfXY(x,y++,setting.color[y-7==sel?2:3],"SOUND         : %s",setting.sound?"ON":"OFF");
		myPrintfXY(x,y++,setting.color[y-7==sel?2:3],"VSYNC         : %s",setting.vsync?"ON":"OFF");
		myPrintfXY(x,y++,setting.color[y-7==sel?2:3],"MAX FRAME SKIP: %d",setting.frameskip);
		myPrintfXY(x,y++,setting.color[y-7==sel?2:3],"CPU CLOCK     : %s",cpu_clocks[setting.cpu_clock]);
		myPrintfXY(x,y++,setting.color[y-7==sel?2:3],"Reset");
		myPrintfXY(x,y++,setting.color[y-7==sel?2:3],"Continue");
		y++;
		myPrintfXY(x,y++,setting.color[3],"Game name : %s", RomName);

		refreshScreen();
	}
	
	save_config(GBAPath);
	memset(&paddata, 0x00, sizeof(paddata));
	wavoutClear();
	set_cpu_clock(0);
}
