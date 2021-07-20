PSPGBA v1.1 (GBA Emulator for PSP) 
==================================
The PSPGBA is a PSP version porting for VisualBoyAdvance v1.7.2 developed by 
Forgotten and the VBA development team. 

Same as other homebrew software, this is just a hobby and don't expect any 
support or update request. Just take it as it is.



LICENSE
-------

    PSPGBA - (VisualBoyAdvance port for PSP)
    Copyright (C) 2005 psp298

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


Installation/Usage
------------------
Unzip the package and  copy all files to  your memory stick, and  from your HOME
screen navigate to  GAME  MEMORY STICK  and select  the emulator to run.  Follow
the screen instruction to use it.

Press "Triangle" to activate the game menu again while playing.


History 
-------
17 Oct 2006	v1.0		First release v1.0
25 Oct 2006	v1.1		Speed increase, add 240x160 screen mode, add vsync on/off


Problem report
--------------
Please report to http://www.dcemu.co.uk/ Homebrew forum if any.


Source code
-----------
There is nearly no change on VBA source code except to fit the PSP memory size limit
/screen size limit in GBA.cpp. 

You can find the porting source in src/psp folder. Other should be same as VBA v1.7.2
except GBA.cpp.

To compile, you will need the pspsdk of which could be found in http://ps2dev.org/