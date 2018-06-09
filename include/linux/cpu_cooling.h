/*
 *  linux/include/linux/cpu_cooling.h
 *
 *  Copyright (C) 2012	Samsung Electronics Co., Ltd(http://www.samsung.com)
 *  Copyright (C) 2012  Amit Daniel <amit.kachhap@linaro.org>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef __CPU_COOLING_H__
#define __CPU_COOLING_H__

#include <linux/thermal.h>
#include <linux/cpumask.h>

#include <linux/module.h>

#ifdef CONFIG_CPU_THERMAL

/**
 * struct cpufreq_cooling_device - data for cooling device with cpufreq
 * @id: unique integer value corresponding to each cpufreq_cooling_device
 *	registered.
 * @cool_dev: thermal_cooling_device pointer to keep track of the
 *	registered cooling device.
 * @cpufreq_state: integer value representing the current state of cpufreq
 *	cooling	devices.
 * @cpufreq_val: integer value representing the absolute value of the clipped
 *	frequency.
 * @allowed_cpus: all the cpus involved for this cpufreq_cooling_device.
 *
 * This structure is required for keeping information of each
 * cpufreq_cooling_device registered. In order to prevent corruption of this a
 * mutex lock cooling_cpufreq_lock is used.
 */
struct cpufreq_cooling_device {
    int id;
    struct thermal_cooling_device *cool_dev;
    unsigned long levels[THERMAL_MAX_TRIPS];
    unsigned long cooling_state;
    unsigned int cpufreq_state;
    unsigned int cpufreq_val;
    struct cpumask allowed_cpus;
    struct cpufreq_cooling_device *notify_device;
    struct notifier_block nb;
    struct list_head node;
};

/**
 * cpufreq_cooling_register - function to create cpufreq cooling device.
 * @clip_cpus: cpumask of cpus where the frequency constraints will happen
 */
struct thermal_cooling_device *
cpufreq_cooling_register(const struct cpumask *clip_cpus);

/**
 * cpufreq_cooling_unregister - function to remove cpufreq cooling device.
 * @cdev: thermal cooling device pointer.
 */
void cpufreq_cooling_unregister(struct thermal_cooling_device *cdev);

unsigned long cpufreq_cooling_get_level(unsigned int cpu, unsigned int freq);
unsigned int cpufreq_cooling_get_frequency(unsigned int cpu, unsigned long level);
#else /* !CONFIG_CPU_THERMAL */
static inline struct thermal_cooling_device *
cpufreq_cooling_register(const struct cpumask *clip_cpus)
{
	return NULL;
}
static inline
void cpufreq_cooling_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
static inline
unsigned long cpufreq_cooling_get_level(unsigned int cpu, unsigned int freq)
{
	return THERMAL_CSTATE_INVALID;
}
unsigned int cpufreq_cooling_get_frequency(unsigned int cpu, unsigned long level)
{
	return 0;
}
#endif	/* CONFIG_CPU_THERMAL */

#endif /* __CPU_COOLING_H__ */
