// SPDX-License-Identifier: GPL-2.0+
/*
 * panel-r36s.c — GameConsole R36S 3.5" 640x480 MIPI-DSI 面板
 *
 * 为什么要有这个驱动：
 *   厂商 dtb 里写的是 compatible = "simple-panel-dsi" + panel-init-sequence，
 *   但 mainline 的 panel-simple **不认** simple-panel-dsi，也不解析
 *   panel-init-sequence；mainline 的 st7701 驱动是重构过的结构化写法
 *   （gamma/GIP 描述符），装不下这段厂商序列。
 *   所以最省事、最好维护的做法：一个 ~200 行的独立驱动，直接发 DCS 序列。
 *
 * 状态：**未编译、未上机**。所有数值来自厂商 dtb 实测，见 dts/README.md。
 *
 * 放进内核：
 *   drivers/gpu/drm/panel/panel-r36s.c
 *   drivers/gpu/drm/panel/Kconfig          （加 config DRM_PANEL_R36S）
 *   drivers/gpu/drm/panel/Makefile         （obj-$(CONFIG_DRM_PANEL_R36S) += panel-r36s.o）
 *   arch/arm64/configs/<defconfig>         （CONFIG_DRM_PANEL_R36S=y）
 *   dts: compatible = "rockchip,r36s-panel";
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <video/mipi_display.h>

struct r36s_panel {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *vcc;
	struct regulator *iovcc;
	struct gpio_desc *reset;
};

static inline struct r36s_panel *to_r36s(struct drm_panel *p)
{
	return container_of(p, struct r36s_panel, panel);
}

/*
 * 厂商 display-timings/original：
 *   58 MHz，640x480，hfp=300 hsync=200 hbp=300，vfp=17 vsync=5 vbp=13
 *   hsync-active=0 vsync-active=0 de-active=0 pixelclk-active=1
 */
