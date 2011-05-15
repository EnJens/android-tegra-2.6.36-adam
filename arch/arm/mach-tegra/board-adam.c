/*
 * arch/arm/mach-tegra/board-harmony.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/console.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/usb/android_composite.h>
#include <linux/input.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include <mach/audio.h>
#include <mach/i2s.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/nand.h>
#include <mach/clk.h>
#include <mach/usb_phy.h>
#include <mach/suspend.h>

#include "clock.h"
#include "board.h"
#include "board-adam.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "wakeups-t2.h"


/* NVidia bootloader tags */
#define ATAG_NVIDIA		0x41000801

#define ATAG_NVIDIA_RM			0x1
#define ATAG_NVIDIA_DISPLAY		0x2
#define ATAG_NVIDIA_FRAMEBUFFER		0x3
#define ATAG_NVIDIA_CHIPSHMOO		0x4
#define ATAG_NVIDIA_CHIPSHMOOPHYS	0x5
#define ATAG_NVIDIA_PRESERVED_MEM_0	0x10000
#define ATAG_NVIDIA_PRESERVED_MEM_N	2
#define ATAG_NVIDIA_FORCE_32		0x7fffffff

//static char *usb_functions[] = { "mtp" };
//static char *usb_functions_adb[] = { "mtp", "adb" };

static char *usb_functions[] = { "mtp", "usb_mass_storage" };
static char *usb_functions_adb[] = { "mtp", "adb", "usb_mass_storage" };

static char *tegra_android_functions_ums[] = {
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	"usb_mass_storage",
#endif
};

static char *tegra_android_functions_ums_adb[] = {
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	"usb_mass_storage",
#endif
#ifdef CONFIG_USB_ANDROID_ADB
	"adb",
#endif
};

static char *tegra_android_functions_all[] = {
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	"usb_mass_storage",
#endif
#ifdef CONFIG_USB_ANDROID_ADB
	"adb",
#endif
};

static struct android_usb_product usb_products[] = {
/*{
	[0] = {
		.product_id = 0x7100,
		.num_functions = ARRAY_SIZE(tegra_android_functions_ums),
		.functions = tegra_android_functions_ums,
	},
	[1] = {
		.product_id = 0x7100,
		.num_functions = ARRAY_SIZE(tegra_android_functions_ums_adb),
		.functions = tegra_android_functions_ums_adb,
	},

	[2] = {
		.product_id = 0x7102,
		.num_functions = ARRAY_SIZE(tegra_android_functions_rndis),
		.functions = tegra_android_functions_rndis,
	},
	[3] = {
		.product_id = 0x7103,
		.num_functions = ARRAY_SIZE(tegra_android_functions_rndis_adb),
		.functions = tegra_android_functions_rndis_adb,
	},
*/
	{
		.product_id     = 0x7100,
		.num_functions  = ARRAY_SIZE(usb_functions_adb),
		.functions      = usb_functions_adb,
	},
	{
		.product_id     = 0x7102,
		.num_functions  = ARRAY_SIZE(usb_functions),
		.functions      = usb_functions,
	},

};

/* standard android USB platform data */
static struct android_usb_platform_data andusb_plat = {
	.vendor_id = 0x955,
	.product_id = 0x7100,
	.manufacturer_name = "NVIDIA",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(tegra_android_functions_all),
	.functions = tegra_android_functions_all,
	/*
	.vendor_id              = 0x0955,
	.product_id             = 0x7100,
	.manufacturer_name      = "NVIDIA",
	.product_name           = "Vega",
	.serial_number          = NULL,
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_adb),
	.functions = usb_functions_adb,
*/
};

static struct platform_device androidusb_device = {
	.name   = "android_usb",
	.id     = -1,
	.dev    = {
		.platform_data  = &andusb_plat,
	},
};

#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
static struct usb_mass_storage_platform_data tegra_usb_fsg_platform = {
	.vendor = "NVIDIA",
	.product = "Tegra 2",
	.nluns = 1,
//	.buf_size = 16384,
	.release        = 0xffff,
};
static struct platform_device tegra_usb_fsg_device = {
	.name = "usb_mass_storage",
	.id = -1,
	.dev = {
		.platform_data = &tegra_usb_fsg_platform,
	},
};
#endif

