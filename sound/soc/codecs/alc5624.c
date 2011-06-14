/*
 * alc5624.c  --  ALC5624 ALSA Soc Audio driver
 *
 * Copyright 2011 Eduardo José Tagle <ejtagle@tutopia.com>
 * Based on rt5624.c , Copyright 2008 Realtek Microelectronics flove <flove@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/soc.h> 
#include <sound/initval.h>
#include <sound/alc5624.h>

/* Index of Codec Register definitions */
#define ALC5624_RESET					0x00	/* RESET CODEC TO DEFAULT */
#define ALC5624_SPK_OUT_VOL				0x02	/* SPEAKER OUT VOLUME */
#define ALC5624_HP_OUT_VOL				0x04	/* HEADPHONE OUTPUT VOLUME */
#define ALC5624_PHONEIN_MONO_OUT_VOL	0x08	/* PHONE INPUT/MONO OUTPUT VOLUME */
#define ALC5624_LINE_IN_VOL				0x0A	/* LINE IN VOLUME */
#define ALC5624_STEREO_DAC_VOL			0x0C	/* STEREO DAC VOLUME */
#define ALC5624_MIC_VOL					0x0E	/* MICROPHONE VOLUME */
#define ALC5624_MIC_ROUTING_CTRL		0x10	/* MIC ROUTING CONTROL */
#define ALC5624_ADC_REC_GAIN			0x12	/* ADC RECORD GAIN */
#define ALC5624_ADC_REC_MIXER			0x14	/* ADC RECORD MIXER CONTROL */
#define ALC5624_OUTPUT_MIXER_CTRL		0x1C	/* OUTPUT MIXER CONTROL */
#define ALC5624_MIC_CTRL				0x22	/* MICROPHONE CONTROL */
#define ALC5624_PD_CTRL_STAT			0x26	/* POWER DOWN CONTROL/STATUS */
#define	ALC5624_MAIN_SDP_CTRL			0x34	/* MAIN SERIAL DATA PORT CONTROL(STEREO I2S) */
#define	ALC5624_PWR_MANAG_ADD1			0x3A	/* POWER MANAGMENT ADDITION 1 */
#define ALC5624_PWR_MANAG_ADD2			0x3C	/* POWER MANAGMENT ADDITION 2 */
#define ALC5624_PWR_MANAG_ADD3			0x3E	/* POWER MANAGMENT ADDITION 3 */
#define ALC5624_GEN_CTRL_REG1			0x40	/* GENERAL PURPOSE CONTROL REGISTER 1 */
#define	ALC5624_GEN_CTRL_REG2			0x42	/* GENERAL PURPOSE CONTROL REGISTER 2 */
#define ALC5624_PLL_CTRL				0x44	/* PLL CONTROL */
#define ALC5624_GPIO_PIN_CONFIG			0x4C	/* GPIO PIN CONFIGURATION */
#define ALC5624_GPIO_PIN_POLARITY		0x4E	/* GPIO PIN POLARITY	 */
#define ALC5624_GPIO_PIN_STICKY			0x50	/* GPIO PIN STICKY	 */
#define ALC5624_GPIO_PIN_WAKEUP			0x52	/* GPIO PIN WAKE UP */
#define ALC5624_GPIO_PIN_STATUS			0x54	/* GPIO PIN STATUS */
#define ALC5624_GPIO_PIN_SHARING		0x56	/* GPIO PIN SHARING */
#define	ALC5624_OVER_TEMP_CURR_STATUS	0x58	/* OVER TEMPERATURE AND CURRENT STATUS */
#define ALC5624_GPIO_OUT_CTRL			0x5C	/* GPIO OUTPUT PIN CONTRL */
#define ALC5624_MISC_CTRL				0x5E	/* MISC CONTROL */
#define	ALC5624_STEREO_DAC_CLK_CTRL1	0x60	/* STEREO DAC CLOCK CONTROL 1 */
#define ALC5624_STEREO_DAC_CLK_CTRL2	0x62	/* STEREO DAC CLOCK CONTROL 2 */
#define ALC5624_PSEUDO_SPATIAL_CTRL		0x68	/* PSEDUEO STEREO /SPATIAL EFFECT BLOCK CONTROL */
#define ALC5624_INDEX_ADDRESS			0x6A	/* INDEX ADDRESS */
#define ALC5624_INDEX_DATA				0x6C	/* INDEX DATA  */
#define ALC5624_EQ_STATUS				0x6E	/* EQ STATUS */
#define ALC5624_VENDOR_ID1	  		    0x7C	/* VENDOR ID1 */
#define ALC5624_VENDOR_ID2	  		    0x7E	/* VENDOR ID2 */

/*** Bit define of Codec Register *** */

/* global definition */
#define RT_L_MUTE						(0x1<<15)		/* Mute Left Control */
#define RT_L_ZC							(0x1<<14)		/* Mute Left Zero-Cross Detector Control */
#define RT_R_MUTE						(0x1<<7)		/* Mute Right Control */
#define RT_R_ZC							(0x1<<6)		/* Mute Right Zero-Cross Detector Control */
#define RT_M_HP_MIXER					(0x1<<15)		/* Mute source to HP Mixer */
#define RT_M_SPK_MIXER					(0x1<<14)		/* Mute source to Speaker Mixer */
#define RT_M_MONO_MIXER					(0x1<<13)		/* Mute source to Mono Mixer */

/* Phone Input/MONO Output Volume(0x08) */
#define M_PHONEIN_TO_HP_MIXER			(0x1<<15)		/* Mute Phone In volume to HP mixer */
#define M_PHONEIN_TO_SPK_MIXER			(0x1<<14)		/* Mute Phone In volume to speaker mixer */
#define M_MONO_OUT_VOL					(0x1<<7)		/* Mute Mono output volume */


/* Mic Routing Control(0x10) */
#define M_MIC1_TO_HP_MIXER				(0x1<<15)		/* Mute MIC1 to HP mixer */
#define M_MIC1_TO_SPK_MIXER				(0x1<<14)		/* Mute MiC1 to SPK mixer */
#define M_MIC1_TO_MONO_MIXER			(0x1<<13)		/* Mute MIC1 to MONO mixer */
#define MIC1_DIFF_INPUT_CTRL			(0x1<<12)		/* MIC1 different input control */
#define M_MIC2_TO_HP_MIXER				(0x1<<7)		/* Mute MIC2 to HP mixer */
#define M_MIC2_TO_SPK_MIXER				(0x1<<6)		/* Mute MiC2 to SPK mixer */
#define M_MIC2_TO_MONO_MIXER			(0x1<<5)		/* Mute MIC2 to MONO mixer */
#define MIC2_DIFF_INPUT_CTRL			(0x1<<4)		/* MIC2 different input control */

/* ADC Record Gain(0x12) */
#define M_ADC_L_TO_HP_MIXER				(0x1<<15)		/* Mute left of ADC to HP Mixer */
#define M_ADC_R_TO_HP_MIXER				(0x1<<14)		/* Mute right of ADC to HP Mixer */
#define M_ADC_L_TO_MONO_MIXER			(0x1<<13)		/* Mute left of ADC to MONO Mixer */
#define M_ADC_R_TO_MONO_MIXER			(0x1<<12)		/* Mute right of ADC to MONO Mixer */
#define ADC_L_GAIN_MASK					(0x1f<<7)		/* ADC Record Gain Left channel Mask */
#define ADC_L_ZC_DET					(0x1<<6)		/* ADC Zero-Cross Detector Control */
#define ADC_R_ZC_DET					(0x1<<5)		/* ADC Zero-Cross Detector Control */
#define ADC_R_GAIN_MASK					(0x1f<<0)		/* ADC Record Gain Right channel Mask */

/* Output Mixer Control(0x1C) */
#define	SPKL_INPUT_SEL_MASK				(0x3<<14)
#define	SPKL_INPUT_SEL_MONO				(0x3<<14)
#define	SPKL_INPUT_SEL_SPK				(0x2<<14)
#define	SPKL_INPUT_SEL_HPL				(0x1<<14)
#define	SPKL_INPUT_SEL_VMID				(0x0<<14)

#define SPK_OUT_SEL_CLASS_D				(0x1<<13)
#define SPK_OUT_SEL_CLASS_AB			(0x0<<13)

#define	SPKR_INPUT_SEL_MASK				(0x3<<11)
#define	SPKR_INPUT_SEL_MONO_MIXER		(0x3<<11)
#define	SPKR_INPUT_SEL_SPK_MIXER		(0x2<<11)
#define	SPKR_INPUT_SEL_HPR_MIXER		(0x1<<11)
#define	SPKR_INPUT_SEL_VMID				(0x0<<11)

#define HPL_INPUT_SEL_HPL_MIXER			(0x1<<9)
#define HPR_INPUT_SEL_HPR_MIXER			(0x1<<8)

#define MONO_INPUT_SEL_MASK				(0x3<<6)
#define MONO_INPUT_SEL_MONO_MIXER		(0x3<<6)
#define MONO_INPUT_SEL_SPK_MIXER		(0x2<<6)
#define MONO_INPUT_SEL_HP_MIXER			(0x1<<6)
#define MONO_INPUT_SEL_VMID				(0x0<<6)

/* Micphone Control define(0x22) */
#define MIC1		1
#define MIC2		2
#define MIC_BIAS_90_PRECNET_AVDD	1
#define	MIC_BIAS_75_PRECNET_AVDD	2

#define MIC1_BOOST_CONTROL_MASK		(0x3<<10)
#define MIC1_BOOST_CONTROL_BYPASS	(0x0<<10)
#define MIC1_BOOST_CONTROL_20DB		(0x1<<10)
#define MIC1_BOOST_CONTROL_30DB		(0x2<<10)
#define MIC1_BOOST_CONTROL_40DB		(0x3<<10)

#define MIC2_BOOST_CONTROL_MASK		(0x3<<8)
#define MIC2_BOOST_CONTROL_BYPASS	(0x0<<8)
#define MIC2_BOOST_CONTROL_20DB		(0x1<<8)
#define MIC2_BOOST_CONTROL_30DB		(0x2<<8)
#define MIC2_BOOST_CONTROL_40DB		(0x3<<8)

#define MIC1_BIAS_VOLT_CTRL_MASK	(0x1<<5)
#define MIC1_BIAS_VOLT_CTRL_90P		(0x0<<5)
#define MIC1_BIAS_VOLT_CTRL_75P		(0x1<<5)

#define MIC2_BIAS_VOLT_CTRL_MASK	(0x1<<4)
#define MIC2_BIAS_VOLT_CTRL_90P		(0x0<<4)
#define MIC2_BIAS_VOLT_CTRL_75P		(0x1<<4)

/* PowerDown control of register(0x26) */
/* power management bits */
#define RT_PWR_PR7					(0x1<<15)	/* write this bit to power down the Speaker Amplifier */
#define RT_PWR_PR6					(0x1<<14)	/* write this bit to power down the Headphone Out and MonoOut */
#define RT_PWR_PR5					(0x1<<13)	/* write this bit to power down the internal clock(without PLL) */
#define RT_PWR_PR3					(0x1<<11)	/* write this bit to power down the mixer(vref/vrefout out off) */
#define RT_PWR_PR2					(0x1<<10)	/* write this bit to power down the mixer(vref/vrefout still on) */
#define RT_PWR_PR1					(0x1<<9) 	/* write this bit to power down the dac */
#define RT_PWR_PR0					(0x1<<8) 	/* write this bit to power down the adc */
#define RT_PWR_REF					(0x1<<3)	/* read only */
#define RT_PWR_ANL					(0x1<<2)	/* read only	 */
#define RT_PWR_DAC					(0x1<<1)	/* read only */
#define RT_PWR_ADC					(0x1)		/* read only */


/* Main Serial Data Port Control(0x34) */
#define MAIN_I2S_MODE_SEL			(0x1<<15)		/* 0:Master mode 1:Slave mode */
#define MAIN_I2S_SADLRCK_CTRL		(0x1<<14)		/* 0:Disable,ADC and DAC use the same fs,1:Enable */
#define MAIN_I2S_BCLK_POLARITY		(0x1<<12)		/* 0:Normal 1:Invert */
#define MAIN_I2S_DA_CLK_SOUR		(0x1<<11)		/* 0:from DA Filter,1:from DA Sigma Delta Clock Divider */

