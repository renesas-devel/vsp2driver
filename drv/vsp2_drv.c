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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>

#include "vsp2.h"
#include "vsp2_bru.h"
#include "vsp2_rwpf.h"
#include "vsp2_uds.h"
#include "vsp2_vspm.h"

#define VSP2_PRINT_ALERT(fmt, args...) \
	pr_alert("vsp2:%d: " fmt, current->pid, ##args)

void vsp2_frame_end(struct vsp2_device *vsp2)
{
	unsigned int i;

	for (i = 0; i < VSP2_COUNT_WPF; ++i) {
		struct vsp2_rwpf *wpf = vsp2->wpf[i];
		struct vsp2_pipeline *pipe;

		if (wpf == NULL)
			continue;

		pipe = to_vsp2_pipeline(&wpf->entity.subdev.entity);

		vsp2_pipeline_frame_end(pipe);
	}

	return;
}

/* -----------------------------------------------------------------------------
 * Entities
 */

/*
 * vsp2_create_links - Create links from all sources to the given sink
 *
 * This function creates media links from all valid sources to the given sink
 * pad. Links that would be invalid according to the VSP2 hardware capabilities
 * are skipped. Those include all links
 *
 * - from a UDS to a UDS (UDS entities can't be chained)
 * - from an entity to itself (no loops are allowed)
 */
static int vsp2_create_links(struct vsp2_device *vsp2, struct vsp2_entity *sink)
{
	struct media_entity *entity = &sink->subdev.entity;
	struct vsp2_entity *source;
	unsigned int pad;
	int ret;

	list_for_each_entry(source, &vsp2->entities, list_dev) {
		u32 flags;

		if (source->type == sink->type)
			continue;

		if (source->type == VSP2_ENTITY_WPF)
			continue;

		flags = source->type == VSP2_ENTITY_RPF &&
			sink->type == VSP2_ENTITY_WPF &&
			source->index == sink->index
		      ? MEDIA_LNK_FL_ENABLED : 0;

		for (pad = 0; pad < entity->num_pads; ++pad) {
			if (!(entity->pads[pad].flags & MEDIA_PAD_FL_SINK))
				continue;

			ret = media_entity_create_link(&source->subdev.entity,
						       source->source_pad,
						       entity, pad, flags);
			if (ret < 0)
				return ret;

			if (flags & MEDIA_LNK_FL_ENABLED)
				source->sink = entity;
		}
	}

	return 0;
}

static void vsp2_destroy_entities(struct vsp2_device *vsp2)
{
	struct vsp2_entity *entity;
	struct vsp2_entity *next;

	list_for_each_entry_safe(entity, next, &vsp2->entities, list_dev) {
		list_del(&entity->list_dev);
		vsp2_entity_destroy(entity);
	}

	v4l2_device_unregister(&vsp2->v4l2_dev);
	media_device_unregister(&vsp2->media_dev);
}

static int vsp2_create_entities(struct vsp2_device *vsp2)
{
	struct media_device *mdev = &vsp2->media_dev;
	struct v4l2_device *vdev = &vsp2->v4l2_dev;
	struct vsp2_entity *entity;
	unsigned int i;
	int ret;

	mdev->dev = vsp2->dev;
	strlcpy(mdev->model, "VSP2", sizeof(mdev->model));
	snprintf(mdev->bus_info, sizeof(mdev->bus_info), "platform:%s",
		 dev_name(mdev->dev));
	ret = media_device_register(mdev);
	if (ret < 0) {
		dev_err(vsp2->dev, "media device registration failed (%d)\n",
			ret);
		return ret;
	}

	vdev->mdev = mdev;
	ret = v4l2_device_register(vsp2->dev, vdev);
	if (ret < 0) {
		dev_err(vsp2->dev, "V4L2 device registration failed (%d)\n",
			ret);
		goto done;
	}

	/* Instantiate all the entities. */
	vsp2->bru = vsp2_bru_create(vsp2);
	if (IS_ERR(vsp2->bru)) {
		ret = PTR_ERR(vsp2->bru);
		goto done;
	}

	list_add_tail(&vsp2->bru->entity.list_dev, &vsp2->entities);

	for (i = 0; i < VSP2_COUNT_RPF; ++i) {
		struct vsp2_rwpf *rpf;

		rpf = vsp2_rpf_create(vsp2, i);
		if (IS_ERR(rpf)) {
			ret = PTR_ERR(rpf);
			goto done;
		}

		vsp2->rpf[i] = rpf;
		list_add_tail(&rpf->entity.list_dev, &vsp2->entities);
	}

	for (i = 0; i < VSP2_COUNT_UDS; ++i) {
		struct vsp2_uds *uds;

		uds = vsp2_uds_create(vsp2, i);
		if (IS_ERR(uds)) {
			ret = PTR_ERR(uds);
			goto done;
		}

		vsp2->uds[i] = uds;
		list_add_tail(&uds->entity.list_dev, &vsp2->entities);
	}

	for (i = 0; i < VSP2_COUNT_WPF; ++i) {
		struct vsp2_rwpf *wpf;

		wpf = vsp2_wpf_create(vsp2, i);
		if (IS_ERR(wpf)) {
			ret = PTR_ERR(wpf);
			goto done;
		}

		vsp2->wpf[i] = wpf;
		list_add_tail(&wpf->entity.list_dev, &vsp2->entities);
	}

	/* Create links. */
	list_for_each_entry(entity, &vsp2->entities, list_dev) {
		if (entity->type == VSP2_ENTITY_RPF)
			continue;

		ret = vsp2_create_links(vsp2, entity);
		if (ret < 0)
			goto done;
	}

	/* Register all subdevs. */
	list_for_each_entry(entity, &vsp2->entities, list_dev) {
		ret = v4l2_device_register_subdev(&vsp2->v4l2_dev,
						  &entity->subdev);
		if (ret < 0)
			goto done;
	}

	ret = v4l2_device_register_subdev_nodes(&vsp2->v4l2_dev);

done:
	if (ret < 0)
		vsp2_destroy_entities(vsp2);

	return ret;
}

static int vsp2_device_init(struct vsp2_device *vsp2)
{
	long vspm_ret = R_VSPM_OK;

	/* Initialize the VSPM driver */
	vspm_ret = vsp2_vspm_drv_init(vsp2);
	if (vspm_ret != R_VSPM_OK) {
		dev_err(vsp2->dev,
			"failed to initialize the VSPM driver : %ld\n",
			vspm_ret);
		return -EFAULT;
	}

	return 0;
}

/*
 * vsp2_device_get - Acquire the VSP2 device
 *
 * Increment the VSP2 reference count and initialize the device if the first
 * reference is taken.
 *
 * Return 0 on success or a negative error code otherwise.
 */
int vsp2_device_get(struct vsp2_device *vsp2)
{
	int ret = 0;

	mutex_lock(&vsp2->lock);
	if (vsp2->ref_count > 0)
		goto done;

	ret = vsp2_device_init(vsp2);
	if (ret < 0)
		goto done;

done:
	if (!ret)
		vsp2->ref_count++;

	mutex_unlock(&vsp2->lock);
	return ret;
}

/*
 * vsp2_device_put - Release the VSP2 device
 *
 * Decrement the VSP2 reference count and cleanup the device if the last
 * reference is released.
 */
void vsp2_device_put(struct vsp2_device *vsp2)
{
	long vspm_ret = R_VSPM_OK;

	mutex_lock(&vsp2->lock);

	if (--vsp2->ref_count == 0) {
		vspm_ret = vsp2_vspm_drv_quit(vsp2);
		if (vspm_ret != R_VSPM_OK)
			dev_err(vsp2->dev,
				"failed to exit the VSPM driver : %ld\n",
				vspm_ret);
	}

	mutex_unlock(&vsp2->lock);
}

/* -----------------------------------------------------------------------------
 * Power Management
 */

#ifdef CONFIG_PM_SLEEP
static int vsp2_pm_suspend(struct device *dev)
{
	struct vsp2_device *vsp2 = dev_get_drvdata(dev);
	unsigned int i = 0;
	int ret;

	WARN_ON(mutex_is_locked(&vsp2->lock));

	if (vsp2->ref_count == 0)
		return 0;

	/* Suspend pipeline */
	for (i = 0; i < VSP2_COUNT_WPF; ++i) {
		struct vsp2_rwpf *wpf = vsp2->wpf[i];
		struct vsp2_pipeline *pipe;

		if (wpf == NULL)
			continue;

		pipe = to_vsp2_pipeline(&wpf->entity.subdev.entity);
		ret = vsp2_pipeline_suspend(pipe);
		if (ret < 0)
			break;
	}

	return ret;
}

static int vsp2_pm_resume(struct device *dev)
{
	struct vsp2_device *vsp2 = dev_get_drvdata(dev);
	unsigned int i = 0;

	WARN_ON(mutex_is_locked(&vsp2->lock));

	if (vsp2->ref_count == 0)
		return 0;

	/* Resume pipeline */
	for (i = 0; i < VSP2_COUNT_WPF; ++i) {
		struct vsp2_rwpf *wpf = vsp2->wpf[i];
		struct vsp2_pipeline *pipe;

		if (wpf == NULL)
			continue;

		pipe = to_vsp2_pipeline(&wpf->entity.subdev.entity);
		vsp2_pipeline_resume(pipe);
	}

	return 0;
}
#endif

static const struct dev_pm_ops vsp2_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(vsp2_pm_suspend, vsp2_pm_resume)
};

