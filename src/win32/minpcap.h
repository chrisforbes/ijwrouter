#ifndef MINPCAP_H
#define MINPCAP_H

#include <time.h>

#define pcap_t void

typedef struct pcap_if_t {
	struct pcap_if_t *next;
	char * name;
	char * description;
	struct pcap_addr *addresses;
	u32 flags;
} pcap_if_t;

#define PCAP_ERRBUF_SIZE 256

typedef struct timeval {
        long    tv_sec;         /* seconds */
        long    tv_usec;        /* and microseconds */
} timeval;

typedef struct pcap_pkthdr {
	timeval ts;	/* time stamp */
	u32 caplen;	/* length of portion present */
	u32 len;	/* length this packet (off wire) */
} pcap_pkthdr;

int	pcap_findalldevs(pcap_if_t **, char *);
void	pcap_freealldevs(pcap_if_t *);
pcap_t	*pcap_open_live(const char *, int, int, int, char *);
u08 const * pcap_next(pcap_t *, pcap_pkthdr *);
int	pcap_sendpacket(pcap_t *, const u08 *, int);

#endif