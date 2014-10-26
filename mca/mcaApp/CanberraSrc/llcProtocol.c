/********************************************************
 * llcProtocol.c
 *
 * a vxWorks driver to implement the IEEE 802.2 LLC Protocol
 * 
 * This is a severely cut-down version of support, which does not
 * have separate queues for sockets bound to different SAPs, and does
 * nothing special to support binding. It supports only the UI format
 * of frames, simply adding the source SAP, destination SAP and control
 * byte onto the data payload.
 *
 * In order to use this network service sublayer, you need to load the
 * library, then call llcRegister(), to install this socket back-end, then
 * attach the network service sublayer to the correct interface e.g.
 *
 * ld < llcLibrary.o (unless the library is built into your application)
 * llcRegister()
 * llcAttach("gei",0)
 * 
 * For more details on where this module fits into the VxWorks network
 * environment, see "VxWorks 5.5 Network Programmer's Guide"
 * DOC-14618-ND-00, pp 7, 223-247
 *
 * Peter Denison, Diamond Light Source Ltd.
 * Mark Rivers, University of Chicago
 ********************************************************/

/* Revision History
 * 
 * 2005-11-10 0.01 Written
 *
 * 2007-12-17 Modified to work on vxWorks 5.4.2 where muxLib, 
 *            rather than muxTkLib must be used because muxTkLib
 *            seems to trash all other protocols.
 * 2013-06-03 VxWorks 6.x API changes. Support requires INCLUDE_NET_POOL 
 *            BSP option.
 * 2014-04-23 Bug fix for Canberra AIM only working with network device
 *            where unit# = 0. Fixed endFindByName(device, 0<-unit) call.
 */

#include <vxWorks.h>
#include <version.h>
#include <muxLib.h>
#ifdef USE_MUXTKLIB
#include <muxTkLib.h>
#endif
#include <netBufLib.h>
#include <msgQLib.h>
#include <usrLib.h>
#include <net/if_llc.h>
#include <netinet/if_ether.h>
#include <stdio.h>
#include <stdlib.h>
#include "llc.h"
#if defined(_WRS_VXWORKS_MAJOR) && (_WRS_VXWORKS_MAJOR >= 6)
#include <end.h>
#endif
#include <string.h>

#define QUEUE_SIZE 512
#if defined(_WRS_VXWORKS_MAJOR) && (_WRS_VXWORKS_MAJOR >= 6)
#define LLC_SNAP_FRAMELEN    8
#endif

int llcDetach(void);
static void mBlkShow(M_BLK_ID mBlk);
static void *copyFromMblk(void *dest, M_BLK_ID src, int offset, int len);

/** Queue on which to put received packets before they are received by
 * the upper layer.
 */
static MSG_Q_ID llcRecvQueue;
/** Debug level. Set to non-zero for output of messages
 */
extern int llcDebug;

/* VxWorks network stacks use a memory pool from which to allocate
 * space for buffers, packets, headers, etc. These are the required
 * tables and pointers for setting up the pool.
 */
static void *cookie;
static NET_POOL netPool;
static NET_POOL_ID pNetPool = &netPool;
static struct ether_addr local_mac_addr;

static M_CL_CONFIG mClBlkConfig = /* mBlk, clBlk configuration table */
{ /* mBlkNum clBlkNum memArea memSize */
/* ---------- -------- ------- ------- */
#if defined(_WRS_VXWORKS_MAJOR) && (_WRS_VXWORKS_MAJOR >= 6)
200, 140, NULL, 0 };
#else
NUM_NET_MBLKS_MIN, NUM_CL_BLKS_MIN, NULL, 0 };
#endif
static int clDescTblNumEnt;
static CL_DESC clDescTbl [] = /* cluster descriptor table */
{ /* clSize clNnum memArea memSize */
  /* ---------- ---- ------- ------- */
#if defined(_WRS_VXWORKS_MAJOR) && (_WRS_VXWORKS_MAJOR >= 6)
    {64,   50, NULL, 0},
    {128,  50, NULL, 0},
    {256,  10, NULL, 0},
    {512,  10, NULL, 0},
    {1024, 20, NULL, 0},
    {2048, 20, NULL, 0}
#else
    {64,     NUM_64_MIN, NULL, 0},
    {128,   NUM_128_MIN, NULL, 0},
    {256,   NUM_256_MIN, NULL, 0},
    {512,   NUM_512_MIN, NULL, 0},
    {1024, NUM_1024_MIN, NULL, 0},
    {2048, NUM_1024_MIN, NULL, 0}
#endif
};

