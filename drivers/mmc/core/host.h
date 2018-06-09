/*
 *  linux/drivers/mmc/core/host.h
 *
 *  Copyright (C) 2003 Russell King, All Rights Reserved.
 *  Copyright 2007 Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _MMC_CORE_HOST_H
#define _MMC_CORE_HOST_H
#include <linux/mmc/host.h>

int mmc_register_host_class(void);
void mmc_unregister_host_class(void);

#if (defined(CONFIG_AMAZON_METRICS_LOG) && defined(ENABLE_SAMSUNG_EMMC_METRICS))
extern void mmc_host_metrics_work(struct work_struct *work);
#endif /* CONFIG_AMAZON_METRICS_LOG */

#endif

