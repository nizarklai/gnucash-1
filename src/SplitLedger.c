/* 
 * FILE:
 * SplitLedger.c 
 *
 * FUNCTION:
 * copy transaction data from engine into split-register object.
 *
 *
 * DESIGN NOTES:
 * Some notes about the "blank split":
 * Q: What is the "blank split"?
 * A: A new, empty split appended to the bottom of the ledger
 *    window.  The blank split provides an area where the user
 *    can type in new split/transaction info.  
 *    The "blank split" is treated in a special way for a number
 *    of reasons:
 *    (1) it must always appear as the bottom-most split
 *        in the Ledger window,
 *    (2) it must be committed if the user edits it, and 
 *        a new blank split must be created.
 *    (3) it must be deleted when the ledger window is closed.
 * To implement the above, the register "user_data" is used
 * to store an SRInfo structure containing the blank split.
 *
 * =====================================================================
 * Some notes on Commit/Rollback:
 * 
 * There's an engine compnenent and a gui componenet to the commit/rollback
 * scheme.  On the engine side, one must always call BeginEdit()
 * before starting to edit a transaction.  When you think you're done,
 * you can call CommitEdit() to commit the changes, or RollbackEdit() to
 * go back to how things were before you started the edit.  Think of it as
 * a one-shot mega-undo for that transaction.
 * 
 * Note that the query engine uses the original values, not the currently
 * edited values, when performing a sort.  This allows your to e.g. edit
 * the date without having the transaction hop around in the gui while you
 * do it.
 * 
 * On the gui side, commits are now performed on a per-transaction basis,
 * rather than a per-split (per-journal-entry) basis.  This means that
 * if you have a transaction with a lot of splits in it, you can edit them
 * all you want without having to commit one before moving to the next.
 * 
 * Similarly, the "cancel" button will now undo the changes to all of the
 * lines in the transaction display, not just to one line (one split) at a
 * time.
 * 
 * =====================================================================
 * Some notes on Reloads & Redraws:
 * 
 * Reloads and redraws tend to be heavyweight. We try to use change flags
 * as much as possible in this code, but imagine the following senario:
 *
 * Create two bank accounts.  Transfer money from one to the other.
 * Open two registers, showing each account. Change the amount in one window.
 * Note that the other window also redraws, to show the new correct amount.
 * 
 * Since you changed an amount value, potentially *all* displayed balances change
 * in *both* register windows (as well as the leger balance in the main window).
 * Three or more windows may be involved if you have e.g. windows open for bank,
 * employer, taxes and your entering a paycheck...  (or correcting a typo in an
 * old paycheck)... changing a date might even cause all entries in all three
 * windows to be re-ordered.
 * 
 * The only thing I can think of is a bit stored with every table entry, stating
 * 'this entry has changed since lst time, redraw it'.  But that still doesn't
 * avoid the overhead of reloading the table from the engine.
 * 
 * 
 *
 * HISTORY:
 * Copyright (c) 1998,1999 Linas Vepstas
 */

/********************************************************************\
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
\********************************************************************/

#include <assert.h>
#include <stdio.h>

#include "top-level.h"

#include "ui-callbacks.h"
#include "messages.h"
#include "SplitLedger.h"
#include "MultiLedger.h"
#include "Refresh.h"
#include "splitreg.h"
#include "table-allgui.h"
#include "util.h"

#define BUFSIZE 1024

typedef struct _SRInfo SRInfo;
struct _SRInfo
{
  /* The blank split at the bottom of the register */
  Split * blank_split;

  /* The currently open transaction, if any */
  Transaction *pending_trans;

  /* A transaction used to remember where to expand the cursor */
  Transaction *cursor_hint_trans;

  /* The default account where new splits are added */
  Account *default_source_account;
};


/* ======================================================== */
/* The force_double_entry_awareness flag controls how the 
 * register behaves if the user failed to specify a transfer-to
 * account when creating a new split. What it does is simple,
 * although it can lead to some confusion to the user.
 * If this flag is set, then any new split will be put into
 * the leader account. What happens visually is that it appears
 * as if there are two transactions, one debiting and one crediting
 * this acount by exactly the same amount. Thus, the user is forced
 * to deal with this somewhat nutty situation.
 *
 * If this flag is *not* set, then the split just sort of 
 * hangs out, without belonging to any account. This will 
 * of course lead to a ledger that fails to balance.
 * Bummer, duude !
 *
 * hack alert -- this flag should really be made a configurable 
 * item in some config script.
 */

static int force_double_entry_awareness = 0;

/* This static indicates the debugging module that this .o belongs to.  */
static short module = MOD_LEDGER;

/* The routines below create, access, and destroy the SRInfo structure
 * used by SplitLedger routines to store data for a particular register.
 * This is the only code that should access the user_data member of a
 * SplitRegister directly. If additional user data is needed, just add
 * it to the SRInfo structure above. */
static void
xaccSRInitRegisterData(SplitRegister *reg)
{
  assert(reg != NULL);

  /* calloc initializes to 0 */
  reg->user_data = calloc(1, sizeof(SRInfo));
  assert(reg->user_data != NULL);
}

static void
xaccSRDestroyRegisterData(SplitRegister *reg)
{
  assert(reg != NULL);

  if (reg->user_data != NULL)
    free(reg->user_data);

  reg->user_data = NULL;
}

static SRInfo *
xaccSRGetInfo(SplitRegister *reg)
{
  assert(reg != NULL);

  if (reg->user_data == NULL)
    xaccSRInitRegisterData(reg);

  return (SRInfo *) reg->user_data;
}

/* ======================================================== */
/* this callback gets called when the user clicks on the gui
 * in such a way as to leave the current transaction, and to 
 * go to a new one.  So, save the current transaction.
 *
 * This callback is centrally involved in the redraw sequence.
 * When the user moves from one cell to another, the following 
 * sequence of events get triggered and cascade down:
 *    enterCB () {
 *      VerifyCursorPosition() {
 *        MoveCursor() {  
 *         callback for move() which is this function (LedgerMoveCursor) {
 *           SaveRegEntry() {...}
 *           RedrawRegEntry() {
 *              SRLoadRegister() {
 *                SRLoadRegEntry() {
 *                   MoveCursor () { }
 *                }
 *             }
 *          } }}}}
 */

