/* read_ca_stats.c - reports number of CA clients and connections */

/* This is just the code from PVS_ClientStats, adapted for being
 * called from devVxStats.
 *						Susanna Jacobson
 *						11/21/97
 */

#include <stdio.h>
#include <stdlib.h>

#include <osiSock.h>
#include <in.h>
#include <ellLib.h>
#include <server.h>
#include <epicsVersion.h>

void
read_ca_stats(
int *pncli,
int *pnconn)
{
	int		connex=0,
			clients=0;
	struct client 	*pclient;

	LOCK_CLIENTQ;
	pclient = (struct client *) ellFirst(&clientQ);
	while (pclient)
	{
		clients++;
#if EPICS_MODIFICATION < 9
		/* 3.14.8.2 and earlier */
		connex=connex+ellCount(&pclient->addrq);
#else
		/* 3.14.9 */
		connex += ellCount(&pclient->chanList) + 
            ellCount(&pclient->chanPendingUpdateARList);
#endif
		pclient = (struct client *) ellNext(&pclient->node);
	}
	UNLOCK_CLIENTQ;

	*pncli=clients;
	*pnconn=connex;
}

