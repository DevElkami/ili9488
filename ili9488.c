#define DEBUG
/*
 * Copyright (C) 2019, Elkami
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>
#include <linux/regulator/consumer.h>

#include <linux/pinctrl/pinctrl-state.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/devinfo.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#define WIDTH_MM 45
#define HEIGHT_MM 33

#define GPIO_HIGH 1
#define GPIO_LOW 0

struct ili9488c {
	struct drm_panel	panel;
	struct mipi_dsi_device	*dsi;

	struct backlight_device *backlight;
	struct regulator	*power;
	struct gpio_desc	*reset;
};

static inline struct ili9488c *panel_to_ili9488c(struct drm_panel *panel)
{
	return container_of(panel, struct ili9488c, panel);
}

void static ili9488c_sendCmd(struct ili9488c* ctx, u8 cmd, const void *data, size_t len)
{		
	int ret = mipi_dsi_dcs_write(ctx->dsi, cmd, data, len);
	if (ret < 0)		
		printk(KERN_ERR "ili9488 send %02x command failed. Size = %ld\n", cmd, len);
	else 
		printk(KERN_INFO "ili9488 send %02x command with success. Size = %ld\n", cmd, len);
}

static int ili9488c_prepare(struct drm_panel *panel)
{
	printk(KERN_INFO "ili9488c_prepare\n");
	struct ili9488c* ctx = panel_to_ili9488c(panel);
	int ret = regulator_enable(ctx->power); /* Power the panel */
	if (ret)
	{
		printk(KERN_ERR "ili9488c_prepare - return regulator_enable\n");
		return ret;
	}
	msleep(5);

	/* And reset it */
	gpiod_set_value(ctx->reset, GPIO_HIGH);
	msleep(1);

	gpiod_set_value(ctx->reset, GPIO_LOW);
	msleep(10);	
	
	gpiod_set_value(ctx->reset, GPIO_HIGH);
	msleep(120);		

	// Init register	
	{
		u8 buf[] = { 0xFF, 0x98, 0x06 }; 
		ili9488c_sendCmd(ctx, 0xFF, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0x00, 0x04, 0x0E, 0x08, 0x17, 0x0A, 0x40, 0x79, 0x4D, 0x07, 0x0E, 0x0A, 0x1A, 0x1D, 0x0F }; 
		ili9488c_sendCmd(ctx, 0xE0, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0x00, 0x1B, 0x1F, 0x02, 0x10, 0x05, 0x32, 0x34, 0x43, 0x02, 0x0A, 0x09, 0x33, 0x37, 0x0F }; 
		ili9488c_sendCmd(ctx, 0xE1, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0x41 }; 
		ili9488c_sendCmd(ctx, 0xC1, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0x00, 0x22, 0x80 }; 
		ili9488c_sendCmd(ctx, 0xC5, buf, sizeof(buf));
	}	
	
	{
		u8 buf[] = { 0x48 }; 
		ili9488c_sendCmd(ctx, 0x36, buf, sizeof(buf));
	}	
	
	{
		u8 buf[] = { 0x70 }; 
		ili9488c_sendCmd(ctx, 0x3A, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0x00 }; 
		ili9488c_sendCmd(ctx, 0xB0, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0xB0 }; 
		ili9488c_sendCmd(ctx, 0xB1, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0x02 }; 
		ili9488c_sendCmd(ctx, 0xB4, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0x02, 0x02, 0x3B }; 
		ili9488c_sendCmd(ctx, 0xB6, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0x00 }; 
		ili9488c_sendCmd(ctx, 0xE9, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0xA9, 0x51, 0x2c, 0x82 }; 
		ili9488c_sendCmd(ctx, 0xF7, buf, sizeof(buf));
	}		
	
	{
		u8 buf[] = { 0x00, 0x00, 0x01, 0x3F }; 
		ili9488c_sendCmd(ctx, 0x2A, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0x00, 0x00, 0x00, 0xEF }; 
		ili9488c_sendCmd(ctx, 0x2B, buf, sizeof(buf));
	}
	
	{
		u8 buf[] = { 0x00 }; 
		ili9488c_sendCmd(ctx, 0x11, buf, sizeof(buf));
		msleep(120);
	}
	
	{
		u8 buf[] = { 0x00 }; 
		ili9488c_sendCmd(ctx, 0x29, buf, sizeof(buf));
		msleep(25);
	}
	
	{
		u8 buf[] = { 0x00 }; 
		ili9488c_sendCmd(ctx, 0x2C, buf, sizeof(buf));
	}	
	
	// TEARING NOT USE here
	/*ret = mipi_dsi_dcs_set_tear_on(ctx->dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret)
	{
		printk(KERN_ERR "ili9488c_prepare - mipi_dsi_dcs_set_tear_on failed\n");
		return ret;
	}*/

	ret = mipi_dsi_dcs_exit_sleep_mode(ctx->dsi);
	if (ret)
	{
		printk(KERN_ERR "ili9488c_prepare - mipi_dsi_dcs_exit_sleep_mode failed\n");
		return ret;
	}

	printk(KERN_ERR "ili9488c_prepare - success\n");
	return 0;
}

