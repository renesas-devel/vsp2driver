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

#ifndef __VSP2_RWPF_H__
#define __VSP2_RWPF_H__

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>

#include "vsp2.h"
#include "vsp2_entity.h"
#include "vsp2_video.h"

#define RWPF_PAD_SINK				0
#define RWPF_PAD_SOURCE				1

struct vsp2_rwpf {
	struct vsp2_entity entity;
	struct vsp2_video video;
	struct v4l2_ctrl_handler ctrls;

	unsigned int max_width;
	unsigned int max_height;

	struct {
		unsigned int left;
		unsigned int top;
	} location;
	struct v4l2_rect crop;

	unsigned int offsets[2];
	dma_addr_t buf_addr[3];
};

static inline struct vsp2_rwpf *to_rwpf(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct vsp2_rwpf, entity.subdev);
}

struct vsp2_rwpf *vsp2_rpf_create(struct vsp2_device *vsp2, unsigned int index);
struct vsp2_rwpf *vsp2_wpf_create(struct vsp2_device *vsp2, unsigned int index);

int vsp2_rwpf_enum_mbus_code(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_fh *fh,
			     struct v4l2_subdev_mbus_code_enum *code);
int vsp2_rwpf_enum_frame_size(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_fh *fh,
			      struct v4l2_subdev_frame_size_enum *fse);
int vsp2_rwpf_get_format(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh,
			 struct v4l2_subdev_format *fmt);
int vsp2_rwpf_set_format(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh,
			 struct v4l2_subdev_format *fmt);
int vsp2_rwpf_get_selection(struct v4l2_subdev *subdev,
			    struct v4l2_subdev_fh *fh,
			    struct v4l2_subdev_selection *sel);
int vsp2_rwpf_set_selection(struct v4l2_subdev *subdev,
			    struct v4l2_subdev_fh *fh,
			    struct v4l2_subdev_selection *sel);

#endif /* __VSP2_RWPF_H__ */