struct tag_tegra {
	__u32 bootarg_key;
	__u32 bootarg_len;
	char bootarg[1];
};

static int __init parse_tag_nvidia(const struct tag *tag)
{
	return 0;
}
__tagtable(ATAG_NVIDIA, parse_tag_nvidia);

static struct tegra_utmip_config utmi_phy_config = {
	.hssync_start_delay = 0,
	.idle_wait_delay = 17,
	.elastic_limit = 16,
	.term_range_adj = 6,
	.xcvr_setup = 9,
	.xcvr_lsfslew = 2,
	.xcvr_lsrslew = 2,
};

static struct tegra_ehci_platform_data tegra_ehci_pdata = {
	.phy_config = &utmi_phy_config,
	.operating_mode = TEGRA_USB_HOST,
	.power_down_on_bus_suspend = 1,
};

static struct tegra_nand_chip_parms nand_chip_parms[] = {
	/* Samsung K5E2G1GACM */
	[0] = {
		.vendor_id   = 0xEC,
		.device_id   = 0xAA,
		.capacity    = 256,
		.timing      = {
			.trp		= 21,
			.trh		= 15,
			.twp		= 21,
			.twh		= 15,
			.tcs		= 31,
			.twhr		= 60,
			.tcr_tar_trr	= 20,
			.twb		= 100,
			.trp_resp	= 30,
			.tadl		= 100,
		},
	},
	/* Hynix HY27UG088G */
	[1] = {
		.vendor_id   = 0xAD,
		.device_id   = 0xDC,
		.capacity    = 512,
		.timing      = {
			.trp		= 12,
			.trh		= 15,
			.twp		= 12,
			.twh		= 10,
			.tcs		= 20,
			.twhr		= 60,
			.tcr_tar_trr	= 20,
			.twb		= 100,
			.trp_resp	= 12,
			.tadl		= 70,
		},
	},
};

struct tegra_nand_platform harmony_nand_data = {
	.max_chips	= 8,
	.chip_parms	= nand_chip_parms,
	.nr_chip_parms  = ARRAY_SIZE(nand_chip_parms),
};

static struct resource resources_nand[] = {
	[0] = {
		.start  = INT_NANDFLASH,
		.end    = INT_NANDFLASH,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_nand_device = {
	.name           = "tegra_nand",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(resources_nand),
	.resource       = resources_nand,
	.dev            = {
		.platform_data = &harmony_nand_data,
	},
};

static struct plat_serial8250_port debug_uart_platform_data[] = {
	{
		.membase	= IO_ADDRESS(TEGRA_UARTD_BASE),
		.mapbase	= TEGRA_UARTD_BASE,
		.irq		= INT_UARTD,
		.flags		= UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE,
		.type		= PORT_TEGRA,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
//		.uartclk	= 1843200,
	}, {
		.flags		= 0
	}
};

static struct platform_device debug_uart = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uart_platform_data,
	},
};


// TODO: Add suspend support!
static void harmony_debug_uart_init()
{
	struct clk *c = NULL;
	c = clk_get_sys("uart.3", NULL);
	clk_set_rate(c, 115200*16);
	clk_enable(c);
	debug_uart_platform_data[0].uartclk = clk_get_rate(c);
	platform_device_register(&debug_uart);
};

static struct tegra_i2c_platform_data harmony_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup	= TEGRA_PINGROUP_DDC,
	.func		= TEGRA_MUX_I2C2,
};

static const struct tegra_pingroup_config i2c2_gen2 = {
	.pingroup	= TEGRA_PINGROUP_PTA,
	.func		= TEGRA_MUX_I2C2,
};

static struct tegra_i2c_platform_data harmony_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 2,
	.bus_clk_rate	= { 100000, 100000 },
	.bus_mux	= { &i2c2_ddc, &i2c2_gen2 },
	.bus_mux_len	= { 1, 1 },
};