static int ili9488c_enable(struct drm_panel *panel)
{
	printk(KERN_INFO "ili9488c_enable\n");
	struct ili9488c *ctx = panel_to_ili9488c(panel);

	msleep(120);

	mipi_dsi_dcs_set_display_on(ctx->dsi);

	// No backlight managnement
	//backlight_enable(ctx->backlight);

	return 0;
}

static int ili9488c_disable(struct drm_panel *panel)
{
	printk(KERN_INFO "ili9488c_disable\n");
	struct ili9488c *ctx = panel_to_ili9488c(panel);

	// No backlight managnement
	//backlight_disable(ctx->backlight);
	return mipi_dsi_dcs_set_display_off(ctx->dsi);
}

static int ili9488c_unprepare(struct drm_panel *panel)
{
	printk(KERN_INFO "ili9488c_unprepare\n");
	struct ili9488c *ctx = panel_to_ili9488c(panel);

	mipi_dsi_dcs_enter_sleep_mode(ctx->dsi);
	regulator_disable(ctx->power);
	gpiod_set_value(ctx->reset, GPIO_LOW);

	return 0;
}

static const struct drm_display_mode driver_drm_default_mode = {
	.clock			= 6250, //In kHz. htotal (360) * vtotal (248) * frame_rate (70) / 1000; (source: https://github.com/freedreno/freedreno/wiki/DSI-Panel-Driver-Porting)
	.vrefresh		= 70, //Hz

/* From Linux source code documentation (https://elixir.bootlin.com/linux/v4.10.14/source/include/drm/drm_modes.h)	
 * The horizontal and vertical timings are defined per the following diagram.
 *
 *               Active                 Front           Sync           Back
 *              Region                 Porch                          Porch
 *     <-----------------------><----------------><-------------><-------------->
 *       //////////////////////|
 *      ////////////////////// |
 *     //////////////////////  |..................               ................
 *                                                _______________
 *     <----- [hv]display ----->
 *     <------------- [hv]sync_start ------------>
 *     <--------------------- [hv]sync_end --------------------->
 *     <-------------------------------- [hv]total ----------------------------->*/

	.hdisplay		= 320,
	.hsync_start	= 320 + 10, /*HAdr + HFP*/
	.hsync_end		= 320 + 10 + 10, /*HAdr + HFP + Hsync*/
	.htotal			= 320 + 10 + 10 + 20, /*HAdr + HFP + Hsync + HBP*/

	.vdisplay		= 240, 
	.vsync_start	= 240 + 4, /*VAdr + VFP*/
	.vsync_end		= 240 + 4 + 2, /*VAdr + VFP + Vsync*/
	.vtotal			= 240 + 4 + 2 + 2, /*VAdr + VFP + Vsync + VBP*/
	
	.width_mm 		= WIDTH_MM,
	.height_mm 		= HEIGHT_MM,
};