/**
 * Callback from the MUX to receive packets
 * @param netCallbackId the handle/ID installed at bind time
 * @param type network service type
 * @param pNetBuf network service datagram
 * @param pSpareData pointer to optional data from driver
 * @return TRUE if the packet was "consumed", and FALSE if the packet was ignored
 */
#ifdef USE_MUXTKLIB
static BOOL llcRcvRtn(void *netCallbackId, long type, M_BLK_ID pNetBuf,
               void *pSpareData)
#else
static BOOL llcRcvRtn(void *netCallbackId, long type, M_BLK_ID pNetBuf,
               LL_HDR_INFO *ll_hdr_info, void *pSpareData)
#endif
{
    unsigned short typelen;

    typelen = ntohs(*(unsigned short *)(pNetBuf->mBlkHdr.mData + SIZEOF_ETHERHEADER - 2));
    if (llcDebug > 10) printf("llc: received any packet, passed type: %04lx length:%d\n", type, typelen);
    if (typelen > ETHERMTU) {
        /* Can't be IEEE 802.2 LLC, which has length in the ether header */
        return FALSE; /* Pass the packet on */
    }

    if (llcDebug > 4) printf("llc: received llc packet, passed type: %04lx length:%04x\n", type, typelen);

    /* In future, this could be extended to put the packet onto a different
     * queue for each bound socket, but for now, just use a single queue */
    msgQSend(llcRecvQueue, (char *)&pNetBuf, sizeof(M_BLK_ID), NO_WAIT, MSG_PRI_NORMAL);
    return TRUE; /* consumes the packet */
}

/**
 * called by MUX in response to muxDevUnload()
 * @param netCallbackId the handle/ID installed at bind time
 *
 * Disconnect this module from the MUX stack
 */
#ifdef USE_MUXTKLIB
static STATUS llcShutdownRtn(void *netCallbackId /* the handle/ID installed at bind time */)
#else
static STATUS llcShutdownRtn(void *netCallbackId, /* the handle/ID installed at bind time */
                             void *pSpare)
#endif
{
    return (muxUnbind(netCallbackId, MUX_PROTO_SNARF, llcRcvRtn));
}

/**
 * called by MUX to restart network services
 * @param netCallbackId the handle/ID installed at bind time
 *
 * We hold no connection-oriented information, so do nothing
 */
#ifdef USE_MUXTKLIB
static STATUS llcRestartRtn(void *netCallbackId /* the handle/ID installed at bind time */)
#else
static STATUS llcRestartRtn(void *pEND, /* the handle/ID installed at bind time */
                            void *pSpare)
#endif
{
    return OK;
}

/**
 * called by MUX to handle error conditions encountered by network drivers
 * @param void *netCallbackId the handle/ID installed at bind time
 * @param END_ERR *pError an error structure, containing code and message
 */
#ifdef USE_MUXTKLIB
static void llcErrorRtn(void *netCallbackId, /* the handle/ID installed at bind time */
                        END_ERR *pError) /* pointer to struct containing error */
#else
static void llcErrorRtn(END_OBJ *pEND, /* the handle/ID installed at bind time */
                        END_ERR *pError, /* pointer to struct containing error */
                        void *extra)
#endif
{
    printf("Error in LLC protocol layer: %d\n", pError->errCode);
    if (pError->pMesg) {
        printf(pError->pMesg);
    }
}

/**
 * initialize LLC network service
 * @param device The device name of the interface to attach this protocol to
 * @param unit The unit number of the interface
 * @return 1 if the device was correctly opened, 0 otherwise
 *
 * The service has to be bound as a "SNARF" protocol device, since the MUX
 * decides which protocol to call based on the ethernet type field, and for
 * LLC, this can be variable.
 */
