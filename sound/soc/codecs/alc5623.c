/*
 * alc5623.c  --  alc562[123] ALSA Soc Audio driver
 *
 * Copyright 2008 Realtek Microelectronics
 * Author: flove <flove@realtek.com> Ethan <eku@marvell.com>
 *
 * Copyright 2010 Arnaud Patard <arnaud.patard@rtp-net.org>
 *
 *
 * Based on WM8753.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/alc5623.h>

#include "alc5623.h"

#define REGISTER_COUNT ALC5623_VENDOR_ID2+2

static int caps_charge = 2000;
module_param(caps_charge, int, 0);
MODULE_PARM_DESC(caps_charge, "ALC5623 cap charge time (msecs)");

/* codec private data */
struct alc5623_priv {

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)
	/* Older versions of ALSA SoC require from us to hold the register
	   cache ourselves and a codec reference */
	struct snd_soc_codec codec;
	u16 reg_cache[REGISTER_COUNT];
#endif
	struct clk* 	mclk;			/* the master clock */
	unsigned int	linevdd_mv;		/* Line Vdd in millivolts */
	enum snd_soc_control_type control_type;
	void *control_data;
	struct mutex mutex;
	u8 id;
	unsigned int sysclk;
	int using_pll;		/* If using PLL */
	unsigned int add_ctrl;
	unsigned int jack_det_ctrl;
};

static const struct {
        u16 reg;        /* register */
        u16 val;        /* value */
} alc5623_reg_default[] = {
        {ALC5623_SPK_OUT_VOL                            , 0x4040 }, /* Unmute left and right channels, enable 0 cross detector, 0db volume */
        {ALC5623_HP_OUT_VOL                             , 0x4040 }, /* Unmute left and right channels, enable 0 cross detector, 0db volume */
        {ALC5623_MONO_AUX_OUT_VOL                       , 0xFFFF }, /* Phone input muted, Mono output muted */
        {ALC5623_AUXIN_VOL                             , 0xFF1F }, /* Mute Aux In volume */
        {ALC5623_LINE_IN_VOL                            , 0xFF1F }, /* Mute Line In volume */
        {ALC5623_STEREO_DAC_VOL                         , 0x6808 }, /* Mute volume output to Mono and Speaker */
        {ALC5623_MIC_VOL                                , 0x0808 }, /* Mic volume = 0db */
        {ALC5623_MIC_ROUTING_CTRL                       , 0xF0F0 }, /* Mute mic volume to Headphone, Speaker and Mono mixers, Differential mode enabled */
        {ALC5623_ADC_REC_GAIN                           , 0xF58B },
        {ALC5623_ADC_REC_MIXER                          , 0x3F3F }, /* Mic1 as recording sources */
        {ALC5623_OUTPUT_MIXER_CTRL                      , 0x5700 },
        {ALC5623_MIC_CTRL                               , 0x0F02 }, /* 1.8uA short current det, Bias volt =0.9Avdd, +40db gain boost */
        {ALC5623_DAI_CONTROL                            , 0x8000 }, /* Slave interfase */
        {ALC5623_STEREO_AD_DA_CLK_CTRL           	, 0x166d},
        {ALC5623_PWR_MANAG_ADD1                         , 0x9D66 }, 
        {ALC5623_PWR_MANAG_ADD2                         , 0xB7FF }, 
        {ALC5623_PWR_MANAG_ADD3                         , 0xF6FF }, 
        {ALC5623_ADD_CTRL_REG                           , 0x5300 }, 
        {ALC5623_GLOBAL_CLK_CTRL_REG                    , 0x0000 }, 
        {ALC5623_PLL_CTRL                               , 0x0000 },
        {ALC5623_GPIO_OUTPUT_PIN_CTRL                   , 0x0002 }, /* Drive High */
        {ALC5623_GPIO_PIN_CONFIG                        , 0x040E }, /* All GPIOs as input */
        {ALC5623_GPIO_PIN_POLARITY                      , 0x040E }, /* All GPIOs high active */
        {ALC5623_GPIO_PIN_STICKY                        , 0x0000 }, /* No sticky ops */
        {ALC5623_GPIO_PIN_WAKEUP                        , 0x0000 }, /* No wakeups */
        {ALC5623_GPIO_PIN_SHARING                       , 0x0000 },
        {ALC5623_JACK_DET_CTRL                          , 0x4180 }, /*jackdetect gpio, headout high, lineout low */
        {ALC5623_MISC_CTRL                              , 0x8000 }, /* Slow Vref */
        {ALC5623_PSEUDO_SPATIAL_CTRL  		        , 0x0003 }, /* Disable */
};

static void alc5623_fill_cache(struct snd_soc_codec *codec)
{

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)
	int i, step = codec->reg_cache_step, size = codec->reg_cache_size;
#else
	int i, step = codec->driver->reg_cache_step, size = codec->driver->reg_cache_size;
#endif
	u16 *cache = codec->reg_cache;

	for (i = 0 ; i < size ; i += step){
		cache[i] = codec->hw_read(codec, i);
	}
}

static void alc5623_sync_cache(struct snd_soc_codec *codec)
{

	/*struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);*/
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)
	int i, step = 2, size = REGISTER_COUNT;
#else
	int i, step = codec->driver->reg_cache_step, size = codec->driver->reg_cache_size;
#endif
	u16 *cache = codec->reg_cache;
	u8 data[3];

	/* Already synchronized, no need to resync again */
	if (!codec->cache_sync)
		return;

	codec->cache_only = 0;

	/* Sync back cached values if they're different from the
	 * hardware default.
	 */
	for (i = 2 ; i < size ; i += step) {
		data[0] = i;
		data[1] = cache[i] >> 8;
		data[2] = cache[i];
		codec->hw_write(codec->control_data, data, 3);
	}

	codec->cache_sync = 0;
};

static inline int alc5623_reset(struct snd_soc_codec *codec)
{
	return snd_soc_write(codec, ALC5623_RESET, 0);
}

static int amp_mixer_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	/* to power-on/off class-d amp generators/speaker */
	/* need to write to 'index-46h' register :        */
	/* so write index num (here 0x46) to reg 0x6a     */
	/* and then 0xffff/0 to reg 0x6c                  */
	snd_soc_write(w->codec, ALC5623_HID_CTRL_INDEX, 0x46);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_write(w->codec, ALC5623_HID_CTRL_DATA, 0xFFFF);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_write(w->codec, ALC5623_HID_CTRL_DATA, 0);
		break;
	}

	return 0;
}

/*
 * ALC5623 Controls
 */

