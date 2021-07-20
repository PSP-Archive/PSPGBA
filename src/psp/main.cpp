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
#include <pspctrl.h>
#include <pspaudiolib.h>
#include <psppower.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include "GBA.h"
#include "Sound.h"
#include "Util.h"
#include "gb/GB.h"
#include "gb/gbGlobals.h"
#include "main.h"
#include "malloc.h"


/* Define the module info section */
PSP_MODULE_INFO("GBA", 0, 1, 1);

/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);



struct EmulatedSystem emulator=GBASystem;
int linesize=GBA_SCREEN_WIDTH;
int systemRedShift = 0;
int systemGreenShift = 6;
int systemBlueShift = 11;
int systemColorDepth = 16;
int systemDebug = 0;
int systemVerbose = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
int systemFrameSkip = 1;
int cartridgeType = -1;
int bSleep = 0;
int speedcount=0;

int emulating = 0;
int RGB_LOW_BITS_MASK=0x821;
u16 systemColorMap16[0x10000];
u32 systemColorMap32[0x10000];
u16 systemGbPalette[24];
char filename[2048];

bool vsync = true;
bool paused = false;
bool debugger = false;
bool systemSoundOn = false;
bool removeIntros = false;

unsigned int soundBufferReady=0;

u16* buf;

int autoFire = 0;
bool autoFireToggle = false;
int scale = 0;
bool rotate = true;
bool specialKey = false;
int save_game_key = false;

static time_t t1;
static time_t t2;
static float fps=0.0;
static float sec = 1.0;

extern bool cpuDisableSfx;

char *sdlGetFilename(char *name)
{
	static char filebuffer[2048];
	
	int len = strlen(name);
	
	char *p = name + len - 1;
	
	while(true) {
	if(*p == '/' ||
		*p == '\\') {
		p++;
		break;
	}
	len--;
	p--;
	if(len == 0)
		break;
	}
	
	if(len == 0)
		strcpy(filebuffer, name);
	else
		strcpy(filebuffer, p);
	return filebuffer;
}


void systemMessage(int num, const char *msg, ...)
{
	myDebug(false,"%s ",msg);
	myScreenFlip();
}

void winlog(const char *, ...)
{
}

void log(const char *, ...)
{
}

bool systemReadJoypads()
{
	return true;
}

u32 systemReadJoypad(int)
{
	u32 res = 0;
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(&pad, 1);
	
	if (pad.Ly>200) pad.Buttons|=PSP_CTRL_DOWN;  // DOWN
	if (pad.Ly<100) pad.Buttons|=PSP_CTRL_UP;    // UP
	if (pad.Lx<100) pad.Buttons|=PSP_CTRL_LEFT;  // LEFT
	if (pad.Lx>200) pad.Buttons|=PSP_CTRL_RIGHT; // RIGHT
	
	
	if (pad.Buttons != 0){

		if (pad.Buttons & PSP_CTRL_SQUARE){
			specialKey=true;
		}
		if (pad.Buttons & PSP_CTRL_TRIANGLE){
			emulating=0;
		} 
		if (pad.Buttons & PSP_CTRL_CIRCLE){
			res |= 1; // a
		}  else {
			res &= 65535- 1; // a	
		}
		
		if (pad.Buttons & PSP_CTRL_CROSS){
			res |= 2; // b
		}  else {
			res &= 65535- 2;
		}

		if (pad.Buttons & PSP_CTRL_UP){
			res |= 64; //up
		}  else {
			res &= 65535- 64;
		}

		if (pad.Buttons & PSP_CTRL_DOWN){
			res |= 128;//down
		}  else {
			res &= 65535- 128;
		}

		if (pad.Buttons & PSP_CTRL_LEFT){
			res |= 32; //left
		}  else {
			res &= 65535- 32;
		}

		if (pad.Buttons & PSP_CTRL_RIGHT){
			res |= 16; //right
		}  else {
			res &= 65535- 16; // right
		}

		if (pad.Buttons & PSP_CTRL_START){
			res |= 8; // start
		}  else {
			res &= 65535- 8; // start
		}

		if (pad.Buttons & PSP_CTRL_SELECT){
			res |= 4; //select
		}  else {
			res &= 65535- 4; // select
		}

		if (pad.Buttons & PSP_CTRL_LTRIGGER){
			res |= 512; //L
		}  else {
			res &= 65535- 512; // L	
		}

		if (pad.Buttons & PSP_CTRL_RTRIGGER){
			res |= 256;//R
		}  else {
			res &= 65535- 256; // R	
		}
	}

	// disallow L+R or U+D of being pressed at the same time
	if((res & 48) == 48)
		res &= ~16;
	if((res & 192) == 192)
		res &= ~128;
	
	if(autoFire) {
		res &= (~autoFire);
		if(autoFireToggle)
			res |= autoFire;
		autoFireToggle = !autoFireToggle;
	}
	
	return res;
}

void systemSetTitle(const char *title)
{
}

void systemScreenCapture(int a)
{
}


void systemWriteDataToSoundBuffer()
{
	soundBufferReady=soundBufferIndex;
	return;
}


/* This function gets called by pspaudiolib every time the
   audio buffer needs to be filled. The sample format is
   16-bit, stereo. */
void audioCallback(void* buf, unsigned int length, void *userdata) {

	if (!systemSoundOn || !soundBufferReady) {
		memset(buf,0, length*4);
		return;
	}
	
	//memcpy(buf,soundFinalWave, length);
	unsigned int l=(length>soundBufferReady)?soundBufferReady:length;
	soundBufferReady = 0;
	__memcpy4a((unsigned long *)buf, (unsigned long *)soundFinalWave, l);
}

