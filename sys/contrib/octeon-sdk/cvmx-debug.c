/***********************license start***************
 * Copyright (c) 2003-2010  Cavium Networks (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.

 *   * Neither the name of Cavium Networks nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR
 * DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM
 * SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE,
 * MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
 * VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR
 * PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 ***********************license end**************************************/


/*
 * @file
 *
 * Interface to debug exception handler
 *
 * <hr>$Revision: 50060 $<hr>
 */

#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
#include <linux/module.h>
#include <asm/octeon/octeon.h>
#include <asm/octeon/cvmx.h>
#include <asm/octeon/cvmx-debug.h>
#include <asm/octeon/cvmx-core.h>
#include <asm/octeon/cvmx-bootmem.h>
#include <asm/octeon/octeon-boot-info.h>
#else
#include <stdint.h>
#include "executive-config.h"
#include "cvmx.h"
#include "cvmx-debug.h"
#include "cvmx-bootmem.h"
#include "cvmx-core.h"
#include "cvmx-coremask.h"

#ifndef __OCTEON_NEWLIB__
#include "../../bootloader/u-boot/include/octeon_mem_map.h"
#else
#include "octeon-boot-info.h"
#endif

#endif

#ifdef CVMX_DEBUG_LOGGING
# undef CVMX_DEBUG_LOGGING
# define CVMX_DEBUG_LOGGING 1
#else
# define CVMX_DEBUG_LOGGING 0
#endif

#ifndef CVMX_DEBUG_ATTACH
# define CVMX_DEBUG_ATTACH 1
#endif

#define CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_STATUS            (0xFFFFFFFFFF301000ull)
#define CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_ADDRESS(num)      (0xFFFFFFFFFF301100ull + 0x100 * (num))
#define CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_ADDRESS_MASK(num) (0xFFFFFFFFFF301108ull + 0x100 * (num))
#define CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_ASID(num)         (0xFFFFFFFFFF301110ull + 0x100 * (num))
#define CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_CONTROL(num)      (0xFFFFFFFFFF301118ull + 0x100 * (num))

#define CVMX_DEBUG_HW_DATA_BREAKPOINT_STATUS                   (0xFFFFFFFFFF302000ull)
#define CVMX_DEBUG_HW_DATA_BREAKPOINT_ADDRESS(num)             (0xFFFFFFFFFF302100ull + 0x100 * (num))
#define CVMX_DEBUG_HW_DATA_BREAKPOINT_ADDRESS_MASK(num)        (0xFFFFFFFFFF302108ull + 0x100 * (num))
#define CVMX_DEBUG_HW_DATA_BREAKPOINT_ASID(num)                (0xFFFFFFFFFF302110ull + 0x100 * (num))
#define CVMX_DEBUG_HW_DATA_BREAKPOINT_CONTROL(num)             (0xFFFFFFFFFF302118ull + 0x100 * (num))

#define ERET_INSN  0x42000018U      /* Hexcode for eret */
#define ISR_DELAY_COUNTER     120000000       /* Could be tuned down */

extern cvmx_debug_comm_t cvmx_debug_uart_comm;
extern cvmx_debug_comm_t cvmx_debug_remote_comm;
static const cvmx_debug_comm_t *cvmx_debug_comms[COMM_SIZE] = {&cvmx_debug_uart_comm, &cvmx_debug_remote_comm};



static cvmx_debug_globals_t *cvmx_debug_globals;

/**
 * @file
 *
 */

#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
uint64_t __cvmx_debug_save_regs_area[32];

volatile uint64_t __cvmx_debug_mode_exception_ignore;
volatile uint64_t __cvmx_debug_mode_exception_occured;

static char cvmx_debug_stack[8*1024] __attribute ((aligned (16)));
char *__cvmx_debug_stack_top = &cvmx_debug_stack[8*1024];

#ifndef __OCTEON_NEWLIB__
extern int cvmx_interrupt_in_isr;
#else
#define cvmx_interrupt_in_isr 0
#endif

#else
uint64_t __cvmx_debug_save_regs_area_all[OCTEON_NUM_CORES][32];
#define __cvmx_debug_save_regs_area __cvmx_debug_save_regs_area_all[cvmx_get_core_num()]

volatile uint64_t __cvmx_debug_mode_exception_ignore_all[OCTEON_NUM_CORES];
#define __cvmx_debug_mode_exception_ignore __cvmx_debug_mode_exception_ignore_all[cvmx_get_core_num()]
volatile uint64_t __cvmx_debug_mode_exception_occured_all[OCTEON_NUM_CORES];
#define __cvmx_debug_mode_exception_occured __cvmx_debug_mode_exception_occured_all[cvmx_get_core_num()]

static char cvmx_debug_stack_all[OCTEON_NUM_CORES][8*1024] __attribute ((aligned (16)));
char *__cvmx_debug_stack_top_all[OCTEON_NUM_CORES];

#define cvmx_interrupt_in_isr 0

#endif


static inline uint32_t cvmx_debug_core_mask(void)
{
#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
#ifdef __OCTEON_NEWLIB__
  extern int __octeon_core_mask;
  return __octeon_core_mask;
#endif
return cvmx_sysinfo_get()->core_mask;
#else
return octeon_get_boot_coremask ();
#endif
}

static inline void cvmx_debug_update_state(cvmx_debug_state_t state)
{
    memcpy(cvmx_debug_globals->state, &state, sizeof(cvmx_debug_state_t));
}

static inline cvmx_debug_state_t cvmx_debug_get_state(void)
{
    cvmx_debug_state_t state;
    memcpy(&state, cvmx_debug_globals->state, sizeof(cvmx_debug_state_t));
    return state;
}

static void cvmx_debug_printf(char *format, ...) __attribute__((format(__printf__, 1, 2)));
static void cvmx_debug_printf(char *format, ...)
{
    va_list ap;

    if (!CVMX_DEBUG_LOGGING)
        return;

    va_start(ap, format);
    cvmx_dvprintf(format, ap);
    va_end(ap);
}

static inline int __cvmx_debug_in_focus(cvmx_debug_state_t state, unsigned core)
{
    return state.focus_core == core;
}

static void cvmx_debug_install_handler(unsigned core)
{
    extern void __cvmx_debug_handler_stage2(void);
    int32_t *trampoline = CASTPTR(int32_t, CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0, BOOTLOADER_DEBUG_TRAMPOLINE_CORE));
    trampoline += core;

    *trampoline = (int32_t)(long)&__cvmx_debug_handler_stage2;

    cvmx_debug_printf("Debug handled installed on core %d at %p\n", core, trampoline);
}

static int cvmx_debug_enabled(void)
{
    return cvmx_debug_booted() || CVMX_DEBUG_ATTACH;
}

