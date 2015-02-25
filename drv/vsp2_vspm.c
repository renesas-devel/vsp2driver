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

#include "vsp2.h"
#include "vsp2_vspm.h"

void vsp2_vspm_param_init(VSPM_IP_PAR *par)
{
	VSPM_VSP_PAR *vsp_par = par->unionIpParam.ptVsp;
	void *temp_vp;
	int i;

	/* Initialize VSPM_IP_PAR. */
	par->uhType = VSPM_TYPE_VSP_VSPS;

	/* Initialize VSPM_VSP_PAR. */
	vsp_par->rpf_num	= 0;
	vsp_par->rpf_order	= 0;
	vsp_par->use_module	= 0;

	for (i = 0; i < 4; i++) {
		T_VSP_IN *vsp_in;

		switch (i) {
		case 0:
			vsp_in = vsp_par->src1_par;
			break;
		case 1:
			vsp_in = vsp_par->src2_par;
			break;
		case 2:
			vsp_in = vsp_par->src3_par;
			break;
		case 3:
			vsp_in = vsp_par->src4_par;
			break;
		default:
			/* Invalid index. */
			break;
		}

		/* Initialize T_VSP_IN. */
		temp_vp = vsp_in->alpha_blend;
		memset(vsp_in, 0x00, sizeof(T_VSP_IN));
		vsp_in->alpha_blend = temp_vp;

		/* Initialize T_VSP_ALPHA. */
		memset(vsp_in->alpha_blend, 0x00, sizeof(T_VSP_ALPHA));
	}

	/* Initialize T_VSP_OUT. */
	memset(vsp_par->dst_par, 0x00, sizeof(T_VSP_OUT));

	/* Initialize T_VSP_UDS. */
	memset(vsp_par->ctrl_par->uds, 0x00, sizeof(T_VSP_OUT));

	/* Initialize T_VSP_BRU. */
	vsp_par->ctrl_par->bru->lay_order	= 0;
	vsp_par->ctrl_par->bru->adiv		= 0;
	memset(vsp_par->ctrl_par->bru->qnt,
		0x00,
		sizeof(vsp_par->ctrl_par->bru->qnt));
	memset(vsp_par->ctrl_par->bru->dith,
		0x00,
		sizeof(vsp_par->ctrl_par->bru->dith));
	vsp_par->ctrl_par->bru->blend_rop	= NULL;
	vsp_par->ctrl_par->bru->connect		= 0;

	/* Initialize T_VSP_BLEND_VIRTUAL. */
	memset(vsp_par->ctrl_par->bru->blend_virtual,
		0x00,
		sizeof(T_VSP_BLEND_VIRTUAL));

	for (i = 0; i < 4; i++) {
		T_VSP_BLEND_CONTROL *vsp_blend_ctrl;

		switch (i) {
		case 0:
			vsp_blend_ctrl =
				vsp_par->ctrl_par->bru->blend_control_a;
			break;
		case 1:
			vsp_blend_ctrl =
				vsp_par->ctrl_par->bru->blend_control_b;
			break;
		case 2:
			vsp_blend_ctrl =
				vsp_par->ctrl_par->bru->blend_control_c;
			break;
		case 3:
			vsp_blend_ctrl =
				vsp_par->ctrl_par->bru->blend_control_d;
			break;
		default:
			/* Invalid index. */
			break;
		}

		/* Initialize T_VSP_BLEND_CONTROL. */
		memset(vsp_blend_ctrl, 0x00, sizeof(T_VSP_BLEND_CONTROL));
	}

	return;
}

static int vsp2_vspm_alloc_vsp_in(struct device *dev, T_VSP_IN **in)
{
	T_VSP_IN *vsp_in = NULL;

	vsp_in = devm_kzalloc(dev, sizeof(*vsp_in), GFP_KERNEL);
	if (vsp_in == NULL) {
		*in = NULL;
		return -ENOMEM;
	}

	*in = vsp_in;

	vsp_in->alpha_blend =
	  devm_kzalloc(dev, sizeof(*vsp_in->alpha_blend), GFP_KERNEL);
	if (vsp_in->alpha_blend == NULL)
		return -ENOMEM;

	return 0;
}

