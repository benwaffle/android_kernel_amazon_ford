#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/version.h>
#include <linux/thermal.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kobject.h>
#include <linux/err.h>
#include "linux/platform_data/mtk_thermal.h"
#include "mach/mtk_thermal_monitor.h"

#define MAX_UNPLUG_CORE	3

extern void hp_limited_cpu_num(int num);

static int mtk_hotplug_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct mtk_cooler_platform_data *pdata = cdev->devdata;
	*state = pdata->max_state;
	return 0;
}

static int mtk_hotplug_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct mtk_cooler_platform_data *pdata = cdev->devdata;
	*state = pdata->state;
	return 0;
}

static int mtk_hotplug_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct mtk_cooler_platform_data *pdata = cdev->devdata;
	int level = pdata->levels[state];

	pdata->level = level;
	if (level >= 0 && level <= MAX_UNPLUG_CORE) {  // only can unplug 3 cpu max
		hp_limited_cpu_num(4-level);
		pr_info("%s state %ld, level %d\n", __func__, state, level);
	}
	else
		pr_err("%s wrong level state %ld, level %d\n", __func__, state, level);
	pdata->state = state;

	return 0;
}

static ssize_t levels_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_cooling_device *cdev = container_of(dev, struct thermal_cooling_device, device);
	struct mtk_cooler_platform_data *pdata = cdev->devdata;
	int i;
	int offset = 0;

	if (!pdata)
		return -EINVAL;
	for (i = 0; i < THERMAL_MAX_TRIPS; i++)
		offset += sprintf(buf + offset, "%d %d\n", i, pdata->levels[i]);
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int level, state;
	struct thermal_cooling_device *cdev = container_of(dev, struct thermal_cooling_device, device);
	struct mtk_cooler_platform_data *pdata = cdev->devdata;

	if (!pdata)
		return -EINVAL;
	if (sscanf(buf, "%d %d\n", &state, &level) != 2)
		return -EINVAL;
	if (state > THERMAL_MAX_TRIPS)
		return -EINVAL;
	if (level < 0 || level > MAX_UNPLUG_CORE)
		return -EINVAL;

	pdata->levels[state] = level;
	return count;
}

static DEVICE_ATTR(levels, S_IRUGO | S_IWUSR, levels_show, levels_store);


static struct thermal_cooling_device_ops mtk_hotplug_cooling_ops = {
	.get_max_state = mtk_hotplug_get_max_state,
	.get_cur_state = mtk_hotplug_get_cur_state,
	.set_cur_state = mtk_hotplug_set_cur_state,
};


static int mtk_hotplug_cooling_probe(struct platform_device *pdev)
{
	struct mtk_cooler_platform_data *pdata = dev_get_platdata(&pdev->dev);

	if (!pdata)
		return -EINVAL;

	pdata->cdev = thermal_cooling_device_register(pdata->type, pdata, &mtk_hotplug_cooling_ops);

	if (IS_ERR(pdata->cdev)) {
		dev_err(&pdev->dev, "Failed to register hotplug cooling device\n");
		return PTR_ERR(pdata->cdev);
	}

	platform_set_drvdata(pdev, pdata->cdev);
	device_create_file(&pdata->cdev->device, &dev_attr_levels);
	dev_info(&pdev->dev, "Cooling device registered: %s\n",	pdata->cdev->type);

	return 0;
}

static int mtk_hotplug_cooling_remove(struct platform_device *pdev)
{
	struct thermal_cooling_device *cdev = platform_get_drvdata(pdev);

	thermal_cooling_device_unregister(cdev);
	return 0;
}

static struct platform_driver mtk_hotplug_cooling_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "mtk-hotplug-cooling",
	},
	.probe = mtk_hotplug_cooling_probe,
	.remove = mtk_hotplug_cooling_remove,
};

struct mtk_cooler_platform_data mtk_hotplug_cooling_pdata = {
	.type = "cpuhotplug",
	.state = 0,
	.max_state = 4, /* 0-4 total 5 */
	.level = 0,
	.levels = {
		0, 0, 0, 0, 0
	},
};

static struct platform_device mtk_hotplug_cooling_device = {
	.name = "mtk-hotplug-cooling",
	.id = -1,
	.dev = {
		.platform_data = &mtk_hotplug_cooling_pdata,
	},
};

static int __init mtk_cooler_hotplug_init(void)
{
	int ret;
	ret = platform_driver_register(&mtk_hotplug_cooling_driver);
	if (ret) {
		pr_err("Unable to register hotplug cooling driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&mtk_hotplug_cooling_device);
	if (ret) {
		pr_err("Unable to register hotplug cooling device (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit mtk_cooler_hotplug_exit(void)
{
	platform_driver_unregister(&mtk_hotplug_cooling_driver);
	platform_device_unregister(&mtk_hotplug_cooling_device);
}
module_init(mtk_cooler_hotplug_init);
module_exit(mtk_cooler_hotplug_exit);
