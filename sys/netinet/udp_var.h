/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)udp_var.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

#ifndef _NETINET_UDP_VAR_H_
#define	_NETINET_UDP_VAR_H_

/*
 * UDP kernel structures and variables.
 */
struct udpiphdr {
	struct ipovly	ui_i;		/* overlaid ip structure */
	struct udphdr	ui_u;		/* udp header */
};
#define	ui_x1		ui_i.ih_x1
#define	ui_pr		ui_i.ih_pr
#define	ui_len		ui_i.ih_len
#define	ui_src		ui_i.ih_src
#define	ui_dst		ui_i.ih_dst
#define	ui_sport	ui_u.uh_sport
#define	ui_dport	ui_u.uh_dport
#define	ui_ulen		ui_u.uh_ulen
#define	ui_sum		ui_u.uh_sum

struct udpstat {
				/* input statistics: */
	u_long	udps_ipackets;		/* total input packets */
	u_long	udps_hdrops;		/* packet shorter than header */
	u_long	udps_badsum;		/* checksum error */
	u_long	udps_nosum;		/* no checksum */
	u_long	udps_badlen;		/* data length larger than packet */
	u_long	udps_noport;		/* no socket on port */
	u_long	udps_noportbcast;	/* of above, arrived as broadcast */
	u_long	udps_fullsock;		/* not delivered, input socket full */
	u_long	udpps_pcbcachemiss;	/* input packets missing pcb cache */
	u_long	udpps_pcbhashmiss;	/* input packets not for hashed pcb */
				/* output statistics: */
	u_long	udps_opackets;		/* total output packets */
	u_long	udps_fastout;		/* output packets on fast path */
	/* of no socket on port, arrived as multicast */
	u_long	udps_noportmcast;
	u_long	udps_filtermcast;	/* blocked by multicast filter */
};

/*
 * Names for UDP sysctl objects.
 */
#define	UDPCTL_CHECKSUM		1	/* checksum UDP packets */
#define	UDPCTL_STATS		2	/* statistics (read-only) */
#define	UDPCTL_MAXDGRAM		3	/* max datagram size */
#define	UDPCTL_RECVSPACE	4	/* default receive buffer space */
#define	UDPCTL_PCBLIST		5	/* list of PCBs for UDP sockets */
#define	UDPCTL_MAXID		6

#define	UDPCTL_NAMES	{						\
	{ 0, 0 },							\
	{ "checksum", CTLTYPE_INT },					\
	{ "stats", CTLTYPE_STRUCT },					\
	{ "maxdgram", CTLTYPE_INT },					\
	{ "recvspace", CTLTYPE_INT },					\
	{ "pcblist", CTLTYPE_STRUCT },					\
}

#ifdef _KERNEL
SYSCTL_DECL(_net_inet_udp);

extern struct pr_usrreqs	udp_usrreqs;

#ifdef VIMAGE_GLOBALS
extern struct inpcbhead		udb;
extern struct inpcbinfo		udbinfo;
extern struct udpstat		udpstat;
extern int			udp_blackhole;
#endif
extern u_long			udp_sendspace;
extern u_long			udp_recvspace;
extern int			udp_log_in_vain;

void		 udp_ctlinput(int, struct sockaddr *, void *);
void		 udp_init(void);
void		 udp_input(struct mbuf *, int);
struct inpcb	*udp_notify(struct inpcb *inp, int errno);
int		 udp_shutdown(struct socket *so);


typedef void(*udp_tun_func_t)(struct mbuf *, int off, struct inpcb *);
int udp_set_kernel_tunneling(struct socket *so, udp_tun_func_t f);
#endif

#endif
