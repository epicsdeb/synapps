/* NCP_COMM_DEFS.H */

/*******************************************************************************
*
* Network Module Communications Definitions
*
*       This module contains definitions of the communications protocol
*       used between networked modules and host computers.
*
* NOTE: Most of these definitions are tied to the AIM firmware, i.e. they
*       define the format of data packets to/from the AIM.
*       Thus these definitions cannot be changed without changing the
*       AIM firmware.
*
********************************************************************************
*
* Revision History
*
*       20-Dec-1993     MLR     Modified from Nuclear Data source
*       06-May-2000     MLR     Changed definitions to architecture-independent
*                               types like INT32.  Defined structures to be
*                               packed for architecture independence.
*                               Defined LSWAP and SSWAP correctly depending
*                               upon architecture.
*       03-Sept-2000    MLR     Moved interlock macros to nmc_sys_defs.h
*       25-Nov-2005     PND     Stripped out ethernet specific bits for
*                               socket comms.
*	21-Sep-2006	PND	Re-added ethernet bits to allow old non-socket
*				comms on platforms which don't support it.
*******************************************************************************/

/*
* Define structure of protocol header block. Fields marked with a "###" are not
* used in Ethernet messages.
*/

struct ncp_comm_header {

        INT32 checkword;                         /* Unique number pattern (NCP_K_CHECKWORD) */
        INT8 protocol_type;                     /* Protocol type */
        UINT8 protocol_flags;                   /* Protocol flags */
        UINT8 message_number;                   /* Message number */
        INT8 message_type;                      /* Message type */
        UINT8 owner_id[6];                      /* ID of owner of module */
        INT8 owner_name[8];                     /* Name of owner of module */
        UINT32 data_size;                       /* Size of message data area */
        UINT8 module_id;                        /* Module ID number ### */
        UINT8 submessage_number;                /* Sequences multi-block commands */
        INT8 spares[2];                         /* Spares */
        UINT16 checksum;                        /* Header checksum ### */
} __attribute__ ((packed));

/*
* Define protocol_type. This field identifies the type/rev of the communications
* protocol in use.
*/

#define NCP_C_PRTYPE_NAM        1               /* Standard NAM-Host protocol */

/*
* Define protocol_flags. This field contains bit flags for the use of the
* communications handlers.
*/

        /* No flags defined yet */

/*
* Define message_type. This field defines the basic format of the data area.
*/

#define NCP_C_MSGTYPE_PACKET    1               /* Command/Response packets */
#define NCP_C_MSGTYPE_MSTATUS   2               /* Module status */
#define NCP_C_MSGTYPE_MEVENT    3               /* Module event message */
#define NCP_C_MSGTYPE_INQUIRY   4               /* Host inquiry */

/*
* Define data area packet format.
*/

struct ncp_comm_packet {

        UINT32 packet_size;                     /* Packet data area size */
        INT8 packet_type;                       /* Packet type (command/response) */
        UINT8 packet_flags;                     /* Packet flags */
        INT16 packet_code;                      /* Packet command/response code */
                                                /* Data starts here! */

} __attribute__ ((packed));

/*
* Define packet_type. This field makes sure that commands are not interpreted as
* responses and vice versa.
*/

#define NCP_C_PTYPE_HCOMMAND    1               /* Host to module command */
#define NCP_C_PTYPE_MRESPONSE   2               /* Module to host response */

/*
* Define flags in packet_flags.
*/

        /* No flags defined yet */

/*
* Module status message structure. This message is returned by a module in
* response to a multicast inquiry message.
* The structure below appears in the data area of the message.
*/

struct ncp_comm_mstatus {

        UINT8 module_type;                      /* Module type ID */
        UINT8 hw_revision;                      /* Module HW revision number */
        UINT8 fw_revision;                      /* Module FW revision number */
        UINT8 module_init;                      /* Module state: true if ever
                                                   has been initialized. */
        INT32 comm_flags;                       /* Communications capability flags */

                                                /* Module type specific info */
                                                /* starts here! */

                /* NAM (AIM) */

        UINT8 num_inputs;                       /* Number of inputs (ADCs) */
        INT32 acq_memory;                       /* Amount of acquisition memory
                                                   in bytes */
        INT32 spares[4];                        /* spares */
} __attribute__ ((packed));

/* Define contents of ncp_comm_status.module_type */

#define NCP_C_MODTYPE_NAM       1               /* Network Acquisition Module */
                                                /* (We call it AIM nowadays) */


/*
* Define structure of data area for host inquiry messages
*/

struct ncp_comm_inquiry {

        INT8 inquiry_type;                      /* Type of inquiry message */

} __attribute__ ((packed));

/*
* Define values for ncp_comm_inquiry.inquiry_type
*/

#define NCP_C_INQTYPE_ALL 1                     /* Everybody responds */
#define NCP_C_INQTYPE_UNOWNED 2                 /* Unowned modules only */
#define NCP_C_INQTYPE_NOTMINE 3                 /* Modules unowned by this host only */

