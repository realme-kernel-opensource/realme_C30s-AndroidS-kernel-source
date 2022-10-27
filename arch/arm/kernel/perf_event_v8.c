// SPDX-License-Identifier: GPL-2.0-only
/*
 * ARMv8 PMUv3 Performance Events handling code.
 *
 * Copyright (C) 2012 ARM Limited
 * Author: Will Deacon <will.deacon@arm.com>
 *
 * This code is based heavily on the ARMv7 perf event code.
 */

#include <asm/irq_regs.h>
#include <asm/perf_event.h>
#include <asm/cp15.h>
#include <asm/virt.h>

#include <linux/acpi.h>
#include <linux/clocksource.h>
#include <linux/kvm_host.h>
#include <linux/of.h>
#include <linux/perf/arm_pmu.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
#include <clocksource/arm_arch_timer.h>

/* ARMv8 Cortex-A53 specific event types. */
#define ARMV8_A53_PERFCTR_PREF_LINEFILL				0xC2

/* ARMv8 Cavium ThunderX specific event types. */
#define ARMV8_THUNDER_PERFCTR_L1D_CACHE_MISS_ST			0xE9
#define ARMV8_THUNDER_PERFCTR_L1D_CACHE_PREF_ACCESS		0xEA
#define ARMV8_THUNDER_PERFCTR_L1D_CACHE_PREF_MISS		0xEB
#define ARMV8_THUNDER_PERFCTR_L1I_CACHE_PREF_ACCESS		0xEC
#define ARMV8_THUNDER_PERFCTR_L1I_CACHE_PREF_MISS		0xED

#define ARMV8_PMU_MAX_COUNTERS  32
#define ARMV8_PMU_COUNTER_MASK  (ARMV8_PMU_MAX_COUNTERS - 1)
/*
* Common architectural and microarchitectural event numbers.
*/
#define ARMV8_PMUV3_PERFCTR_SW_INCR             0x00
#define ARMV8_PMUV3_PERFCTR_L1I_CACHE_REFILL            0x01
#define ARMV8_PMUV3_PERFCTR_L1I_TLB_REFILL          0x02
#define ARMV8_PMUV3_PERFCTR_L1D_CACHE_REFILL            0x03
#define ARMV8_PMUV3_PERFCTR_L1D_CACHE               0x04
#define ARMV8_PMUV3_PERFCTR_L1D_TLB_REFILL          0x05
#define ARMV8_PMUV3_PERFCTR_LD_RETIRED              0x06
#define ARMV8_PMUV3_PERFCTR_ST_RETIRED              0x07
#define ARMV8_PMUV3_PERFCTR_INST_RETIRED            0x08
#define ARMV8_PMUV3_PERFCTR_EXC_TAKEN               0x09
#define ARMV8_PMUV3_PERFCTR_EXC_RETURN              0x0A
#define ARMV8_PMUV3_PERFCTR_CID_WRITE_RETIRED           0x0B
#define ARMV8_PMUV3_PERFCTR_PC_WRITE_RETIRED            0x0C
#define ARMV8_PMUV3_PERFCTR_BR_IMMED_RETIRED            0x0D
#define ARMV8_PMUV3_PERFCTR_BR_RETURN_RETIRED           0x0E
#define ARMV8_PMUV3_PERFCTR_UNALIGNED_LDST_RETIRED      0x0F
#define ARMV8_PMUV3_PERFCTR_BR_MIS_PRED             0x10
#define ARMV8_PMUV3_PERFCTR_CPU_CYCLES              0x11
#define ARMV8_PMUV3_PERFCTR_BR_PRED             0x12
#define ARMV8_PMUV3_PERFCTR_MEM_ACCESS              0x13
#define ARMV8_PMUV3_PERFCTR_L1I_CACHE               0x14
#define ARMV8_PMUV3_PERFCTR_L1D_CACHE_WB            0x15
#define ARMV8_PMUV3_PERFCTR_L2D_CACHE               0x16
#define ARMV8_PMUV3_PERFCTR_L2D_CACHE_REFILL            0x17
#define ARMV8_PMUV3_PERFCTR_L2D_CACHE_WB            0x18
#define ARMV8_PMUV3_PERFCTR_BUS_ACCESS              0x19
#define ARMV8_PMUV3_PERFCTR_MEMORY_ERROR            0x1A
#define ARMV8_PMUV3_PERFCTR_INST_SPEC               0x1B
#define ARMV8_PMUV3_PERFCTR_TTBR_WRITE_RETIRED          0x1C
#define ARMV8_PMUV3_PERFCTR_BUS_CYCLES              0x1D
#define ARMV8_PMUV3_PERFCTR_CHAIN               0x1E
#define ARMV8_PMUV3_PERFCTR_L1D_CACHE_ALLOCATE          0x1F
#define ARMV8_PMUV3_PERFCTR_L2D_CACHE_ALLOCATE          0x20
#define ARMV8_PMUV3_PERFCTR_BR_RETIRED              0x21
#define ARMV8_PMUV3_PERFCTR_BR_MIS_PRED_RETIRED         0x22
#define ARMV8_PMUV3_PERFCTR_STALL_FRONTEND          0x23
#define ARMV8_PMUV3_PERFCTR_STALL_BACKEND           0x24
#define ARMV8_PMUV3_PERFCTR_L1D_TLB             0x25
#define ARMV8_PMUV3_PERFCTR_L1I_TLB             0x26
#define ARMV8_PMUV3_PERFCTR_L2I_CACHE               0x27
#define ARMV8_PMUV3_PERFCTR_L2I_CACHE_REFILL            0x28
#define ARMV8_PMUV3_PERFCTR_L3D_CACHE_ALLOCATE          0x29
#define ARMV8_PMUV3_PERFCTR_L3D_CACHE_REFILL            0x2A
#define ARMV8_PMUV3_PERFCTR_L3D_CACHE               0x2B
#define ARMV8_PMUV3_PERFCTR_L3D_CACHE_WB            0x2C
#define ARMV8_PMUV3_PERFCTR_L2D_TLB_REFILL          0x2D
#define ARMV8_PMUV3_PERFCTR_L2I_TLB_REFILL          0x2E
#define ARMV8_PMUV3_PERFCTR_L2D_TLB             0x2F
#define ARMV8_PMUV3_PERFCTR_L2I_TLB             0x30
#define ARMV8_PMUV3_PERFCTR_REMOTE_ACCESS           0x31
#define ARMV8_PMUV3_PERFCTR_LL_CACHE                0x32
#define ARMV8_PMUV3_PERFCTR_LL_CACHE_MISS           0x33
#define ARMV8_PMUV3_PERFCTR_DTLB_WALK               0x34
#define ARMV8_PMUV3_PERFCTR_ITLB_WALK               0x35
#define ARMV8_PMUV3_PERFCTR_LL_CACHE_RD             0x36
#define ARMV8_PMUV3_PERFCTR_LL_CACHE_MISS_RD            0x37
#define ARMV8_PMUV3_PERFCTR_REMOTE_ACCESS_RD            0x38

/* Statistical profiling extension microarchitectural events */
#define ARMV8_SPE_PERFCTR_SAMPLE_POP                0x4000
#define ARMV8_SPE_PERFCTR_SAMPLE_FEED               0x4001
#define ARMV8_SPE_PERFCTR_SAMPLE_FILTRATE           0x4002
#define ARMV8_SPE_PERFCTR_SAMPLE_COLLISION          0x4003

