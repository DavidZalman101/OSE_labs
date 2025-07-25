/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.

	user_mem_assert(curenv, (void*) s, len, 0);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.

	struct Env *new_env = NULL;
	int rval = 0;

	if ((rval = env_alloc(&new_env, curenv->env_id)) != 0)
		return rval;

	new_env->env_status = ENV_NOT_RUNNABLE;
	new_env->env_tf = curenv->env_tf;
	new_env->env_tf.tf_regs.reg_eax = 0;
	return new_env->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	//LAB 4: Your code here.
	struct Env *env;
	int rval = 0;

	if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
		return -E_INVAL;

	if ((rval = envid2env(envid, &env, 1)) != 0)
		return rval;

	env->env_status = status;
	return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	int rval = 0;
	struct Env *env;

	if ((rval = envid2env(envid, &env, 1)) != 0) {
		cprintf("here");
		return rval;
	}

	user_mem_assert(env, (void*) tf, sizeof(struct Trapframe), PTE_W);

	// set up the trap frmae
	// run in RING 3
	//tf->tf_es |= 3;
	//tf->tf_ds |= 3;
	tf->tf_cs |= 3;
	tf->tf_ss |= 3;
	// enable interrupts
	tf->tf_eflags |= FL_IF;
	// set IO Privilage level to Ring 0
	tf->tf_eflags &= ~FL_IOPL_3;
	// final set
	env->env_tf = *tf;
	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env *env = NULL;

	if (envid2env(envid, &env, 1) != 0) {
		return -E_BAD_ENV;
	}

	env->env_pgfault_upcall = func;
	return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	struct Env *env = NULL;
	struct PageInfo * new_page;

	if (envid2env(envid, &env, 1) != 0)
		return -E_BAD_ENV;

	if ((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P))
		return -E_INVAL;

	if (perm & ~PTE_SYSCALL)
		return -E_INVAL;

	if ((uintptr_t) va >= UTOP)
		return -E_INVAL;

	if ((uintptr_t) ROUNDDOWN(va, PGSIZE) != (uintptr_t) va)
		return -E_INVAL;

	if (!(new_page = page_alloc(ALLOC_ZERO)))
		return -E_NO_MEM;

	if ((page_insert(env->env_pgdir, new_page, va, perm)) != 0) {
		page_free(new_page);
		return -E_NO_MEM;
	}

	return 0;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	struct Env *src_env = NULL, *dst_env = NULL;
	struct PageInfo *src_page = NULL;
	pte_t *src_page_pte;

	if (envid2env(srcenvid, &src_env, 1) != 0)
		return -E_BAD_ENV;

	if (envid2env(dstenvid, &dst_env, 1) != 0)
		return -E_BAD_ENV;

	if ((uintptr_t) srcva >= UTOP)
		return -E_INVAL;

	if ((uintptr_t) dstva >= UTOP)
		return -E_INVAL;

	if ((uintptr_t) ROUNDDOWN(srcva, PGSIZE) != (uintptr_t) srcva)
		return -E_INVAL;

	if ((uintptr_t) ROUNDDOWN(dstva, PGSIZE) != (uintptr_t) dstva)
		return -E_INVAL;

	src_page = page_lookup(src_env->env_pgdir, srcva, &src_page_pte);

	if (src_page == NULL)
		return -E_INVAL;

	if (!(*src_page_pte & PTE_W) && (perm & PTE_W))
		return -E_INVAL;

	if ((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P))
		return -E_INVAL;

	if (perm & ~PTE_SYSCALL)
		return -E_INVAL;

	if (page_insert(dst_env->env_pgdir, src_page, dstva, perm) != 0)
		return -E_NO_MEM;

	return 0;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	struct Env *env = NULL;

	if (envid2env(envid, &env, 1) != 0)
		return -E_BAD_ENV;

	if ((uintptr_t) va >= UTOP)
		return -E_INVAL;

	if ((uintptr_t) ROUNDDOWN(va, PGSIZE) != (uintptr_t) va)
		return -E_INVAL;

	page_remove(env->env_pgdir, va);

	return 0;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	struct Env *env = NULL;
	struct PageInfo *page = NULL;
	pte_t *pte_p = NULL;
	int r = 0;

	if ((r = envid2env(envid, &env, 0)) != 0)
		return r;

