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
#include "vsp2_uds.h"
#include "vsp2_vspm.h"

#define UDS_MIN_SIZE				4U
#define UDS_MAX_SIZE				8190U

#define UDS_MIN_FACTOR				0x0100
#define UDS_MAX_FACTOR				0xffff

/* -----------------------------------------------------------------------------
 * Scaling Computation
 */

void vsp2_uds_set_alpha(struct vsp2_uds *uds, unsigned int alpha)
{
	VSPM_VSP_PAR *vsp_par =
		uds->entity.vsp2->vspm->ip_par.unionIpParam.ptVsp;
	T_VSP_UDS *vsp_uds = vsp_par->ctrl_par->uds;

	vsp_uds->anum0 = (alpha & (0x000000FF <<  0)) >>  0;
	vsp_uds->anum1 = (alpha & (0x000000FF <<  8)) >>  8;
	vsp_uds->anum2 = (alpha & (0x000000FF << 16)) >> 16;
}

/*
 * uds_output_size - Return the output size for an input size and scaling ratio
 * @input: input size in pixels
 * @ratio: scaling ratio in U4.12 fixed-point format
 */
static unsigned int uds_output_size(unsigned int input, unsigned int ratio)
{
	if (ratio > 4096) {
		/* Down-scaling */
		unsigned int mp;

		mp = ratio / 4096;
		mp = mp < 4 ? 1 : (mp < 8 ? 2 : 4);

		return (input - 1) / mp * mp * 4096 / ratio + 1;
	} else {
		/* Up-scaling */
		return (input - 1) * 4096 / ratio + 1;
	}
}

/*
 * uds_output_limits - Return the min and max output sizes for an input size
 * @input: input size in pixels
 * @minimum: minimum output size (returned)
 * @maximum: maximum output size (returned)
 */
static void uds_output_limits(unsigned int input,
			      unsigned int *minimum, unsigned int *maximum)
{
	*minimum = max(uds_output_size(input, UDS_MAX_FACTOR), UDS_MIN_SIZE);
	*maximum = min(uds_output_size(input, UDS_MIN_FACTOR), UDS_MAX_SIZE);
}