/* ARMv8 recommended implementation defined event types */
#define ARMV8_IMPDEF_PERFCTR_L1D_CACHE_RD           0x40
#define ARMV8_IMPDEF_PERFCTR_L1D_CACHE_WR           0x41
#define ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_RD        0x42
#define ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_WR        0x43
#define ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_INNER     0x44
#define ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_OUTER     0x45
#define ARMV8_IMPDEF_PERFCTR_L1D_CACHE_WB_VICTIM        0x46
#define ARMV8_IMPDEF_PERFCTR_L1D_CACHE_WB_CLEAN         0x47
#define ARMV8_IMPDEF_PERFCTR_L1D_CACHE_INVAL            0x48
#define ARMV8_IMPDEF_PERFCTR_L1D_TLB_REFILL_RD          0x4C
#define ARMV8_IMPDEF_PERFCTR_L1D_TLB_REFILL_WR          0x4D
#define ARMV8_IMPDEF_PERFCTR_L1D_TLB_RD             0x4E
#define ARMV8_IMPDEF_PERFCTR_L1D_TLB_WR             0x4F
#define ARMV8_IMPDEF_PERFCTR_L2D_CACHE_RD           0x50
#define ARMV8_IMPDEF_PERFCTR_L2D_CACHE_WR           0x51
#define ARMV8_IMPDEF_PERFCTR_L2D_CACHE_REFILL_RD        0x52
#define ARMV8_IMPDEF_PERFCTR_L2D_CACHE_REFILL_WR        0x53
#define ARMV8_IMPDEF_PERFCTR_L2D_CACHE_WB_VICTIM        0x56
#define ARMV8_IMPDEF_PERFCTR_L2D_CACHE_WB_CLEAN         0x57
#define ARMV8_IMPDEF_PERFCTR_L2D_CACHE_INVAL            0x58
#define ARMV8_IMPDEF_PERFCTR_L2D_TLB_REFILL_RD          0x5C
#define ARMV8_IMPDEF_PERFCTR_L2D_TLB_REFILL_WR          0x5D
#define ARMV8_IMPDEF_PERFCTR_L2D_TLB_RD             0x5E
#define ARMV8_IMPDEF_PERFCTR_L2D_TLB_WR             0x5F
#define ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_RD          0x60
#define ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_WR          0x61
#define ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_SHARED          0x62
#define ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_NOT_SHARED      0x63
#define ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_NORMAL          0x64
#define ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_PERIPH          0x65
#define ARMV8_IMPDEF_PERFCTR_MEM_ACCESS_RD          0x66
#define ARMV8_IMPDEF_PERFCTR_MEM_ACCESS_WR          0x67
#define ARMV8_IMPDEF_PERFCTR_UNALIGNED_LD_SPEC          0x68
#define ARMV8_IMPDEF_PERFCTR_UNALIGNED_ST_SPEC          0x69
#define ARMV8_IMPDEF_PERFCTR_UNALIGNED_LDST_SPEC        0x6A
#define ARMV8_IMPDEF_PERFCTR_LDREX_SPEC             0x6C
#define ARMV8_IMPDEF_PERFCTR_STREX_PASS_SPEC            0x6D
#define ARMV8_IMPDEF_PERFCTR_STREX_FAIL_SPEC            0x6E
#define ARMV8_IMPDEF_PERFCTR_STREX_SPEC             0x6F
#define ARMV8_IMPDEF_PERFCTR_LD_SPEC                0x70
#define ARMV8_IMPDEF_PERFCTR_ST_SPEC                0x71
#define ARMV8_IMPDEF_PERFCTR_LDST_SPEC              0x72
#define ARMV8_IMPDEF_PERFCTR_DP_SPEC                0x73
#define ARMV8_IMPDEF_PERFCTR_ASE_SPEC               0x74
#define ARMV8_IMPDEF_PERFCTR_VFP_SPEC               0x75
#define ARMV8_IMPDEF_PERFCTR_PC_WRITE_SPEC          0x76
#define ARMV8_IMPDEF_PERFCTR_CRYPTO_SPEC            0x77
#define ARMV8_IMPDEF_PERFCTR_BR_IMMED_SPEC          0x78
#define ARMV8_IMPDEF_PERFCTR_BR_RETURN_SPEC         0x79
#define ARMV8_IMPDEF_PERFCTR_BR_INDIRECT_SPEC           0x7A
#define ARMV8_IMPDEF_PERFCTR_ISB_SPEC               0x7C
#define ARMV8_IMPDEF_PERFCTR_DSB_SPEC               0x7D
#define ARMV8_IMPDEF_PERFCTR_DMB_SPEC               0x7E
#define ARMV8_IMPDEF_PERFCTR_EXC_UNDEF              0x81
#define ARMV8_IMPDEF_PERFCTR_EXC_SVC                0x82
#define ARMV8_IMPDEF_PERFCTR_EXC_PABORT             0x83
#define ARMV8_IMPDEF_PERFCTR_EXC_DABORT             0x84
#define ARMV8_IMPDEF_PERFCTR_EXC_IRQ                0x86
#define ARMV8_IMPDEF_PERFCTR_EXC_FIQ                0x87
#define ARMV8_IMPDEF_PERFCTR_EXC_SMC                0x88
#define ARMV8_IMPDEF_PERFCTR_EXC_HVC                0x8A
#define ARMV8_IMPDEF_PERFCTR_EXC_TRAP_PABORT            0x8B
#define ARMV8_IMPDEF_PERFCTR_EXC_TRAP_DABORT            0x8C
#define ARMV8_IMPDEF_PERFCTR_EXC_TRAP_OTHER         0x8D
#define ARMV8_IMPDEF_PERFCTR_EXC_TRAP_IRQ           0x8E
#define ARMV8_IMPDEF_PERFCTR_EXC_TRAP_FIQ           0x8F
#define ARMV8_IMPDEF_PERFCTR_RC_LD_SPEC             0x90
#define ARMV8_IMPDEF_PERFCTR_RC_ST_SPEC             0x91
#define ARMV8_IMPDEF_PERFCTR_L3D_CACHE_RD           0xA0
#define ARMV8_IMPDEF_PERFCTR_L3D_CACHE_WR           0xA1
#define ARMV8_IMPDEF_PERFCTR_L3D_CACHE_REFILL_RD        0xA2
#define ARMV8_IMPDEF_PERFCTR_L3D_CACHE_REFILL_WR        0xA3
#define ARMV8_IMPDEF_PERFCTR_L3D_CACHE_WB_VICTIM        0xA6
#define ARMV8_IMPDEF_PERFCTR_L3D_CACHE_WB_CLEAN         0xA7
#define ARMV8_IMPDEF_PERFCTR_L3D_CACHE_INVAL            0xA8

/*
* Per-CPU PMCR: config reg
*/
#define ARMV8_PMU_PMCR_E    (1 << 0) /* Enable all counters */
#define ARMV8_PMU_PMCR_P    (1 << 1) /* Reset all counters */
#define ARMV8_PMU_PMCR_C    (1 << 2) /* Cycle counter reset */
#define ARMV8_PMU_PMCR_D    (1 << 3) /* CCNT counts every 64th cpu cycle */
#define ARMV8_PMU_PMCR_X    (1 << 4) /* Export to ETM */
#define ARMV8_PMU_PMCR_DP   (1 << 5) /* Disable CCNT if non-invasive debug*/
#define ARMV8_PMU_PMCR_LC   (1 << 6) /* Overflow on 64 bit cycle counter */
#define ARMV8_PMU_PMCR_N_SHIFT  11   /* Number of counters supported */
#define ARMV8_PMU_PMCR_N_MASK   0x1f
#define ARMV8_PMU_PMCR_MASK 0x7f     /* Mask for writable bits */

/*
* PMOVSR: counters overflow flag status reg
*/
#define ARMV8_PMU_OVSR_MASK     0xffffffff  /* Mask for writable bits */
#define ARMV8_PMU_OVERFLOWED_MASK   ARMV8_PMU_OVSR_MASK

/*
* PMXEVTYPER: Event selection reg
*/
#define ARMV8_PMU_EVTYPE_MASK   0xc800ffff  /* Mask for writable bits */
#define ARMV8_PMU_EVTYPE_EVENT  0xffff      /* Mask for EVENT bits */
/*
 * Event filters for PMUv3
*/
#define ARMV8_PMU_EXCLUDE_EL1   (1U << 31)
#define ARMV8_PMU_EXCLUDE_EL0   (1U << 30)
#define ARMV8_PMU_INCLUDE_EL2   (1U << 27)