static void
LedgerMoveCursor  (Table *table, 
                   int *p_new_phys_row, 
                   int *p_new_phys_col, 
                   void * client_data)
{
   int new_phys_row = *p_new_phys_row;
   int new_phys_col = *p_new_phys_col;
   SplitRegister *reg = (SplitRegister *) client_data;
   SRInfo *info = xaccSRGetInfo(reg);
   Transaction *newtrans = NULL;
   Locator *locator;
   Split *split;
   int style;

   PINFO ("LedgerMoveCursor(): start calback %d %d \n",
          new_phys_row, new_phys_col);

   /* The split where we are moving to */
   split = xaccGetUserData (reg->table, new_phys_row, new_phys_col);
   if (split != NULL)
     newtrans = xaccSplitGetParent(split);

   /* commit the contents of the cursor into the database */
   xaccSRSaveRegEntry (reg, newtrans);
   xaccSRRedrawRegEntry (reg); 

   PINFO ("LedgerMoveCursor(): after redraw %d %d \n",
          new_phys_row, new_phys_col);

   reg->cursor_phys_row = new_phys_row;
   locator = table->locators[new_phys_row][new_phys_col];
   reg->cursor_virt_row = locator->virt_row;

   /* if auto-expansion is enabled, we need to redraw the register
    * to expand out the splits at the new location.  We do some
    * tomfoolery here to trick the code into expanding the new location.
    * This little futz is sleazy, but it does suceed in getting the 
    * LoadRegister code into expanding the appropriate split.
    * 
    */   
   style = ((reg->type) & REG_STYLE_MASK);
   if ((REG_SINGLE_DYNAMIC == style) ||
       (REG_DOUBLE_DYNAMIC == style)) 
   {
      Split *split, *oldsplit;
      oldsplit = xaccSRGetCurrentSplit (reg);
      split = xaccGetUserData (reg->table, new_phys_row, new_phys_col);
      reg->table->current_cursor->user_data = (void *) split;

      /* if a null split, provide a hint for where the cursor should go */
      if (NULL == split) {
         reg->cursor_phys_row = new_phys_row;
         // reg->cursor_virt_row = reg->table->current_cursor_virt_row;
         info->cursor_hint_trans = xaccSplitGetParent (oldsplit);
      }
      xaccRegisterRefresh (reg);
      gnc_refresh_main_window();

      /* indicate what row we *should* have gone to */
      *p_new_phys_row = table->current_cursor_phys_row;
      PINFO ("LedgerMoveCursor(): after dynamic %d %d stored val %d\n",
           *p_new_phys_row, new_phys_col, reg->cursor_phys_row);
   }
}

/* ======================================================== */
/* this callback gets called when the user clicks on the gui
 * in such a way as to leave the current transaction, and to 
 * go to a new one. It is called to verify what the coordinates
 * of the new cell will be. It currently does nothing.
 */

static void
LedgerTraverse  (Table *table, 
                 int *p_new_phys_row, 
                 int *p_new_phys_col, 
                 void * client_data)
{
   int new_phys_row = *p_new_phys_row;
   int new_phys_col = *p_new_phys_col;
   SplitRegister *reg = (SplitRegister *) client_data;
   SRInfo *info = xaccSRGetInfo(reg);
   int style;

   /* For now, just do nothing. The auto mode handling is done entirely
    * by LedgerMoveCursor above. */
   return;

   /* if auto-expansion is enabled, we need to redraw the register
    * to expand out the splits at the new location.  We do some
    * tomfoolery here to trick the code into expanding the new location.
    * This little futz is sleazy, but it does suceed in getting the 
    * LoadRegister code into expanding the appropriate split.
    */   
   style = ((reg->type) & REG_STYLE_MASK);
   if ((REG_SINGLE_DYNAMIC == style) ||
       (REG_DOUBLE_DYNAMIC == style)) 
   {
      Split *split, *oldsplit;
      int save_num_phys_rows, save_num_virt_rows;
      int save_cursor_phys_row, save_cursor_virt_row;

      /* Save the values that xaccSRCountRows will futz up */
      save_num_phys_rows = reg->num_phys_rows;
      save_num_virt_rows = reg->num_virt_rows;
      save_cursor_phys_row = reg->cursor_phys_row;
      save_cursor_virt_row = reg->cursor_virt_row;

      ENTER ("LedgerTraverse with %d %d \n", new_phys_row , new_phys_col);
      oldsplit = xaccSRGetCurrentSplit (reg);
      split = xaccGetUserData (reg->table, new_phys_row, new_phys_col);
      reg->table->current_cursor->user_data = (void *) split;

      /* if a null split, provide a hint for where the cursor should go */
      if (NULL == split) {
         reg->cursor_phys_row = new_phys_row;
         // reg->cursor_virt_row = reg->table->current_cursor_virt_row;
         info->cursor_hint_trans = xaccSplitGetParent (oldsplit);
      }

      xaccRegisterCountHack (reg);
      reg->table->current_cursor->user_data = (void *) oldsplit;

      LEAVE ("LedgerTraverse with %d \n", reg->cursor_phys_row);
      /* indicate what row we *should* go to */
      *p_new_phys_row = reg->cursor_phys_row;

      /* Restore the values */
      reg->num_phys_rows = save_num_phys_rows;
      reg->num_virt_rows = save_num_virt_rows;
      reg->cursor_phys_row = save_cursor_phys_row;
      reg->cursor_virt_row = save_cursor_virt_row;
   }
}

/* ======================================================== */

static void
LedgerDestroy (SplitRegister *reg)
{
   SRInfo *info = xaccSRGetInfo(reg);
   Transaction *trans;

   /* be sure to destroy the "blank split" */
   if (info->blank_split) {
      /* split destroy will automatically remove it
       * from its parent account */
      trans = xaccSplitGetParent (info->blank_split);

      /* Make sure we don't commit this below */
      if (trans == info->pending_trans)
        info->pending_trans = NULL;

      xaccTransBeginEdit (trans, 1);
      xaccTransDestroy (trans);
      xaccTransCommitEdit (trans);
      info->blank_split = NULL;
   }

   /* be sure to take care of any open transactions */
   if (info->pending_trans) {
      /* Committing this should have been taken care of by
       * xaccLedgerDisplayClose. But, we'll check again. */
      if (xaccTransIsOpen(info->pending_trans))
        xaccTransCommitEdit (info->pending_trans);

      info->pending_trans = NULL;
   }

   xaccSRDestroyRegisterData(reg);
}

/* ======================================================== */

Transaction * 
xaccSRGetCurrentTrans (SplitRegister *reg)
{
  Split *split;
  int pr, pc;
  int vr, vc;

  split = xaccSRGetCurrentSplit (reg);
  if (split != NULL)
    return xaccSplitGetParent(split);

  /* Split is blank. Assume it is the blank split of a multi-line
   * transaction. Go back one row to find a split in the transaction. */
  pr = reg->cursor_phys_row;
  pc = reg->table->current_cursor_phys_col;
  vr = reg->table->locators[pr][pc]->virt_row;
  vc = reg->table->locators[pr][pc]->virt_col;
  vr --;
  if ((0 > vr) || (0 > vc)) {
    PERR ("Internal Error: SaveRegEntry(): bad row \n");
    return NULL;
  }

  split = (Split *) reg->table->user_data[vr][vc];
  if (split == NULL) {
    PERR ("Internal Error: SaveRegEntry(): no parent \n");
    return NULL;
  }

  return xaccSplitGetParent(split);
}

/* ======================================================== */

Split * 
xaccSRGetCurrentSplit (SplitRegister *reg)
{
   CellBlock *cursor;
   Split *split;

   /* get the handle to the current split and transaction */
   cursor = reg->table->current_cursor;
   if (!cursor) return NULL;
   split = (Split *) (cursor->user_data);

   return split;
}

/* ======================================================== */