#ifdef IPC_SEND_NO_LOOP

	if ((uintptr_t) srcva < UTOP) {
		/* src wants to send a page */
		if ((uintptr_t) srcva % PGSIZE != 0)
			return -E_INVAL;

		if ((perm & (PTE_U | PTE_P)) != (PTE_P | PTE_U))
			return -E_INVAL;

		if (perm & ~PTE_SYSCALL)
			return -E_INVAL;

		if (!(page = page_lookup(curenv->env_pgdir, srcva, &pte_p)))
			return -E_INVAL;

		if ((*pte_p & PTE_W) == 0 && (perm & PTE_W) == PTE_W)
			return -E_INVAL;
	}

	if (!env->env_ipc_recving) {
		/*
			The reciever env is not expected ipc right now.
			So leave the data prepared (value,page,addr,perm)
			and go to sleep. When the reciever will ask for data
			he'll see your offer, take it (if can) and wake you.
		*/

		// fill the needed data for the reciever to collect if can
		curenv->env_ipc_to = envid;
		curenv->env_ipc_value_send = value;
		curenv->env_ipc_page = page;
		curenv->env_ipc_map_addr = srcva;
		curenv->env_ipc_map_perm = perm;

		// go sleep
		curenv->env_status = ENV_NOT_RUNNABLE;
		return 0;
	} else {
		// the reciver env is expecting ipc right now.
		// send it!
		if ((uintptr_t) env->env_ipc_dstva < UTOP) {
			/* reciver wants to get a page */
			if ((r = page_insert(env->env_pgdir, page, env->env_ipc_dstva, perm)) != 0)
				goto done;

			env->env_ipc_perm = perm;
			goto done;
		}
	}

done:
	env->env_ipc_recving = 0;
	env->env_ipc_from = curenv->env_id;
	env->env_ipc_value = value;
	env->env_status = ENV_RUNNABLE;
	curenv->env_ipc_to = -1;
	return r;

#else
	if (!env->env_ipc_recving)
		return -E_IPC_NOT_RECV;

	if ((uintptr_t) srcva < UTOP) {
		/* src wants to send a page */
		if ((uintptr_t) srcva % PGSIZE != 0)
			return -E_INVAL;

		if ((perm & (PTE_U | PTE_P)) != (PTE_P | PTE_U))
			return -E_INVAL;

		if (perm & ~PTE_SYSCALL)
			return -E_INVAL;

		if (!(page = page_lookup(curenv->env_pgdir, srcva, &pte_p)))
			return -E_INVAL;

		if ((*pte_p & PTE_W) == 0 && (perm & PTE_W) == PTE_W)
			return -E_INVAL;

		if ((uintptr_t) env->env_ipc_dstva < UTOP) {
			/* rcv wants to get a page */
			if ((r = page_insert(env->env_pgdir, page, env->env_ipc_dstva, perm)) != 0)
				return r;

			env->env_ipc_perm = perm;
			goto done;
		}
	}

	env->env_ipc_perm = 0;
done:
	env->env_ipc_recving = 0;
	env->env_ipc_from = curenv->env_id;
	env->env_ipc_value = value;
	env->env_status = ENV_RUNNABLE;
	return 0;
#endif
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	int i, r = 0;
	if ((uintptr_t) dstva < UTOP && (uintptr_t) dstva % PGSIZE != 0)
		return -E_INVAL;

	curenv->env_ipc_recving = 1;
	curenv->env_ipc_dstva = dstva;
	curenv->env_status = ENV_NOT_RUNNABLE;

#ifdef IPC_SEND_NO_LOOP
	// Scan the envs to see if anyone tried sending you an ipc.
	for (i = 0; i < NENV; i++)
		if (envs[i].env_status == ENV_NOT_RUNNABLE &&
			envs[i].env_ipc_to == curenv->env_id) {

			/* someone waited for me */
			// note: all the perm/addr checks were done in the sender
			if ((uintptr_t) envs[i].env_ipc_map_addr < UTOP &&
				(r = page_insert(curenv->env_pgdir, envs[i].env_ipc_page,
							 	 dstva, envs[i].env_ipc_map_perm)) != 0) {
				// dont forget to free that page
				return r;
			}

			// collect data
			curenv->env_ipc_recving = 0;
			curenv->env_ipc_from = envs[i].env_id;
			curenv->env_ipc_value = envs[i].env_ipc_value_send;
			envs[i].env_ipc_to = -1;

			// erase all the extra data the sender has, to avoid leakage
			envs[i].env_ipc_to = -1;
			envs[i].env_ipc_value_send = 0;
			envs[i].env_ipc_page = NULL;
			envs[i].env_ipc_map_addr = NULL;
			envs[i].env_ipc_map_perm = 0;

			// wake them both up
			envs[i].env_status = ENV_RUNNABLE;
			curenv->env_status = ENV_RUNNABLE;
			break;
		}
#endif

	return 0;
}

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	return time_msec();
}

/*
	Transmit a packet with E1000.
	return values:
		-E1000_ERR_IVALID_ARG
		-E1000_ERR_TX_PKT_TOO_BIG
		-E1000_ERR_TX_QUEUE_FULL
		0 (Success)
*/
static int
sys_net_try_send(void *buf, uint32_t buf_len)
{
	user_mem_assert(curenv, buf, buf_len, PTE_U);

	void *va = ROUNDDOWN(buf, PGSIZE);
	int offset = (uintptr_t) buf - (uintptr_t)va;
	struct PageInfo *p = page_lookup(curenv->env_pgdir, va, NULL);
	physaddr_t pa = page2pa(p);
	void *buf_pa = (uint8_t*)pa + offset;

	// Increment refrence count before giving hardware
	p->pp_ref++; // Prevent page from bring freed

	int res = transmit_packet((uint8_t*) buf_pa, buf_len);

	if (res == -E1000_ERR_TX_QUEUE_FULL) {
		// go sleep
		curenv->env_tf.tf_regs.reg_eax = -E1000_ERR_RX_QUEUE_EMPTY;
		curenv->env_status = ENV_NET_WAIT;
		sched_yield();
	}

	return res;
}