/*
* PMUSERENR: user enable reg
*/
#define ARMV8_PMU_USERENR_MASK  0xf     /* Mask for writable bits */
#define ARMV8_PMU_USERENR_EN    (1 << 0) /* PMU regs can be accessed at EL0 */
#define ARMV8_PMU_USERENR_SW    (1 << 1) /* PMSWINC can be written at EL0 */
#define ARMV8_PMU_USERENR_CR    (1 << 2) /* Cycle counter can be read at EL0 */
#define ARMV8_PMU_USERENR_ER    (1 << 3) /* Event counter can be read at EL0 */

/*
 * ARMv8 Architectural defined events, not all of these may
 * be supported on any given implementation. Unsupported events will
 * be disabled at run-time based on the PMCEID registers.
 */
static const unsigned armv8_pmuv3_perf_map[PERF_COUNT_HW_MAX] = {
	PERF_MAP_ALL_UNSUPPORTED,
	[PERF_COUNT_HW_CPU_CYCLES]		= ARMV8_PMUV3_PERFCTR_CPU_CYCLES,
	[PERF_COUNT_HW_INSTRUCTIONS]		= ARMV8_PMUV3_PERFCTR_INST_RETIRED,
	[PERF_COUNT_HW_CACHE_REFERENCES]	= ARMV8_PMUV3_PERFCTR_L1D_CACHE,
	[PERF_COUNT_HW_CACHE_MISSES]		= ARMV8_PMUV3_PERFCTR_L1D_CACHE_REFILL,
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS]	= ARMV8_PMUV3_PERFCTR_PC_WRITE_RETIRED,
	[PERF_COUNT_HW_BRANCH_MISSES]		= ARMV8_PMUV3_PERFCTR_BR_MIS_PRED,
	[PERF_COUNT_HW_BUS_CYCLES]		= ARMV8_PMUV3_PERFCTR_BUS_CYCLES,
	[PERF_COUNT_HW_STALLED_CYCLES_FRONTEND]	= ARMV8_PMUV3_PERFCTR_STALL_FRONTEND,
	[PERF_COUNT_HW_STALLED_CYCLES_BACKEND]	= ARMV8_PMUV3_PERFCTR_STALL_BACKEND,
};

