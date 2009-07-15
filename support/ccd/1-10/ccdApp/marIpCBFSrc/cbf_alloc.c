/**********************************************************************
 * cbf_alloc -- memory allocation                                     *
 *                                                                    *
 * Version 0.6 13 January 1999                                        *
 *                                                                    *
 *            Paul Ellis (ellis@ssrl.slac.stanford.edu) and           *
 *         Herbert J. Bernstein (yaya@bernstein-plus-sons.com)        *
 **********************************************************************/
  
/**********************************************************************
 *                               NOTICE                               *
 * Creative endeavors depend on the lively exchange of ideas. There   *
 * are laws and customs which establish rights and responsibilities   *
 * for authors and the users of what authors create.  This notice     *
 * is not intended to prevent you from using the software and         *
 * documents in this package, but to ensure that there are no         *
 * misunderstandings about terms and conditions of such use.          *
 *                                                                    *
 * Please read the following notice carefully.  If you do not         *
 * understand any portion of this notice, please seek appropriate     *
 * professional legal advice before making use of the software and    *
 * documents included in this software package.  In addition to       *
 * whatever other steps you may be obliged to take to respect the     *
 * intellectual property rights of the various parties involved, if   *
 * you do make use of the software and documents in this package,     *
 * please give credit where credit is due by citing this package,     *
 * its authors and the URL or other source from which you obtained    *
 * it, or equivalent primary references in the literature with the    *
 * same authors.                                                      *
 *                                                                    *
 * Some of the software and documents included within this software   *
 * package are the intellectual property of various parties, and      *
 * placement in this package does not in any way imply that any       *
 * such rights have in any way been waived or diminished.             *
 *                                                                    *
 * With respect to any software or documents for which a copyright    *
 * exists, ALL RIGHTS ARE RESERVED TO THE OWNERS OF SUCH COPYRIGHT.   *
 *                                                                    *
 * Even though the authors of the various documents and software      *
 * found here have made a good faith effort to ensure that the        *
 * documents are correct and that the software performs according     *
 * to its documentation, and we would greatly appreciate hearing of   *
 * any problems you may encounter, the programs and documents any     *
 * files created by the programs are provided **AS IS** without any   *
 * warranty as to correctness, merchantability or fitness for any     *
 * particular or general use.                                         *
 *                                                                    *
 * THE RESPONSIBILITY FOR ANY ADVERSE CONSEQUENCES FROM THE USE OF    *
 * PROGRAMS OR DOCUMENTS OR ANY FILE OR FILES CREATED BY USE OF THE   *
 * PROGRAMS OR DOCUMENTS LIES SOLELY WITH THE USERS OF THE PROGRAMS   *
 * OR DOCUMENTS OR FILE OR FILES AND NOT WITH AUTHORS OF THE          *
 * PROGRAMS OR DOCUMENTS.                                             *
 **********************************************************************/
 
