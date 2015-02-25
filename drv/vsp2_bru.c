/*************************************************************************/ /*
 VSP2

 Copyright (C) 2015 Renesas Electronics Corporation

 License        Dual MIT/GPLv2

 The contents of this file are subject to the MIT license as set out below.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 Alternatively, the contents of this file may be used under the terms of
 the GNU General Public License Version 2 ("GPL") in which case the provisions
 of GPL are applicable instead of those above.

 If you wish to allow use of your version of this file only under the terms of
 GPL, and not to allow others to use your version of this file under the terms
 of the MIT license, indicate your decision by deleting the provisions above
 and replace them with the notice and other provisions required by GPL as set
 out in the file called "GPL-COPYING" included in this distribution. If you do
 not delete the provisions above, a recipient may use your version of this file
 under the terms of either the MIT license or GPL.

 This License is also included in this distribution in the file called
 "MIT-COPYING".

 EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


 GPLv2:
 If you wish to use this file under the terms of GPL, following terms are
 effective.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/ /*************************************************************************/

#include <linux/device.h>
#include <linux/gfp.h>

#include <media/v4l2-subdev.h>

#include "vsp2.h"
#include "vsp2_bru.h"
#include "vsp2_rwpf.h"
#include "vsp2_vspm.h"

#define BRU_MIN_SIZE				1U
#define BRU_MAX_SIZE				8190U

/* -----------------------------------------------------------------------------
 * Controls
 */

static int bru_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct vsp2_bru *bru =
		container_of(ctrl->handler, struct vsp2_bru, ctrls);
	VSPM_VSP_PAR *vsp_par =
		bru->entity.vsp2->vspm->ip_par.unionIpParam.ptVsp;
	T_VSP_BRU *vsp_bru = vsp_par->ctrl_par->bru;

	if (!vsp2_entity_is_streaming(&bru->entity))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_BG_COLOR:
		vsp_bru->blend_virtual->color =
			ctrl->val | (0xff << VI6_BRU_VIRRPF_COL_A_SHIFT);
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops bru_ctrl_ops = {
	.s_ctrl = bru_s_ctrl,
};

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Core Operations
 */

