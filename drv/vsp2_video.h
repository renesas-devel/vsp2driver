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

#ifndef __VSP2_VIDEO_H__
#define __VSP2_VIDEO_H__

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <media/media-entity.h>
#include <media/videobuf2-core.h>

struct vsp2_video;

/*
 * struct vsp2_format_info - VSP2 video format description
 * @mbus: media bus format code
 * @fourcc: V4L2 pixel format FCC identifier
 * @planes: number of planes
 * @bpp: bits per pixel
 * @hwfmt: VSP2 hardware format
 * @swap_yc: the Y and C components are swapped (Y comes before C)
 * @swap_uv: the U and V components are swapped (V comes before U)
 * @hsub: horizontal subsampling factor
 * @vsub: vertical subsampling factor
 * @alpha: has an alpha channel
 */
struct vsp2_format_info {
	u32 fourcc;
	unsigned int mbus;
	unsigned int hwfmt;
	unsigned int swap;
	unsigned int planes;
	unsigned int bpp[3];
	bool swap_yc;
	bool swap_uv;
	unsigned int hsub;
	unsigned int vsub;
	bool alpha;
};

enum vsp2_pipeline_state {
	VSP2_PIPELINE_STOPPED,
	VSP2_PIPELINE_RUNNING,
	VSP2_PIPELINE_STOPPING,
};

/*
 * struct vsp2_pipeline - A VSP2 hardware pipeline
 * @media: the media pipeline
 * @irqlock: protects the pipeline state
 * @lock: protects the pipeline use count and stream count
 */
struct vsp2_pipeline {
	struct media_pipeline pipe;

	spinlock_t irqlock;
	enum vsp2_pipeline_state state;
	wait_queue_head_t wq;

	struct mutex lock;
	unsigned int use_count;
	unsigned int stream_count;
	unsigned int buffers_ready;

	unsigned int num_video;
	unsigned int num_inputs;
	struct vsp2_rwpf *inputs[VSP2_COUNT_RPF];
	struct vsp2_rwpf *output;
	struct vsp2_entity *bru;
	struct vsp2_entity *uds;
	struct vsp2_entity *uds_input;

	struct list_head entities;
};

static inline struct vsp2_pipeline *to_vsp2_pipeline(struct media_entity *e)
{
	if (likely(e->pipe))
		return container_of(e->pipe, struct vsp2_pipeline, pipe);
	else
		return NULL;
}

struct vsp2_video_buffer {
	struct vb2_buffer buf;
	struct list_head queue;
	struct vsp2_video *video;

	dma_addr_t addr[3];
	unsigned int length[3];

};

static inline struct vsp2_video_buffer *
to_vsp2_video_buffer(struct vb2_buffer *vb)
{
	return container_of(vb, struct vsp2_video_buffer, buf);
}

struct vsp2_video_operations {
	void (*queue)(struct vsp2_video *video, struct vsp2_video_buffer *buf);
};

struct vsp2_video {
	struct vsp2_device *vsp2;
	struct vsp2_entity *rwpf;

	const struct vsp2_video_operations *ops;

	struct video_device video;
	enum v4l2_buf_type type;
	struct media_pad pad;

	struct mutex lock;
	struct v4l2_pix_format_mplane format;
	const struct vsp2_format_info *fmtinfo;

	struct vsp2_pipeline pipe;
	unsigned int pipe_index;

	struct vb2_queue queue;
	void *alloc_ctx;
	spinlock_t irqlock;
	struct list_head irqqueue;
	unsigned int sequence;
};

static inline struct vsp2_video *to_vsp2_video(struct video_device *vdev)
{
	return container_of(vdev, struct vsp2_video, video);
}

int vsp2_video_init(struct vsp2_video *video, struct vsp2_entity *rwpf);
void vsp2_video_cleanup(struct vsp2_video *video);

void vsp2_pipeline_frame_end(struct vsp2_pipeline *pipe);

void vsp2_pipeline_propagate_alpha(struct vsp2_pipeline *pipe,
				   struct vsp2_entity *input,
				   unsigned int alpha);

#endif /* __VSP2_VIDEO_H__ */