static struct tegra_i2c_platform_data harmony_i2c3_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_i2c_platform_data harmony_dvc_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.is_dvc		= true,
};

static struct i2c_board_info __initdata harmony_i2c_bus1_board_info[] = {
	{
		I2C_BOARD_INFO("alcxxxx", 0x34),
	},
};

static struct tegra_audio_platform_data tegra_audio_pdata = {
	.i2s_master	= false,
	.dsp_master	= false,
	.dma_on		= true,  /* use dma by default */
	.i2s_clk_rate	= 240000000,
	.dap_clk	= "clk_dev1",
	.audio_sync_clk = "audio_2x",
	.mode		= I2S_BIT_FORMAT_I2S,
	.fifo_fmt	= I2S_FIFO_16_LSB,
	.bit_size	= I2S_BIT_SIZE_16,
};


static struct i2c_board_info __initdata  at168_device[] = {
	{
		I2C_BOARD_INFO("at168_i2c", 0x5c),
	},
};

/*static struct i2c_board_info __initdata tp_i2c_device1[] = {
	{
		I2C_BOARD_INFO("egalax_i2c", 0x04),
	},
};

static struct i2c_board_info __initdata tp_i2c_device2[] = {
	{
		I2C_BOARD_INFO("IT7260", 0x46)
	},
};

static struct i2c_board_info __initdata t20_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("bma150", 0x38),
	},
};
*/
static void harmony_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &harmony_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &harmony_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &harmony_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &harmony_dvc_platform_data;

	platform_device_register(&tegra_i2c_device1);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device4);

//	i2c_register_board_info(0, harmony_i2c_bus1_board_info,
//				ARRAY_SIZE(harmony_i2c_bus1_board_info));
	
	//Ant Start
	// G-sensor
/*	i2c_register_board_info(0, t20_i2c_boardinfo,
							ARRAY_SIZE(t20_i2c_boardinfo));
	i2c_register_board_info(3, tp_i2c_device1, ARRAY_SIZE(tp_i2c_device1));
	i2c_register_board_info(3, tp_i2c_device2, ARRAY_SIZE(tp_i2c_device2));	*/
	
}

#ifdef CONFIG_KEYBOARD_GPIO
#define GPIO_KEY(_id, _gpio, _iswake)           \
        {                                       \
                .code = _id,                    \
                .gpio = TEGRA_GPIO_##_gpio,     \
                .active_low = 1,                \
                .desc = #_id,                   \
                .type = EV_KEY,                 \
                .wakeup = _iswake,              \
                .debounce_interval = 10,        \
        }

static struct gpio_keys_button harmony_keys[] = {
        [0] = GPIO_KEY(KEY_VOLUMEUP, PD4, 0),
        [1] = GPIO_KEY(KEY_VOLUMEDOWN, PV4, 0),
        [3] = GPIO_KEY(KEY_BACK, PH0, 0),
};

#define PMC_WAKE_STATUS 0x14

static int harmony_wakeup_key(void)
{
        unsigned long status =
                readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS);

        return status & TEGRA_WAKE_GPIO_PV2 ? KEY_POWER : KEY_RESERVED;
}

static struct gpio_keys_platform_data harmony_keys_platform_data = {
        .buttons        = harmony_keys,
        .nbuttons       = ARRAY_SIZE(harmony_keys),
        .wakeup_key     = harmony_wakeup_key,
};

static struct platform_device harmony_keys_device = {
        .name   = "gpio-keys",
        .id     = 0,
        .dev    = {
                .platform_data  = &harmony_keys_platform_data,
        },
};

static void harmony_keys_init(void)
{
        int i;

        for (i = 0; i < ARRAY_SIZE(harmony_keys); i++)
                tegra_gpio_enable(harmony_keys[i].gpio);
}

#endif

#ifdef CONFIG_HARMONY_GPS_CONTROL

#endif