static void cvmx_debug_init_globals(void)
{
    int toclear = 0;
    uint64_t phys;
    void *a;

    if (cvmx_debug_globals)
        return;

    if (cvmx_get_core_num() != 0)
    {
        volatile size_t i;
        /* Delay here just enough for the writing of the version. */
        for(i = 0; i < sizeof(cvmx_debug_globals_t)/2 + 8; i++)
          ;
    }

    a = cvmx_bootmem_alloc_named(sizeof(cvmx_debug_globals_t), 8, CVMX_DEBUG_GLOBALS_BLOCK_NAME);
    if (a)
    {
       phys = cvmx_ptr_to_phys(a);
       toclear = 1;
    }
    else
    {
        const cvmx_bootmem_named_block_desc_t *debug_globals_nblk;
        debug_globals_nblk = cvmx_bootmem_find_named_block (CVMX_DEBUG_GLOBALS_BLOCK_NAME);
        phys = debug_globals_nblk->base_addr;
    }
    cvmx_debug_globals = CASTPTR(cvmx_debug_globals_t, CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0, phys));
    cvmx_debug_printf("Debug named block at %p\n", cvmx_debug_globals);
    if (toclear)
        cvmx_debug_printf("Debug named block cleared\n");

    if (toclear)
    {
        memset (cvmx_debug_globals, 0, sizeof(cvmx_debug_globals_t));
        cvmx_debug_globals->version = CVMX_DEBUG_GLOBALS_VERSION;
        cvmx_debug_globals->tlb_entries = cvmx_core_get_tlb_entries();
    }
    else
    {
        volatile size_t i;
        /* Delay here just enough for the writing of the version. */
        for(i = 0; i < sizeof(cvmx_debug_globals_t) + 8; i++)
          ;
    }
}


static void cvmx_debug_globals_check_version(void)
{
    if (cvmx_debug_globals->version != CVMX_DEBUG_GLOBALS_VERSION)
    {
        cvmx_dprintf("Wrong version on the globals struct spinining; expected %d, got:  %d.\n", (int)CVMX_DEBUG_GLOBALS_VERSION, (int)(cvmx_debug_globals->version));
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
        panic("Wrong version.\n");
#endif
        while (1)
            ;
    }
}

static inline volatile cvmx_debug_core_context_t *cvmx_debug_core_context(void);
static inline void cvmx_debug_save_core_context(volatile cvmx_debug_core_context_t *context);

void cvmx_debug_init(void)
{
    cvmx_debug_state_t state;
    int core;
    const cvmx_debug_comm_t *comm;
    cvmx_spinlock_t *lock;
    unsigned int coremask = cvmx_debug_core_mask();

    if (!cvmx_debug_enabled())
        return;

    cvmx_debug_init_globals();

#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
    // Put a barrier until all cores have got to this point.
    cvmx_coremask_barrier_sync(coremask);
#endif
    cvmx_debug_globals_check_version();


    comm = cvmx_debug_comms[cvmx_debug_globals->comm_type];
    lock = &cvmx_debug_globals->lock;

    core = cvmx_get_core_num();
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
    /*  Install the debugger handler on the cores. */
    {
        int core1 = 0;
        for (core1 = 0; core1 < OCTEON_NUM_CORES; core1++)
        {
            if ((1<<core1) & coremask)
                cvmx_debug_install_handler(core1);
        }
    }
#else
    cvmx_debug_install_handler(core);
#endif

    if (comm->init)
        comm->init();

    {
        cvmx_spinlock_lock(lock);
        state = cvmx_debug_get_state();
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
        state.known_cores |= coremask;
        state.core_finished &= ~coremask;
#else
        state.known_cores |= (1 << core);
        state.core_finished &= ~(1 << core);
#endif
        cvmx_debug_update_state(state);
        cvmx_spinlock_unlock(lock);
    }

#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
    // Put a barrier until all cores have got to this point.
    cvmx_coremask_barrier_sync(coremask);

    if (cvmx_coremask_first_core(coremask))
#endif
    {
        cvmx_debug_printf("cvmx_debug_init core: %d\n", core);
        state = cvmx_debug_get_state();
        state.focus_core = core;
        state.active_cores = state.known_cores;
        state.focus_switch = 1;
        state.step_isr = 1;
        cvmx_debug_printf("Known cores at init: 0x%x\n", (int)state.known_cores);
        cvmx_debug_update_state(state);

        /* Initialize __cvmx_debug_stack_top_all. */
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
        {
            int i;
            for (i = 0; i < OCTEON_NUM_CORES; i++)
                __cvmx_debug_stack_top_all[i] = &cvmx_debug_stack_all[i][8*1024];
        }
#endif
        cvmx_debug_globals->init_complete = 1;
        CVMX_SYNCW;
    }
    while (!cvmx_debug_globals->init_complete)
    {
        /* Spin waiting for init to complete */
    }

    if (cvmx_debug_booted())
        cvmx_debug_trigger_exception();

    /*  Install the break handler after might tripper the debugger exception. */
#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
    if (cvmx_coremask_first_core(coremask))
#endif
    {
        if (comm->install_break_handler)
            comm->install_break_handler();
    }
}

static int cvmx_debug_putpacket_noformat(char *packet);

static __attribute__ ((format (printf, 1, 2))) int cvmx_debug_putpacket(char *format, ...)
{
    va_list ap;
    size_t n;
    char packet[CVMX_DEBUG_MAX_RESPONSE_SIZE];

    if (cvmx_debug_comms[cvmx_debug_globals->comm_type]->putpacket == NULL)
        return 0;

    va_start(ap, format);
    n = vsnprintf(packet, sizeof(packet), format, ap);
    va_end(ap);

    if (n >= sizeof(packet))
    {
        cvmx_debug_printf("packet truncated (needed %d bytes): %s\n", (int)n, packet);
        return 0;
    }
    return cvmx_debug_putpacket_noformat(packet);
}

static int cvmx_debug_putpacket_noformat(char *packet)
{
    if (cvmx_debug_comms[cvmx_debug_globals->comm_type]->putpacket == NULL)
        return 0;
    cvmx_debug_printf("Reply: %s\n", packet);
    return cvmx_debug_comms[cvmx_debug_globals->comm_type]->putpacket(packet);
}

static int cvmx_debug_active_core(cvmx_debug_state_t state, int core)
{
    return state.active_cores & (1 << core);
}

static volatile cvmx_debug_core_context_t *cvmx_debug_core_context(void)
{
    return &cvmx_debug_globals->contextes[cvmx_get_core_num()];
}