/* I2S DA SIGMA delta clock divider */
#define MAIN_I2S_CLK_DIV_MASK		(0x7<<8)
#define MAIN_I2S_CLK_DIV_2			(0x0<<8)		
#define MAIN_I2S_CLK_DIV_4			(0x1<<8)
#define MAIN_I2S_CLK_DIV_8			(0x2<<8)
#define MAIN_I2S_CLK_DIV_16			(0x3<<8)
#define MAIN_I2S_CLK_DIV_32			(0x4<<8)
#define MAIN_I2S_CLK_DIV_64			(0x5<<8)

#define MAIN_I2S_PCM_MODE			(0x1<<6)		/* PCM    	0:mode A				,1:mode B  */
												 	/* Non PCM	0:Normal SADLRCK/SDALRCK,1:Invert SADLRCK/SDALRCK  */
/* Data Length Slection */
#define MAIN_I2S_DL_MASK			(0x3<<2)		/* main i2s Data Length mask	 */
#define MAIN_I2S_DL_16				(0x0<<2)		/* 16 bits */
#define MAIN_I2S_DL_20				(0x1<<2)		/* 20 bits */
#define	MAIN_I2S_DL_24				(0x2<<2)		/* 24 bits */
#define MAIN_I2S_DL_32				(0x3<<2)		/* 32 bits */

/* PCM Data Format Selection */
#define MAIN_I2S_DF_MASK			(0x3)			/* main i2s Data Format mask */
#define MAIN_I2S_DF_I2S				(0x0)			/* I2S FORMAT  */
#define MAIN_I2S_DF_RIGHT			(0x1)			/* RIGHT JUSTIFIED format */
#define	MAIN_I2S_DF_LEFT			(0x2)			/* LEFT JUSTIFIED  format */
#define MAIN_I2S_DF_PCM				(0x3)			/* PCM format */

/* Power managment addition 1 (0x3A),0:Disable,1:Enable */
#define PWR_HI_R_LOAD_MONO			(0x1<<15)
#define PWR_HI_R_LOAD_HP			(0x1<<14)	
#define PWR_ZC_DET_PD				(0x1<<13)
#define PWR_MAIN_I2S				(0x1<<11)	
#define	PWR_MIC_BIAS1_DET			(0x1<<5)
#define	PWR_MIC_BIAS2_DET			(0x1<<4)
#define PWR_MIC_BIAS1				(0x1<<3)	
#define PWR_MIC_BIAS2				(0x1<<2)	
#define PWR_MAIN_BIAS				(0x1<<1)
#define PWR_DAC_REF					(0x1)


/* Power managment addition 2(0x3C),0:Disable,1:Enable */
#define EN_THREMAL_SHUTDOWN			(0x1<<15)
#define PWR_CLASS_AB				(0x1<<14)
#define PWR_MIXER_VREF				(0x1<<13)
#define PWR_PLL						(0x1<<12)
#define PWR_VOICE_CLOCK				(0x1<<10)
#define PWR_L_DAC_CLK				(0x1<<9)
#define PWR_R_DAC_CLK				(0x1<<8)
#define PWR_L_ADC_CLK_GAIN			(0x1<<7)
#define PWR_R_ADC_CLK_GAIN			(0x1<<6)
#define PWR_L_HP_MIXER				(0x1<<5)
#define PWR_R_HP_MIXER				(0x1<<4)
#define PWR_SPK_MIXER				(0x1<<3)
#define PWR_MONO_MIXER				(0x1<<2)
#define PWR_L_ADC_REC_MIXER			(0x1<<1)
#define PWR_R_ADC_REC_MIXER			(0x1)


/* Power managment addition 3(0x3E),0:Disable,1:Enable */
#define PWR_MONO_VOL				(0x1<<14)
#define PWR_SPK_LN_OUT				(0x1<<13)
#define PWR_SPK_RN_OUT				(0x1<<12)
#define PWR_HP_L_OUT				(0x1<<11)
#define PWR_HP_R_OUT				(0x1<<10)
#define PWR_SPK_L_OUT				(0x1<<9)
#define PWR_SPK_R_OUT				(0x1<<8)
#define PWR_LINE_IN_L				(0x1<<7)
#define PWR_LINE_IN_R				(0x1<<6)
#define PWR_PHONE_MIXER				(0x1<<5)
#define PWR_PHONE_VOL				(0x1<<4)
#define PWR_MIC1_VOL_CTRL			(0x1<<3)
#define PWR_MIC2_VOL_CTRL			(0x1<<2)
#define PWR_MIC1_BOOST				(0x1<<1)
#define PWR_MIC2_BOOST				(0x1)

/* General Purpose Control Register 1(0x40) */
#define GP_CLK_FROM_PLL					(0x1<<15)	/* Clock source from PLL output */
#define GP_CLK_FROM_MCLK				(0x0<<15)	/* Clock source from MCLK output */

#define GP_HP_AMP_CTRL_MASK				(0x3<<8)
#define GP_HP_AMP_CTRL_RATIO_100		(0x0<<8)		/* 1.00 Vdd */
#define GP_HP_AMP_CTRL_RATIO_125		(0x1<<8)		/* 1.25 Vdd */
#define GP_HP_AMP_CTRL_RATIO_150		(0x2<<8)		/* 1.50 Vdd */

#define GP_SPK_D_AMP_CTRL_MASK			(0x3<<6)
#define GP_SPK_D_AMP_CTRL_RATIO_175		(0x0<<6)		/* 1.75 Vdd */
#define GP_SPK_D_AMP_CTRL_RATIO_150		(0x1<<6)		/* 1.50 Vdd	*/
#define GP_SPK_D_AMP_CTRL_RATIO_125		(0x2<<6)		/* 1.25 Vdd */
#define GP_SPK_D_AMP_CTRL_RATIO_100		(0x3<<6)		/* 1.00 Vdd */

#define GP_SPK_AB_AMP_CTRL_MASK			(0x7<<3)
#define GP_SPK_AB_AMP_CTRL_RATIO_225	(0x0<<3)		/* 2.25 Vdd */
#define GP_SPK_AB_AMP_CTRL_RATIO_200	(0x1<<3)		/* 2.00 Vdd */
#define GP_SPK_AB_AMP_CTRL_RATIO_175	(0x2<<3)		/* 1.75 Vdd */
#define GP_SPK_AB_AMP_CTRL_RATIO_150	(0x3<<3)		/* 1.50 Vdd */
#define GP_SPK_AB_AMP_CTRL_RATIO_125	(0x4<<3)		/* 1.25 Vdd	*/
#define GP_SPK_AB_AMP_CTRL_RATIO_100	(0x5<<3)		/* 1.00 Vdd */

/* General Purpose Control Register 2(0x42) */
#define GP2_VOICE_TO_DIG_PATH_EN		(0x1<<15)
#define GP2_CLASS_AB_SEL_DIFF			(0x0<<13)		/* Differential Mode of Class AB */
#define GP2_CLASS_D_SEL_SINGLE			(0x1<<13)		/* Single End mode of Class AB */
#define GP2_PLL_PRE_DIV_1				(0x0<<0)		/* PLL pre-Divider 1 */
#define GP2_PLL_PRE_DIV_2				(0x1<<0)		/* PLL pre-Divider 2 */

/* PLL Control(0x44) */
#define PLL_M_CODE_MASK				0xF				/* PLL M code mask */
#define PLL_K_CODE_MASK				(0x7<<4)		/* PLL K code mask */
#define PLL_BYPASS_N				(0x1<<7)		/* bypass PLL N code */
#define PLL_N_CODE_MASK				(0xFF<<8)		/* PLL N code mask */

#define PLL_CTRL_M_VAL(m)		((m)&0xf)
#define PLL_CTRL_K_VAL(k)		(((k)&0x7)<<4)
#define PLL_CTRL_N_VAL(n)		(((n)&0xff)<<8)

/* GPIO Pin Configuration(0x4C) */
#define GPIO_1						(0x1<<1)
#define	GPIO_2						(0x1<<2)
#define	GPIO_3						(0x1<<3)
#define GPIO_4						(0x1<<4)
#define GPIO_5						(0x1<<5)


/* INTERRUPT CONTROL(0x5E) */
#define DISABLE_FAST_VREG			(0x1<<15)

#define AVC_TARTGET_SEL_MASK		(0x3<<12)
#define	AVC_TARTGET_SEL_NONE		(0x0<<12)
#define	AVC_TARTGET_SEL_R 			(0x1<<12)
#define	AVC_TARTGET_SEL_L			(0x2<<12)
#define	AVC_TARTGET_SEL_BOTH		(0x3<<12)

/* Stereo DAC Clock Control 1(0x60) */
#define STEREO_SCLK_DIV1_MASK		(0xF<<12)
#define STEREO_SCLK_DIV1_1			(0x0<<12)
#define STEREO_SCLK_DIV1_2			(0x1<<12)
#define STEREO_SCLK_DIV1_3			(0x2<<12)
#define STEREO_SCLK_DIV1_4			(0x3<<12)
#define STEREO_SCLK_DIV1_5			(0x4<<12)
#define STEREO_SCLK_DIV1_6			(0x5<<12)
#define STEREO_SCLK_DIV1_7			(0x6<<12)
#define STEREO_SCLK_DIV1_8			(0x7<<12)
#define STEREO_SCLK_DIV1_9			(0x8<<12)
#define STEREO_SCLK_DIV1_10			(0x9<<12)
#define STEREO_SCLK_DIV1_11			(0xA<<12)
#define STEREO_SCLK_DIV1_12			(0xB<<12)
#define STEREO_SCLK_DIV1_13			(0xC<<12)
#define STEREO_SCLK_DIV1_14			(0xD<<12)
#define STEREO_SCLK_DIV1_15			(0xE<<12)
#define STEREO_SCLK_DIV1_16			(0xF<<12)

#define STEREO_SCLK_DIV2_MASK		(0x7<<8)
#define STEREO_SCLK_DIV2_2			(0x0<<8)
#define STEREO_SCLK_DIV2_4			(0x1<<8)
#define STEREO_SCLK_DIV2_8			(0x2<<8)
#define STEREO_SCLK_DIV2_16			(0x3<<8)
#define STEREO_SCLK_DIV2_32			(0x4<<8)

#define STEREO_AD_WCLK_DIV1_MASK	(0xF<<4)
#define STEREO_AD_WCLK_DIV1_1		(0x0<<4)
#define STEREO_AD_WCLK_DIV1_2		(0x1<<4)
#define STEREO_AD_WCLK_DIV1_3		(0x2<<4)
#define STEREO_AD_WCLK_DIV1_4		(0x3<<4)
#define STEREO_AD_WCLK_DIV1_5		(0x4<<4)
#define STEREO_AD_WCLK_DIV1_6		(0x5<<4)
#define STEREO_AD_WCLK_DIV1_7		(0x6<<4)
#define STEREO_AD_WCLK_DIV1_8		(0x7<<4)
#define STEREO_AD_WCLK_DIV1_9		(0x8<<4)
#define STEREO_AD_WCLK_DIV1_10		(0x9<<4)
#define STEREO_AD_WCLK_DIV1_11		(0xA<<4)
#define STEREO_AD_WCLK_DIV1_12		(0xB<<4)
#define STEREO_AD_WCLK_DIV1_13		(0xC<<4)
#define STEREO_AD_WCLK_DIV1_14		(0xD<<4)
#define STEREO_AD_WCLK_DIV1_15		(0xE<<4)
#define STEREO_AD_WCLK_DIV1_16		(0xF<<4)

#define STEREO_AD_WCLK_DIV2_MASK	(0x7<<1)
#define STEREO_AD_WCLK_DIV2_2		(0x0<<1)
#define STEREO_AD_WCLK_DIV2_4		(0x1<<1)
#define STEREO_AD_WCLK_DIV2_8		(0x2<<1)
#define STEREO_AD_WCLK_DIV2_16		(0x3<<1)
#define STEREO_AD_WCLK_DIV2_32		(0x4<<1)

#define STEREO_DA_WCLK_DIV_MASK		(1)
#define STEREO_DA_WCLK_DIV_32		(0)
#define STEREO_DA_WCLK_DIV_64		(1)