/**********************************************************************
 *                          The IUCr Policy                           *
 *                                 on                                 *
 *     the Use of the Crystallographic Information File (CIF)         *
 *                                                                    *
 * The Crystallographic Information File (Hall, Allen & Brown,        *
 * 1991) is, as of January 1992, the recommended method for           *
 * submitting publications to Acta Crystallographica Section C. The   *
 * International Union of Crystallography holds the Copyright on      *
 * the CIF, and has applied for Patents on the STAR File syntax       *
 * which is the basis for the CIF format.                             *
 *                                                                    *
 * It is a principal objective of the IUCr to promote the use of      *
 * CIF for the exchange and storage of scientific data. The IUCr's    *
 * sponsorship of the CIF development was motivated by its            *
 * responsibility to its scientific journals, which set the           *
 * standards in crystallographic publishing. The IUCr intends that    *
 * CIFs will be used increasingly for electronic submission of        *
 * manuscripts to these journals in future. The IUCr recognises       *
 * that, if the CIF and the STAR File are to be adopted as a means    *
 * for universal data exchange, the syntax of these files must be     *
 * strictly and uniformly adhered to. Even small deviations from      *
 * the syntax would ultimately cause the demise of the universal      *
 * file concept. Through its Copyrights and Patents the IUCr has      *
 * taken the steps needed to ensure strict conformance with this      *
 * syntax.                                                            *
 *                                                                    *
 * The IUCr policy on the use of the CIF and STAR File processes is   *
 * as follows:                                                        *
 * _________________________________________________________________  *
 *                                                                    *
 *  * 1 CIFs and STAR Files may be generated, stored or transmitted,  *
 *    without permission or charge, provided their purpose is not     *
 *    specifically for profit or commercial gain, and provided that   *
 *    the published syntax is strictly adhered to.                    *
 *  * 2 Computer software may be developed for use with CIFs or STAR  *
 *    files, without permission or charge, provided it is distributed *
 *    in the public domain. This condition also applies to software   *
 *    for which a charge is made, provided that its primary function  *
 *    is for use with files that satisfy condition 1 and that it is   *
 *    distributed as a minor component of a larger package of         *
 *    software.                                                       *
 *  * 3 Permission will be granted for the use of CIFs and STAR Files *
 *    for specific commercial purposes (such as databases or network  *
 *    exchange processes), and for the distribution of commercial     *
 *    CIF/STAR software, on written application to the IUCr Executive *
 *    Secretary, 2 Abbey Square, Chester CH1 2HU, England. The        *
 *    nature, terms and duration of the licences granted will be      *
 *    determined by the IUCr Executive and Finance Committees.        *
 *                                                                    *
 * _________________________________________________________________  *
 *                                                                    *
 * In summary, the IUCr wishes to promote the use of the STAR File    *
 * concepts as a standard universal data file. It will insist on      *
 * strict compliance with the published syntax for all                *
 * applications. To assist with this compliance, the IUCr provides    *
 * public domain software for checking the logical integrity of a     *
 * CIF, and for validating the data name definitions contained        *
 * within a CIF. Detailed information on this software, and the       *
 * associated dictionaries, may be obtained from the IUCr Office at   *
 * 5 Abbey Square, Chester CH1 2HU, England.                          *
 **********************************************************************/

#ifdef __cplusplus

extern "C" {

#endif

#include "cbf.h"
#include "cbf_alloc.h"

#include <stdlib.h>
#include <string.h>


  /* Reallocate a block of memory (never lose the old block on failure) */

int cbf_realloc (void **old_block, size_t *old_nelem, 
                 size_t elsize, size_t nelem)
{
  void *new_block;

  
    /* Are the arguments valid? */
    
  if (!old_block || elsize == 0)

    return CBF_ARGUMENT;


    /* Is the size alread correct? */

  if (old_nelem)

    if (*old_nelem == nelem)

      return 0;


    /* Allocate the memory */

  if (nelem > 0)
  {
    new_block = malloc (nelem * elsize);

    if (!new_block)

      return CBF_ALLOC;
  }
  else

    new_block = NULL;


    /* Copy the old data */

  if (old_nelem)
  
    if (*old_block && *old_nelem > 0 && nelem > 0)
    {
      if (*old_nelem > nelem)
    
        *old_nelem = nelem;

      memcpy (new_block, *old_block, *old_nelem * elsize);
    }


    /* Free the old memory */

  if (*old_block)

    free (*old_block);


    /* Clear the new data */

  if (!old_nelem)

    memset (new_block, 0, nelem * elsize);
  
  else
  
    if (nelem > 0 && nelem > *old_nelem)

      memset (((char *) new_block) + *old_nelem * elsize, 0,
                                     (nelem - *old_nelem) * elsize);


    /* Replace the old data */

  *old_block = new_block;

  if (old_nelem)
  
    *old_nelem = nelem;
  

    /* Success */

  return 0;
}


  /* Allocate a block of memory */

int cbf_alloc (void **new_block, size_t *new_nelem, 
               size_t elsize, size_t nelem)
{
    /* Are the arguments valid? */
    
  if (!new_block)

    return CBF_ARGUMENT;


    /* Initialise */

  *new_block = NULL;

  if (new_nelem)
  
    *new_nelem = 0;


    /* Allocate the memory */

  return cbf_realloc (new_block, new_nelem, elsize, nelem);
}


  /* Free a block of memory */

int cbf_free (void **old_block, size_t *old_nelem)
{
    /* Are the arguments valid? */
    
  if (!old_block)

    return CBF_ARGUMENT;


    /* Free the memory */

  if (*old_block)

    free (*old_block);

  *old_block = NULL;

  if (old_nelem)
  
    *old_nelem = 0;


    /* Success */

  return 0;
}


#ifdef __cplusplus

}

#endif
