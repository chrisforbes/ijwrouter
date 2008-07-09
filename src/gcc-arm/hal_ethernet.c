#include "../common.h"
#include "../hal_debug.h"
#include "../ip/rfc.h"
#include "../hal_ethernet.h"
#include "../../include/marvell-hw.h"

#include "../pack1.h"
typedef struct dma_descriptor_t
{
	u32 status;
	u16 size, count;
	void * data;
	struct dma_descriptor_t * next;
} PACKED_STRUCT dma_descriptor_t;
#include "../packdefault.h"

#define NUM_DESCRIPTORS		16
#define ETH_BUFFER_SIZE		2048
static dma_descriptor_t rx_desc[NUM_DESCRIPTORS];
static dma_descriptor_t * rx_first = 0;		// first available (handed off by unimac)

static void prepare_buffer_chain( dma_descriptor_t * p, u08 n )
{
	dma_descriptor_t * first = p;

	while( n-- )
	{
		p->data = malloc( ETH_BUFFER_SIZE );	// todo: 8-byte align
		p->size = ETH_BUFFER_SIZE;
		p->count = 0;
		p->status = (1 << 31) |	// unimac owns the buffer
					(1 << 23);	// enable interrupt
		p->next = n ? (p+1) : first;	// circular buffer for infinite DMA
		++p;
	}
}

u08 eth_init( void )
{
	// todo: ensure descriptors are aligned to 16-byte boundaries, and
	//	buffers are aligned to 8-byte boundaries. (hardware limitation)

	prepare_buffer_chain( rx_desc, NUM_DESCRIPTORS );

	HW_UNIMAC_BASE->first_rx_ptr[0] = (u32)rx_desc;
	HW_UNIMAC_BASE->current_rx_ptr[0] = (u32)rx_desc;

	HW_UNIMAC_BASE->pcxr = (3 << 22) |	// marvell header mode
						   (3 << 18) | (1 << 14) | (3 << 9);	// reserved SBO
	HW_UNIMAC_BASE->sdcr = (3 << 12) | // DMA burst size = 32 bytes
						   (1 << 9)	| // receive interrupt on frame boundary
						   (3 << 6); // little-endian native mode for tx/rx

	// todo: other necessary initialization
	// todo: configure the switch via SMI
	// todo: wire up interrupt handler

	HW_IRQ_BASE->ier = (1 << 9);	// enable unimac interrupt

	HW_UNIMAC_BASE->pcr = (1 << 15) | //reserved SBO
						  (1 << 7); // enable unimac!

	HW_UNIMAC_BASE->sdcmr = (1 << 7);	// enable rx DMA

	// todo: tx configuration
	return 1;
}

// todo: hook this as an ISR for source 9
void eth_rx_isr( void )
{
	if (!rx_first)
		rx_first = HW_UNIMAC_BASE->first_rx_ptr[0];
}

u08 eth_getpacket( eth_packet * p )
{
	HW_UNIMAC_BASE->idr = (1 << 9);	// turn off unimac interrupt

	dma_descriptor_t * d = rx_first;
	if (d)
	{
		rx_first = d->next;
		if (rx_first->status & (1 << 31)) 
			rx_first = 0;		// todo: check this is sortof correct?
	}
	
	HW_UNIMAC_BASE->ier = (1 << 9);	// restore unimac interrupt

	if (!d) return 0;	// no packet

	// todo: check for broken packet
	// todo: write this packet into the eth_packet provided
	// todo: 
	return 1;
}