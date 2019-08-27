/*
 * FILENAME: targnios.c
 *
 * Copyright 2004-2008 InterNiche Technologies Inc. All rights reserved.
 *
 * Target board support routines for InterNiche TCP/IP.
 * This one for the ALTERA Cyclone board with the ALTERA Nios2 Core running
 * with the uCOS-II RTOS.
 *
 * This file for:
 *   ALTERA Cyclone Dev board with the ALTERA Nios2 Core.
 *   SMSC91C11 10/100 Ethernet Controller
 *   GNU C Compiler provided by ALTERA Quartus Toolset.
 *   Quartus HAL BSP
 *   uCOS-II RTOS Rel 2.76 as distributed by Altera/NIOS2
 *
 * MODULE: NIOS2GCC
 * ROUTINES: prep_enet(), pre_task_setup(), post_task_setup()
 * PORTABLE: no
 *
 * 06/21/2004
 * 10/21/2008 (skh)
 * 
 */

/* Nichestack definitions */
#include <stm32h7xx_hal.h>

/* Includes ------------------------------------------------------------------*/
#include "cpu.h"

#include "ipport.h"
#include "libport.h"
#include "tcpport.h"
#include "net.h"
#include "in_utils.h"
#include "memwrap.h"
#include "ifec.h"

#ifdef VFS_FILES
#include "vfsfiles.h"
#endif

/*
 * Altera Niche Stack Nios port modification:
 * Conditionally include OS-dependant code.
 */
#ifndef SUPERLOOP
#include "osport.h"
#endif

#ifdef USE_PPP
#include "ppp_port.h"
#include "comline.h"
#include "ifmap.h"
#include "../mppp/mppp.h"
extern int ppp_default_type;  /* in ppp code */
extern int ppp_static;     /* number static PPP ifaces to set */
#endif   /* USE_PPP */

#ifdef USE_MODEM
#include "../modem/mdmport.h"
#endif

#ifdef INCLUDE_NVPARMS
#include "nvparms.h"
#endif

#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
#include "igmp_cmn.h"
#endif /* IP multicast and (IGMPv1 or IGMPv2) */

#ifdef SMTP_ALERTS
#include "smtpport.h"
#endif


/*
 * Altera Niche Stack Nios port modification:
 * Move conditional compilation on SUPERLOOP so that the following
 * code will be used w/ SUPERLOOP
 */

#include <ether.h>

extern struct net netstatic[MAXNETS];   

#if defined(MEMDEV_SIZE) && defined(VFS_FILES)
int   init_memdev(void);
#endif

#ifdef INCLUDE_NVPARMS
void nv_defaults(void);   /* set default Integrator NV parameters */
#endif  /* INCLUDE_NVPARMS */

extern int (*port_prep)(int);
extern int prep_armintcp(int ifaces_found);

#ifdef USE_PPP
extern int prep_ppp(int ifaces_found);
extern int   (*ppp_type_callback)(LINEP);
extern int   ppp_type_setup(LINEP);
#endif
#ifdef USE_SLIP
extern int prep_slip(int ifaces_found);
#endif

/* hardware setup called from main before anything else (e.g.
 * before tasks, printf, memory alloc, etc. 
 *
 * Return NULL if OK, else brief error message
 */