Split * 
xaccSRGetBlankSplit (SplitRegister *reg)
{
  SRInfo *info = xaccSRGetInfo(reg);

  return info->blank_split;
}

/* ======================================================== */

gncBoolean
xaccSRGetSplitRowCol (SplitRegister *reg, Split *split,
                      int *virt_row, int *virt_col)
{
  Table *table = reg->table;
  int v_row, v_col;
  Split *s;

  for (v_row = 1; v_row < table->num_virt_rows; v_row++)
    for (v_col = 0; v_col < table->num_virt_cols; v_col++)
    {
      s = (Split *) table->user_data[v_row][v_col];

      if (s == split)
      {
        if (virt_row != NULL)
          *virt_row = v_row;
        if (virt_col != NULL)
          *virt_col = v_col;

        return GNC_T;
      }
    }

  return GNC_F;
}

/* ======================================================== */

void
xaccSRDeleteCurrentSplit (SplitRegister *reg)
{
  SRInfo *info = xaccSRGetInfo(reg);
  Split *split, *s;
  Transaction *trans;
  int i, num_splits;
  Account *account, **affected_accounts;

  /* get the current split based on cursor position */
  split = xaccSRGetCurrentSplit(reg);
  if (split == NULL)
    return;

  /* If we just deleted the blank split, clean up. The user is
   * allowed to delete the blank split as a method for discarding any
   * edits they may have made to it. */
  if (split == info->blank_split)
  {
    account = xaccSplitGetAccount(split);
    xaccAccountDisplayRefresh(account);
    return;
  }

  /* make a copy of all of the accounts that will be  
   * affected by this deletion, so that we can update
   * their register windows after the deletion.
   */
  trans = xaccSplitGetParent(split);
  num_splits = xaccTransCountSplits(trans);
  affected_accounts = (Account **) malloc((num_splits + 1) *
                                          sizeof(Account *));
  assert(affected_accounts != NULL);

  for (i = 0; i < num_splits; i++) 
  {
    s = xaccTransGetSplit(trans, i);
    affected_accounts[i] = xaccSplitGetAccount(s);
  }
  affected_accounts[num_splits] = NULL;

  account = xaccSplitGetAccount(split);

  xaccTransBeginEdit(trans, 1);
  xaccAccountBeginEdit(account, 1);
  xaccSplitDestroy(split);
  xaccAccountCommitEdit(account);
  xaccTransCommitEdit(trans);

  /* Check pending transaction */
  if (trans == info->pending_trans)
    info->pending_trans = NULL;

  gnc_account_list_ui_refresh(affected_accounts);

  free(affected_accounts);

  gnc_refresh_main_window ();
}

/* ======================================================== */

void
xaccSRDeleteCurrentTrans (SplitRegister *reg)
{
  SRInfo *info = xaccSRGetInfo(reg);
  Split *split, *s;
  Transaction *trans;
  int i, num_splits;
  Account *account, **affected_accounts;

  /* get the current split based on cursor position */
  split = xaccSRGetCurrentSplit(reg);
  if (split == NULL)
    return;

  /* If we just deleted the blank split, clean up. The user is
   * allowed to delete the blank split as a method for discarding any
   * edits they may have made to it. */
  if (split == info->blank_split)
  {
    trans = xaccSplitGetParent (info->blank_split);
    account = xaccSplitGetAccount(split);

    /* Make sure we don't commit this later on */
    if (trans == info->pending_trans)
      info->pending_trans = NULL;

    xaccTransBeginEdit (trans, 1);
    xaccTransDestroy (trans);
    xaccTransCommitEdit (trans);

    info->blank_split = NULL;

    xaccAccountDisplayRefresh(account);
    return;
  }

  /* make a copy of all of the accounts that will be  
   * affected by this deletion, so that we can update
   * their register windows after the deletion.
   */
  trans = xaccSplitGetParent(split);
  num_splits = xaccTransCountSplits(trans);
  affected_accounts = (Account **) malloc((num_splits + 1) *
                                          sizeof(Account *));
  assert(affected_accounts != NULL);

  for (i = 0; i < num_splits; i++) 
  {
    s = xaccTransGetSplit(trans, i);
    affected_accounts[i] = xaccSplitGetAccount(s);
  }
  affected_accounts[num_splits] = NULL;

  xaccTransBeginEdit(trans, 1);
  xaccTransDestroy(trans);
  xaccTransCommitEdit(trans);

  /* Check pending transaction */
  if (trans == info->pending_trans)
    info->pending_trans = NULL;

  gnc_account_list_ui_refresh(affected_accounts);

  free(affected_accounts);

  gnc_refresh_main_window ();
}

/* ======================================================== */

void
xaccSREmptyCurrentTrans (SplitRegister *reg)
{
  SRInfo *info = xaccSRGetInfo(reg);
  Split *split, *s;
  Split **splits;
  Transaction *trans;
  int i, num_splits;
  Account *account, **affected_accounts;

  /* get the current split based on cursor position */
  split = xaccSRGetCurrentSplit(reg);
  if (split == NULL)
    return;

  /* If we just deleted the blank split, clean up. The user is
   * allowed to delete the blank split as a method for discarding any
   * edits they may have made to it. */
  if (split == info->blank_split)
  {
    trans = xaccSplitGetParent (info->blank_split);
    account = xaccSplitGetAccount(split);

    /* Make sure we don't commit this later on */
    if (trans == info->pending_trans)
      info->pending_trans = NULL;

    xaccTransBeginEdit (trans, 1);
    xaccTransDestroy (trans);
    xaccTransCommitEdit (trans);

    info->blank_split = NULL;

    xaccAccountDisplayRefresh(account);
    return;
  }

  /* make a copy of all of the accounts that will be  
   * affected by this deletion, so that we can update
   * their register windows after the deletion.
   */
  trans = xaccSplitGetParent(split);
  num_splits = xaccTransCountSplits(trans);
  affected_accounts = (Account **) malloc((num_splits + 1) *
                                          sizeof(Account *));
  splits = calloc(num_splits, sizeof(Split *));

  assert(affected_accounts != NULL);
  assert(splits != NULL);

  for (i = 0; i < num_splits; i++) 
  {
    s = xaccTransGetSplit(trans, i);
    splits[i] = s;
    affected_accounts[i] = xaccSplitGetAccount(s);
  }
  affected_accounts[num_splits] = NULL;

  xaccTransBeginEdit(trans, 1);
  for (i = 0; i < num_splits; i++)
    if (splits[i] != split)
      xaccSplitDestroy(splits[i]);
  xaccTransCommitEdit(trans);

  /* Check pending transaction */
  if (trans == info->pending_trans)
    info->pending_trans = NULL;

  gnc_account_list_ui_refresh(affected_accounts);

  free(affected_accounts);
  free(splits);

  gnc_refresh_main_window ();
}

/* ======================================================== */

void
xaccSRCancelCursorSplitChanges (SplitRegister *reg)
{
  Split * split;
  unsigned int changed;

  changed = xaccSplitRegisterGetChangeFlag(reg);
  if (!changed)
    return;

  /* We're just cancelling the current split here, not the transaction */
  /* When cancelling edits, reload the cursor from the transaction */
  split = xaccSRGetCurrentSplit(reg);
  xaccSRLoadRegEntry(reg, split);
  xaccRefreshTableGUI(reg->table);
  xaccSplitRegisterClearChangeFlag(reg);
}