int llcAttach(char *device, int unit)
{
    int i;

#ifdef USE_MUXTKLIB
    cookie= muxTkBind (device, /* char * pName, interface name, for example: ln, ei */
                       unit, /* int unit, unit number */
                       &llcRcvRtn, /* llcRcvRtn( ) Receive data from the MUX */
                       &llcShutdownRtn, /* llcShutdownRtn( ) Shut down the network service. */
                       &llcRestartRtn, /* llcRestartRtn( ) Restart a suspended network service.*/
                       &llcErrorRtn, /* llcErrorRtn( ) Receive an error notification from the MUX. */
                       MUX_PROTO_SNARF, /* long type, from RFC1700 or user-defined */
                       "IEEE802.2 LLC", /* char * pProtoName, string name of service */
                       NULL, /* void * pNetCallBackId, returned to svc sublayer during recv */
                       NULL, /* void * pNetSvcInfo, ref to netSrvInfo structure */
                       NULL /* void * pNetDrvInfo ref to netDrvInfo structure */
                       );
#else
#if defined(vxWorks) && defined(_WRS_VXWORKS_MAJOR) && (_WRS_VXWORKS_MAJOR >= 6)
    END_OBJ *pEnd;
    unsigned char mac[6];

    pEnd= endFindByName(device, unit);
    if (pEnd == NULL)
    {
        printf("llcAttach: invalid Ethernet device name %s\n", device);
        return ERROR;
    }
    muxIoctl(pEnd, EIOCGADDR, (caddr_t) mac);
    memcpy(&local_mac_addr, mac, 6);
#else
    /* Need to get our local MAC address */
    struct ifnet *ifPtr;
    char temp_device[6];
    
    sprintf(temp_device, "%s%d", device, unit);
    ifPtr = ifunit(temp_device);
    memcpy(&local_mac_addr, ((struct arpcom *)ifPtr)->ac_enaddr, sizeof(struct ether_addr));
#endif
    cookie= muxBind   (device, /* char * pName, interface name, for example: ln, ei */
                       unit, /* int unit, unit number */
                       &llcRcvRtn, /* llcRcvRtn( ) Receive data from the MUX */
                       &llcShutdownRtn, /* llcShutdownRtn( ) Shut down the network service. */
                       &llcRestartRtn, /* llcRestartRtn( ) Restart a suspended network service.*/
                       &llcErrorRtn, /* llcErrorRtn( ) Receive an error notification from the MUX. */
                       MUX_PROTO_SNARF, /* long type, from RFC1700 or user-defined */
                       "IEEE802.2 LLC", /* char * pProtoName, string name of service */
                       NULL /* void * pNetCallBackId, returned to svc sublayer during recv */
                       );
#endif
    if (cookie==NULL) {
        perror("Failed to bind service:");
        return -1;
    }

    if (llcDebug > 0) printf("Attached LLC interface to %s unit %d\n",
                             device, unit);

    /* Allocate and initialize the memory pool */
    mClBlkConfig.memSize = (mClBlkConfig.mBlkNum * (M_BLK_SZ + sizeof(long))) +
                           (mClBlkConfig.clBlkNum * CL_BLK_SZ);
    mClBlkConfig.memArea = malloc(mClBlkConfig.memSize);
    clDescTblNumEnt = (NELEMENTS(clDescTbl));
    for (i=0; i< clDescTblNumEnt; i++) {
        clDescTbl[i].memSize = (clDescTbl[i].clNum * (clDescTbl[i].clSize + sizeof(long)));
        clDescTbl[i].memArea = malloc( clDescTbl[i].memSize );
    }
    
    if (netPoolInit (pNetPool, &mClBlkConfig, &clDescTbl[0], clDescTblNumEnt, NULL) != OK)
        return -1;

    /* Create the message queue for received packets */
    llcRecvQueue = msgQCreate(QUEUE_SIZE, sizeof(M_BLK_ID), MSG_Q_FIFO);
    if (!llcRecvQueue) {
        perror("Allocate receive queue:");
        return -1;
    }
    
    return 0;
}

/**
 * Detach from the device
 *
 * @return Unconditionally 1
 */
int llcDetach(void)
{
    netPoolDelete (pNetPool);
    muxUnbind(cookie, MUX_PROTO_SNARF, llcRcvRtn);
    return 1;
}

/**
 * send a packet using the LLC protocol
 *
 * @param addr The MAC address and SAP to send the packet to
 * @param sap  The source SAP to send the packet from
 * @param type Unused
 * @param data The data to send
 * @param datalen Length of the data to send
 * @return ERROR or length of data sent ( = datalen)
 *  
 * Note: This only supports UI type packets
 */
