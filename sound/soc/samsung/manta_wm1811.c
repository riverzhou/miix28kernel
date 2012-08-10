/*
 * Copyright (C) 2012 Google, Inc.
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 * Copyright (C) 2012 Wolfson Microelectronics
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/mfd/wm8994/registers.h>

#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../codecs/wm8994.h"
#include "../../../arch/arm/mach-exynos/board-manta.h"

#define MCLK1_FREQ	24000000
#define MCLK2_FREQ	32768

struct manta_wm1811 {
	struct clk *clk;
	bool clock_on;
	struct snd_soc_jack jack;
};

static const struct snd_kcontrol_new manta_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
};

const struct snd_soc_dapm_widget manta_widgets_lunchbox[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),

	SND_SOC_DAPM_INPUT("S5P RP"),
};

const struct snd_soc_dapm_widget manta_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("2nd Mic", NULL),
	SND_SOC_DAPM_MIC("3rd Mic", NULL),

	SND_SOC_DAPM_INPUT("S5P RP"),
};

const struct snd_soc_dapm_route manta_paths_lunchbox[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },
	{ "SPK", NULL, "SPKOUTRN" },
	{ "SPK", NULL, "SPKOUTRP" },

	{ "IN1LP", NULL, "MICBIAS1" },
	{ "IN1LN", NULL, "MICBIAS1" },
	{ "MICBIAS1", NULL, "Main Mic" },

	{ "IN1RP", NULL, "MICBIAS1" },
	{ "IN1RN", NULL, "MICBIAS1" },
	{ "MICBIAS1", NULL, "Sub Mic" },

	{ "IN2RP:VXRP", NULL, "MICBIAS2" },
	{ "MICBIAS2", NULL, "Headset Mic" },

	{ "AIF1DAC1L", NULL, "S5P RP" },
	{ "AIF1DAC1R", NULL, "S5P RP" },
};

const struct snd_soc_dapm_route manta_paths[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },
	{ "SPK", NULL, "SPKOUTRN" },
	{ "SPK", NULL, "SPKOUTRP" },

	{ "IN1LP", NULL, "MICBIAS1" },
	{ "IN1LN", NULL, "MICBIAS1" },
	{ "MICBIAS1", NULL, "3rd Mic" },

	{ "IN1RP", NULL, "MICBIAS2" },
	{ "IN1RN", NULL, "MICBIAS2" },
	{ "MICBIAS1", NULL, "Headset Mic" },

	{ "IN2LP:VXRN", NULL, "MICBIAS1" },
	{ "MICBIAS2", NULL, "2nd Mic" },

	{ "IN2RP:VXRP", NULL, "MICBIAS1" },
	{ "MICBIAS2", NULL, "Main Mic" },

	{ "AIF1DAC1L", NULL, "S5P RP" },
	{ "AIF1DAC1R", NULL, "S5P RP" },
};

static int manta_set_bias_level_post(struct snd_soc_card *card,
					struct snd_soc_dapm_context *dapm,
					enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct manta_wm1811 *machine =
				snd_soc_card_get_drvdata(card);
	int ret;

	if (dapm->dev != codec_dai->dev)
		return 0;

	if (level == SND_SOC_BIAS_STANDBY) {
		/*
		 * Playback/capture has stopped, so switch to the slower
		 * MCLK2 for reduced power consumption. hw_params handles
		 * turning the FLL back on when needed.
		 */
		ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_MCLK2,
						MCLK2_FREQ, SND_SOC_CLOCK_IN);
		if (ret < 0) {
			pr_err("Failed to switch away from FLL: %d\n", ret);
			return ret;
		}

		ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1,
						0, 0, 0);
		if (ret < 0) {
			pr_err("Failed to stop FLL: %d\n", ret);
			return ret;
		}

		/* Stop the reference clock for the codec's FLL */
		if (machine->clock_on) {
			clk_disable(machine->clk);
			machine->clock_on = false;
		}
	}

	return 0;
}

static int manta_wm1811_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct manta_wm1811 *machine =
				snd_soc_card_get_drvdata(rtd->codec->card);
	unsigned int pll_out;
	int ret;

	/* Start the reference clock for the codec's FLL */
	if (!machine->clock_on) {
		clk_enable(machine->clk);
		machine->clock_on = true;
	}

	/* AIF1CLK should be >=3MHz for optimal performance */
	if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE)
		pll_out = params_rate(params) * 384;
	else if (params_rate(params) == 8000 || params_rate(params) == 11025)
		pll_out = params_rate(params) * 512;
	else
		pll_out = params_rate(params) * 256;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Switch the FLL */
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1, WM8994_FLL_SRC_MCLK1,
					MCLK1_FREQ, pll_out);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to start FLL1\n");

	/* Then switch AIF1CLK to it */
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1,
					pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to switch to FLL1\n");

	return 0;
}