/* Stereo DAC Clock Control 2(0x62) */
#define STEREO_DA_FILTER_DIV1_MASK	(0xF<<12)
#define STEREO_DA_FILTER_DIV1_1		(0x0<<12)
#define STEREO_DA_FILTER_DIV1_2		(0x1<<12)
#define STEREO_DA_FILTER_DIV1_3		(0x2<<12)
#define STEREO_DA_FILTER_DIV1_4		(0x3<<12)
#define STEREO_DA_FILTER_DIV1_5		(0x4<<12)
#define STEREO_DA_FILTER_DIV1_6		(0x5<<12)
#define STEREO_DA_FILTER_DIV1_7		(0x6<<12)
#define STEREO_DA_FILTER_DIV1_8		(0x7<<12)
#define STEREO_DA_FILTER_DIV1_9		(0x8<<12)
#define STEREO_DA_FILTER_DIV1_10	(0x9<<12)
#define STEREO_DA_FILTER_DIV1_11	(0xA<<12)
#define STEREO_DA_FILTER_DIV1_12	(0xB<<12)
#define STEREO_DA_FILTER_DIV1_13	(0xC<<12)
#define STEREO_DA_FILTER_DIV1_14	(0xD<<12)
#define STEREO_DA_FILTER_DIV1_15	(0xE<<12)
#define STEREO_DA_FILTER_DIV1_16	(0xF<<12)

#define STEREO_DA_FILTER_DIV2_MASK	(0x7<<9)
#define STEREO_DA_FILTER_DIV2_2		(0x0<<9)
#define STEREO_DA_FILTER_DIV2_4		(0x1<<9)
#define STEREO_DA_FILTER_DIV2_8		(0x2<<9)
#define STEREO_DA_FILTER_DIV2_16	(0x3<<9)
#define STEREO_DA_FILTER_DIV2_32	(0x4<<9)

#define STEREO_AD_FILTER_DIV1_MASK	(0xF<<4)
#define STEREO_AD_FILTER_DIV1_1		(0x0<<4)
#define STEREO_AD_FILTER_DIV1_2		(0x1<<4)
#define STEREO_AD_FILTER_DIV1_3		(0x2<<4)
#define STEREO_AD_FILTER_DIV1_4		(0x3<<4)
#define STEREO_AD_FILTER_DIV1_5		(0x4<<4)
#define STEREO_AD_FILTER_DIV1_6		(0x5<<4)
#define STEREO_AD_FILTER_DIV1_7		(0x6<<4)
#define STEREO_AD_FILTER_DIV1_8		(0x7<<4)
#define STEREO_AD_FILTER_DIV1_9		(0x8<<4)
#define STEREO_AD_FILTER_DIV1_10	(0x9<<4)
#define STEREO_AD_FILTER_DIV1_11	(0xA<<4)
#define STEREO_AD_FILTER_DIV1_12	(0xB<<4)
#define STEREO_AD_FILTER_DIV1_13	(0xC<<4)
#define STEREO_AD_FILTER_DIV1_14	(0xD<<4)
#define STEREO_AD_FILTER_DIV1_15	(0xE<<4)
#define STEREO_AD_FILTER_DIV1_16	(0xF<<4)

#define STEREO_AD_FILTER_DIV2_MASK	(0x7<<1)
#define STEREO_AD_FILTER_DIV2_2		(0x0<<1)
#define STEREO_AD_FILTER_DIV2_4		(0x1<<1)
#define STEREO_AD_FILTER_DIV2_8		(0x2<<1)
#define STEREO_AD_FILTER_DIV2_16	(0x3<<1)
#define STEREO_AD_FILTER_DIV2_32	(0x4<<1)

/* Pseudo Stereo & Spatial Effect Block Control(0x68) */
#define SPATIAL_CTRL_EN				(0x1<<15)
#define ALL_PASS_FILTER_EN			(0x1<<14)
#define PSEUDO_STEREO_EN			(0x1<<13)
#define STEREO_EXPANSION_EN			(0x1<<12)

#define SPATIAL_GAIN_MASK			(0x3<<6)
#define SPATIAL_GAIN_1_0			(0x0<<6)
#define SPATIAL_GAIN_1_5			(0x1<<6)
#define SPATIAL_GAIN_2_0			(0x2<<6)

#define SPATIAL_RATIO_MASK			(0x3<<4)
#define SPATIAL_RATIO_0_0			(0x0<<4)
#define SPATIAL_RATIO_0_66			(0x1<<4)
#define SPATIAL_RATIO_1_0			(0x2<<4)

#define APF_MASK					(0x3)
#define APF_FOR_48K					(0x3)
#define APF_FOR_44_1K				(0x2)
#define APF_FOR_32K					(0x1)

#define ALC5624_PLL_FR_MCLK			0

/* Indexed registers */
#define IDX_EQ_BAND0_COEFF			0x00	/* EQ Band 0 Coefficient (LP0:a1) */
#define IDX_EQ_BAND0_GAIN			0x01	/* EQ Band 0 Gain (LP0:Ho) */

#define IDX_EQ_BAND1_COEFF0			0x02	/* EQ Band 1 Coefficient (BP1:a1) */
#define IDX_EQ_BAND1_COEFF1			0x03	/* EQ Band 1 Coefficient (BP1:a2) */
#define IDX_EQ_BAND1_GAIN			0x04	/* EQ Band 1 Gain (BP1:Ho) */

#define IDX_EQ_BAND2_COEFF0			0x05	/* EQ Band 2 Coefficient (BP2:a1) */
#define IDX_EQ_BAND2_COEFF1			0x06	/* EQ Band 2 Coefficient (BP2:a2) */
#define IDX_EQ_BAND2_GAIN			0x07	/* EQ Band 2 Gain (BP2:Ho) */

#define IDX_EQ_BAND3_COEFF0			0x08	/* EQ Band 3 Coefficient (BP3:a1) */
#define IDX_EQ_BAND3_COEFF1			0x09	/* EQ Band 3 Coefficient (BP3:a2) */
#define IDX_EQ_BAND3_GAIN			0x0A	/* EQ Band 3 Gain (BP3:Ho) */

#define IDX_EQ_BAND4_COEFF			0x0B	/* EQ Band 0 Coefficient (HPF:a1) */
#define IDX_EQ_BAND4_GAIN			0x0C	/* EQ Band 0 Gain (HPF:Ho) */

#define IDX_EQ_CTRL_STAT			0x10	/* EQ control and status register */
#define IDX_EQ_INPUT_VOL			0x11	/* EQ input volume */
#define IDX_EQ_OUTPUT_VOL			0x12	/* EQ output volume */

#define IDX_AUTO_VOL_CTRL0			0x20	/* Auto volume control 0 */
#define IDX_AUTO_VOL_CTRL1			0x21	/* Auto volume control 1 */
#define IDX_AUTO_VOL_CTRL2			0x22	/* Auto volume control 2 */
#define IDX_AUTO_VOL_CTRL3			0x23	/* Auto volume control 3 */
#define IDX_AUTO_VOL_CTRL4			0x24	/* Auto volume control 4 */
#define IDX_AUTO_VOL_CTRL5			0x25	/* Auto volume control 5 */

#define IDX_DIG_INTERNAL			0x39	/* Digital internal register */

#define IDX_CLASS_D_TEMP_SENSOR		0x4A	/* Class D temperature sensor */

#define IDX_AD_DA_MIXER_INTERNAL	0x54	/* AD DA internal mixer register */


/* virtual HP mixers regs */
#define VIRTUAL_HPL_MIXER	(ALC5624_VENDOR_ID2+2)
#define VIRTUAL_HPR_MIXER	(ALC5624_VENDOR_ID2+4)

/* Virtual index registers: Useful for equalizer settings */
#define VIRTUAL_IDX_BASE (ALC5624_VENDOR_ID2+6)

#define VIRTUAL_IDX_EQ_BAND0_COEFF 			(VIRTUAL_IDX_BASE+IDX_EQ_BAND0_COEFF)
#define VIRTUAL_IDX_EQ_BAND0_GAIN 			(VIRTUAL_IDX_BASE+IDX_EQ_BAND0_GAIN)
	
#define VIRTUAL_IDX_EQ_BAND1_COEFF0 		(VIRTUAL_IDX_BASE+IDX_EQ_BAND1_COEFF0)
#define VIRTUAL_IDX_EQ_BAND1_COEFF1 		(VIRTUAL_IDX_BASE+IDX_EQ_BAND1_COEFF1)
#define VIRTUAL_IDX_EQ_BAND1_GAIN 			(VIRTUAL_IDX_BASE+IDX_EQ_BAND1_GAIN)

#define VIRTUAL_IDX_EQ_BAND2_COEFF0 		(VIRTUAL_IDX_BASE+IDX_EQ_BAND2_COEFF0)
#define VIRTUAL_IDX_EQ_BAND2_COEFF1 		(VIRTUAL_IDX_BASE+IDX_EQ_BAND2_COEFF1)
#define VIRTUAL_IDX_EQ_BAND2_GAIN 			(VIRTUAL_IDX_BASE+IDX_EQ_BAND2_GAIN)

#define VIRTUAL_IDX_EQ_BAND3_COEFF0 		(VIRTUAL_IDX_BASE+IDX_EQ_BAND3_COEFF0)
#define VIRTUAL_IDX_EQ_BAND3_COEFF1 		(VIRTUAL_IDX_BASE+IDX_EQ_BAND3_COEFF1)
#define VIRTUAL_IDX_EQ_BAND3_GAIN 			(VIRTUAL_IDX_BASE+IDX_EQ_BAND3_GAIN)

#define VIRTUAL_IDX_EQ_BAND4_COEFF 			(VIRTUAL_IDX_BASE+IDX_EQ_BAND4_COEFF)
#define VIRTUAL_IDX_EQ_BAND4_GAIN 			(VIRTUAL_IDX_BASE+IDX_EQ_BAND4_GAIN)

#define VIRTUAL_IDX_EQ_CTRL_STAT 			(VIRTUAL_IDX_BASE+IDX_EQ_CTRL_STAT)
#define VIRTUAL_IDX_EQ_INPUT_VOL 			(VIRTUAL_IDX_BASE+IDX_EQ_INPUT_VOL)
#define VIRTUAL_IDX_EQ_OUTPUT_VOL 			(VIRTUAL_IDX_BASE+IDX_EQ_OUTPUT_VOL)

#define VIRTUAL_IDX_AUTO_VOL_CTRL0 			(VIRTUAL_IDX_BASE+IDX_AUTO_VOL_CTRL0)
#define VIRTUAL_IDX_AUTO_VOL_CTRL1 			(VIRTUAL_IDX_BASE+IDX_AUTO_VOL_CTRL1)
#define VIRTUAL_IDX_AUTO_VOL_CTRL2 			(VIRTUAL_IDX_BASE+IDX_AUTO_VOL_CTRL2)
#define VIRTUAL_IDX_AUTO_VOL_CTRL3 			(VIRTUAL_IDX_BASE+IDX_AUTO_VOL_CTRL3)
#define VIRTUAL_IDX_AUTO_VOL_CTRL4 			(VIRTUAL_IDX_BASE+IDX_AUTO_VOL_CTRL4)
#define VIRTUAL_IDX_AUTO_VOL_CTRL5 			(VIRTUAL_IDX_BASE+IDX_AUTO_VOL_CTRL5)

#define VIRTUAL_IDX_DIG_INTERNAL 			(VIRTUAL_IDX_BASE+IDX_DIG_INTERNAL)

#define VIRTUAL_IDX_CLASS_D_TEMP_SENSOR 	(VIRTUAL_IDX_BASE+IDX_CLASS_D_TEMP_SENSOR)

#define VIRTUAL_IDX_AD_DA_MIXER_INTERNAL 	(VIRTUAL_IDX_BASE+IDX_AD_DA_MIXER_INTERNAL)

#define REGISTER_COUNT (VIRTUAL_IDX_AD_DA_MIXER_INTERNAL + 2)

/* codec private data */
struct alc5624_priv {

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)
	/* Older versions of ALSA SoC require from us to hold the register
	   cache ourselves and a codec reference */
	struct snd_soc_codec codec;
	u16 reg_cache[REGISTER_COUNT];
