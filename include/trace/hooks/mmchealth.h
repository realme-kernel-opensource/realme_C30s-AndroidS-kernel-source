/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmchealth
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_MMCHEALTH_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_MMCHEALTH_H
#include <linux/tracepoint.h>
#include <linux/mmc/mmc.h>
#include <trace/hooks/vendor_hooks.h>
/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */
struct mmc_card;
struct mmc_csd;

DECLARE_HOOK(android_rvh_mmc_health,
	TP_PROTO(struct mmc_card *hba),
	TP_ARGS(hba));

#endif /* _TRACE_HOOK_MMCHEALTH_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