char *
pre_task_setup()
{
   /* preset buffer counts; may be overridden from command line */
   bigbufs = NUMBIGBUFS;
   lilbufs = NUMLILBUFS;
   bigbufsiz = BIGBUFSIZE;
   lilbufsiz = LILBUFSIZE;

   /* Install callback to prep_armintcp from prep_ifaces() */
   port_prep = prep_armintcp;

/*
 * Altera Niche Stack Nios port modification:
 * The alt_iniche_dev initialization code provides
 * for calling get_ip_addr() (which is in *application* 
 * code) for IP address assignment, for each mac (net).
 * The following is thus disabled.
 */
#ifndef INCLUDE_NVPARMS
   {
      int i = 0;              /* network index */

#ifdef USE_SMSC91X
      /* Ethernet */
      if (i < MAXNETS)
      {
         netstatic[i].n_ipaddr = htonl(0x0a000064);  /* 10.0.0.100 */
         netstatic[i].snmask = htonl(0xff000000);    /* 255.0.0.0 */
         netstatic[i].n_defgw = htonl(0x0a000001);   /* 10.0.0.1 */
#ifdef DHCP_CLIENT
         netstatic[i].n_flags |= NF_DHCPC;
#endif
         i++;
      }
#endif /* SMSC91x */

#ifdef USE_PPP
      /* PPP */
      if (i < MAXNETS)
      {
         netstatic[i].n_ipaddr = htonl(0xc0a80301);  /* 192.168.3.1 */
         netstatic[i].snmask = htonl(0xffffff00);    /* 255.255.255.0 */
         netstatic[i].n_defgw = htonl(0x00000000);   /* 0.0.0.0 */
         i++;
      }

#ifdef LB_XOVER
      if (i < MAXNETS)
      {
         netstatic[i].n_ipaddr = htonl(0x00000000);
         netstatic[i].snmask = htonl(0x00000000);
         netstatic[i].n_defgw = htonl(0x00000000);
         i++;
      }
#endif /* LB_XOVER */
#endif /* USE_PPP */

#ifdef MAC_LOOPBACK
      /* loopback */
      if (i < MAXNETS)
      {
         netstatic[i].n_ipaddr = htonl(0x7f000001); /* 127.0.0.1 */
         netstatic[i].snmask = htonl(0xff000000);   /* 255.0.0.0 */
         netstatic[i].n_defgw = htonl(0x00000000);  /* 0.0.0.0 */
         i++;
      }
#endif /* MAC_LOOPBACK */
   }

#else  /* INCLUDE_NVPARMS is used */

   /* set callback to set up default NV values. The generic NV system 
    * code in misclib\nvparams.c may call this NIOS2INI specific 
    * routine if it is set.
    */
   set_nv_defaults = nv_defaults;   /* set nv parameter init routine */

#endif /* INCLUDE_NVPARMS */
#ifdef   USE_PPP
#ifdef   MINI_IP
   /* If we are using the mini IP layer and PPP, then  overwrite the 
    * NDIS hook with the PPP prep routine 
    */
   port_prep = prep_ppp;
#endif   /* MINI_IP */

   /* for all PPP implementations, set a callback so the we can
    * set the device type of new PPP links.
    */
   ppp_default_type = LN_PORTSET;
   ppp_type_callback = ppp_type_setup;

   /* Set the number of static PPP ifaces. Allow one each for modem and
    * PPPOE, and two for Loopback-crossover.
    */
   ppp_static = 1;   /* base for modem or UART direct connect */

#ifdef LB_XOVER
   ppp_static += 2;     /* add pair for loopback crosover */
#endif   /* LB_XOVER */


#ifdef NOTDEF
   /* This little block of code is usefully for testing PPPoE in loopback
    * on windows (e.g. bouncing the packets off the NDIS driver) but for
    * most purposes should not be used.
    */
#ifdef USE_PPPOE
#ifdef PPPOE_LBTEST
   ppp_static += 2;     /* loopback test needs 2 interfaces */
#else
   ppp_static++;
#endif   /* PPPOE_LBTEST */
#endif   /* USE_PPPOE */
#endif   /* NOTDEF */

#endif   /* USE_PPP */


   return NULL;
}

/* more setup called after tasks are set up
 *
 * Return NULL if OK, else brief error message
 */

char *
post_task_setup()
{
   return NULL;
}


/*
 * memtrapsize - memory allocator trap size
 */
unsigned memtrapsize = 0;

#ifdef INCLUDE_NVPARMS

/* FUNCTION: nv_defaults()
 *
 * This is called from the NV parameters syystem if there is
 * no NV file in flash. It builds a list of functional default
 * NV values for testing and/or system generation.
 *
 * PARAM1: none
 *
 * RETURNS: void
 */

static char * logfilename = "login.nv";
static char * srvfilename = "server.nv";
static char * natfilename = "natdb.nv";