#endif
	struct clk* mclk;				/* the master clock */
	enum snd_soc_control_type control_type;
	void *control_data;
	u8 id;
	unsigned int sysclk;
	
	/* original R/W functions */
	unsigned int (*bus_hw_read)(struct snd_soc_codec *codec, unsigned int reg);
	int (*bus_hw_write)(void*,const char*, int);  /* codec->control_data(->struct i2c_client), pdata, datalen */
	void *bus_control_data;			/* bus control_data to use when calling the original bus fns */
};


static const struct {
	u16 reg;	/* register */
	u16 val;	/* value */
} alc5624_reg_default[] = {
	{ALC5624_SPK_OUT_VOL				, 0x4040 }, /* Unmute left and right channels, enable 0 cross detector, 0db volume */
	{ALC5624_HP_OUT_VOL					, 0x4040 }, /* Unmute left and right channels, enable 0 cross detector, 0db volume */
	{ALC5624_PHONEIN_MONO_OUT_VOL		, 0xDFDF }, /* Phone input muted, Mono output muted */
	{ALC5624_LINE_IN_VOL				, 0xFF1F }, /* Mute Line In volume */
	{ALC5624_STEREO_DAC_VOL				, 0x6808 }, /* Mute volume output to Mono and Speaker */
	{ALC5624_MIC_VOL					, 0x0808 }, /* Mic volume = 0db */
	{ALC5624_MIC_ROUTING_CTRL			, 0xF0F0 }, /* Mute mic volume to Headphone, Speaker and Mono mixers, Differential mode enabled */
	{ALC5624_ADC_REC_GAIN				, 0xF58B },
	{ALC5624_ADC_REC_MIXER				, 0x3F3F }, /* Mic1 as recording sources */
	{ALC5624_OUTPUT_MIXER_CTRL			, 0x6B00 }, /* SPKL from HP mixer, Class D amplifier, SPKR from HP mixer, HPL from headphone mixer, HPR from headphone mixer, Mono from VMid */
	{ALC5624_MIC_CTRL					, 0x0F02 }, /* 1.8uA short current det, Bias volt =0.9Avdd, +40db gain boost */
	{ALC5624_PD_CTRL_STAT				, 0x0000 }, /* No powerdown */
	{ALC5624_MAIN_SDP_CTRL				, 0x8000 }, /* Slave interfase */
	{ALC5624_PWR_MANAG_ADD1				, 0x083F }, /* Nothing powered down */
	{ALC5624_PWR_MANAG_ADD2				, 0xE7FF }, /* Nothing powered down */
	{ALC5624_PWR_MANAG_ADD3				, 0x7FFF }, /* Nothing powered down */
	{ALC5624_GEN_CTRL_REG1				, 0x00E8 }, /* 1v as Vmid for all amps */
	{ALC5624_GEN_CTRL_REG2				, 0x0000 }, /* Class AB differential mode */
	{ALC5624_PLL_CTRL					, 0x0000 },
	{ALC5624_GPIO_PIN_CONFIG			, 0x2E3E }, /* All GPIOs as input */
	{ALC5624_GPIO_PIN_POLARITY			, 0x2E3E }, /* All GPIOs high active */
	{ALC5624_GPIO_PIN_STICKY			, 0x0000 }, /* No sticky ops */
	{ALC5624_GPIO_PIN_WAKEUP			, 0x0000 }, /* No wakeups */
	{ALC5624_GPIO_PIN_SHARING			, 0x0000 },
	{ALC5624_GPIO_OUT_CTRL				, 0x0000 },
	{ALC5624_MISC_CTRL					, 0x8800 }, /* Slow Vref, Strong amp, No AVC, thermal shutdown enabled */
	{ALC5624_STEREO_DAC_CLK_CTRL1		, 0x3075 },
	{ALC5624_STEREO_DAC_CLK_CTRL2		, 0x1010 },
	{ALC5624_PSEUDO_SPATIAL_CTRL		, 0x0053 }, /* Disable */
	{VIRTUAL_HPL_MIXER					, 0x0000 },
	{VIRTUAL_HPR_MIXER					, 0x0000 },
	{VIRTUAL_IDX_EQ_BAND0_COEFF			, 0x1b1b },
	{VIRTUAL_IDX_EQ_BAND0_GAIN 			, 0xf510 },
	{VIRTUAL_IDX_EQ_BAND1_COEFF0		, 0xc10f },
	{VIRTUAL_IDX_EQ_BAND1_COEFF1		, 0x1ef6 },
	{VIRTUAL_IDX_EQ_BAND1_GAIN 			, 0xf65f },
	{VIRTUAL_IDX_EQ_BAND2_COEFF0 		, 0xc159 },
	{VIRTUAL_IDX_EQ_BAND2_COEFF1 		, 0x1eb3 },
	{VIRTUAL_IDX_EQ_BAND2_GAIN 			, 0xf510 },
	{VIRTUAL_IDX_EQ_BAND3_COEFF0 		, 0xc386 },
	{VIRTUAL_IDX_EQ_BAND3_COEFF1 		, 0x1cd0 },
	{VIRTUAL_IDX_EQ_BAND3_GAIN 			, 0x0adc },
	{VIRTUAL_IDX_EQ_BAND4_COEFF 		, 0x0436 },
	{VIRTUAL_IDX_EQ_BAND4_GAIN 			, 0x2298 },
	{VIRTUAL_IDX_EQ_CTRL_STAT 			, 0x0000 }, /* Disable Equalizer */
	{VIRTUAL_IDX_EQ_INPUT_VOL 			, 0x0000 }, /* 0db */
	{VIRTUAL_IDX_EQ_OUTPUT_VOL 			, 0x0001 }, /* 0db */
	{VIRTUAL_IDX_AUTO_VOL_CTRL0 		, 0x0050 },
	{VIRTUAL_IDX_AUTO_VOL_CTRL1 		, 0x2710 },
	{VIRTUAL_IDX_AUTO_VOL_CTRL2 		, 0x0BB8 },
	{VIRTUAL_IDX_AUTO_VOL_CTRL3 		, 0x01F4 },
	{VIRTUAL_IDX_AUTO_VOL_CTRL4 		, 0x0190 },
	{VIRTUAL_IDX_AUTO_VOL_CTRL5 		, 0x0200 },
	{VIRTUAL_IDX_DIG_INTERNAL 			, 0x0000 },
	{VIRTUAL_IDX_CLASS_D_TEMP_SENSOR 	, 0x4444 },
	{VIRTUAL_IDX_AD_DA_MIXER_INTERNAL	, 0xE184 },
};

/*
 * ALC5624 Controls
 */
 
/* from -46.5 to 0 dB in 1.5 dB steps - 32 steps */
static DECLARE_TLV_DB_SCALE(spkhpmonooutvol_tlv, -4650, 150, 0); 

/* from -34.5 to 12 dB in 1.5 dB steps - 32 steps */
static DECLARE_TLV_DB_SCALE(daclinmicphonevol_tlv, -3450, 150, 0); 

/* from -16.5 to 30 dB in 1.5 dB steps - 32 steps */
static DECLARE_TLV_DB_SCALE(capvol_tlv, -1650, 150, 0); 

static const char *alc5624_spkl_pga[] = {"Vmid","HPL mixer","SPK mixer","Mono Mixer"};
static const char *alc5624_spkr_pga[] = {"Vmid","HPR mixer","SPK mixer","Mono Mixer"};
static const char *alc5624_hpl_pga[]  = {"Vmid","HPL mixer"};
static const char *alc5624_hpr_pga[]  = {"Vmid","HPR mixer"};
static const char *alc5624_mono_pga[] = {"Vmid","HP mixer","SPK mixer","Mono Mixer"};
static const char *alc5624_amp_type_sel[] = {"Class AB","Class D"};
static const char *alc5624_mic_boost_sel[] = {"Bypass","20db","30db","40db"};

static const struct soc_enum alc5624_enum[] = {
SOC_ENUM_SINGLE(ALC5624_OUTPUT_MIXER_CTRL,14, 4, alc5624_spkl_pga), /* spk left input sel 0 */	
SOC_ENUM_SINGLE(ALC5624_OUTPUT_MIXER_CTRL,11, 4, alc5624_spkr_pga), /* spk right input sel 1 */	
SOC_ENUM_SINGLE(ALC5624_OUTPUT_MIXER_CTRL,9,  2, alc5624_hpl_pga), /* hp left input sel 2 */	
SOC_ENUM_SINGLE(ALC5624_OUTPUT_MIXER_CTRL,8,  2, alc5624_hpr_pga), /* hp right input sel 3 */	
SOC_ENUM_SINGLE(ALC5624_OUTPUT_MIXER_CTRL,6,  4, alc5624_mono_pga), /* mono input sel 4 */
SOC_ENUM_SINGLE(ALC5624_MIC_CTRL		,10, 4, alc5624_mic_boost_sel), /*Mic1 boost sel 5 */
SOC_ENUM_SINGLE(ALC5624_MIC_CTRL		,8,  4, alc5624_mic_boost_sel), /*Mic2 boost sel 6 */
SOC_ENUM_SINGLE(ALC5624_OUTPUT_MIXER_CTRL,12, 2,alc5624_amp_type_sel), /*Speaker AMP sel 7 */
};

static const struct snd_kcontrol_new alc5624_snd_controls[] = {
SOC_DOUBLE_TLV("Speaker Playback Volume", 	ALC5624_SPK_OUT_VOL, 8, 0, 31, 1, spkhpmonooutvol_tlv),	
SOC_DOUBLE("Speaker Playback Switch", 		ALC5624_SPK_OUT_VOL, 15, 7, 1, 1),
SOC_DOUBLE_TLV("Headphone Playback Volume", ALC5624_HP_OUT_VOL, 8, 0, 31, 1, spkhpmonooutvol_tlv),
SOC_DOUBLE("Headphone Playback Switch", 	ALC5624_HP_OUT_VOL, 15, 7, 1, 1),
SOC_SINGLE_TLV("Mono Playback Volume", 		ALC5624_PHONEIN_MONO_OUT_VOL, 0, 31, 1, spkhpmonooutvol_tlv),
SOC_SINGLE("Mono Playback Switch", 			ALC5624_PHONEIN_MONO_OUT_VOL, 7, 1, 1),
SOC_SINGLE_TLV("Phone In Volume", 			ALC5624_PHONEIN_MONO_OUT_VOL, 8, 31, 1, daclinmicphonevol_tlv),
SOC_DOUBLE_TLV("Line In Volume", 			ALC5624_LINE_IN_VOL, 8, 0, 31, 1, daclinmicphonevol_tlv),
SOC_DOUBLE_TLV("PCM Playback Volume", 		ALC5624_STEREO_DAC_VOL, 8, 0, 31, 1, daclinmicphonevol_tlv),
SOC_DOUBLE("PCM Playback Switch", 			ALC5624_STEREO_DAC_VOL, 15, 7, 1, 1),
SOC_SINGLE_TLV("Mic 1 Volume", 				ALC5624_MIC_VOL, 8, 31, 1, daclinmicphonevol_tlv),
SOC_SINGLE_TLV("Mic 2 Volume", 				ALC5624_MIC_VOL, 0, 31, 1, daclinmicphonevol_tlv),
SOC_ENUM("Mic 1 Boost", 					alc5624_enum[5]),
SOC_ENUM("Mic 2 Boost", 					alc5624_enum[6]),
SOC_ENUM("Speaker Amp Type",				alc5624_enum[7]),
SOC_DOUBLE_TLV("Capture Volume", 			ALC5624_ADC_REC_GAIN, 7, 0, 31, 0, capvol_tlv),
	};

/*
 * DAPM controls
 */

/* Left Headphone Mixers */
static const struct snd_kcontrol_new alc5624_hpl_mixer_controls[] = {
SOC_DAPM_SINGLE("LineIn Playback Switch", VIRTUAL_HPL_MIXER, 4, 1, 0),
SOC_DAPM_SINGLE("PhoneIn Playback Switch", VIRTUAL_HPL_MIXER, 3, 1, 0),
SOC_DAPM_SINGLE("Mic1 Playback Switch", VIRTUAL_HPL_MIXER, 2, 1, 0),
SOC_DAPM_SINGLE("Mic2 Playback Switch", VIRTUAL_HPL_MIXER, 1, 1, 0),
SOC_DAPM_SINGLE("PCM Playback Switch", VIRTUAL_HPL_MIXER, 0, 1, 0),
SOC_DAPM_SINGLE("RecordL Playback Switch", ALC5624_ADC_REC_GAIN, 15, 1,1),
};