/*
* Module event message structure. This message is sent when an event occurs in
* the module. The structure below describes the data field of the message.
*/

struct ncp_comm_mevent {

        UINT8 event_type;                       /* Event type */
        INT32 event_id1;                        /* Event identifiers (type dependent) */
        INT32 event_id2;
} __attribute__ ((packed));

/* Define contents of ncp_comm_mevent.event_type */

#define NCP_C_EVTYPE_ACQOFF     1               /* Acquisition stopped:
                                                   event_id1 is the ADC number */
#define NCP_C_EVTYPE_BUFFER     2               /* List buffer filled:
                                                   event_id1 is the ADC number
                                                   event_id2 is the buffer number (0 or 1) */
#define NCP_C_EVTYPE_DIAGCOMP   3               /* Diagnostics complete */

#define NCP_C_EVTYPE_SERVICERQST 4              /* Service request aserted
                                                   on ICB Bus */

/* Define contents of ncp_hcmd_setmodevsap.mevsource */

/* 0 - 255 are recerved for ADC numbers */

#define NCP_K_MEVSRC_ICB  256                   /* Service Request from ICB */



/*
* Misc communications structures and constants
*/

#define NMC_K_MAX_NIMSG 1492                    /* Max size of an NI message */

#define NMC_K_MAX_UNANSMSG 3                    /* Max number of unanswered
                                                   inquiry messages allowed
                                                   before a module is marked
                                                   UNREACHABLE */
#define NCP_K_CHECKWORD         0XAF0366F2      /* Value of
                                                   ncp_comm_header.checkword */

/* Define the structure of an IEEE 802 extended packet header */
struct enet_header {
        UINT8 dest[6];
        UINT8 source[6];
        UINT16 length;
} __attribute__ ((packed));

struct snap_header {
        UINT8 dsap;
        UINT8 ssap;
        UINT8 control;
        UINT8 snap_id[5];
} __attribute__ ((packed));

/* Define the maximum header size for all network types (Ethernet, FDDI,etc.) */
#define NCP_MAX_HEADER_SIZE (sizeof(struct enet_header)+sizeof(struct snap_header))

/* Link layer information passed at the start of each packet passed up to
   the higher layers, also including the SNAP header sent out in all packets */
struct llinfo {
        UINT8 source[6];
        UINT8 snap_id[5];
} __attribute__ ((packed));

/* Define structure of a packet to/from an AIM module */
struct enet_packet{
        struct enet_header enet_header;
        struct snap_header snap_header;
        struct ncp_comm_header ncp_comm_header;
        union {
           struct ncp_comm_mstatus ncp_comm_mstatus;
           struct ncp_comm_packet  ncp_comm_packet;
           struct ncp_comm_inquiry ncp_comm_inquiry;
           struct ncp_comm_mevent  ncp_comm_mevent;
        }  __attribute__ ((packed)) data;
} __attribute__ ((packed));

struct status_packet{
        struct enet_header enet_header;
        struct snap_header snap_header;
        struct ncp_comm_header ncp_comm_header;
        struct ncp_comm_mstatus ncp_comm_mstatus;
} __attribute__ ((packed));
struct response_packet{
        struct enet_header enet_header;
        struct snap_header snap_header;
        struct ncp_comm_header ncp_comm_header;
        struct ncp_comm_packet  ncp_comm_packet;
        UINT8 ncp_packet_data[NMC_K_MAX_NIMSG -
                                      sizeof(struct ncp_comm_header) -
                                      sizeof(struct ncp_comm_packet)];
} __attribute__ ((packed));
struct inquiry_packet{
        struct enet_header enet_header;
        struct snap_header snap_header;
        struct ncp_comm_header ncp_comm_header;
        struct ncp_comm_inquiry ncp_comm_inquiry;
} __attribute__ ((packed));
struct event_packet{
        struct enet_header enet_header;
        struct snap_header snap_header;
        struct ncp_comm_header ncp_comm_header;
        struct ncp_comm_mevent ncp_comm_mevent;
} __attribute__ ((packed));

/* Number of seconds to multicast inquiry packets to the network */
#define NCP_BROADCAST_PERIOD 20

/*
* Module status codes: these are returned in NCP_COMM_PACKET.PACKET_CODE; note
* that some codes are defined in AIM_COMM_DEFS.H along with their structure
* definitions. Any code defined here has a 0 length packet data field.
*/

#define NCP_K_MRESP_SUCCESS     9               /* Successful command completion */
#define NCP_K_MRESP_INVALADC    18              /* Bad ADC number */
#define NCP_K_MRESP_INVALCMD    26              /* Bad command */
                                                /* NCP_K_MRESP_ADCSTATUS 35 */
#define NCP_K_MRESP_OWNERNOTSET 42              /* Module owned, not overridden */
                                                /*             MODCOMMERR 50 */
                                                /*             MODERROR 58 */
                                                /*             MSGTOOBIG 66 */
                                                /*             INVMODNUM 74 */
                                                /*             UNKMODULE 82 */
                                                /*             UNOWNEDMOD 90 */
                                                /*             INVMODRESP 98 */