/* ======================================================== */

void
xaccSRCancelCursorTransChanges (SplitRegister *reg)
{
  SRInfo *info = xaccSRGetInfo(reg);
  Transaction *trans;
  Split *split;
  int i, num_splits = 0, more_splits = 0;
  Account **affected_accounts = NULL;

  /* Get the currently open transaction, rollback the edits on it, and
   * then repaint everything. To repaint everything, make a note of
   * all of the accounts that will be affected by this rollback. Geez,
   * there must be some easier way of doing redraw notification. */
  trans = info->pending_trans;

  if (!xaccTransIsOpen(trans))
  {
    xaccSRCancelCursorSplitChanges(reg);
    return;
  }

  num_splits = xaccTransCountSplits (trans);
  affected_accounts = (Account **) malloc ((num_splits+1) *
                                           sizeof (Account *));

  for (i = 0; i < num_splits; i++)
  {
    split = xaccTransGetSplit (trans, i);
    affected_accounts[i] = xaccSplitGetAccount (split);
  }
  affected_accounts[num_splits] = NULL;
   
  xaccTransRollbackEdit (trans);
     
  /* and do some more redraw, for the new set of accounts .. */
  more_splits = xaccTransCountSplits (trans);
  affected_accounts = (Account **) realloc (affected_accounts, 
                                            (more_splits+num_splits+1) *
                                            sizeof (Account *));

  for (i = 0; i < more_splits; i++)
  {
    split = xaccTransGetSplit (trans, i);
    affected_accounts[i+num_splits] = xaccSplitGetAccount (split);
  }
  affected_accounts[num_splits+more_splits] = NULL;
   
  xaccAccListDisplayRefresh (affected_accounts);
  free (affected_accounts);

  info->pending_trans = NULL;

  gnc_refresh_main_window ();
}

/* ======================================================== */

void 
xaccSRRedrawRegEntry (SplitRegister *reg) 
{
   Split *split;
   Transaction *trans;
   unsigned int changed;

   /* use the changed flag to avoid heavy-weight redraws
    * This will help cut down on uneccessary register redraws.  */
   changed = xaccSplitRegisterGetChangeFlag (reg);
   if (!changed) return;

   split = xaccSRGetCurrentSplit (reg);
   trans = xaccSplitGetParent (split);

   /* refresh the register windows */
   /* This split belongs to a transaction that might be displayed
    * in any number of windows.  Changing any one split is likely
    * to affect any account windows associated with the other splits
    * in this transaction.  So basically, send redraw events to all
    * of the splits.
    */
   gnc_transaction_ui_refresh(trans);
   gnc_refresh_main_window();
}

/* ======================================================== */
/* Copy from the register object to the engine */