/* Right Headphone Mixers */
static const struct snd_kcontrol_new alc5624_hpr_mixer_controls[] = {
SOC_DAPM_SINGLE("LineIn Playback Switch", VIRTUAL_HPR_MIXER, 4, 1, 0),
SOC_DAPM_SINGLE("PhoneIn Playback Switch", VIRTUAL_HPR_MIXER, 3, 1, 0),
SOC_DAPM_SINGLE("Mic1 Playback Switch", VIRTUAL_HPR_MIXER, 2, 1, 0),
SOC_DAPM_SINGLE("Mic2 Playback Switch", VIRTUAL_HPR_MIXER, 1, 1, 0),
SOC_DAPM_SINGLE("PCM Playback Switch", VIRTUAL_HPR_MIXER, 0, 1, 0),
SOC_DAPM_SINGLE("RecordR Playback Switch", ALC5624_ADC_REC_GAIN, 14, 1,1),
};

//Left Record Mixer
static const struct snd_kcontrol_new alc5624_captureL_mixer_controls[] = {
SOC_DAPM_SINGLE("Mic1 Capture Switch", ALC5624_ADC_REC_MIXER, 14, 1, 1),
SOC_DAPM_SINGLE("Mic2 Capture Switch", ALC5624_ADC_REC_MIXER, 13, 1, 1),
SOC_DAPM_SINGLE("LineInL Capture Switch",ALC5624_ADC_REC_MIXER,12, 1, 1),
SOC_DAPM_SINGLE("Phone Capture Switch", ALC5624_ADC_REC_MIXER, 11, 1, 1),
SOC_DAPM_SINGLE("HPMixerL Capture Switch", ALC5624_ADC_REC_MIXER,10, 1, 1),
SOC_DAPM_SINGLE("SPKMixer Capture Switch",ALC5624_ADC_REC_MIXER,9, 1, 1),
SOC_DAPM_SINGLE("MonoMixer Capture Switch",ALC5624_ADC_REC_MIXER,8, 1, 1),
};

//Right Record Mixer
static const struct snd_kcontrol_new alc5624_captureR_mixer_controls[] = {
SOC_DAPM_SINGLE("Mic1 Capture Switch", ALC5624_ADC_REC_MIXER, 6, 1, 1),
SOC_DAPM_SINGLE("Mic2 Capture Switch", ALC5624_ADC_REC_MIXER, 5, 1, 1),
SOC_DAPM_SINGLE("LineInR Capture Switch",ALC5624_ADC_REC_MIXER,4, 1, 1),
SOC_DAPM_SINGLE("Phone Capture Switch", ALC5624_ADC_REC_MIXER, 3, 1, 1),
SOC_DAPM_SINGLE("HPMixer Capture Switch", ALC5624_ADC_REC_MIXER,2, 1, 1),
SOC_DAPM_SINGLE("SPKMixer Capture Switch",ALC5624_ADC_REC_MIXER,1, 1, 1),
SOC_DAPM_SINGLE("MonoMixer Capture Switch",ALC5624_ADC_REC_MIXER,0, 1, 1),
};

/* Speaker Mixer */
static const struct snd_kcontrol_new alc5624_speaker_mixer_controls[] = {
SOC_DAPM_SINGLE("LineIn Playback Switch", ALC5624_LINE_IN_VOL, 14, 1, 1),
SOC_DAPM_SINGLE("PhoneIn Playback Switch", ALC5624_PHONEIN_MONO_OUT_VOL, 14, 1, 1),
SOC_DAPM_SINGLE("Mic1 Playback Switch", ALC5624_MIC_ROUTING_CTRL, 14, 1, 1),
SOC_DAPM_SINGLE("Mic2 Playback Switch", ALC5624_MIC_ROUTING_CTRL, 6, 1, 1),
SOC_DAPM_SINGLE("PCM Playback Switch", ALC5624_STEREO_DAC_VOL, 14, 1, 1),
};

/* Mono Mixer */
static const struct snd_kcontrol_new alc5624_mono_mixer_controls[] = {
SOC_DAPM_SINGLE("LineIn Playback Switch", ALC5624_LINE_IN_VOL, 13, 1, 1),
SOC_DAPM_SINGLE("Mic1 Playback Switch", ALC5624_MIC_ROUTING_CTRL, 13, 1, 1),
SOC_DAPM_SINGLE("Mic2 Playback Switch", ALC5624_MIC_ROUTING_CTRL, 5, 1, 1),
SOC_DAPM_SINGLE("PCM Playback Switch", ALC5624_STEREO_DAC_VOL, 13, 1, 1),
SOC_DAPM_SINGLE("RecL Playback Switch", ALC5624_ADC_REC_GAIN, 13, 1, 1),
SOC_DAPM_SINGLE("RecR Playback Switch", ALC5624_ADC_REC_GAIN, 12, 1, 1),
SOC_DAPM_SINGLE("RecordL Playback Switch", ALC5624_ADC_REC_GAIN, 13, 1,1),
SOC_DAPM_SINGLE("RecordR Playback Switch", ALC5624_ADC_REC_GAIN, 12, 1,1),
};
 
/* mono output mux */
static const struct snd_kcontrol_new alc5624_mono_mux_controls =
SOC_DAPM_ENUM("Route", alc5624_enum[4]);

/* speaker left output mux */
static const struct snd_kcontrol_new alc5624_hp_spkl_mux_controls =
SOC_DAPM_ENUM("Route", alc5624_enum[0]);

/* speaker right output mux */
static const struct snd_kcontrol_new alc5624_hp_spkr_mux_controls =
SOC_DAPM_ENUM("Route", alc5624_enum[1]);

/* headphone left output mux */
static const struct snd_kcontrol_new alc5624_hpl_out_mux_controls =
SOC_DAPM_ENUM("Route", alc5624_enum[2]);

/* headphone right output mux */
static const struct snd_kcontrol_new alc5624_hpr_out_mux_controls =
SOC_DAPM_ENUM("Route", alc5624_enum[3]);

/* We have to create a fake left and right HP mixers because
 * the codec only has a single control that is shared by both channels.
 * This makes it impossible to determine the audio path using the current
 * register map, thus we add a new (virtual) register to help determine the
 * audio route within the device.
 */
static int mixer_event (struct snd_soc_dapm_widget *w, 
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	
	u16 l, r, lineIn,mic1,mic2, phone, pcm;
	dev_dbg(codec->dev,"%s()\n", __FUNCTION__);
	
	l = snd_soc_read(codec, VIRTUAL_HPL_MIXER);
	r = snd_soc_read(codec, VIRTUAL_HPR_MIXER);
	lineIn = snd_soc_read(codec, ALC5624_LINE_IN_VOL);
	mic1 = snd_soc_read(codec, ALC5624_MIC_ROUTING_CTRL);
	mic2 = snd_soc_read(codec, ALC5624_MIC_ROUTING_CTRL);
	phone = snd_soc_read(codec,ALC5624_PHONEIN_MONO_OUT_VOL);
	pcm = snd_soc_read(codec, ALC5624_STEREO_DAC_VOL);

	if (event & SND_SOC_DAPM_PRE_REG)
		return 0;
		
	if (l & 0x1 || r & 0x1)
		snd_soc_write(codec, ALC5624_STEREO_DAC_VOL, pcm & 0x7fff);
	else
		snd_soc_write(codec, ALC5624_STEREO_DAC_VOL, pcm | 0x8000);

	if (l & 0x2 || r & 0x2)
		snd_soc_write(codec, ALC5624_MIC_ROUTING_CTRL, mic2 & 0xff7f);
	else
		snd_soc_write(codec, ALC5624_MIC_ROUTING_CTRL, mic2 | 0x0080);

	if (l & 0x4 || r & 0x4)
		snd_soc_write(codec, ALC5624_MIC_ROUTING_CTRL, mic1 & 0x7fff);
	else
		snd_soc_write(codec, ALC5624_MIC_ROUTING_CTRL, mic1 | 0x8000);

	if (l & 0x8 || r & 0x8)
		snd_soc_write(codec, ALC5624_PHONEIN_MONO_OUT_VOL, phone & 0x7fff);
	else
		snd_soc_write(codec, ALC5624_PHONEIN_MONO_OUT_VOL, phone | 0x8000);

	if (l & 0x10 || r & 0x10)
		snd_soc_write(codec, ALC5624_LINE_IN_VOL, lineIn & 0x7fff);
	else
		snd_soc_write(codec, ALC5624_LINE_IN_VOL, lineIn | 0x8000);

	return 0;
}


