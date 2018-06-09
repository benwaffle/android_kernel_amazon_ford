/*
 * Copyright (C) 2012-2014 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "mali_hw_core.h"
#include "mali_osk.h"
#include "mali_kernel_common.h"
#include "mali_osk_mali.h"

#define MAX_RECORD_CORE 64
static struct mali_hw_core * dump_core_list[MAX_RECORD_CORE] = {0};
static int dump_core_idx = 0;
_mali_osk_errcode_t mali_hw_core_create(struct mali_hw_core *core, const _mali_osk_resource_t *resource, u32 reg_size)
{
	core->phys_addr = resource->base;
	core->phys_offset = resource->base - _mali_osk_resource_base_address();
	core->description = resource->description;
	core->size = reg_size;

	MALI_DEBUG_ASSERT(core->phys_offset < core->phys_addr);

	if (_MALI_OSK_ERR_OK == _mali_osk_mem_reqregion(core->phys_addr, core->size, core->description)) {
		core->mapped_registers = _mali_osk_mem_mapioregion(core->phys_addr, core->size, core->description);
		if (NULL != core->mapped_registers) {
			MALI_DEBUG_PRINT(2, ("SUCC map register for %s, Virt: %p, Size: 0x%x, Phy: %p\n", core->description, core->mapped_registers, reg_size, core->phys_addr));
			if(dump_core_idx < MAX_RECORD_CORE) {
				dump_core_list[dump_core_idx] = core;
				dump_core_idx++;
			} else {
				/* find a deleted item, normally cores are created once on probe */
				int i;
				for(i = 0; i < MAX_RECORD_CORE; i++) {
					if(dump_core_list[i] == NULL) {
						dump_core_list[i] = core;
					}
				}
				MALI_DEBUG_PRINT(2, ("Warnning, out of recorded cores %d\n", dump_core_idx ));				
			}
			return _MALI_OSK_ERR_OK;
		} else {
			MALI_PRINT_ERROR(("Failed to map memory region for core %s at phys_addr 0x%08X\n", core->description, core->phys_addr));
		}
		_mali_osk_mem_unreqregion(core->phys_addr, core->size);
	} else {
		MALI_PRINT_ERROR(("Failed to request memory region for core %s at phys_addr 0x%08X\n", core->description, core->phys_addr));
	}

	return _MALI_OSK_ERR_FAULT;
}

void mali_hw_core_delete(struct mali_hw_core *core)
{
	int i;
	for(i = 0; i < dump_core_idx; i++){
		if(core == dump_core_list[i]) {
			dump_core_list[i] = NULL;
			break;
		}
	}
	_mali_osk_mem_unmapioregion(core->phys_addr, core->size, core->mapped_registers);
	core->mapped_registers = NULL;
	_mali_osk_mem_unreqregion(core->phys_addr, core->size);
}

void mali_hw_core_dump(){
	unsigned long addr;
	u32 val;
	int i;
	for(i = 0; i < dump_core_idx; i ++) {
		struct mali_hw_core * core = dump_core_list[i];
		if(core && core->mapped_registers) {		
			MALI_DEBUG_PRINT(2, ("Mali HW core dump : Start ---- %s \n", core->description));
			for(addr = (unsigned long)core->mapped_registers; addr - (unsigned long)core->mapped_registers < core->size; addr += 4) {
				val = *(volatile u32 *)addr;
				/*Only show none zero*/
				if(val) {	
					MALI_DEBUG_PRINT(2, ("[0x%08x] -> 0x%08x\n",  addr - (unsigned long)core->mapped_registers + core->phys_addr,  *(volatile u32 *)addr));
				}
			}			
			MALI_DEBUG_PRINT(2, ("Mali HW core dump : End ----- %s \n", core->description));
		}
	}
}

