/* Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/log2.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include "smb-reg.h"
#include "smb-lib.h"
#include "fact-smb-limitation.h"
#include "storm-watch.h"
#include <linux/pmic-voter.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/qpnp/qpnp-adc.h>

#define HIGHRANGE	80
#define LOWRANGE	60

static int BatteryTestStatus_enable;
static ssize_t factory_limitation_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(int), "%d\n", BatteryTestStatus_enable);
}
static ssize_t factory_limitation_status_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{

	int err = kstrtoint(buf, 10, &BatteryTestStatus_enable);
	if (err < 0)
		pr_err("Couldn't get new BatteryTestStatus_enable %d\n", err);

	return size;
}

static struct device_attribute attrs[] = {
	__ATTR(BatteryTestStatus, 0664,
	factory_limitation_status_show, factory_limitation_status_store),
};

void factory_limitation_execute(struct smb_charger *chip, int batt_capacity) {
	int rc, usb_present;
	union power_supply_propval pval = {0, };

	rc = smblib_get_prop_usb_present(chip, &pval);
	if (rc < 0)
		pr_err("Couldn't get usb present rc=%d\n", rc);

	rc = -ENODATA;
	usb_present = pval.intval;

	pr_debug("%s : BatteryTestStatus_enable = %d usb_present = %d batt_capacity = %d\n", __func__, BatteryTestStatus_enable, usb_present, batt_capacity);

	if (usb_present && !BatteryTestStatus_enable) {
		pr_debug("%s : usb_present && !BatteryTestStatus_enable\n", __func__);
		rc = smblib_set_usb_suspend(chip, false);
		if (rc)
			dev_err(chip->dev,
				"Couldn't enable charge rc=%d\n", rc);
		return;
	} else if (!usb_present || !BatteryTestStatus_enable) {
		pr_debug("%s : !usb_present || !BatteryTestStatus_enable\n", __func__);
		return;
	}

	if (batt_capacity >= HIGHRANGE) {
		pr_debug("%s : smbcharge_get_prop_batt_capacity >= HIGHRANGE\n", __func__);
		rc = smblib_set_usb_suspend(chip, true);
		if (rc)
			dev_err(chip->dev, "Couldn't disable charge rc=%d\n", rc);
	} else if (batt_capacity <= LOWRANGE) {
		pr_debug("%s : smbcharge_get_prop_batt_capacity <= LOWRANGE\n", __func__);
		rc = smblib_set_usb_suspend(chip, false);
		if (rc)
			dev_err(chip->dev, "Couldn't enable charge rc=%d\n", rc);
	}
}

void factory_limitation_init(void *kobj) {
	unsigned char attr_count;
	int rc;

	BatteryTestStatus_enable = 1;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		rc = sysfs_create_file(kobj, &attrs[attr_count].attr);
		if (rc < 0) {
			sysfs_remove_file(kobj, &attrs[attr_count].attr);
		}
	}
}

void factory_limitation_remove(void *kobj) {
	unsigned char attr_count;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++)
		sysfs_remove_file(kobj, &attrs[attr_count].attr);
}

MODULE_DESCRIPTION("Factory Limitation Driver");
MODULE_LICENSE("GPL v2");