static const struct snd_soc_dapm_widget alc5624_dapm_widgets[] = {
SND_SOC_DAPM_MUX("Mono Out Mux", SND_SOC_NOPM, 0, 0,
	&alc5624_mono_mux_controls),
SND_SOC_DAPM_MUX("Left Speaker Out Mux", SND_SOC_NOPM, 0, 0,
	&alc5624_hp_spkl_mux_controls),
SND_SOC_DAPM_MUX("Right Speaker Out Mux", SND_SOC_NOPM, 0, 0,
	&alc5624_hp_spkr_mux_controls),
SND_SOC_DAPM_MUX("Left Headphone Out Mux", SND_SOC_NOPM, 0, 0,
	&alc5624_hpl_out_mux_controls),
SND_SOC_DAPM_MUX("Right Headphone Out Mux", SND_SOC_NOPM, 0, 0,
	&alc5624_hpr_out_mux_controls),
SND_SOC_DAPM_MIXER_E("Left HP Mixer",ALC5624_PWR_MANAG_ADD2, 5, 0,
	&alc5624_hpl_mixer_controls[0], ARRAY_SIZE(alc5624_hpl_mixer_controls),
	mixer_event, SND_SOC_DAPM_POST_REG),
SND_SOC_DAPM_MIXER_E("Right HP Mixer",ALC5624_PWR_MANAG_ADD2, 4, 0,
	&alc5624_hpr_mixer_controls[0], ARRAY_SIZE(alc5624_hpr_mixer_controls),
	mixer_event, SND_SOC_DAPM_POST_REG),
SND_SOC_DAPM_MIXER("Mono Mixer", ALC5624_PWR_MANAG_ADD2, 2, 0,
	&alc5624_mono_mixer_controls[0], ARRAY_SIZE(alc5624_mono_mixer_controls)),
SND_SOC_DAPM_MIXER("Speaker Mixer", ALC5624_PWR_MANAG_ADD2,3,0,
	&alc5624_speaker_mixer_controls[0],
	ARRAY_SIZE(alc5624_speaker_mixer_controls)),	
SND_SOC_DAPM_MIXER("Left Record Mixer", ALC5624_PWR_MANAG_ADD2,1,0,
	&alc5624_captureL_mixer_controls[0],
	ARRAY_SIZE(alc5624_captureL_mixer_controls)),	
SND_SOC_DAPM_MIXER("Right Record Mixer", ALC5624_PWR_MANAG_ADD2,0,0,
	&alc5624_captureR_mixer_controls[0],
	ARRAY_SIZE(alc5624_captureR_mixer_controls)),				
SND_SOC_DAPM_DAC("Left DAC", "Left HiFi Playback", ALC5624_PWR_MANAG_ADD2,9, 0),
SND_SOC_DAPM_DAC("Right DAC", "Right HiFi Playback", ALC5624_PWR_MANAG_ADD2, 8, 0),	
SND_SOC_DAPM_MIXER("IIS Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_MIXER("HP Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_ADC("Left ADC", "Left HiFi Capture", ALC5624_PWR_MANAG_ADD2, 7, 0),
SND_SOC_DAPM_ADC("Right ADC", "Right HiFi Capture", ALC5624_PWR_MANAG_ADD2, 6, 0),

SND_SOC_DAPM_PGA("Mono Out PGA", ALC5624_PWR_MANAG_ADD3, 14, 1, NULL, 0),
SND_SOC_DAPM_PGA("Left Speaker N", ALC5624_PWR_MANAG_ADD3, 13, 1, NULL, 0),
SND_SOC_DAPM_PGA("Right Speaker N", ALC5624_PWR_MANAG_ADD3, 12, 1, NULL, 0),
SND_SOC_DAPM_PGA("Left Headphone", ALC5624_PWR_MANAG_ADD3, 11, 1, NULL, 0),
SND_SOC_DAPM_PGA("Right Headphone", ALC5624_PWR_MANAG_ADD3, 10, 1, NULL, 0),
SND_SOC_DAPM_PGA("Left Speaker", ALC5624_PWR_MANAG_ADD3, 9, 1, NULL, 0),
SND_SOC_DAPM_PGA("Right Speaker", ALC5624_PWR_MANAG_ADD3, 8, 1, NULL, 0),
SND_SOC_DAPM_PGA("Left Line In", ALC5624_PWR_MANAG_ADD3, 7, 1, NULL, 0),
SND_SOC_DAPM_PGA("Right Line In", ALC5624_PWR_MANAG_ADD3, 6, 1, NULL, 0),
SND_SOC_DAPM_PGA("Phone In PGA", ALC5624_PWR_MANAG_ADD3, 5, 1, NULL, 0),
SND_SOC_DAPM_PGA("Phone In Mixer", ALC5624_PWR_MANAG_ADD3, 4, 1, NULL, 0),
SND_SOC_DAPM_PGA("Mic 1 PGA", ALC5624_PWR_MANAG_ADD3, 3, 1, NULL, 0),
SND_SOC_DAPM_PGA("Mic 2 PGA", ALC5624_PWR_MANAG_ADD3, 2, 1, NULL, 0),
SND_SOC_DAPM_PGA("Mic 1 Pre Amp", ALC5624_PWR_MANAG_ADD3, 1, 1, NULL, 0),
SND_SOC_DAPM_PGA("Mic 2 Pre Amp", ALC5624_PWR_MANAG_ADD3, 0, 1, NULL, 0),

SND_SOC_DAPM_MICBIAS("Mic Bias1", ALC5624_PWR_MANAG_ADD1, 3, 0),
SND_SOC_DAPM_MICBIAS("Mic Bias2", ALC5624_PWR_MANAG_ADD1, 2, 0),
SND_SOC_DAPM_OUTPUT("MONO"),
SND_SOC_DAPM_OUTPUT("HPL"),
SND_SOC_DAPM_OUTPUT("HPR"),
SND_SOC_DAPM_OUTPUT("SPKL"),
SND_SOC_DAPM_OUTPUT("SPKR"),
SND_SOC_DAPM_OUTPUT("SPKLN"),
SND_SOC_DAPM_OUTPUT("SPKRN"),
SND_SOC_DAPM_INPUT("LINEL"),
SND_SOC_DAPM_INPUT("LINER"),
SND_SOC_DAPM_INPUT("PHONEIN"),
SND_SOC_DAPM_INPUT("MIC1"),
SND_SOC_DAPM_INPUT("MIC2"),
SND_SOC_DAPM_INPUT("PCMIN"),
SND_SOC_DAPM_VMID("VMID"),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* left HP mixer */
	{"Left HP Mixer", "LineIn Playback Switch", "LINEL"},
	{"Left HP Mixer", "PhoneIn Playback Switch","PHONEIN"},
	{"Left HP Mixer", "Mic1 Playback Switch","MIC1"},
	{"Left HP Mixer", "Mic2 Playback Switch","MIC2"},
	{"Left HP Mixer", "PCM Playback Switch","Left DAC"},
	{"Left HP Mixer", "RecordL Playback Switch","Left Record Mixer"},
	
	/* right HP mixer */
	{"Right HP Mixer", "LineIn Playback Switch", "LINER"},
	{"Right HP Mixer", "PhoneIn Playback Switch","PHONEIN"},
	{"Right HP Mixer", "Mic1 Playback Switch","MIC1"},
	{"Right HP Mixer", "Mic2 Playback Switch","MIC2"},
	{"Right HP Mixer", "PCM Playback Switch","Right DAC"},
	{"Right HP Mixer", "RecordR Playback Switch","Right Record Mixer"},
	
	/* virtual mixer - mixes left & right channels for spk and mono */
	{"IIS Mixer", NULL, "Left DAC"},
	{"IIS Mixer", NULL, "Right DAC"},
	{"Line Mixer", NULL, "Left Line In"},
	{"Line Mixer", NULL, "Right Line In"},	
	{"HP Mixer", NULL, "Left HP Mixer"},
	{"HP Mixer", NULL, "Right HP Mixer"},
	
	/* speaker mixer */
	{"Speaker Mixer", "LineIn Playback Switch","Line Mixer"},
	{"Speaker Mixer", "PhoneIn Playback Switch","PHONEIN"},
	{"Speaker Mixer", "Mic1 Playback Switch","MIC1"},
	{"Speaker Mixer", "Mic2 Playback Switch","MIC2"},
	{"Speaker Mixer", "PCM Playback Switch","IIS Mixer"},

	/* mono mixer */
	{"Mono Mixer", "LineIn Playback Switch","Line Mixer"},
	{"Mono Mixer", "PhoneIn Playback Switch","PHONEIN"},
	{"Mono Mixer", "Mic1 Playback Switch","MIC1"},
	{"Mono Mixer", "Mic2 Playback Switch","MIC2"},
	{"Mono Mixer", "PCM Playback Switch","IIS Mixer"},
	{"Mono Mixer", "RecordL Playback Switch","Left Record Mixer"},
	{"Mono Mixer", "RecordR Playback Switch","Right Record Mixer"},
	
	/*Left record mixer */
	{"Left Record Mixer", "Mic1 Capture Switch","Mic 1 Pre Amp"},
	{"Left Record Mixer", "Mic2 Capture Switch","Mic 2 Pre Amp"},
	{"Left Record Mixer", "LineInL Capture Switch","LINEL"},
	{"Left Record Mixer", "Phone Capture Switch","PHONEIN"},
	{"Left Record Mixer", "HPMixerL Capture Switch","Left HP Mixer"},
	{"Left Record Mixer", "SPKMixer Capture Switch","Speaker Mixer"},
	{"Left Record Mixer", "MonoMixer Capture Switch","Mono Mixer"},
	
	/*Right record mixer */
	{"Right Record Mixer", "Mic1 Capture Switch","Mic 1 Pre Amp"},
	{"Right Record Mixer", "Mic2 Capture Switch","Mic 2 Pre Amp"},
	{"Right Record Mixer", "LineInR Capture Switch","LINER"},
	{"Right Record Mixer", "Phone Capture Switch","PHONEIN"},
	{"Right Record Mixer", "HPMixerR Capture Switch","Right HP Mixer"},
	{"Right Record Mixer", "SPKMixer Capture Switch","Speaker Mixer"},
	{"Right Record Mixer", "MonoMixer Capture Switch","Mono Mixer"},	

	/* headphone left mux */
	{"Left Headphone Out Mux", "HPL mixer", "Left HP Mixer"},

	/* headphone right mux */
	{"Right Headphone Out Mux", "HPR mixer", "Right HP Mixer"},

	/* speaker left mux */
	{"Left Speaker Out Mux", "HPL mixer", "Left HP Mixer"},
	{"Left Speaker Out Mux", "SPK mixer", "Speaker Mixer"},
	{"Left Speaker Out Mux", "Mono Mixer", "Mono Mixer"},

	/* speaker right mux */
	{"Right Speaker Out Mux", "HPR mixer", "Right HP Mixer"},
	{"Right Speaker Out Mux", "SPK mixer", "Speaker Mixer"},
	{"Right Speaker Out Mux", "Mono Mixer", "Mono Mixer"},

	/* mono mux */
	{"Mono Out Mux", "HP mixer", "HP Mixer"},
	{"Mono Out Mux", "SPK mixer", "Speaker Mixer"},
	{"Mono Out Mux", "Mono Mixer", "Mono Mixer"},
	
	/* output pga */
	{"HPL", NULL, "Left Headphone"},
	{"Left Headphone", NULL, "Left Headphone Out Mux"},
	{"HPR", NULL, "Right Headphone"},
	{"Right Headphone", NULL, "Right Headphone Out Mux"},
	{"SPKL", NULL, "Left Speaker"},
	{"SPKLN", NULL, "Left Speaker N"},
	{"Left Speaker", NULL, "Left Speaker Out Mux"},
	{"Left Speaker N", NULL, "Left Speaker Out Mux"},
	{"SPKR", NULL, "Right Speaker"},
	{"SPKRN", NULL, "Right Speaker N"},	
	{"Right Speaker", NULL, "Right Speaker Out Mux"},
	{"Right Speaker N", NULL, "Right Speaker Out Mux"},
	{"MONO", NULL, "Mono Out"},
	{"Mono Out", NULL, "Mono Out Mux"},

	/* input pga */
	{"Left Line In", NULL, "LINEL"},
	{"Right Line In", NULL, "LINER"},
	{"Phone In PGA", NULL, "PHONEIN"},
	{"Phone In Mixer", NULL, "PHONEIN"},
	{"Mic 1 Pre Amp", NULL, "MIC1"},
	{"Mic 2 Pre Amp", NULL, "MIC2"},	
	{"Mic 1 PGA", NULL, "Mic 1 Pre Amp"},
	{"Mic 2 PGA", NULL, "Mic 2 Pre Amp"},

	/* left ADC */
	{"Left ADC", NULL, "Left Record Mixer"},

	/* right ADC */
	{"Right ADC", NULL, "Right Record Mixer"},
	{NULL, NULL, NULL},	
};

static int alc5624_dai_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int is_play = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct snd_soc_codec *codec = dai->codec;

	if (is_play) {

		/* Enable class AB power amplifier and thermal detect */
		snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD2,0xC000,0xC000); 
		
		/* Power up speaker amplifier, Headphone out,Mono out, and Stereo DAC */
		snd_soc_update_bits(codec,ALC5624_PD_CTRL_STAT,0xC200,0x0000);
	
	} else {

		/* Power up Stereo ADC */
		snd_soc_update_bits(codec,ALC5624_PD_CTRL_STAT,0x0100,0x0000);
	
	}

	return 0;
}

static void alc5624_dai_shutdown(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	int is_play = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct snd_soc_codec *codec = dai->codec;

	if (is_play) {
	
		/* Disable class AB power amplifier and thermal detect */
		snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD2,0xC000,0x0000); 
		
		/* Power down speaker amplifier, Headphone out,Mono out, and Stereo DAC */
		snd_soc_update_bits(codec,ALC5624_PD_CTRL_STAT,0xC200,0xC200);

	} else {
	
		/* Power up Stereo ADC */
		snd_soc_update_bits(codec,ALC5624_PD_CTRL_STAT,0x0100,0x0100);
		
	}
}

/* PLL divisors */
struct _pll_div {
	u32 pll_in;
	u32 pll_out;
	u16 regvalue;
};

static const struct _pll_div codec_pll_div[] = {
	
	{  2048000,  8192000,	0x0ea0},		
	{  3686400,  8192000,	0x4e27},	
	{ 12000000,  8192000,	0x456b},   
	{ 13000000,  8192000,	0x495f},
	{ 13100000,	 8192000,	0x0320},	
	{  2048000,  11289600,	0xf637},
	{  3686400,  11289600,	0x2f22},	
	{ 12000000,  11289600,	0x3e2f},   
	{ 13000000,  11289600,	0x4d5b},
	{ 13100000,	 11289600,	0x363b},	
	{  2048000,  16384000,	0x1ea0},
	{  3686400,  16384000,	0x9e27},	
	{ 12000000,  16384000,	0x452b},   
	{ 13000000,  16384000,	0x542f},
	{ 13100000,	 16384000,	0x03a0},	
	{  2048000,  16934400,	0xe625},
	{  3686400,  16934400,	0x9126},	
	{ 12000000,  16934400,	0x4d2c},   
	{ 13000000,  16934400,	0x742f},
	{ 13100000,	 16934400,	0x3c27},			
	{  2048000,  22579200,	0x2aa0},
	{  3686400,  22579200,	0x2f20},	
	{ 12000000,  22579200,	0x7e2f},   
	{ 13000000,  22579200,	0x742f},
	{ 13100000,	 22579200,	0x3c27},		
	{  2048000,  24576000,	0x2ea0},
	{  3686400,  24576000,	0xee27},	
	{ 12000000,  24576000,	0x2915},   
	{ 13000000,  24576000,	0x772e},
	{ 13100000,	 24576000,	0x0d20},	
};