static const DECLARE_TLV_DB_SCALE(vol_tlv, -3450, 150, 0);
static const DECLARE_TLV_DB_SCALE(hp_tlv, -4650, 150, 0);
static const DECLARE_TLV_DB_SCALE(adc_rec_tlv, -1650, 150, 0);
static const unsigned int boost_tlv[] = {
	TLV_DB_RANGE_HEAD(3),
	0, 0, TLV_DB_SCALE_ITEM(0, 0, 0),
	1, 1, TLV_DB_SCALE_ITEM(2000, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(3000, 0, 0),
};
static const DECLARE_TLV_DB_SCALE(dig_tlv, 0, 600, 0);

static const struct snd_kcontrol_new rt5621_vol_snd_controls[] = {
	SOC_DOUBLE_TLV("Speaker Playback Volume",
			ALC5623_SPK_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Speaker Playback Switch",
			ALC5623_SPK_OUT_VOL, 15, 7, 1, 1),
	SOC_DOUBLE_TLV("Headphone Playback Volume",
			ALC5623_HP_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Headphone Playback Switch",
			ALC5623_HP_OUT_VOL, 15, 7, 1, 1),
};

static const struct snd_kcontrol_new rt5622_vol_snd_controls[] = {
	SOC_DOUBLE_TLV("Speaker Playback Volume",
			ALC5623_SPK_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Speaker Playback Switch",
			ALC5623_SPK_OUT_VOL, 15, 7, 1, 1),
	SOC_DOUBLE_TLV("Line Playback Volume",
			ALC5623_HP_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Line Playback Switch",
			ALC5623_HP_OUT_VOL, 15, 7, 1, 1),
};

static const struct snd_kcontrol_new alc5623_vol_snd_controls[] = {
	SOC_DOUBLE_TLV("Line Playback Volume",
			ALC5623_SPK_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Line Playback Switch",
			ALC5623_SPK_OUT_VOL, 15, 7, 1, 1),
	SOC_DOUBLE_TLV("Headphone Playback Volume",
			ALC5623_HP_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Headphone Playback Switch",
			ALC5623_HP_OUT_VOL, 15, 7, 1, 1),
};

static const struct snd_kcontrol_new alc5623_snd_controls[] = {
	SOC_DOUBLE_TLV("Auxout Playback Volume",
			ALC5623_MONO_AUX_OUT_VOL, 8, 0, 31, 1, hp_tlv),
	SOC_DOUBLE("Auxout Playback Switch",
			ALC5623_MONO_AUX_OUT_VOL, 15, 7, 1, 1),
	SOC_DOUBLE_TLV("PCM Playback Volume",
			ALC5623_STEREO_DAC_VOL, 8, 0, 31, 1, vol_tlv),
	SOC_DOUBLE_TLV("AuxI Capture Volume",
			ALC5623_AUXIN_VOL, 8, 0, 31, 1, vol_tlv),
	SOC_DOUBLE_TLV("LineIn Capture Volume",
			ALC5623_LINE_IN_VOL, 8, 0, 31, 1, vol_tlv),
	SOC_SINGLE_TLV("Mic1 Capture Volume",
			ALC5623_MIC_VOL, 8, 31, 1, vol_tlv),
	SOC_SINGLE_TLV("Mic2 Capture Volume",
			ALC5623_MIC_VOL, 0, 31, 1, vol_tlv),
	SOC_DOUBLE_TLV("Rec Capture Volume",
			ALC5623_ADC_REC_GAIN, 7, 0, 31, 0, adc_rec_tlv),
	SOC_SINGLE_TLV("Mic 1 Boost Volume",
			ALC5623_MIC_CTRL, 10, 2, 0, boost_tlv),
	SOC_SINGLE_TLV("Mic 2 Boost Volume",
			ALC5623_MIC_CTRL, 8, 2, 0, boost_tlv),
	SOC_SINGLE_TLV("Digital Boost Volume",
			ALC5623_ADD_CTRL_REG, 4, 3, 0, dig_tlv),
};

/*
 * DAPM Controls
 */
static const struct snd_kcontrol_new alc5623_hp_mixer_controls[] = {
SOC_DAPM_SINGLE("LI2HP Playback Switch", ALC5623_LINE_IN_VOL, 15, 1, 1),
SOC_DAPM_SINGLE("AUXI2HP Playback Switch", ALC5623_AUXIN_VOL, 15, 1, 1),
SOC_DAPM_SINGLE("MIC12HP Playback Switch", ALC5623_MIC_ROUTING_CTRL, 15, 1, 1),
SOC_DAPM_SINGLE("MIC22HP Playback Switch", ALC5623_MIC_ROUTING_CTRL, 7, 1, 1),
SOC_DAPM_SINGLE("DAC2HP Playback Switch", ALC5623_STEREO_DAC_VOL, 15, 1, 1),
};

static const struct snd_kcontrol_new alc5623_hpl_mixer_controls[] = {
SOC_DAPM_SINGLE("ADC2HP_L Playback Switch", ALC5623_ADC_REC_GAIN, 15, 1, 1),
};

static const struct snd_kcontrol_new alc5623_hpr_mixer_controls[] = {
SOC_DAPM_SINGLE("ADC2HP_R Playback Switch", ALC5623_ADC_REC_GAIN, 14, 1, 1),
};

static const struct snd_kcontrol_new alc5623_mono_mixer_controls[] = {
SOC_DAPM_SINGLE("ADC2MONO_L Playback Switch", ALC5623_ADC_REC_GAIN, 13, 1, 1),
SOC_DAPM_SINGLE("ADC2MONO_R Playback Switch", ALC5623_ADC_REC_GAIN, 12, 1, 1),
SOC_DAPM_SINGLE("LI2MONO Playback Switch", ALC5623_LINE_IN_VOL, 13, 1, 1),
SOC_DAPM_SINGLE("AUXI2MONO Playback Switch", ALC5623_AUXIN_VOL, 13, 1, 1),
SOC_DAPM_SINGLE("MIC12MONO Playback Switch", ALC5623_MIC_ROUTING_CTRL, 13, 1, 1),
SOC_DAPM_SINGLE("MIC22MONO Playback Switch", ALC5623_MIC_ROUTING_CTRL, 5, 1, 1),
SOC_DAPM_SINGLE("DAC2MONO Playback Switch", ALC5623_STEREO_DAC_VOL, 13, 1, 1),
};

static const struct snd_kcontrol_new alc5623_speaker_mixer_controls[] = {
SOC_DAPM_SINGLE("LI2SPK Playback Switch", ALC5623_LINE_IN_VOL, 14, 1, 1),
SOC_DAPM_SINGLE("AUXI2SPK Playback Switch", ALC5623_AUXIN_VOL, 14, 1, 1),
SOC_DAPM_SINGLE("MIC12SPK Playback Switch", ALC5623_MIC_ROUTING_CTRL, 14, 1, 1),
SOC_DAPM_SINGLE("MIC22SPK Playback Switch", ALC5623_MIC_ROUTING_CTRL, 6, 1, 1),
SOC_DAPM_SINGLE("DAC2SPK Playback Switch", ALC5623_STEREO_DAC_VOL, 14, 1, 1),
};

/* Left Record Mixer */
static const struct snd_kcontrol_new alc5623_captureL_mixer_controls[] = {
SOC_DAPM_SINGLE("Mic1 Capture Switch", ALC5623_ADC_REC_MIXER, 14, 1, 1),
SOC_DAPM_SINGLE("Mic2 Capture Switch", ALC5623_ADC_REC_MIXER, 13, 1, 1),
SOC_DAPM_SINGLE("LineInL Capture Switch", ALC5623_ADC_REC_MIXER, 12, 1, 1),
SOC_DAPM_SINGLE("Left AuxI Capture Switch", ALC5623_ADC_REC_MIXER, 11, 1, 1),
SOC_DAPM_SINGLE("HPMixerL Capture Switch", ALC5623_ADC_REC_MIXER, 10, 1, 1),
SOC_DAPM_SINGLE("SPKMixer Capture Switch", ALC5623_ADC_REC_MIXER, 9, 1, 1),
SOC_DAPM_SINGLE("MonoMixer Capture Switch", ALC5623_ADC_REC_MIXER, 8, 1, 1),
};

/* Right Record Mixer */
static const struct snd_kcontrol_new alc5623_captureR_mixer_controls[] = {
SOC_DAPM_SINGLE("Mic1 Capture Switch", ALC5623_ADC_REC_MIXER, 6, 1, 1),
SOC_DAPM_SINGLE("Mic2 Capture Switch", ALC5623_ADC_REC_MIXER, 5, 1, 1),
SOC_DAPM_SINGLE("LineInR Capture Switch", ALC5623_ADC_REC_MIXER, 4, 1, 1),
SOC_DAPM_SINGLE("Right AuxI Capture Switch", ALC5623_ADC_REC_MIXER, 3, 1, 1),
SOC_DAPM_SINGLE("HPMixerR Capture Switch", ALC5623_ADC_REC_MIXER, 2, 1, 1),
SOC_DAPM_SINGLE("SPKMixer Capture Switch", ALC5623_ADC_REC_MIXER, 1, 1, 1),
SOC_DAPM_SINGLE("MonoMixer Capture Switch", ALC5623_ADC_REC_MIXER, 0, 1, 1),
};

static const char *alc5623_spk_n_sour_sel[] = {
		"RN/-R", "RP/+R", "LN/-R", "Vmid" };
static const char *alc5623_hpl_out_input_sel[] = {
		"Vmid", "HP Left Mix"};
static const char *alc5623_hpr_out_input_sel[] = {
		"Vmid", "HP Right Mix"};
static const char *alc5623_spkout_input_sel[] = {
		"Vmid", "HPOut Mix", "Speaker Mix", "Mono Mix"};
static const char *alc5623_aux_out_input_sel[] = {
		"Vmid", "HPOut Mix", "Speaker Mix", "Mono Mix"};

/* auxout output mux */
static const struct soc_enum alc5623_aux_out_input_enum =
SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 6, 4, alc5623_aux_out_input_sel);
static const struct snd_kcontrol_new alc5623_auxout_mux_controls =
SOC_DAPM_ENUM("Route", alc5623_aux_out_input_enum);

/* speaker output mux */
static const struct soc_enum alc5623_spkout_input_enum =
SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 10, 4, alc5623_spkout_input_sel);
static const struct snd_kcontrol_new alc5623_spkout_mux_controls =
SOC_DAPM_ENUM("Route", alc5623_spkout_input_enum);