static volatile uint64_t *cvmx_debug_regnum_to_context_ref(int regnum, volatile cvmx_debug_core_context_t *context)
{
    /* Must be kept in sync with mips_octeon_reg_names in gdb/mips-tdep.c. */
    if (regnum < 32)
        return &context->regs[regnum];
    switch (regnum)
    {
        case 32: return &context->cop0.status;
        case 33: return &context->lo;
        case 34: return &context->hi;
        case 35: return &context->cop0.badvaddr;
        case 36: return &context->cop0.cause;
        case 37: return &context->cop0.depc;
        default: return NULL;
    }
}

static int cvmx_debug_probe_load(unsigned char *ptr, unsigned char *result)
{
    volatile unsigned char *p = ptr;
    int ok;
    unsigned char tem;

    {
        __cvmx_debug_mode_exception_ignore = 1;
        __cvmx_debug_mode_exception_occured = 0;
        /* We don't handle debug-mode exceptions in delay slots.  Avoid them.  */
        asm volatile (".set push		\n\t"
                      ".set noreorder	\n\t"
                      "nop			\n\t"
                      "lbu %0, %1		\n\t"
                      "nop			\n\t"
                      ".set pop" : "=r"(tem) : "m"(*p));
        ok = __cvmx_debug_mode_exception_occured == 0;
        __cvmx_debug_mode_exception_ignore = 0;
        __cvmx_debug_mode_exception_occured = 0;
	*result = tem;
    }
    return ok;
}

static int cvmx_debug_probe_store(unsigned char *ptr)
{
    volatile unsigned char *p = ptr;
    int ok;

    __cvmx_debug_mode_exception_ignore = 1;
    __cvmx_debug_mode_exception_occured = 0;
    /* We don't handle debug-mode exceptions in delay slots.  Avoid them.  */
    asm volatile (".set push		\n\t"
                  ".set noreorder	\n\t"
                  "nop			\n\t"
                  "sb $0, %0		\n\t"
                  "nop			\n\t"
                  ".set pop" : "=m"(*p));
    ok = __cvmx_debug_mode_exception_occured == 0;

    __cvmx_debug_mode_exception_ignore = 0;
    __cvmx_debug_mode_exception_occured = 0;
    return ok;
}

/* Put the hex value of t into str. */
static void strhex(char *str, unsigned char t)
{
  char a[] = "0123456789ABCDEF";
  str[0] = a[(t>>4)];
  str[1] = a[t&0xF];
  str[2] = 0;
}

/**
  * Initialize the performance counter control registers.
  *
  */
static void cvmx_debug_set_perf_control_reg (int perf_event, int perf_counter)
{
    volatile cvmx_debug_core_context_t *context = cvmx_debug_core_context();
    cvmx_core_perf_control_t control;

    control.u32 = 0;
    control.s.u = 1;
    control.s.s = 1;
    control.s.k = 1;
    control.s.ex = 1;
    control.s.w = 1;
    control.s.m = 1 - perf_counter;
    control.s.event = perf_event;

    context->cop0.perfctrl[perf_counter] = control.u32;
}

static cvmx_debug_command_t cvmx_debug_process_packet(char *packet)
{
    const char *buf = packet;
    cvmx_debug_command_t result = COMMAND_NOP;
    cvmx_debug_state_t state = cvmx_debug_get_state();

    /* A one letter command code represents what to do.  */
    switch (*buf++)
    {
        case '?':   /* What protocol do I speak? */
            cvmx_debug_putpacket_noformat("S0A");
            break;

        case '\003':   /* Control-C */
            cvmx_debug_putpacket_noformat("T9");
            break;

        case 'F':   /* Change the focus core */
        {
            int core;
            sscanf(buf, "%x", &core);

            /* Only cores in the exception handler may become the focus.
            If a core not in the exception handler got focus the
            debugger would hang since nobody would talk to it.  */
            if (state.handler_cores & (1 << core))
            {
                /* Focus change reply must be sent before the focus
                changes. Otherwise the new focus core will eat our ACK
                from the debugger.  */
                cvmx_debug_putpacket("F%02x", core);
                cvmx_debug_comms[cvmx_debug_globals->comm_type]->change_core(state.focus_core, core);
                state.focus_core = core;
                cvmx_debug_update_state(state);
                break;
            }
            else
                cvmx_debug_putpacket_noformat("!Core is not in the exception handler. Focus not changed.");
        /* Nothing changed, so we send back the old value */
        }
        /* fall through */
        case 'f':   /* Get the focus core */
            cvmx_debug_putpacket("F%02x", (unsigned)state.focus_core);
            break;

        case 'J': /* Set the flag for skip-over-isr in Single-Stepping mode */
        {
            if (*buf == '1')
                state.step_isr = 1;   /* Step in ISR */
            else
                state.step_isr = 0;   /* Step over ISR */
            cvmx_debug_update_state(state);
        }
        /* Fall through. The reply to the set step-isr command is the
           same as the get step-isr command */

        case 'j':   /* Reply with step_isr status  */
            cvmx_debug_putpacket("J%x", (unsigned)state.step_isr);
            break;


        case 'I':   /* Set the active cores */
        {
            long long active_cores;
            sscanf(buf, "%llx", &active_cores);
            /* Limit the active mask to the known to exist cores */
            state.active_cores = active_cores & state.known_cores;

            /* Lazy user hack to have 0 be all cores */
            if (state.active_cores == 0)
                state.active_cores = state.known_cores;

            /* The focus core must be in the active_cores mask */
            if ((state.active_cores & (1 << state.focus_core)) == 0)
            {
                cvmx_debug_putpacket_noformat("!Focus core was added to the masked.");
                state.active_cores |= 1 << state.focus_core;
            }

            cvmx_debug_update_state(state);
        }
        /* Fall through. The reply to the set active cores command is the
           same as the get active cores command */

        case 'i':   /* Get the active cores */
            cvmx_debug_putpacket("I%llx", (long long) state.active_cores);
            break;

        case 'A':   /* Setting the step mode all or one */
        {
            if (*buf == '1')
                state.step_all = 1;   /* A step or continue will start all cores */
            else
                state.step_all = 0;   /* A step or continue only affects the focus core */
            cvmx_debug_update_state(state);
        }
        /* Fall through. The reply to the set step-all command is the
           same as the get step-all command */

        case 'a':   /* Getting the current step mode */
            cvmx_debug_putpacket("A%x", (unsigned)state.step_all);
            break;

        case 'g':   /* read a register from global place. */
        {
            volatile cvmx_debug_core_context_t *context = cvmx_debug_core_context();
            int regno;
            volatile uint64_t *reg;

            /* Get the register number to read */
            sscanf(buf, "%x", &regno);

            reg = cvmx_debug_regnum_to_context_ref(regno, context);
            if (!reg)
                cvmx_debug_printf("Register #%d is not valid\n", regno);
            cvmx_debug_putpacket("%llx", (unsigned long long) *reg);
        }
        break;

        case 'G':   /* set the value of a register. */
        {
            volatile cvmx_debug_core_context_t *context = cvmx_debug_core_context();
            int regno;
            volatile uint64_t *reg;
            long long value;

            /* Get the register number to read */
            if (sscanf(buf, "%x,%llx", &regno, &value) != 2)
            {
                cvmx_debug_printf("G packet corrupt: %s\n", buf);
                goto error_packet;
            }

            reg = cvmx_debug_regnum_to_context_ref(regno, context);
            if (!reg)
            {
                cvmx_debug_printf("Register #%d is not valid\n", regno);
                goto error_packet;
            }
            *reg = value;
        }
        break;

        case 'm':   /* Memory read. mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
        {
            long long addr, i, length;
            unsigned char *ptr;
            char *reply;

            if (sscanf(buf, "%llx,%llx", &addr, &length) != 2)
            {
                cvmx_debug_printf("m packet corrupt: %s\n", buf);
                goto error_packet;
            }
            if (length >= 1024)
            {
                cvmx_debug_printf("m packet length out of range: %lld\n", length);
                goto error_packet;
            }

            reply = __builtin_alloca(length * 2 + 1);
            ptr = (unsigned char *)(long)addr;
            for (i = 0; i < length; i++)
            {
                /* Probe memory.  If not accessible fail.   */
                unsigned char t;
                if (!cvmx_debug_probe_load(&ptr[i], &t))
                  goto error_packet;
                strhex(&reply[i * 2], t);
            }
            cvmx_debug_putpacket_noformat(reply);
        }
        break;

        case 'M':   /* Memory write. MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
        {
            long long addr, i, length;
            unsigned char *ptr;
            char value[1024];

            if (sscanf(buf, "%llx,%llx:%1024s", &addr, &length, value) != 3)
            {
                cvmx_debug_printf("M packet corrupt: %s\n", buf);
                goto error_packet;
            }
            
            ptr = (unsigned char *)(long)addr;
            for (i = 0; i < length; i++)
            {
                int c;
                int n;
                char tempstr[3] = {0, 0, 0};
                memcpy (tempstr, &value[i * 2], 2);
            
                n = sscanf(tempstr, "%2x", &c);
                if (n != 1)
                {
                    cvmx_debug_printf("M packet corrupt: %s\n", &value[i * 2]);
                    goto error_packet;
                }
                /* Probe memory.  If not accessible fail.   */
                if (!cvmx_debug_probe_store(&ptr[i]))
                {
                    cvmx_debug_printf("M cannot write: %p\n", &ptr[i]);
                    goto error_packet;
                }
                ptr[i] = c;
            }
            cvmx_debug_putpacket_noformat("+");
        }
        break;

        case 'e':  /* Set/get performance counter events. e[1234]XX..X: [01]
                      is the performance counter to set X is the performance
                      event.  [34] is to get the same thing.  */
        {
            int perf_event = 0;
            int counter, encoded_counter;
            volatile cvmx_debug_core_context_t *context = cvmx_debug_core_context();
            sscanf(buf, "%1d%x", &encoded_counter, &perf_event);

            switch (encoded_counter)
            {
                case 1: /* Set performance counter0 event. */
                case 2: /* Set performance counter1 event. */

                counter = encoded_counter - 1;
                context->cop0.perfval[counter] = 0;
                cvmx_debug_set_perf_control_reg(perf_event, counter);
                break;

                case 3: /* Get performance counter0 event. */
                case 4: /* Get performance counter1 event. */
                {
                    cvmx_core_perf_control_t c;
                    counter = encoded_counter - 3;
                    /* Pass performance counter0 event and counter to
                       the debugger.  */
                    c.u32 = context->cop0.perfctrl[counter];
                    cvmx_debug_putpacket("%llx,%llx", (long long) context->cop0.perfval[counter], (long long) c.s.event);
                }
                break;
            }
        }
        break;