static int vsp2_probe(struct platform_device *pdev)
{
	struct vsp2_device *vsp2;
	int ret;

	vsp2 = devm_kzalloc(&pdev->dev, sizeof(*vsp2), GFP_KERNEL);
	if (vsp2 == NULL)
		return -ENOMEM;

	vsp2->dev = &pdev->dev;
	mutex_init(&vsp2->lock);
	INIT_LIST_HEAD(&vsp2->entities);

	ret = vsp2_vspm_init(vsp2, pdev->id);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to initialize VSPM info\n");
		return ret;
	}

	/* Instanciate entities */
	ret = vsp2_create_entities(vsp2);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to create entities\n");
		return ret;
	}

	platform_set_drvdata(pdev, vsp2);

	return 0;
}

static int vsp2_remove(struct platform_device *pdev)
{
	struct vsp2_device *vsp2 = platform_get_drvdata(pdev);

	vsp2_destroy_entities(vsp2);

	return 0;
}

static struct platform_driver vsp2_driver = {
	.probe		= vsp2_probe,
	.remove		= vsp2_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= DRVNAME,
		.pm	= &vsp2_pm_ops,
	},
};

static void vsp2_dev_release(struct device *dev)
{
	return;
}

static struct platform_device vsp2_devices[] = {
	{
		.name		= DEVNAME,
		.id		= DEVID_0,
		.dev		= {
					.coherent_dma_mask = ~0,
					.release = vsp2_dev_release,
				},
	},
	{
		.name		= DEVNAME,
		.id		= DEVID_1,
		.dev		= {
					.coherent_dma_mask = ~0,
					.release = vsp2_dev_release,
				},
	},
};