static const unsigned armv8_pmuv3_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
						[PERF_COUNT_HW_CACHE_OP_MAX]
						[PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	PERF_CACHE_MAP_ALL_UNSUPPORTED,

	[C(L1D)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_L1D_CACHE,
	[C(L1D)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_L1D_CACHE_REFILL,

	[C(L1I)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_L1I_CACHE,
	[C(L1I)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_L1I_CACHE_REFILL,

	[C(DTLB)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_L1D_TLB_REFILL,
	[C(DTLB)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_L1D_TLB,

	[C(ITLB)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_L1I_TLB_REFILL,
	[C(ITLB)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_L1I_TLB,

	[C(BPU)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_BR_PRED,
	[C(BPU)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_BR_MIS_PRED,
};

static const unsigned armv8_a53_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
					      [PERF_COUNT_HW_CACHE_OP_MAX]
					      [PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	PERF_CACHE_MAP_ALL_UNSUPPORTED,

	[C(L1D)][C(OP_PREFETCH)][C(RESULT_MISS)] = ARMV8_A53_PERFCTR_PREF_LINEFILL,

	[C(NODE)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_RD,
	[C(NODE)][C(OP_WRITE)][C(RESULT_ACCESS)] = ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_WR,
};

static const unsigned armv8_a57_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
					      [PERF_COUNT_HW_CACHE_OP_MAX]
					      [PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	PERF_CACHE_MAP_ALL_UNSUPPORTED,

	[C(L1D)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_RD,
	[C(L1D)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_RD,
	[C(L1D)][C(OP_WRITE)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_WR,
	[C(L1D)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_WR,

	[C(DTLB)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_IMPDEF_PERFCTR_L1D_TLB_REFILL_RD,
	[C(DTLB)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_IMPDEF_PERFCTR_L1D_TLB_REFILL_WR,

	[C(NODE)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_RD,
	[C(NODE)][C(OP_WRITE)][C(RESULT_ACCESS)] = ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_WR,
};

static const unsigned armv8_a73_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
					      [PERF_COUNT_HW_CACHE_OP_MAX]
					      [PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	PERF_CACHE_MAP_ALL_UNSUPPORTED,

	[C(L1D)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_RD,
	[C(L1D)][C(OP_WRITE)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_WR,
};

static const unsigned armv8_thunder_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
						   [PERF_COUNT_HW_CACHE_OP_MAX]
						   [PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	PERF_CACHE_MAP_ALL_UNSUPPORTED,

	[C(L1D)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_RD,
	[C(L1D)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_RD,
	[C(L1D)][C(OP_WRITE)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_WR,
	[C(L1D)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_THUNDER_PERFCTR_L1D_CACHE_MISS_ST,
	[C(L1D)][C(OP_PREFETCH)][C(RESULT_ACCESS)] = ARMV8_THUNDER_PERFCTR_L1D_CACHE_PREF_ACCESS,
	[C(L1D)][C(OP_PREFETCH)][C(RESULT_MISS)] = ARMV8_THUNDER_PERFCTR_L1D_CACHE_PREF_MISS,

	[C(L1I)][C(OP_PREFETCH)][C(RESULT_ACCESS)] = ARMV8_THUNDER_PERFCTR_L1I_CACHE_PREF_ACCESS,
	[C(L1I)][C(OP_PREFETCH)][C(RESULT_MISS)] = ARMV8_THUNDER_PERFCTR_L1I_CACHE_PREF_MISS,

	[C(DTLB)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_L1D_TLB_RD,
	[C(DTLB)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_IMPDEF_PERFCTR_L1D_TLB_REFILL_RD,
	[C(DTLB)][C(OP_WRITE)][C(RESULT_ACCESS)] = ARMV8_IMPDEF_PERFCTR_L1D_TLB_WR,
	[C(DTLB)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_IMPDEF_PERFCTR_L1D_TLB_REFILL_WR,
};

static const unsigned armv8_vulcan_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
					      [PERF_COUNT_HW_CACHE_OP_MAX]
					      [PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	PERF_CACHE_MAP_ALL_UNSUPPORTED,

	[C(L1D)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_RD,
	[C(L1D)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_RD,
	[C(L1D)][C(OP_WRITE)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_WR,
	[C(L1D)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_WR,

	[C(DTLB)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_L1D_TLB_RD,
	[C(DTLB)][C(OP_WRITE)][C(RESULT_ACCESS)] = ARMV8_IMPDEF_PERFCTR_L1D_TLB_WR,
	[C(DTLB)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_IMPDEF_PERFCTR_L1D_TLB_REFILL_RD,
	[C(DTLB)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_IMPDEF_PERFCTR_L1D_TLB_REFILL_WR,

	[C(NODE)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_RD,
	[C(NODE)][C(OP_WRITE)][C(RESULT_ACCESS)] = ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_WR,
};

static ssize_t
armv8pmu_events_sysfs_show(struct device *dev,
			   struct device_attribute *attr, char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);

	return sprintf(page, "event=0x%04llx\n", pmu_attr->id);
}

#define ARMV8_EVENT_ATTR(name, config) \
	PMU_EVENT_ATTR(name, armv8_event_attr_##name, \
		       config, armv8pmu_events_sysfs_show)

ARMV8_EVENT_ATTR(sw_incr, ARMV8_PMUV3_PERFCTR_SW_INCR);
ARMV8_EVENT_ATTR(l1i_cache_refill, ARMV8_PMUV3_PERFCTR_L1I_CACHE_REFILL);
ARMV8_EVENT_ATTR(l1i_tlb_refill, ARMV8_PMUV3_PERFCTR_L1I_TLB_REFILL);
ARMV8_EVENT_ATTR(l1d_cache_refill, ARMV8_PMUV3_PERFCTR_L1D_CACHE_REFILL);
ARMV8_EVENT_ATTR(l1d_cache, ARMV8_PMUV3_PERFCTR_L1D_CACHE);
ARMV8_EVENT_ATTR(l1d_tlb_refill, ARMV8_PMUV3_PERFCTR_L1D_TLB_REFILL);
ARMV8_EVENT_ATTR(ld_retired, ARMV8_PMUV3_PERFCTR_LD_RETIRED);
ARMV8_EVENT_ATTR(st_retired, ARMV8_PMUV3_PERFCTR_ST_RETIRED);
ARMV8_EVENT_ATTR(inst_retired, ARMV8_PMUV3_PERFCTR_INST_RETIRED);
ARMV8_EVENT_ATTR(exc_taken, ARMV8_PMUV3_PERFCTR_EXC_TAKEN);
ARMV8_EVENT_ATTR(exc_return, ARMV8_PMUV3_PERFCTR_EXC_RETURN);
ARMV8_EVENT_ATTR(cid_write_retired, ARMV8_PMUV3_PERFCTR_CID_WRITE_RETIRED);
ARMV8_EVENT_ATTR(pc_write_retired, ARMV8_PMUV3_PERFCTR_PC_WRITE_RETIRED);
ARMV8_EVENT_ATTR(br_immed_retired, ARMV8_PMUV3_PERFCTR_BR_IMMED_RETIRED);
ARMV8_EVENT_ATTR(br_return_retired, ARMV8_PMUV3_PERFCTR_BR_RETURN_RETIRED);
ARMV8_EVENT_ATTR(unaligned_ldst_retired, ARMV8_PMUV3_PERFCTR_UNALIGNED_LDST_RETIRED);
ARMV8_EVENT_ATTR(br_mis_pred, ARMV8_PMUV3_PERFCTR_BR_MIS_PRED);
ARMV8_EVENT_ATTR(cpu_cycles, ARMV8_PMUV3_PERFCTR_CPU_CYCLES);
ARMV8_EVENT_ATTR(br_pred, ARMV8_PMUV3_PERFCTR_BR_PRED);
ARMV8_EVENT_ATTR(mem_access, ARMV8_PMUV3_PERFCTR_MEM_ACCESS);
ARMV8_EVENT_ATTR(l1i_cache, ARMV8_PMUV3_PERFCTR_L1I_CACHE);
ARMV8_EVENT_ATTR(l1d_cache_wb, ARMV8_PMUV3_PERFCTR_L1D_CACHE_WB);
ARMV8_EVENT_ATTR(l2d_cache, ARMV8_PMUV3_PERFCTR_L2D_CACHE);
ARMV8_EVENT_ATTR(l2d_cache_refill, ARMV8_PMUV3_PERFCTR_L2D_CACHE_REFILL);
ARMV8_EVENT_ATTR(l2d_cache_wb, ARMV8_PMUV3_PERFCTR_L2D_CACHE_WB);
ARMV8_EVENT_ATTR(bus_access, ARMV8_PMUV3_PERFCTR_BUS_ACCESS);
ARMV8_EVENT_ATTR(memory_error, ARMV8_PMUV3_PERFCTR_MEMORY_ERROR);
ARMV8_EVENT_ATTR(inst_spec, ARMV8_PMUV3_PERFCTR_INST_SPEC);
ARMV8_EVENT_ATTR(ttbr_write_retired, ARMV8_PMUV3_PERFCTR_TTBR_WRITE_RETIRED);
ARMV8_EVENT_ATTR(bus_cycles, ARMV8_PMUV3_PERFCTR_BUS_CYCLES);
/* Don't expose the chain event in /sys, since it's useless in isolation */
ARMV8_EVENT_ATTR(l1d_cache_allocate, ARMV8_PMUV3_PERFCTR_L1D_CACHE_ALLOCATE);
ARMV8_EVENT_ATTR(l2d_cache_allocate, ARMV8_PMUV3_PERFCTR_L2D_CACHE_ALLOCATE);
ARMV8_EVENT_ATTR(br_retired, ARMV8_PMUV3_PERFCTR_BR_RETIRED);
ARMV8_EVENT_ATTR(br_mis_pred_retired, ARMV8_PMUV3_PERFCTR_BR_MIS_PRED_RETIRED);
ARMV8_EVENT_ATTR(stall_frontend, ARMV8_PMUV3_PERFCTR_STALL_FRONTEND);
ARMV8_EVENT_ATTR(stall_backend, ARMV8_PMUV3_PERFCTR_STALL_BACKEND);
ARMV8_EVENT_ATTR(l1d_tlb, ARMV8_PMUV3_PERFCTR_L1D_TLB);
ARMV8_EVENT_ATTR(l1i_tlb, ARMV8_PMUV3_PERFCTR_L1I_TLB);
ARMV8_EVENT_ATTR(l2i_cache, ARMV8_PMUV3_PERFCTR_L2I_CACHE);
ARMV8_EVENT_ATTR(l2i_cache_refill, ARMV8_PMUV3_PERFCTR_L2I_CACHE_REFILL);
ARMV8_EVENT_ATTR(l3d_cache_allocate, ARMV8_PMUV3_PERFCTR_L3D_CACHE_ALLOCATE);
ARMV8_EVENT_ATTR(l3d_cache_refill, ARMV8_PMUV3_PERFCTR_L3D_CACHE_REFILL);
ARMV8_EVENT_ATTR(l3d_cache, ARMV8_PMUV3_PERFCTR_L3D_CACHE);
ARMV8_EVENT_ATTR(l3d_cache_wb, ARMV8_PMUV3_PERFCTR_L3D_CACHE_WB);
ARMV8_EVENT_ATTR(l2d_tlb_refill, ARMV8_PMUV3_PERFCTR_L2D_TLB_REFILL);
ARMV8_EVENT_ATTR(l2i_tlb_refill, ARMV8_PMUV3_PERFCTR_L2I_TLB_REFILL);
ARMV8_EVENT_ATTR(l2d_tlb, ARMV8_PMUV3_PERFCTR_L2D_TLB);
ARMV8_EVENT_ATTR(l2i_tlb, ARMV8_PMUV3_PERFCTR_L2I_TLB);
ARMV8_EVENT_ATTR(remote_access, ARMV8_PMUV3_PERFCTR_REMOTE_ACCESS);
ARMV8_EVENT_ATTR(ll_cache, ARMV8_PMUV3_PERFCTR_LL_CACHE);
ARMV8_EVENT_ATTR(ll_cache_miss, ARMV8_PMUV3_PERFCTR_LL_CACHE_MISS);
ARMV8_EVENT_ATTR(dtlb_walk, ARMV8_PMUV3_PERFCTR_DTLB_WALK);
ARMV8_EVENT_ATTR(itlb_walk, ARMV8_PMUV3_PERFCTR_ITLB_WALK);
ARMV8_EVENT_ATTR(ll_cache_rd, ARMV8_PMUV3_PERFCTR_LL_CACHE_RD);
ARMV8_EVENT_ATTR(ll_cache_miss_rd, ARMV8_PMUV3_PERFCTR_LL_CACHE_MISS_RD);
ARMV8_EVENT_ATTR(remote_access_rd, ARMV8_PMUV3_PERFCTR_REMOTE_ACCESS_RD);
ARMV8_EVENT_ATTR(sample_pop, ARMV8_SPE_PERFCTR_SAMPLE_POP);
ARMV8_EVENT_ATTR(sample_feed, ARMV8_SPE_PERFCTR_SAMPLE_FEED);
ARMV8_EVENT_ATTR(sample_filtrate, ARMV8_SPE_PERFCTR_SAMPLE_FILTRATE);
ARMV8_EVENT_ATTR(sample_collision, ARMV8_SPE_PERFCTR_SAMPLE_COLLISION);

static struct attribute *armv8_pmuv3_event_attrs[] = {
	&armv8_event_attr_sw_incr.attr.attr,
	&armv8_event_attr_l1i_cache_refill.attr.attr,
	&armv8_event_attr_l1i_tlb_refill.attr.attr,
	&armv8_event_attr_l1d_cache_refill.attr.attr,
	&armv8_event_attr_l1d_cache.attr.attr,
	&armv8_event_attr_l1d_tlb_refill.attr.attr,
	&armv8_event_attr_ld_retired.attr.attr,
	&armv8_event_attr_st_retired.attr.attr,
	&armv8_event_attr_inst_retired.attr.attr,
	&armv8_event_attr_exc_taken.attr.attr,
	&armv8_event_attr_exc_return.attr.attr,
	&armv8_event_attr_cid_write_retired.attr.attr,
	&armv8_event_attr_pc_write_retired.attr.attr,
	&armv8_event_attr_br_immed_retired.attr.attr,
	&armv8_event_attr_br_return_retired.attr.attr,
	&armv8_event_attr_unaligned_ldst_retired.attr.attr,
	&armv8_event_attr_br_mis_pred.attr.attr,
	&armv8_event_attr_cpu_cycles.attr.attr,
	&armv8_event_attr_br_pred.attr.attr,
	&armv8_event_attr_mem_access.attr.attr,
	&armv8_event_attr_l1i_cache.attr.attr,
	&armv8_event_attr_l1d_cache_wb.attr.attr,
	&armv8_event_attr_l2d_cache.attr.attr,
	&armv8_event_attr_l2d_cache_refill.attr.attr,
	&armv8_event_attr_l2d_cache_wb.attr.attr,
	&armv8_event_attr_bus_access.attr.attr,
	&armv8_event_attr_memory_error.attr.attr,
	&armv8_event_attr_inst_spec.attr.attr,
	&armv8_event_attr_ttbr_write_retired.attr.attr,
	&armv8_event_attr_bus_cycles.attr.attr,
	&armv8_event_attr_l1d_cache_allocate.attr.attr,
	&armv8_event_attr_l2d_cache_allocate.attr.attr,
	&armv8_event_attr_br_retired.attr.attr,
	&armv8_event_attr_br_mis_pred_retired.attr.attr,
	&armv8_event_attr_stall_frontend.attr.attr,
	&armv8_event_attr_stall_backend.attr.attr,
	&armv8_event_attr_l1d_tlb.attr.attr,
	&armv8_event_attr_l1i_tlb.attr.attr,
	&armv8_event_attr_l2i_cache.attr.attr,
	&armv8_event_attr_l2i_cache_refill.attr.attr,
	&armv8_event_attr_l3d_cache_allocate.attr.attr,
	&armv8_event_attr_l3d_cache_refill.attr.attr,
	&armv8_event_attr_l3d_cache.attr.attr,
	&armv8_event_attr_l3d_cache_wb.attr.attr,
	&armv8_event_attr_l2d_tlb_refill.attr.attr,
	&armv8_event_attr_l2i_tlb_refill.attr.attr,
	&armv8_event_attr_l2d_tlb.attr.attr,
	&armv8_event_attr_l2i_tlb.attr.attr,
	&armv8_event_attr_remote_access.attr.attr,
	&armv8_event_attr_ll_cache.attr.attr,
	&armv8_event_attr_ll_cache_miss.attr.attr,
	&armv8_event_attr_dtlb_walk.attr.attr,
	&armv8_event_attr_itlb_walk.attr.attr,
	&armv8_event_attr_ll_cache_rd.attr.attr,
	&armv8_event_attr_ll_cache_miss_rd.attr.attr,
	&armv8_event_attr_remote_access_rd.attr.attr,
	&armv8_event_attr_sample_pop.attr.attr,
	&armv8_event_attr_sample_feed.attr.attr,
	&armv8_event_attr_sample_filtrate.attr.attr,
	&armv8_event_attr_sample_collision.attr.attr,
	NULL,
};

static umode_t
armv8pmu_event_attr_is_visible(struct kobject *kobj,
			       struct attribute *attr, int unused)
{
	struct device *dev = kobj_to_dev(kobj);
	struct pmu *pmu = dev_get_drvdata(dev);
	struct arm_pmu *cpu_pmu = container_of(pmu, struct arm_pmu, pmu);
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr.attr);

	if (pmu_attr->id < ARMV8_PMUV3_MAX_COMMON_EVENTS &&
	    test_bit(pmu_attr->id, cpu_pmu->pmceid_bitmap))
		return attr->mode;

	if (pmu_attr->id >= ARMV8_PMUV3_EXT_COMMON_EVENT_BASE) {
		u64 id = pmu_attr->id - ARMV8_PMUV3_EXT_COMMON_EVENT_BASE;

		if (id < ARMV8_PMUV3_MAX_COMMON_EVENTS &&
		    test_bit(id, cpu_pmu->pmceid_ext_bitmap))
			return attr->mode;
	}

	return 0;
}

static struct attribute_group armv8_pmuv3_events_attr_group = {
	.name = "events",
	.attrs = armv8_pmuv3_event_attrs,
	.is_visible = armv8pmu_event_attr_is_visible,
};

PMU_FORMAT_ATTR(event, "config:0-15");
PMU_FORMAT_ATTR(long, "config1:0");

static inline bool armv8pmu_event_is_64bit(struct perf_event *event)
{
	return event->attr.config1 & 0x1;
}

static struct attribute *armv8_pmuv3_format_attrs[] = {
	&format_attr_event.attr,
	&format_attr_long.attr,
	NULL,
};

static struct attribute_group armv8_pmuv3_format_attr_group = {
	.name = "format",
	.attrs = armv8_pmuv3_format_attrs,
};

/*
 * Perf Events' indices
 */
#define	ARMV8_IDX_CYCLE_COUNTER	0
#define	ARMV8_IDX_COUNTER0	1
#define	ARMV8_IDX_COUNTER_LAST(cpu_pmu) \
	(ARMV8_IDX_CYCLE_COUNTER + cpu_pmu->num_events - 1)

/*
 * We must chain two programmable counters for 64 bit events,
 * except when we have allocated the 64bit cycle counter (for CPU
 * cycles event). This must be called only when the event has
 * a counter allocated.
 */
static inline bool armv8pmu_event_is_chained(struct perf_event *event)
{
	int idx = event->hw.idx;

	return !WARN_ON(idx < 0) &&
	       armv8pmu_event_is_64bit(event) &&
	       (idx != ARMV8_IDX_CYCLE_COUNTER);
}

/*
 * ARMv8 low level PMU access
 */

/*
 * Perf Event to low level counters mapping
 */
#define	ARMV8_IDX_TO_COUNTER(x)	\
	(((x) - ARMV8_IDX_COUNTER0) & ARMV8_PMU_COUNTER_MASK)

static inline u32 armv8pmu_pmcr_read(void)
{
	u32 value;
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r" (value));
	return value;
}

static inline void armv8pmu_pmcr_write(u32 val)
{
	val &= ARMV8_PMU_PMCR_MASK;
	isb();
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r" (val));
}

static inline int armv8pmu_has_overflowed(u32 pmovsr)
{
	return pmovsr & ARMV8_PMU_OVERFLOWED_MASK;
}

static inline int armv8pmu_counter_valid(struct arm_pmu *cpu_pmu, int idx)
{
	return idx >= ARMV8_IDX_CYCLE_COUNTER &&
		idx <= ARMV8_IDX_COUNTER_LAST(cpu_pmu);
}

static inline int armv8pmu_counter_has_overflowed(u32 pmnc, int idx)
{
	return pmnc & BIT(ARMV8_IDX_TO_COUNTER(idx));
}

static inline void armv8pmu_select_counter(int idx)
{
	u32 counter = ARMV8_IDX_TO_COUNTER(idx);
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r" (counter));
	isb();
}

static inline u32 armv8pmu_read_evcntr(int idx)
{
	u32 value = 0;
	armv8pmu_select_counter(idx);
	asm volatile("mrc p15, 0, %0, c9, c13, 2" : "=r" (value));
	return value;
}

static inline u64 armv8pmu_read_hw_counter(struct perf_event *event)
{
	int idx = event->hw.idx;
	u64 val = 0;

	val = armv8pmu_read_evcntr(idx);
	if (armv8pmu_event_is_chained(event))
		val = (val << 32) | armv8pmu_read_evcntr(idx - 1);
	return val;
}

static u64 armv8pmu_read_counter(struct perf_event *event)
{
	struct arm_pmu *cpu_pmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	u64 value = 0;

	if (!armv8pmu_counter_valid(cpu_pmu, idx))
		pr_err("CPU%u reading wrong counter %d\n",
			smp_processor_id(), idx);
	else if (idx == ARMV8_IDX_CYCLE_COUNTER)
		asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (value));
	else
		value = armv8pmu_read_hw_counter(event);

	return value;
}

static inline void armv8pmu_write_evcntr(int idx, u32 value)
{
	armv8pmu_select_counter(idx);
	asm volatile("mcr p15, 0, %0, c9, c13, 2" : : "r" (value));
}

static inline void armv8pmu_write_hw_counter(struct perf_event *event,
					     u64 value)
{
	int idx = event->hw.idx;
	armv8pmu_write_evcntr(idx, value);
}

static void armv8pmu_write_counter(struct perf_event *event, u64 value)
{
	struct arm_pmu *cpu_pmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	if (!armv8pmu_counter_valid(cpu_pmu, idx))
		pr_err("CPU%u writing wrong counter %d\n",
			smp_processor_id(), idx);
	else if (idx == ARMV8_IDX_CYCLE_COUNTER) {
		/*
		 * The cycles counter is really a 64-bit counter.
		 * When treating it as a 32-bit counter, we only count
		 * the lower 32 bits, and set the upper 32-bits so that
		 * we get an interrupt upon 32-bit overflow.
		 */
		if (!armv8pmu_event_is_64bit(event))
			asm volatile("mcrr p15, 0, %0, %1, c9" : : "r" (value), "r" (0xffffffff));
	} else
		armv8pmu_write_hw_counter(event, value);
}

static inline void armv8pmu_write_evtype(int idx, u32 val)
{
	armv8pmu_select_counter(idx);
	val &= ARMV8_PMU_EVTYPE_MASK;
	asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r" (val));
}

static inline void armv8pmu_write_event_type(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	/*
	 * For chained events, the low counter is programmed to count
	 * the event of interest and the high counter is programmed
	 * with CHAIN event code with filters set to count at all ELs.
	 */
	if (armv8pmu_event_is_chained(event)) {
		u32 chain_evt = ARMV8_PMUV3_PERFCTR_CHAIN |
				ARMV8_PMU_INCLUDE_EL2;

		armv8pmu_write_evtype(idx - 1, hwc->config_base);
		armv8pmu_write_evtype(idx, chain_evt);
	} else {
		armv8pmu_write_evtype(idx, hwc->config_base);
	}
}

static inline int armv8pmu_enable_counter(int idx)
{
	u32 counter = ARMV8_IDX_TO_COUNTER(idx);
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r" (BIT(counter)));
	return idx;
}

static inline void armv8pmu_enable_event_counter(struct perf_event *event)
{
	int idx = event->hw.idx;
	armv8pmu_enable_counter(idx);
}

static inline int armv8pmu_disable_counter(int idx)
{
	u32 counter = ARMV8_IDX_TO_COUNTER(idx);
	asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r" (BIT(counter)));
	return idx;
}

static inline void armv8pmu_disable_event_counter(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	armv8pmu_disable_counter(idx);
}

static inline int armv8pmu_enable_intens(int idx)
{
	u32 counter = ARMV8_IDX_TO_COUNTER(idx);
	asm volatile("mcr p15, 0, %0, c9, c14, 1" : : "r" (BIT(counter)));
	return idx;
}

static inline int armv8pmu_enable_event_irq(struct perf_event *event)
{
	return armv8pmu_enable_intens(event->hw.idx);
}

static inline int armv8pmu_disable_intens(int idx)
{
	u32 counter = ARMV8_IDX_TO_COUNTER(idx);
	asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r" (BIT(counter)));
	isb();
	/* Clear the overflow flag in case an interrupt is pending. */
	asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r" (BIT(counter)));
	isb();

	return idx;
}

static inline int armv8pmu_disable_event_irq(struct perf_event *event)
{
	return armv8pmu_disable_intens(event->hw.idx);
}

static inline u32 armv8pmu_getreset_flags(void)
{
	u32 value;

	/* Read */
	asm volatile("mrc p15, 0, %0, c9, c12, 3" : "=r" (value));

	/* Write to clear flags */
	value &= ARMV8_PMU_OVSR_MASK;
	asm volatile("mcr p15, 0, %0, c9, c12, 3" : : "r" (value));

	return value;
}

static void armv8pmu_enable_event(struct perf_event *event)
{
	unsigned long flags;
	struct arm_pmu *cpu_pmu = to_arm_pmu(event->pmu);
	struct pmu_hw_events *events = this_cpu_ptr(cpu_pmu->hw_events);

	/*
	 * Enable counter and interrupt, and set the counter to count
	 * the event that we're interested in.
	 */
	raw_spin_lock_irqsave(&events->pmu_lock, flags);

	/*
	 * Disable counter
	 */
	armv8pmu_disable_event_counter(event);

	/*
	 * Set event (if destined for PMNx counters).
	 */
	armv8pmu_write_event_type(event);

	/*
	 * Enable interrupt for this counter
	 */
	armv8pmu_enable_event_irq(event);

	/*
	 * Enable counter
	 */
	armv8pmu_enable_event_counter(event);

	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static void armv8pmu_disable_event(struct perf_event *event)
{
	unsigned long flags;
	struct arm_pmu *cpu_pmu = to_arm_pmu(event->pmu);
	struct pmu_hw_events *events = this_cpu_ptr(cpu_pmu->hw_events);

	/*
	 * Disable counter and interrupt
	 */
	raw_spin_lock_irqsave(&events->pmu_lock, flags);

	/*
	 * Disable counter
	 */
	armv8pmu_disable_event_counter(event);

	/*
	 * Disable interrupt for this counter
	 */
	armv8pmu_disable_event_irq(event);

	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static void armv8pmu_start(struct arm_pmu *cpu_pmu)
{
	unsigned long flags;
	struct pmu_hw_events *events = this_cpu_ptr(cpu_pmu->hw_events);

	raw_spin_lock_irqsave(&events->pmu_lock, flags);
	/* Enable all counters */
	armv8pmu_pmcr_write(armv8pmu_pmcr_read() | ARMV8_PMU_PMCR_E);
	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static void armv8pmu_stop(struct arm_pmu *cpu_pmu)
{
	unsigned long flags;
	struct pmu_hw_events *events = this_cpu_ptr(cpu_pmu->hw_events);

	raw_spin_lock_irqsave(&events->pmu_lock, flags);
	/* Disable all counters */
	armv8pmu_pmcr_write(armv8pmu_pmcr_read() & ~ARMV8_PMU_PMCR_E);
	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static irqreturn_t armv8pmu_handle_irq(struct arm_pmu *cpu_pmu)
{
	u32 pmovsr;
	struct perf_sample_data data;
	struct pmu_hw_events *cpuc = this_cpu_ptr(cpu_pmu->hw_events);
	struct pt_regs *regs;
	int idx;

	/*
	 * Get and reset the IRQ flags
	 */
	pmovsr = armv8pmu_getreset_flags();

	/*
	 * Did an overflow occur?
	 */
	if (!armv8pmu_has_overflowed(pmovsr))
		return IRQ_NONE;

	/*
	 * Handle the counter(s) overflow(s)
	 */
	regs = get_irq_regs();

	/*
	 * Stop the PMU while processing the counter overflows
	 * to prevent skews in group events.
	 */
	armv8pmu_stop(cpu_pmu);
	for (idx = 0; idx < cpu_pmu->num_events; ++idx) {
		struct perf_event *event = cpuc->events[idx];
		struct hw_perf_event *hwc;

		/* Ignore if we don't have an event. */
		if (!event)
			continue;

		/*
		 * We have a single interrupt for all counters. Check that
		 * each counter has overflowed before we process it.
		 */
		if (!armv8pmu_counter_has_overflowed(pmovsr, idx))
			continue;

		hwc = &event->hw;
		armpmu_event_update(event);
		perf_sample_data_init(&data, 0, hwc->last_period);
		if (!armpmu_event_set_period(event))
			continue;

		if (perf_event_overflow(event, &data, regs))
			cpu_pmu->disable(event);
	}
	armv8pmu_start(cpu_pmu);

	/*
	 * Handle the pending perf events.
	 *
	 * Note: this call *must* be run with interrupts disabled. For
	 * platforms that can have the PMU interrupts raised as an NMI, this
	 * will not work.
	 */
	irq_work_run();

	return IRQ_HANDLED;
}

static int armv8pmu_get_single_idx(struct pmu_hw_events *cpuc,
				    struct arm_pmu *cpu_pmu)
{
	int idx;

	for (idx = ARMV8_IDX_COUNTER0; idx < cpu_pmu->num_events; idx++) {
		if (!test_and_set_bit(idx, cpuc->used_mask))
			return idx;
	}
	return -EAGAIN;
}

static int armv8pmu_get_chain_idx(struct pmu_hw_events *cpuc,
				   struct arm_pmu *cpu_pmu)
{
	int idx;

	/*
	 * Chaining requires two consecutive event counters, where
	 * the lower idx must be even.
	 */
	for (idx = ARMV8_IDX_COUNTER0 + 1; idx < cpu_pmu->num_events; idx += 2) {
		if (!test_and_set_bit(idx, cpuc->used_mask)) {
			/* Check if the preceding even counter is available */
			if (!test_and_set_bit(idx - 1, cpuc->used_mask))
				return idx;
			/* Release the Odd counter */
			clear_bit(idx, cpuc->used_mask);
		}
	}
	return -EAGAIN;
}

static int armv8pmu_get_event_idx(struct pmu_hw_events *cpuc,
				  struct perf_event *event)
{
	struct arm_pmu *cpu_pmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	unsigned long evtype = hwc->config_base & ARMV8_PMU_EVTYPE_EVENT;

	/* Always prefer to place a cycle counter into the cycle counter. */
	if (evtype == ARMV8_PMUV3_PERFCTR_CPU_CYCLES) {
		if (!test_and_set_bit(ARMV8_IDX_CYCLE_COUNTER, cpuc->used_mask))
			return ARMV8_IDX_CYCLE_COUNTER;
	}

	/*
	 * Otherwise use events counters
	 */
	if (armv8pmu_event_is_64bit(event))
		return	armv8pmu_get_chain_idx(cpuc, cpu_pmu);
	else
		return armv8pmu_get_single_idx(cpuc, cpu_pmu);
}

static void armv8pmu_clear_event_idx(struct pmu_hw_events *cpuc,
				     struct perf_event *event)
{
	int idx = event->hw.idx;

	clear_bit(idx, cpuc->used_mask);
	if (armv8pmu_event_is_chained(event))
		clear_bit(idx - 1, cpuc->used_mask);
}

/*
 * Add an event filter to a given event.
 */
static int armv8pmu_set_event_filter(struct hw_perf_event *event,
				     struct perf_event_attr *attr)
{
	unsigned long config_base = 0;

	if (attr->exclude_idle)
		return -EPERM;

	/*
	 * If we're running in hyp mode, then we *are* the hypervisor.
	 * Therefore we ignore exclude_hv in this configuration, since
	 * there's no hypervisor to sample anyway. This is consistent
	 * with other architectures (x86 and Power).
	 */
	if (is_kernel_in_hyp_mode()) {
		if (!attr->exclude_kernel && !attr->exclude_host)
			config_base |= ARMV8_PMU_INCLUDE_EL2;
		if (attr->exclude_guest)
			config_base |= ARMV8_PMU_EXCLUDE_EL1;
		if (attr->exclude_host)
			config_base |= ARMV8_PMU_EXCLUDE_EL0;
	} else {
		if (!attr->exclude_hv && !attr->exclude_host)
			config_base |= ARMV8_PMU_INCLUDE_EL2;
	}

	/*
	 * Filter out !VHE kernels and guest kernels
	 */
	if (attr->exclude_kernel)
		config_base |= ARMV8_PMU_EXCLUDE_EL1;

	if (attr->exclude_user)
		config_base |= ARMV8_PMU_EXCLUDE_EL0;

	/*
	 * Install the filter into config_base as this is used to
	 * construct the event type.
	 */
	event->config_base = config_base;

	return 0;
}

static int armv8pmu_filter_match(struct perf_event *event)
{
	unsigned long evtype = event->hw.config_base & ARMV8_PMU_EVTYPE_EVENT;
	return evtype != ARMV8_PMUV3_PERFCTR_CHAIN;
}

static void armv8pmu_reset(void *info)
{
	struct arm_pmu *cpu_pmu = (struct arm_pmu *)info;
	u32 idx, nb_cnt = cpu_pmu->num_events;

	/* The counter and interrupt enable registers are unknown at reset. */
	for (idx = ARMV8_IDX_CYCLE_COUNTER; idx < nb_cnt; ++idx) {
		armv8pmu_disable_counter(idx);
		armv8pmu_disable_intens(idx);
	}

	/*
	 * Initialize & Reset PMNC. Request overflow interrupt for
	 * 64 bit cycle counter but cheat in armv8pmu_write_counter().
	 */
	armv8pmu_pmcr_write(ARMV8_PMU_PMCR_P | ARMV8_PMU_PMCR_C |
			    ARMV8_PMU_PMCR_LC);
}

static int __armv8_pmuv3_map_event(struct perf_event *event,
				   const unsigned (*extra_event_map)
						  [PERF_COUNT_HW_MAX],
				   const unsigned (*extra_cache_map)
						  [PERF_COUNT_HW_CACHE_MAX]
						  [PERF_COUNT_HW_CACHE_OP_MAX]
						  [PERF_COUNT_HW_CACHE_RESULT_MAX])
{
	int hw_event_id;
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);

	hw_event_id = armpmu_map_event(event, &armv8_pmuv3_perf_map,
				       &armv8_pmuv3_perf_cache_map,
				       ARMV8_PMU_EVTYPE_EVENT);

	if (armv8pmu_event_is_64bit(event))
		event->hw.flags |= ARMPMU_EVT_64BIT;

	/* Only expose micro/arch events supported by this PMU */
	if ((hw_event_id > 0) && (hw_event_id < ARMV8_PMUV3_MAX_COMMON_EVENTS)
	    && test_bit(hw_event_id, armpmu->pmceid_bitmap)) {
		return hw_event_id;
	}

	return armpmu_map_event(event, extra_event_map, extra_cache_map,
				ARMV8_PMU_EVTYPE_EVENT);
}

static int armv8_pmuv3_map_event(struct perf_event *event)
{
	return __armv8_pmuv3_map_event(event, NULL, NULL);
}

static int armv8_a53_map_event(struct perf_event *event)
{
	return __armv8_pmuv3_map_event(event, NULL, &armv8_a53_perf_cache_map);
}

static int armv8_a57_map_event(struct perf_event *event)
{
	return __armv8_pmuv3_map_event(event, NULL, &armv8_a57_perf_cache_map);
}

static int armv8_a73_map_event(struct perf_event *event)
{
	return __armv8_pmuv3_map_event(event, NULL, &armv8_a73_perf_cache_map);
}

static int armv8_thunder_map_event(struct perf_event *event)
{
	return __armv8_pmuv3_map_event(event, NULL,
				       &armv8_thunder_perf_cache_map);
}

static int armv8_vulcan_map_event(struct perf_event *event)
{
	return __armv8_pmuv3_map_event(event, NULL,
				       &armv8_vulcan_perf_cache_map);
}

struct armv8pmu_probe_info {
	struct arm_pmu *pmu;
	bool present;
};

static void __armv8pmu_probe_pmu(void *info)
{
	struct armv8pmu_probe_info *probe = info;
	struct arm_pmu *cpu_pmu = probe->pmu;
	u32 dfr0;
	u32 pmceid[2];
	int pmuver;

	asm volatile("mrc p15, 0, %0, c0, c1, 2" : "=r" (dfr0));
	pmuver = (dfr0 & (0xf << 24)) >> 24;
	if (pmuver == 0xf || pmuver == 0)
		return;

	probe->present = true;

	/* Read the nb of CNTx counters supported from PMNC */
	cpu_pmu->num_events = (armv8pmu_pmcr_read() >> ARMV8_PMU_PMCR_N_SHIFT)
		& ARMV8_PMU_PMCR_N_MASK;

	/* Add the CPU cycles counter */
	cpu_pmu->num_events += 1;

	asm volatile("mrc p15, 0, %0, c9, c12, 6" : "=r" (pmceid[0]));
	asm volatile("mrc p15, 0, %0, c9, c12, 7" : "=r" (pmceid[1]));

	bitmap_from_arr32(cpu_pmu->pmceid_bitmap,
			     pmceid, ARMV8_PMUV3_MAX_COMMON_EVENTS);
}

static int armv8pmu_probe_pmu(struct arm_pmu *cpu_pmu)
{
	struct armv8pmu_probe_info probe = {
		.pmu = cpu_pmu,
		.present = false,
	};
	int ret;

	ret = smp_call_function_any(&cpu_pmu->supported_cpus,
				    __armv8pmu_probe_pmu,
				    &probe, 1);
	if (ret)
		return ret;

	return probe.present ? 0 : -ENODEV;
}

static int armv8_pmu_init(struct arm_pmu *cpu_pmu, char *name,
			  int (*map_event)(struct perf_event *event),
			  const struct attribute_group *events,
			  const struct attribute_group *format)
{
	int ret = armv8pmu_probe_pmu(cpu_pmu);
	if (ret)
		return ret;

	cpu_pmu->handle_irq		= armv8pmu_handle_irq;
	cpu_pmu->enable			= armv8pmu_enable_event;
	cpu_pmu->disable		= armv8pmu_disable_event;
	cpu_pmu->read_counter		= armv8pmu_read_counter;
	cpu_pmu->write_counter		= armv8pmu_write_counter;
	cpu_pmu->get_event_idx		= armv8pmu_get_event_idx;
	cpu_pmu->clear_event_idx	= armv8pmu_clear_event_idx;
	cpu_pmu->start			= armv8pmu_start;
	cpu_pmu->stop			= armv8pmu_stop;
	cpu_pmu->reset			= armv8pmu_reset;
	cpu_pmu->set_event_filter	= armv8pmu_set_event_filter;
	cpu_pmu->filter_match		= armv8pmu_filter_match;

	cpu_pmu->name			= name;
	cpu_pmu->map_event		= map_event;
	cpu_pmu->attr_groups[ARMPMU_ATTR_GROUP_EVENTS] = events ?
			events : &armv8_pmuv3_events_attr_group;
	cpu_pmu->attr_groups[ARMPMU_ATTR_GROUP_FORMATS] = format ?
			format : &armv8_pmuv3_format_attr_group;

	return 0;
}

static int armv8_pmuv3_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_pmuv3",
			      armv8_pmuv3_map_event, NULL, NULL);
}

static int armv8_a34_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cortex_a34",
			      armv8_pmuv3_map_event, NULL, NULL);
}

static int armv8_a35_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cortex_a35",
			      armv8_a53_map_event, NULL, NULL);
}

static int armv8_a53_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cortex_a53",
			      armv8_a53_map_event, NULL, NULL);
}

static int armv8_a55_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cortex_a55",
			      armv8_pmuv3_map_event, NULL, NULL);
}

static int armv8_a57_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cortex_a57",
			      armv8_a57_map_event, NULL, NULL);
}

static int armv8_a65_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cortex_a65",
			      armv8_pmuv3_map_event, NULL, NULL);
}

static int armv8_a72_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cortex_a72",
			      armv8_a57_map_event, NULL, NULL);
}

static int armv8_a73_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cortex_a73",
			      armv8_a73_map_event, NULL, NULL);
}

static int armv8_a75_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cortex_a75",
			      armv8_pmuv3_map_event, NULL, NULL);
}

static int armv8_a76_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cortex_a76",
			      armv8_pmuv3_map_event, NULL, NULL);
}

static int armv8_a77_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cortex_a77",
			      armv8_pmuv3_map_event, NULL, NULL);
}

static int armv8_e1_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_neoverse_e1",
			      armv8_pmuv3_map_event, NULL, NULL);
}

static int armv8_n1_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_neoverse_n1",
			      armv8_pmuv3_map_event, NULL, NULL);
}

static int armv8_thunder_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_cavium_thunder",
			      armv8_thunder_map_event, NULL, NULL);
}

static int armv8_vulcan_pmu_init(struct arm_pmu *cpu_pmu)
{
	return armv8_pmu_init(cpu_pmu, "armv8_brcm_vulcan",
			      armv8_vulcan_map_event, NULL, NULL);
}

static const struct of_device_id armv8_pmu_of_device_ids[] = {
	{.compatible = "arm,armv8-pmuv3",	.data = armv8_pmuv3_init},
	{.compatible = "arm,cortex-a34-pmu",	.data = armv8_a34_pmu_init},
	{.compatible = "arm,cortex-a35-pmu",	.data = armv8_a35_pmu_init},
	{.compatible = "arm,cortex-a53-pmu",	.data = armv8_a53_pmu_init},
	{.compatible = "arm,cortex-a55-pmu",	.data = armv8_a55_pmu_init},
	{.compatible = "arm,cortex-a57-pmu",	.data = armv8_a57_pmu_init},
	{.compatible = "arm,cortex-a65-pmu",	.data = armv8_a65_pmu_init},
	{.compatible = "arm,cortex-a72-pmu",	.data = armv8_a72_pmu_init},
	{.compatible = "arm,cortex-a73-pmu",	.data = armv8_a73_pmu_init},
	{.compatible = "arm,cortex-a75-pmu",	.data = armv8_a75_pmu_init},
	{.compatible = "arm,cortex-a76-pmu",	.data = armv8_a76_pmu_init},
	{.compatible = "arm,cortex-a77-pmu",	.data = armv8_a77_pmu_init},
	{.compatible = "arm,neoverse-e1-pmu",	.data = armv8_e1_pmu_init},
	{.compatible = "arm,neoverse-n1-pmu",	.data = armv8_n1_pmu_init},
	{.compatible = "cavium,thunder-pmu",	.data = armv8_thunder_pmu_init},
	{.compatible = "brcm,vulcan-pmu",	.data = armv8_vulcan_pmu_init},
	{},
};

static int armv8_pmu_device_probe(struct platform_device *pdev)
{
	return arm_pmu_device_probe(pdev, armv8_pmu_of_device_ids, NULL);
}

static struct platform_driver armv8_pmu_driver = {
	.driver		= {
		.name	= ARMV8_PMU_PDEV_NAME,
		.of_match_table = armv8_pmu_of_device_ids,
		.suppress_bind_attrs = true,
	},
	.probe		= armv8_pmu_device_probe,
};

static int __init armv8_pmu_driver_init(void)
{
	return platform_driver_register(&armv8_pmu_driver);
}
device_initcall(armv8_pmu_driver_init)

void arch_perf_update_userpage(struct perf_event *event,
			       struct perf_event_mmap_page *userpg, u64 now)
{
	u32 freq;
	u32 shift;

	/*
	 * Internal timekeeping for enabled/running/stopped times
	 * is always computed with the sched_clock.
	 */
	freq = arch_timer_get_rate();
	userpg->cap_user_time = 1;

	clocks_calc_mult_shift(&userpg->time_mult, &shift, freq,
			NSEC_PER_SEC, 0);
	/*
	 * time_shift is not expected to be greater than 31 due to
	 * the original published conversion algorithm shifting a
	 * 32-bit value (now specifies a 64-bit value) - refer
	 * perf_event_mmap_page documentation in perf_event.h.
	 */
	if (shift == 32) {
		shift = 31;
		userpg->time_mult >>= 1;
	}
	userpg->time_shift = (u16)shift;
	userpg->time_offset = -now;
}