gncBoolean
xaccSRSaveRegEntry (SplitRegister *reg, Transaction *newtrans)
{
   SRInfo *info = xaccSRGetInfo(reg);
   Split *split;
   Transaction *trans;
   unsigned int changed;
   int style;

   /* use the changed flag to avoid heavy-weight updates
    * of the split & transaction fields. This will help
    * cut down on uneccessary register redraws.  */
   changed = xaccSplitRegisterGetChangeFlag (reg);
   if (!changed) return GNC_F;

   style = (reg->type) & REG_STYLE_MASK;   

   /* get the handle to the current split and transaction */
   split = xaccSRGetCurrentSplit (reg);
   trans = xaccSRGetCurrentTrans (reg);
   if (trans == NULL)
     return GNC_F;

   ENTER ("xaccSRSaveRegEntry(): save split is %p \n", split);

   /* determine whether we should commit the pending transaction */
   if (info->pending_trans != trans) {
     if (xaccTransIsOpen (info->pending_trans))
       xaccTransCommitEdit (info->pending_trans);
     xaccTransBeginEdit (trans, 0);
     info->pending_trans = trans;
   }

   /* If we are committing the blank split, add it to the account now */
   if (xaccTransGetSplit(trans, 0) == info->blank_split)
     xaccAccountInsertSplit (info->default_source_account, info->blank_split);

   if (split == NULL) {
      /* If we were asked to save data for a row for which there is no
       * associated split, then assume that this was a row that was
       * set aside for adding splits to an existing transaction.
       * xaccSRGetCurrent will handle this case, too. We will create
       * a new split, copy the row contents to that split, and append
       * the split to the pre-existing transaction. */

      split = xaccMallocSplit ();
      xaccTransAppendSplit (trans, split);

      if (force_double_entry_awareness)
        xaccAccountInsertSplit (info->default_source_account, split);

      assert (reg->table->current_cursor);
      reg->table->current_cursor->user_data = (void *) split;
   }

   DEBUG ("xaccSRSaveRegEntry(): updating trans addr=%p\n", trans);

   /* copy the contents from the cursor to the split */
   if (MOD_DATE & changed) {
      /* commit any pending changes */
      xaccCommitDateCell (reg->dateCell);
      DEBUG ("xaccSRSaveRegEntry(): MOD_DATE DMY= %2d/%2d/%4d \n",
                               reg->dateCell->date.tm_mday,
                               reg->dateCell->date.tm_mon+1,
                               reg->dateCell->date.tm_year+1900);

      xaccTransSetDate (trans, reg->dateCell->date.tm_mday,
                               reg->dateCell->date.tm_mon+1,
                               reg->dateCell->date.tm_year+1900);
   }

   if (MOD_NUM & changed) {
      DEBUG ("xaccSRSaveRegEntry(): MOD_NUM: %s\n", reg->numCell->value);
      xaccTransSetNum (trans, reg->numCell->value);
   }
   
   if (MOD_DESC & changed) {
      DEBUG ("xaccSRSaveRegEntry(): MOD_DESC: %s\n", reg->descCell->cell.value);
      xaccTransSetDescription (trans, reg->descCell->cell.value);
   }

   if (MOD_RECN & changed) {
      DEBUG ("xaccSRSaveRegEntry(): MOD_RECN: %c\n", reg->recnCell->value[0]);
      xaccSplitSetReconcile (split, reg->recnCell->value[0]);
   }

   if (MOD_ACTN & changed) {
      DEBUG ("xaccSRSaveRegEntry(): MOD_ACTN: %s\n", reg->actionCell->cell.value);
      xaccSplitSetAction (split, reg->actionCell->cell.value);
   }

   if (MOD_MEMO & changed) {
      DEBUG ("xaccSRSaveRegEntry(): MOD_MEMO: %s\n", reg->memoCell->value);
      xaccSplitSetMemo (split, reg->memoCell->value);
   }

   /* -------------------------------------------------------------- */
   /* OK, the handling of transfers gets complicated because it 
    * depends on what was displayed to the user.  For a multi-line
    * display, we just reparent the indicated split, its it,
    * and that's that.  For a two-line display, we want to reparent
    * the "other" split, but only if there is one ...
    * XFRM is the straight split, MXFRM is the mirrored split.
    */
   if (MOD_XFRM & changed) {
      Account *old_acc=NULL, *new_acc=NULL;
      DEBUG ("xaccSRSaveRegEntry(): MOD_XFRM: %s\n", reg->xfrmCell->cell.value);

      /* do some reparenting. Insertion into new account will automatically
       * delete this split from the old account */
      old_acc = xaccSplitGetAccount (split);
      new_acc = xaccGetAccountByName (trans, reg->xfrmCell->cell.value);
      xaccAccountInsertSplit (new_acc, split);
   
      /* make sure any open windows of the old account get redrawn */
      gnc_account_ui_refresh(old_acc);
      gnc_refresh_main_window();
   }

   if (MOD_MXFRM & changed) {
      Split *other_split = NULL;
      DEBUG ("xaccSRSaveRegEntry(): MOD_MXFRM: %s\n", reg->mxfrmCell->cell.value);

      other_split = xaccGetOtherSplit(split);

      /* other_split may be null for two very different reasons:
       * (1) the parent transaction has three or more splits in it,
       *     and so the "other" split is ambiguous, and thus null.
       * (2) the parent transaction has only this one split as a child.
       *     and "other" is null because there is no other.
       *
       * In the case (2), we want to create the other split, so that 
       * the user's request to transfer actually works out.
       */

      if (!other_split) {
         other_split = xaccTransGetSplit (trans, 1);
         if (!other_split) {
            double  amt = xaccSplitGetShareAmount (split);
            double  prc = xaccSplitGetSharePrice (split);
            other_split = xaccMallocSplit ();
            xaccSplitSetMemo (other_split, xaccSplitGetMemo (split));
            xaccSplitSetAction (other_split, xaccSplitGetAction (split));
            xaccSplitSetSharePriceAndAmount (other_split, prc, -amt);
            xaccTransAppendSplit (trans, other_split);
         }
      }

      if (other_split) {
         Account *old_acc=NULL, *new_acc=NULL;

         /* do some reparenting. Insertion into new account will automatically
          * delete from the old account */
         old_acc = xaccSplitGetAccount (other_split);
         new_acc = xaccGetAccountByName (trans, reg->mxfrmCell->cell.value);
         xaccAccountInsertSplit (new_acc, other_split);
   
         /* make sure any open windows of the old account get redrawn */
         gnc_account_ui_refresh(old_acc);
         gnc_refresh_main_window();
      }
   }

   if (MOD_XTO & changed) {
      /* hack alert -- implement this */
   }

   /* The AMNT and NAMNT updates only differ by sign.  Basically, 
    * the split and transaction cursors show minus the quants that
    * the single and double cursors show, and so when updates happen,
    * the extra minus sign must also be handled.
    */
   if ((MOD_AMNT | MOD_NAMNT) & changed) {
      double new_amount;
      if (MOD_AMNT & changed) {
         new_amount = (reg->creditCell->amount) - (reg->debitCell->amount);
      } else {
         new_amount = (reg->ndebitCell->amount) - (reg->ncreditCell->amount);
      }
      DEBUG ("xaccSRSaveRegEntry(): MOD_AMNT: %f\n", new_amount);
      if ((EQUITY_REGISTER   == (reg->type & REG_TYPE_MASK)) ||
          (STOCK_REGISTER    == (reg->type & REG_TYPE_MASK)) ||
          (CURRENCY_REGISTER == (reg->type & REG_TYPE_MASK)) ||
          (PORTFOLIO         == (reg->type & REG_TYPE_MASK))) 
      { 
         xaccSplitSetShareAmount (split, new_amount);
      } else {
         xaccSplitSetValue (split, new_amount);
      }
   }

   if (MOD_PRIC & changed) {
      Account *acc;
      int n;
      DEBUG ("xaccSRSaveRegEntry(): MOD_PRIC: %f\n", reg->priceCell->amount);
      xaccSplitSetSharePrice (split, reg->priceCell->amount);

      /* Here we handle a very special case: the user just created 
       * an account, which now has two splits in it, and the user 
       * is editing the opening balance split.  Then copy the price
       * over to the last split, so that the account balance, when
       * computed, won't be obviously bad.  Strictly speaking, everything
       * will automatically fix itself once the user closes the window,
       * or if they start editing the second split, and so we don't
       * really have to do this.  This is more of a feel-good thing,
       * so that they won't see even breifly what looks like bad values, 
       * and that might give them the willies.  We want them to feel good.
       */
      acc = xaccSplitGetAccount (split);
      n = xaccAccountGetNumSplits (acc);
      if (2 == n) {
         Split *s = xaccAccountGetSplit (acc, 0);
         if (s == split) {
            Transaction *t;
            double currprice;
            s = xaccAccountGetSplit (acc, 1);
            currprice = xaccSplitGetSharePrice (s);
            if (DEQ (currprice, 1.0)) {
               t = xaccSplitGetParent (s);
               xaccTransBeginEdit (t, 0);
               xaccSplitSetSharePrice (s, reg->priceCell->amount);
               xaccTransCommitEdit (t);
            }
         }
      }
   }

   if (MOD_VALU & changed) {
      DEBUG ("xaccSRSaveRegEntry(): MOD_VALU: %f\n", reg->valueCell->amount);
      xaccSplitSetValue (split, (reg->valueCell->amount));
   }

   PINFO ("xaccSRSaveRegEntry(): finished saving split %s of trans %s \n", 
      xaccSplitGetMemo(split), xaccTransGetDescription(trans));

   /* If the modified split is the "blank split", then it is now an
    * official part of the account. Set the blank split to NULL, so
    * we can be sure of getting a new split.  */
   split = xaccTransGetSplit (trans, 0);
   if (split == info->blank_split)
     info->blank_split = NULL;

   /* If the new transaction is different from the current,
    * commit the current and set the pending transaction to NULL. */
   if (trans != newtrans) {
     xaccTransCommitEdit (trans);
     info->pending_trans = NULL;
   }

   return GNC_T;
}

/* ======================================================== */