#if 0
        case 't': /* Return the trace buffer read data register contents. */
        {
            uint64_t tra_data;
            uint64_t tra_ctl;
            char tmp[64];

            /* If trace buffer is disabled no trace data information is available. */
            if ((tra_ctl & 0x1) == 0)
            {
                cvmx_debug_putpacket_noformat("!Trace buffer not enabled\n");
                cvmx_debug_putpacket_noformat("t");
            }
            else
            {
                cvmx_debug_putpacket_noformat("!Trace buffer is enabled\n");
                tra_data = cvmx_read_csr(OCTEON_TRA_READ_DATA);
                mem2hex (&tra_data, tmp, 8);
                strcpy (debug_output_buffer, "t");
                strcat (debug_output_buffer, tmp);
                cvmx_debug_putpacket_noformat(debug_output_buffer);
            }
        }
        break;
#endif

        case 'Z': /* Insert hardware breakpoint: Z[di]NN..N,AA.A, [di] data or
                     instruction, NN..Nth at address AA..A */
        {
            enum type
            {
                WP_LOAD = 1,
                WP_STORE = 2,
                WP_ACCESS = 3
            };

            int num, size;
            long long addr;
            enum type type;
            char bp_type;
            const int BE = 1, TE = 4;
            int n;
            volatile cvmx_debug_core_context_t *context = cvmx_debug_core_context();

            n = sscanf(buf, "%c%x,%llx,%x,%x", &bp_type, &num, &addr, &size, &type);
            switch (bp_type)
            {
                case 'i':	// Instruction hardware breakpoint
                    if (n != 3 || num > 4)
                    {
                        cvmx_debug_printf("Z packet corrupt: %s\n", buf);
                        goto error_packet;
                    }

                    context->hw_ibp.address[num] = addr;
                    context->hw_ibp.address_mask[num] = 0;
                    context->hw_ibp.asid[num] = 0;
                    context->hw_ibp.control[num] = BE | TE;
                    break;

                case 'd':	// Data hardware breakpoint
                {
                    uint64_t dbc = 0xff0 | BE | TE;
                    uint64_t dbm;
                    if (n != 5 || num > 4)
                    {
                        cvmx_debug_printf("Z packet corrupt: %s\n", buf);
                        goto error_packet;
                    }

                    /* Set DBC[BE,TE,BLM]. */
                    context->hw_dbp.address[num] = addr;
                    context->hw_dbp.asid[num] = 0;

                    dbc |= type == WP_STORE ? 0x1000 : type == WP_LOAD ? 0x2000 : 0;
                    /* Mask the bits depending on the size for
                    debugger to stop while accessing parts of the
                    memory location.  */
                    dbm = (size == 8) ? 0x7 : ((size == 4) ? 3
                                        : (size == 2) ? 1 : 0);
                    context->hw_dbp.address_mask[num] = dbm;
                    context->hw_dbp.control[num] = dbc;
                    break;
                }
                default:
                    cvmx_debug_printf("z packet corrupt: %s\n", buf);
                    goto error_packet;
            }
        }
        break;

        case 'z': /* Remove hardware breakpoint: z[di]NN..N remove NN..Nth
breakpoint.  */
        {
            int num;
            char bp_type;
            volatile cvmx_debug_core_context_t *context = cvmx_debug_core_context();

            if (sscanf(buf, "%c%x", &bp_type, &num) != 2 || num > 4)
            {
                cvmx_debug_printf("z packet corrupt: %s\n", buf);
                goto error_packet;
            }

            switch (bp_type)
            {
                case 'i':	// Instruction hardware breakpoint
                    context->hw_ibp.address[num] = 0;
                    context->hw_ibp.address_mask[num] = 0;
                    context->hw_ibp.asid[num] = 0;
                    context->hw_ibp.control[num] = 0;
                    break;
                case 'd':	// Data hardware breakpoint
                    context->hw_dbp.address[num] = 0;
                    context->hw_dbp.address_mask[num] = 0;
                    context->hw_dbp.asid[num] = 0;
                    context->hw_dbp.control[num] = 0;
                    break;
                default:
                    cvmx_debug_printf("z packet corrupt: %s\n", buf);
                    goto error_packet;
            }
        }
        break;

        case 's':   /* Single step. sAA..AA Step one instruction from AA..AA (optional) */
            result = COMMAND_STEP;
            break;

        case 'c':   /* Continue. cAA..AA Continue at address AA..AA (optional) */
            result = COMMAND_CONTINUE;
            break;

        case '+':   /* Don't know. I think it is a communications sync */
            /* Ignoring this command */
            break;

        default:
            cvmx_debug_printf("Unknown debug command: %s\n", buf - 1);