void
nv_defaults()
{
   int   iface = 0;
   VFILE *fp;
   int   e;

   /* store default IP info */
#ifdef USE_SMSC91X
   inet_nvparms.ifs[iface].ipaddr =  htonl(0x0a000067);   /* 10.0.0.106 */
#ifdef USE_PPPOE
   inet_nvparms.ifs[iface].subnet = htonl(0xfffe0000);    /* 255.254.0.0 */
   inet_nvparms.ifs[iface].gateway = htonl(0x00000000);   /* 0.0.0.0 */
#else
   inet_nvparms.ifs[iface].subnet = htonl(0xff000000);    /* 255.0.0.0 */
   inet_nvparms.ifs[iface].gateway = htonl(0x0a000001);   /* 10.0.0.1 */
#endif
   inet_nvparms.ifs[iface].client_dhcp = 0;

   dprintf("nv_defaults: set net %d IP to %u.%u.%u.%u\n",
    iface, PUSH_IPADDR(inet_nvparms.ifs[iface].ipaddr) );

   iface++;
#endif /* */

#ifdef USE_PPP
   /* PPP */
   inet_nvparms.ifs[iface].ipaddr = htonl(0x00000000);
   inet_nvparms.ifs[iface].subnet = htonl(0xffffff00);    /* 255.255.255.0 */
   inet_nvparms.ifs[iface].gateway = htonl(0xc0a80801);   /* 192.168.8.1 */
   iface++;

#ifdef LB_XOVER
   inet_nvparms.ifs[iface].ipaddr = htonl(0x00000000);
   inet_nvparms.ifs[iface].subnet = htonl(0x00000000);
   inet_nvparms.ifs[iface].gateway = htonl(0x00000000);
   iface++;
#endif /* LB_XOVER */

#endif /* USE_PPP */

   /* loopback */
   inet_nvparms.ifs[iface].ipaddr = htonl(0x00000000);
   inet_nvparms.ifs[iface].subnet = htonl(0x00000000);
   inet_nvparms.ifs[iface].gateway = htonl(0x00000000);
#ifdef IP_MULTICAST
   inet_nvparms.ifs[iface].igmp_oper_mode = IGMP_MODE_DEFAULT;
#endif
#ifdef USE_COMPORT
   comport_nvparms.LineProtocol = 1;
#endif   /* defined (USE_PPP) || defined(USE_SLIP) */

#ifdef   USE_PPP
   ppp_nvparms.ppp_ConsoleLog = 0;    /* Log Modem & PPP events to console */
   ppp_nvparms.ppp_FileLog = 0;       /* Log Modem & PPP events to file */
   ppp_nvparms.ppp_keepalive = 0;     /* seconds between PPP echos, 0=disable */
   ppp_nvparms.ppp_client_tmo = 500;  /* timeout for connects as client */
   ppp_nvparms.line_tmo = 300;
#ifdef PPP_VJC
   ppp_nvparms.ppp_request_vj = 1;    /* request that the other side do VJ compression */
#endif   /* PPP_VJC */
#ifdef PAP_SUPPORT
   ppp_nvparms.require_pap = 0;
#endif   /* PAP_SUPPORT */
#ifdef USE_PPPOE
   strcpy(ppp_nvparms.username, "ppptest2");
   strcpy(ppp_nvparms.password, "2ppptest");
   strcpy(ppp_nvparms.secret, "2ppptest");
#endif   /* USE_PPPOE */
#endif   /* USE_PPP */

#ifdef USE_MODEM
   strcpy(modem_nvparms.dial_phone, "YOUR_ISP_PHONE\n");
   strcpy(ppp_nvparms.username, "YOUR_ISP_NAME");
   strcpy(ppp_nvparms.password, "YOUR_ISP_PASSWORD");
   strcpy(modem_nvparms.modem_init, "AT&D0&C1\n");
   strcpy(ppp_nvparms.loginfile, logfilename);
   strcpy(ppp_nvparms.logservefile, srvfilename);
#endif   /* USE_MODEM */

#ifdef USE_COMPORT
   comport_nvparms.comport = '2';
   comport_nvparms.LineProtocol = 1;
#endif   /* USE_COMPORT */

#ifdef NATRT
   natrt_nvparms.nat_enabled = 1;
   natrt_nvparms.nat_inet = 1;
   natrt_nvparms.nat_localnet = 0;
   natrt_nvparms.tcp_timeout = 300;
   natrt_nvparms.udp_timeout = 500;
#endif   /* NATRT */

#ifdef SMTP_ALERTS
   smtp_nvparms.mserver = htonl(0x00000000);
   strcpy(smtp_nvparms.rcpt, "\n");
#endif /* SMTP_ALERTS */

#ifdef DNS_CLIENT_UPDT
   strcpy(inet_nvparms.dns_zone_name, "iniche.com");   /* zone name */
#endif /* DNS_CLIENT_UPDT */

   /* create the login script file */
   fp = nv_fopen(logfilename, "w+");
   if (fp == NULL)
      dprintf("error creating %s\n", logfilename);
   else
   {
      e = nv_fprintf(fp, "#\n#empty login.nv\n#\n");
      if (e < 1)
      {
         dprintf("nv_defaults: error writing %s\n", logfilename);
      }
      nv_fclose(fp);
   }

   /* create the some required NV files */
   fp = nv_fopen(srvfilename, "w+");
   if (fp == NULL)
      dprintf("error creating %s\n", srvfilename);
   else
   {
      e = nv_fprintf(fp, "#\n#empty server.nv\n#\n");
      if (e < 1)
      {
         dprintf("nv_defaults: error writing %s\n", srvfilename);
      }
      nv_fclose(fp);
   }
   
   fp = nv_fopen(natfilename, "w+");
   if (fp == NULL)
      dprintf("error creating %s\n", natfilename);
   else
   {
      e = nv_fprintf(fp, "#\n#empty natdb.nv\n#\n");
      if (e < 1)
      {
         dprintf("nv_defaults: error writing %s\n", natfilename);
      }
      nv_fclose(fp);
   }
}

