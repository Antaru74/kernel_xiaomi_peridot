// SPDX-License-Identifier: GPL-2.0
/*
 * QCOM CPUFreq HW Undervolt Implementation
 * Copyright (c) 2026, Antaru74
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>

/*
 * Per-CPU undervolt data structure
 */
struct cpu_undervolt_data {
	int undervolt_offset_mv;
	struct device_attribute undervolt_attr;
	spinlock_t lock;
};

static struct cpu_undervolt_data *cpu_undervolt[NR_CPUS];

/*
 * Sysfs handler: display current undervolt offset
 */
static ssize_t undervolt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cpu_undervolt_data *data =
		container_of(attr, struct cpu_undervolt_data, undervolt_attr);
	unsigned long flags;
	int val;

	spin_lock_irqsave(&data->lock, flags);
	val = data->undervolt_offset_mv;
	spin_unlock_irqrestore(&data->lock, flags);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

/*
 * Sysfs handler: set undervolt offset (in mV)
 * Valid range: -200mV to 0mV
 */
static ssize_t undervolt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cpu_undervolt_data *data =
		container_of(attr, struct cpu_undervolt_data, undervolt_attr);
	unsigned long flags;
	int val, cpu;

	if (kstrtoint(buf, 10, &val) != 0)
		return -EINVAL;

	/* Validate range: -200mV to 0mV */
	if (val > 0 || val < -200) {
		pr_err("Undervolt offset out of range. Valid: -200 to 0 mV\n");
		return -EINVAL;
	}

	cpu = dev->id;
	if (cpu < 0 || cpu >= NR_CPUS)
		return -EINVAL;

	spin_lock_irqsave(&data->lock, flags);
	cpu_undervolt[cpu]->undervolt_offset_mv = val;
	spin_unlock_irqrestore(&data->lock, flags);

	pr_info("CPU%d undervolt offset set to %d mV\n", cpu, val);
	return count;
}

/*
 * Apply undervolt offset to voltage value
 * @volt: Original voltage in microvolts (uV)
 * @offset_mv: Offset in millivolts (negative value reduces voltage)
 * Returns adjusted voltage
 */
unsigned long qcom_apply_undervolt(unsigned int cpu, unsigned long volt)
{
	struct cpu_undervolt_data *data;
	unsigned long flags;
	int offset_mv;
	long new_volt;

	if (cpu >= NR_CPUS || !cpu_undervolt[cpu])
		return volt;

	data = cpu_undervolt[cpu];
	spin_lock_irqsave(&data->lock, flags);
	offset_mv = data->undervolt_offset_mv;
	spin_unlock_irqrestore(&data->lock, flags);

	if (offset_mv == 0)
		return volt;

	new_volt = volt + (offset_mv * 1000);
	
	/* Ensure voltage doesn't go negative */
	if (new_volt < 0)
		return volt;

	return (unsigned long)new_volt;
}
EXPORT_SYMBOL(qcom_apply_undervolt);

/*
 * Get current undervolt offset for CPU
 */
int qcom_get_undervolt_offset(unsigned int cpu)
{
	if (cpu >= NR_CPUS || !cpu_undervolt[cpu])
		return 0;

	return cpu_undervolt[cpu]->undervolt_offset_mv;
}
EXPORT_SYMBOL(qcom_get_undervolt_offset);

/*
 * Initialize undervolt sysfs attribute for a CPU
 */
static int undervolt_register_cpu(unsigned int cpu)
{
	struct cpu_undervolt_data *data;
	struct device *cpu_dev;
	int ret;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev)
		return -ENODEV;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->undervolt_offset_mv = 0;
	spin_lock_init(&data->lock);

	sysfs_attr_init(&data->undervolt_attr.attr);
	data->undervolt_attr.attr.name = "undervolt";
	data->undervolt_attr.attr.mode = 0644;
	data->undervolt_attr.show = undervolt_show;
	data->undervolt_attr.store = undervolt_store;

	ret = device_create_file(cpu_dev, &data->undervolt_attr);
	if (ret) {
		pr_err("Failed to create undervolt sysfs for CPU%d\n", cpu);
		kfree(data);
		return ret;
	}

	cpu_undervolt[cpu] = data;
	pr_info("Undervolt interface registered for CPU%d\n", cpu);
	return 0;
}

/*
 * Unregister undervolt sysfs attribute
 */
static void undervolt_unregister_cpu(unsigned int cpu)
{
	struct device *cpu_dev;

	if (cpu >= NR_CPUS || !cpu_undervolt[cpu])
		return;

	cpu_dev = get_cpu_device(cpu);
	if (cpu_dev)
		device_remove_file(cpu_dev, &cpu_undervolt[cpu]->undervolt_attr);

	kfree(cpu_undervolt[cpu]);
	cpu_undervolt[cpu] = NULL;
}

static int __init qcom_undervolt_init(void)
{
	unsigned int cpu;
	int ret;

	pr_info("Initializing QCOM CPUFreq HW Undervolt module\n");

	for_each_possible_cpu(cpu) {
		ret = undervolt_register_cpu(cpu);
		if (ret)
			pr_warn("Failed to register undervolt for CPU%d: %d\n", cpu, ret);
	}

	return 0;
}

static void __exit qcom_undervolt_exit(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu)
		undervolt_unregister_cpu(cpu);

	pr_info("QCOM CPUFreq HW Undervolt module unloaded\n");
}

module_init(qcom_undervolt_init);
module_exit(qcom_undervolt_exit);

MODULE_DESCRIPTION("QCOM CPUFreq HW Undervolt Sysfs Interface");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Antaru74");
