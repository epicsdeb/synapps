/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
This module implements fifo queues, similar to and inspired by
epicsRingBytes, but with a fixed element size and such that a put
overwrites the last element if the queue is full. Put and get
operations always work on a single element.

The implementation allows one reader and one writer to access the queue
without taking a mutex, except where unavoidable, i.e. when the queue is
full.
\*************************************************************************/
#ifndef INCLseq_queueh
#define INCLseq_queueh

typedef struct seqQueue *QUEUE;

/* to avoid overflow when calculating next put/get positions */
#define seqQueueMaxNumElems (((size_t)-1)>>1)

/* Create a new queue with the given element size and
   number of elements and return it, if successful,
   otherwise return NULL.
   Restrictions:
      numElems > 0
      numElems <= seqQueueMaxNumElems
      elemSize > 0
*/
epicsShareFunc QUEUE seqQueueCreate(size_t numElems, size_t elemSize);

/* Return whether all invariants are satisfied */
epicsShareFunc boolean seqQueueInvariant(QUEUE q);

/* A common precondition of the following operations is
   that their QUEUE argument is valid; they do not check
   this.

   A QUEUE is valid if it has been received as a
   non-NULL result of calling seqQueueCreate, and if
   seqQueueDelete has not been called for it.

   Note that seqQueueGet returns FALSE on success and
   that seqQueuePut returns FALSE if no element was
   overwritten. */

/* Destroy the queue, freeing all memory. */
epicsShareFunc void seqQueueDestroy(QUEUE q);

/* Get an element from the queue if possible. Return
   whether the queue was empty and therefore no
   element is available. The value argument must point to a
   memory area with at least seqQueueElemSize(q)
   bytes. */
epicsShareFunc boolean seqQueueGet(QUEUE q, void *value);

/* Put an element into the queue. Return whether the
   queue was full and therefore its last element was
   overwritten. The value argument must point to a
   memory area with at least seqQueueElemSize(q)
   bytes. */
epicsShareFunc boolean seqQueuePut(QUEUE q, const void *value);

/* Remove all elements. Cheap. */
epicsShareFunc void seqQueueFlush(QUEUE q);

/* How many free elements are left. */
epicsShareFunc size_t seqQueueFree(const QUEUE q);

/* How many elements are used up. */
epicsShareFunc size_t seqQueueUsed(const QUEUE q);

/* Number of elements (fixed on construction). */
epicsShareFunc size_t seqQueueNumElems(const QUEUE q);

/* Element size (fixed on construction). */
epicsShareFunc size_t seqQueueElemSize(const QUEUE q);

/* Whether empty, same as seqQueueUsed(q)==0 */
epicsShareFunc boolean seqQueueIsEmpty(const QUEUE q);

/* Whether full, same as seqQueueFree(q)==0 */
epicsShareFunc boolean seqQueueIsFull(const QUEUE q);


/* Unsafe operations; use with care */
typedef void* seqQueueFunc(void *dest, const void *src, size_t elemSize);

/* Like seqQueueGet but does not copy the element's data;
   instead the user supplied function is called.
   seqQueueGet(q,v) == seqQueueGetF(q,memcpy,v)
   */
epicsShareFunc boolean seqQueueGetF(QUEUE q, seqQueueFunc *f, void *arg);

/* Like seqQueuePut but does not copy the element's data;
   instead the user supplied function is called.
   seqQueuePut(q,v) == seqQueuePutF(q,memcpy,v) */
epicsShareFunc boolean seqQueuePutF(QUEUE q, seqQueueFunc *f, const void *arg);

#endif /* INCLseq_queueh */