static struct platform_device *harmony_devices[] __initdata = {
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	&tegra_usb_fsg_device,
#endif
	&androidusb_device,
	&debug_uart,
	&pmu_device,
	&tegra_nand_device,
	&tegra_udc_device,
#ifdef CONFIG_KEYBOARD_GPIO
        &harmony_keys_device,
#endif
	&tegra_ehci3_device,
	&tegra_spi_device1,
	&tegra_spi_device2,
	&tegra_spi_device3,
	&tegra_spi_device4,
	&tegra_gart_device,
	&tegra_i2s_device1,
};

static void __init tegra_harmony_fixup(struct machine_desc *desc,
	struct tag *tags, char **cmdline, struct meminfo *mi)
{
	mi->nr_banks = 2;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size = 448 * SZ_1M;
	mi->bank[1].start = SZ_512M;
	mi->bank[1].size = SZ_512M;
}

static __initdata struct tegra_clk_init_table harmony_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "clk_dev1",	NULL,		26000000,	true},
	{ "clk_m",	NULL,		12000000,	true},
	{ "blink",      "clk_32k",      32768,          false},
	{ "3d",		"pll_m",	266400000,	true},
	{ "2d",		"pll_m",	266400000,	true},
	{ "vi",		"pll_m",	50000000,	true},
	{ "vi_sensor",	"pll_m",	111000000,	false},
	{ "epp",	"pll_m",	266400000,	true},
	{ "mpe",	"pll_m",	111000000,	false},
	{ "emc",	"pll_m",	666000000,	true},
	{ "pll_c",	"clk_m",	600000000,	true},
	{ "pll_c_out1",	"pll_c",	240000000,	true},
	{ "vde",	"pll_c",	240000000,	false},
	{ "pll_p",	"clk_m",	216000000,	true},
	{ "pll_p_out1",	"pll_p",	28800000,	true},
	{ "pll_a",	"pll_p_out1",	56448000,	true},
	{ "pll_a_out0",	"pll_a",	11289600,	true},
	{ "i2s1",	"pll_a_out0",	11289600,	true},
	{ "audio",	"pll_a_out0",	11289600,	true},
	{ "audio_2x",	"audio",	22579200,	false},
	{ "pll_p_out2",	"pll_p",	48000000,	true},
	{ "pll_p_out3",	"pll_p",	72000000,	true},
	{ "i2c1_i2c",	"pll_p_out3",	72000000,	true},
	{ "i2c2_i2c",	"pll_p_out3",	72000000,	true},
	{ "i2c3_i2c",	"pll_p_out3",	72000000,	true},
	{ "dvc_i2c",	"pll_p_out3",	72000000,	true},
	{ "csi",	"pll_p_out3",	72000000,	false},
	{ "pll_p_out4",	"pll_p",	108000000,	true},
	{ "sclk",	"pll_p_out4",	108000000,	true},
	{ "hclk",	"sclk",		108000000,	true},
	{ "pclk",	"hclk",		54000000,	true},
	{ "apbdma",	"hclk",		54000000,	true},
	{ "spdif_in",	"pll_p",	36000000,	false},
	{ "csite",	"pll_p",	144000000,	true},
	{ "uartd",	"pll_p",	216000000,	true},
	{ "host1x",	"pll_p",	144000000,	true},
	{ "disp1",	"pll_p",	216000000,	true},
	{ "pll_d",	"clk_m",	1000000,	false},
	{ "pll_d_out0",	"pll_d",	500000,		false},
	{ "dsi",	"pll_d",	1000000,	false},
	{ "pll_u",	"clk_m",	480000000,	true},
	{ "clk_d",	"clk_m",	24000000,	true},
	{ "timer",	"clk_m",	12000000,	true},
	{ "i2s2",	"clk_m",	11289600,	false},
	{ "spdif_out",	"clk_m",	12000000,	false},
	{ "spi",	"clk_m",	12000000,	false},
	{ "xio",	"clk_m",	12000000,	false},
	{ "twc",	"clk_m",	12000000,	false},
	{ "sbc1",	"clk_m",	12000000,	false},
	{ "sbc2",	"clk_m",	12000000,	false},
	{ "sbc3",	"clk_m",	12000000,	false},
	{ "sbc4",	"clk_m",	12000000,	false},
	{ "ide",	"clk_m",	12000000,	false},
	{ "ndflash",	"clk_m",	108000000,	true},
	{ "vfir",	"clk_m",	12000000,	false},
	{ "sdmmc1",	"clk_m",	48000000,	true},
	{ "sdmmc2",	"clk_m",	48000000,	true},
	{ "sdmmc3",	"clk_m",	48000000,	true},
	{ "sdmmc4",	"clk_m",	48000000,	true},
	{ "la",		"clk_m",	12000000,	false},
	{ "owr",	"clk_m",	12000000,	false},
	{ "nor",	"clk_m",	12000000,	false},
	{ "mipi",	"clk_m",	12000000,	false},
	{ "i2c1",	"clk_m",	3000000,	false},
	{ "i2c2",	"clk_m",	3000000,	false},
	{ "i2c3",	"clk_m",	3000000,	false},
	{ "dvc",	"clk_m",	3000000,	false},
	{ "uarta",	"clk_m",	12000000,	false},
	{ "uartb",	"clk_m",	12000000,	false},
	{ "uartc",	"clk_m",	12000000,	false},
	{ "uarte",	"clk_m",	12000000,	false},
	{ "cve",	"clk_m",	12000000,	false},
	{ "tvo",	"clk_m",	12000000,	false},
	{ "hdmi",	"clk_m",	12000000,	false},
	{ "tvdac",	"clk_m",	12000000,	false},
	{ "disp2",	"clk_m",	12000000,	false},
	{ "usbd",	"clk_m",	12000000,	false},
	{ "usb2",	"clk_m",	12000000,	false},
	{ "usb3",	"clk_m",	12000000,	true},
	{ "isp",	"clk_m",	12000000,	false},
	{ "csus",	"clk_m",	12000000,	false},
	{ "pwm",	"clk_32k",	32768,		false},
	{ "clk_32k",	NULL,		32768,		true},
	{ "pll_s",	"clk_32k",	32768,		false},
	{ "rtc",	"clk_32k",	32768,		true},
	{ "kbc",	"clk_32k",	32768,		true},
	{ NULL,		NULL,		0,		0},
};

