/********************************************************************\
 * Transaction.c -- the transaction data structure                  *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1997 Linas Vepstas                                 *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, write to the Free Software      *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.        *
 *                                                                  *
 *   Author: Rob Clark                                              *
 * Internet: rclark@cs.hmc.edu                                      *
 *  Address: 609 8th Street                                         *
 *           Huntington Beach, CA 92648-4632                        *
\********************************************************************/

#include "config.h"

#include "date.h"
#include "Transaction.h"
#include "util.h"

/********************************************************************\
 * Because I can't use C++ for this project, doesn't mean that I    *
 * can't pretend too!  These functions perform actions on the       *
 * Transaction data structure, in order to encapsulate the knowledge       *
 * of the internals of the Transaction in one file.                        *
\********************************************************************/

/********************************************************************\
 * initSplit
 * Initialize a splitaction structure
\********************************************************************/

void
xaccInitSplit( Split * split )
  {
  
  /* fill in some sane defaults */
  split->debit       = NULL;
  split->parent      = NULL;
  
  split->memo        = XtNewString("");
  split->reconciled  = NREC;
  split->damount     = 0.0;
  split->share_price = 1.0;

  split->write_flag  = 0;
  }

/********************************************************************\
\********************************************************************/
Split *
xaccMallocSplit( void )
  {
  Split *split = (Split *)_malloc(sizeof(Split));
  xaccInitSplit (split);
  return split;
  }

/********************************************************************\
\********************************************************************/

void
xaccFreeSplit( Split *split )
  {
  if (!split) return;

  /* free a split only if it is not claimed
   * by any accounts. */
  if (split->debit) return;

  xaccRemoveSplit (split);

  XtFree(split->memo);

  /* just in case someone looks up freed memory ... */
  split->memo        = 0x0;
  split->reconciled  = NREC;
  split->damount     = 0.0;
  split->share_price = 1.0;
  split->parent      = NULL;

  split->write_flag  = 0;
  _free(split);
}

/********************************************************************\
\********************************************************************/

int
xaccCountSplits (Split **tarray)
{
   Split *split;
   int nsplit = 0;

   if (!tarray) return 0;

   split = tarray[0];
   while (split) {
      nsplit ++;
      split = tarray[nsplit];
   }
   return nsplit;
}

/********************************************************************\
 * initTransaction
 * Initialize a transaction structure
\********************************************************************/

void
initTransaction( Transaction * trans )
  {
  
  /* fill in some sane defaults */
  trans->debit = 0x0;
  trans->credit = 0x0;
  
  trans->num         = XtNewString("");
  trans->description = XtNewString("");
  trans->memo        = XtNewString("");
  trans->action      = XtNewString("");
  trans->catagory    = 0;

  trans->splits      = (Split **) _malloc (sizeof (Split *));
  trans->splits[0]   = NULL;

  trans->reconciled  = NREC;
  trans->damount     = 0.0;
  trans->share_price = 1.0;

  trans->credit_balance = 0.0;
  trans->credit_cleared_balance = 0.0;
  trans->debit_balance = 0.0;
  trans->debit_cleared_balance = 0.0;

  trans->date.year   = 1900;        
  trans->date.month  = 1;        
  trans->date.day    = 1;        

  trans->write_flag  = 0;
  }

/********************************************************************\
\********************************************************************/
Transaction *
mallocTransaction( void )
  {
  Transaction *trans = (Transaction *)_malloc(sizeof(Transaction));
  initTransaction (trans);
  return trans;
  }

/********************************************************************\
\********************************************************************/

void
freeTransaction( Transaction *trans )
  {
  if (!trans) return;

  /* free a transaction only if it is not claimed
   * by any accounts. */
  if (trans->debit) return;
  if (trans->credit) return;
/*
hack alert -- don't do this until splits are fully
implemented and tested.
  if (NULL != trans->splits[0]) return;
*/

  _free (trans->splits);
  XtFree(trans->num);
  XtFree(trans->description);
  XtFree(trans->memo);
  XtFree(trans->action);

  /* just in case someone looks up freed memory ... */
  trans->num         = 0x0;
  trans->description = 0x0;
  trans->memo        = 0x0;
  trans->action      = 0x0;
  trans->catagory    = 0;
  trans->reconciled  = NREC;
  trans->damount     = 0.0;
  trans->share_price = 1.0;

  trans->credit_balance = 0.0;
  trans->credit_cleared_balance = 0.0;
  trans->debit_balance = 0.0;
  trans->debit_cleared_balance = 0.0;

  trans->date.year   = 1900;        
  trans->date.month  = 1;        
  trans->date.day    = 1;        

  trans->write_flag  = 0;
  _free(trans);
}