static unsigned int uds_compute_ratio(unsigned int input, unsigned int output)
{
	/* TODO: This is an approximation that will need to be refined. */
	return (input - 1) * 4096 / (output - 1);
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Core Operations
 */

static int uds_s_stream(struct v4l2_subdev *subdev, int enable)
{
	struct vsp2_uds *uds = to_uds(subdev);
	const struct v4l2_mbus_framefmt *output;
	const struct v4l2_mbus_framefmt *input;
	unsigned int hscale;
	unsigned int vscale;
	bool multitap;
	VSPM_VSP_PAR *vsp_par =
		uds->entity.vsp2->vspm->ip_par.unionIpParam.ptVsp;
	T_VSP_UDS *vsp_uds = vsp_par->ctrl_par->uds;

	if (!enable)
		return 0;

	input = &uds->entity.formats[UDS_PAD_SINK];
	output = &uds->entity.formats[UDS_PAD_SOURCE];

	hscale = uds_compute_ratio(input->width, output->width);
	vscale = uds_compute_ratio(input->height, output->height);

	dev_dbg(uds->entity.vsp2->dev, "hscale %u vscale %u\n", hscale, vscale);

	/* Multi-tap scaling can't be enabled along with alpha scaling.
	 */
	if (uds->scale_alpha)
		multitap = false;
	else
		multitap = true;

	vsp_uds->amd = VSP_AMD_NO;
	vsp_uds->fmd = VSP_FMD;
	vsp_uds->clip = VSP_CLIP_OFF;
	vsp_uds->alpha = uds->scale_alpha ? VSP_ALPHA_ON : VSP_ALPHA_OFF;
	vsp_uds->complement = multitap ? VSP_COMPLEMENT_BC : VSP_COMPLEMENT_BIL;

	/* Set the scaling ratios and the output size. */
	vsp_uds->x_ratio	= hscale;
	vsp_uds->y_ratio	= vscale;
	vsp_uds->out_cwidth	= output->width;
	vsp_uds->out_cheight	= output->height;

	vsp_uds->athres0	= 0;
	vsp_uds->athres1	= 0;
	vsp_uds->filcolor	= 0;

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Pad Operations
 */

static int uds_enum_mbus_code(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_fh *fh,
			      struct v4l2_subdev_mbus_code_enum *code)
{
	static const unsigned int codes[] = {
		V4L2_MBUS_FMT_ARGB8888_1X32,
		V4L2_MBUS_FMT_AYUV8_1X32,
	};

	if (code->pad == UDS_PAD_SINK) {
		if (code->index >= ARRAY_SIZE(codes))
			return -EINVAL;

		code->code = codes[code->index];
	} else {
		struct v4l2_mbus_framefmt *format;

		/* The UDS can't perform format conversion, the sink format is
		 * always identical to the source format.
		 */
		if (code->index)
			return -EINVAL;

		format = v4l2_subdev_get_try_format(fh, UDS_PAD_SINK);
		code->code = format->code;
	}

	return 0;
}

static int uds_enum_frame_size(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_fh *fh,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	struct v4l2_mbus_framefmt *format;

	format = v4l2_subdev_get_try_format(fh, UDS_PAD_SINK);

	if (fse->index || fse->code != format->code)
		return -EINVAL;

	if (fse->pad == UDS_PAD_SINK) {
		fse->min_width = UDS_MIN_SIZE;
		fse->max_width = UDS_MAX_SIZE;
		fse->min_height = UDS_MIN_SIZE;
		fse->max_height = UDS_MAX_SIZE;
	} else {
		uds_output_limits(format->width, &fse->min_width,
				  &fse->max_width);
		uds_output_limits(format->height, &fse->min_height,
				  &fse->max_height);
	}

	return 0;
}

static int uds_get_format(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct vsp2_uds *uds = to_uds(subdev);

	fmt->format = *vsp2_entity_get_pad_format(&uds->entity, fh, fmt->pad,
						  fmt->which);

	return 0;
}

static void uds_try_format(struct vsp2_uds *uds, struct v4l2_subdev_fh *fh,
			   unsigned int pad, struct v4l2_mbus_framefmt *fmt,
			   enum v4l2_subdev_format_whence which)
{
	struct v4l2_mbus_framefmt *format;
	unsigned int minimum;
	unsigned int maximum;

	switch (pad) {
	case UDS_PAD_SINK:
		/* Default to YUV if the requested format is not supported. */
		if (fmt->code != V4L2_MBUS_FMT_ARGB8888_1X32 &&
		    fmt->code != V4L2_MBUS_FMT_AYUV8_1X32)
			fmt->code = V4L2_MBUS_FMT_AYUV8_1X32;

		fmt->width = clamp(fmt->width, UDS_MIN_SIZE, UDS_MAX_SIZE);
		fmt->height = clamp(fmt->height, UDS_MIN_SIZE, UDS_MAX_SIZE);
		break;

	case UDS_PAD_SOURCE:
		/* The UDS scales but can't perform format conversion. */
		format = vsp2_entity_get_pad_format(&uds->entity, fh,
						    UDS_PAD_SINK, which);
		fmt->code = format->code;

		uds_output_limits(format->width, &minimum, &maximum);
		fmt->width = clamp(fmt->width, minimum, maximum);
		uds_output_limits(format->height, &minimum, &maximum);
		fmt->height = clamp(fmt->height, minimum, maximum);
		break;
	}

	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
}

static int uds_set_format(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct vsp2_uds *uds = to_uds(subdev);
	struct v4l2_mbus_framefmt *format;

	uds_try_format(uds, fh, fmt->pad, &fmt->format, fmt->which);

	format = vsp2_entity_get_pad_format(&uds->entity, fh, fmt->pad,
					    fmt->which);
	*format = fmt->format;

	if (fmt->pad == UDS_PAD_SINK) {
		/* Propagate the format to the source pad. */
		format = vsp2_entity_get_pad_format(&uds->entity, fh,
						    UDS_PAD_SOURCE, fmt->which);
		*format = fmt->format;

		uds_try_format(uds, fh, UDS_PAD_SOURCE, format, fmt->which);
	}

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Operations
 */

static struct v4l2_subdev_video_ops uds_video_ops = {
	.s_stream = uds_s_stream,
};

static struct v4l2_subdev_pad_ops uds_pad_ops = {
	.enum_mbus_code = uds_enum_mbus_code,
	.enum_frame_size = uds_enum_frame_size,
	.get_fmt = uds_get_format,
	.set_fmt = uds_set_format,
};

static struct v4l2_subdev_ops uds_ops = {
	.video	= &uds_video_ops,
	.pad    = &uds_pad_ops,
};

/* -----------------------------------------------------------------------------
 * Initialization and Cleanup
 */

struct vsp2_uds *vsp2_uds_create(struct vsp2_device *vsp2, unsigned int index)
{
	struct v4l2_subdev *subdev;
	struct vsp2_uds *uds;
	int ret;

	uds = devm_kzalloc(vsp2->dev, sizeof(*uds), GFP_KERNEL);
	if (uds == NULL)
		return ERR_PTR(-ENOMEM);

	uds->entity.type = VSP2_ENTITY_UDS;
	uds->entity.index = index;

	ret = vsp2_entity_init(vsp2, &uds->entity, 2);
	if (ret < 0)
		return ERR_PTR(ret);

	/* Initialize the V4L2 subdev. */
	subdev = &uds->entity.subdev;
	v4l2_subdev_init(subdev, &uds_ops);

	subdev->entity.ops = &vsp2_media_ops;
	subdev->internal_ops = &vsp2_subdev_internal_ops;
	snprintf(subdev->name, sizeof(subdev->name), "%s uds.%u",
		 dev_name(vsp2->dev), index);
	v4l2_set_subdevdata(subdev, uds);
	subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	vsp2_entity_init_formats(subdev, NULL);

	return uds;
}
