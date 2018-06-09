/*
 * Copyright (C) 2010, 2012-2014 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_osk_misc.c
 * Implementation of the OS abstraction layer for the kernel device driver
 */
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include "mali_osk.h"
#include "mt_reg_base.h"
#include "mt_clkmgr.h"
#include "mt_spm.h"

extern void smi_dumpDebugMsg(void);
extern int m4u_dump_debug_registers(void);;

void _mali_osk_dbgmsg( const char *fmt, ... )
{
	va_list args;
	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);
}

u32 _mali_osk_snprintf( char *buf, u32 size, const char *fmt, ... )
{
	int res;
	va_list args;
	va_start(args, fmt);

	res = vscnprintf(buf, (size_t)size, fmt, args);

	va_end(args);
	return res;
}
#if 0
#define CLK_CFG_0           (INFRA_BASE + 0x0040)
#define VENCPLL_CON0        (DDRPHY_BASE+0x800)
#define MMPLL_CON0          (APMIXEDSYS_BASE + 0x0230)

void _mali_osk_ctxprintf(struct seq_file  *print_ctx, const char *fmt, ...)
{
	va_list args;
	char buf[512];

	va_start(args, fmt);
	vscnprintf(buf, 512, fmt, args);
	seq_printf(print_ctx, buf);
	va_end(args);

}
#endif
extern void mali_hw_core_dump();
void _mali_osk_dump_regs() {
	unsigned long addr;
	
	MALI_DEBUG_PRINT(2, ("MALI START (Ignore 0) ------\n"));

	mali_hw_core_dump();

	MALI_DEBUG_PRINT(2, ("MALI POWER DUMP ---------------\n"));
	MALIK_MSG("MFG clock is %d and SMI clock is %d (From clkmgr view)\n", clock_is_on(MT_CG_MFG_G3D), clock_is_on(MT_CG_DISP0_SMI_COMMON));

#define SHOW_REG(name) MALI_DEBUG_PRINT(2, (" [%s]\t[%p] = 0x%08x\n",#name, name, *((volatile u32 *)name)))
	SHOW_REG(CLK_CFG_1);
	SHOW_REG(MMPLL_CON0);
	SHOW_REG(MMPLL_CON1);
	SHOW_REG(MMPLL_PWR_CON0);
	SHOW_REG(UNIVPLL_CON0);	
	SHOW_REG(UNIVPLL_CON1);
	SHOW_REG(UNIVPLL_PWR_CON0);
	SHOW_REG(SPM_MFG_PWR_CON);
	MALI_DEBUG_PRINT(2, ("MFG START ------------------\n"));
 	for(addr = G3D_CONFIG_BASE; addr < G3D_CONFIG_BASE + 0x19c; addr += 4) {
		MALI_DEBUG_PRINT(2, ("MFG[0x%08x] -> 0x%08x \n", addr - G3D_CONFIG_BASE + 0x13000000, *(volatile u32 *)addr));
 	}	
}

void _mali_osk_abort(void)
{
#if 0
    int index;
#endif

	/* make a simple fault by dereferencing a NULL pointer */
	dump_stack();
	_mali_osk_dump_regs();
#if 0
	for (index = 0; index < 5; index++)
    {
        MALI_DEBUG_PRINT(2, ("=== [MALI] PLL Dump %d ===\n", index));       
        MALI_DEBUG_PRINT(2, ("CLK_CFG_0: 0x%08x\n", *((volatile unsigned int*)CLK_CFG_0)));
        MALI_DEBUG_PRINT(2, ("VENCPLL_CON0: 0x%08x\n", *((volatile unsigned int*)VENCPLL_CON0)));
        MALI_DEBUG_PRINT(2, ("MMPLL_CON0: 0x%08x\n", *((volatile unsigned int*)MMPLL_CON0)));

        MALI_DEBUG_PRINT(2, ("=== [MALI] SMI Dump %d ===\n", index));
        smi_dumpDebugMsg();

        MALI_DEBUG_PRINT(2, ("=== [MALI] 8127 m4u not provide API? M4U Dump %d ===\n", index));
        /*m4u_dump_debug_registers();*/
    }
#endif
#if 1
	*(int *)0 = 0;
#endif
}

void _mali_osk_break(void)
{
	_mali_osk_abort();
}

u32 _mali_osk_get_pid(void)
{
	/* Thread group ID is the process ID on Linux */
	return (u32)current->tgid;
}

char *_mali_osk_get_comm(void)
{
	return (char *)current->comm;
}

u32 _mali_osk_get_tid(void)
{
	/* pid is actually identifying the thread on Linux */
	u32 tid = current->pid;

	/* If the pid is 0 the core was idle.  Instead of returning 0 we return a special number
	 * identifying which core we are on. */
	if (0 == tid) {
		tid = -(1 + raw_smp_processor_id());
	}

	return tid;
}