/* headphone left output mux */
static const struct soc_enum alc5623_hpl_out_input_enum =
SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 9, 2, alc5623_hpl_out_input_sel);
static const struct snd_kcontrol_new alc5623_hpl_out_mux_controls =
SOC_DAPM_ENUM("Route", alc5623_hpl_out_input_enum);

/* headphone right output mux */
static const struct soc_enum alc5623_hpr_out_input_enum =
SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 8, 2, alc5623_hpr_out_input_sel);
static const struct snd_kcontrol_new alc5623_hpr_out_mux_controls =
SOC_DAPM_ENUM("Route", alc5623_hpr_out_input_enum);

/* speaker output N select */
static const struct soc_enum alc5623_spk_n_sour_enum =
SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 14, 4, alc5623_spk_n_sour_sel);
static const struct snd_kcontrol_new alc5623_spkoutn_mux_controls =
SOC_DAPM_ENUM("Route", alc5623_spk_n_sour_enum);

static const struct snd_soc_dapm_widget alc5623_dapm_widgets[] = {
/* Muxes */
SND_SOC_DAPM_MUX("AuxOut Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_auxout_mux_controls),
SND_SOC_DAPM_MUX("SpeakerOut Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_spkout_mux_controls),
SND_SOC_DAPM_MUX("Left Headphone Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_hpl_out_mux_controls),
SND_SOC_DAPM_MUX("Right Headphone Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_hpr_out_mux_controls),
SND_SOC_DAPM_MUX("SpeakerOut N Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_spkoutn_mux_controls),

/* output mixers */
SND_SOC_DAPM_MIXER("HP Mix", SND_SOC_NOPM, 0, 0,
	&alc5623_hp_mixer_controls[0],
	ARRAY_SIZE(alc5623_hp_mixer_controls)),
SND_SOC_DAPM_MIXER("HPR Mix", ALC5623_PWR_MANAG_ADD2, 4, 0,
	&alc5623_hpr_mixer_controls[0],
	ARRAY_SIZE(alc5623_hpr_mixer_controls)),
SND_SOC_DAPM_MIXER("HPL Mix", ALC5623_PWR_MANAG_ADD2, 5, 0,
	&alc5623_hpl_mixer_controls[0],
	ARRAY_SIZE(alc5623_hpl_mixer_controls)),
SND_SOC_DAPM_MIXER("HPOut Mix", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_MIXER("Mono Mix", ALC5623_PWR_MANAG_ADD2, 2, 0,
	&alc5623_mono_mixer_controls[0],
	ARRAY_SIZE(alc5623_mono_mixer_controls)),
SND_SOC_DAPM_MIXER("Speaker Mix", ALC5623_PWR_MANAG_ADD2, 3, 0,
	&alc5623_speaker_mixer_controls[0],
	ARRAY_SIZE(alc5623_speaker_mixer_controls)),

/* input mixers */
SND_SOC_DAPM_MIXER("Left Capture Mix", ALC5623_PWR_MANAG_ADD2, 1, 0,
	&alc5623_captureL_mixer_controls[0],
	ARRAY_SIZE(alc5623_captureL_mixer_controls)),
SND_SOC_DAPM_MIXER("Right Capture Mix", ALC5623_PWR_MANAG_ADD2, 0, 0,
	&alc5623_captureR_mixer_controls[0],
	ARRAY_SIZE(alc5623_captureR_mixer_controls)),

SND_SOC_DAPM_DAC("Left DAC", "Left HiFi Playback",
	ALC5623_PWR_MANAG_ADD2, 9, 0),
SND_SOC_DAPM_DAC("Right DAC", "Right HiFi Playback",
	ALC5623_PWR_MANAG_ADD2, 8, 0),
SND_SOC_DAPM_MIXER("I2S Mix", ALC5623_PWR_MANAG_ADD1, 15, 0, NULL, 0),
SND_SOC_DAPM_MIXER("AuxI Mix", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_MIXER("Line Mix", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_ADC("Left ADC", "Left HiFi Capture",
	ALC5623_PWR_MANAG_ADD2, 7, 0),
SND_SOC_DAPM_ADC("Right ADC", "Right HiFi Capture",
	ALC5623_PWR_MANAG_ADD2, 6, 0),
SND_SOC_DAPM_PGA("Left Headphone", ALC5623_PWR_MANAG_ADD3, 10, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Headphone", ALC5623_PWR_MANAG_ADD3, 9, 0, NULL, 0),
SND_SOC_DAPM_PGA("SpeakerOut", ALC5623_PWR_MANAG_ADD3, 12, 0, NULL, 0),
SND_SOC_DAPM_PGA("Left AuxOut", ALC5623_PWR_MANAG_ADD3, 14, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right AuxOut", ALC5623_PWR_MANAG_ADD3, 13, 0, NULL, 0),
SND_SOC_DAPM_PGA("Left LineIn", ALC5623_PWR_MANAG_ADD3, 7, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right LineIn", ALC5623_PWR_MANAG_ADD3, 6, 0, NULL, 0),
SND_SOC_DAPM_PGA("Left AuxI", ALC5623_PWR_MANAG_ADD3, 5, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right AuxI", ALC5623_PWR_MANAG_ADD3, 4, 0, NULL, 0),
SND_SOC_DAPM_PGA("MIC1 PGA", ALC5623_PWR_MANAG_ADD3, 3, 0, NULL, 0),
SND_SOC_DAPM_PGA("MIC2 PGA", ALC5623_PWR_MANAG_ADD3, 2, 0, NULL, 0),
SND_SOC_DAPM_PGA("MIC1 Pre Amp", ALC5623_PWR_MANAG_ADD3, 1, 0, NULL, 0),
SND_SOC_DAPM_PGA("MIC2 Pre Amp", ALC5623_PWR_MANAG_ADD3, 0, 0, NULL, 0),
SND_SOC_DAPM_MICBIAS("Mic Bias1", ALC5623_PWR_MANAG_ADD1, 11, 0),

SND_SOC_DAPM_OUTPUT("AUXOUTL"),
SND_SOC_DAPM_OUTPUT("AUXOUTR"),
SND_SOC_DAPM_OUTPUT("HPL"),
SND_SOC_DAPM_OUTPUT("HPR"),
SND_SOC_DAPM_OUTPUT("SPKOUT"),
SND_SOC_DAPM_OUTPUT("SPKOUTN"),
SND_SOC_DAPM_INPUT("LINEINL"),
SND_SOC_DAPM_INPUT("LINEINR"),
SND_SOC_DAPM_INPUT("AUXINL"),
SND_SOC_DAPM_INPUT("AUXINR"),
SND_SOC_DAPM_INPUT("MIC1"),
SND_SOC_DAPM_INPUT("MIC2"),
SND_SOC_DAPM_VMID("Vmid"),
};

static const char *alc5623_amp_names[] = {"AB Amp", "D Amp"};
static const struct soc_enum alc5623_amp_enum =
	SOC_ENUM_SINGLE(ALC5623_OUTPUT_MIXER_CTRL, 13, 2, alc5623_amp_names);
static const struct snd_kcontrol_new alc5623_amp_mux_controls =
	SOC_DAPM_ENUM("Route", alc5623_amp_enum);

static const struct snd_soc_dapm_widget alc5623_dapm_amp_widgets[] = {
SND_SOC_DAPM_PGA_E("D Amp", ALC5623_PWR_MANAG_ADD2, 14, 0, NULL, 0,
	amp_mixer_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
SND_SOC_DAPM_PGA("AB Amp", ALC5623_PWR_MANAG_ADD2, 15, 0, NULL, 0),
SND_SOC_DAPM_MUX("AB-D Amp Mux", SND_SOC_NOPM, 0, 0,
	&alc5623_amp_mux_controls),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* virtual mixer - mixes left & right channels */
	{"I2S Mix", NULL,				"Left DAC"},
	{"I2S Mix", NULL,				"Right DAC"},
	{"Line Mix", NULL,				"Right LineIn"},
	{"Line Mix", NULL,				"Left LineIn"},
	{"AuxI Mix", NULL,				"Left AuxI"},
	{"AuxI Mix", NULL,				"Right AuxI"},
	{"AUXOUTL", NULL,				"Left AuxOut"},
	{"AUXOUTR", NULL,				"Right AuxOut"},

	/* HP mixer */
	{"HPL Mix", "ADC2HP_L Playback Switch",		"Left Capture Mix"},
	{"HPL Mix", NULL,				"HP Mix"},
	{"HPR Mix", "ADC2HP_R Playback Switch",		"Right Capture Mix"},
	{"HPR Mix", NULL,				"HP Mix"},
	{"HP Mix", "LI2HP Playback Switch",		"Line Mix"},
	{"HP Mix", "AUXI2HP Playback Switch",		"AuxI Mix"},
	{"HP Mix", "MIC12HP Playback Switch",		"MIC1 PGA"},
	{"HP Mix", "MIC22HP Playback Switch",		"MIC2 PGA"},
	{"HP Mix", "DAC2HP Playback Switch",		"I2S Mix"},

	/* speaker mixer */
	{"Speaker Mix", "LI2SPK Playback Switch",	"Line Mix"},
	{"Speaker Mix", "AUXI2SPK Playback Switch",	"AuxI Mix"},
	{"Speaker Mix", "MIC12SPK Playback Switch",	"MIC1 PGA"},
	{"Speaker Mix", "MIC22SPK Playback Switch",	"MIC2 PGA"},
	{"Speaker Mix", "DAC2SPK Playback Switch",	"I2S Mix"},

	/* mono mixer */
	{"Mono Mix", "ADC2MONO_L Playback Switch",	"Left Capture Mix"},
	{"Mono Mix", "ADC2MONO_R Playback Switch",	"Right Capture Mix"},
	{"Mono Mix", "LI2MONO Playback Switch",		"Line Mix"},
	{"Mono Mix", "AUXI2MONO Playback Switch",	"AuxI Mix"},
	{"Mono Mix", "MIC12MONO Playback Switch",	"MIC1 PGA"},
	{"Mono Mix", "MIC22MONO Playback Switch",	"MIC2 PGA"},
	{"Mono Mix", "DAC2MONO Playback Switch",	"I2S Mix"},

	/* Left record mixer */
	{"Left Capture Mix", "LineInL Capture Switch",	"LINEINL"},
	{"Left Capture Mix", "Left AuxI Capture Switch", "AUXINL"},
	{"Left Capture Mix", "Mic1 Capture Switch",	"MIC1 Pre Amp"},
	{"Left Capture Mix", "Mic2 Capture Switch",	"MIC2 Pre Amp"},
	{"Left Capture Mix", "HPMixerL Capture Switch", "HPL Mix"},
	{"Left Capture Mix", "SPKMixer Capture Switch", "Speaker Mix"},
	{"Left Capture Mix", "MonoMixer Capture Switch", "Mono Mix"},

	/*Right record mixer */
	{"Right Capture Mix", "LineInR Capture Switch",	"LINEINR"},
	{"Right Capture Mix", "Right AuxI Capture Switch",	"AUXINR"},
	{"Right Capture Mix", "Mic1 Capture Switch",	"MIC1 Pre Amp"},
	{"Right Capture Mix", "Mic2 Capture Switch",	"MIC2 Pre Amp"},
	{"Right Capture Mix", "HPMixerR Capture Switch", "HPR Mix"},
	{"Right Capture Mix", "SPKMixer Capture Switch", "Speaker Mix"},
	{"Right Capture Mix", "MonoMixer Capture Switch", "Mono Mix"},

	/* headphone left mux */
	{"Left Headphone Mux", "HP Left Mix",		"HPL Mix"},
	{"Left Headphone Mux", "Vmid",			"Vmid"},

	/* headphone right mux */
	{"Right Headphone Mux", "HP Right Mix",		"HPR Mix"},
	{"Right Headphone Mux", "Vmid",			"Vmid"},

	/* speaker out mux */
	{"SpeakerOut Mux", "Vmid",			"Vmid"},
	{"SpeakerOut Mux", "HPOut Mix",			"HPOut Mix"},
	{"SpeakerOut Mux", "Speaker Mix",		"Speaker Mix"},
	{"SpeakerOut Mux", "Mono Mix",			"Mono Mix"},

	/* Mono/Aux Out mux */
	{"AuxOut Mux", "Vmid",				"Vmid"},
	{"AuxOut Mux", "HPOut Mix",			"HPOut Mix"},
	{"AuxOut Mux", "Speaker Mix",			"Speaker Mix"},
	{"AuxOut Mux", "Mono Mix",			"Mono Mix"},

	/* output pga */
	{"HPL", NULL,					"Left Headphone"},
	{"Left Headphone", NULL,			"Left Headphone Mux"},
	{"HPR", NULL,					"Right Headphone"},
	{"Right Headphone", NULL,			"Right Headphone Mux"},
	{"Left AuxOut", NULL,				"AuxOut Mux"},
	{"Right AuxOut", NULL,				"AuxOut Mux"},

	/* input pga */
	{"Left LineIn", NULL,				"LINEINL"},
	{"Right LineIn", NULL,				"LINEINR"},
	{"Left AuxI", NULL,				"AUXINL"},
	{"Right AuxI", NULL,				"AUXINR"},
	{"MIC1 Pre Amp", NULL,				"MIC1"},
	{"MIC2 Pre Amp", NULL,				"MIC2"},
	{"MIC1 PGA", NULL,				"MIC1 Pre Amp"},
	{"MIC2 PGA", NULL,				"MIC2 Pre Amp"},

	/* left ADC */
	{"Left ADC", NULL,				"Left Capture Mix"},

	/* right ADC */
	{"Right ADC", NULL,				"Right Capture Mix"},

	{"SpeakerOut N Mux", "RN/-R",			"SpeakerOut"},
	{"SpeakerOut N Mux", "RP/+R",			"SpeakerOut"},
	{"SpeakerOut N Mux", "LN/-R",			"SpeakerOut"},
	{"SpeakerOut N Mux", "Vmid",			"Vmid"},

	{"SPKOUT", NULL,				"SpeakerOut"},
	{"SPKOUTN", NULL,				"SpeakerOut N Mux"},
};

static const struct snd_soc_dapm_route intercon_spk[] = {
	{"SpeakerOut", NULL,				"SpeakerOut Mux"},
};

static const struct snd_soc_dapm_route intercon_amp_spk[] = {
	{"AB Amp", NULL,				"SpeakerOut Mux"},
	{"D Amp", NULL,					"SpeakerOut Mux"},
	{"AB-D Amp Mux", "AB Amp",			"AB Amp"},
	{"AB-D Amp Mux", "D Amp",			"D Amp"},
	{"SpeakerOut", NULL,				"AB-D Amp Mux"},
};

/* PLL divisors */
struct _pll_div {
	u32 pll_in;
	u32 pll_out;
	u16 regvalue;
};

/* Note : pll code from original alc5623 driver. Not sure of how good it is */
/* useful only for master mode */
static const struct _pll_div codec_master_pll_div[] = {

/*	{  2048000,  8192000,	0x0ea0},
	{  3686400,  8192000,	0x4e27},
	{ 12000000,  8192000,	0x456b},
	{ 13000000,  8192000,	0x495f},
	{ 13100000,  8192000,	0x0320},
	{  2048000,  11289600,	0xf637},
	{  3686400,  11289600,	0x2f22},
	{ 12000000,  11289600,	0x3e2f},
	{ 13000000,  11289600,	0x4d5b},
	{ 13100000,  11289600,	0x363b},
	{  2048000,  16384000,	0x1ea0},
	{  3686400,  16384000,	0x9e27},
	{ 12000000,  16384000,	0x452b},
	{ 13000000,  16384000,	0x542f},
	{ 13100000,  16384000,	0x03a0},
	{  2048000,  16934400,	0xe625},
	{  3686400,  16934400,	0x9126},
	{ 12000000,  16934400,	0x4d2c},
	{ 13000000,  16934400,	0x742f},
	{ 13100000,  16934400,	0x3c27},
	{  2048000,  22579200,	0x2aa0},
	{  3686400,  22579200,	0x2f20},
	{ 12000000,  22579200,	0x7e2f},
	{ 13000000,  22579200,	0x742f},
	{ 13100000,  22579200,	0x3c27},
	{  2048000,  24576000,	0x2ea0},
	{  3686400,  24576000,	0xee27},
	{ 12000000,  24576000,	0x2915},
	{ 13000000,  24576000,	0x772e},
	{ 13100000,  24576000,	0x0d20},
*/
};

static const struct _pll_div codec_slave_pll_div[] = {
/*
	{  1024000,  16384000,  0x3ea0},
	{  1411200,  22579200,	0x3ea0},
	{  1536000,  24576000,	0x3ea0},
	{  2048000,  16384000,  0x1ea0},
	{  2822400,  22579200,	0x1ea0},
	{  3072000,  24576000,	0x1ea0},
*/
};

/* Greatest common divisor */
static unsigned int gcd(unsigned int u, unsigned int v)
{
	int shift;

	/* GCD(0,x) := x */
	if (u == 0 || v == 0)
		return u | v;

	/* Let shift := lg K, where K is the greatest power of 2
	   dividing both u and v. */
	for (shift = 0; ((u | v) & 1) == 0; ++shift) {
		u >>= 1;
		v >>= 1;
	}

	while ((u & 1) == 0)
		u >>= 1;

	/* From here on, u is always odd. */
	do {
		while ((v & 1) == 0)  /* Loop X */
			v >>= 1;

		/* Now u and v are both odd, so diff(u, v) is even.
		   Let u = min(u, v), v = diff(u, v)/2. */
		if (u < v) {
			v -= u;
		} else {
		
			unsigned int diff = u - v;
			u = v;
			v = diff;
			}
			v >>= 1;
	} while (v != 0);

	return u << shift;
}

static int alc5623_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id,
		int source, unsigned int freq_in, unsigned int freq_out)
{
	int i;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);
	unsigned int rin, rout, cd, m, finkhz, msel, ksel, psel, fvcodifsel;
	unsigned int fvcosel;
	u16 reg;

	if (pll_id < ALC5623_PLL_FR_MCLK || pll_id > ALC5623_PLL_FR_BCK)
		return -ENODEV;

	/* Codec sys-clock from MCLK */
	snd_soc_update_bits(codec,ALC5623_GLOBAL_CLK_CTRL_REG,0x8000, 0x0000);

	/* Disable PLL power */
	snd_soc_update_bits(codec, ALC5623_PWR_MANAG_ADD2,
				ALC5623_PWR_ADD2_PLL,
				0);

	/* pll is not used in slave mode */
	reg = snd_soc_read(codec, ALC5623_DAI_CONTROL);
	if (reg & ALC5623_DAI_SDP_SLAVE_MODE)
		return 0;

	alc5623->using_pll = 0;

	/* If input and output frequency are the same, or no freq specified, disable PLL */
	if ((!freq_in || !freq_out) || freq_in == freq_out) {
		dev_dbg(codec->dev, "%s(): disabling PLL\n", __FUNCTION__);
		return 0;
	}
	
	/* Now, given input and output frequencies, we must find the 
	   values for the PLL */
	
	/* 
	  According to datasheet:
	  	  Fout = (MCLK * (N+2)) / ((M + 2) * (K + 2)) 
		  
	  Where:
		K < 8
	    M < 16
	    N < 256
	    P < 2
	 */

/*
	switch (pll_id) {
	case ALC5623_PLL_FR_MCLK:
		for (i = 0; i < ARRAY_SIZE(codec_master_pll_div); i++) {
			if (codec_master_pll_div[i].pll_in == freq_in
			   && codec_master_pll_div[i].pll_out == freq_out) {
				// PLL source from MCLK 
				pll_div  = codec_master_pll_div[i].regvalue;
				break;
			}
		}
		break;
	case ALC5623_PLL_FR_BCK:
		for (i = 0; i < ARRAY_SIZE(codec_slave_pll_div); i++) {
			if (codec_slave_pll_div[i].pll_in == freq_in
			   && codec_slave_pll_div[i].pll_out == freq_out) {
				//PLL source from Bitclk
				gbl_clk = ALC5623_GBL_CLK_PLL_SOUR_SEL_BITCLK;
				pll_div = codec_slave_pll_div[i].regvalue;
				break;
			}
		}
		break;
	default:
		return -EINVAL;
	}

	if (!pll_div)
		return -EINVAL;

	snd_soc_write(codec, ALC5623_GLOBAL_CLK_CTRL_REG, gbl_clk);
	snd_soc_write(codec, ALC5623_PLL_CTRL, pll_div);
	snd_soc_update_bits(codec, ALC5623_PWR_MANAG_ADD2,
				ALC5623_PWR_ADD2_PLL,
				ALC5623_PWR_ADD2_PLL);
	gbl_clk |= ALC5623_GBL_CLK_SYS_SOUR_SEL_PLL;
	snd_soc_write(codec, ALC5623_GLOBAL_CLK_CTRL_REG, gbl_clk);
*/
	/* First, get the maximum common divider between input and 
	   output frequencies */
	   
	/* Get the greatest common divisor */
	cd = gcd(freq_in,freq_out);
	
	/* Reduce frequencies using it */
	rin = freq_in / cd;
	rout = freq_out / cd;
	
	/* To synthetize fout from fin, we must multiply fin by rout, then 
	   divide by rin. but, in the process, we must try to make sure the 
	   Fvco to stay between 90 ... 100Mhz.
	   
	   Input frequency is first divided by (M+2)*(P+1), then multiplied by N
	   (that is the Fvco), then divided by (K+2) 
	   
	   rout = (N+2)
	   rin  = ((M + 2) * (K + 2) * (P+1))
	   */
	   
	/* Adjust rout and rin to place them in range, if needed */
	if (rout < 2 || rin < 2) {
		rout <<= 1;
		rin <<= 1;
		cd >>= 1;
	}
	   
	/* Check that rout can be achieved */
	if (rout < 2 || rout > 257) {
		dev_err(codec->dev, "N:%d outside of bounds of PLL (1 < N < 258), rin:%d, cd:%d\n", rout,rin,cd);
		return -EINVAL;
	}
	
	/* Get the input frequency in khz, even if losing precision. We will only
	   use it to calculate the approximate VCO frequency and avoid overflows
	   during calculations */
	finkhz = freq_in;
	   
	/* By default, no m selected */
	msel = 0;
	ksel = 0;
	psel = 0;
	fvcodifsel = 0;
	fvcosel = 0;
	   
	/* Iterate through all possible values for M, checking if we can reach
	   the requested ratio */
	for (m = 1; m < 18; m++) {
		unsigned int kdp,fvco,fvcodif;	
		
		/* Check if we can divide without remainder rin by this m value. 
		   This would mean this m value is usable */
		if ((rin % m) != 0) 
			continue;
		
		/* Calculate K * P */
		kdp = rin / m;
		
		/* Filter out impossible K values ... */
		if (kdp > 18 || kdp < 2 || ((kdp & 1) != 0 && kdp > 9)) 
			continue;
		
		/* Calculate PLL Fvco assuming P = 1 */
		fvco = finkhz * rout / m;
		
		/* Calculate distance to optimum fvco */
		fvcodif = (fvco > 95000) ? (fvco - 95000) : (95000 - fvco);
		
		/* If the fvcodif is less than the previously selected one, or no
		   previous selected one... */
		if (!msel || fvcodif < fvcodifsel) {
		
			/* Keep it. Assumes P = 1 */
			msel = m;
			ksel = kdp;
			psel = 1;
			fvcodifsel = fvcodif;
			fvcosel = fvco;
		}
		
		/* If kdp is even, then we can try another configuration with P = 2.
			This means halving the Fvco */
		if (!(kdp & 1) && kdp > 3) {
		
			/* Scale frequency down */
			fvco >>= 1;

			/* Calculate distance to optimum fvco */
			fvcodif = (fvco > 95000) ? (fvco - 95000) : (95000 - fvco);
		
			/* If the fvcodif is less than the previously selected one, or no
				previous selected one... */
			if (!msel || fvcodif < fvcodifsel) {
		
				/* Keep it - P = 2*/
				msel = m;
				ksel = kdp >> 1;
				psel = 2;
				fvcodifsel = fvcodif;
				fvcosel = fvco;
			}
		}
	}

	/* Well, check if there was a valid config */
	if (!msel) {
		dev_err(codec->dev, "No valid M,K,P coefficients gives the needed divider %d for PLL\n", rin);
		return -EINVAL;
	}
	   
	/* Well. we got it. */
	dev_dbg(codec->dev, "Using M:%d, N:%d, K:%d, P:%d coefficients for PLL (div:%d=%d), fvco:%d khz\n", msel,rout,ksel,psel,rin,msel*ksel*psel,fvcosel);

	/* Set the PLL predivider */
	snd_soc_update_bits(codec,ALC5623_GLOBAL_CLK_CTRL_REG, 0x0001,(ksel > 1) ? 0x0001 : 0x0000);
	
	/* set PLL parameter */
 	snd_soc_write(codec,ALC5623_PLL_CTRL,
		( (msel - 2) & 0xF) |			/* M coefficient */
		(((ksel - 2) & 0x7) << 4) |		/* K coefficient */
		((msel == 1) ? 0x80 : 0x00) | 	/* M bypass */
		(((rout - 2) & 0xFF) << 8)		/* N coefficient */
		);

	/* enable PLL power */
	snd_soc_update_bits(codec,ALC5623_PWR_MANAG_ADD2, 0x0100,0x0100);	
	
	/* Codec sys-clock from PLL */
	snd_soc_update_bits(codec,ALC5623_GLOBAL_CLK_CTRL_REG,0x8000,0x8000);

	/* We are using the PLL */
	alc5623->using_pll = 1;

	return 0;
}

struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 regvalue;
};

/* codec hifi mclk (after PLL) clock divider coefficients */
/* values inspired from column BCLK=32Fs of Appendix A table */
static const struct _coeff_div coeff_div[] = {
	{24576000,8000,0x3C6B},
	{16384000,8000,0x3A69},
	{24576000,12000,0x3A69},
	{18432000,12000,0x2C6B},
	{24576000,16000,0x2C6B},
	{16384000,16000,0x2A69},
	{24576000,24000,0x2A69},
	{18432000,24000,0x1C6B},
	{24576000,32000,0x1C6B},
	{16384000,32000,0x1A69},
	{24576000,48000,0x1A69},
	{18432000,48000,0x0C6B},
	{22579200,11025,0x3A69},
	{16934400,11025,0x2C6B},
	{22579200,22050,0x2A69},
	{16934400,22050,0x1C6B},
	{22579200,44100,0x1A69},
	{16934400,44100,0x0C6B},
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

/*
 * Clock after PLL and dividers
 */
static int alc5623_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);

	switch (freq) {
	case  8192000:
	case 11289600:
	case 12288000:
	case 16384000:
	case 16934400:
	case 18432000:
	case 22579200:
	case 24576000:
		alc5623->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int alc5623_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface = ALC5623_DAI_SDP_MASTER_MODE;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		iface = ALC5623_DAI_SDP_SLAVE_MODE;
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= ALC5623_DAI_I2S_DF_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface |= ALC5623_DAI_I2S_DF_RIGHT;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= ALC5623_DAI_I2S_DF_LEFT;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= ALC5623_DAI_I2S_DF_PCM;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= ALC5623_DAI_I2S_DF_PCM | ALC5623_DAI_I2S_PCM_MODE;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= ALC5623_DAI_MAIN_I2S_BCLK_POL_CTRL;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= ALC5623_DAI_MAIN_I2S_BCLK_POL_CTRL;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return -EINVAL;
	}

	return snd_soc_write(codec, ALC5623_DAI_CONTROL, iface);
}

static int alc5623_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	printk(KERN_DEBUG "%s++", __func__);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
#else	
	struct snd_soc_codec *codec = rtd->codec;
#endif
	struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);
	int coeff, rate;
	u16 iface;

	iface = snd_soc_read(codec, ALC5623_DAI_CONTROL);
	iface &= ~ALC5623_DAI_I2S_DL_MASK;

	printk(KERN_DEBUG "iface %x", iface);
	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		iface |= ALC5623_DAI_I2S_DL_16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= ALC5623_DAI_I2S_DL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= ALC5623_DAI_I2S_DL_24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= ALC5623_DAI_I2S_DL_32;
		break;
	default:
		return -EINVAL;
	}

	printk(KERN_DEBUG "iface %x", iface);
	
	/* set iface & srate */
	snd_soc_write(codec, ALC5623_DAI_CONTROL, iface);
	rate = params_rate(params);
	printk(KERN_DEBUG "rate %x", rate);
	coeff = get_coeff(alc5623->sysclk, rate);
	if (coeff < 0)
		return -EINVAL;

	coeff = coeff_div[coeff].regvalue;
	dev_dbg(codec->dev, "%s: sysclk=%d,rate=%d,coeff=0x%04x\n",
		__func__, alc5623->sysclk, rate, coeff);
	snd_soc_write(codec, ALC5623_STEREO_AD_DA_CLK_CTRL, coeff);

	return 0;
}

static int alc5623_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 hp_mute = ALC5623_MISC_M_DAC_L_INPUT | ALC5623_MISC_M_DAC_R_INPUT;
	u16 mute_reg = snd_soc_read(codec, ALC5623_MISC_CTRL) & ~hp_mute;

	if (mute)
		mute_reg |= hp_mute;

	return snd_soc_write(codec, ALC5623_MISC_CTRL, mute_reg);
}

#define ALC5623_ADD2_POWER_EN (ALC5623_PWR_ADD2_VREF \
	| ALC5623_PWR_ADD2_DAC_REF_CIR)

#define ALC5623_ADD3_POWER_EN (ALC5623_PWR_ADD3_MAIN_BIAS \
	| ALC5623_PWR_ADD3_MIC1_BOOST_AD)

#define ALC5623_ADD1_POWER_EN \
	(ALC5623_PWR_ADD1_SHORT_CURR_DET_EN | ALC5623_PWR_ADD1_SOFTGEN_EN \
	| ALC5623_PWR_ADD1_DEPOP_BUF_HP | ALC5623_PWR_ADD1_HP_OUT_AMP \
	| ALC5623_PWR_ADD1_HP_OUT_ENH_AMP)

#define ALC5623_ADD1_POWER_EN_5622 \
	(ALC5623_PWR_ADD1_SHORT_CURR_DET_EN \
	| ALC5623_PWR_ADD1_HP_OUT_AMP)

static void enable_power_depop(struct snd_soc_codec *codec)
{
	struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, ALC5623_PWR_MANAG_ADD1,
				ALC5623_PWR_ADD1_SOFTGEN_EN,
				ALC5623_PWR_ADD1_SOFTGEN_EN);

	snd_soc_write(codec, ALC5623_PWR_MANAG_ADD3, ALC5623_ADD3_POWER_EN);

	snd_soc_update_bits(codec, ALC5623_MISC_CTRL,
				ALC5623_MISC_HP_DEPOP_MODE2_EN,
				ALC5623_MISC_HP_DEPOP_MODE2_EN);

	msleep(500);

	snd_soc_write(codec, ALC5623_PWR_MANAG_ADD2, ALC5623_ADD2_POWER_EN);

	/* avoid writing '1' into 5622 reserved bits */
	if (alc5623->id == 0x22)
		snd_soc_write(codec, ALC5623_PWR_MANAG_ADD1,
			ALC5623_ADD1_POWER_EN_5622);
	else
		snd_soc_write(codec, ALC5623_PWR_MANAG_ADD1,
			ALC5623_ADD1_POWER_EN);

	/* disable HP Depop2 */
	snd_soc_update_bits(codec, ALC5623_MISC_CTRL,
				ALC5623_MISC_HP_DEPOP_MODE2_EN,
				0);

}

static int alc5623_set_bias_level(struct snd_soc_codec *codec,
				      enum snd_soc_bias_level level)
{
	struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);
	
	dev_dbg(codec->dev, "%s(): level: %d\n", __FUNCTION__,level);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	if (codec->dapm.bias_level == level)
#else
	if (codec->bias_level == level)
#endif
	{
		return 0;
	}

	switch (level) {
	case SND_SOC_BIAS_ON:
		enable_power_depop(codec);
		
		/* If using the PLL enable it and use it as clock source */
		if (alc5623->using_pll) {
	        	/* enable PLL power */
		        snd_soc_update_bits(codec,ALC5623_PWR_MANAG_ADD2, 0x0100,0x0100);

		        /* Codec sys-clock from PLL */
		        snd_soc_update_bits(codec,ALC5623_GLOBAL_CLK_CTRL_REG,0x8000,0x8000);
		}
		break;
	case SND_SOC_BIAS_PREPARE:

		//LOW ON----- power on all power not controlled by DAPM - Codec sys-clock from MCLK */
		snd_soc_update_bits(codec,ALC5623_GLOBAL_CLK_CTRL_REG,0x8000,0x0000);
		enable_power_depop(codec);
		//snd_soc_update_bits(codec,ALC5623_PWR_MANAG_ADD1,0xC144,0xc144);
		//snd_soc_update_bits(codec,ALC5623_PWR_MANAG_ADD2,,);
		//snd_soc_update_bits(codec,ALC5623_PWR_MANAG_ADD3,,);

		break;
	case SND_SOC_BIAS_STANDBY:

		/* If resuming operation from stop ... */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF)
#else
		if (codec->bias_level == SND_SOC_BIAS_OFF)
#endif
		{
		
			/* Enable the codec MCLK */
			clk_enable(alc5623->mclk);
	
			/* Reset the codec */
			alc5623_reset(codec);
			mdelay(1);
		
			/* Sync registers to cache */
			alc5623_sync_cache(codec);
		}

		/* everything off except vref/vmid, */
		snd_soc_update_bits(codec,ALC5623_GLOBAL_CLK_CTRL_REG,0x8000,0x0000);
		snd_soc_write(codec, ALC5623_PWR_MANAG_ADD2,
				ALC5623_PWR_ADD2_VREF);
		snd_soc_write(codec, ALC5623_PWR_MANAG_ADD3,
				ALC5623_PWR_ADD3_MAIN_BIAS);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF)
#else
		if (codec->bias_level == SND_SOC_BIAS_OFF)
#endif
		{
			/* Enable fast Vref */
			snd_soc_update_bits(codec, ALC5623_MISC_CTRL, 0x8000, 0x0000);
			
			/* Let it stabilize */
			mdelay(10);
			
			/* Disable fast Vref */
			snd_soc_update_bits(codec, ALC5623_MISC_CTRL, 0x8000, 0x8000);
		}


		break;
	case SND_SOC_BIAS_OFF:

		/* everything off, dac mute, inactive, Codec sys-clock from MCLK */
		snd_soc_update_bits(codec,ALC5623_GLOBAL_CLK_CTRL_REG,0x8000,0x0000);
		snd_soc_write(codec, ALC5623_PWR_MANAG_ADD2, 0);
		snd_soc_write(codec, ALC5623_PWR_MANAG_ADD3, 0);
		snd_soc_write(codec, ALC5623_PWR_MANAG_ADD1, 0);


		/* Make sure all writes from now on will be resync when resuming */
		codec->cache_sync = 1;
		
		/* Disable the codec MCLK */
		clk_disable(alc5623->mclk);

		break;
	}
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	codec->dapm.bias_level = level;
#else
	codec->bias_level = level;