void wavoutClear()
{
	systemSoundOn=false;
	memset(soundFinalWave, 0, sizeof(soundFinalWave));
}

bool systemSoundInit()
{
	if (systemSoundOn)
		return 1;
	else
		return 0;
}

void systemSoundShutdown()
{
	wavoutClear();
}

void systemSoundPause()
{
	wavoutClear();
}

void systemSoundResume()
{
}

void systemSoundReset()
{
}

static int ticks = 0;

u32 systemGetClock()
{
  return ticks++;
}

void systemUpdateMotionSensor()
{
}

int systemGetSensorX()
{
	return 0;
}

int systemGetSensorY()
{
	return 0;
}

void systemGbPrint(u8 *data,int pages,int feed,int palette, int contrast)
{
}

void systemScreenMessage(const char *msg)
{
}

bool systemCanChangeSoundQuality()
{
	return true;
}

bool systemPauseOnFrame()
{
	return false;
}

void systemGbBorderOn()
{
}

void systemShowSpeed(int speed)
{
}

 
void systemFrame()
{
}


void system10Frames(int rate)
{
#ifdef speedtest
	speedcount++;
	if (speedcount==150)
	{
		time(&t2);
		sec=difftime(t2,t1);
		fps=1500/sec;
		time(&t1);
	}
#endif
}


extern int dispBufferNumber ;	
extern void gfxDrawSprites2();
void systemDrawScreen()
{
	switch (scale)
	{
		case SCR_FULL_3_2: 
			myBitBltGe3_2((u16*)pix,vsync);
#ifdef speedtest			
			myPrintfXY(1,1,0xffffffff,"%.2f fps",fps);
			myPrintfXY(1,2,0xffffffff,"%.2f sec",sec);
#endif		
			myScreenFlip();
			break;
		case SCR_FULL_16_9: 
			myBitBltGe((u16*)pix,vsync);
#ifdef speedtest			
			myPrintfXY(1,1,0xffffffff,"%.2f fps",fps);
			myPrintfXY(1,2,0xffffffff,"%.2f sec",sec);
#endif			
			myScreenFlip();
			break;
		case SCR_GBA_240_160:
#ifdef speedtest			
			myPrintfXY(1,1,0xffffffff,"%.2f fps",fps);
			myPrintfXY(1,2,0xffffffff,"%.2f sec",sec);
#endif			
			myScreenFlip();
			pix=(u8*)(getVramDrawBuffer() + 28672 + 120);
			break;
	}
}


void startGame(int skip, bool vc, bool sound, int zoom, bool reset)
{
	if (cartridgeType<0)
		return;
	if (reset)
		CPUReset();
	
	systemFrameSkip = skip;
	vsync=vc;
#ifdef speedtest
	if (skip==9)
		systemFrameSkip=1500;
#endif
	systemSoundOn = sound;
	scale=zoom;
	
	myScreenClear();
	myScreenFlip();
	myScreenClear();
	
	switch (scale)
	{
		case SCR_FULL_3_2: 
			linesize=GBA_SCREEN_WIDTH;
			pix = (u8*)buf;
			break;
		case SCR_FULL_16_9: 
			linesize=GBA_SCREEN_WIDTH;
			pix = (u8*)buf;
			break;
		case SCR_GBA_240_160:
			linesize=PSP_LINE_SIZE;
			pix=(u8*)(getVramDrawBuffer() + 28672 + 120);
			break;
	}

	emulating = 1;
	time(&t1);
	speedcount=0;
	while(emulating){
		emulator.emuMain(500000);
	}


}


/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
	emulating = 0;
	bSleep=1;
	set_cpu_clock(0);
	save_config(GBAPath);
	sceKernelExitGame();
	return 0;
}

int power_callback(int unknown, int pwrflags, void *common)
{
	if (pwrflags & PSP_POWER_CB_POWER_SWITCH || pwrflags & PSP_POWER_CB_SUSPENDING || pwrflags & PSP_POWER_CB_STANDBY) {
		if (!bSleep){
			bSleep=1;
			scePowerLock(0);
			set_cpu_clock(0);
			save_config(GBAPath);
			scePowerUnlock(0);
		}
	}
	if(pwrflags & PSP_POWER_CB_BATTERY_LOW){
		if (!bSleep){
			bSleep=1;
			scePowerLock(0);
			set_cpu_clock(0);
			save_config(GBAPath);
			scePowerUnlock(0);
		}
	}
	if(pwrflags & PSP_POWER_CB_RESUME_COMPLETE){
		bSleep=0;
	}
	int cbid = sceKernelCreateCallback("Power Callback", power_callback, NULL);
	scePowerRegisterCallback(0, cbid);
	return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	cbid = sceKernelCreateCallback("Power Callback", power_callback, NULL);
	scePowerRegisterCallback(0, cbid);
	sceKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", CallbackThread,
				     0x11, 0xFA0, 0, 0);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, 0, 0);
	}

	return thid;
}
char *__psp_argv_0;

void debuggerOutput(char *, u32)
{
}

void (*dbgOutput)(char *, u32) = debuggerOutput;

int main(int argc, char* argv[])
{

	buf = (u16*)(0x44088000); 
	pix = (u8*)buf;
	

	strcpy(GBAPath,argv[0]);
	*(strrchr(GBAPath, '/')+1) = 0;	
	
	SetupCallbacks();

	pspAudioInit();
	pspAudioSetChannelCallback(0, audioCallback, NULL);
	
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
	
	myScreenInit(PSP_DISPLAY_PIXEL_FORMAT_565,GU_PSM_5650);
	menu();
	

	sceKernelExitGame();
	return 0;
}