#endif  /* INCLUDE_NVPARMS */

int 
prep_armintcp(int ifaces_found)
{
/*
 * Altera Niche Stack Nios port modification:
 * Call eth_dev_init, in alt_iniche_dev.c, 
 * to step through all devices and all their respective
 * low-level initialization routines.
 */
#ifdef ALT_INICHE
   ifaces_found = eth_dev_init(ifaces_found);
#endif


#ifdef USE_PPP
   ifaces_found = prep_ppp(ifaces_found);
#endif

#ifdef USE_SLIP
   ifaces_found = prep_slip(ifaces_found);
#endif

   return ifaces_found;
}


#ifdef   USE_PPP

/* FUNCTION: ppp_type_setup(M_PPP)
 *
 * This per port routine is called via ppp_type_callback whenver
 * ppp sets up a new PPP link. It should set the PPP line type 
 * (line->lower_type) based on local port configuration info 
 * (either hardcoded or read from FLASH). Possible options include:
 *
 * LN_ATMODEM - Hayes compatible dialup modem.
 * LN_PPPOE - PPP over ethernet (also needs some ethernet setup)
 * LN_LBXOVER - Loopback/crossover links for testing
 * LN_UART - direct connect via RS-232 (or similar) UART.
 *
 *    The MPPP passed already has the ifp attached and configured 
 * from NVparms, so you can use if name and IP addressing info to
 * select the types. If you need to set up other PPP mappings (like 
 * selecting a modem, uart, or ethernet device) this is a good place 
 * to do it.
 *
 * returns 0 if OK or ENP error code
 */

   /* Table to allocate PPPs in the following order:
    * 
    * #ifdef LB_XOVER, assign first two to LB_XOVER client & server
    * #ifdef USE_MODEM, assign one to modem
    * #else ifdef USE_COMPORT, assign one to UART
    * #ifdef USE_PPPOE, assign one (or two if loopback) 
    *                   which will map to NDIS ethernet.
    */

static   int   ppps_already = 0;
static   int   ppp_types[] = 
{
#ifdef LB_XOVER
   LN_LBXOVER, LN_LBXOVER,
#endif
#ifdef USE_MODEM
   LN_ATMODEM,
#else 
#ifdef USE_COMPORT
   LN_UART,
#endif   /* USE_COMPORT */
#endif   /* USE_MODEM */
#ifdef USE_PPPOE
   LN_PPPOE,
#ifdef PPPOE_LBTEST
   LN_PPPOE,         /* PPPOE loopback test needs second interface */
#endif   /* PPPOE_LBTEST */
#endif   /* USE_PPPOE */

   /* extra entry are LB_XOVER for testing */
#ifdef LB_XOVER
   LN_LBXOVER, LN_LBXOVER,
   LN_LBXOVER, LN_LBXOVER,
#endif
};