/*
	Recieve a packet with E1000.
	return values:
		-E1000_ERR_IVALID_ARG
		-E1000_ERR_RX_PKT_TOO_BIG
		-E1000_ERR_RX_QUEUE_FULL
		0+ = number of bytes read from buffer (Success)
*/
static int
sys_net_try_recv(uint32_t *buf_len)
{
	int res = receive_packet(buf_len);

	if (res == -E1000_ERR_RX_QUEUE_EMPTY) {
		// go sleep
		curenv->env_tf.tf_regs.reg_eax = -E1000_ERR_RX_QUEUE_EMPTY;
		curenv->env_status = ENV_NET_WAIT;
		sched_yield();
	}

	return res;
}

static int
sys_net_get_hwaddr(void *buf, size_t size)
{
	if (size < 6)
		return -E_INVAL;
	
	uint64_t hwaddr = e1000_eeprom_get_hwaddr();
	memcpy(buf, (uint8_t*)&hwaddr, 6);
	return 0;
}

static int
sys_e1000_map_buffers(void *va)
{
	int i;
	uintptr_t buffer_va;
	pte_t *pte;

	for (i = 0; i < NRDX; i++) {

		buffer_va = (uintptr_t)rx_bufs[i];

		assert(buffer_va % PGSIZE == 0);
		assert(PADDR((void*)buffer_va) % PGSIZE == 0);

		pte = pgdir_walk(curenv->env_pgdir, va + i * PGSIZE, 1);
		assert(!(*pte & PTE_P));
		*pte = PADDR(rx_bufs[i]) | PTE_P | PTE_U | PTE_W;
		invlpg(va);
	}

	return 0;
}

static int
sys_e1000_receive_packet_done(int buf_idx)
{
	int res = receive_packet_done(buf_idx);

	if (res < 0) {
		// go sleep
		curenv->env_tf.tf_regs.reg_eax = res;
		curenv->env_status = ENV_NET_WAIT;
		sched_yield();
	}

	return res;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
	int32_t r_val = 0;

	switch (syscallno) {

		case SYS_cputs:
			sys_cputs((char*) a1, (size_t) a2);
			r_val = a2; // return the len that was printed
			goto done;

		case SYS_cgetc:
			r_val = sys_cgetc();
			goto done;

		case SYS_getenvid:
			r_val = sys_getenvid();
			goto done;

		case SYS_env_destroy:
			r_val = sys_env_destroy((envid_t) a1);
			goto done;

		case SYS_yield:
			sys_yield();
			goto done;

		case SYS_exofork:
			r_val = sys_exofork();
			goto done;

		case SYS_env_set_status:
			r_val = sys_env_set_status((envid_t) a1, (int) a2);
			goto done;

		case SYS_page_alloc:
			r_val = sys_page_alloc((envid_t) a1, (void*) a2, (int) a3);
			goto done;

		case SYS_page_map:
			r_val = sys_page_map((envid_t) a1, (void*) a2, (envid_t) a3, (void*) a4, (int) a5);
			goto done;

		case SYS_page_unmap:
			r_val = sys_page_unmap((envid_t) a1, (void*) a2);
			goto done;

		case SYS_env_set_pgfault_upcall:
			r_val = sys_env_set_pgfault_upcall((envid_t) a1, (void*) a2);
			goto done;

		case SYS_ipc_try_send:
			r_val = sys_ipc_try_send((envid_t) a1, (uint32_t) a2, (void*) a3, (unsigned) a4);
			goto done;

		case SYS_ipc_recv:
			r_val = sys_ipc_recv((void*) a1);
			goto done;

		case SYS_env_set_trapframe:
			r_val = sys_env_set_trapframe((envid_t) a1, (struct Trapframe*) a2);
			goto done;

		case SYS_time_msec:
			r_val = sys_time_msec();
			goto done;

		case SYS_net_send:
			r_val = sys_net_try_send((void*) a1, (uint32_t) a2);
			goto done;

		case SYS_net_try_recv:
			r_val = sys_net_try_recv((uint32_t*) a1);
			goto done;

		case SYS_net_get_hwaddr:
			r_val = sys_net_get_hwaddr((void*) a1, (uint32_t) a2);
			goto done;

		case SYS_e1000_map_buffers:
			r_val = sys_e1000_map_buffers((void*) a1);
			goto done;

		case SYS_e1000_receive_packet_done:
			r_val = sys_e1000_receive_packet_done((int) a1);
			goto done;

		default:
			r_val = -E_INVAL;
	}

done:
	return r_val;
}