static int alc5624_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id,
		int source, unsigned int freq_in, unsigned int freq_out)
{
	int i;
	struct snd_soc_codec *codec = codec_dai->codec;
	dev_dbg(codec->dev, "%s()\n", __FUNCTION__);

	/* disable PLL power */
	snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD2,0x1000, 0x0000);	
	
	if (!freq_in || !freq_out) 
		return 0;

	for (i = 0; i < ARRAY_SIZE(codec_pll_div); i++) {
		if (codec_pll_div[i].pll_in == freq_in && 
			codec_pll_div[i].pll_out == freq_out) {
							 	
				/* set PLL parameter */
			 	snd_soc_write(codec,ALC5624_PLL_CTRL,codec_pll_div[i].regvalue);
			 	
				/* enable PLL power */
				snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD2, 0x1000,0x1000);	
				
				/* Codec sys-clock from PLL */
				snd_soc_update_bits(codec,ALC5624_GEN_CTRL_REG1,0x8000,0x8000);
			 	
				return 0;
		}
	}
	
	return -EINVAL;
}

/*
 * Clock after PLL and dividers
 */
static int alc5624_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir) 
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct alc5624_priv *alc5624 = snd_soc_codec_get_drvdata(codec);
	dev_dbg(codec->dev, "clk_id = %d, freq = %d\n", clk_id, freq);

	switch (freq) {
	case  8192000:	
	case 11289600:
	case 12288000:
	case 16384000:
	case 16934400:
	case 18432000:
	case 22579200:	
	case 24576000:
		alc5624->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int alc5624_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;
	dev_dbg(codec->dev, "%s()\n", __FUNCTION__);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface = 0x0000;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		iface = 0x8000;
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x0000;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface |= 0x0001;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0002;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= 0x0043;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		iface |= 0x0000;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x1040;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x1000;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0040;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec,ALC5624_MAIN_SDP_CTRL,0x9043,iface);
	return 0;
}

struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 fs;
	u16 regvalue1;
	u16 regvalue2;
};


/* codec hifi mclk (after PLL) clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{ 8192000,  8000, 256*4, 0x3272,0x1212},
	{12288000,  8000, 384*4, 0x5272,0x2222},

	/* 11.025k */
	{11289600, 11025, 256*4, 0x3272,0x1212},
	{16934400, 11025, 384*4, 0x5272,0x2222},

	/* 16k */
	{12288000, 16000, 384*2, 0x2272,0x2020},
	{16384000, 16000, 256*4, 0x3272,0x1212},
	{24576000, 16000, 384*4, 0x5272,0x2222},

	/* 22.05k */	
	{11289600, 22050, 256*2, 0x3172,0x1010},
	{16934400, 22050, 384*2, 0x2272,0x2020},

	/* 32k */
	{12288000, 32000, 384*2, 0x2172,0x2121},
	{16384000, 32000, 256*2, 0x3172,0x1010},
	{24576000, 32000, 384*2, 0x2272,0x2020},

	/* 44.1k */
	{11289600, 44100, 256*1, 0x3072,0x0000},
	{22579200, 44100, 256*2, 0x3172,0x1010},

	/* 48k */
	{12288000, 48000, 256*1, 0x3072,0x0000},
	{24576000, 48000, 256*2, 0x3172,0x1010},

};


static int get_coeff(int mclk, int rate)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}
	return -EINVAL;
}


static int alc5624_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
#else	
	struct snd_soc_codec *codec = rtd->codec;
#endif
	struct alc5624_priv *alc5624 = snd_soc_codec_get_drvdata(codec);
	
	u16 iface=snd_soc_read(codec,ALC5624_MAIN_SDP_CTRL)&0xfff3;
	int coeff = get_coeff(alc5624->sysclk, params_rate(params));

	dev_dbg(codec->dev, "%s()\n", __FUNCTION__);	
	
	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		iface |= 0x0000;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0008;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= 0x000c;
		break;
	default:
		return -EINVAL;
	}

	/* set iface & srate */
	snd_soc_write(codec, ALC5624_MAIN_SDP_CTRL, iface);
	if (coeff >= 0)
	{
		snd_soc_write(codec, ALC5624_STEREO_DAC_CLK_CTRL1,coeff_div[coeff].regvalue1);
		snd_soc_write(codec, ALC5624_STEREO_DAC_CLK_CTRL2,coeff_div[coeff].regvalue2);
	}
	return 0;
}

static int alc5624_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	dev_dbg(codec->dev, "%s()\n", __FUNCTION__);

	if (mute) {
		snd_soc_update_bits(codec, ALC5624_MISC_CTRL, (0x3<<5), (0x3<<5));
	} else {
		snd_soc_update_bits(codec, ALC5624_MISC_CTRL, (0x3<<5), 0);
	}
	return 0;
}

static int alc5624_set_bias_level(struct snd_soc_codec *codec,
				      enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
	
		// FULL ON-----power on all power not controlled by DAPM
		snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD1,0xE803,0x2803);
		snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD2,0x2400,0x2000);
		snd_soc_update_bits(codec,ALC5624_PD_CTRL_STAT,0x2C00,0x0000);
		
		break;
		
	case SND_SOC_BIAS_PREPARE:
	
		//LOW ON----- power on all power not controlled by DAPM
		snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD1,0xE803,0x2803);
		snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD2,0x2400,0x2000);
		snd_soc_update_bits(codec,ALC5624_PD_CTRL_STAT,0x2C00,0x0000);

		break;
		
	case SND_SOC_BIAS_STANDBY:
	
		/* everything off except vref/vmid, */
		snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD1,0xE803,0x0003);
		snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD2,0x2400,0x2000);
		snd_soc_update_bits(codec,ALC5624_PD_CTRL_STAT,0x2C00,0x0000);

		break;
		
	case SND_SOC_BIAS_OFF:
	
		/* everything off */
		snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD1,0xE803,0x0000);
		snd_soc_update_bits(codec,ALC5624_PWR_MANAG_ADD2,0x2400,0x0000);
		snd_soc_update_bits(codec,ALC5624_PD_CTRL_STAT,0x2C00,0x2C00);
	
		break;
	}
	
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	codec->dapm.bias_level = level;
#else
	codec->bias_level = level;
#endif
	return 0;
}
 
#define ALC5624_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32_LE)
			
static struct snd_soc_dai_ops alc5624_dai_ops = {
		.startup		= alc5624_dai_startup,
		.shutdown		= alc5624_dai_shutdown,
		.hw_params 		= alc5624_pcm_hw_params,
		.digital_mute 	= alc5624_digital_mute,
		.set_fmt 		= alc5624_set_dai_fmt,
		.set_sysclk 	= alc5624_set_dai_sysclk,
		.set_pll 		= alc5624_set_dai_pll,
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
static struct snd_soc_dai_driver alc5624_dai = {
#else
struct snd_soc_dai alc5624_dai = {
#endif
	.name = "alc5624-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =	8000,
		.rate_max =	48000,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = ALC5624_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =	8000,
		.rate_max =	48000,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = ALC5624_FORMATS,},
	.ops = &alc5624_dai_ops,
	.symmetric_rates = 1,
}; 

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)
EXPORT_SYMBOL_GPL(alc5624_dai);
#endif

/* Check if a register is volatile or not to forbid or not caching its value */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
static int alc5624_volatile_register(struct snd_soc_codec *codec,
	unsigned int reg)
#else
static int alc5624_volatile_register(unsigned int reg)
#endif
{
	if (reg == ALC5624_PD_CTRL_STAT ||
		reg == ALC5624_GPIO_PIN_STATUS ||
		reg == ALC5624_OVER_TEMP_CURR_STATUS ||
		reg == ALC5624_INDEX_DATA ||
		reg == ALC5624_EQ_STATUS)
		return 1;
	return 0;
}

/* read alc5624 hw register */
static unsigned int alc5624_hw_read(struct snd_soc_codec *codec,
	unsigned int reg)
{
	struct alc5624_priv *alc5624 = snd_soc_codec_get_drvdata(codec);
	
	/* If it is a real register, call the original bus access function */
	if (reg <= ALC5624_VENDOR_ID2) {
		/* bus_hw_read expects that codec->control_data is pointing to 
		   the original control_data.That is the only field accessed. Create
		   a temporary struct with the required data */
		struct snd_soc_codec tmpcodec;
		tmpcodec.control_data = alc5624->bus_control_data;
		return alc5624->bus_hw_read(&tmpcodec,reg);
	}
	
	/* If dealing with one of the virtual mixers, return 0. This fn 
	   won't be called anymore, as the cache will hold the written
	   value, and that value will be used instead of the actual value
	   of the nonexistant register */
	if (reg == VIRTUAL_HPL_MIXER ||
		reg == VIRTUAL_HPR_MIXER)
		return 0;
	
	/* Dealing with one of the indexed registers. Perform the access */
	if (reg >= VIRTUAL_IDX_BASE &&
		reg < REGISTER_COUNT) {
		
		struct snd_soc_codec tmpcodec;		
		u8 data[3];
		int ret;
		
		/* Access the indexed register */
		data[0] = ALC5624_INDEX_ADDRESS;
		data[1] = (u8)((reg-VIRTUAL_IDX_BASE) >> 8);
		data[2] = (u8)(reg-VIRTUAL_IDX_BASE);
		if ((ret = alc5624->bus_hw_write(alc5624->bus_control_data,data,3)) < 0)
			return ret;
			
		/* Get its value and return it:  */
		/* bus_hw_read expects that codec->control_data is pointing to 
		   the original control_data.That is the only field accessed. Create
		   a temporary struct with the required data */
		tmpcodec.control_data = alc5624->bus_control_data;
		return alc5624->bus_hw_read(&tmpcodec,ALC5624_INDEX_DATA);
	}
	
	/* Register does not exist */
	return -EIO;
}

/* write to the alc5624 register space */
static int alc5624_hw_write(void* control_data,const char* data_in_s,int len)
{
	struct alc5624_priv *alc5624 = control_data;
	u8* data_in = (u8*)data_in_s;
	
	/* If it is a real register, call the original bus access function */
	if (data_in[0] <= ALC5624_VENDOR_ID2)
		return alc5624->bus_hw_write(alc5624->bus_control_data,data_in_s,len);
	
	/* If dealing with one of the virtual mixers, discard the value, as there
	   is no real hw register. This value will also be stored in the register
	   cache, and that value will be used instead of the actual value of the
	   nonexistant register, when read */
	if (data_in[0] == VIRTUAL_HPL_MIXER ||
		data_in[0] == VIRTUAL_HPR_MIXER)
		return 0;
	
	/* Dealing with one of the indexed registers. Perform the access */
	if (data_in[0] >= VIRTUAL_IDX_BASE &&
		data_in[0] < REGISTER_COUNT) {
		
		u8 data[3];
		int ret;
		
		/* Access the indexed register */
		data[0] = ALC5624_INDEX_ADDRESS;
		data[1] = 0; /* hi */
		data[2] = data_in[0]-VIRTUAL_IDX_BASE; /* lo */
		if ((ret = alc5624->bus_hw_write(alc5624->bus_control_data,data,3)) < 0)
			return ret;

		/* Set its value and return */			
		data[0] = ALC5624_INDEX_DATA;
		data[1] = data_in[1];
		data[2] = data_in[2];
		return alc5624->bus_hw_write(alc5624->bus_control_data,data,3);
	}
	
	/* Register does not exist */
	return -EIO;
}

/* Reset the codec */
static int alc5624_reset(struct snd_soc_codec *codec)
{
	struct alc5624_priv *alc5624 = snd_soc_codec_get_drvdata(codec);
	u8 data[3];
	
	data[0] = ALC5624_RESET;
	data[1] = 0;
	data[2] = 0;
	return alc5624->bus_hw_write(alc5624->bus_control_data, data, 3);
}


