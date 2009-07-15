/******************************************************************************
 * devJ201.h -- Header file for codes referencing the Jorway 201 diagnostic
 *              switch and light register card.
 *
 *-----------------------------------------------------------------------------
 * Author:  Eric Bjorklund
 * Date:    11 August 1995
 *
 *-----------------------------------------------------------------------------
 * Modification Log:
 * 11/08/95	Bjo	Original release
 *
 *****************************************************************************/

/*=====================
 * j201_card -- Structure describing a Jorway 201 card
 *
 * The global variable "j201_list_head" points to a linked list of j201_card
 * structures.  This list may be used by diagnostic programs such as SWLITE
 * to locate all the j201's in the system, and read and write their registers.
 */
typedef struct j201_card   j201_card;

struct j201_card {
   j201_card       *link;	/* Pointer to next j201 card in list	*/
   unsigned short   b;		/* Branch address for this card		*/
   unsigned short   c;		/* Crate address for this card		*/
   unsigned short   n;		/* Slot address for this card		*/
   unsigned short   offline;	/* True if crate is off-line		*/
   int              switch_ext;	/* Camac variable for switch register	*/
   int              light_ext;	/* Camac variable for light register	*/
   int              crate_ext;	/* Camac variable for crate controller	*/
};/*j201_card*/
