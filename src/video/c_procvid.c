#include "../asm_call.h"
#include "../c_intrf.h"
#include "../cfg.h"
#include "../cpu/execute.h"
#include "../cpu/regs.h"
#include "../init.h"
#include "../input.h"
#include "../ui.h"
#include "c_procvid.h"
#include "procvid.h"


void showvideo(void)
{
	if (++ccud != cacheud) ccud = 0;
	asm_call(copyvid);
	if (pressed[KeyStateSelct] & 1) asm_call(saveselect);
}


u4 SwapMouseButtons(u4 const buttons)
{
	return (buttons & ~0x3) | (buttons << 1 & 0x2) | (buttons >> 1 & 0x1);
}


void processmouse1(void)
{
	s2 dx;
	s2 dy;
#ifndef __MSDOS__
	if (MouseCount > 1)
	{
		mouse = 0;
		MultiMouseProcess();
		u4 buttons = MouseButtons[0];
		if (mouse1lh == 1) buttons = SwapMouseButtons(buttons);
		mousebuttons = buttons;
		dx = MouseMoveX[0];
		dy = MouseMoveY[0];
	}
	else
#endif
	{
		u4 buttons = Get_MouseData() & 0xFFFF;
		if (mouse1lh == 1) buttons = SwapMouseButtons(buttons);
		mousebuttons = buttons;
		u4 const eax = Get_MousePositionDisplacement();
		dy = eax >> 16;
		dx = eax;
	}
	if (dx != 0) mousexdir = dx < 0 ? dx = -dx, 1 : 0;
	mousexpos = dx;
	if (dy != 0) mouseydir = dy < 0 ? dy = -dy, 1 : 0;
	mouseypos = dy;
}


void processmouse2(void)
{
	static u1 ssautoswb;

	u4 buttons;
#ifndef __MSDOS__
	if (MouseCount > 1)
	{
		mouse = 1;
		MultiMouseProcess();
		buttons = MouseButtons[1];
	}
	else
#endif
	{
		buttons = Get_MouseData() & 0xFFFF;
	}
	if (mouse2lh == 1) buttons = SwapMouseButtons(buttons);
	mousebuttons = buttons;

	if (device2 == 2)
	{
		if (pressed[SSAutoFire] == 0)
		{
			ssautoswb = 0;
		}
		else if (ssautoswb != 1)
		{
			ssautosw  ^= 0x20;
			ssautoswb  = 1;
			Msgptr     = ssautosw != 0 ? "AUTOFIRE ENABLED." : "AUTOFIRE DISABLED.";
			MessageOn  = MsgCount;
		}
	}

#ifndef __MSDOS__
	s2 dx;
	s2 dy;
	if (MouseCount > 1)
	{
		dx = MouseMoveX[1];
		dy = MouseMoveY[1];
	}
	else
#endif
	{
		u4 const eax = Get_MousePositionDisplacement();
		dy = eax >> 16;
		dx = eax;
	}

	if (2 <= device2 && device2 <= 4)
	{
		s4 x = mousexloc + dx;
		if (x <   0) x =   0;
		if (x > 255) x = 255;
		mousexloc = x;
		latchx    = x + 40;
		extlatch  = 0x40;
	}
	if (dx != 0) mousexdir = dx < 0 ? dx = -dx, 1 : 0;
	mousexpos = dx;

	if (2 <= device2 && device2 <= 4)
	{
		s4 y = mouseyloc + dy;
		if (y <   0) y =   0;
		if (y > 223) y = 223;
		mouseyloc = y;
		latchy    = y;
	}
	if (dy != 0) mouseydir = dy < 0 ? dy = -dy, 1 : 0;
	mouseypos = dy;
}


#ifdef __MSDOS__
void outputhex(u1* buf, u1 const val)
{
	for (u4 i = 0; i != 2; buf += 8, ++i)
	{
		u4 const  digit = val >> (i * 4) & 0x0F;
		u1 const* src   = FontData[digit + 1];
		u1*       dst   = buf;
		u4        y     = 8;
		do
		{
			u1 ah = *src++;
			u4 x  = 8;
			do
			{
				if (ah & 0x80)
				{
					dst[0]   = 128;
					dst[289] = 192;
				}
				ah <<= 1;
				++dst;
			}
			while (--x != 0);
			dst += 280;
		}
		while (--y != 0);
	}
}
#endif