error_packet:
            cvmx_debug_putpacket_noformat("-");
            break;
    }

    return result;
}

static cvmx_debug_command_t cvmx_debug_process_next_packet(void)
{
    char packet[CVMX_DEBUG_MAX_REQUEST_SIZE];
    if (cvmx_debug_comms[cvmx_debug_globals->comm_type]->getpacket(packet, CVMX_DEBUG_MAX_REQUEST_SIZE))
    {
        cvmx_debug_printf("Request: %s\n", packet);
        return cvmx_debug_process_packet(packet);
    }
    return COMMAND_NOP;
}

/* If a core isn't in the active core mask we need to start him up again. We
   can only do this if the core didn't hit a breakpoint or single step. If the
   core hit CVMX_CIU_DINT interrupt (generally happens when while executing
   _exit() at the end of the program). Remove the core from known cores so
   that when the cores in active core mask are done executing the program, the
   focus will not be transfered to this core.  */

static int cvmx_debug_stop_core(cvmx_debug_state_t state, unsigned core, cvmx_debug_register_t *debug_reg, int proxy)
{
    if (!cvmx_debug_active_core(state, core) && !debug_reg->s.dbp && !debug_reg->s.dss && (debug_reg->s.dint != 1))
    {
        debug_reg->s.sst = 0;
        cvmx_debug_printf("Core #%d not in active cores, continuing.\n", core);
        return 0;
    }
    if ((state.core_finished & (1<<core)) && proxy)
      return 0;
    return 1;
}

/* check to see if current exc is single-stepped and  that no other exc
   was also simultaneously noticed. */
static int cvmx_debug_single_step_exc(cvmx_debug_register_t *debug_reg)
{
    if (debug_reg->s.dss && !debug_reg->s.dib && !debug_reg->s.dbp && !debug_reg->s.ddbs && !debug_reg->s.ddbl)
        return 1;
    return 0;
}

static void cvmx_debug_set_focus_core(cvmx_debug_state_t *state, int core)
{
    if (state->ever_been_in_debug)
        cvmx_debug_putpacket("!Core %2x taking focus.", core);
    cvmx_debug_comms[cvmx_debug_globals->comm_type]->change_core (state->focus_core, core);
    state->focus_core = core;
}

static void cvmx_debug_may_elect_as_focus_core(cvmx_debug_state_t *state, int core, cvmx_debug_register_t *debug_reg)
{
    /* If another core has already elected itself as the focus core, we're late.  */
    if (state->handler_cores & (1 << state->focus_core))
        return;

    /* If we hit a breakpoint, elect ourselves.  */
    if (debug_reg->s.dib || debug_reg->s.dbp || debug_reg->s.ddbs || debug_reg->s.ddbl)
        cvmx_debug_set_focus_core(state, core);

    /* It is possible the focus core has completed processing and exited the
       program. When this happens the focus core will not be in
       known_cores. If this is the case we need to elect a new focus. */
    if ((state->known_cores & (1 << state->focus_core)) == 0)
        cvmx_debug_set_focus_core(state, core);
}

static void cvmx_debug_send_stop_reason(cvmx_debug_register_t *debug_reg, volatile cvmx_debug_core_context_t *context)
{
    /* Handle Debug Data Breakpoint Store/Load Exception. */
    if (debug_reg->s.ddbs || debug_reg->s.ddbl)
        cvmx_debug_putpacket("T8:%x", (int) context->hw_dbp.status);
    else
        cvmx_debug_putpacket_noformat("T9");
}


static void cvmx_debug_clear_status(volatile cvmx_debug_core_context_t *context)
{
    /* SW needs to clear the BreakStatus bits after a watchpoint is hit or on
       reset.  */
    context->hw_dbp.status &= ~0x3fff;

    /* Clear MCD0, which is write-1-to-clear.  */
    context->cop0.multicoredebug |= 1;
}

static void cvmx_debug_sync_up_cores(void)
{
    cvmx_debug_state_t state;
    do {
        state = cvmx_debug_get_state();
    } while (state.step_all && state.handler_cores != 0);
}

/* Delay the focus core a little if it is likely another core needs to steal
   focus. Once we enter the main loop focus can't be stolen */
static void cvmx_debug_delay_focus_core(cvmx_debug_state_t state, unsigned core, cvmx_debug_register_t *debug_reg)
{
    volatile int i;
    if (debug_reg->s.dss || debug_reg->s.dbp || core != state.focus_core)
        return;
    for (i = 0; i < 24000; i++)
    {
        asm volatile (".set push		\n\t"
                      ".set noreorder		\n\t"
                      "nop			\n\t"
                      "nop			\n\t"
                      "nop			\n\t"
                      "nop			\n\t"
                      ".set pop");
        /* Spin giving the breakpoint core time to steal focus */
    }

}

/* If this core was single-stepping in a group,
   && it was not the last focus-core,
   && last focus-core happens to be inside an ISR, blocking focus-switch
   then burn some cycles, to avoid unnecessary focus toggles. */
