/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/kthread.h>

#include "sde_irq.h"
#include "sde_core_irq.h"

static uint32_t g_sde_irq_status;

void sde_irq_update(struct msm_kms *msm_kms, bool enable)
{
	struct sde_kms *sde_kms = to_sde_kms(msm_kms);

	if (!msm_kms || !sde_kms) {
		SDE_ERROR("invalid kms arguments\n");
		return;
	}

	sde_kms->irq_enabled = enable;

	if (enable)
		enable_irq(sde_kms->irq_num);
	else
		disable_irq(sde_kms->irq_num);
}

irqreturn_t sde_irq(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	u32 interrupts;

	sde_kms->hw_intr->ops.get_interrupt_sources(sde_kms->hw_intr,
			&interrupts);

	/* store irq status in case of irq-storm debugging */
	g_sde_irq_status = interrupts;

	/*
	 * Taking care of MDP interrupt
	 */
	if (interrupts & IRQ_SOURCE_MDP) {
		interrupts &= ~IRQ_SOURCE_MDP;
		sde_core_irq(sde_kms);
	}

	/*
	 * Routing all other interrupts to external drivers
	 */
	while (interrupts) {
		irq_hw_number_t hwirq = fls(interrupts) - 1;
		unsigned int mapping;
		int rc;

		mapping = irq_find_mapping(sde_kms->irq_controller.domain,
				hwirq);
		if (mapping == 0) {
			SDE_EVT32(hwirq, SDE_EVTLOG_ERROR);
			goto error;
		}

		rc = generic_handle_irq(mapping);
		if (rc < 0) {
			SDE_EVT32(hwirq, mapping, rc, SDE_EVTLOG_ERROR);
			goto error;
		}

		interrupts &= ~(1 << hwirq);
	}

	return IRQ_HANDLED;

error:
	/* bad situation, inform irq system, it may disable overall MDSS irq */
	return IRQ_NONE;
}

void sde_irq_preinstall(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);

	if (!sde_kms->dev || !sde_kms->dev->dev) {
		pr_err("invalid device handles\n");
		return;
	}

	sde_core_irq_preinstall(sde_kms);

	sde_kms->irq_num = platform_get_irq(sde_kms->dev->platformdev, 0);
	if (sde_kms->irq_num < 0) {
		SDE_ERROR("invalid irq number %d\n", sde_kms->irq_num);
		return;
	}

	/* disable irq until power event enables it */
	if (!sde_kms->splash_data.cont_splash_en && !sde_kms->irq_enabled)
		irq_set_status_flags(sde_kms->irq_num, IRQ_NOAUTOEN);
}

int sde_irq_postinstall(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	int rc;

	if (!kms) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	rc = sde_core_irq_postinstall(sde_kms);

	return rc;
}

void sde_irq_uninstall(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);

	if (!kms) {
		SDE_ERROR("invalid parameters\n");
		return;
	}

	sde_core_irq_uninstall(sde_kms);
	sde_core_irq_domain_fini(sde_kms);
}