static struct snd_soc_ops manta_wm1811_aif1_ops = {
	.hw_params = manta_wm1811_aif1_hw_params,
};

static struct snd_soc_dai_link manta_dai[] = {
	{
		.name = "media-pri",
		.stream_name = "Media primary",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "wm8994-codec",
		.ops = &manta_wm1811_aif1_ops,
	},
	{
		.name = "media-sec",
		.stream_name = "Media secondary",
		.cpu_dai_name = "samsung-i2s.4",
		.codec_dai_name = "wm8994-aif1",
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
		.platform_name = "samsung-idma",
#else
		.platform_name = "samsung-audio",
#endif
		.codec_name = "wm8994-codec",
		.ops = &manta_wm1811_aif1_ops,
	},
};

static int manta_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct manta_wm1811 *machine =
				snd_soc_card_get_drvdata(codec->card);
	int ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_MCLK2,
				MCLK2_FREQ, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec->dev, "Unable to switch to MCLK2\n");

	/* Force AIF1CLK on as it will be master for jack detection */
	ret = snd_soc_dapm_force_enable_pin(&codec->dapm, "AIF1CLK");
	if (ret < 0)
		dev_err(codec->dev, "Failed to enable AIF1CLK\n");

	ret = snd_soc_dapm_disable_pin(&codec->dapm, "S5P RP");
	if (ret < 0)
		dev_err(codec->dev, "Failed to disable S5P RP\n");

	ret = snd_soc_jack_new(codec, "Headset",
				SND_JACK_HEADSET | SND_JACK_MECHANICAL,
				&machine->jack);
	if (ret) {
		dev_err(codec->dev, "Failed to create jack: %d\n", ret);
		return ret;
	}

	wm8958_mic_detect(codec, &machine->jack, NULL, NULL);

	return 0;
}

static int manta_card_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;

	snd_soc_update_bits(codec, WM8994_AIF1_MASTER_SLAVE,
					WM8994_AIF1_TRI_MASK, WM8994_AIF1_TRI);

	return 0;
}

static int manta_card_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;

	snd_soc_update_bits(codec, WM8994_AIF1_MASTER_SLAVE,
					WM8994_AIF1_TRI_MASK, 0);

	return 0;
}

static struct snd_soc_card manta = {
	.name = "Manta-I2S",
	.owner = THIS_MODULE,
	.dai_link = manta_dai,
	.num_links = ARRAY_SIZE(manta_dai),

	.set_bias_level_post = manta_set_bias_level_post,

	.controls = manta_controls,
	.num_controls = ARRAY_SIZE(manta_controls),
	.dapm_widgets = manta_widgets,
	.num_dapm_widgets = ARRAY_SIZE(manta_widgets),
	.dapm_routes = manta_paths,
	.num_dapm_routes = ARRAY_SIZE(manta_paths),

	.late_probe = manta_late_probe,

	.suspend_post = manta_card_suspend_post,
	.resume_pre = manta_card_resume_pre,
};

static int __devinit snd_manta_probe(struct platform_device *pdev)
{
	struct manta_wm1811 *machine;
	int ret;
	int hwrev = exynos5_manta_get_revision();

	machine = kzalloc(sizeof(*machine), GFP_KERNEL);
	if (!machine) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	machine->clk = clk_get(&pdev->dev, "system_clk");
	if (IS_ERR(machine->clk)) {
		pr_err("failed to get system_clk\n");
		ret = PTR_ERR(machine->clk);
		goto err_clk_get;
	}

	snd_soc_card_set_drvdata(&manta, machine);

	if (hwrev < MANTA_REV_PRE_ALPHA) {
		manta.dapm_widgets = manta_widgets_lunchbox,
		manta.num_dapm_widgets = ARRAY_SIZE(manta_widgets_lunchbox),
		manta.dapm_routes = manta_paths_lunchbox;
		manta.num_dapm_routes = ARRAY_SIZE(manta_paths_lunchbox);
	}

	manta.dev = &pdev->dev;
	ret = snd_soc_register_card(&manta);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed %d\n", ret);
		goto err_register_card;
	}

	return 0;

err_register_card:
	clk_put(machine->clk);
err_clk_get:
	kfree(machine);
err_kzalloc:
	return ret;
}

static int __devexit snd_manta_remove(struct platform_device *pdev)
{
	struct manta_wm1811 *machine = snd_soc_card_get_drvdata(&manta);

	snd_soc_unregister_card(&manta);
	clk_put(machine->clk);
	kfree(machine);

	return 0;
}

static struct platform_driver snd_manta_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "manta-i2s",
		.pm = &snd_soc_pm_ops,
	},
	.probe = snd_manta_probe,
	.remove = __devexit_p(snd_manta_remove),
};

module_platform_driver(snd_manta_driver);

MODULE_DESCRIPTION("ALSA SoC Manta WM1811");
MODULE_LICENSE("GPL");
