/* 
 * FILE:
 * Ledger.c 
 *
 * FUNCTION:
 * copy transaction data into ledger
 */

#include "Ledger.h"
#include "register.h"
#include "Transaction.h"

#define BUFSIZE 1024

void
xaccLoadRegister (BasicRegister *reg, Split **slist)
{
   int i;
   Split *split;
   Transaction *trans;
   char buff[BUFSIZE];

   i=0;
   split = slist[0]; 
   while (split) {

      trans = (Transaction *) (split->parent);

      xaccMoveCursor (reg->table, i, 0);

      sprintf (buff, "%2d/%2d/%4d", trans->date.day, 
                                    trans->date.month,
                                    trans->date.year);

      xaccSetBasicCellValue (reg->dateCell, buff);

      xaccSetBasicCellValue (reg->numCell, trans->num);
      xaccSetBasicCellValue (&(reg->actionCell->cell), split->action);
      xaccSetQuickFillCellValue (reg->descCell, trans->description);
      xaccSetBasicCellValue (reg->memoCell, split->memo);

      buff[0] = split->reconciled;
      buff[1] = 0x0;
      xaccSetBasicCellValue (reg->recnCell, buff);

      xaccSetDebCredCellValue (reg->debitCell, 
                               reg->creditCell, split->damount);

      xaccSetAmountCellValue (reg->balanceCell, split->balance);
      xaccCommitEdits (reg->table);

      i++;
      split = slist[i];
   }
   xaccRefreshTable (reg->table);
}

/* ======================================================== */

void xaccLoadXferCell (ComboCell *cell,  AccountGroup *grp)
{
   Account * acc;
   int n;

   if (!grp) return;

   /* build the xfer menu out of account names */
   /* traverse sub-accounts ecursively */
   n = 0;
   acc = getAccount (grp, n);
   while (acc) {
      xaccAddComboCellMenuItem (cell, acc->accountName);
      xaccLoadXferCell (cell, acc->children);
      n++;
      acc = getAccount (grp, n);
   }
}

/* =======================  end of file =================== */
