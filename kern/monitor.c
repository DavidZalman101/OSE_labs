// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/types.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display backtrace of the stack", mon_backtrace },
	{ "showmappings", "Display memory mappings", show_mappings},
	{ "permmap", "set/clear/change permission of address mapping", perm_map},
	{ "dumpmem", "Display contents of a range of memory", dump_mem},
	{ "continue", "continue execution from the current EIP (debbuger)", mon_continue},
	{ "step", "Single step one instruction (debbuger)", mon_step},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_step(int argc, char **argv, struct Trapframe *tf)
{
	/*
		Set the trap flag (TF) to 1.
		How does that work:
			> The CPU executes one instruction
			> Because TF = 1, a debug trap occurs after
			  the executed instruction.
			> trap() -> monitor now can step again or continue
	*/
	tf->tf_eflags |= FL_TF;

	// exit monitor and resume execution
	return -1;
}

int
mon_continue(int argc, char **argv, struct Trapframe *tf)
{
	// exit monitor and resume execution
	tf->tf_eflags &= ~FL_TF;
	return -1;
}

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
show_mappings(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t va_start, va_end;
	uint32_t n_pages, i;
	char *endptr;
	char perm[6];

	// get input
	if (argc != 3) {
		cprintf(RED_STR("Usage: showmappings va_start va_end\n"));
		return 0;
	}

	va_start = strtol(argv[1], &endptr, 0);

	if (*endptr) {
		cprintf(RED_STR("showmappings: Invalid start address %s\n"), argv[1]);
		return 0;
	}

	va_end = strtol(argv[2], &endptr, 0);

	if (*endptr) {
		cprintf(RED_STR("showmappings: Invalid end address %s\n"), argv[2]);
		return 0;
	}

	va_start = ROUNDDOWN(va_start, PGSIZE);
	va_end = ROUNDUP(va_end + 1, PGSIZE);
	n_pages = (va_end - va_start) / PGSIZE;

	// get mapping
	cprintf("  VA            PA       FLAGS\n");
	for (i = 0; i < n_pages; i++, va_start += PGSIZE) {
		pte_t *pte_p = pgdir_walk(kern_pgdir, (void*) va_start, 0);

		if (pte_p && (*pte_p & PTE_P)) {
			perm[0] = (*pte_p & PTE_D) ? 'D' : '-';
			perm[1] = (*pte_p & PTE_A) ? 'A' : '-';
			perm[2] = (*pte_p & PTE_U) ? 'U' : '-';
			perm[3] = (*pte_p & PTE_W) ? 'W' : '-';
			perm[4] = (*pte_p & PTE_P) ? 'P' : '-';
			perm[5] = '\0';
			cprintf("0x%08x -> 0x%08x %s\n", va_start, PTE_ADDR(*pte_p), perm);
		} else
			cprintf("0x%08x -> 0x-------- -----\n", va_start);
	}

	return 0;
}

int
perm_map(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t va;
	char *endptr, *permarg, mode;
	int i, perm_flags = 0;
	pte_t* pte_p;
	char perm[6] = "-----";

	if (argc != 3) {
		cprintf(RED_STR("Usage: permmap va [+/-/=]PERM (e.g. +WU, -U, =W)\n"));
		return 0;
	}

	va = strtol(argv[1], &endptr, 0);

	if (*endptr) {
		cprintf(RED_STR("permmap: Invalid start address %s\n"), argv[1]);
		return 0;
	}

	permarg = argv[2];
	mode = permarg[0];

	if (mode != '+' && mode != '-' && mode != '=') {
		cprintf(RED_STR("permmap: Invalid mode: must start with +, -, or =\n"));
		return 0;
	}

	for (i = 1; permarg[i]; i++) {

		if (permarg[i] >= 'a' && permarg[i] <= 'z')
			permarg[i] -= 32; //convert to upper case

		switch (permarg[i]) {
			case 'P': perm_flags |= PTE_P; break;
			case 'W': perm_flags |= PTE_W; break;
			case 'U': perm_flags |= PTE_U; break;
			case 'A': perm_flags |= PTE_A; break;
			case 'D': perm_flags |= PTE_D; break;
			default:
				cprintf(RED_STR("Unknown permission: %c\n"), permarg[i]);
				return 0;
		}
	}


	if (!(pte_p = pgdir_walk(kern_pgdir, (void*) va, 0))) {
		cprintf(RED_STR("No PTE for VA 0x%08x\n"), va);
		return 0;
	}

	if (!(*pte_p & PTE_P)) {
		cprintf(RED_STR("Page at VA 0x%08x not present\n"), va);
		return 0;
	}

	// Modify permissions
	if (mode == '+')
		*pte_p |= perm_flags;
	else if (mode == '-')
		*pte_p &= ~perm_flags;
	else if (mode == '=')
		*pte_p = PTE_ADDR(*pte_p) | perm_flags;

	// Invalidate TLB
	tlb_invalidate(kern_pgdir, (void*) va);

	perm[0] = (*pte_p & PTE_D) ? 'D' : '-';
	perm[1] = (*pte_p & PTE_A) ? 'A' : '-';
	perm[2] = (*pte_p & PTE_U) ? 'U' : '-';
	perm[3] = (*pte_p & PTE_W) ? 'W' : '-';
	perm[4] = (*pte_p & PTE_P) ? 'P' : '-';
	perm[5] = '\0';

	cprintf("Updated PTE at VA 0x%08x: PTE = 0x%08x %s\n", va, PTE_ADDR(*pte_p), perm);
	return 0;
}