static const struct drm_display_mode r36s_mode = {
	.clock		= 58000,
	.hdisplay	= 640,
	.hsync_start	= 640 + 300,
	.hsync_end	= 640 + 300 + 200,
	.htotal		= 640 + 300 + 200 + 300,
	.vdisplay	= 480,
	.vsync_start	= 480 + 17,
	.vsync_end	= 480 + 17 + 5,
	.vtotal		= 480 + 17 + 5 + 13,
	.flags		= DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
	.type		= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

/* 厂商 panel-init-sequence 逐条展开（由 dtb 自动生成，勿手改） */
static int r36s_init_sequence(struct mipi_dsi_device *dsi)
{
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x30);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x52);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xe3, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x25, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0x28, 0x6f);
	mipi_dsi_dcs_write_seq(dsi, 0x29, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x2a, 0xdf);
	mipi_dsi_dcs_write_seq(dsi, 0x2c, 0x22);
	mipi_dsi_dcs_write_seq(dsi, 0xc3, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x37, 0x9c);
	mipi_dsi_dcs_write_seq(dsi, 0x38, 0xa7);
	mipi_dsi_dcs_write_seq(dsi, 0x39, 0x41);
	mipi_dsi_dcs_write_seq(dsi, 0x80, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0x91, 0x67);
	mipi_dsi_dcs_write_seq(dsi, 0x92, 0x67);
	mipi_dsi_dcs_write_seq(dsi, 0xa0, 0x55);
	mipi_dsi_dcs_write_seq(dsi, 0xa1, 0x50);
	mipi_dsi_dcs_write_seq(dsi, 0xa3, 0x58);
	mipi_dsi_dcs_write_seq(dsi, 0xa4, 0x9c);
	mipi_dsi_dcs_write_seq(dsi, 0xa7, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xa8, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xa9, 0x21);
	mipi_dsi_dcs_write_seq(dsi, 0xaa, 0xfc);
	mipi_dsi_dcs_write_seq(dsi, 0xab, 0x28);
	mipi_dsi_dcs_write_seq(dsi, 0xac, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0xad, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0xae, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0xaf, 0x03);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0xb1, 0x26);
	mipi_dsi_dcs_write_seq(dsi, 0xb2, 0x28);
	mipi_dsi_dcs_write_seq(dsi, 0xb3, 0x28);
	mipi_dsi_dcs_write_seq(dsi, 0xb4, 0x03);
	mipi_dsi_dcs_write_seq(dsi, 0xb5, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0xb6, 0x26);
	mipi_dsi_dcs_write_seq(dsi, 0xb7, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0xb8, 0x26);
	mipi_dsi_dcs_write_seq(dsi, 0x2c, 0x22);
	mipi_dsi_dcs_write_seq(dsi, 0x5c, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0xc0, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xc1, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xc2, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x30);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x52);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xd0, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xb1, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0xd1, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xb2, 0x11);
	mipi_dsi_dcs_write_seq(dsi, 0xd2, 0x12);
	mipi_dsi_dcs_write_seq(dsi, 0xb3, 0x32);
	mipi_dsi_dcs_write_seq(dsi, 0xd3, 0x33);
	mipi_dsi_dcs_write_seq(dsi, 0xb4, 0x36);
	mipi_dsi_dcs_write_seq(dsi, 0xd4, 0x36);
	mipi_dsi_dcs_write_seq(dsi, 0xb5, 0x3c);
	mipi_dsi_dcs_write_seq(dsi, 0xd5, 0x3c);
	mipi_dsi_dcs_write_seq(dsi, 0xb6, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0xd6, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0xb7, 0x3e);
	mipi_dsi_dcs_write_seq(dsi, 0xd7, 0x3e);
	mipi_dsi_dcs_write_seq(dsi, 0xb8, 0x0e);
	mipi_dsi_dcs_write_seq(dsi, 0xd8, 0x0d);
	mipi_dsi_dcs_write_seq(dsi, 0xb9, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0xd9, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0xba, 0x11);
	mipi_dsi_dcs_write_seq(dsi, 0xda, 0x12);
	mipi_dsi_dcs_write_seq(dsi, 0xbb, 0x11);
	mipi_dsi_dcs_write_seq(dsi, 0xdb, 0x11);
	mipi_dsi_dcs_write_seq(dsi, 0xbc, 0x13);
	mipi_dsi_dcs_write_seq(dsi, 0xdc, 0x14);
	mipi_dsi_dcs_write_seq(dsi, 0xbd, 0x14);
	mipi_dsi_dcs_write_seq(dsi, 0xdd, 0x14);
	mipi_dsi_dcs_write_seq(dsi, 0xbe, 0x16);
	mipi_dsi_dcs_write_seq(dsi, 0xde, 0x18);
	mipi_dsi_dcs_write_seq(dsi, 0xbf, 0x0e);
	mipi_dsi_dcs_write_seq(dsi, 0xdf, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0xc0, 0x17);
	mipi_dsi_dcs_write_seq(dsi, 0xe0, 0x17);
	mipi_dsi_dcs_write_seq(dsi, 0xc1, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0xe1, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x30);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x52);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x03);
	mipi_dsi_dcs_write_seq(dsi, 0x08, 0x8a);
	mipi_dsi_dcs_write_seq(dsi, 0x09, 0x8b);
	mipi_dsi_dcs_write_seq(dsi, 0x30, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x31, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x32, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x33, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x34, 0x61);
	mipi_dsi_dcs_write_seq(dsi, 0x35, 0xd4);
	mipi_dsi_dcs_write_seq(dsi, 0x36, 0x24);
	mipi_dsi_dcs_write_seq(dsi, 0x37, 0x03);
	mipi_dsi_dcs_write_seq(dsi, 0x40, 0x86);
	mipi_dsi_dcs_write_seq(dsi, 0x41, 0x87);
	mipi_dsi_dcs_write_seq(dsi, 0x42, 0x84);
	mipi_dsi_dcs_write_seq(dsi, 0x43, 0x85);
	mipi_dsi_dcs_write_seq(dsi, 0x44, 0x11);
	mipi_dsi_dcs_write_seq(dsi, 0x45, 0xde);
	mipi_dsi_dcs_write_seq(dsi, 0x46, 0xdd);
	mipi_dsi_dcs_write_seq(dsi, 0x47, 0x11);
	mipi_dsi_dcs_write_seq(dsi, 0x48, 0xe0);
	mipi_dsi_dcs_write_seq(dsi, 0x49, 0xdf);
	mipi_dsi_dcs_write_seq(dsi, 0x50, 0x82);
	mipi_dsi_dcs_write_seq(dsi, 0x51, 0x83);
	mipi_dsi_dcs_write_seq(dsi, 0x52, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x53, 0x81);
	mipi_dsi_dcs_write_seq(dsi, 0x54, 0x11);
	mipi_dsi_dcs_write_seq(dsi, 0x55, 0xe2);
	mipi_dsi_dcs_write_seq(dsi, 0x56, 0xe1);
	mipi_dsi_dcs_write_seq(dsi, 0x57, 0x11);
	mipi_dsi_dcs_write_seq(dsi, 0x58, 0xe4);
	mipi_dsi_dcs_write_seq(dsi, 0x59, 0xe3);
	mipi_dsi_dcs_write_seq(dsi, 0x82, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x83, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x84, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x85, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x86, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x87, 0x0e);
	mipi_dsi_dcs_write_seq(dsi, 0x88, 0x0e);
	mipi_dsi_dcs_write_seq(dsi, 0x89, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0x8a, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0x8b, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x8c, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x8d, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x8e, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x8f, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0x90, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0x98, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x99, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x9a, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x9b, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x9c, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x9d, 0x0e);
	mipi_dsi_dcs_write_seq(dsi, 0x9e, 0x0e);
	mipi_dsi_dcs_write_seq(dsi, 0x9f, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0xa0, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0xa1, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0xa2, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0xa3, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0xa4, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0xa5, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0xa6, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0xe0, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xe1, 0x52);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x30);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x52);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x36, 0x02);

	return 0;
}