#endif
	return 0;
}

#define ALC5623_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops alc5623_dai_ops = {
		.hw_params = alc5623_pcm_hw_params,
		.digital_mute = alc5623_mute,
		.set_fmt = alc5623_set_dai_fmt,
		.set_sysclk = alc5623_set_dai_sysclk,
		.set_pll = alc5623_set_dai_pll,
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
static struct snd_soc_dai_driver alc5623_dai = {
#else
struct snd_soc_dai alc5623_dai = {
#endif
	.name = "alc5623-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =	8000,
		.rate_max =	48000,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = ALC5623_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =	8000,
		.rate_max =	48000,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = ALC5623_FORMATS,},

	.ops = &alc5623_dai_ops,
};

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)
EXPORT_SYMBOL_GPL(alc5623_dai);
#endif

/* Check if a register is volatile or not to forbid or not caching its value */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
static int alc5623_volatile_register(struct snd_soc_codec *codec,
	unsigned int reg)
#else
static int alc5623_volatile_register(unsigned int reg)
#endif
{
	if (
		reg == ALC5623_GPIO_PIN_STATUS ||
		reg == ALC5623_OVER_CURR_STATUS ||
		reg == ALC5623_HID_CTRL_INDEX ||
		reg == ALC5623_HID_CTRL_DATA)
		return 1;
	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
static int alc5623_suspend(struct snd_soc_codec *codec, pm_message_t mesg)
{
#else
static int alc5623_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
#endif

	alc5623_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
static int alc5623_resume(struct snd_soc_codec *codec)
{
#else
static int alc5623_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
#endif

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)
	int i, step = codec->reg_cache_step, size = codec->reg_cache_size;
#else
	int i, step = codec->driver->reg_cache_step, size = codec->driver->reg_cache_size;
#endif
	u16 *cache = codec->reg_cache;

	/* Sync reg_cache with the hardware */
	for (i = 0 ; i < size ; i += step)
		snd_soc_write(codec, i, cache[i]);

	alc5623_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* charge alc5623 caps */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	if (codec->dapm.suspend_bias_level == SND_SOC_BIAS_ON) {
		alc5623_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
		codec->dapm.bias_level = SND_SOC_BIAS_ON;
		alc5623_set_bias_level(codec, codec->dapm.bias_level);
	}
#else
	if (codec->suspend_bias_level == SND_SOC_BIAS_ON) {
		alc5623_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
		codec->bias_level = SND_SOC_BIAS_ON;
		alc5623_set_bias_level(codec, codec->bias_level);
	}
#endif

	return 0;
}

static int alc5623_probe(struct snd_soc_codec *codec)
{
	printk(KERN_DEBUG "%s++", __func__);

	struct alc5623_priv *alc5623 = snd_soc_codec_get_drvdata(codec);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)	
	struct snd_soc_dapm_context *dapm = &codec->dapm;
#endif
	int ret,i;
	int linebias;
	unsigned long reg;

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, alc5623->control_type);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	/* Enable the codec MCLK ... Otherwise, we can't read or write registers */
	clk_enable(alc5623->mclk);

	alc5623_reset(codec);
	mdelay(1);

	alc5623_fill_cache(codec);

	/* Modify the default values to properly config the CODEC */
	for (i = 0; i < ARRAY_SIZE(alc5623_reg_default); i++) {
		snd_soc_write(codec,alc5623_reg_default[i].reg,alc5623_reg_default[i].val);
	}
	
	/* Configure amplifier bias voltages based on voltage supplies */
	linebias = alc5623->linevdd_mv >> 1;
	reg = 0;
	
	/* Line Out amplifier bias */
	if (linebias <  1250) {
		reg |= (5 << 12); // SPK class AB bias: 1.0Vdd
	} else
	if (linebias <  1500) {
		reg |= (4 << 12); // SPK class AB bias: 1.25Vdd
	} else
	if (linebias <  1750) {
		reg |= (3 << 12); // SPK class AB bias: 1.5Vdd
	} else
	if (linebias <  2000) {
		reg |= (2 << 12); // SPK class AB bias: 1.75Vdd
	} else
	if (linebias <  2250) {
		reg |= (1 << 12); // SPK class AB bias: 2.0Vdd
	} /* else 0=2.25v bias */
	
	/* Set the amplifier biases */
	snd_soc_update_bits(codec,ALC5623_ADD_CTRL_REG, 0x7000, reg);
	
	/* power on device */
	//alc5623_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	if (alc5623->add_ctrl) {
		snd_soc_write(codec, ALC5623_ADD_CTRL_REG,
				alc5623->add_ctrl);
	}

	if (alc5623->jack_det_ctrl) {
		snd_soc_write(codec, ALC5623_JACK_DET_CTRL,
				alc5623->jack_det_ctrl);
	}

	switch (alc5623->id) {
	case 0x21:
		snd_soc_add_controls(codec, rt5621_vol_snd_controls,
			ARRAY_SIZE(rt5621_vol_snd_controls));
		break;
	case 0x22:
		snd_soc_add_controls(codec, rt5622_vol_snd_controls,
			ARRAY_SIZE(rt5622_vol_snd_controls));
		break;
	case 0x23:
		snd_soc_add_controls(codec, alc5623_vol_snd_controls,
			ARRAY_SIZE(alc5623_vol_snd_controls));
		break;
	default:
		return -EINVAL;
	}
	snd_soc_add_controls(codec, alc5623_snd_controls,
			ARRAY_SIZE(alc5623_snd_controls));

	/* Disable the codec MCLK */
	clk_disable(alc5623->mclk);

	/* On linux 2.6.38+ we need to register controls here */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
	snd_soc_dapm_new_controls(dapm, alc5623_dapm_widgets,
					ARRAY_SIZE(alc5623_dapm_widgets));

	/* set up audio path interconnects */
	snd_soc_dapm_add_routes(dapm, audio_map, ARRAY_SIZE(audio_map));

	switch (alc5623->id) {
	case 0x21:
	case 0x22:
		snd_soc_dapm_new_controls(dapm, alc5623_dapm_amp_widgets,
					ARRAY_SIZE(alc5623_dapm_amp_widgets));
		snd_soc_dapm_add_routes(dapm, intercon_amp_spk,
					ARRAY_SIZE(intercon_amp_spk));
		break;
	case 0x23:
		snd_soc_dapm_add_routes(dapm, intercon_spk,
					ARRAY_SIZE(intercon_spk));
		break;
	default:
		return -EINVAL;
	}
