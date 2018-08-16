/*
 * Copyright (c) 2016 - 2017 Cadence Design Systems Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>
#include <xtensa/tie/xt_interrupt.h>
#include <xtensa/tie/xt_sync.h>
#include <xtensa/xtruntime.h>

#include "xrp_dsp_hw.h"
#include "xrp_debug.h"

typedef uint32_t __u32;
#include "xrp_hw_simple_dsp_interface.h"

enum xrp_irq_mode {
	XRP_IRQ_NONE,
	XRP_IRQ_LEVEL,
	XRP_IRQ_EDGE,
};
static enum xrp_irq_mode host_irq_mode;
static enum xrp_irq_mode device_irq_mode;
static uint32_t device_irq;

void ipcm_send(void);
void ipcm_ack(void);

static void xrp_irq_handler(void)
{
	pr_debug("%s\n", __func__);
	if (device_irq_mode == XRP_IRQ_LEVEL) {
		ipcm_ack();
	}
}

void xrp_hw_send_host_irq(void)
{
	switch (host_irq_mode) {
	case XRP_IRQ_EDGE:
	case XRP_IRQ_LEVEL:
		ipcm_send();
		break;
	default:
		break;
	}
}

void xrp_hw_wait_device_irq(void)
{
	unsigned old_intlevel;

	if (device_irq_mode == XRP_IRQ_NONE)
		return;

	pr_debug("%s: waiting for device IRQ...\n", __func__);
	old_intlevel = XTOS_SET_INTLEVEL(XCHAL_NUM_INTLEVELS - 1);
	_xtos_interrupt_enable(device_irq);
	XT_WAITI(0);
	_xtos_interrupt_disable(device_irq);
	XTOS_RESTORE_INTLEVEL(old_intlevel);
}

void xrp_hw_set_sync_data(void *p)
{
	static const enum xrp_irq_mode irq_mode[] = {
		[XRP_DSP_SYNC_IRQ_MODE_NONE] = XRP_IRQ_NONE,
		[XRP_DSP_SYNC_IRQ_MODE_LEVEL] = XRP_IRQ_LEVEL,
		[XRP_DSP_SYNC_IRQ_MODE_EDGE] = XRP_IRQ_EDGE,
	};
	struct xrp_hw_simple_sync_data *hw_sync = p;

	if (hw_sync->device_irq_mode < sizeof(irq_mode) / sizeof(*irq_mode)) {
		device_irq_mode = irq_mode[hw_sync->device_irq_mode];
		device_irq = hw_sync->device_irq;
		pr_debug("%s: device_irq_mode = %d, device_irq = %d\n",
			__func__, device_irq_mode, device_irq);
	} else {
		device_irq_mode = XRP_IRQ_NONE;
	}

	if (hw_sync->host_irq_mode < sizeof(irq_mode) / sizeof(*irq_mode)) {
		host_irq_mode = irq_mode[hw_sync->host_irq_mode];
		pr_debug("%s: host_irq_mode = %d\n",
			__func__, host_irq_mode);
	} else {
		host_irq_mode = XRP_IRQ_NONE;
	}

	if (device_irq_mode != XRP_IRQ_NONE) {
		_xtos_interrupt_disable(device_irq);
		_xtos_set_interrupt_handler(device_irq, xrp_irq_handler);
	}
}
