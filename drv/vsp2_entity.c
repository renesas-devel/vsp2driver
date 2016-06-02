/*************************************************************************/ /*
 VSP2

 Copyright (C) 2015-2016 Renesas Electronics Corporation

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

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>

#include "vsp2.h"
#include "vsp2_entity.h"
#include "vsp2_video.h"

bool vsp2_entity_is_streaming(struct vsp2_entity *entity)
{
	unsigned long flags;
	bool streaming;

	spin_lock_irqsave(&entity->lock, flags);
	streaming = entity->streaming;
	spin_unlock_irqrestore(&entity->lock, flags);

	return streaming;
}

int vsp2_entity_set_streaming(struct vsp2_entity *entity, bool streaming)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&entity->lock, flags);
	entity->streaming = streaming;
	spin_unlock_irqrestore(&entity->lock, flags);

	if (!streaming)
		return 0;

	if (!entity->subdev.ctrl_handler)
		return 0;

	ret = v4l2_ctrl_handler_setup(entity->subdev.ctrl_handler);
	if (ret < 0) {
		spin_lock_irqsave(&entity->lock, flags);
		entity->streaming = false;
		spin_unlock_irqrestore(&entity->lock, flags);
	}

	return ret;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Operations
 */

struct v4l2_mbus_framefmt *
vsp2_entity_get_pad_format(struct vsp2_entity *entity,
			   struct v4l2_subdev_fh *fh,
			   unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &entity->formats[pad];
	default:
		return NULL;
	}
}

/*
 * vsp2_entity_init_formats - Initialize formats on all pads
 * @subdev: V4L2 subdevice
 * @fh: V4L2 subdev file handle
 *
 * Initialize all pad formats with default values. If fh is not NULL, try
 * formats are initialized on the file handle. Otherwise active formats are
 * initialized on the device.
 */
void vsp2_entity_init_formats(struct v4l2_subdev *subdev,
			    struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;
	unsigned int pad;

	for (pad = 0; pad < subdev->entity.num_pads - 1; ++pad) {
		memset(&format, 0, sizeof(format));

		format.pad = pad;
		format.which = fh ? V4L2_SUBDEV_FORMAT_TRY
			     : V4L2_SUBDEV_FORMAT_ACTIVE;

		v4l2_subdev_call(subdev, pad, set_fmt, fh, &format);
	}
}

static int vsp2_entity_open(struct v4l2_subdev *subdev,
			    struct v4l2_subdev_fh *fh)
{
	vsp2_entity_init_formats(subdev, fh);

	return 0;
}

const struct v4l2_subdev_internal_ops vsp2_subdev_internal_ops = {
	.open = vsp2_entity_open,
};

/* -----------------------------------------------------------------------------
 * Media Operations
 */

static int vsp2_entity_link_setup(struct media_entity *entity,
				  const struct media_pad *local,
				  const struct media_pad *remote, u32 flags)
{
	struct vsp2_entity *source;

	if (!(local->flags & MEDIA_PAD_FL_SOURCE))
		return 0;

	source = container_of(local->entity, struct vsp2_entity, subdev.entity);

	if (!source->route)
		return 0;

	if (flags & MEDIA_LNK_FL_ENABLED) {
		if (source->sink)
			return -EBUSY;
		source->sink = remote->entity;
		source->sink_pad = remote->index;
	} else {
		source->sink = NULL;
		source->sink_pad = 0;
	}

	return 0;
}

const struct media_entity_operations vsp2_media_ops = {
	.link_setup = vsp2_entity_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

/* -----------------------------------------------------------------------------
 * Initialization
 */

static const struct vsp2_route vsp2_routes[] = {
	{ VSP2_ENTITY_BRU, 0, VI6_DPR_BRU_ROUTE,
	  { VI6_DPR_NODE_BRU_IN(0), VI6_DPR_NODE_BRU_IN(1),
	    VI6_DPR_NODE_BRU_IN(2), VI6_DPR_NODE_BRU_IN(3), } },
	{ VSP2_ENTITY_RPF, 0, VI6_DPR_RPF_ROUTE(0), { VI6_DPR_NODE_RPF(0), } },
	{ VSP2_ENTITY_RPF, 1, VI6_DPR_RPF_ROUTE(1), { VI6_DPR_NODE_RPF(1), } },
	{ VSP2_ENTITY_RPF, 2, VI6_DPR_RPF_ROUTE(2), { VI6_DPR_NODE_RPF(2), } },
	{ VSP2_ENTITY_RPF, 3, VI6_DPR_RPF_ROUTE(3), { VI6_DPR_NODE_RPF(3), } },
	{ VSP2_ENTITY_UDS, 0, VI6_DPR_UDS_ROUTE(0), { VI6_DPR_NODE_UDS(0), } },
	{ VSP2_ENTITY_WPF, 0, 0, { VI6_DPR_NODE_WPF(0), } },
};

int vsp2_entity_init(struct vsp2_device *vsp2, struct vsp2_entity *entity,
		     unsigned int num_pads)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(vsp2_routes); ++i) {
		if (vsp2_routes[i].type == entity->type &&
		    vsp2_routes[i].index == entity->index) {
			entity->route = &vsp2_routes[i];
			break;
		}
	}

	if (i == ARRAY_SIZE(vsp2_routes))
		return -EINVAL;

	spin_lock_init(&entity->lock);

	entity->vsp2 = vsp2;
	entity->source_pad = num_pads - 1;

	/* Allocate formats and pads. */
	entity->formats = devm_kzalloc(vsp2->dev,
				       num_pads * sizeof(*entity->formats),
				       GFP_KERNEL);
	if (entity->formats == NULL)
		return -ENOMEM;

	entity->pads = devm_kzalloc(vsp2->dev, num_pads * sizeof(*entity->pads),
				    GFP_KERNEL);
	if (entity->pads == NULL)
		return -ENOMEM;

	/* Initialize pads. */
	for (i = 0; i < num_pads - 1; ++i)
		entity->pads[i].flags = MEDIA_PAD_FL_SINK;

	entity->pads[num_pads - 1].flags = MEDIA_PAD_FL_SOURCE;

	/* Initialize the media entity. */
	return media_entity_init(&entity->subdev.entity, num_pads,
				 entity->pads, 0);
}

void vsp2_entity_destroy(struct vsp2_entity *entity)
{
	if (entity->video)
		vsp2_video_cleanup(entity->video);
	if (entity->subdev.ctrl_handler)
		v4l2_ctrl_handler_free(entity->subdev.ctrl_handler);
	media_entity_cleanup(&entity->subdev.entity);
}