static int vsp2_vspm_alloc_vsp_bru(struct device *dev, T_VSP_BRU **bru)
{
	T_VSP_BRU *vsp_bru = NULL;

	vsp_bru = devm_kzalloc(dev, sizeof(*vsp_bru), GFP_KERNEL);
	if (vsp_bru == NULL) {
		*bru = NULL;
		return -ENOMEM;
	}

	*bru = vsp_bru;

	vsp_bru->blend_virtual =
	  devm_kzalloc(dev, sizeof(*vsp_bru->blend_virtual), GFP_KERNEL);
	if (vsp_bru->blend_virtual == NULL)
		return -ENOMEM;

	vsp_bru->blend_control_a =
	  devm_kzalloc(dev, sizeof(*vsp_bru->blend_control_a), GFP_KERNEL);
	if (vsp_bru->blend_control_a == NULL)
		return -ENOMEM;
	vsp_bru->blend_control_b =
	  devm_kzalloc(dev, sizeof(*vsp_bru->blend_control_b), GFP_KERNEL);
	if (vsp_bru->blend_control_b == NULL)
		return -ENOMEM;
	vsp_bru->blend_control_c =
	  devm_kzalloc(dev, sizeof(*vsp_bru->blend_control_c), GFP_KERNEL);
	if (vsp_bru->blend_control_c == NULL)
		return -ENOMEM;
	vsp_bru->blend_control_d =
	  devm_kzalloc(dev, sizeof(*vsp_bru->blend_control_d), GFP_KERNEL);
	if (vsp_bru->blend_control_d == NULL)
		return -ENOMEM;

	return 0;
}

static int vsp2_vspm_alloc(struct vsp2_device *vsp2)
{
	VSPM_VSP_PAR *vsp_par = NULL;
	int ret = 0;

	vsp2->vspm = devm_kzalloc(vsp2->dev, sizeof(*vsp2->vspm), GFP_KERNEL);
	if (vsp2->vspm == NULL)
		return -ENOMEM;

	vsp2->vspm->ip_par.unionIpParam.ptVsp =
		devm_kzalloc(vsp2->dev,
		sizeof(*vsp2->vspm->ip_par.unionIpParam.ptVsp),
		GFP_KERNEL);
	if (vsp2->vspm->ip_par.unionIpParam.ptVsp == NULL)
		return -ENOMEM;

	vsp_par = vsp2->vspm->ip_par.unionIpParam.ptVsp;

	ret = vsp2_vspm_alloc_vsp_in(vsp2->dev, &vsp_par->src1_par);
	if (ret != 0)
		return -ENOMEM;
	ret = vsp2_vspm_alloc_vsp_in(vsp2->dev, &vsp_par->src2_par);
	if (ret != 0)
		return -ENOMEM;
	ret = vsp2_vspm_alloc_vsp_in(vsp2->dev, &vsp_par->src3_par);
	if (ret != 0)
		return -ENOMEM;
	ret = vsp2_vspm_alloc_vsp_in(vsp2->dev, &vsp_par->src4_par);
	if (ret != 0)
		return -ENOMEM;

	vsp_par->dst_par =
	  devm_kzalloc(vsp2->dev, sizeof(*vsp_par->dst_par), GFP_KERNEL);
	if (vsp_par->dst_par == NULL)
		return -ENOMEM;

	vsp_par->ctrl_par =
	  devm_kzalloc(vsp2->dev, sizeof(*vsp_par->ctrl_par), GFP_KERNEL);
	if (vsp_par->ctrl_par == NULL)
		return -ENOMEM;

	ret = vsp2_vspm_alloc_vsp_bru(vsp2->dev, &vsp_par->ctrl_par->bru);
	if (ret != 0)
		return -ENOMEM;

	vsp_par->ctrl_par->uds =
	  devm_kzalloc(vsp2->dev, sizeof(*vsp_par->ctrl_par->uds), GFP_KERNEL);
	if (vsp_par->ctrl_par->uds == NULL)
		return -ENOMEM;

	return 0;
}

long vsp2_vspm_drv_init(struct vsp2_device *vsp2)
{
	long ret = R_VSPM_OK;

	ret = VSPM_lib_DriverInitialize(&vsp2->vspm->hdl);
	if (ret != R_VSPM_OK) {
		dev_dbg(vsp2->dev,
			"failed to VSPM_lib_DriverInitialize : %ld\n",
			ret);
		return ret;
	}

	vsp2_vspm_param_init(&vsp2->vspm->ip_par);

	return ret;
}