static void
xaccSRLoadTransEntry (SplitRegister *reg, Split *split, int do_commit)
{
   char buff[2];
   double baln;
   int typo = reg->type & REG_TYPE_MASK;
   /* int style = reg->type & REG_STYLE_MASK; */

   /* don't even bother doing a load if there is no current cursor */
   if (!(reg->table->current_cursor)) return;

   ENTER ("SRLoadTransEntry(): s=%p commit=%d\n", split, do_commit);
   if (!split) {
      /* we interpret a NULL split as a blank split */
      xaccSetDateCellValueSecs (reg->dateCell, 0);
      xaccSetBasicCellValue (reg->numCell, "");
      xaccSetQuickFillCellValue (reg->descCell, "");
      xaccSetBasicCellValue (reg->recnCell, "");
      xaccSetPriceCellValue  (reg->shrsCell,  0.0);
      xaccSetPriceCellValue (reg->balanceCell, 0.0);

      xaccSetComboCellValue (reg->actionCell, "");
      xaccSetBasicCellValue (reg->memoCell, "");
      xaccSetComboCellValue (reg->xfrmCell, "");
      xaccSetComboCellValue (reg->mxfrmCell, "");
      xaccSetDebCredCellValue (reg->debitCell, 
                               reg->creditCell, 0.0);
      xaccSetDebCredCellValue (reg->ndebitCell, 
                               reg->ncreditCell, 0.0);
      xaccSetPriceCellValue (reg->priceCell, 0.0);
      xaccSetPriceCellValue (reg->valueCell, 0.0);

   } else {
      long long secs;
      double amt;
      char * accname=NULL;
      Transaction *trans = xaccSplitGetParent (split);
   
      secs = xaccTransGetDateL (trans);
      xaccSetDateCellValueSecsL (reg->dateCell, secs);
   
      xaccSetBasicCellValue (reg->numCell, xaccTransGetNum (trans));
      xaccSetQuickFillCellValue (reg->descCell, xaccTransGetDescription (trans));
   
      buff[0] = xaccSplitGetReconcile (split);
      buff[1] = 0x0;
      xaccSetBasicCellValue (reg->recnCell, buff);
   
      /* For income and expense acounts, we have to reverse
       * the meaning of balance, since, in a dual entry
       * system, income will show up as a credit to a
       * bank account, and a debit to the income account.
       * Thus, positive and negative are interchanged */
      baln = xaccSplitGetBalance (split);
      if ((INCOME_REGISTER == typo) ||
          (EXPENSE_REGISTER == typo)) { 
         baln = -baln;
      }
      xaccSetPriceCellValue (reg->balanceCell, baln);
   
      xaccSetPriceCellValue (reg->shrsCell,  xaccSplitGetShareBalance (split));

      xaccSetComboCellValue (reg->actionCell, xaccSplitGetAction (split));

      /* Show the transfer-from account name.                            
       * What gets displayed depends on the display format.                
       * For a multi-line display, show the account for each member split.  
       * For a one or two-line display, show the other account, but only    
       * if there are exactly two splits.                                   
       * xfrm is the "straight" display, "mxfrm" is the "mirrored" display.
       */
      accname = xaccAccountGetName (xaccSplitGetAccount (split));
      xaccSetComboCellValue (reg->xfrmCell, accname);
      {
         Split *s = xaccGetOtherSplit (split);
         if (s) {
            accname = xaccAccountGetName (xaccSplitGetAccount (s));
         } else {
            /* determine whether s is null because threre are three
             * or more splits, or whether there is only one ... */
            s = xaccTransGetSplit (xaccSplitGetParent(split), 1);
            if (s) {
               accname = SPLIT_STR;    /* three or more .. */
            } else {
               accname  = "";          /* none ... */
            }
         }
         xaccSetComboCellValue (reg->mxfrmCell, accname);
      }
   
      xaccSetBasicCellValue (reg->memoCell, xaccSplitGetMemo (split));
   
      buff[0] = xaccSplitGetReconcile (split);
      buff[1] = 0x0;
      xaccSetBasicCellValue (reg->recnCell, buff);
   
      if ((EQUITY_REGISTER   == typo) ||
          (STOCK_REGISTER    == typo) ||
          (CURRENCY_REGISTER == typo) ||
          (PORTFOLIO         == typo)) 
      { 
         amt = xaccSplitGetShareAmount (split);
      } else {
         amt = xaccSplitGetValue (split);
      }
      xaccSetDebCredCellValue (reg->debitCell, reg->creditCell, amt);
      xaccSetDebCredCellValue (reg->ndebitCell, reg->ncreditCell, -amt);
      xaccSetPriceCellValue (reg->priceCell, xaccSplitGetSharePrice (split));
      xaccSetPriceCellValue (reg->valueCell, xaccSplitGetValue (split));
   }

   reg->table->current_cursor->user_data = (void *) split;

   /* copy cursor contents into the table */
   if (do_commit) {
      xaccCommitCursor (reg->table);
   }
   LEAVE("SRLoadTransEntry():\n");
}

/* ======================================================== */

#define xaccSRLoadSplitEntry  xaccSRLoadTransEntry

#ifdef LATER
static void
xaccSRLoadSplitEntry (SplitRegister *reg, Split *split, int do_commit)
{
   char buff[2];

   if (!split) {
   } else {
   }

   reg->table->current_cursor->user_data = (void *) split;

   /* copy cursor contents into the table */
   if (do_commit) {
      xaccCommitCursor (reg->table);
   }
}
#endif 

/* ======================================================== */

void
xaccSRLoadRegEntry (SplitRegister *reg, Split *split)
{
   xaccSRLoadTransEntry (reg, split, 0);

   /* copy cursor contents into the table */
   xaccCommitCursor (reg->table);
}

/* ======================================================== */

