/*-
 * Copyright (c) 1989, 1990 William F. Jolitz.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: vector.s, 386BSD 0.1 unknown origin
 * $FreeBSD$
 */

/*
 * Interrupt entry points for external interrupts triggered by I/O APICs
 * as well as IPI handlers.
 */

#include "opt_smp.h"

#include <machine/asmacros.h>
#include <x86/apicreg.h>

#include "assym.s"

/*
 * I/O Interrupt Entry Point.  Rather than having one entry point for
 * each interrupt source, we use one entry point for each 32-bit word
 * in the ISR.  The handler determines the highest bit set in the ISR,
 * translates that into a vector, and passes the vector to the
 * lapic_handle_intr() function.
 */
#define	ISR_VEC(index, vec_name)					\
	.text ;								\
	SUPERALIGN_TEXT ;						\
IDTVEC(vec_name) ;							\
	PUSH_FRAME ;							\
	SET_KERNEL_SREGS ;						\
	cld ;								\
	FAKE_MCOUNT(TF_EIP(%esp)) ;					\
	movl	lapic, %edx ;	/* pointer to local APIC */		\
	movl	LA_ISR + 16 * (index)(%edx), %eax ;	/* load ISR */	\
	bsrl	%eax, %eax ;	/* index of highest set bit in ISR */	\
	jz	1f ;							\
	addl	$(32 * index),%eax ;					\
	pushl	%esp		;                                       \
	pushl	%eax ;		/* pass the IRQ */			\
	call	lapic_handle_intr ;					\
	addl	$8, %esp ;	/* discard parameter */			\
1: ;									\
	MEXITCOUNT ;							\
	jmp	doreti

/*
 * Handle "spurious INTerrupts".
 * Notes:
 *  This is different than the "spurious INTerrupt" generated by an
 *   8259 PIC for missing INTs.  See the APIC documentation for details.
 *  This routine should NOT do an 'EOI' cycle.
 */
	.text
	SUPERALIGN_TEXT
IDTVEC(spuriousint)

	/* No EOI cycle used here */

	iret

	ISR_VEC(1, apic_isr1)
	ISR_VEC(2, apic_isr2)
	ISR_VEC(3, apic_isr3)
	ISR_VEC(4, apic_isr4)
	ISR_VEC(5, apic_isr5)
	ISR_VEC(6, apic_isr6)
	ISR_VEC(7, apic_isr7)

/*
 * Local APIC periodic timer handler.
 */
	.text
	SUPERALIGN_TEXT
IDTVEC(timerint)
	PUSH_FRAME
	SET_KERNEL_SREGS
	cld
	FAKE_MCOUNT(TF_EIP(%esp))
	pushl	%esp
	call	lapic_handle_timer
	add	$4, %esp
	MEXITCOUNT
	jmp	doreti

/*
 * Local APIC CMCI handler.
 */
	.text
	SUPERALIGN_TEXT
IDTVEC(cmcint)
	PUSH_FRAME
	SET_KERNEL_SREGS
	cld
	FAKE_MCOUNT(TF_EIP(%esp))
	call	lapic_handle_cmc
	MEXITCOUNT
	jmp	doreti

/*
 * Local APIC error interrupt handler.
 */
	.text
	SUPERALIGN_TEXT
IDTVEC(errorint)
	PUSH_FRAME
	SET_KERNEL_SREGS
	cld
	FAKE_MCOUNT(TF_EIP(%esp))
	call	lapic_handle_error
	MEXITCOUNT
	jmp	doreti

#ifdef SMP
/*
 * Global address space TLB shootdown.
 */
	.text
	SUPERALIGN_TEXT
IDTVEC(invltlb)
	pushl	%eax
	pushl	%ds
	movl	$KDSEL, %eax		/* Kernel data selector */
	movl	%eax, %ds

#if defined(COUNT_XINVLTLB_HITS) || defined(COUNT_IPIS)
	pushl	%fs
	movl	$KPSEL, %eax		/* Private space selector */
	movl	%eax, %fs
	movl	PCPU(CPUID), %eax
	popl	%fs
#ifdef COUNT_XINVLTLB_HITS
	incl	xhits_gbl(,%eax,4)
#endif
#ifdef COUNT_IPIS
	movl	ipi_invltlb_counts(,%eax,4),%eax
	incl	(%eax)
#endif
#endif

	movl	%cr3, %eax		/* invalidate the TLB */
	movl	%eax, %cr3

	movl	lapic, %eax
	movl	$0, LA_EOI(%eax)	/* End Of Interrupt to APIC */

	lock
	incl	smp_tlb_wait

	popl	%ds
	popl	%eax
	iret

/*
 * Single page TLB shootdown
 */
	.text
	SUPERALIGN_TEXT
IDTVEC(invlpg)
	pushl	%eax
	pushl	%ds
	movl	$KDSEL, %eax		/* Kernel data selector */
	movl	%eax, %ds

#if defined(COUNT_XINVLTLB_HITS) || defined(COUNT_IPIS)
	pushl	%fs
	movl	$KPSEL, %eax		/* Private space selector */
	movl	%eax, %fs
	movl	PCPU(CPUID), %eax
	popl	%fs