int llcSendPacket(struct sockaddr_llc *addr, int sap, int type, 
                  char *data, int datalen)
{
    int ret;
    int len;
    struct llc llc_head;
    M_BLK_ID pMblk;
    M_BLK_ID pDst, pSrc;
    
    llc_head.llc_dsap = addr->sllc_sap;
    llc_head.llc_ssap = sap;
    llc_head.llc_control = LLC_UI;
  
    /* LLC header is put on by this code. Any SNAP header is expected to be
       at the start of the data packet */
    if (datalen < LLC_SNAP_FRAMELEN - LLC_UFRAMELEN) {
        return ERROR;
    }

    len = datalen + LLC_UFRAMELEN;

    /* Get a memory block from the pool to put the packet in. The block will
     * be returned to the pool once it is taken off the queue.
     */
    pMblk = netTupleGet(pNetPool, len, M_WAIT, MT_DATA, TRUE);
    if (!pMblk) {
        return ERROR;
    }

    /* Copy header into memory block */
    memcpy(pMblk->mBlkHdr.mData, &llc_head, LLC_UFRAMELEN);
  
    /* Copy data into memory block */
    memcpy(pMblk->mBlkHdr.mData + LLC_UFRAMELEN, data, datalen);
    pMblk->mBlkHdr.mLen = len;

    /* form the link-level header if necessary */
    /* It seems that adding the header isn't necessary with muxTkLib despite the comments
       in section B.2.21 of the VxWorks 5.5 Network Programmer's Guide, p282
       The source code to the TkMux layer (provided by WindRiver technical
       support) confirms this.
    */
#ifndef USE_MUXTKLIB
    pSrc = netTupleGet(pNetPool, sizeof(struct ether_addr), M_WAIT,
                       MT_IFADDR, TRUE);
    if (!pSrc) {
        perror("llcSendPacket: error getting source packet");
        return ERROR;
    }
    memcpy(pSrc->mBlkHdr.mData, &local_mac_addr, sizeof(struct ether_addr));
    pDst = netTupleGet(pNetPool, sizeof(struct ether_addr), M_WAIT,
                       MT_IFADDR, TRUE);
    if (!pDst) {
        perror("llcSendPacket: error getting destination packet");
        return ERROR;
    }
    memcpy(pDst->mBlkHdr.mData, addr->sllc_mac, sizeof(struct ether_addr));
    pDst->mBlkHdr.reserved = len;
    pMblk = muxAddressForm(cookie, pMblk, pSrc, pDst);
    if (pMblk == NULL)
    {
        perror("llcSendPacket: muxAddressForm() returns NULL");
        return ERROR;
    }
    netMblkClChainFree(pSrc);
    netMblkClChainFree(pDst);
#endif

    /* Dump packet if debugging level set */
    if (llcDebug > 4) printf("llc: sending packet, destination: %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x len: %d\n", 
                             addr->sllc_mac[0],
                             addr->sllc_mac[1],
                             addr->sllc_mac[2],
                             addr->sllc_mac[3],
                             addr->sllc_mac[4],
                             addr->sllc_mac[5],
                             len);
    if (llcDebug > 7) mBlkShow(pMblk);

    /* send the data */
#ifdef USE_MUXTKLIB
    ret = muxTkSend(cookie, /* returned by muxTkBind()*/
                    pMblk, /* data to be sent */
                    addr->sllc_mac, /* destination MAC address */
                    len, /* network protocol that is calling us */
                    NULL); /* spare data passed on each send */
#else
    ret = muxSend(  cookie, /* returned by muxTkBind()*/
                    pMblk); /* data to be sent */
#endif

    if (ret == ERROR) {
        printf("Error - ethernet packet not sent\n");
        return ERROR;
    } else {
        return datalen;
    }
}

/**
 * receive an LLC protocol packet
 *
 * @param addr Structure to fill in with the source information from the
 *             received packet
 * @param data Pointer to buffer to accept received packet
 * @param datalen Length of buffer
 * @return Length of data returned, or 0 if an error occurred
 *
 * Called by the upper layers to take a packet off the internal queue.
 * Currently only supports blocking access, and therefore waits forever
 * for a packet to arrive. 
 */