static int r36s_prepare(struct drm_panel *panel)
{
	struct r36s_panel *ctx = to_r36s(panel);
	int ret;

	ret = regulator_enable(ctx->iovcc);
	if (ret)
		return ret;
	ret = regulator_enable(ctx->vcc);
	if (ret) {
		regulator_disable(ctx->iovcc);
		return ret;
	}

	dev_info(panel->dev, "r36s: prepare\n");
	msleep(2);				/* prepare-delay-ms */
	gpiod_set_value_cansleep(ctx->reset, 0);
	msleep(1);				/* reset-delay-ms */
	gpiod_set_value_cansleep(ctx->reset, 1);
	msleep(20);				/* init-delay-ms */

	return 0;
}

static int r36s_unprepare(struct drm_panel *panel)
{
	struct r36s_panel *ctx = to_r36s(panel);

	gpiod_set_value_cansleep(ctx->reset, 0);
	msleep(20);				/* unprepare-delay-ms */
	regulator_disable(ctx->vcc);
	regulator_disable(ctx->iovcc);

	return 0;
}

static int r36s_enable(struct drm_panel *panel)
{
	struct r36s_panel *ctx = to_r36s(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dev_info(dev, "r36s: enable (lanes=%d flags=0x%lx)\n",
		 dsi->lanes, dsi->mode_flags);
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = r36s_init_sequence(dsi);
	if (ret) {
		dev_err(dev, "r36s: init sequence failed %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "exit sleep mode failed: %d\n", ret);
		return ret;
	}

	msleep(120);				/* enable-delay-ms */

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0)
		dev_err(dev, "set display on failed: %d\n", ret);

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	return ret;
}

static int r36s_disable(struct drm_panel *panel)
{
	struct r36s_panel *ctx = to_r36s(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "set display off failed: %d\n", ret);

	msleep(50);				/* disable-delay-ms */

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "enter sleep mode failed: %d\n", ret);

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	return 0;
}

static int r36s_get_modes(struct drm_panel *panel,
			  struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &r36s_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs r36s_funcs = {
	.prepare	= r36s_prepare,
	.unprepare	= r36s_unprepare,
	.enable		= r36s_enable,
	.disable	= r36s_disable,
	.get_modes	= r36s_get_modes,
};

static int r36s_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct r36s_panel *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->vcc = devm_regulator_get(dev, "VCC");
	if (IS_ERR(ctx->vcc))
		return dev_err_probe(dev, PTR_ERR(ctx->vcc), "VCC supply\n");

	ctx->iovcc = devm_regulator_get(dev, "IOVCC");
	if (IS_ERR(ctx->iovcc))
		return dev_err_probe(dev, PTR_ERR(ctx->iovcc), "IOVCC supply\n");

	ctx->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset))
		return dev_err_probe(dev, PTR_ERR(ctx->reset), "reset gpio\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	/*
	 * 厂商 dsi,flags = 0x0a03（VIDEO|BURST|bit9|LPM）。
	 * 注意：bit9 在厂商 4.4 头文件里叫 EOT_PACKET，mainline 改名为
	 * NO_EOT_PACKET 且语义相反；mainline 默认就发 EOT，所以不置这一位。
	 */
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &r36s_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return ret;

	dev_info(dev, "r36s: probe ok, 4 lanes, 640x480\n");
	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "attach failed\n");
	}

	return 0;
}

static void r36s_remove(struct mipi_dsi_device *dsi)
{
	struct r36s_panel *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id r36s_of_match[] = {
	{ .compatible = "rockchip,r36s-panel" },
	{ }
};
MODULE_DEVICE_TABLE(of, r36s_of_match);

static struct mipi_dsi_driver r36s_driver = {
	.probe	= r36s_probe,
	.remove	= r36s_remove,
	.driver = {
		.name		= "panel-r36s",
		.of_match_table	= r36s_of_match,
	},
};
module_mipi_dsi_driver(r36s_driver);

MODULE_AUTHOR("TreeOS");
MODULE_DESCRIPTION("GameConsole R36S 640x480 DSI panel");
MODULE_LICENSE("GPL");
