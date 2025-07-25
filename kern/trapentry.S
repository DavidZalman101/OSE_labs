/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num, perm)\
	.section .text;\
	.globl name;\
	.type name, @function;\
	.align 2;\
	name:\
	pushl $(num);\
	jmp _alltraps;\
	.section .data;\
	.align 4;\
	.long name;\
	.long num;\
	.long perm;\
	.section .text

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num, perm)\
	.globl name;\
	.type name, @function;\
	.align 2;\
	name:\
	pushl $0;\
	pushl $(num);\
	jmp _alltraps;\
	.section .data;\
	.long name;\
	.long num;\
	.long perm;\
	.section .text


/* Define the trap table - will be used in kern/trap.c */
.section .data
.global trap_table
trap_table:

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

// divide error
TRAPHANDLER_NOEC(t_divide, T_DIVIDE, 0)

// debug exception
TRAPHANDLER_NOEC(t_debug, T_DEBUG, 0)

// non-maskable interrupt
TRAPHANDLER_NOEC(t_nmi, T_NMI, 0)

// breakpoint
TRAPHANDLER_NOEC(t_brkpt, T_BRKPT, 3)

// overflow
TRAPHANDLER_NOEC(t_oflow, T_OFLOW, 0)

// bounds check
TRAPHANDLER_NOEC(t_bound, T_BOUND, 0)

// illegal opcode
TRAPHANDLER_NOEC(t_illop, T_ILLOP, 0)

// device not available
TRAPHANDLER_NOEC(t_device, T_DEVICE, 0)

// double fault
TRAPHANDLER(t_dblflt, T_DBLFLT, 0)

// T_COPROC - reserved (not generated by recent processors)
//

// invalid task switch segment
TRAPHANDLER(t_tss, T_TSS, 0)

// segment not present
TRAPHANDLER(t_segnp, T_SEGNP, 0)

// stack exception
TRAPHANDLER(t_stack, T_STACK, 0)

// general protection fault
TRAPHANDLER(t_gpflt, T_GPFLT, 0)

// page fault
TRAPHANDLER(t_pgflt, T_PGFLT, 0)

// T_RES - reserved
//

// floating point error
TRAPHANDLER_NOEC(t_fperr, T_FPERR, 0)

// aligment check
TRAPHANDLER(t_align, T_ALIGN, 0)

// machine check
TRAPHANDLER_NOEC(t_mchk, T_MCHK, 0)

// SIMD floating point error
TRAPHANDLER_NOEC(t_simderr, T_SIMDERR, 0)

// 20 - 31 - reserved
//

// system call
TRAPHANDLER_NOEC(t_syscall, T_SYSCALL, 3)

// catchall
TRAPHANDLER_NOEC(t_default, T_DEFAULT, 0)

// IRQS
// timer
TRAPHANDLER_NOEC(t_irq_timer, IRQ_OFFSET + IRQ_TIMER, 0)

// kbd
TRAPHANDLER_NOEC(t_irq_kbd, IRQ_OFFSET + IRQ_KBD, 0)

// serial
TRAPHANDLER_NOEC(t_irq_serial, IRQ_OFFSET + IRQ_SERIAL, 0)

// spurious
TRAPHANDLER_NOEC(t_irq_spurious, IRQ_OFFSET + IRQ_SPURIOUS, 0)

// E1000
TRAPHANDLER_NOEC(t_irq_e1000, IRQ_OFFSET + IRQ_E1000, 0)

// ide
TRAPHANDLER_NOEC(t_irq_ide, IRQ_OFFSET + IRQ_IDE, 0)

// error
TRAPHANDLER_NOEC(t_irq_error, IRQ_OFFSET + IRQ_ERROR, 0)

/* Define the trap table end - will be used in kern/trap.c */
.section .data
.global trap_table_end
trap_table_end:

.text

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
	 // now we are working with the TSS stack (kernel)
	 // push remianing values - ds es eax ... edi
	 pushl %ds
	 pushl %es
	 pushal

	// load GD_KD into %ds and %es
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es

	//pushl %esp to pass a pointer to the Trapframe as an argument to trap()
	pushl %esp

	// call trap (can trap ever return?)
	call trap

	// return falls through to trepret...
trapret:
		popal
		popl %es
		popl %ds
		// trapno and errcode
		addl $0x8, %esp
		iret