#endif

	return ret;
}

/* power down chip */
static int alc5623_remove(struct snd_soc_codec *codec)
{
	alc5623_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
static struct snd_soc_codec_driver soc_codec_device_alc5623 = {
	.probe = alc5623_probe,
	.remove = alc5623_remove,
	.suspend = alc5623_suspend,
	.resume = alc5623_resume,
	.set_bias_level = alc5623_set_bias_level,
	.reg_cache_size = ALC5623_VENDOR_ID2+2,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
};
#endif

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)	
static struct snd_soc_codec *alc5623_codec = NULL;
#endif

/*
 * ALC5623 2 wire address is determined by A1 pin
 * state during powerup.
 *    low  = 0x1a
 *    high = 0x1b
 */
static int alc5623_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	printk(KERN_DEBUG "%s++", __func__);
	struct alc5623_platform_data *pdata;
	struct alc5623_priv *alc5623;
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)		
	struct snd_soc_codec *codec;
#endif
	struct clk* mclk;

	int ret, vid1, vid2;

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
	
	/* Enable it to be able to access codec registers */
	clk_enable(mclk);
	
	/* Read chip ids */
	vid1 = i2c_smbus_read_word_data(client, ALC5623_VENDOR_ID1);
	if (vid1 < 0) {
		dev_err(&client->dev, "failed to read I2C\n");
		return -EIO;
	}
	vid1 = ((vid1 & 0xff) << 8) | (vid1 >> 8);

	vid2 = i2c_smbus_read_byte_data(client, ALC5623_VENDOR_ID2);
	if (vid2 < 0) {
		dev_err(&client->dev, "failed to read I2C\n");
		return -EIO;
	}

	if ((vid1 != 0x10ec) || (vid2 != id->driver_data)) {
		dev_err(&client->dev, "unknown or wrong codec\n");
		dev_err(&client->dev, "Expected %x:%lx, got %x:%x\n",
				0x10ec, id->driver_data,
				vid1, vid2);
		return -ENODEV;
	}

	/* Disable the clock */
	clk_disable(mclk);

	dev_dbg(&client->dev, "Found codec id : alc56%02x\n", vid2);

	alc5623 = kzalloc(sizeof(struct alc5623_priv), GFP_KERNEL);
	if (alc5623 == NULL)
		return -ENOMEM;

	alc5623->mclk = mclk;

	/* Store the supply voltages used for amplifiers */
	alc5623->linevdd_mv = pdata->linevdd_mv;	/* Line Vdd in millivolts */

	alc5623->add_ctrl = pdata->add_ctrl;
	alc5623->jack_det_ctrl = pdata->jack_det_ctrl;

	alc5623->id = vid2;
	switch (alc5623->id) {
	case 0x21:
		alc5623_dai.name = "alc5621-hifi";
		break;
	case 0x22:
		alc5623_dai.name = "alc5622-hifi";
		break;
	case 0x23:
		alc5623_dai.name = "alc5623-hifi";
		break;
	default:
		kfree(alc5623);
		return -EINVAL;
	}

	i2c_set_clientdata(client, alc5623);
	alc5623->control_data = client;
	alc5623->control_type = SND_SOC_I2C;
	mutex_init(&alc5623->mutex);

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)	
	/* linux 2.6.36 version setup is quite by hand */

	codec = &alc5623->codec;
	snd_soc_codec_set_drvdata(codec, alc5623);

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	codec->name = "ALC5623";
	codec->owner = THIS_MODULE;
	codec->control_data = client;	
	codec->bias_level = SND_SOC_BIAS_OFF;
	codec->set_bias_level = alc5623_set_bias_level;	
	codec->dai = &alc5623_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = ALC5623_VENDOR_ID2+2;
	codec->reg_cache_step = 2;
	codec->reg_cache = &alc5623->reg_cache[0];
	codec->volatile_register = alc5623_volatile_register;	
	codec->cache_sync = 1;
	codec->idle_bias_off = 1;
	
	codec->dev = &client->dev;
	alc5623_dai.dev = &client->dev;
	alc5623_codec = codec; /* so later probe can attach the codec to the card */
	
	/* call the codec probe function */