long vsp2_vspm_drv_quit(struct vsp2_device *vsp2)
{
	long ret = R_VSPM_OK;

	ret = VSPM_lib_DriverQuit(vsp2->vspm->hdl);
	if (ret != R_VSPM_OK) {
		dev_dbg(vsp2->dev,
			"failed to VSPM_lib_DriverQuit : %ld\n",
			ret);
	}

	return ret;
}

static void vsp2_vspm_drv_entry_cb(unsigned long job_id, long result,
				   unsigned long user_data)
{
	struct vsp2_device *vsp2;

	vsp2 = (struct vsp2_device *)user_data;

	if (job_id != vsp2->vspm->job_id)
		dev_err(vsp2->dev,
			"VSPM_lib_Entry: unexpected job id %lu (exp=%lu)\n",
			job_id, vsp2->vspm->job_id);

	if (result != R_VSPM_OK)
		dev_err(vsp2->dev, "VSPM_lib_Entry: result=%ld\n", result);

	vsp2_frame_end(vsp2);

	return;
}

void vsp2_vspm_drv_entry_work(struct work_struct *work)
{
	long ret = R_VSPM_OK;

	struct vsp2_vspm_entry_work *entry_work;
	struct vsp2_device *vsp2;
	VSPM_VSP_PAR *vsp_par;

	entry_work = (struct vsp2_vspm_entry_work *)work;
	vsp2 = entry_work->vsp2;
	vsp_par = vsp2->vspm->ip_par.unionIpParam.ptVsp;

	if (vsp_par->use_module & VSP_BRU_USE) {
		/* Set lay_order of BRU. */
		vsp_par->ctrl_par->bru->lay_order = VSP_LAY_VIRTUAL;

		if (vsp_par->rpf_num >= 1)
			vsp_par->ctrl_par->bru->lay_order |= (VSP_LAY_1 << 4);

		if (vsp_par->rpf_num >= 2)
			vsp_par->ctrl_par->bru->lay_order |= (VSP_LAY_2 << 8);

		if (vsp_par->rpf_num >= 3)
			vsp_par->ctrl_par->bru->lay_order |= (VSP_LAY_3 << 12);

		if (vsp_par->rpf_num >= 4)
			vsp_par->ctrl_par->bru->lay_order |= (VSP_LAY_4 << 16);
	} else {
		/* Not use BRU. Set RPF0 to parent layer. */
		vsp_par->src1_par->pwd = VSP_LAYER_PARENT;
	}

	ret = VSPM_lib_Entry(vsp2->vspm->hdl, &vsp2->vspm->job_id,
			     vsp2->vspm->job_pri, &vsp2->vspm->ip_par,
			     (unsigned long)vsp2, vsp2_vspm_drv_entry_cb);
	if (ret != R_VSPM_OK) {
		dev_err(vsp2->dev, "failed to VSPM_lib_Entry : %ld\n", ret);

		vsp2_frame_end(vsp2);
	}

	return;
}

void vsp2_vspm_drv_entry(struct vsp2_device *vsp2)
{
	vsp2->vspm->entry_work.vsp2 = vsp2;

	schedule_work((struct work_struct *)&vsp2->vspm->entry_work);

	return;
}

static void vsp2_vspm_work_queue_init(struct vsp2_device *vsp2)
{
	/* Initialize the work queue
	 * for the entry of job to the VSPM driver. */
	INIT_WORK((struct work_struct *)&vsp2->vspm->entry_work,
		  vsp2_vspm_drv_entry_work);

	return;
}

int vsp2_vspm_init(struct vsp2_device *vsp2, int dev_id)
{
	int ret = 0;

	ret = vsp2_vspm_alloc(vsp2);
	if (ret != 0)
		return -ENOMEM;

	/* Initialize the work queue. */
	vsp2_vspm_work_queue_init(vsp2);

	/* Initialize the parameters to VSPM driver. */
	vsp2_vspm_param_init(&vsp2->vspm->ip_par);

	/* Set the VSPM job priority. */
	vsp2->vspm->job_pri = (dev_id == DEVID_1) ? VSP2_VSPM_JOB_PRI_1
						  : VSP2_VSPM_JOB_PRI_0;

	return 0;
}