static void cvmx_debug_delay_isr_core(unsigned core, uint32_t depc, int single_stepped_exc_only,
                                      cvmx_debug_state_t state)
{
    volatile uint64_t i;
    if(!single_stepped_exc_only || state.step_isr || core == state.focus_core || state.focus_switch)
        return;

    cvmx_debug_printf ("Core #%u spinning for focus at 0x%x\n", core, (unsigned int)depc);

    for(i = ISR_DELAY_COUNTER; i > 0 ; i--)
    {
       state = cvmx_debug_get_state();
       /* Spin giving the focus core time to service ISR */
       /* But cut short the loop, if we can.  Shrink down i, only once. */
       if (i > 600000 && state.focus_switch)
           i = 500000;
    }
    
}

static int cvmx_debug_perform_proxy(cvmx_debug_register_t *debug_reg, volatile cvmx_debug_core_context_t *context)
{
    unsigned core = cvmx_get_core_num();
    cvmx_debug_state_t state = cvmx_debug_get_state();
    cvmx_debug_command_t command = COMMAND_NOP;
    int single_stepped_exc_only = cvmx_debug_single_step_exc (debug_reg);

    /* All cores should respect the focus core if it has to
       stop focus switching while servicing an interrupt.
       If the system is single-stepping, then the following
       code path is valid. If the current core tripped on a
       break-point or some other error while going through
       an ISR, then we shouldn't be returning unconditionally.
       In that case (non-single-step case) we must enter
       the debugger exception stub fully. */
    if (!state.step_isr && (cvmx_interrupt_in_isr || (context->cop0.status & 0x2ULL)) && single_stepped_exc_only)
    {
        cvmx_spinlock_lock(&cvmx_debug_globals->lock);
        state = cvmx_debug_get_state();
        /* If this is the focus core, switch off focus switching
           till ISR_DELAY_COUNTER. This will let focus core
           keep the focus until the ISR is completed. */
        if(state.focus_switch && core == state.focus_core)
        {
            cvmx_debug_printf ("Core #%u stopped focus stealing at 0x%llx\n", core, (unsigned long long)context->cop0.depc);
            state.focus_switch = 0;
        }
        /* Alow other cores to steal focus.
           Focus core has completed ISR. */
        if (*(uint32_t*)((__SIZE_TYPE__)context->cop0.depc) == ERET_INSN && core == state.focus_core)
        {
            cvmx_debug_printf ("Core #%u resumed focus stealing at 0x%llx\n", core, (unsigned long long)context->cop0.depc);
            state.focus_switch = 1;
        }
        cvmx_debug_update_state(state);
        cvmx_spinlock_unlock(&cvmx_debug_globals->lock);
        cvmx_debug_printf ("Core #%u resumed skipping isr.\n", core);
        return 0;
    }

    /* Delay the focus core a little if it is likely another core needs to
       steal focus. Once we enter the main loop focus can't be stolen */
    cvmx_debug_delay_focus_core(state, core, debug_reg);

    cvmx_debug_delay_isr_core (core, context->cop0.depc, single_stepped_exc_only, state);

    /* The following section of code does two critical things. First, it
       populates the handler_cores bitmask of all cores in the exception
       handler. Only one core at a time can update this field. Second it
       changes the focus core if needed. */
    {
        cvmx_debug_printf("Core #%d stopped\n", core);
        cvmx_spinlock_lock(&cvmx_debug_globals->lock);
        state = cvmx_debug_get_state();

        state.handler_cores |= (1 << core);
        cvmx_debug_may_elect_as_focus_core(&state, core, debug_reg);

/* Push all updates before exiting the critical section */
        state.focus_switch = 1;
        cvmx_debug_update_state(state);
        cvmx_spinlock_unlock(&cvmx_debug_globals->lock);
    }
    if (__cvmx_debug_in_focus(state, core))
        cvmx_debug_send_stop_reason(debug_reg, context);

    do {
        state = cvmx_debug_get_state();
        /* Note the focus core can change in this loop. */
        if (__cvmx_debug_in_focus(state, core))
        {
            command = cvmx_debug_process_next_packet();
            state = cvmx_debug_get_state();
            /* When resuming let the other cores resume as well with
               step-all.  */
            if (command != COMMAND_NOP && state.step_all)
            {
                state.command = command;
                cvmx_debug_update_state(state);
            }
        }
        /* When steping all cores, update the non focus core's command too. */
        else if (state.step_all)
            command = state.command;

        /* If we did not get a command and the communication changed return,
           we are changing the communications. */
        if (command == COMMAND_NOP && cvmx_debug_globals->comm_changed)
        {
            /* FIXME, this should a sync not based on cvmx_coremask_barrier_sync.  */
#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
            /* Sync up.  */
            cvmx_coremask_barrier_sync(state.handler_cores);
#endif
            return 1;
        }
    } while (command == COMMAND_NOP);

    debug_reg->s.sst = command == COMMAND_STEP;
    cvmx_debug_printf("Core #%d running\n", core);

    {
        cvmx_spinlock_lock(&cvmx_debug_globals->lock);
        state = cvmx_debug_get_state();
        state.handler_cores ^= (1 << core);
        cvmx_debug_update_state(state);
        cvmx_spinlock_unlock(&cvmx_debug_globals->lock);
    }

    cvmx_debug_sync_up_cores();
    /* Now that all cores are out, reset the command.  */
    if (__cvmx_debug_in_focus(state, core))
    {
        cvmx_spinlock_lock(&cvmx_debug_globals->lock);
        state = cvmx_debug_get_state();
        state.command = COMMAND_NOP;
        cvmx_debug_update_state(state);
        cvmx_spinlock_unlock(&cvmx_debug_globals->lock);
    }
    return 0;
}