static int __init vsp2_init(void)
{
	int ercd = 0;
	unsigned int i = 0;
	unsigned int unreg_num = 0;

	for (i = 0; i < ARRAY_SIZE(vsp2_devices); i++) {
		ercd = platform_device_register(&vsp2_devices[i]);
		if (ercd) {
			VSP2_PRINT_ALERT(
			    "failed to add a platform-level device : %s.%d\n",
			    vsp2_devices[i].name,  vsp2_devices[i].id);
			goto err_exit;
		}
	}

	ercd = platform_driver_register(&vsp2_driver);
	if (ercd) {
		VSP2_PRINT_ALERT(
		  "failed to register a driver for platform-level devices.\n");
		goto err_exit;
	}

	return ercd;

err_exit:
	unreg_num = i;
	for (i = 0; i < unreg_num; i++)
		platform_device_unregister(&vsp2_devices[i]);

	return ercd;
}

static void __exit vsp2_exit(void)
{
	unsigned int i = 0;

	platform_driver_unregister(&vsp2_driver);

	for (i = 0; i < ARRAY_SIZE(vsp2_devices); i++)
		platform_device_unregister(&vsp2_devices[i]);
}

module_init(vsp2_init);
module_exit(vsp2_exit);

MODULE_ALIAS("vsp2");
MODULE_AUTHOR("Renesas Electronics Corporation");
MODULE_DESCRIPTION("Renesas VSP2 Driver");
MODULE_LICENSE("Dual MIT/GPL");
