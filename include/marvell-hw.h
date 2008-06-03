#ifndef MARVELL_HW_H
#define MARVELL_HW_H

typedef struct hw_mcr_chan_t
{
	u32 cfgr;	// configuration register
	u32 cs;		// timing parameter 0
	u32 sc;		// timing parameter 1
} hw_mcr_chan_t;

typedef struct hw_mcr_t
{
	u32 csr;		// control and status register
	hw_mcr_chan_t channels[4];
} hw_mcr_t;

#define HW_MCR_BASE	((hw_mcr_t volatile *) 0x80006000)


typedef struct hw_sysconf_t
{
	u32 scr;	// system configuration register
	u32 arb_lo;	// bus arbiter low
	u32 arb_hi;	// bus arbiter hi
} hw_sysconf_t;

#define HW_SYSCONF_BASE ((hw_sysconf_t volatile *) 0x80002000)

typedef struct hw_interrupt_t
{
	u32 rsr;	// request status
	u32 ssr;	// source status
	u32 ier;	// interrupt enable/query
	u32 idr;	// interrupt disable
	u32 softint;	// software-programmed interrupt, only in IRQ mode
} hw_interrupt_t;

#define HW_IRQ_BASE ((hw_interrupt_t volatile *) 0x90008000)
#define HW_FIQ_BASE ((hw_interrupt_t volatile *) 0x90008100)

typedef struct hw_timer_t
{
	u32 tlength[4];
	u32 cr;
	u32 tvalue[4];
	u32 irq;
	u32 imask;
} hw_timer_t;

#define HW_TIMER_BASE ((hw_timer_t volatile *) 0x90009000)

typedef struct hw_unimac_smi_t
{
	u32 smir;
} hw_unimac_smi_t;

typedef struct hw_unimac_t
{
	u32 pcr;	// port configuration
	u32 reserved0;
	u32 pcxr;	// extended port configuration
	u32 reserved1;
	u32 pcmr;	// command register
	u32 reserved2;
	u32 psr;	// status register
	u32 reserved3[3];
	u32 htpr;	// hash table pointer	-- todo: type?
	u32 reserved4;
	u32 fcsal;	// flow control source lo
	u32 reserved5;
	u32 fcsah;	// flow control source hi
	u32 reserved6;
	u32 sdcr;	// sdma configuration
	u32 reserved7;
	u32 sdcmr;	// sdma command
	u32 reserved8;
	u32 icr;	// interrupt cause
	u32 irsr;	// interrupt reset select
	u32 imr;

	u32 reserved9;

	// ip diffsvc support
	u32 dsc0l;	// diffsvc codepoint -> priority 0
	u32 dsc0h;	
	u32 dsc1l;	// diffsvc codepoint -> priority 1
	u32 dsc1h;
	u32 vpt2p;	// vlan priority tag to priority mapping

	u32 reserved10[3];

	// dma queues
	u32 first_rx_ptr[4];	// ptr to first descriptor for each queue
	u32 reserved11[4];		// 16 bytes of nothing
	u32 current_rx_ptr[4];	// ptr to current descriptor for each queue
	u32 reserved12[12];		// 48 bytes of nothing
	u32 current_tx_ptr[2];	// ptr to current tx descriptor for each queue
} hw_unimac_t;

#define HW_UNIMAC_SMI_BASE	((hw_unimac_smi_t volatile *) 0x80008010 )
#define HW_UNIMAC_BASE		((hw_unimac_t volatile *) 0x80008400 )

typedef struct hw_dma_t
{
	// there's actually only 2 DMA channels, but declaring as if we had 4 makes
	// the structure a lot saner-looking
	u32 count[4];
	u32 src[4];
	u32 dest[4];
	u32 next_desc[4];
	u32 control[4];

	u32 reserved0[4];

	u32 arbiter_control;
	u32 reserved1[3];

	u32 current_desc[4];
	u32 interupt_mask[4];
	u32 interrupt_reset_select[4];
	u32 interrupt_status[4];
} hw_dma_t;

#define HW_DMA_BASE	((hw_dma_t volatile *) 0x8000e800 )

#endif