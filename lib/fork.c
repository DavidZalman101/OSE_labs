// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if ((err & FEC_WR) == 0)
		panic("pgfault: faulting access was not a write");

	if ((uvpd[PDX(addr)] & PTE_P) == 0)
		panic("pgfault: pde not present");

	if ((uvpt[PGNUM(addr)] & (PTE_COW | PTE_P)) == 0)
		panic("pgfault: pte not COW | P");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	// LAB 4: Your code here.
	addr = ROUNDDOWN(addr, PGSIZE);
	r = sys_page_alloc(0, (void*) PFTEMP, PTE_P | PTE_U | PTE_W);

	if (r != 0)
		panic("pgfault: sys_page_alloc: %e", r);

	memmove((void*) PFTEMP, addr, PGSIZE);

	r = sys_page_map(0, (void*) PFTEMP, 0, addr, PTE_P | PTE_U | PTE_W);

	if (r != 0)
		panic("pgfault: sys_pape_map: %e", r);

	r = sys_page_unmap(0, PFTEMP);

	if (r != 0)
		panic("pgfault: sys_pape_unmap: %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	// LAB 5: Your code here.
	void *addr = (void*)( pn * PGSIZE);

	if ((uvpt[pn] & PTE_SHARE) == PTE_SHARE) {
		// Shared memory
		// Do not use 0xfff to mask out the relevant bits from tha page table
		// entry. 0xfff picks up the accessed and dirty bits as well.
		r = sys_page_map(0, addr, envid, addr, (uvpt[pn] & PTE_SYSCALL) | PTE_SHARE );

		if (r != 0)
			panic("duppage: sys_page_map: %e", r);

	} else if ((uvpt[pn] & PTE_W) == PTE_W || (uvpt[pn] & PTE_COW) == PTE_COW) {
		// Writable or Copy on write
		r = sys_page_map(0, addr, envid, addr, PTE_U | PTE_P | PTE_COW);

		if (r != 0)
			panic("duppage: sys_page_map: %e", r);

		r = sys_page_map(0, addr, 0, addr, PTE_U | PTE_P | PTE_COW);

		if (r != 0)
			panic("duppage: sys_page_map: %e", r);
	}else {
		// Regular
		r = sys_page_map(0, addr, envid, addr, PTE_U | PTE_P);

		if (r != 0)
			panic("duppage: sys_page_map: %e", r);
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t envid = 0;
	uintptr_t addr = 0;
	int r = 0;

	// create a child proc
	set_pgfault_handler(pgfault);
	envid = sys_exofork();

	if (envid < 0)
		panic("fork: sys_exofork: %e", envid);

	if (envid == 0) {
		// child - update thisenv and update pgfault handler
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// parent
	// copy address space
	for (addr = 0; addr < UXSTACKTOP - PGSIZE; addr += PGSIZE)
		if ((uvpd[PDX(addr)] & PTE_P) == PTE_P && (uvpt[PGNUM(addr)] & PTE_P) == PTE_P)
			duppage(envid, PGNUM(addr));

	// allocate childs exception stack - No COW
	if ((r = sys_page_alloc(envid, (void*) (UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) != 0)
		panic("fork: sys_page_alloc: %e", r);

	// set childs upcall
	extern void _pgfault_upcall(void);
	if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) != 0)
		panic("fork: sys_env_set_pgfault_upcall: %e", r);

	// set child as runnable
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) != 0)
		panic("fork: sys_env_set_status: %e", r);

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