/* Fetch register values from the hw */
static void alc5624_fill_cache(struct snd_soc_codec *codec)
{
	/*struct alc5624_priv *alc5624 = snd_soc_codec_get_drvdata(codec);*/
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)
	int i, step = 2, size = REGISTER_COUNT;
#else
	int i, step = codec->driver->reg_cache_step, size = codec->driver->reg_cache_size;
#endif
	u16 *cache = codec->reg_cache;
	for (i = 0 ; i < size ; i += step)
		cache[i] = alc5624_hw_read(codec, i);
}

/* Sync reg_cache with the hardware - But skip the RESET address */
static int alc5624_sync(struct snd_soc_codec *codec)
{
	/*struct alc5624_priv *alc5624 = snd_soc_codec_get_drvdata(codec);*/
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)
	int i, step = 2, size = REGISTER_COUNT;
#else
	int i, step = codec->driver->reg_cache_step, size = codec->driver->reg_cache_size;
#endif
	u16 *cache = codec->reg_cache;
	u8 data[3];

	for (i = 2 ; i < size ; i += step) {
		data[0] = i;
		data[1] = cache[i] >> 8;
		data[2] = cache[i];
		alc5624_hw_write(codec->control_data, data, 3);
	}

	return 0;
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
static int alc5624_suspend(struct snd_soc_codec *codec, pm_message_t mesg)
{
#else
static int alc5624_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
#endif
	struct alc5624_priv *alc5624 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s()\n", __FUNCTION__);
	alc5624_set_bias_level(codec, SND_SOC_BIAS_OFF);
	
	/* Disable the codec MCLK */
	clk_disable(alc5624->mclk);
	
	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
static int alc5624_resume(struct snd_soc_codec *codec)
{
	/* Get the requested mode */
	enum snd_soc_bias_level level = codec->dapm.suspend_bias_level;
#else
static int alc5624_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	/* Get the requested mode */
	enum snd_soc_bias_level level = codec->suspend_bias_level;
#endif
	struct alc5624_priv *alc5624 = snd_soc_codec_get_drvdata(codec);
	
	dev_dbg(codec->dev, "%s()\n", __FUNCTION__);	
	
	/* Enable the codec MCLK */
	clk_enable(alc5624->mclk);
	
	/* Reset the codec */
	alc5624_reset(codec);
	mdelay(1);
	alc5624_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* Sync reg_cache with the hardware */
	alc5624_sync(codec);

	/* charge alc5624 to the requested mode */
	alc5624_set_bias_level(codec, level);

	return 0;
}

static int alc5624_probe(struct snd_soc_codec *codec)
{
	struct alc5624_priv *alc5624 = snd_soc_codec_get_drvdata(codec);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)	
	struct snd_soc_dapm_context *dapm = &codec->dapm;
#endif
	int ret,i;

	/* Get the default read and write functions for this bus */
	ret = snd_soc_codec_set_cache_io(codec, 8, 16, alc5624->control_type);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	/* Get the original hw R/W functions */
	alc5624->bus_hw_read = codec->hw_read;
	alc5624->bus_hw_write = codec->hw_write;
	alc5624->bus_control_data = codec->control_data;
	
	/* And install our own functions to be able to provide virtual registers */
	codec->hw_read = alc5624_hw_read;
	codec->hw_write = alc5624_hw_write;
	codec->control_data = alc5624;
	
	/* Reset the codec */
	alc5624_reset(codec);
	mdelay(1);
	
	/* Set the default values into the CODEC */
	for (i = 0; i < ARRAY_SIZE(alc5624_reg_default); i++) {
		snd_soc_write(codec,alc5624_reg_default[i].reg,alc5624_reg_default[i].val);
	}
	
	/* Fill cache with the register values */
	alc5624_fill_cache(codec);

	/* power on device */
	alc5624_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	/* On linux 2.6.38+ we need to register controls here */
	snd_soc_add_controls(codec, alc5624_snd_controls,
					ARRAY_SIZE(alc5624_snd_controls));

	snd_soc_dapm_new_controls(dapm, alc5624_dapm_widgets,
					ARRAY_SIZE(alc5624_dapm_widgets));

	/* set up audio path interconnects */
	snd_soc_dapm_add_routes(dapm, audio_map, ARRAY_SIZE(audio_map));
#endif
	
	return ret;
}

/* power down chip */
static int alc5624_remove(struct snd_soc_codec *codec)
{
	alc5624_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
static struct snd_soc_codec_driver soc_codec_device_alc5624 = {
	.probe 				= alc5624_probe,
	.remove 			= alc5624_remove,
	.suspend 			= alc5624_suspend,
	.resume 			= alc5624_resume,
	.volatile_register 	= alc5624_volatile_register,
	.set_bias_level 	= alc5624_set_bias_level,
	.reg_cache_size 	= REGISTER_COUNT,
	.reg_word_size 		= sizeof(u16),
	.reg_cache_step 	= 2,
};
#endif

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)	
static struct snd_soc_codec *alc5624_codec = NULL;
#endif

static __devinit int alc5624_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct alc5624_platform_data *pdata;
	struct alc5624_priv *alc5624;
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)		
	struct snd_soc_codec *codec;
#endif
	struct clk* mclk;
	
	int ret, vid1, vid2, ver;

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "Missing platform data\n");	
		return -ENODEV;
	}
	
	/* Get the MCLK */
	mclk = clk_get(NULL, pdata->mclk);
	if (IS_ERR(mclk)) {
		dev_err(&client->dev, "Unable to get MCLK\n");
		return -ENODEV;
	} 
	
	/* Enable it */
	clk_enable(mclk);
	
	/* Read chip ids */
	vid1 = i2c_smbus_read_word_data(client, ALC5624_VENDOR_ID1);
	if (vid1 < 0) {
		dev_err(&client->dev, "failed to read I2C\n");
		clk_disable(mclk);
		clk_put(mclk);
		return -EIO;
	}
	vid1 = ((vid1 & 0xff) << 8) | (vid1 >> 8);

	vid2 = i2c_smbus_read_word_data(client, ALC5624_VENDOR_ID2);
	if (vid2 < 0) {
		dev_err(&client->dev, "failed to read I2C\n");
		clk_disable(mclk);
		clk_put(mclk);
		return -EIO;
	}
	ver  = (vid2 >> 8) & 0xff;	/* Version */
	vid2 = (vid2 & 0xff); 		/* Device */

	if ((vid1 != 0x10ec) || (vid2 != id->driver_data)) {
		dev_err(&client->dev, "unknown or wrong codec\n");
		dev_err(&client->dev, "Expected %x:%lx, got %x:%x\n",
				0x10ec, id->driver_data,
				vid1, vid2);
		clk_disable(mclk);
		clk_put(mclk);
		return -ENODEV;
	}

	dev_dbg(&client->dev, "Found codec id : alc5624, Version: %d\n", ver);

	alc5624 = kzalloc(sizeof(struct alc5624_priv), GFP_KERNEL);
	if (alc5624 == NULL) {
		dev_err(&client->dev, "no memory for context\n");
		clk_disable(mclk);
		clk_put(mclk);
		return -ENOMEM;
	}


	alc5624->id = vid2;
	
	/* Store the MCLK clock handle */
	alc5624->mclk = mclk; 

	i2c_set_clientdata(client, alc5624);
	alc5624->control_data = client;
	alc5624->control_type = SND_SOC_I2C;

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)	
	/* linux 2.6.36 version setup is quite by hand */

	codec = &alc5624->codec;
	snd_soc_codec_set_drvdata(codec, alc5624);
	codec->reg_cache = &alc5624->reg_cache[0];
	codec->reg_cache_size = REGISTER_COUNT;

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	codec->name = "ALC5624";
	codec->owner = THIS_MODULE;
	codec->dai = &alc5624_dai;
	codec->num_dai = 1;
	codec->control_data = client;
	codec->set_bias_level = alc5624_set_bias_level;
	codec->volatile_register = alc5624_volatile_register;
	codec->dev = &client->dev;
	alc5624_dai.dev = &client->dev;
	alc5624_codec = codec; /* so later probe can attach the codec to the card */
	
	/* call the codec probe function */
	ret = alc5624_probe(codec);
	if (ret != 0) {
		dev_err(&client->dev, "Failed to probe codec: %d\n", ret);	
		clk_disable(mclk);
		clk_put(mclk);
		kfree(alc5624);
		return ret;
	}
	

	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		dev_err(&client->dev, "Failed to register codec: %d\n", ret);	
		clk_disable(mclk);
		clk_put(mclk);
		kfree(alc5624);
		return ret;
	}

	ret = snd_soc_register_dai(&alc5624_dai);
	if (ret != 0) {
		dev_err(&client->dev, "Failed to register DAI: %d\n", ret);	
		snd_soc_unregister_codec(codec);
		clk_disable(mclk);
		clk_put(mclk);
		kfree(alc5624);
		return ret;
	}
	
#else
	/* linux 2.6.38 setup is very streamlined :) */
	ret = snd_soc_register_codec(&client->dev,
		&soc_codec_device_alc5624, &alc5624_dai, 1);
	if (ret != 0) {
		dev_err(&client->dev, "Failed to register codec: %d\n", ret);
		clk_disable(mclk);
		clk_put(mclk);
		kfree(alc5624);
	}
#endif

	return ret;
}

static __devexit int alc5624_i2c_remove(struct i2c_client *client)
{
	struct alc5624_priv *alc5624 = i2c_get_clientdata(client);

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)	
	/* linux 2.6.36 version device removal is quite by hand */
	
	alc5624_remove(&alc5624->codec);
	
	snd_soc_unregister_dai(&alc5624_dai);
	snd_soc_unregister_codec(&alc5624->codec);

	alc5624_dai.dev = NULL;
	alc5624_codec = NULL;

#else
	/* linux 2.6.38 device removal is very streamlined :) */
	snd_soc_unregister_codec(&client->dev);
	
#endif
	/* Disable and release clock */
	clk_disable(alc5624->mclk);
	clk_put(alc5624->mclk);
	
	kfree(alc5624);
	return 0;
}

static const struct i2c_device_id alc5624_i2c_table[] = {
	{"alc5624", 0x20},
	{}
};
MODULE_DEVICE_TABLE(i2c, alc5624_i2c_table);

/*  i2c codec control layer */
static struct i2c_driver alc5624_i2c_driver = {
	.driver = {
		.name = "alc5624",
		.owner = THIS_MODULE,
	},
	.probe = alc5624_i2c_probe,
	.remove =  __devexit_p(alc5624_i2c_remove),
	.id_table = alc5624_i2c_table,
};

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)	
static int alc5624_plat_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	int ret = 0;

	if (!alc5624_codec) {
		dev_err(&pdev->dev, "I2C client not yet instantiated\n");
		return -ENODEV;
	}

	/* Associate the codec to the card */
	socdev->card->codec = alc5624_codec;

	/* Register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register new PCMs\n");
	}

	/* Register the controls and widgets */
	snd_soc_add_controls(alc5624_codec, alc5624_snd_controls,
					ARRAY_SIZE(alc5624_snd_controls));

	snd_soc_dapm_new_controls(alc5624_codec, alc5624_dapm_widgets,
					ARRAY_SIZE(alc5624_dapm_widgets));

	/* set up audio path interconnects */
	snd_soc_dapm_add_routes(alc5624_codec, audio_map, ARRAY_SIZE(audio_map));
	
	return ret;
}

/* power down chip */
static int alc5624_plat_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	/* Release PCMs and DAPM controls */
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_alc5624 = {
	.probe = 	alc5624_plat_probe,
	.remove = 	alc5624_plat_remove,
	.suspend = 	alc5624_suspend,
	.resume =	alc5624_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_alc5624);
#endif

static int __init alc5624_modinit(void)
{
	int ret;

	ret = i2c_add_driver(&alc5624_i2c_driver);
	if (ret != 0) {
		pr_err("%s: can't add i2c driver", __func__);
		return ret;
	}

	return ret;
}
module_init(alc5624_modinit);

static void __exit alc5624_modexit(void)
{
	i2c_del_driver(&alc5624_i2c_driver);
}
module_exit(alc5624_modexit);

MODULE_DESCRIPTION("ASoC ALC5624 driver");
MODULE_AUTHOR("Eduardo José Tagle <ejtagle@tutopia.com>");
MODULE_LICENSE("GPL");  

