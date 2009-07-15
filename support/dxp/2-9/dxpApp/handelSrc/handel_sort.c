
/*
 * handel_sort.c
 *
 * Created 10/16/01 -- PJF
 *
 * Copyright (c) 2002,2003,2004, X-ray Instrumentation Associates
 *               2005, XIA LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 *   * Redistributions in binary form must reproduce the 
 *     above copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 *   * Neither the name of X-ray Instrumentation Associates 
 *     nor the names of its contributors may be used to endorse 
 *     or promote products derived from this software without 
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 */


#include <stdlib.h>

#include "xia_handel.h"
#include "xia_sort.h"


/*****************************************************************************
 *
 * Since the Firmware LLs tend to be rather small, we can use insertion sort
 * to quickly sort it.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaInsertSort(Firmware **head, int (*compare)(const void *key1, const void *key2))
{
	Firmware *iterator = NULL;
	Firmware *toInsert = NULL;

	iterator = getListNext(*head);

	while (iterator != NULL)
	{
		if (compare(getListPrev(iterator), iterator) == 1) 
		{
			toInsert = iterator;
			while (compare(getListPrev(toInsert), toInsert) == 1)
			{
				xiaSwap(getListPrev(toInsert), toInsert);

				if (getListPrev(toInsert) == NULL)
				{
					break;
				}
			}
		}

		iterator = getListNext(iterator);
	}

/* Need to re-point head at the new head of the list. 
 * Just walk head backwards until head->prev == NULL. Could use a
 * pointer-to-a-pointer here.
 */
	while (getListPrev(*head) != NULL)
	{
		*head = getListPrev(*head);
	}


	return 0;
}


/*****************************************************************************
 *
 * This routine merge sorts an array of elements data. This code is 
 * shamelessly torn from "Mastering Algorithms with C" by Kyle Loudon 
 * (O'Reilly Associates) p 318.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaMergeSort(void *data, int size, int esize, int i, int k, int (*compare)(const void *key1, const void *key2))
{
	int j;

	if (i < k)
	{
		j = (int)(((i + k - 1)) / 2);

		if (xiaMergeSort(data, size, esize, i, j, compare) < 0)
		{
			return -1;
		}

		if (xiaMergeSort(data, size, esize, j + 1, k, compare) < 0)
		{
			return -1;
		}

		if (xiaMerge(data, esize, i, j, k, compare) < 0)
		{
			return -1;
		}
	}

	return 0;
}

/*****************************************************************************
 * 
 * This routine is used by xiaMergeSort to merge together elements. Taken from
 * the same reference as xiaMergeSort.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaMerge(void *data, int esize, int i, int j, int k, int (*compare)(const void *key1, const void *key2))
{	
	
	char *a = data;
	char *m;

	int ipos;
	int jpos;
	int mpos;

	
	ipos = i;
	jpos = j + 1;
	mpos = 0;


	if ((m = (char *)handel_md_alloc(esize * ((k - i) + 1))) == NULL)
	{
		return -1;
	}

	while (ipos <= j || jpos <= k)
	{
		if (ipos > j)
		{
			while (jpos <= k)
			{
				memcpy(&m[mpos * esize], &a[jpos * esize], esize);
				jpos++;
				mpos++;
			}

			continue;
		
		} else if (jpos > k) {

			while (ipos <= j)
			{
				memcpy(&m[mpos * esize], &a[ipos * esize], esize);
				ipos++;
				mpos++;
			}

			continue;
		}

		if (compare(&a[ipos * esize], &a[jpos * esize]) < 0)
		{
			memcpy(&m[mpos * esize], &a[ipos * esize], esize);
			ipos++;
			mpos++;
		
		} else {

			memcpy(&m[mpos * esize], &a[jpos * esize], esize);
			jpos++;
			mpos++;
		}
	}

	memcpy(&a[i * esize], m, esize * ((k - i) + 1));
	handel_md_free(m);
	
	return 0;
}


/*****************************************************************************
 *
 * This routine swaps two elements in a Firmware LL. Ultimately, this should
 * be done as a macro and then it would be LL-type independent.
 *
 *****************************************************************************/
HANDEL_STATIC void HANDEL_API xiaSwap(Firmware *left, Firmware *right)
{
	left->next = right->next;
	right->prev = left->prev;
	
	if (left->prev != NULL)
	{
		left->prev->next = right;
	}

	left->prev = right;

	if (right->next != NULL)
	{
		right->next->prev = left;
	}

	right->next = left;

}
