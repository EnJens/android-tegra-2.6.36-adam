/*
 * Support for TI bq24610 (bqTINY-II) Dual Input (USB/AC Adpater)
 * 1-Cell Li-Ion Charger connected via GPIOs.
 *
 * Copyright (c) 2008 Philipp Zabel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

struct regulator_init_data;

/**
 * bq24022_mach_info - platform data for bq24610
 * @gpio_nce: GPIO line connected to the nCE pin, used to enable / disable charging
 */
struct bq24022_mach_info {
	int gpio_nce;
	struct regulator_init_data *init_data;
};