/********************************************************************\
\********************************************************************/
void
xaccInsertSplit (Transaction *trans, Split *split) 
{
   int i, num;
   Split **oldarray;

   if (!trans) return;
   if (!split) return;
   
   split->parent = (struct _transaction *) trans;
   num = xaccCountSplits (trans->splits);

   oldarray = trans->splits;
   trans->splits = (Split **) _malloc ((num+2)*sizeof(Split *));
   for (i=0; i<num; i++) {
      (trans->splits)[i] = oldarray[i];
   }
   trans->splits[num] = split;
   trans->splits[num+1] = NULL;

   if (oldarray) _free (oldarray);
}

/********************************************************************\
\********************************************************************/

void
xaccRemoveSplit (Split *split) 
{
   int i=0, n=0;
   Split *s;
   Transaction *trans;

   if (!split) return;
   trans = (Transaction *) split->parent;
   split->parent = NULL;

   if (!trans) return;

   s = trans->splits[0];
   while (s) {
     trans->splits[i] = trans->splits[n];
     if (split == s) { i--; }
     i++;
     n++;
     s = trans->splits[n];
   }
   trans->splits[i] = NULL;

   /* hack alert -- we should also remove it from the account */
}

/********************************************************************\
 * sorting comparison function
 *
 * returns a negative value if transaction a is dated earlier than b, 
 * returns a positive value if transaction a is dated later than b, 
 *
 * This function tries very hard to uniquely order all transactions.
 * If two transactions occur on the same date, then thier "num" fields
 * are compared.  If the num fileds are identical, then the description
 * fileds are compared.  If these are identical, then the memo fileds 
 * are compared.  Hopefully, there will not be any transactions that
 * occur on the same day that have all three of these values identical.
 *
 * Note that being able to establish this kind of absolute order is 
 * important for some of the ledger display functions.  In particular,
 * grep for "running_balance" in the code, and see the notes there.
 *
 * Yes, this kindof code dependency is ugly, but the alternatives seem
 * ugly too.
 *
\********************************************************************/

int
xaccTransOrder (Transaction **ta, Transaction **tb)
{
  int retval;
  char *da, *db;

  if ( (*ta) && !(*tb) ) return -1;
  if ( !(*ta) && (*tb) ) return +1;
  if ( !(*ta) && !(*tb) ) return 0;

  /* if dates differ, return */
  retval = datecmp (&((*ta)->date), &((*tb)->date));
  if (retval) return retval;

  /* otherwise, sort on transaction strings */
  da = (*ta)->num;
  db = (*tb)->num;
  if (da && db) {
    retval = strcmp (da, db);
    /* if strings differ, return */
    if (retval) return retval;
  } else 
  if (!da && db) {
    return -1;
  } else 
  if (da && !db) {
    return +1;
  }

  /* otherwise, sort on transaction strings */
  da = (*ta)->description;
  db = (*tb)->description;
  if (da && db) {
    retval = strcmp (da, db);
    /* if strings differ, return */
    if (retval) return retval;
  } else 
  if (!da && db) {
    return -1;
  } else 
  if (da && !db) {
    return +1;
  }

  /* otherwise, sort on transaction strings */
  da = (*ta)->memo;
  db = (*tb)->memo;
  if (da && db) {
    retval = strcmp (da, db);
    /* if strings differ, return */
    if (retval) return retval;
  } else 
  if (!da && db) {
    return -1;
  } else 
  if (da && !db) {
    return +1;
  }

  /* otherwise, sort on transaction strings */
  da = (*ta)->action;
  db = (*tb)->action;
  if (da && db) {
    retval = strcmp (da, db);
    /* if strings differ, return */
    if (retval) return retval;
  } else 
  if (!da && db) {
    return -1;
  } else 
  if (da && !db) {
    return +1;
  }

  return 0;
}

/********************************************************************\
\********************************************************************/

int
xaccCountTransactions (Transaction **tarray)
{
   Transaction *trans;
   int ntrans = 0;

   if (!tarray) return 0;

   trans = tarray[0];
   while (trans) {
      ntrans ++;
      trans = tarray[ntrans];
   }
   return ntrans;
}

/************************ END OF ************************************\
\************************* FILE *************************************/