int
dump_mem_v(int argc,char **argv)
{
	uintptr_t start_va, end_va, va, curr_va;
	pte_t *pte_p;
	int i;

	start_va = strtol(argv[2], NULL, 0);
	end_va = strtol(argv[3], NULL, 0);

	if (start_va >= end_va) {
		cprintf(RED_STR("Invalid range: start >= end\n"));
		return 0;
	}

	for (va = start_va; va < end_va; va += 16) {
		cprintf("0x%08x:", va);

		for (i = 0; i < 16 && va + i < end_va; i++) {
			curr_va = va + i;
			pte_p = pgdir_walk(kern_pgdir, (void*)curr_va, 0);

			if (pte_p && (*pte_p & PTE_P))
				cprintf(" %02x", *(uint8_t*)curr_va);
			else
				cprintf(" ??");
		}
		cprintf("\n");
	}

	return 0;
}

int
dump_mem_p(int argc,char **argv)
{
	physaddr_t start_pa, end_pa;
	uintptr_t start_va, end_va, va, curr_va;
	int i;

	start_pa = strtol(argv[2], NULL, 0);
	end_pa = strtol(argv[3], NULL, 0);

	if (start_pa >= end_pa) {
		cprintf(RED_STR("Invalid range: start >= end\n"));
		return 0;
	}

	if (end_pa >= npages * PGSIZE) {
		cprintf(RED_STR("Invalid range: end >= PHYSTOP\n"));
		return 0;
	}

	start_va = (uintptr_t) KADDR(start_pa);
	end_va = (uintptr_t) KADDR(end_pa);

	for (va = start_va; va < end_va; va += 16) {
	    cprintf("0x%08x:", PADDR((void*)va));

	    for (i = 0; i < 16 && va + i < end_va; i++) {
		curr_va = va + i;
		cprintf(" %02x", *(uint8_t*)curr_va);
	    }

	    cprintf("\n");
	}

	return 0;
}

int
dump_mem(int argc, char **argv, struct Trapframe *tf)
{

	if (argc != 4 || (strcmp(argv[1], "-v") != 0 && strcmp(argv[1], "-p") != 0)) {
		cprintf(RED_STR("Usage: dumpmem -v/p start_addr end_addr\n"));
		return 0;
	}

	return !strcmp(argv[1], "-v") ? dump_mem_v(argc, argv) : dump_mem_p(argc, argv);
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	/*
		This figure shows the stack frame organization:

		Position		Contents		Frame
		-------------------------------------
		4n+8(%ebp)		arg word n		Prev	High address
						...
		8(%ebp)			arg word 0		Prev
		-------------------------------------
		4(%ebp)			return add		Curr
		-------------------------------------
		0(%ebp)			prev %ebp		Curr
		-------------------------------------
		-4(%ebp)		unspecified		Curr
						...
		0(%esp)			variable size	Curr	Low address
		-------------------------------------
	*/
	uint32_t ebp, eip, args[5];
	int i;
	struct Eipdebuginfo info;

	cprintf("Stack backtrace:\n");

	// get the current ebp
	ebp = read_ebp();

	/*
		kern/enrty.S:
		when we init the kernel stack, we set the first ebp
		to be $0x0. Therefore, we know that the last kernel
		stack frame is the one where ebp != 0
	*/
	while (ebp) {
		eip = *((uint32_t*)ebp + 1);
		debuginfo_eip(eip, &info);

		// TODO: read exactly the number of function args
		//		 use info.eip_fn_narg, and dont forget to
		//		 zero the rest! other wise you'll carry
		//		 garbage!
		for (i = 0; i < 5; i++)
			args[i] = *((uint32_t*)ebp + 2 + i);

		cprintf("ebp %08x eip %08x args %08x %08x %08x %08x %08x\n\t%s:%d: %.*s+%d\n",
				ebp, eip, args[0], args[1], args[2], args[3], args[4],
				info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name,
				(uint32_t*)eip - info.eip_fn_addr);

		ebp = *((uint32_t*)ebp);
	}

	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