static int bru_s_stream(struct v4l2_subdev *subdev, int enable)
{
	struct vsp2_pipeline *pipe = to_vsp2_pipeline(&subdev->entity);
	struct vsp2_bru *bru = to_bru(subdev);
	struct v4l2_mbus_framefmt *format;
	unsigned int flags;
	unsigned int i;
	int ret;
	VSPM_VSP_PAR *vsp_par =
		bru->entity.vsp2->vspm->ip_par.unionIpParam.ptVsp;
	T_VSP_BRU *vsp_bru = vsp_par->ctrl_par->bru;
	u32 inctrl;

	ret = vsp2_entity_set_streaming(&bru->entity, enable);
	if (ret < 0)
		return ret;

	if (!enable)
		return 0;

	format = &bru->entity.formats[BRU_PAD_SOURCE];

	/* The hardware is extremely flexible but we have no userspace API to
	 * expose all the parameters, nor is it clear whether we would have use
	 * cases for all the supported modes. Let's just harcode the parameters
	 * to sane default values for now.
	 */

	/* Disable dithering and enable color data normalization unless the
	 * format at the pipeline output is premultiplied.
	 */
	flags = pipe->output ? pipe->output->video.format.flags : 0;
	inctrl = flags & V4L2_PIX_FMT_FLAG_PREMUL_ALPHA ?
		 0 : VI6_BRU_INCTRL_NRM;
	vsp_bru->adiv    = (inctrl & (1 << 28)) >> 28;
	vsp_bru->qnt[0]  = (inctrl & (1 << 16)) >> 16;
	vsp_bru->qnt[1]  = (inctrl & (1 << 17)) >> 17;
	vsp_bru->qnt[2]  = (inctrl & (1 << 18)) >> 18;
	vsp_bru->qnt[3]  = (inctrl & (1 << 19)) >> 19;
	vsp_bru->dith[0] = (inctrl & (0x000F <<  0)) >>  0;
	vsp_bru->dith[1] = (inctrl & (0x000F <<  4)) >>  4;
	vsp_bru->dith[2] = (inctrl & (0x000F <<  8)) >>  8;
	vsp_bru->dith[3] = (inctrl & (0x000F << 12)) >> 12;

	/* Set the background position to cover the whole output image. */
	vsp_bru->blend_virtual->width		= format->width;
	vsp_bru->blend_virtual->height		= format->height;
	vsp_bru->blend_virtual->x_position	= 0;
	vsp_bru->blend_virtual->y_position	= 0;
	vsp_bru->blend_virtual->pwd		= VSP_LAYER_PARENT;

	/* Route BRU input 1 as SRC input to the ROP unit and configure the ROP
	 * unit with a NOP operation to make BRU input 1 available as the
	 * Blend/ROP unit B SRC input.
	 */
	vsp_bru->blend_rop = NULL;

	for (i = 0; i < 4; ++i) {
		bool premultiplied = false;
		u32 ctrl = 0;
		T_VSP_BLEND_CONTROL *vsp_bru_ctrl = NULL;
		switch (i) {
		case 0:
			vsp_bru_ctrl = vsp_bru->blend_control_a;
			break;
		case 1:
			vsp_bru_ctrl = vsp_bru->blend_control_b;
			break;
		case 2:
			vsp_bru_ctrl = vsp_bru->blend_control_c;
			break;
		case 3:
			vsp_bru_ctrl = vsp_bru->blend_control_d;
			break;
		default:
			/* Invalid index. */
			break;
		}

		/* Configure all Blend/ROP units corresponding to an enabled BRU
		 * input for alpha blending. Blend/ROP units corresponding to
		 * disabled BRU inputs are used in ROP NOP mode to ignore the
		 * SRC input.
		 */
		if (bru->inputs[i].rpf) {
			ctrl |= VI6_BRU_CTRL_RBC;

			premultiplied = bru->inputs[i].rpf->video.format.flags
				      & V4L2_PIX_FMT_FLAG_PREMUL_ALPHA;
		} else {
			ctrl |= VI6_BRU_CTRL_CROP(VI6_ROP_NOP)
			     |  VI6_BRU_CTRL_AROP(VI6_ROP_NOP);
		}

		/* Select the virtual RPF as the Blend/ROP unit A DST input to
		 * serve as a background color.
		 */
		if (i == 0)
			ctrl |= VI6_BRU_CTRL_DSTSEL_VRPF;

		/* Route BRU inputs 0 to 3 as SRC inputs to Blend/ROP units A to
		 * D in that order. The Blend/ROP unit B SRC is hardwired to the
		 * ROP unit output, the corresponding register bits must be set
		 * to 0.
		 */
		if (i != 1)
			ctrl |= VI6_BRU_CTRL_SRCSEL_BRUIN(i);

		vsp_bru_ctrl->rbc  = (ctrl & (1 << 31)) >> 31;
		vsp_bru_ctrl->crop = (ctrl & (0xF <<  4)) >>  4;
		vsp_bru_ctrl->arop = (ctrl & (0xF <<  0)) >>  0;

		/* Harcode the blending formula to
		 *
		 *	DSTc = DSTc * (1 - SRCa) + SRCc * SRCa
		 *	DSTa = DSTa * (1 - SRCa) + SRCa
		 *
		 * when the SRC input isn't premultiplied, and to
		 *
		 *	DSTc = DSTc * (1 - SRCa) + SRCc
		 *	DSTa = DSTa * (1 - SRCa) + SRCa
		 *
		 * otherwise.
		 */
		vsp_bru_ctrl->blend_formula = VSP_FORM_BLEND0;
		vsp_bru_ctrl->blend_coefx = VSP_COEFFICIENT_BLENDX4;
		vsp_bru_ctrl->blend_coefy = (premultiplied) ?
			VSP_COEFFICIENT_BLENDY5 : VSP_COEFFICIENT_BLENDY3;
		vsp_bru_ctrl->aformula = VSP_FORM_ALPHA0;
		vsp_bru_ctrl->acoefx = VSP_COEFFICIENT_ALPHAX4;
		vsp_bru_ctrl->acoefy = VSP_COEFFICIENT_ALPHAY5;
		vsp_bru_ctrl->acoefx_fix = 0;    /* Set coefficient x. */
		vsp_bru_ctrl->acoefy_fix = 0xFF; /* Set coefficient y. */
	}

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Pad Operations
 */

/*
 * The BRU can't perform format conversion, all sink and source formats must be
 * identical. We pick the format on the first sink pad (pad 0) and propagate it
 * to all other pads.
 */

static int bru_enum_mbus_code(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_fh *fh,
			      struct v4l2_subdev_mbus_code_enum *code)
{
	static const unsigned int codes[] = {
		V4L2_MBUS_FMT_ARGB8888_1X32,
		V4L2_MBUS_FMT_AYUV8_1X32,
	};
	struct v4l2_mbus_framefmt *format;

	if (code->pad == BRU_PAD_SINK(0)) {
		if (code->index >= ARRAY_SIZE(codes))
			return -EINVAL;

		code->code = codes[code->index];
	} else {
		if (code->index)
			return -EINVAL;

		format = v4l2_subdev_get_try_format(fh, BRU_PAD_SINK(0));
		code->code = format->code;
	}

	return 0;
}

static int bru_enum_frame_size(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_fh *fh,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index)
		return -EINVAL;

	if (fse->code != V4L2_MBUS_FMT_ARGB8888_1X32 &&
	    fse->code != V4L2_MBUS_FMT_AYUV8_1X32)
		return -EINVAL;

	fse->min_width = BRU_MIN_SIZE;
	fse->max_width = BRU_MAX_SIZE;
	fse->min_height = BRU_MIN_SIZE;
	fse->max_height = BRU_MAX_SIZE;

	return 0;
}