static void cvmx_debug_save_core_context(volatile cvmx_debug_core_context_t *context)
{
    unsigned i;
    memcpy((char *) context->regs, __cvmx_debug_save_regs_area, sizeof(context->regs));
    asm("mflo %0" : "=r"(context->lo));
    asm("mfhi %0" : "=r"(context->hi));
    CVMX_MF_COP0(context->cop0.index, COP0_INDEX);
    CVMX_MF_COP0(context->cop0.entrylo[0], COP0_ENTRYLO0);
    CVMX_MF_COP0(context->cop0.entrylo[1], COP0_ENTRYLO1);
    CVMX_MF_COP0(context->cop0.entryhi, COP0_ENTRYHI);
    CVMX_MF_COP0(context->cop0.pagemask, COP0_PAGEMASK);
    CVMX_MF_COP0(context->cop0.status, COP0_STATUS);
    CVMX_MF_COP0(context->cop0.cause, COP0_CAUSE);
    CVMX_MF_COP0(context->cop0.debug, COP0_DEBUG);
    CVMX_MF_COP0(context->cop0.multicoredebug, COP0_MULTICOREDEBUG);
    CVMX_MF_COP0(context->cop0.perfval[0], COP0_PERFVALUE0);
    CVMX_MF_COP0(context->cop0.perfval[1], COP0_PERFVALUE1);
    CVMX_MF_COP0(context->cop0.perfctrl[0], COP0_PERFCONTROL0);
    CVMX_MF_COP0(context->cop0.perfctrl[1], COP0_PERFCONTROL1);
    /* Save DEPC and DESAVE since debug-mode exceptions (see
       debug_probe_{load,store}) can clobber these.  */
    CVMX_MF_COP0(context->cop0.depc, COP0_DEPC);
    CVMX_MF_COP0(context->cop0.desave, COP0_DESAVE);

    context->hw_ibp.status = cvmx_read_csr(CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_STATUS);
    for (i = 0; i < 4; i++)
    {
        context->hw_ibp.address[i] = cvmx_read_csr(CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_ADDRESS(i));
        context->hw_ibp.address_mask[i] = cvmx_read_csr(CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_ADDRESS_MASK(i));
        context->hw_ibp.asid[i] = cvmx_read_csr(CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_ASID(i));
        context->hw_ibp.control[i] = cvmx_read_csr(CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_CONTROL(i));
    }

    context->hw_dbp.status = cvmx_read_csr(CVMX_DEBUG_HW_DATA_BREAKPOINT_STATUS);
    for (i = 0; i < 4; i++)
    {
        context->hw_dbp.address[i] = cvmx_read_csr(CVMX_DEBUG_HW_DATA_BREAKPOINT_ADDRESS(i));
        context->hw_dbp.address_mask[i] = cvmx_read_csr(CVMX_DEBUG_HW_DATA_BREAKPOINT_ADDRESS_MASK(i));
        context->hw_dbp.asid[i] = cvmx_read_csr(CVMX_DEBUG_HW_DATA_BREAKPOINT_ASID(i));
        context->hw_dbp.control[i] = cvmx_read_csr(CVMX_DEBUG_HW_DATA_BREAKPOINT_CONTROL(i));
    }

    for (i = 0; i < cvmx_debug_globals->tlb_entries; i++)
    {
        CVMX_MT_COP0(i, COP0_INDEX);
        asm volatile ("tlbr");
        CVMX_MF_COP0(context->tlbs[i].entrylo[0], COP0_ENTRYLO0);
        CVMX_MF_COP0(context->tlbs[i].entrylo[1], COP0_ENTRYLO1);
        CVMX_MF_COP0(context->tlbs[i].entryhi, COP0_ENTRYHI);
        CVMX_MF_COP0(context->tlbs[i].pagemask, COP0_PAGEMASK);
    }
    CVMX_SYNCW;
}

static void cvmx_debug_restore_core_context(volatile cvmx_debug_core_context_t *context)
{
    int i;
    memcpy(__cvmx_debug_save_regs_area, (char *) context->regs, sizeof(context->regs));
    asm("mtlo %0" :: "r"(context->lo));
    asm("mthi %0" :: "r"(context->hi));
    /* We don't change the TLB so no need to restore it.  */
    cvmx_write_csr(CVMX_DEBUG_HW_DATA_BREAKPOINT_STATUS, context->hw_dbp.status);
    for (i = 0; i < 4; i++)
    {
        cvmx_write_csr(CVMX_DEBUG_HW_DATA_BREAKPOINT_ADDRESS(i), context->hw_dbp.address[i]);
        cvmx_write_csr(CVMX_DEBUG_HW_DATA_BREAKPOINT_ADDRESS_MASK(i), context->hw_dbp.address_mask[i]);
        cvmx_write_csr(CVMX_DEBUG_HW_DATA_BREAKPOINT_ASID(i), context->hw_dbp.asid[i]);
        cvmx_write_csr(CVMX_DEBUG_HW_DATA_BREAKPOINT_CONTROL(i), context->hw_dbp.control[i]);
    }
    cvmx_write_csr(CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_STATUS, context->hw_ibp.status);
    for (i = 0; i < 4; i++)
    {
        cvmx_write_csr(CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_ADDRESS(i), context->hw_ibp.address[i]);
        cvmx_write_csr(CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_ADDRESS_MASK(i), context->hw_ibp.address_mask[i]);
        cvmx_write_csr(CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_ASID(i), context->hw_ibp.asid[i]);
        cvmx_write_csr(CVMX_DEBUG_HW_INSTRUCTION_BREAKPOINT_CONTROL(i), context->hw_ibp.control[i]);
    }
    CVMX_MT_COP0(context->cop0.index, COP0_INDEX);
    CVMX_MT_COP0(context->cop0.entrylo[0], COP0_ENTRYLO0);
    CVMX_MT_COP0(context->cop0.entrylo[1], COP0_ENTRYLO1);
    CVMX_MT_COP0(context->cop0.entryhi, COP0_ENTRYHI);
    CVMX_MT_COP0(context->cop0.pagemask, COP0_PAGEMASK);
    CVMX_MT_COP0(context->cop0.status, COP0_STATUS);
    CVMX_MT_COP0(context->cop0.cause, COP0_CAUSE);
    CVMX_MT_COP0(context->cop0.debug, COP0_DEBUG);
    CVMX_MT_COP0(context->cop0.multicoredebug, COP0_MULTICOREDEBUG);
    CVMX_MT_COP0(context->cop0.perfval[0], COP0_PERFVALUE0);
    CVMX_MT_COP0(context->cop0.perfval[1], COP0_PERFVALUE1);
    CVMX_MT_COP0(context->cop0.perfctrl[0], COP0_PERFCONTROL0);
    CVMX_MT_COP0(context->cop0.perfctrl[1], COP0_PERFCONTROL1);
    CVMX_MT_COP0(context->cop0.depc, COP0_DEPC);
    CVMX_MT_COP0(context->cop0.desave, COP0_DESAVE);
}

static inline void cvmx_debug_print_cause(volatile cvmx_debug_core_context_t *context)
{
    if (!CVMX_DEBUG_LOGGING)
        return;
    if (context->cop0.multicoredebug & 1)
        cvmx_dprintf("MCD0 was pulsed\n");
    if (context->cop0.multicoredebug & (1 << 16))
        cvmx_dprintf("Exception %lld in Debug Mode\n", (long long)((context->cop0.debug >> 10) & 0x1f));
    if (context->cop0.debug & (1 << 19))
        cvmx_dprintf("DDBSImpr\n");
    if (context->cop0.debug & (1 << 18))
        cvmx_dprintf("DDBLImpr\n");
    if (context->cop0.debug & (1 << 5))
        cvmx_dprintf("DINT\n");
    if (context->cop0.debug & (1 << 4))
        cvmx_dprintf("Debug Instruction Breakpoint (DIB) exception\n");
    if (context->cop0.debug & (1 << 3))
        cvmx_dprintf("Debug Date Break Store (DDBS) exception\n");
    if (context->cop0.debug & (1 << 2))
        cvmx_dprintf("Debug Date Break Load (DDBL) exception\n");
    if (context->cop0.debug & (1 << 1))
        cvmx_dprintf("Debug Breakpoint (DBp) exception\n");
    if (context->cop0.debug & (1 << 0))
        cvmx_dprintf("Debug Single Step (DSS) exception\n");
}

