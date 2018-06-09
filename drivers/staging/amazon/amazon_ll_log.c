/*
 * amazon_ll_log.c
 *
 * Store low level software log to /proc/pllk.
 *
 * Copyright (C) 2017 Amazon Technologies Inc. All rights reserved.
 * Qiang Liu (qanliu@amazon.com)
 * TODO: Add additional contributor's names.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/tlbflush.h>
#include <linux/bootmem.h>
#include <linux/console.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#endif

#define LOG_MAGIC 0x414d5a4e	/* "AMZN" */

static u32 amazonlog_base = CONFIG_AMAZON_LOW_LEVEL_LOG_DRAM_ADDR;
static u32 amazonlog_size = CONFIG_AMAZON_LOW_LEVEL_LOG_DRAM_SIZE;

/* These variables are also protected by logbuf_lock */
static char *amazon_log_buf;
static unsigned int *amazon_log_pos;
static unsigned int amazon_log_size;
static int amazonlogging;

#ifdef CONFIG_OF
static int __init amazon_log_fdt_find_info(unsigned long node, const char *uname,
			int depth, void *data)
{
	__be32 *prop;
	unsigned long len;

	prop = of_get_flat_dt_prop(node, "amazonlog-base", &len);
	if (!prop || (len != sizeof(unsigned int))) {
		pr_err("%s: Can't find amazon log-base property\n", __func__);
		return 0;
	}

	amazonlog_base = be32_to_cpu(prop[0]);

	prop = of_get_flat_dt_prop(node, "amazonlog-size", &len);
	if (!prop || (len != sizeof(unsigned int))) {
		pr_err("%s: Can't find amazon log-size property\n", __func__);
		return 0;
	}

	amazonlog_size = be32_to_cpu(prop[0]);

	return 1;
}
#endif

static void amazon_log_buf_write(struct console *console, const char *text,
			      unsigned int size)
{
	if (amazon_log_buf && amazon_log_pos) {
		unsigned int pos = *amazon_log_pos;

		if (likely(size + pos <= amazon_log_size)) {
			memcpy(&amazon_log_buf[pos], text, size);
		} else {
			unsigned int first = amazon_log_size - pos;
			unsigned int second = size - first;
			memcpy(&amazon_log_buf[pos], text, first);
			memcpy(&amazon_log_buf[0], text + first, second);
		}

		(*amazon_log_pos) += size; /* Check overflow */
		if (unlikely(*amazon_log_pos >= amazon_log_size))
			*amazon_log_pos -= amazon_log_size;
	}

	return;
}

static int amazon_ll_log_show(struct seq_file *m, void *v)
{
	seq_write(m, amazon_log_buf, amazonlog_size);
	return 0;
}

static int amazon_ll_log_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, amazon_ll_log_show, inode->i_private);
}

static const struct file_operations amazon_ll_log_file_ops = {
	.owner = THIS_MODULE,
	.open = amazon_ll_log_file_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

#ifdef CONFIG_ARM64
	prot = pgprot_writecombine(PAGE_KERNEL);
#else
	prot = pgprot_noncached(PAGE_KERNEL);
#endif
	pages = kmalloc(sizeof(struct page *) * page_count, GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n", __func__, page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}

	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		pr_err("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}

static int __init amazon_log_buf_init(void)
{
	unsigned int *amazon_log_mag;
	void *vaddr;
	struct proc_dir_entry *entry;
	unsigned int len = 0;

#ifdef CONFIG_OF
	if (of_scan_flat_dt(amazonlog_fdt_find_info, NULL)) {
		BUG_ON(memblock_reserve(amazonlog_base, amazonlog_size) != 0);
		pr_info("Reserved amazon log memory : %dMB at %#.8x\n",
			((unsigned)amazonlog_size >> 20), (unsigned)amazonlog_base);
	}
#endif
	vaddr = remap_lowmem(amazonlog_base, amazonlog_size);
	amazon_log_size = amazonlog_size - (sizeof(*amazon_log_pos) + sizeof(*amazon_log_mag));
	amazon_log_buf = vaddr;
	amazon_log_pos = (unsigned int *)(vaddr + amazon_log_size);
	amazon_log_mag = (unsigned int *)(vaddr + amazon_log_size + sizeof(*amazon_log_pos));

	do {
		len += printk(KERN_WARNING "%s", amazon_log_buf+len);
		len++;
	} while (len < *amazon_log_pos);

	pr_warning("%s: *amazon_log_mag:%x *amazon_log_pos:%x "
		"amazon_log_buf:%p amazon_log_size:%u\n",
		__func__, *amazon_log_mag, *amazon_log_pos, amazon_log_buf,
		amazon_log_size);

	if (*amazon_log_mag != LOG_MAGIC) {
		pr_warning("%s: no old log found\n", __func__);
		*amazon_log_pos = 0;
		*amazon_log_mag = LOG_MAGIC;
	}

	/* save the pre-console log into amazon log buffer */

	amazonlogging = 1;
	/* register_log_text_hook(emit_amazon_log, amazon_log_buf, amazon_log_size); */
	entry = proc_create("pllk", 0444, NULL, &amazon_ll_log_file_ops);
	if (!entry) {
		pr_err("ram_console: failed to create proc entry\n");
		return 0;
	}

	return 0;
}
console_initcall(amazon_log_buf_init);