#ifdef COUNT_XINVLTLB_HITS
	incl	xhits_pg(,%eax,4)
#endif
#ifdef COUNT_IPIS
	movl	ipi_invlpg_counts(,%eax,4),%eax
	incl	(%eax)
#endif
#endif

	movl	smp_tlb_addr1, %eax
	invlpg	(%eax)			/* invalidate single page */

	movl	lapic, %eax
	movl	$0, LA_EOI(%eax)	/* End Of Interrupt to APIC */

	lock
	incl	smp_tlb_wait

	popl	%ds
	popl	%eax
	iret

/*
 * Page range TLB shootdown.
 */
	.text
	SUPERALIGN_TEXT
IDTVEC(invlrng)
	pushl	%eax
	pushl	%edx
	pushl	%ds
	movl	$KDSEL, %eax		/* Kernel data selector */
	movl	%eax, %ds

#if defined(COUNT_XINVLTLB_HITS) || defined(COUNT_IPIS)
	pushl	%fs
	movl	$KPSEL, %eax		/* Private space selector */
	movl	%eax, %fs
	movl	PCPU(CPUID), %eax
	popl	%fs
#ifdef COUNT_XINVLTLB_HITS
	incl	xhits_rng(,%eax,4)
#endif
#ifdef COUNT_IPIS
	movl	ipi_invlrng_counts(,%eax,4),%eax
	incl	(%eax)
#endif
#endif

	movl	smp_tlb_addr1, %edx
	movl	smp_tlb_addr2, %eax
1:	invlpg	(%edx)			/* invalidate single page */
	addl	$PAGE_SIZE, %edx
	cmpl	%eax, %edx
	jb	1b

	movl	lapic, %eax
	movl	$0, LA_EOI(%eax)	/* End Of Interrupt to APIC */

	lock
	incl	smp_tlb_wait

	popl	%ds
	popl	%edx
	popl	%eax
	iret

/*
 * Invalidate cache.
 */
	.text
	SUPERALIGN_TEXT
IDTVEC(invlcache)
	pushl	%eax
	pushl	%ds
	movl	$KDSEL, %eax		/* Kernel data selector */
	movl	%eax, %ds

#ifdef COUNT_IPIS
	pushl	%fs
	movl	$KPSEL, %eax		/* Private space selector */
	movl	%eax, %fs
	movl	PCPU(CPUID), %eax
	popl	%fs
	movl	ipi_invlcache_counts(,%eax,4),%eax
	incl	(%eax)
#endif

	wbinvd

	movl	lapic, %eax
	movl	$0, LA_EOI(%eax)	/* End Of Interrupt to APIC */

	lock
	incl	smp_tlb_wait

	popl	%ds
	popl	%eax
	iret

/*
 * Handler for IPIs sent via the per-cpu IPI bitmap.
 */
#ifndef XEN
	.text
	SUPERALIGN_TEXT
IDTVEC(ipi_intr_bitmap_handler)	
	PUSH_FRAME
	SET_KERNEL_SREGS
	cld

	movl	lapic, %edx
	movl	$0, LA_EOI(%edx)	/* End Of Interrupt to APIC */
	
	FAKE_MCOUNT(TF_EIP(%esp))

	call	ipi_bitmap_handler
	MEXITCOUNT
	jmp	doreti
#endif
/*
 * Executed by a CPU when it receives an IPI_STOP from another CPU.
 */
	.text
	SUPERALIGN_TEXT
IDTVEC(cpustop)
	PUSH_FRAME
	SET_KERNEL_SREGS
	cld

	movl	lapic, %eax
	movl	$0, LA_EOI(%eax)	/* End Of Interrupt to APIC */

	call	cpustop_handler

	POP_FRAME
	iret

/*
 * Executed by a CPU when it receives a RENDEZVOUS IPI from another CPU.
 *
 * - Calls the generic rendezvous action function.
 */
	.text
	SUPERALIGN_TEXT
IDTVEC(rendezvous)
	PUSH_FRAME
	SET_KERNEL_SREGS
	cld

#ifdef COUNT_IPIS
	movl	PCPU(CPUID), %eax
	movl	ipi_rendezvous_counts(,%eax,4), %eax
	incl	(%eax)
#endif
	call	smp_rendezvous_action

	movl	lapic, %eax
	movl	$0, LA_EOI(%eax)	/* End Of Interrupt to APIC */
	POP_FRAME
	iret
	
/*
 * Clean up when we lose out on the lazy context switch optimization.
 * ie: when we are about to release a PTD but a cpu is still borrowing it.
 */
	SUPERALIGN_TEXT
IDTVEC(lazypmap)
	PUSH_FRAME
	SET_KERNEL_SREGS
	cld

	call	pmap_lazyfix_action

	movl	lapic, %eax
	movl	$0, LA_EOI(%eax)	/* End Of Interrupt to APIC */
	POP_FRAME
	iret
#endif /* SMP */