//	ret = alc5623_probe(codec);
//	if (ret != 0) {
//		dev_err(&client->dev, "Failed to probe codec: %d\n", ret);	
//		return ret;
//	}
	

	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		dev_err(&client->dev, "Failed to register codec: %d\n", ret);	
		kfree(alc5623);
		return ret;
	}

	ret = snd_soc_register_dai(&alc5623_dai);
	if (ret != 0) {
		dev_err(&client->dev, "Failed to register DAI: %d\n", ret);	
		snd_soc_unregister_codec(codec);
		kfree(alc5623);
		return ret;
	}
	
#else
	/* linux 2.6.38 setup is very streamlined :) */
	ret =  snd_soc_register_codec(&client->dev,
		&soc_codec_device_alc5623, &alc5623_dai, 1);
	if (ret != 0) {
		dev_err(&client->dev, "Failed to register codec: %d\n", ret);
		kfree(alc5623);
	}

#endif

	return ret;
}

static int alc5623_i2c_remove(struct i2c_client *client)
{
	struct alc5623_priv *alc5623 = i2c_get_clientdata(client);

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)	
	/* linux 2.6.36 version device removal is quite by hand */
	
	alc5623_remove(&alc5623->codec);
	
	snd_soc_unregister_dai(&alc5623_dai);
	snd_soc_unregister_codec(&alc5623->codec);

	alc5623_dai.dev = NULL;
	alc5623_codec = NULL;