void
xaccSRCountRows (SplitRegister *reg, Split **slist, 
                 Account *default_source_acc)
{
   SRInfo *info = xaccSRGetInfo(reg);
   int i;
   Split *split = NULL;
   Split *save_current_split = NULL;
   Transaction *save_current_trans = NULL;
   int save_cursor_phys_row = -1;
   int save_cursor_virt_row = -1;
   Table *table;
   int num_phys_rows;
   int num_virt_rows;
   int style;
   int multi_line, dynamic;
   CellBlock *lead_cursor;
   gncBoolean found_split = GNC_F;

   table = reg->table;
   style = (reg->type) & REG_STYLE_MASK;
   multi_line  = (REG_MULTI_LINE == style);
   dynamic = ((REG_SINGLE_DYNAMIC == style) || (REG_DOUBLE_DYNAMIC == style));
   if ((REG_SINGLE_LINE == style) ||
       (REG_SINGLE_DYNAMIC == style)) {
      lead_cursor = reg->single_cursor;
   } else {
      lead_cursor = reg->double_cursor;
   }

   /* save the current cursor location; we do this by saving
    * a pointer to the currently edited split; we restore the 
    * cursor to this location when we are done. */
   save_current_split = xaccSRGetCurrentSplit (reg);
   save_current_trans = xaccSRGetCurrentTrans (reg);
   save_cursor_phys_row = reg->cursor_phys_row;
   save_cursor_virt_row = reg->cursor_virt_row;

   /* num_phys_rows is the number of rows in all the cursors.
    * num_virt_rows is the number of cursors (including the header).
    * Count the number of rows needed.  
    * the phys row count will be equal to 
    * +1   for the header
    * +n   that is, one (transaction) row for each split passed in,
    * +n   one blank edit row for each transaction
    * +p   where p is the sum total of all the splits in the transaction
    * +2   an editable transaction and split at the end.
    */
   num_phys_rows = reg->header->numRows;
   num_virt_rows = 1;

   i=0;
   if (slist) {
      split = slist[0]; 
   } else {
      split = NULL;
   }
   while (split) {
      /* do not count the blank split */
      if (split != info->blank_split) {
         Transaction *trans;
         int do_expand = 0;

         /* lets determine where to locate the cursor ... */
         if (!found_split) {
           /* Check to see if we find a perfect match */
           if (split == save_current_split) {
             save_cursor_phys_row = num_phys_rows;
             save_cursor_virt_row = num_virt_rows;
             found_split = GNC_T;
           }
           /* Otherwise, check for a close match. This could happen if
              we are collapsing from multi-line to single, e.g. */
           else if (xaccSplitGetParent(split) == save_current_trans) {
             save_cursor_phys_row = num_phys_rows;
             save_cursor_virt_row = num_virt_rows;
           }             
         }

         /* if multi-line, then show all splits.  If dynamic then
          * show all splits only if this is the hot split. 
          */
         do_expand = multi_line;
         do_expand = do_expand || 
                     (dynamic && xaccIsPeerSplit(split,save_current_split)); 
         if (NULL == save_current_split) {
            trans = xaccSplitGetParent (split);
            do_expand = do_expand || (trans == info->cursor_hint_trans);
         }

         if (do_expand) 
         {
            Split * secondary;
            int j = 0;

            /* add one row for a transaction */
            num_virt_rows ++;
            num_phys_rows += reg->trans_cursor->numRows; 

            /* Add a row for each split, minus one, plus one.
             * Essentially, do the following:
             * j = xaccTransCountSplits (trans);
             * num_virt_rows += j;
             * num_phys_rows += j * reg->split_cursor->numRows; 
             * except that we also have to find teh saved cursor row,
             * Thus, we need a real looop over the splits.
             * The do..while will automaticaly put a blank (null) 
             * split at the end 
             */
            trans = xaccSplitGetParent (split);
            j = 0;
            do {
               secondary = xaccTransGetSplit (trans, j);
               if (secondary != split) {

                 /* lets determine where to locate the cursor ... */
                 if (!found_split) {
                   /* Check to see if we find a perfect match. We have to
                    * check the transaction in case the split is NULL (blank).
                    */
                   if ((secondary == save_current_split) &&
                       (trans == save_current_trans)) {
                     save_cursor_phys_row = num_phys_rows;
                     save_cursor_virt_row = num_virt_rows;
                     found_split = GNC_T;
                   }
                 }

                 num_virt_rows ++;
                 num_phys_rows += reg->split_cursor->numRows; 
               }
               j++;
            } while (secondary);
         } else {

            /* the simple case ... add one row for a transaction */
            num_virt_rows ++;
            num_phys_rows += lead_cursor->numRows; 
         }
      }
      i++;
      split = slist[i];
   }

   /* ---------------------------------------------------------- */
   /* the "blank split", if it exists, is at the end */
   if (info->blank_split != NULL) {
      /* lets determine where to locate the cursor ... */
      if (!found_split && info->blank_split == save_current_split) {
         save_cursor_phys_row = num_phys_rows;
         save_cursor_virt_row = num_virt_rows;
         found_split = GNC_T;
      }
   }

   if (multi_line) {
      num_virt_rows += 2; 
      num_phys_rows += reg->trans_cursor->numRows;
      num_phys_rows += reg->split_cursor->numRows;
   } else {
      num_virt_rows += 1; 
      num_phys_rows += lead_cursor->numRows;
   }

   /* check to make sure we got a good cursor position */
   if ((num_phys_rows <= save_cursor_phys_row) ||
       (num_virt_rows <= save_cursor_virt_row)) 
   {
       save_cursor_phys_row = num_phys_rows - reg->split_cursor->numRows;
       save_cursor_virt_row = num_virt_rows - 1;
   }
   if ((save_cursor_phys_row < (reg->header->numRows)) ||
       (save_cursor_virt_row < 1))
   {
      save_cursor_phys_row = reg->header->numRows;
      save_cursor_virt_row = 1;
   }

   /* finally, record the values */
   reg->num_phys_rows = num_phys_rows;
   reg->num_virt_rows = num_virt_rows;
   reg->cursor_phys_row = save_cursor_phys_row;
   reg->cursor_virt_row = save_cursor_virt_row;
}

/* ======================================================== */

