#include "../endmem.h"
#include "../init.h"
#include "../initc.h"
#include "c_irq.h"
#include "execute.h"
#include "memory.h"


static u4 makedl(u4 edx)
{
	edx &= 0xFFFFFF3C;
	if ((flagnz & 0x00018000) != 0) edx |= 0x00000080; // Negative.
	if ((flagnz & 0x0000FFFF) == 0) edx |= 0x00000002; // Zero.
	if ((flagc  & 0x000000FF) != 0) edx |= 0x00000001; // Carry.
	if ((flago  & 0x000000FF) != 0) edx |= 0x00000040; // Overflow.
	return edx;
}


static void call_membank0w8(u2 const cx, u1 const al)
{
	u4 eax;
	u4 ecx;
	u4 ebx;
	asm volatile("call %P3" : "=a" (eax), "=c" (ecx), "=b" (ebx) : "X" (membank0w8), "a" (al), "c" (cx) : "cc", "memory");
}


void IRQemulmode(u4* const pedx, u1** const pesi)
{
	xpc = *pesi - initaddrl;

	u2 cx = xs;

	call_membank0w8(cx, (u1)(xpc >> 8));
	cx = (cx - 1) & stackand | stackor;

	call_membank0w8(cx, (u1)xpc);
	cx = (cx - 1) & stackand | stackor;

	u4 const edx = makedl(*pedx);
	call_membank0w8(cx, (u1)edx);
	cx = (cx - 1) & stackand | stackor;

	xs = cx;

	u2 const ax = irqv8;
	xpb = 0;
	xpc = ax;
	u1* const esi = ax & 0x8000 ? snesmmap[0] : snesmap2[0];
	initaddrl = esi;

	*pedx = edx & 0xFFFFFFF3 | 0x00000004;
	*pesi = esi + ax;
}
