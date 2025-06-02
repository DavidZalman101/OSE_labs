
#include "fs.h"


#ifdef BC_EVICT
void bc_evict(void);
static size_t cached_block_count = 0;
#endif


// Return the virtual address of this disk block.
void*
diskaddr(uint32_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (uvpd[PDX(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (uvpt[PGNUM(va)] & PTE_D) != 0;
}

// Fault any disk block that is read in to memory by
// loading it from disk.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	void* addr_aligned = ROUNDDOWN(addr, SECTSIZE);
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;
	bool tried_to_make_space = 0;

#ifdef BC_EVICT
	if (cached_block_count >= 64)
		bc_evict();
	
	cached_block_count++;
#endif

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_eip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page.
	// Hint: first round addr to page boundary. fs/ide.c has code to read
	// the disk.
	//
	// LAB 5: you code here:
	// allocate a page
	if ((r = sys_page_alloc(0, addr_aligned, PTE_U | PTE_P | PTE_W)) != 0)
		panic("bc_pgfault: sys_page_alloc %e", r);

	// read from disk to the allocated page
	if ((r = ide_read(blockno * BLKSECTS, addr_aligned, BLKSECTS)) != 0)
		panic("bc_pgfault: ide_read %e", r);

	// Clear the dirty bit for the disk block page since we just read the
	// block from disk
	if ((r = sys_page_map(0, addr_aligned, 0, addr_aligned, uvpt[PGNUM(addr_aligned)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);

	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	// LAB 5: Your code here.
	void *addr_aligned = ROUNDDOWN(addr, BLKSIZE);

	if (!va_is_mapped(addr_aligned) || !va_is_dirty(addr_aligned))
		return;

	// mapped and dirty -- write to disk
	if ((r = ide_write(blockno * BLKSECTS, addr_aligned, BLKSECTS)) != 0)
		panic("flush_block: ide_write: %e", r);

	// clear the dirty bit
	if ((r = sys_page_map(0, addr_aligned, 0, addr_aligned, uvpt[PGNUM(addr_aligned)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);
}

#ifdef BC_EVICT
// Evict blocks that are no longer in use.
void
bc_evict(void)
{
	// NOTE: here we relay on a block size of be = PGSIZE
	int r, blockno;
	void *addr;
	pte_t pte;

	for (blockno = 1; blockno < super->s_nblocks; blockno++) {

		addr = diskaddr(blockno);
		pte = uvpt[PGNUM(addr)];

		// skip unmapped
		if ((pte & PTE_P) != PTE_P)
			continue;

		// page recently used - mark it as unused
		// if it is really used, the hardware will mark it
		// accessed again, o.w. someone is hogging the block.
		if ((pte & PTE_A) == PTE_A) {
			// clear the Accessed Bit
			if ((r = sys_page_map(0, (void*) addr, 0, (void*) addr, (pte & PTE_SYSCALL) & ~PTE_A)) != 0)
				panic("bc_evict: sys_page_map: %e", r);
			continue;
		}

		// evict
		// dirty? flush
		if ((pte & PTE_D) == PTE_D)
			flush_block((void*) addr);

		if ((r = sys_page_unmap(0, (void*) addr)) != 0)
			panic("bc_evict: sys_page_unmap: %e", r);

		cached_block_count--;
	}
}
#endif

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
	struct Super super;
	set_pgfault_handler(bc_pgfault);
	check_bc();

	// cache the super block by reading it once
	memmove(&super, diskaddr(1), sizeof super);
}