#define NCP_K_MRESP_UNLCHN      106             /* the number of bytes/4 is not */
#define NCP_K_MRESP_INVALACQMODE        114     /* Host sent invalid acquisition mode */
                                                /* an even number of channels */
#define NCP_K_MRESP_INVALSTACQADR       122     /* Starting address of setacqaddr
                                                   command does not start on 256
                                                   byte boundry */
#define NCP_K_MRESP_ACQRNGFRC   130             /* The acqision range contains
                                                   a fractional channel */
#define NCP_K_MRESP_INVALSTMEMADR       138     /* The specified start address does
                                                   is not on an even longword boundry */
#define NCP_K_MRESP_MEMFRC              146     /* The memory range contains a fractional
                                                   channel */
#define NCP_K_MRESP_ACQONCOMMINVAL      154     /* Command invalid while acquisition
                                                   is on */
#define NCP_K_MRESP_APUTIMEOUT          162     /* APU timed out on command */
                                                /*              READIOERR 170 */
                                                /*              INVNETYPE 178 */
                                                /*              TOOMANYMODULES 186 */
                                                /*              NOSUCHMODULE 194 */
                                                /*              RETACQSETUP 203 */
                                                /*              NONISAPS 210 */
                                                /*              MODNOTREACHABLE 218 */
                                                /*              RETMEMCMP 227 */
#define NCP_K_MRESP_RQSTMEMSIZETOOLG    234     /* requested memory size too large */
#define NCP_K_MRESP_SETHOSTMEMSIZETOOLG 242     /* size was too large in set host memory */
                                                /*              RETLISTSTAT 251 */
#define NCP_K_MRESP_INVALOFFSETRETLIST  258     /* bad offset in RETLISTMEM */
#define NCP_K_MRESP_INVALLISTBUFFER     266     /* bad list buffer specified */
#define NCP_K_MRESP_DLISTBUFFNOTFULL    274     /* dlist buffer not full */
#define NCP_K_MRESP_COMMINVLINCURRMODE  282     /* Command invalid in current mode */
                                                /*              NOTOWNED 290 */
                                                /*              MODINUSE 298 */
#define NCP_K_MRESP_INVLMEVSRC          330

/* Define some macros for doing common things */

/* Copy an Ethernet address */
#define COPY_ENET_ADDR(a,b) \
        b[0] = a[0]; \
        b[1] = a[1]; \
        b[2] = a[2]; \
        b[3] = a[3]; \
        b[4] = a[4]; \
        b[5] = a[5];

/* Compare two Ethernet addresses */
#define COMPARE_ENET_ADDR(a,b) \
       (a[0] == b[0] && \
        a[1] == b[1] && \
        a[2] == b[2] && \
        a[3] == b[3] && \
        a[4] == b[4] && \
        a[5] == b[5])

/* Copy a SNAP address */
#define COPY_SNAP(a,b) \
        b[0] = a[0]; \
        b[1] = a[1]; \
        b[2] = a[2]; \
        b[3] = a[3]; \
        b[4] = a[4];

/* Compare two SNAP addresses */
#define COMPARE_SNAP(a,b) \
       (a[0] == b[0] && \
        a[1] == b[1] && \
        a[2] == b[2] && \
        a[3] == b[3] && \
        a[4] == b[4])

/*
   Byte swapping macros. NOTE: These macros require that the routines
   which use them contain the following statement:
        register UINT8 tmp, *p;
   These macros swap a short or long integer "in place", i.e. the result
   replaces the previous value.

	Linux uses macros with 2 or none undersores
	VxWorks needs 1 underscore
*/

#ifndef __BYTE_ORDER
#define __BYTE_ORDER _BYTE_ORDER
#endif

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#endif

/* There is a serious bug in Cygin compiler.  
 * It sets __BYTE_ORDER == __BIG_ENDIAN when
 * it is really little endian! */
#if (__BYTE_ORDER == __BIG_ENDIAN) && (!CYGWIN32)
/* #warning "Big Endian" */
/* Swap an INT16 or UINT16 integer */
#define SSWAP(num) \
      { register UINT8 tmp, *p; \
        p = (UINT8 *) &num; \
        tmp = *p; \
        *p = *(p+1); \
        *(p+1) = tmp; }

/* Swap an INT32 or UINT32 integer */
#define LSWAP(num) \
      { register UINT8 tmp, *p; \
        p = (UINT8 *) &num; \
        tmp = *p; \
        *p = *(p+3); \
        *(p+3) = tmp; \
        tmp = *(p+1); \
        *(p+1) = *(p+2); \
        *(p+2) = tmp; }

#else  /* Nothing to do, little-endian */
/* #warning "Little Endian" */
#define SSWAP(num)
#define LSWAP(num)
#endif
