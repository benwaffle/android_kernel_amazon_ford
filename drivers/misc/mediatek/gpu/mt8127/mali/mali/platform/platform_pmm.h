/*
 * Copyright (C) 2010, 2012-2014 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef __PLATFORM_PMM_H__
#define __PLATFORM_PMM_H__

struct mali_gpu_utilization_data;

typedef enum mali_power_mode
{
    MALI_POWER_MODE_ON  = 0x1,
    MALI_POWER_MODE_DEEP_SLEEP,
    MALI_POWER_MODE_LIGHT_SLEEP,
    //MALI_POWER_MODE_NUM
} mali_power_mode;

/** @brief Platform power management initialisation of MALI
 *
 * This is called from the entrypoint of the driver to initialize the platform
 *
 */
void mali_pmm_init(void);

/** @brief Platform power management deinitialisation of MALI
 *
 * This is called on the exit of the driver to terminate the platform
 *
 * @return _MALI_OSK_ERR_OK on success otherwise, a suitable _mali_osk_errcode_t error.
 */
void mali_pmm_deinit(void);

/** @brief Platform power management mode change of MALI
 *
 * This is called on the power state transistion of MALI.
 * @param mode defines the power modes
 */
void mali_pmm_tri_mode(mali_power_mode mode);

/** @brief Platform power management specific GPU utilization handler
 *
 * When GPU utilization handler is enabled, this function will be
 * periodically called.
 *
 * @param utilization The Mali GPU's work loading from 0 ~ 256. 0 = no utilization, 256 = full utilization.
 */
void mali_pmm_utilization_handler(struct mali_gpu_utilization_data *data);

unsigned long gpu_get_current_utilization(void);

void mali_platform_power_mode_change(mali_power_mode power_mode);

#endif

