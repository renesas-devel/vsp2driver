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

#ifndef __VSP2_ENTITY_H__
#define __VSP2_ENTITY_H__

#include <linux/list.h>
#include <linux/spinlock.h>

#include <media/v4l2-subdev.h>

struct vsp2_device;
struct vsp2_video;

enum vsp2_entity_type {
	VSP2_ENTITY_BRU,
	VSP2_ENTITY_RPF,
	VSP2_ENTITY_UDS,
	VSP2_ENTITY_WPF,
};

/*
 * struct vsp2_route - Entity routing configuration
 * @type: Entity type this routing entry is associated with
 * @index: Entity index this routing entry is associated with
 * @reg: Output routing configuration register
 * @inputs: Target node value for each input
 *
 * Each $vsp2_route entry describes routing configuration for the entity
 * specified by the entry's @type and @index. @reg indicates the register that
 * holds output routing configuration for the entity, and the @inputs array
 * store the target node value for each input of the entity.
 */
struct vsp2_route {
	enum vsp2_entity_type type;
	unsigned int index;
	unsigned int reg;
	unsigned int inputs[4];
};

struct vsp2_entity {
	struct vsp2_device *vsp2;

	enum vsp2_entity_type type;
	unsigned int index;
	const struct vsp2_route *route;

	struct list_head list_dev;
	struct list_head list_pipe;

	struct media_pad *pads;
	unsigned int source_pad;

	struct media_entity *sink;
	unsigned int sink_pad;

	struct v4l2_subdev subdev;
	struct v4l2_mbus_framefmt *formats;

	struct vsp2_video *video;

	spinlock_t lock;		/* Protects the streaming field */
	bool streaming;
};

static inline struct vsp2_entity *to_vsp2_entity(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct vsp2_entity, subdev);
}

int vsp2_entity_init(struct vsp2_device *vsp2, struct vsp2_entity *entity,
		     unsigned int num_pads);
void vsp2_entity_destroy(struct vsp2_entity *entity);

extern const struct v4l2_subdev_internal_ops vsp2_subdev_internal_ops;
extern const struct media_entity_operations vsp2_media_ops;

struct v4l2_mbus_framefmt *
vsp2_entity_get_pad_format(struct vsp2_entity *entity,
			   struct v4l2_subdev_fh *fh,
			   unsigned int pad, u32 which);
void vsp2_entity_init_formats(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_fh *fh);

bool vsp2_entity_is_streaming(struct vsp2_entity *entity);
int vsp2_entity_set_streaming(struct vsp2_entity *entity, bool streaming);

#endif /* __VSP2_ENTITY_H__ */