int llcReceivePacket(struct sockaddr_llc *addr, char *data, int datalen)
{
    int ret;
    int len;
    struct {
        struct ether_header eth_head;
        struct llc llc_head;
    } __attribute__ ((packed)) header; 
    M_BLK_ID pMblk;

    /* Pull a packet off the internal queue */
    ret = msgQReceive(llcRecvQueue, (char *)&pMblk, sizeof(M_BLK_ID),
                      WAIT_FOREVER);

    if (ret == ERROR || errno == S_objLib_OBJ_UNAVAILABLE) {
        return 0;
    }
    
    if (llcDebug > 7) mBlkShow(pMblk); 

    /* Copy header from block */
    copyFromMblk(&header, pMblk, 0, SIZEOF_ETHERHEADER + LLC_UFRAMELEN);

    if (llcDebug > 8) {
        printf("llc: received packet: \n"
               "  destination = %02x:%02x:%02x:%02x:%02x:%02x\n"
               "       source = %02x:%02x:%02x:%02x:%02x:%02x\n",
               header.eth_head.ether_dhost[0],
               header.eth_head.ether_dhost[1],
               header.eth_head.ether_dhost[2],
               header.eth_head.ether_dhost[3],
               header.eth_head.ether_dhost[4],
               header.eth_head.ether_dhost[5],
               header.eth_head.ether_shost[0],
               header.eth_head.ether_shost[1],
               header.eth_head.ether_shost[2],
               header.eth_head.ether_shost[3],
               header.eth_head.ether_shost[4],
               header.eth_head.ether_shost[5]);
    }
    
    /* If we have anywhere to put it, fill in the sockaddr with details
     * of where the packet came from */
    if (addr) {
        addr->sa_len = sizeof(struct sockaddr_llc);
        addr->sllc_family = AF_LLC;
        addr->sllc_arphrd = ARPHRD_ETHER;
        addr->sllc_sap = header.llc_head.llc_dsap;
        memcpy(&addr->sllc_mac,
               pMblk->mBlkHdr.mData + sizeof(struct ether_addr),
               sizeof(struct ether_addr));
    }

    /* The length field in the mBlkHdr is apparently unreliable, and can cause
       the copying of too much data - use the length field inside the Ethernet
       header in the data instead. */
    len = ntohs(header.eth_head.ether_type);
    if (llcDebug > 8) printf("length of packet: %d\n", len);
    /* Remove the LLC header from the payload length*/
    len -= LLC_UFRAMELEN;
    if (len > datalen) {
        len = datalen;
    }
    /* Copy data from block - starting at beginning of LLC payload */
    copyFromMblk(data, pMblk, SIZEOF_ETHERHEADER + LLC_UFRAMELEN, len);
    
    netMblkClChainFree(pMblk);
    return len;
}

/**
 * Copy data from a M_BLK_ID linked structure to a buffer
 *
 * @param dest Destination of the copied data
 * @param src netBuf to copy out of
 * @param offset offset from start of data to start copying from
 * @param len length of data to copy
 * @return NULL if the block could not be completely copied, dest otherwise
 */
void *copyFromMblk(void *dest, M_BLK_ID src, int offset, int len)
{
    M_BLK_ID copy;
    void *retval = NULL;

    if (!src) {
        return NULL;
    }

    /* Create a new MBlk chain pointing to the data we want */
    copy = netMblkChainDup(pNetPool, src, offset, len, M_WAIT);
    /* Copy from that new chain */
    if (netMblkToBufCopy(copy, dest, NULL) == len) {
        retval = dest;
    }
    /* Release the newly created chain */
    netMblkClChainFree(copy);
    return retval;
}

/**
 * Debugging function to display the relevant details of an mBlk
 */
void mBlkShow(M_BLK_ID mBlk)
{
    M_BLK_HDR *hdr;
    M_PKT_HDR *pkt;
    CL_BLK *ctr;
    
    printf("MBlk: %p\n", mBlk);
    if (!mBlk) return;
    hdr = &mBlk->mBlkHdr;
    pkt = &mBlk->mBlkPktHdr;
    ctr = mBlk->pClBlk;
    printf("  Next: %p  NextPkt: %p  Data: %p  Len: %d\n", hdr->mNext,
           hdr->mNextPkt, hdr->mData, hdr->mLen);
    printf("  Type: %d  Flags: %x\n", hdr->mType, hdr->mFlags);
    printf("  Recv I/F: %p  Packet length: %d\n", pkt->rcvif, pkt->len);
    if (hdr->mData) {
        d(hdr->mData, hdr->mLen, 1);
    }
    if (hdr->mNext) {
        mBlkShow(hdr->mNext);
    }
}

