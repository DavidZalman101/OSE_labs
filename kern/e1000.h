#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

// Includes
#include <kern/pci.h>
#include <kern/pmap.h>
#include <kern/picirq.h>
#include <kern/env.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/ns.h> // For union Nspic definition O_o

// Defines
#define E1000_VEN_ID 0x8086
#define E1000_DEV_ID_82540EM 0x100E
#define DEFAULT_MAC_ADDRESS_LOW_ORDER  0x12005452
#define DEFAULT_MAC_ADDRESS_HIGH_ORDER 0x00005634

/*	Register Set.

	Registers are defined to be 32 bits and should be accessed as 32 bit values.
	There register are physically located on the NIC, but are mapped into the host
	memory address sapce.

	RW - register is both readable and writable.
	RO - register is read only.
	WO - register is write only.
	R/clr - register is read only and is cleared when read.
	A - register array.
*/
#define E1000_REG(offset) (*(volatile uint32_t*)(e1000_base + offset)) /* Read Register at offset */

#define E1000_CTRL   0x00000 /* Device Control - RW       */
#define E1000_STATUS 0x00008 /* Device Status - RO        */
#define E1000_TCTL   0x00400 /* Transmit Control - RW     */
#define E1000_TIPG   0x00410 /* Transmit Control - RW     */
#define E1000_TDBAL  0x03800 /* TX Base Address Low - RW  */
#define E1000_TDBAH  0x03804 /* TX Base Address High - RW */
#define E1000_TDLEN  0x03808 /* TX Length - RW            */
#define E1000_TDH    0x03810 /* TX Head - RW              */
#define E1000_TDT    0x03818 /* TX Tail - RW              */

/* Transmit Control */
#define E1000_TCTL_RST    0x00000001 /* software reset                */
#define E1000_TCTL_EN     0x00000002 /* enable tx                     */
#define E1000_TCTL_BCE    0x00000004 /* busy check enable             */
#define E1000_TCTL_PSP    0x00000008 /* pad short packets             */
#define E1000_TCTL_CT     0x00000ff0 /* collision threshold           */
#define E1000_TCTL_COLD   0x003ff000 /* collision distance            */
#define E1000_TCTL_SWXOFF 0x00400000 /* SW Xoff transmission          */
#define E1000_TCTL_PBE    0x00800000 /* Packet Burst Enable           */
#define E1000_TCTL_RTLC   0x01000000 /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000 /* No Re-transmit on underrun    */
#define E1000_TCTL_MULR   0x10000000 /* Multiple request support      */

#define E1000_TCTL_CT_SHIFT   0x04 /* collision threshold shift */
#define E1000_TCTL_COLD_SHIFT 0x0c /* collision distance shift  */

/* Transmit Descriptor Status */
#define E1000_TX_STAT_DD_SHIFT     0x00 /* TX Status DD     */
#define E1000_TX_STAT_EC_SHIFT     0x01 /* TX Status EC     */
#define E1000_TX_STAT_LC_SHIFT     0x02 /* TX Status LC     */
#define E1000_TX_STAT_RSV_RU_SHIFT 0x03 /* TX Status RSV-TU */

#define E1000_TX_STAT_DD		   0x01 /* TX Status DD */


/* Transmit Descriptor Command */
#define E1000_TX_CMD_EOP_SHIFT     0x00 /* TX CMD EOP     */
#define E1000_TX_CMD_IFCS_SHIFT    0x01 /* TX CMD IFCS    */
#define E1000_TX_CMD_IC_SHIFT      0x02 /* TX CMD IC      */
#define E1000_TX_CMD_RS_SHIFT      0x03 /* TX CMD RS      */
#define E1000_TX_CMD_RSV_RPS_SHIFT 0x04 /* TX CMD RSV_RPC */
#define E1000_TX_CMD_DEXT_SHIFT    0x05 /* TX CMD DEXT    */
#define E1000_TX_CMD_VLE_SHIFT     0x06 /* TX CMD VLE     */
#define E1000_TX_CMD_IDE_SHIFT     0x07 /* TX CMD IDE     */