static struct tegra_suspend_platform_data harmony_suspend = {
	.cpu_timer = 5000,
	.cpu_off_timer = 5000,
	.core_timer = 0x7e7e,
	.core_off_timer = 0x7f,
	.separate_req = true,
	.corereq_high = false,
	.sysclkreq_high = true,
	.suspend_mode = TEGRA_SUSPEND_LP0,
};

static void __init tegra_harmony_init(void)
{
	tegra_common_init();

	tegra_init_suspend(&harmony_suspend);

	tegra_clk_init_from_table(harmony_clk_init_table);

	harmony_pinmux_init();

	tegra_ehci3_device.dev.platform_data = &tegra_ehci_pdata;

	tegra_i2s_device1.dev.platform_data = &tegra_audio_pdata;

	platform_add_devices(harmony_devices, ARRAY_SIZE(harmony_devices));

	harmony_panel_init();
	harmony_sdhci_init();
	harmony_i2c_init();
	harmony_power_init();
	harmony_keys_init();
	/* Temporary hack to enable gps/memsic sensors. Move to proper initialization */
/*	tegra_gpio_enable(TEGRA_GPIO_PV3);
	gpio_request(TEGRA_GPIO_PV3, "gps_control");
	gpio_direction_output(TEGRA_GPIO_PV3, 0);
	gpio_set_value(TEGRA_GPIO_PV3, 1);*/
//	harmony_debug_uart_init();
}

MACHINE_START(HARMONY, "harmony")
	.boot_params  = 0x00000100,
	.phys_io        = IO_APB_PHYS,
	.io_pg_offst    = ((IO_APB_VIRT) >> 18) & 0xfffc,
	.fixup		= tegra_harmony_fixup,
	.init_irq       = tegra_init_irq,
	.init_machine   = tegra_harmony_init,
	.map_io         = tegra_map_common_io,
	.timer          = &tegra_timer,
MACHINE_END
