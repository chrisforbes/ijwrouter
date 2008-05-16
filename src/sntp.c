#include "common.h"
#include "hal_debug.h"
#include "hal_time.h"

#include "ip/rfc.h"
#include "ip/udp.h"
#include "ip/conf.h"

#define SNTP_PORT	123
#define MIN_INTERVAL	5000			// once per five minutes
#define MAX_INTERVAL	3600000			// once per hour

#define TIMEOUT			3000				// 5 seconds
#define RESYNC_INTERVAL	7200000			// once every two hours

static enum { SNTP_NOT_READY, SNTP_IDLE, SNTP_WAITING_FOR_RESPONSE } state;
static u32 interval = MIN_INTERVAL;
static u32 next_sync_time = 0;

static udp_sock sock;

static u08 timeserver[] = { 192, 43, 244, 18 };	// time.nist.gov
					// todo: use DNS here, so their load-balancing *works*
					// todo: use alternate time sources if the primary isn't available

typedef struct sntp_timeval_t { u32 seconds; u32 frac; } sntp_timeval_t;

typedef struct sntp_t
{
	u08 version, stratum, poll, precision;
	u32 root_delay;
	u32 root_dispersion;
	u32 reference_ident;

	sntp_timeval_t reftime;
	sntp_timeval_t otime;
	sntp_timeval_t rxtime;
	sntp_timeval_t txtime;
} sntp_t;

void sntp_send_request( void )
{
	sntp_t request = { 0x61, 0 };
	udp_send( sock, make_ip( timeserver[0], timeserver[1], timeserver[2], timeserver[3] ), 
		SNTP_PORT, (u08 const *)&request, sizeof( sntp_t ) );

	logf( "sntp: request sent\n" );
}

void sntp_get_response( sntp_t const * resp )
{
	if ((resp->version & 0x3) == 0x3)	// unsynchronized
	{
		logf( "sntp: time server reports it is unsynchronized\n" );
		return;
	}

	interval = MIN_INTERVAL;	// retry timer
	next_sync_time = ticks() + RESYNC_INTERVAL;
	state = SNTP_IDLE;

	// todo: set time offset
	set_time( resp->txtime.seconds );

	logf( "sntp: clock synchronized.\n" );
}

static void sntp_event( udp_sock sock, udp_event_e evt, 
	u32 from_ip, u16 from_port, u08 const * data, u16 len )
{
	if (evt != UDP_EVENT_PACKET) return;
	sock; from_ip; from_port; data;

	if (len != sizeof(sntp_t))
	{
		logf( "sntp: bad response size (was %d, expected %d)\n", len, sizeof(sntp_t) );
		return;
	}

	sntp_get_response( (sntp_t const *) data );
}

void sntp_init( void )
{
	sock = udp_new_sock( SNTP_PORT, 0, sntp_event );
	if (sock == INVALID_UDP_SOCK)
	{
		logf( "sntp: unable to bind socket\n" );
		return;
	}

	logf( "sntp: started successfully\n" );
	state = SNTP_IDLE;
}

void sntp_process( void )
{
	//logf( "sntp: state=%d dt=%d\n", state, ticks() - next_sync_time );
	switch( state )
	{
	case SNTP_NOT_READY: 
		return;	// we fail at being ready - probably host configuration isnt finished yet

	case SNTP_IDLE:
		if (ticks() > next_sync_time)
		{
			sntp_send_request();
			next_sync_time = ticks() + TIMEOUT;
			state = SNTP_WAITING_FOR_RESPONSE;
		}
		break;

	case SNTP_WAITING_FOR_RESPONSE:
		if (ticks() > next_sync_time)
		{
			next_sync_time = ticks() + interval;
			interval *= 2;
			if (interval > MAX_INTERVAL)
				interval = MAX_INTERVAL;
			state = SNTP_IDLE;
		}
	}
}