void
xaccSRLoadRegister (SplitRegister *reg, Split **slist, 
                    Account *default_source_acc)
{
   SRInfo *info = xaccSRGetInfo(reg);
   int i = 0;
   Split *split=NULL, *last_split=NULL;
   Split *save_current_split=NULL;
   Table *table;
   int phys_row;
   int save_phys_col;
   int vrow;
   int type, style;
   int multi_line, dynamic;
   CellBlock *lead_cursor;
   gncBoolean found_pending = GNC_F;

   info->default_source_account = default_source_acc;

   table = reg->table;
   type  = (reg->type) & REG_TYPE_MASK;
   style = (reg->type) & REG_STYLE_MASK;
   multi_line  = (REG_MULTI_LINE == style);
   dynamic = ((REG_SINGLE_DYNAMIC == style) || (REG_DOUBLE_DYNAMIC == style));
   if ((REG_SINGLE_LINE == style) ||
       (REG_SINGLE_DYNAMIC == style)) {
      lead_cursor = reg->single_cursor;
   } else {
      lead_cursor = reg->double_cursor;
   }

   /* count the number of rows */
   xaccSRCountRows (reg, slist, default_source_acc);

   /* save the current cursor location; we do this by saving a pointer
    * to the currently edited split and physical column; we restore
    * the cursor to this location when we are done. */
   save_current_split = xaccSRGetCurrentSplit (reg);
   save_phys_col = table->current_cursor_phys_col;

   /* disable move callback -- we con't want the cascade of 
    * callbacks while we are fiddling with loading the register */
   table->move_cursor = NULL;
   xaccMoveCursorGUI (table, -1, -1);

   /* resize the table to the sizes we just counted above */
   /* num_virt_cols is always one. */
   xaccSetTableSize (table, reg->num_phys_rows, reg->num_cols, 
                            reg->num_virt_rows, 1);

   /* make sure that the header is loaded */
   xaccSetCursor (table, reg->header, 0, 0, 0, 0);

   PINFO ("xaccSRLoadRegister(): "
          "load register of %d phys rows ----------- \n", reg->num_phys_rows);

   /* populate the table */
   i=0;
   vrow = 1;   /* header is vrow zero */
   phys_row = reg->header->numRows;
   if (slist) {
      split = slist[0]; 
   } else {
      split = NULL;
   }
   while (split) {

     if (info->pending_trans == xaccSplitGetParent (split))
       found_pending = GNC_T;

      /* do not load the blank split */
      if (split != info->blank_split) {
         Transaction *trans;
         int do_expand;

         PINFO ("xaccSRLoadRegister(): "
                "load trans %d at phys row %d \n", i, phys_row);

         /* if multi-line, then show all splits.  If dynamic then
          * show all splits only if this is the hot split. 
          */
         do_expand = multi_line;
         do_expand = do_expand || 
                     (dynamic && xaccIsPeerSplit(split,save_current_split)); 
         if (NULL == save_current_split) {
            trans = xaccSplitGetParent (split);
            do_expand = do_expand || (trans == info->cursor_hint_trans);
         }

         if (do_expand) 
         {
            Split * secondary;
            int j = 0;

            xaccSetCursor (table, reg->trans_cursor, phys_row, 0, vrow, 0);
            xaccMoveCursor (table, phys_row, 0);
            xaccSRLoadTransEntry (reg, split, 1);
            vrow ++;
            phys_row += reg->trans_cursor->numRows; 

            /* loop over all of the splits in the transaction */
            /* the do..while will automaticaly put a blank (null) split at the end */
            trans = xaccSplitGetParent (split);
            j = 0;
            do {
               secondary = xaccTransGetSplit (trans, j);

               if (secondary != split) {
                  xaccSetCursor (table, reg->split_cursor, phys_row, 0, vrow, 0);
                  xaccMoveCursor (table, phys_row, 0);
                  xaccSRLoadSplitEntry (reg, secondary, 1);
                  PINFO ("xaccSRLoadRegister(): "
                         "load split %d at phys row %d addr=%p \n", 
                          j, phys_row, secondary);
                  vrow ++;
                  phys_row += reg->split_cursor->numRows; 
               }

               j++;
            } while (secondary);

         } else {
            /* the simple case ... */
            xaccSetCursor (table, lead_cursor, phys_row, 0, vrow, 0);
            xaccMoveCursor (table, phys_row, 0);
            xaccSRLoadTransEntry (reg, split, 1);
            vrow ++;
            phys_row += lead_cursor->numRows; 
         }
      } else {
         PINFO ("xaccSRLoadRegister(): "
                "skip trans %d (blank split) \n", i);
      }
   

      last_split = split;
      i++; 
      split = slist[i];
   }

   /* add the "blank split" at the end.  We use either the blank
    * split or we create a new one, as needed. */
   if (info->blank_split != NULL) {
      split = info->blank_split;
      if (info->pending_trans == xaccSplitGetParent(split))
        found_pending = GNC_T;
   } else {
      Transaction *trans;
      double last_price = 0.0;

      trans = xaccMallocTransaction ();
      xaccTransBeginEdit (trans, 1);
      xaccTransSetDateToday (trans);
      xaccTransCommitEdit (trans);
      split = xaccTransGetSplit (trans, 0);
      info->blank_split = split;
      reg->destroy = LedgerDestroy;

      /* kind of a cheesy hack to get the price on the last split right
       * when doing stock accounts.   This will guess incorrectly for a 
       * ledger showing multiple stocks, but seems cool for a single stock.
       */
      if ((STOCK_REGISTER == type) ||
          (PORTFOLIO      == type)) 
      {
         last_price = xaccSplitGetSharePrice (last_split);
         xaccSplitSetSharePrice (split, last_price);
      }
   }

   /* do the split row of the blank split */
   if (multi_line) {
      Transaction *trans;

      /* do the transaction row of the blank split */
      xaccSetCursor (table, reg->trans_cursor, phys_row, 0, vrow, 0);
      xaccMoveCursor (table, phys_row, 0);
      xaccSRLoadTransEntry (reg, split, 1);
      vrow ++;
      phys_row += reg->trans_cursor->numRows; 
   
      trans = xaccSplitGetParent (split);
      split = xaccTransGetSplit (trans, 1);
      xaccSetCursor (table, reg->split_cursor, phys_row, 0, vrow, 0);
      xaccMoveCursor (table, phys_row, 0);
      xaccSRLoadSplitEntry (reg, split, 1);
      vrow ++;
      phys_row += reg->split_cursor->numRows; 
   } else {
      xaccSetCursor (table, lead_cursor, phys_row, 0, vrow, 0);
      xaccMoveCursor (table, phys_row, 0);
      xaccSRLoadTransEntry (reg, split, 1);
      vrow ++;
      phys_row += lead_cursor->numRows; 
   }

   /* restore the cursor to its rightful position */
   i = 0;
   while ((save_phys_col + i < reg->num_cols) || (save_phys_col > 0)) {
     if (gnc_register_cell_valid(table, reg->cursor_phys_row,
                                 save_phys_col + i)) {
       xaccMoveCursorGUI (table, reg->cursor_phys_row, save_phys_col + i);
       break;
     }
     if (gnc_register_cell_valid(table, reg->cursor_phys_row,
                                 save_phys_col - i)) {
       xaccMoveCursorGUI (table, reg->cursor_phys_row, save_phys_col - i);
       break;
     }
     i++;
   }

   /* If we didn't find the pending transaction, it was removed
    * from the account. It might not even exist any more.
    * Make sure we don't access it. */
   if (!found_pending)
     info->pending_trans = NULL;

   xaccRefreshTableGUI (table);

   /* enable callback for cursor user-driven moves */
   table->move_cursor = LedgerMoveCursor;
   table->traverse = LedgerTraverse;
   table->client_data = (void *) reg;
}

/* ======================================================== */
/* walk account tree recursively, pulling out all the names */

static void 
LoadXferCell (ComboCell *cell,  
              AccountGroup *grp,
              char *base_currency, char *base_security)
{
   Account * acc;
   int n;

   ENTER ("LoadXferCell(): curr=%s secu=%s\n", base_currency, base_security);

   if (!grp) return;

   /* Build the xfer menu out of account names.
    * Traverse sub-accounts recursively.
    * Valid transfers can occur only between accounts
    * with the same base currency.
    */
   n = 0;
   acc = xaccGroupGetAccount (grp, n);
   while (acc) {
      char *curr, *secu;

      curr = xaccAccountGetCurrency (acc);
      secu = xaccAccountGetSecurity (acc);
      if (secu && (0x0 == secu[0])) secu = 0x0;

      DEBUG ("LoadXferCell(): curr=%s secu=%s acct=%s\n", 
        curr, secu, xaccAccountGetName (acc));
      if ( (!safe_strcmp(curr,base_currency)) ||
           (!safe_strcmp(curr,base_security)) ||
           (secu && (!safe_strcmp(secu,base_currency))) ||
           (secu && (!safe_strcmp(secu,base_security))) )
      {
         xaccAddComboCellMenuItem (cell, xaccAccountGetName (acc));
      }
      LoadXferCell (cell, xaccAccountGetChildren (acc), 
                   base_currency, base_security);
      n++;
      acc = xaccGroupGetAccount (grp, n);
   }
   LEAVE ("LoadXferCell()\n");
}

/* ======================================================== */

static void
xaccLoadXferCell (ComboCell *cell,  
                  AccountGroup *grp, 
                  Account *base_account)
{
   char *curr, *secu;

   curr = xaccAccountGetCurrency (base_account);
   secu = xaccAccountGetSecurity (base_account);
   if (secu && (0x0 == secu[0])) secu = 0x0;

   xaccClearComboCellMenu (cell);
   xaccAddComboCellMenuItem (cell, "");
   LoadXferCell (cell, grp, curr, secu);
}

/* ======================================================== */

void
xaccSRLoadXferCells (SplitRegister *reg, Account *base_account)
{
  AccountGroup *group;

  group = xaccGetAccountRoot(base_account);

  assert((group != NULL) && (base_account != NULL));

  xaccLoadXferCell(reg->xfrmCell, group, base_account);
  xaccLoadXferCell(reg->mxfrmCell, group, base_account);
}

/* =======================  end of file =================== */
