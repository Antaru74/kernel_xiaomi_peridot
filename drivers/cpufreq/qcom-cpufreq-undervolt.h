// SPDX-License-Identifier: GPL-2.0
/*
 * QCOM CPUFreq HW Undervolt Header
 */

#ifndef __QCOM_CPUFREQ_UNDERVOLT_H__
#define __QCOM_CPUFREQ_UNDERVOLT_H__

#include <linux/types.h>

/*
 * Apply undervolt offset to voltage value
 */
unsigned long qcom_apply_undervolt(unsigned int cpu, unsigned long volt);

/*
 * Get current undervolt offset for CPU (in mV)
 */
int qcom_get_undervolt_offset(unsigned int cpu);

#endif /* __QCOM_CPUFREQ_UNDERVOLT_H__ */