#define E1000_TX_CMD_RS		       0x08 /* TX CMD  RS       */
#define E1000_TX_CMD_EOP		   0x01 /* TX CMD  RS       */

/* Transmit Descriptor TIPG */
# define E1000_TIPG_IPGT_MASK  0x000003FF
# define E1000_TIPG_IPGR1_MASK 0x000FFC00
# define E1000_TIPG_IPGR2_MASK 0x3FF00000

/* Receive */
#define E1000_RAL    0x05400 /* Receive Address Low - RW Array              */
#define E1000_RAH    0x05404 /* Receive Address High - RW Array             */
#define E1000_RAH_AV (1<<31) /* Receive Address High Address Valid - RW bit */
#define E1000_MTA    0x05200 /* Multicast Table Array - RW Array            */
#define E1000_RDBAL  0x02800 /* RX Descriptor Base Address Low - RW         */
#define E1000_RDBAH  0x02804 /* RX Descriptor Base Address High - RW        */
#define E1000_RDLEN  0x02808 /* RX Descriptor Length - RW                   */
#define E1000_RDH    0x02810 /* RX Descriptor Head - RW                     */
#define E1000_RDT    0x02818 /* RX Descriptor Tail - RW                     */
#define E1000_RCTL   0x00100 /* RX Control - RW                             */


/* Receive Control */
#define E1000_RCTL_EN    0x00000002 /* enable                */
#define E1000_RCTL_LPE   0x00000020 /* long packet enable    */
#define E1000_RCTL_LBM   0x000000c0 /* loopback mode         */
#define E1000_RCTL_SZ    0x00030000 /* rx buffer size        */
#define E1000_RCTL_BSEX  0x02000000 /* Buffer size extension */
#define E1000_RCTL_SECRC 0x04000000 /* strip ethernet CRC    */

// MO
#define E1000_RCTL_MO_SHIFT 12         /* multicast offset shift */
#define E1000_RCTL_MO_0     0x00000000 /* multicast offset 11:0  */
#define E1000_RCTL_MO_1     0x00001000 /* multicast offset 12:1  */
#define E1000_RCTL_MO_2     0x00002000 /* multicast offset 13:2  */
#define E1000_RCTL_MO_3     0x00003000 /* multicast offset 15:4  */
#define E1000_RCTL_MDR      0x00004000 /* multicast desc ring 0  */

// RDMTS
#define E1000_RCTL_RDMTS       0x00000300 /* Min Threshold Size         */
#define E1000_RCTL_RDMTS_HALF  0x00000000 /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_QUAT  0x00000100 /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_EIGTH 0x00000200 /* rx desc min threshold size */

// BAM
#define E1000_RCTL_BAM 0x00008000    /* broadcast enable */

/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048 0x00000000 /* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024 0x00010000 /* rx buffer size 1024 */
#define E1000_RCTL_SZ_512  0x00020000 /* rx buffer size 512  */
#define E1000_RCTL_SZ_256  0x00030000 /* rx buffer size 256  */

/* Receive Descriptor STAT */
#define E1000_RXD_STAT_DD    0x01   /* Descriptor Done         */
#define E1000_RXD_STAT_EOP   0x02   /* End of Packet           */
#define E1000_RXD_STAT_IXSM  0x04   /* Ignore checksum         */
#define E1000_RXD_STAT_VP    0x08   /* IEEE VLAN Packet        */
#define E1000_RXD_STAT_UDPCS 0x10   /* UDP xsum caculated      */
#define E1000_RXD_STAT_TCPCS 0x20   /* TCP xsum calculated     */
#define E1000_RXD_STAT_IPCS  0x40   /* IP xsum calculated      */
#define E1000_RXD_STAT_PIF   0x80   /* passed in-exact filter  */
#define E1000_RXD_STAT_IPIDV 0x200  /* IP identification valid */
#define E1000_RXD_STAT_UDPV  0x400  /* Valid UDP checksum      */
#define E1000_RXD_STAT_ACK   0x8000 /* ACK Packet indication   */