#else
	/* linux 2.6.38 device removal is very streamlined :) */
	snd_soc_unregister_codec(&client->dev);
	
#endif

	kfree(alc5623);
	return 0;
}

static const struct i2c_device_id alc5623_i2c_table[] = {
	{"alc5621", 0x21},
	{"alc5622", 0x22},
	{"alc5623", 0x23},
	{}
};
MODULE_DEVICE_TABLE(i2c, alc5623_i2c_table);

/*  i2c codec control layer */
static struct i2c_driver alc5623_i2c_driver = {
	.driver = {
		.name = "alc562x-codec",
		.owner = THIS_MODULE,
	},
	.probe = alc5623_i2c_probe,
	.remove =  __devexit_p(alc5623_i2c_remove),
	.id_table = alc5623_i2c_table,
};

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,36)	
static int alc5623_plat_probe(struct platform_device *pdev)
{
	printk(KERN_DEBUG "%s++", __func__);
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	int ret = 0;

	if (!alc5623_codec) {
		dev_err(&pdev->dev, "I2C client not yet instantiated\n");
		return -ENODEV;
	}
	
	struct alc5623_priv *alc5623;
	alc5623 = snd_soc_codec_get_drvdata(alc5623_codec);

	/* Associate the codec to the card */
	socdev->card->codec = alc5623_codec;
	//alc5623_codec->card = socdev->card;

	/* Register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register new PCMs\n");
	}

        /* call the codec probe function */
        ret = alc5623_probe(alc5623_codec);
        if (ret != 0) {
                dev_err(&pdev->dev, "Failed to probe codec: %d\n", ret);
                return ret;
        }

	/* Register the controls and widgets */
//	snd_soc_add_controls(alc5623_codec, alc5623_snd_controls,
//					ARRAY_SIZE(alc5623_snd_controls));

	snd_soc_dapm_new_controls(alc5623_codec, alc5623_dapm_widgets,
					ARRAY_SIZE(alc5623_dapm_widgets));

	/* set up audio path interconnects */
	snd_soc_dapm_add_routes(alc5623_codec, audio_map, ARRAY_SIZE(audio_map));
	
	switch (alc5623->id) {
	case 0x21:
	case 0x22:
		snd_soc_dapm_new_controls(alc5623_codec, alc5623_dapm_amp_widgets,
					ARRAY_SIZE(alc5623_dapm_amp_widgets));
		snd_soc_dapm_add_routes(alc5623_codec, intercon_amp_spk,
					ARRAY_SIZE(intercon_amp_spk));
		break;
	case 0x23:
		snd_soc_dapm_add_routes(alc5623_codec, intercon_spk,
					ARRAY_SIZE(intercon_spk));
		break;
	default:
		return -EINVAL;
	}

	/* make sure to register the dapm widgets */
	snd_soc_dapm_new_widgets(alc5623_codec);
	
	return ret;
}

/* power down chip */
static int alc5623_plat_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	/* Release PCMs and DAPM controls */
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_alc5623 = {
	.probe = 	alc5623_plat_probe,
	.remove = 	alc5623_plat_remove,
	.suspend = 	alc5623_suspend,
	.resume =	alc5623_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_alc5623);
#endif

static int __init alc5623_modinit(void)
{
	int ret;

	ret = i2c_add_driver(&alc5623_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "%s: can't add i2c driver", __func__);
		return ret;
	}

	return ret;
}
module_init(alc5623_modinit);

static void __exit alc5623_modexit(void)
{
	i2c_del_driver(&alc5623_i2c_driver);
}
module_exit(alc5623_modexit);

MODULE_DESCRIPTION("ASoC alc5621/2/3 driver");
MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>");
MODULE_LICENSE("GPL");
