#include <kern/e1000.h>

// LAB 6: Your driver code here
struct tx_desc tx_queue[NTDX] __attribute__((aligned(16)));
struct rx_desc rx_queue[NRDX] __attribute__((aligned(16)));
uint8_t rx_bufs[NRDX][PGSIZE] __attribute__((aligned(PGSIZE)));

static int next_idx = 0;

static int buffer_in_use[NRDX] = {0}; // Track which bufferes are with user space
static int available_buffers = NRDX;  // Count of free buffers

/*
	Gets called automatically when JOS finds the E1000 dev during boot.
	Enables the device - allocate memory space/IO ports and interrupt lines.
	Sets up driver initialization - prepares the device for use by JOS.
*/
int
e1000_attach(struct pci_func *pcif)
{
	physaddr_t bar0_base;
	uint32_t bar0_size;
	// pci_func_enable negotiates an MMIO region with the E1000 and stores its base
	// and size in BAR 0. This is a range of physical memory addresses assigned to
	// this device.
	pci_func_enable(pcif);

	// Create a virtual memory mapping for the E1000 BAR 0.
	bar0_base = pcif->reg_base[0];
	bar0_size = pcif->reg_size[0];
	e1000_base = mmio_map_region(bar0_base, bar0_size);

	// test - eeprom - hwaddr
	uint64_t hwaddr = e1000_eeprom_get_hwaddr();
	uint8_t *mac = (uint8_t*)&hwaddr;
	cprintf("E1000: hwaddr = %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	// test - print the device status register - expecting 0x80080783
	cprintf("E1000 status = [%x]\n", E1000_REG(E1000_STATUS));

	transmit_init();
	receive_init();

	return 0;
}

/*
	Transmit Initialization - 14.5
	- Allocate memory for the transmit descriptor list.
	- Program the transmit descriptor base address registers.
	- Set the transmit descriptor length register.
	- Initialize the transmit descriptor head and tail.
	- Initialize the transmit control register.
	- Initialize the transmit inter packet gap register.
*/
static void
transmit_init(void)
{
	uint32_t i;

	// mem init
	memset(tx_queue, 0, sizeof(tx_queue));
	assert((PADDR((void*) tx_queue) & 0xF) == 0x0); // 16-bytes aligned

	for (i = 0; i < NTDX; i++) {
		tx_queue[i].buf_addr = 0;
		tx_queue[i].cmd |= E1000_TX_CMD_RS;
		tx_queue[i].status |= E1000_TX_STAT_DD;
	}

	// base init
	E1000_REG(E1000_TDBAL) = PADDR((void*)tx_queue);
	E1000_REG(E1000_TDBAH) = 0;

	// len init
	assert((sizeof(tx_queue) & 0x7F) == 0); // 128-byte aligned
	E1000_REG(E1000_TDLEN) = sizeof(tx_queue);

	// head & tail init
	E1000_REG(E1000_TDH) = 0x0;
	E1000_REG(E1000_TDT) = 0x0;

	// control init
	E1000_REG(E1000_TCTL) &= ~(E1000_TCTL_CT | E1000_TCTL_COLD);
	E1000_REG(E1000_TCTL) |= (E1000_TCTL_EN)    |
							 (E1000_TCTL_PSP)   |
							 (0x10 << E1000_TCTL_CT_SHIFT) |
							 (0x40 << E1000_TCTL_COLD_SHIFT);

	// Inter packet gap init
	E1000_REG(E1000_TIPG) &= ~(E1000_TIPG_IPGT_MASK | E1000_TIPG_IPGR1_MASK |
							   E1000_TIPG_IPGR2_MASK);
	E1000_REG(E1000_TIPG) = (0xa) | (0x4 << 10) | (0x6 << 20);
	asm volatile("mfence" ::: "memory");
}

	static int tail_idx = 0;

int
transmit_packet(void *buf, uint32_t buf_len)
{
	if (buf_len > TDX_BUF_SIZE)
		return -E1000_ERR_TX_PKT_TOO_BIG;
	

	if (!(tx_queue[tail_idx].status & E1000_TX_STAT_DD))
		return -E1000_ERR_TX_QUEUE_FULL;

	assert(tx_queue[tail_idx].cmd & E1000_TX_CMD_RS);

	// prepare to insert packet to tx queue
	tx_queue[tail_idx].buf_addr = (uintptr_t)buf;
	tx_queue[tail_idx].len = buf_len;
	tx_queue[tail_idx].cmd = E1000_TX_CMD_EOP | E1000_TX_CMD_RS;
	tx_queue[tail_idx].status &= ~E1000_TX_STAT_DD;

	// insert - make sure all memory was written before and after
	asm volatile("mfence" ::: "memory");
	tail_idx = (tail_idx + 1) % NTDX;
	asm volatile("mfence" ::: "memory");
	E1000_REG(E1000_TDT) = tail_idx;
	asm volatile("mfence" ::: "memory");
	return 0;
}

/*
	Receive Initialization - 14.4
	- Program the Receive Address Registers.
	- Init the Multicast Table array.
	- Allocate memory for receive descriptor list.
	- Program the Receive Base Adress registers.
	- Set the Receive Descriptor Length.
	- Init the Receive Descriptor Head and Tail.
	- Program the Receive Control Register.
*/
static void
receive_init(void)
{
	int i;
	size_t jp_data_offset = offsetof(union Nsipc, pkt.jp_data);

	// init Receive regs
	// use the hardware MAC address
	//E1000_REG(E1000_RAL) = DEFAULT_MAC_ADDRESS_LOW_ORDER;
	//E1000_REG(E1000_RAH) = DEFAULT_MAC_ADDRESS_HIGH_ORDER;
	// by default, it comes from EEPROM
	E1000_REG(E1000_RAH) |= E1000_RAH_AV;

	// init MTA
	uint32_t mta_addr = E1000_MTA;
	for (i = 0; i < 128; i++, mta_addr += 4)
		E1000_REG(mta_addr) = 0;
	
	// base init
	assert(PADDR(rx_queue) % 16 == 0);
	E1000_REG(E1000_RDBAL) = PADDR(rx_queue);
	E1000_REG(E1000_RDBAH) = 0;

	// len init
	assert(sizeof(rx_queue) % 128 == 0);
	E1000_REG(E1000_RDLEN) = sizeof(rx_queue);

	// head & tail init
	E1000_REG(E1000_RDH) = 0;
	E1000_REG(E1000_RDT) = NRDX - 1;

	// control init
	E1000_REG(E1000_RCTL) &= ~E1000_RCTL_LPE; // Disable Long packets
	E1000_REG(E1000_RCTL) &= ~E1000_RCTL_LBM; // loopback mode - normal operation

	// RDMT
	E1000_REG(E1000_RCTL) &= ~E1000_RCTL_RDMTS; // clear
	E1000_REG(E1000_RCTL) |= E1000_RCTL_RDMTS_QUAT; // set

	// MO
	E1000_REG(E1000_RCTL) &= ~(E1000_RCTL_MO_1 | E1000_RCTL_MO_2 | E1000_RCTL_MO_3); // clear
	E1000_REG(E1000_RCTL) |= E1000_RCTL_MO_0; // set

	// BAM
	E1000_REG(E1000_RCTL) |= E1000_RCTL_BAM;

	// Buf Size/Extension
	E1000_REG(E1000_RCTL) &= ~E1000_RCTL_BSEX; // clear
	E1000_REG(E1000_RCTL) &= ~(E1000_RCTL_SZ_1024 | E1000_RCTL_SZ_512 | E1000_RCTL_SZ_256); // clear
	// 00b -> 2048b // set

	// Strip CRC
	E1000_REG(E1000_RCTL) |= E1000_RCTL_SECRC;

	// mem init
	memset(rx_queue, 0, sizeof(rx_queue));
	for (i = 0; i < NRDX; i++) {
		memset(rx_bufs[i], 0, RDX_BUF_SIZE);
		rx_queue[i].buf_addr = PADDR(rx_bufs[i]) + jp_data_offset;
		rx_queue[i].status = 0;
	}

	// Enable
	asm volatile("mfence" ::: "memory");
	E1000_REG(E1000_RCTL) |= E1000_RCTL_EN;
	asm volatile("mfence" ::: "memory");

	// Intr
	E1000_REG(E1000_IMC) = 0xffffffff;  // Clear all interrupts first
	E1000_REG(E1000_IMS) = E1000_IMS_RXDMT0 | E1000_IMS_RXT0 | E1000_ICR_TXQE;
	irq_setmask_8259A(irq_mask_8259A & ~(1<<11));
}

int
receive_packet(uint32_t *buf_len)
{
	// Check if we have any free buffers
	if (available_buffers == 0)
		return -E1000_ERR_RX_QUEUE_EMPTY; // TODO: fix syscall: put to sleep even when RX is FULL

	int to_copy = 0, to_ret;
	uint32_t rec_buf_len = rx_queue[next_idx].len;

	// check if queue is empty
	if (!(rx_queue[next_idx].status & E1000_RXD_STAT_DD))
		return -E1000_ERR_RX_QUEUE_EMPTY;

	// check if the packet has legal len
	if (rec_buf_len > RDX_BUF_SIZE)
		return -E1000_ERR_RX_PKT_TOO_BIG;

	// Mark buffer as in use
	buffer_in_use[next_idx] = 1;
	available_buffers--;

	// copy to buffer - ZERO COPY
	*buf_len = rec_buf_len;
	int buffer_idx = next_idx;
	next_idx = (next_idx + 1) % NRDX;

	return buffer_idx;
}

int
receive_packet_done(int buffer_idx)
{

	// if buffer is not free, go sleep
	if (buffer_idx < 0 || buffer_idx >= NRDX || !buffer_in_use[buffer_idx])
		return -E_INVAL;

	// Mark buffer as free
	buffer_in_use[buffer_idx] = 0;
	available_buffers++;

	// Tell hardware this buffer is available again

	rx_queue[buffer_idx].status &= ~E1000_RXD_STAT_DD;
	asm volatile("mfence" ::: "memory");
	E1000_REG(E1000_RDT) = buffer_idx;
	asm volatile("mfence" ::: "memory");
	return 0;
}

void
e1000_interrupt(void)
{
	int i;
	uint32_t icr = E1000_REG(E1000_ICR);
	asm volatile("mfence" ::: "memory");

	if (!(icr & (E1000_ICR_RXT0 | E1000_ICR_RXDMT0)))
		return;

	// wake up all the envs whome are waiting on net
	for (i = 0; i < NENV; i++)
		if (envs[i].env_status == ENV_NET_WAIT)
			envs[i].env_status = ENV_RUNNABLE;

	// check tx completions
	check_tx_completions();
	
	return;
}

static uint16_t
e1000_eeprom_read(uint8_t addr)
{
	// Clear ADDR and DATA
	E1000_REG(E1000_EERD) &= ~(E1000_EERD_ADDR_MASK | E1000_EERD_DATA_MASK);
	// Simultanionsly Place the address and turn on the start bit
	E1000_REG(E1000_EERD) |= (addr << E1000_EERD_ADDR_SHIFT) | E1000_EERD_START;
	// epoll on EERD until hardware sets EERD.DONE
	while (!(E1000_REG(E1000_EERD) & E1000_EERD_DONE)) {
		//noice
	}
	// read EERD.DATA
	return (E1000_REG(E1000_EERD) & E1000_EERD_DATA_MASK) >> E1000_EERD_DATA_SHIFT;
}

uint64_t
e1000_eeprom_get_hwaddr(void)
{
	return (e1000_eeprom_read(E1000_EEPROM_ETHADDR_WORD0)) |
		   ( e1000_eeprom_read(E1000_EEPROM_ETHADDR_WORD1) << 16) |
		   ((uint64_t) e1000_eeprom_read(E1000_EEPROM_ETHADDR_WORD2) << 32);
}

void
check_tx_completions(void)
{
	static int last_cleaned = 0;
	while (tx_queue[last_cleaned].status & E1000_TX_STAT_DD && last_cleaned < tail_idx) {
		// Hardware is done with this descriptor
		physaddr_t pa = tx_queue[last_cleaned].buf_addr;
		struct PageInfo *p = pa2page(pa);

		// Decrement refrence count - page can now be freed if needed
		page_decref(p);

		last_cleaned = (last_cleaned + 1) % NTDX;
	}
}