/* Receive Descriptor ERR */
#define E1000_RXD_ERR_CE   0x01 /* CRC Error               */
#define E1000_RXD_ERR_SE   0x02 /* Symbol Error            */
#define E1000_RXD_ERR_SEQ  0x04 /* Sequence Error          */
#define E1000_RXD_ERR_CXE  0x10 /* Carrier Extension Error */
#define E1000_RXD_ERR_TCPE 0x20 /* TCP/UDP Checksum Error  */
#define E1000_RXD_ERR_IPE  0x40 /* IP Checksum Error       */
#define E1000_RXD_ERR_RXE  0x80 /* Rx Data Error           */

/* Receive Descriptor SPC */
#define E1000_RXD_SPC_VLAN_MASK 0x0FFF /* VLAN ID is in lower 12 bits */
#define E1000_RXD_SPC_PRI_MASK  0xE000 /* Priority is in upper 3 bits */
#define E1000_RXD_SPC_PRI_SHIFT 13
#define E1000_RXD_SPC_CFI_MASK  0x1000 /* CFI is bit 12 */
#define E1000_RXD_SPC_CFI_SHIFT 12

/* Intrrupts */
#define E1000_IMS 	     0x000D0    /* Interrupt Mask Set - RW      */
#define E1000_IMC 	     0x000D8    /* Interrupt Mask Clear - WO    */
#define E1000_IMS_RXDMT0 0x00000010 /* rx desc min. threshold       */
#define E1000_ICR_RXT0   0x00000080 /* rx timer intr (ring 0)       */
#define E1000_IMS_RXT0   0x00000080 /* rx timer intr                */
#define E1000_ICR        0x000C0    /* Interrupt Cause Read - R/clr */
#define E1000_ICR_RXDMT0 0x00000010 /* rx desc min. threshold (0)   */
#define E1000_ICR_TXQE   0x00000002 /* Transmit Queue empty         */

/* EEPROM */
#define E1000_EERD            0x00014 /* EEPROM Read - RW */
#define E1000_EERD_START      0x00000001
#define E1000_EERD_DONE       0x00000010
#define E1000_EERD_ADDR_SHIFT 8
#define E1000_EERD_ADDR_MASK  0x0000ff00
#define E1000_EERD_DATA_SHIFT 16
#define E1000_EERD_DATA_MASK  0xffff0000

/* EEPROM ETHADDR */
#define E1000_EEPROM_ETHADDR_WORD0  0x00
#define E1000_EEPROM_ETHADDR_WORD1  0x01
#define E1000_EEPROM_ETHADDR_WORD2  0x02

// Errors
#define E1000_ERR_IVALID_ARG     1
#define E1000_ERR_TX_QUEUE_FULL  2
#define E1000_ERR_TX_PKT_TOO_BIG 3
#define E1000_ERR_RX_PKT_TOO_BIG 4
#define E1000_ERR_RX_QUEUE_EMPTY 5

// Constants
#define NTDX         64   /* Number of TDESC      */
#define TDX_BUF_SIZE 2048 /* Tranmist Buffer Size */
#define NRDX         128  /* Number of RDESC      */
#define RDX_BUF_SIZE 2048 /* Receive Buffer Size  */

// Structures
// Transmit Descriptor (TDESC) Layout - Legacy Mode
struct tx_desc {
	uint64_t buf_addr;
	uint16_t len;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status; // [TU-RSV, LC, EC, DD][Reserved]
	uint8_t css;
	uint16_t special;
} __attribute__((packed));

// Receive Descriptor (RDESC) Layout
struct rx_desc {
	uint64_t buf_addr;
	uint16_t len;
	uint16_t csum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
} __attribute__((packed));

// Globals vars
volatile uint8_t *e1000_base;
extern uint8_t rx_bufs[NRDX][PGSIZE];

// Function Declartions
int e1000_attach(struct pci_func *);
static void transmit_init(void);
static void receive_init(void);
int transmit_packet(void *, uint32_t );
int receive_packet(uint32_t*);
void e1000_interrupt(void);
static uint16_t e1000_eeprom_read(uint8_t addr);
uint64_t e1000_eeprom_get_hwaddr(void);
int receive_packet_done(int);
void check_tx_completions(void);
#endif	// JOS_KERN_E1000_H