int
ppp_type_setup(LINEP line)
{
   int err = 0;

   /* make sure we're not off the end of the static table */
   if (ppps_already >= (sizeof(ppp_types)/sizeof(int)))
   {
      /* If table is fully assigned, then set a default */
#ifdef USE_PPPOE
      line->lower_type = LN_PPPOE;
#else
#ifdef USE_MODEM
      line->lower_type = LN_ATMODEM;
#else
#ifdef USE_COMPORT
      line->lower_type = LN_UART;
#else
      /* use whatever's last in the table */
      line->lower_type = ppp_types[(sizeof(ppp_types)/sizeof(int)) - 1];
#endif /* USE_COMPORT */
#endif /* USE_MODEM */
#endif /* USE_PPPOE */
      dprintf("ppp_type_setup: defaulting type to %d\n", line->lower_type);
   }
   else
   {
      /* assign next type from table */
      line->lower_type = ppp_types[ppps_already++];
   }

   /* If it's PPPoE we need to assign it to an ethernet interface too */
#ifdef USE_PPPOE
   if(line->lower_type == LN_PPPOE)
   {
      NET ifp;

      /* find first ethernet interface */
      for(ifp = (NET)netlist.q_head; ifp; ifp = ifp->n_next)
      {
         if(ifp->mib.ifType == ETHERNET)
         {
            M_PPP mppp;
            /* bind PPP iface for this line (upper) to ethernet (lower) */
            mppp = (M_PPP)(line->upper_unit);
            ifmap_bind(mppp->ifp, ifp);
#ifdef PPPOE_LBTEST
            /* for PPPOE loopback it helps to add routes */
#endif   /* PPPOE_LBTEST */
            break;
         }
      }
      /* If no ethernet was found then set error code */
      if(!ifp)
         err = ENP_LOGIC;
   }
#endif   /* USE_PPPOE */

   return err;   /* OK return */
}



/* FUNCTION: ln_uinit()
 *
 * Initializes the device specified in the com_line struct
 *
 * PARAM1: line         pointer to com_line structure
 *
 * RETURN: 0 if successful, otherwise an error code
 */

int
ln_uinit(LINEP line)
{
   int   unit;
   int   err;

   if (line->ln_state != LN_INITIAL)
   {
      dtrap();       /* don't call this multiple times */
      return (0);      /* return OK anyway... */
   }

   if (units_set++ > 0)
   {
      dtrap();    /* trying to set up multiple UARTs unsupported */
   }
   unit = 1;  /* channel 1 == unit 1 */
   err = uart_init(unit);  /* open UART and assign comport */
   
   if (err)
      return (err);    /* report hardware failures */

   line->lower_unit = unit;
   line->ln_state = LN_CONNECTED;
   uarts[unit].line = line;

   return (0);
}



/* FUNCTION: ln_uconnect()
 *
 * Put PPP device into the 'connected' state
 *
 * PARAM1: line;        pointer to com_line structure 
 * 
 * RETURN: 0 if successful, otherwise an error code
 */
int
ln_uconnect(LINEP line)
{
   int   err;
   
   if (line->ln_state == LN_INITIAL) 
   {
      err = ln_uinit(line);
      if (err)
         return (err);   /* report hardware failures */
   }
   line->ln_state = LN_CONNECTED;

   /* Indicate to PPP that lower link is now up */
   ppp_lowerup((M_PPP)(line->upper_unit), LCP_STATE);

   return (0);
}



/* FUNCTION: ln_udisconnect()
 *
 * 'Disconnect' the PPP device
 *
 * PARAM1: line         pointer to com_line structure
 *
 * RETURN: 0 (always successful)
 */

int
ln_udisconnect(LINEP line)
{
   line->ln_state = LN_DISCONNECTED;
   return (0);
}



/* FUNCTION: ln_putc()
 *
 * send a single byte to the PPP device 
 *
 * PARAM1: line         pointer to com_line structure
 * PARAM2: ch           character to be sent
 *
 * RETURN: 0 if successful, otherwise an error code
 */

int
ln_uputc(LINEP line, int ch)
{
   return (uart_putc(line->lower_unit, (u_char)ch));
}

#endif   /* USE_PPP */

/*
 * Altera Niche Stack Nios port modification:
 * Removed conditioanl compilation on SUPERLOOP to here.
 */