void __cvmx_debug_handler_stage3 (void)
{
    volatile cvmx_debug_core_context_t *context;
    int comms_changed = 0;

    cvmx_debug_printf("Entering debug exception handler\n");
    cvmx_debug_printf("Debug named block at %p\n", cvmx_debug_globals);
    if (__cvmx_debug_mode_exception_occured)
    {
        uint64_t depc;
        CVMX_MF_COP0(depc, COP0_DEPC);
        cvmx_dprintf("Unexpected debug-mode exception occured at 0x%llx, 0x%llx spinning\n", (long long) depc, (long long)(__cvmx_debug_mode_exception_occured));
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
        panic("Unexpected debug-mode exception occured at 0x%llx, 0x%llx\n", (long long) depc, (long long)(__cvmx_debug_mode_exception_occured));
#endif
        while (1)
            ;
    }

    context = cvmx_debug_core_context();
    cvmx_debug_save_core_context(context);

    {
        cvmx_debug_state_t state;
        cvmx_spinlock_lock(&cvmx_debug_globals->lock);
        state = cvmx_debug_get_state();
        state.ever_been_in_debug = 1;
        cvmx_debug_update_state (state);
        cvmx_spinlock_unlock(&cvmx_debug_globals->lock);
    }
    cvmx_debug_print_cause(context);

    do
    {
        int needs_proxy;
        comms_changed = 0;
        /* If the communication changes, change it. */
        cvmx_spinlock_lock(&cvmx_debug_globals->lock);
        if (cvmx_debug_globals->comm_changed)
        {
            cvmx_debug_printf("Communication changed: %d\n", (int)cvmx_debug_globals->comm_changed);
            if (cvmx_debug_globals->comm_changed > COMM_SIZE)
            {
                cvmx_dprintf("Unknown communication spinning: %lld > %d.\n", (long long)cvmx_debug_globals->comm_changed, (int)(COMM_SIZE));
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
                panic("Unknown communication.\n");
#endif
                while (1)
                    ;
            }
            cvmx_debug_globals->comm_type = cvmx_debug_globals->comm_changed - 1;
            cvmx_debug_globals->comm_changed = 0;
        }
        cvmx_spinlock_unlock(&cvmx_debug_globals->lock);
        needs_proxy = cvmx_debug_comms[cvmx_debug_globals->comm_type]->needs_proxy;

        {
            cvmx_debug_register_t debug_reg;
            cvmx_debug_state_t state;
            unsigned core = cvmx_get_core_num();

            state = cvmx_debug_get_state();
            debug_reg.u64 = context->cop0.debug;
            /* All cores stop on any exception.  See if we want nothing from this and
               it should resume.  This needs to be done for non proxy based debugging
               so that some non active-cores can control the other cores.  */
            if (!cvmx_debug_stop_core(state, core, &debug_reg, needs_proxy))
            {
                context->cop0.debug = debug_reg.u64;
                break;
            }
        }

        if (needs_proxy)
        {
            cvmx_debug_register_t debug_reg;
            debug_reg.u64 = context->cop0.debug;
            cvmx_debug_printf("Starting to proxy\n");
            comms_changed = cvmx_debug_perform_proxy(&debug_reg, context);
            context->cop0.debug = debug_reg.u64;
        }
        else
        {
            cvmx_debug_printf("Starting to wait for remote host\n");
            cvmx_debug_comms[cvmx_debug_globals->comm_type]->wait_for_resume(context, cvmx_debug_get_state());
        }
    } while (comms_changed);

    cvmx_debug_clear_status(context);

    cvmx_debug_restore_core_context(context);
    cvmx_debug_printf("Exiting debug exception handler\n");
}

void cvmx_debug_trigger_exception(void)
{
  /* Set CVMX_CIU_DINT to enter debug exception handler.  */
  cvmx_write_csr (CVMX_CIU_DINT, 1 << cvmx_get_core_num ());
  /* Perform an immediate read after every write to an RSL register to force
     the write to complete. It doesn't matter what RSL read we do, so we
     choose CVMX_MIO_BOOT_BIST_STAT because it is fast and harmless */
  cvmx_read_csr (CVMX_MIO_BOOT_BIST_STAT);
}

/**
 * Inform debugger about the end of the program. This is
 * called from crt0 after all the C cleanup code finishes.
 * Our current stack is the C one, not the debug exception
 * stack. */
void cvmx_debug_finish(void)
{
    unsigned coreid = cvmx_get_core_num();
    cvmx_debug_state_t state;

    cvmx_debug_printf ("Debug _exit reached!, core %d, cvmx_debug_globals = %p\n", coreid, cvmx_debug_globals);

#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
    fflush (stdout);
    fflush (stderr);
#endif

    cvmx_spinlock_lock(&cvmx_debug_globals->lock);
    state = cvmx_debug_get_state();
    state.known_cores ^= (1 << coreid);
    state.core_finished |= (1<<coreid);
    cvmx_debug_update_state(state);

    /* Tell the user the core has finished. */
    if (state.ever_been_in_debug)
        cvmx_debug_putpacket("!Core %d finish.", coreid);

    /* Notify the debugger if all cores have completed the program */
    if ((cvmx_debug_core_mask () & state.core_finished) == cvmx_debug_core_mask ())
    {
        cvmx_debug_printf("All cores done!\n");
        if (state.ever_been_in_debug)
            cvmx_debug_putpacket_noformat("D0");
    }
    if (state.focus_core == coreid && state.known_cores != 0)
    {
        /* Loop through cores looking for someone to handle interrupts.
           Since we already check that known_cores is non zero, this
           should always find a core */
        unsigned newcore;
        for (newcore = 0; newcore < CVMX_DEBUG_MAX_CORES; newcore++)
        {
           if (state.known_cores & (1<<newcore))
           {
               cvmx_debug_printf("Routing uart interrupts to Core #%u.\n", newcore);
               cvmx_debug_set_focus_core(&state, newcore);
               cvmx_debug_update_state(state);
               break;
            }
        }
    }
    cvmx_spinlock_unlock(&cvmx_debug_globals->lock);

    /* If we ever been in the debug, report to it that we have exited the core. */
    if (state.ever_been_in_debug)
        cvmx_debug_trigger_exception();
}