static struct v4l2_rect *bru_get_compose(struct vsp2_bru *bru,
					 struct v4l2_subdev_fh *fh,
					 unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_crop(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &bru->inputs[pad].compose;
	default:
		return NULL;
	}
}

static int bru_get_format(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct vsp2_bru *bru = to_bru(subdev);

	fmt->format = *vsp2_entity_get_pad_format(&bru->entity, fh, fmt->pad,
						  fmt->which);

	return 0;
}

static void bru_try_format(struct vsp2_bru *bru, struct v4l2_subdev_fh *fh,
			   unsigned int pad, struct v4l2_mbus_framefmt *fmt,
			   enum v4l2_subdev_format_whence which)
{
	struct v4l2_mbus_framefmt *format;

	switch (pad) {
	case BRU_PAD_SINK(0):
		/* Default to YUV if the requested format is not supported. */
		if (fmt->code != V4L2_MBUS_FMT_ARGB8888_1X32 &&
		    fmt->code != V4L2_MBUS_FMT_AYUV8_1X32)
			fmt->code = V4L2_MBUS_FMT_AYUV8_1X32;
		break;

	default:
		/* The BRU can't perform format conversion. */
		format = vsp2_entity_get_pad_format(&bru->entity, fh,
						    BRU_PAD_SINK(0), which);
		fmt->code = format->code;
		break;
	}

	fmt->width = clamp(fmt->width, BRU_MIN_SIZE, BRU_MAX_SIZE);
	fmt->height = clamp(fmt->height, BRU_MIN_SIZE, BRU_MAX_SIZE);
	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
}

static int bru_set_format(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct vsp2_bru *bru = to_bru(subdev);
	struct v4l2_mbus_framefmt *format;

	bru_try_format(bru, fh, fmt->pad, &fmt->format, fmt->which);

	format = vsp2_entity_get_pad_format(&bru->entity, fh, fmt->pad,
					    fmt->which);
	*format = fmt->format;

	/* Reset the compose rectangle */
	if (fmt->pad != BRU_PAD_SOURCE) {
		struct v4l2_rect *compose;

		compose = bru_get_compose(bru, fh, fmt->pad, fmt->which);
		compose->left = 0;
		compose->top = 0;
		compose->width = format->width;
		compose->height = format->height;
	}

	/* Propagate the format code to all pads */
	if (fmt->pad == BRU_PAD_SINK(0)) {
		unsigned int i;

		for (i = 0; i <= BRU_PAD_SOURCE; ++i) {
			format = vsp2_entity_get_pad_format(&bru->entity, fh,
							    i, fmt->which);
			format->code = fmt->format.code;
		}
	}

	return 0;
}

