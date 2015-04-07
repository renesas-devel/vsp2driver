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

#include <media/v4l2-subdev.h>

#include "vsp2.h"
#include "vsp2_rwpf.h"
#include "vsp2_video.h"

#define RWPF_MIN_WIDTH				1
#define RWPF_MIN_HEIGHT				1

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Pad Operations
 */

int vsp2_rwpf_enum_mbus_code(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_fh *fh,
			     struct v4l2_subdev_mbus_code_enum *code)
{
	static const unsigned int codes[] = {
		V4L2_MBUS_FMT_ARGB8888_1X32,
		V4L2_MBUS_FMT_AYUV8_1X32,
	};

	if (code->index >= ARRAY_SIZE(codes))
		return -EINVAL;

	code->code = codes[code->index];

	return 0;
}

int vsp2_rwpf_enum_frame_size(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_fh *fh,
			      struct v4l2_subdev_frame_size_enum *fse)
{
	struct vsp2_rwpf *rwpf = to_rwpf(subdev);
	struct v4l2_mbus_framefmt *format;

	format = v4l2_subdev_get_try_format(fh, fse->pad);

	if (fse->index || fse->code != format->code)
		return -EINVAL;

	if (fse->pad == RWPF_PAD_SINK) {
		fse->min_width = RWPF_MIN_WIDTH;
		fse->max_width = rwpf->max_width;
		fse->min_height = RWPF_MIN_HEIGHT;
		fse->max_height = rwpf->max_height;
	} else {
		/* The size on the source pad are fixed and always identical to
		 * the size on the sink pad.
		 */
		fse->min_width = format->width;
		fse->max_width = format->width;
		fse->min_height = format->height;
		fse->max_height = format->height;
	}

	return 0;
}

static struct v4l2_rect *
vsp2_rwpf_get_crop(struct vsp2_rwpf *rwpf, struct v4l2_subdev_fh *fh, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_crop(fh, RWPF_PAD_SINK);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &rwpf->crop;
	default:
		return NULL;
	}
}

int vsp2_rwpf_get_format(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh,
			 struct v4l2_subdev_format *fmt)
{
	struct vsp2_rwpf *rwpf = to_rwpf(subdev);

	fmt->format = *vsp2_entity_get_pad_format(&rwpf->entity, fh, fmt->pad,
						  fmt->which);

	return 0;
}

int vsp2_rwpf_set_format(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh,
			 struct v4l2_subdev_format *fmt)
{
	struct vsp2_rwpf *rwpf = to_rwpf(subdev);
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *crop;

	/* Default to YUV if the requested format is not supported. */
	if (fmt->format.code != V4L2_MBUS_FMT_ARGB8888_1X32 &&
	    fmt->format.code != V4L2_MBUS_FMT_AYUV8_1X32)
		fmt->format.code = V4L2_MBUS_FMT_AYUV8_1X32;

	format = vsp2_entity_get_pad_format(&rwpf->entity, fh, fmt->pad,
					    fmt->which);

	if (fmt->pad == RWPF_PAD_SOURCE) {
		/* The RWPF performs format conversion but can't scale, only the
		 * format code can be changed on the source pad.
		 */
		format->code = fmt->format.code;
		fmt->format = *format;
		return 0;
	}

	format->code = fmt->format.code;
	format->width = clamp_t(unsigned int, fmt->format.width,
				RWPF_MIN_WIDTH, rwpf->max_width);
	format->height = clamp_t(unsigned int, fmt->format.height,
				 RWPF_MIN_HEIGHT, rwpf->max_height);
	format->field = V4L2_FIELD_NONE;
	format->colorspace = V4L2_COLORSPACE_SRGB;

	fmt->format = *format;

	/* Update the sink crop rectangle. */
	crop = vsp2_rwpf_get_crop(rwpf, fh, fmt->which);
	crop->left = 0;
	crop->top = 0;
	crop->width = fmt->format.width;
	crop->height = fmt->format.height;

	/* Propagate the format to the source pad. */
	format = vsp2_entity_get_pad_format(&rwpf->entity, fh, RWPF_PAD_SOURCE,
					    fmt->which);
	*format = fmt->format;

	return 0;
}

int vsp2_rwpf_get_selection(struct v4l2_subdev *subdev,
			    struct v4l2_subdev_fh *fh,
			    struct v4l2_subdev_selection *sel)
{
	struct vsp2_rwpf *rwpf = to_rwpf(subdev);
	struct v4l2_mbus_framefmt *format;

	/* Cropping is implemented on the sink pad. */
	if (sel->pad != RWPF_PAD_SINK)
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = *vsp2_rwpf_get_crop(rwpf, fh, sel->which);
		break;

	case V4L2_SEL_TGT_CROP_BOUNDS:
		format = vsp2_entity_get_pad_format(&rwpf->entity, fh,
						    RWPF_PAD_SINK, sel->which);
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = format->width;
		sel->r.height = format->height;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

int vsp2_rwpf_set_selection(struct v4l2_subdev *subdev,
			    struct v4l2_subdev_fh *fh,
			    struct v4l2_subdev_selection *sel)
{
	struct vsp2_rwpf *rwpf = to_rwpf(subdev);
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *crop;

	/* Cropping is implemented on the sink pad. */
	if (sel->pad != RWPF_PAD_SINK)
		return -EINVAL;

	if (sel->target != V4L2_SEL_TGT_CROP)
		return -EINVAL;

	/* Make sure the crop rectangle is entirely contained in the image. The
	 * WPF top and left offsets are limited to 255.
	 */
	format = vsp2_entity_get_pad_format(&rwpf->entity, fh, RWPF_PAD_SINK,
					    sel->which);

	if (format->code == V4L2_MBUS_FMT_AYUV8_1X32) {
		sel->r.left = (sel->r.left + 1) & ~1;
		sel->r.top = (sel->r.top + 1) & ~1;
		sel->r.width = (sel->r.width) & ~1;
		sel->r.height = (sel->r.height) & ~1;
	}

	sel->r.left = min_t(unsigned int, sel->r.left, format->width - 2);
	sel->r.top = min_t(unsigned int, sel->r.top, format->height - 2);
	if (rwpf->entity.type == VSP2_ENTITY_WPF) {
		int maxcrop =
			format->code == V4L2_MBUS_FMT_AYUV8_1X32 ? 254 : 255;
		sel->r.left = min_t(unsigned int, sel->r.left, maxcrop);
		sel->r.top = min_t(unsigned int, sel->r.top, maxcrop);
	}
	sel->r.width = min_t(unsigned int, sel->r.width,
			     format->width - sel->r.left);
	sel->r.height = min_t(unsigned int, sel->r.height,
			      format->height - sel->r.top);

	crop = vsp2_rwpf_get_crop(rwpf, fh, sel->which);
	*crop = sel->r;

	/* Propagate the format to the source pad. */
	format = vsp2_entity_get_pad_format(&rwpf->entity, fh, RWPF_PAD_SOURCE,
					    sel->which);
	format->width = crop->width;
	format->height = crop->height;

	return 0;
}
