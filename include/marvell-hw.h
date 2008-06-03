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

#endif