static int bru_get_selection(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_fh *fh,
			     struct v4l2_subdev_selection *sel)
{
	struct vsp2_bru *bru = to_bru(subdev);

	if (sel->pad == BRU_PAD_SOURCE)
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = BRU_MAX_SIZE;
		sel->r.height = BRU_MAX_SIZE;
		return 0;

	case V4L2_SEL_TGT_COMPOSE:
		sel->r = *bru_get_compose(bru, fh, sel->pad, sel->which);
		return 0;

	default:
		return -EINVAL;
	}
}

static int bru_set_selection(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_fh *fh,
			     struct v4l2_subdev_selection *sel)
{
	struct vsp2_bru *bru = to_bru(subdev);
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *compose;

	if (sel->pad == BRU_PAD_SOURCE)
		return -EINVAL;

	if (sel->target != V4L2_SEL_TGT_COMPOSE)
		return -EINVAL;

	/* The compose rectangle top left corner must be inside the output
	 * frame.
	 */
	format = vsp2_entity_get_pad_format(&bru->entity, fh, BRU_PAD_SOURCE,
					    sel->which);
	sel->r.left = clamp_t(unsigned int, sel->r.left, 0, format->width - 1);
	sel->r.top = clamp_t(unsigned int, sel->r.top, 0, format->height - 1);

	/* Scaling isn't supported, the compose rectangle size must be identical
	 * to the sink format size.
	 */
	format = vsp2_entity_get_pad_format(&bru->entity, fh, sel->pad,
					    sel->which);
	sel->r.width = format->width;
	sel->r.height = format->height;

	compose = bru_get_compose(bru, fh, sel->pad, sel->which);
	*compose = sel->r;

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Operations
 */

static struct v4l2_subdev_video_ops bru_video_ops = {
	.s_stream = bru_s_stream,
};

static struct v4l2_subdev_pad_ops bru_pad_ops = {
	.enum_mbus_code = bru_enum_mbus_code,
	.enum_frame_size = bru_enum_frame_size,
	.get_fmt = bru_get_format,
	.set_fmt = bru_set_format,
	.get_selection = bru_get_selection,
	.set_selection = bru_set_selection,
};

static struct v4l2_subdev_ops bru_ops = {
	.video	= &bru_video_ops,
	.pad    = &bru_pad_ops,
};

/* -----------------------------------------------------------------------------
 * Initialization and Cleanup
 */

struct vsp2_bru *vsp2_bru_create(struct vsp2_device *vsp2)
{
	struct v4l2_subdev *subdev;
	struct vsp2_bru *bru;
	int ret;

	bru = devm_kzalloc(vsp2->dev, sizeof(*bru), GFP_KERNEL);
	if (bru == NULL)
		return ERR_PTR(-ENOMEM);

	bru->entity.type = VSP2_ENTITY_BRU;

	ret = vsp2_entity_init(vsp2, &bru->entity, 5);
	if (ret < 0)
		return ERR_PTR(ret);

	/* Initialize the V4L2 subdev. */
	subdev = &bru->entity.subdev;
	v4l2_subdev_init(subdev, &bru_ops);

	subdev->entity.ops = &vsp2_media_ops;
	subdev->internal_ops = &vsp2_subdev_internal_ops;
	snprintf(subdev->name, sizeof(subdev->name), "%s bru",
		 dev_name(vsp2->dev));
	v4l2_set_subdevdata(subdev, bru);
	subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	vsp2_entity_init_formats(subdev, NULL);

	/* Initialize the control handler. */
	v4l2_ctrl_handler_init(&bru->ctrls, 1);
	v4l2_ctrl_new_std(&bru->ctrls, &bru_ctrl_ops, V4L2_CID_BG_COLOR,
			  0, 0xffffff, 1, 0);

	bru->entity.subdev.ctrl_handler = &bru->ctrls;

	if (bru->ctrls.error) {
		dev_err(vsp2->dev, "bru: failed to initialize controls\n");
		ret = bru->ctrls.error;
		vsp2_entity_destroy(&bru->entity);
		return ERR_PTR(ret);
	}

	return bru;
}
