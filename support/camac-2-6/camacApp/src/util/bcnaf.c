/******************************************************************************
 * bcnaf() -- Issue CAMAC commands from IOC console
 *
 *-----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	23-Apr-1995
 *
 *-----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 09-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *-----------------------------------------------------------------------------
 * ROUTINE DESCRIPTION:
 *
 * This is a diagnostic routine which allows you to execute CAMAC commands
 * from the IOC console (or a telnet session into the IOC).  The routine
 * executes the specified command and displays the data word (if this is a
 * read function), the Q and X responses, and the status of the operation.
 * This information is displayed on the console or telnet session terminal.
 *
 * CALLING SEQUENCE:
 *
 * bcnaf (b, c, n, a, f, data)
 *	b    = branch number
 *	c    = crate number
 *	n    = slot number
 *	a    = subaddress
 *	f    = function code
 *	data = data (if write function)
 *
 *****************************************************************************/

/************************************************************************/
/* Header Files								*/
/************************************************************************/

#include	<stdlib.h>	/* Standard C library routines		*/
#include	<stdio.h>	/* Standard I/O routines		*/
#include	<vxWorks.h>	/* VxWorks definitions & routines	*/
#include	<errMdef.h>	/* EPICS error message routines		*/
#include	<camacLib.h>	/* ESONE CAMAC routines & message codes	*/


/************************************************************************/
/* Code									*/
/************************************************************************/

int bcnaf (int b, int c, int n, int a, int f, int data)
{
   int           dataWord;		/* Data returned from operation	*/
   int           ext;			/* CAMAC channel variable	*/
   int           q, x;			/* Q and X replys		*/
   char         *statMsg;		/* Address of status text	*/
   static char   statText [256];	/* Buffer for status text 	*/
   int           status;		/* Local status variable	*/

  /*---------------------
   * Initialize the CAMAC channel variable and the reply values
   */
   q = x = 0;
   dataWord = data;
   statMsg = statText;

   cdreg (&ext, b, c, n, a);
   ctstat (&status);

  /*---------------------
   * If CAMAC variable initialization succeeded, issue the command
   */
   if (status == OK) {
      cfsa (f, ext, &dataWord, &q);
      ctstat (&status);
      x = X_STATUS (status);
   }/*end if ext initialized*/

  /*---------------------
   * Get the status text.  Status text is either:
   * o OK (if status is 0),
   * o ERROR (if status is less than 0),
   * o The text message corresponding to a valid VxWorks or EPICS status code,
   * o The code itself displayed in hex.
   */
   if (status == OK)
      statMsg = "OK";

   else if (status < 0)
      statMsg = "ERROR";

   else if (errSymFind(status, statMsg))
      sprintf (statText, "0x%06x", status);

  /*---------------------
   * Print the results
   */
   printf ("Command: B(%d) C(%d) N(%d) A(%d) F(%d)\n", b, c, n, a, f);
   printf ("Reply:   q = %d, x = %d,  data = 0x%06x (%d)\n",
            q, x, dataWord, dataWord);
   printf ("Status: %s\n\n", statMsg);

  /*---------------------
   * Since the VxWorks shell always displays the return status value of
   * a call, return it something usefull (i.e. the status code from the
   * CAMAC function call).
   */
   return status;

}/*end bcnaf()*/