static int ili9488c_get_modes(struct drm_panel *panel)
{
	printk(KERN_INFO "ili9488c_get_modes\n");
	struct drm_connector *connector = panel->connector;
	struct ili9488c *ctx = panel_to_ili9488c(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &driver_drm_default_mode);
	if (!mode) 
	{
		dev_err(&ctx->dsi->dev, "failed to add mode %ux%ux@%u\n", driver_drm_default_mode.hdisplay, driver_drm_default_mode.vdisplay, driver_drm_default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	panel->connector->display_info.width_mm = WIDTH_MM;
	panel->connector->display_info.height_mm = HEIGHT_MM;

	return 1;
}

static const struct drm_panel_funcs ili9488c_funcs = {
	.prepare	= ili9488c_prepare,
	.unprepare	= ili9488c_unprepare,
	.enable		= ili9488c_enable,
	.disable	= ili9488c_disable,
	.get_modes	= ili9488c_get_modes,
};

static int ili9488c_dsi_probe(struct mipi_dsi_device *dsi)
{
	printk(KERN_INFO "ili9488 driver loaded - v1.2.0.3\n");	

	int ret;
	struct ili9488c* ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dsi = dsi;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = &dsi->dev;
	ctx->panel.funcs = &ili9488c_funcs;	
	ctx->power = devm_regulator_get(&dsi->dev, "power");
	if (IS_ERR(ctx->power)) 
	{
		printk(KERN_ERR "ili9488c_dsi_probe - Couldn't get our power regulator\n");
		return PTR_ERR(ctx->power);
	}
		
	// reset GPIO
	ctx->reset = devm_gpiod_get(&dsi->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset)) 	
		printk(KERN_ERR "ili9488c_dsi_probe - Couldn't get our reset GPIO\n");	
	else	
		printk(KERN_INFO "ili9488c_dsi_probe - Get reset GPIO - OK\n");
	
	// Handle GPIO 32
	struct gpio_desc* descGpio32 = devm_gpiod_get(&dsi->dev, "enable", GPIOD_OUT_HIGH);
	if (IS_ERR(descGpio32)) 
	{
		printk(KERN_ERR "ili9488c_dsi_probe - Couldn't get our GPIO32 directly\n");				
	}
	else	
	{
		printk(KERN_INFO "ili9488c_dsi_probe - Get GPIO 32 - OK\n");
		
		gpiod_set_value(descGpio32, GPIO_HIGH);	
		printk(KERN_INFO "ili9488c_dsi_probe - Set GPIO32 to high level\n");
	}	

	// No backlight managnement
	/*np = of_parse_phandle(dsi->dev.of_node, "backlight", 0);
	if (np) {
		ctx->backlight = of_find_backlight_by_node(np);
		of_node_put(np);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}
*/
	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
	{
		printk(KERN_ERR "ili9488c_dsi_probe - drm_panel_add failed\n");
		return ret;
	}

	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->lanes = 1;

	printk(KERN_INFO "ili9488c_dsi_probe success\n");

	return mipi_dsi_attach(dsi);
}

static int ili9488c_dsi_remove(struct mipi_dsi_device *dsi)
{
	printk(KERN_INFO "ili9488c_dsi_remove\n");
	struct ili9488c *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	// No backlight management
	/*if (ctx->backlight)
		put_device(&ctx->backlight->dev);*/

	return 0;
}

static const struct of_device_id ili9488c_of_match[] = {
	{ .compatible = "ilitek,ili9488_dev" },
	{ }
};
MODULE_DEVICE_TABLE(of, ili9488c_of_match);

static struct mipi_dsi_driver ili9488c_dsi_driver = {
	.probe		= ili9488c_dsi_probe,
	.remove		= ili9488c_dsi_remove,
	.driver = {
		.name		= "ili9488_dev",
		.of_match_table	= ili9488c_of_match,
	},
};
module_mipi_dsi_driver(ili9488c_dsi_driver);

MODULE_AUTHOR("Elkami");
MODULE_DESCRIPTION("Ilitek ILI9488 Controller Driver");
MODULE_LICENSE("GPL v2");
