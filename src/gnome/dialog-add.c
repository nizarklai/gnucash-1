/********************************************************************\
 * AccWindow.c -- window for creating new accounts for xacc         *
 *                (X-Accountant)                                    *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1997, 1998, 1999 Linas Vepstas                     *
 * Copyright (C) 1999 Jeremy Collins                                *
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

/* TODO: 
 * -- tooltips for the widgets in the window
 * -- history boxes (i'm not sure they are called this way) for the text 
 *    input boxes.
 * -- the user should be able to operate the window with the keybard only
 * -- the notes box should have scrollbar if the text excedes the visible area
 *    (a scrolled window does not do the trick :)
 * -- the parent account widget should show up *always* with the selected account in the 
 *    visible area. (if there are lots of accounts the selected account is offscreen)
 * -- make the account code work.
 */

#include <gnome.h>
#include <stdio.h>
#include <ctype.h>

#include "config.h"

#include "AccWindow.h"
#include "AccInfo.h"
#include "Account.h"
#include "top-level.h"
#include "gnucash.h"
#include "MainWindow.h"
#include "messages.h"
#include "util.h"
#include "FileDialog.h" /* for gncGetCurrentGroup, does this belong there?*/

/* This static indicates the debugging module that this .o belongs to.  */
static short module = MOD_GUI;

/* Please look at ../motif/AccWindow.c for info on what should be
   going on in these functions */

struct _accwindow
{
  GnomeDialog 	*dialog;
  
  Account	*parentAccount;
  Account       *newAccount;
  gint		 type;
};

static int _accWindow_last_used_account_type = BANK;

static GtkWidget*
gnc_ui_get_widget (GtkWidget *widget, gchar *widget_name)
{
  GtkWidget *found_widget;
  
  if (widget->parent)
    widget = gtk_widget_get_toplevel (widget);
  found_widget = (GtkWidget*) gtk_object_get_data (GTK_OBJECT (widget),
                                                   widget_name);
  if (!found_widget)
    g_warning ("Widget not found: %s", widget_name);
  return found_widget;
}


/********************************************************************\
 * Function: gnc_ui_accWindow_list_cb -  the callback for the list  *
 *           of account types window                                *
 *                                                                  *
 * Args:   list - the list widget, child - the selected item, data  *
 *         - specifies whether is a select or a deselect callback   *
 * Return: none                                                     *
 * Notes:  data==0 -- unselect callback, data == 1 select callback  *
 \********************************************************************/
static void 
gnc_ui_accWindow_list_cb (GtkWidget* list, GtkWidget *child, gpointer data)
{
  GtkWidget *toplevel, *entrySecurity;
  AccWindow *accData;
  
  toplevel      = gtk_widget_get_toplevel(GTK_WIDGET(list));
  entrySecurity = gtk_object_get_data(GTK_OBJECT(toplevel), "entrySecurity");
  accData       = gtk_object_get_data(GTK_OBJECT(toplevel), "accData");
  
  /* for some reason, when the list widget is destroyed, this callback
     is called once more to deselect an eventually selected item, by
     than accData is long gone... */
  if(!accData) return;
  
  if(0==(int)data) /* unselect */
  {
    accData->type = -1;
    gtk_widget_set_sensitive(GTK_WIDGET(entrySecurity), FALSE);
  }
  else if(1==(int)data) /* select */
  {
    accData->type = (int)gtk_object_get_data(GTK_OBJECT(child), "accType");
    _accWindow_last_used_account_type = accData->type;
    /* Set the Security field depending on what account type is selected */
    switch (accData->type)
    {
    case STOCK    : 
    case MUTUAL   :
    case CURRENCY : 
      gtk_widget_set_sensitive(GTK_WIDGET(entrySecurity), TRUE );
      break;
    default       : 
      gtk_widget_set_sensitive(GTK_WIDGET(entrySecurity), FALSE);
    }
  }
}

/********************************************************************\
 * Function: gnc_ui_accWindow_list_fill - fils the list of account  *
 *           types widget with the account types                    *
 *                                                                  * 
 * Args:   listOfTypes - the widget to be filled                    * 
 * Return: none                                                     *
\********************************************************************/
static void
gnc_ui_accWindow_list_fill ( GtkWidget *listOfTypes )
{
  gint       i;
  GtkWidget *list_item;
  /* one "dynamic" cast is enough! */
  GtkContainer *l = GTK_CONTAINER(listOfTypes);
  
  for (i=0; i<NUM_ACCOUNT_TYPES; i++) 
  {
    GtkWidget       *label;
    gchar           *acctype;
    
    acctype = xaccAccountGetTypeStr (i);
    
    list_item=gtk_list_item_new_with_label (acctype);
    gtk_container_add(l, list_item);
    gtk_object_set_data(GTK_OBJECT(list_item), "accType", (gpointer)i);
    gtk_widget_show(list_item);
  }
  /* the last selected account will become selected again */
  gtk_list_select_item(GTK_LIST(listOfTypes), 
                       _accWindow_last_used_account_type);
}

/********************************************************************\
 * Function: gnc_ui_accWindow_tree_select - the account selection   *
 *           callback                                               *
 *                                                                  * 
 * Args:   widget - the tree widget, child - the selected child,    *
 *         data - tells whether is a select or a deselect callack   * 
 * Return: none                                                     *
 * Notes: data == 0 unselect, data == 1 select                      *
 \********************************************************************/
static void
gnc_ui_accWindow_tree_select ( GtkWidget *widget, GtkWidget *child, 
                               gpointer data )
{
  GtkWidget   *toplevel;
  GtkWidget   *listOfTypes;
  GList       *children;
  GtkListItem *listItem;
  int        accountType, parentAccType;

  AccWindow   *accData;
	
  /* Find accData so we can set the account type */
  toplevel    = gtk_widget_get_toplevel(GTK_WIDGET(widget));
  listOfTypes = gtk_object_get_data(GTK_OBJECT(toplevel), "listOfTypes");
  accData     = gtk_object_get_data(GTK_OBJECT(toplevel), "accData");  
  
  children = GTK_LIST(listOfTypes)->children;
  
  if((int)data==0) /* deselect */
  {
    accData->parentAccount = NULL;
    /* sel all list widgets to be sensitive */
    while(children)
    {
      listItem = children->data;
      gtk_widget_set_sensitive(GTK_WIDGET(listItem), TRUE);
      children = children->next;
    }
  }
  else if((int)data==1)  /* select */
  {
    accData->parentAccount = (Account*)
      gtk_object_get_user_data(GTK_OBJECT(child));
    
    if(accData->parentAccount == NULL) /* new toplevel account */
    {
      /* validate all account types */
      while(children)
      {
        listItem = children->data;
        gtk_widget_set_sensitive(GTK_WIDGET(listItem), TRUE);
        children = children->next;
      }
      return;
    }

    parentAccType = xaccAccountGetType(accData->parentAccount);
    /* set the alowable account types for this parent, the relation
       is taken from xacc */
    while(children)
    {
      listItem    = children->data;
      accountType = (int)gtk_object_get_data(GTK_OBJECT(listItem), 
                                             "accType");
      switch(parentAccType) {
      case BANK:
      case CASH: 
      case ASSET:
      case STOCK:
      case MUTUAL:
      case CURRENCY:
        if((accountType == BANK)
           ||(accountType == CASH)
           ||(accountType == ASSET)
           ||(accountType == STOCK)
           ||(accountType == MUTUAL)
           ||(accountType == CURRENCY))
        {
          gtk_widget_set_sensitive(GTK_WIDGET(listItem), TRUE);
        }
        else
        {
          gtk_list_item_deselect(GTK_LIST_ITEM(listItem)); 
          gtk_widget_set_sensitive(GTK_WIDGET(listItem), FALSE); 
        }
        break;
      case CREDIT:
      case LIABILITY:
        if((accountType == CREDIT)
           ||(accountType == LIABILITY))
        {
          gtk_widget_set_sensitive(GTK_WIDGET(listItem), TRUE);
        }
        else
        {
          gtk_list_item_deselect(GTK_LIST_ITEM(listItem)); 
          gtk_widget_set_sensitive(GTK_WIDGET(listItem), FALSE); 
        }
        break;
      case INCOME:
        if(accountType == INCOME)
        {
          gtk_widget_set_sensitive(GTK_WIDGET(listItem), TRUE);
        }
        else
        {
          gtk_list_item_deselect(GTK_LIST_ITEM(listItem)); 
          gtk_widget_set_sensitive(GTK_WIDGET(listItem), FALSE); 
        }
        break;
      case EXPENSE:
        if(accountType == EXPENSE)
        {
          gtk_widget_set_sensitive(GTK_WIDGET(listItem), TRUE);
        }
        else
        {
          gtk_list_item_deselect(GTK_LIST_ITEM(listItem)); 
          gtk_widget_set_sensitive(GTK_WIDGET(listItem), FALSE); 
        }
        break;
      case EQUITY:
        if(accountType == EQUITY)
        {
          gtk_widget_set_sensitive(GTK_WIDGET(listItem), TRUE);
        }
        else
        {
          gtk_list_item_deselect(GTK_LIST_ITEM(listItem)); 
          gtk_widget_set_sensitive(GTK_WIDGET(listItem), FALSE); 
        }
        break;
      default:
        PERR("gnc_ui_accWindow_tree_select(): "
              "don't know how to handle %d account type!\n", parentAccType);
      }
      children = children->next;
    }
    
    /* now select a new account if the account class has changed */
    switch(parentAccType) {
    case BANK:
    case CASH: 
    case ASSET:
    case STOCK:
    case MUTUAL:
    case CURRENCY:
      if(!((accData->type == BANK)
           ||(accData->type == CASH)
           ||(accData->type == ASSET)
           ||(accData->type == STOCK)
           ||(accData->type == MUTUAL)
           ||(accData->type == CURRENCY)))
      {
        gtk_list_select_item(GTK_LIST(listOfTypes), parentAccType);
      }
      break;
    case CREDIT:
    case LIABILITY:
      if(!((accData->type == CREDIT)
           ||(accData->type == LIABILITY)))
      {
        gtk_list_select_item(GTK_LIST(listOfTypes), parentAccType);
      }
      break;
    case INCOME:
      if(!(accountType == INCOME))
      {
        gtk_list_select_item(GTK_LIST(listOfTypes), parentAccType);
      }
      break;
    case EXPENSE:
      if(!(accountType == EXPENSE))
      {
        gtk_list_select_item(GTK_LIST(listOfTypes), parentAccType);
      }
      break;
    case EQUITY:
      if(!(accountType == EQUITY))
      {
        gtk_list_select_item(GTK_LIST(listOfTypes), parentAccType);
      }
      break;
    default:
      PERR("gnc_ui_accWindow_tree_select(): "
           "don't what to select for %d account type!\n", parentAccType);
    }
  }
}

/*
 * these are used to select the coresponding child in the created
 * tree. they are needed because we can only select the item after the
 * whole tree is created. we've got a recursive call too, and passing the
 * same data in the recursive call does not make much sense.
 */
static GtkWidget *selected_tree = NULL;
static GtkWidget *selected_child = NULL;
static Account *selectedOne = NULL;

/********************************************************************\
 * Function: gnc_ui_accWindow_tree_fill- fills the tree widget      *
 *           with accounts                                          *
 *                                                                  *
 * Args:   tree      - top level of tree                            *
 *         accts     - the account group to use in filling tree     *
 *         selectedOne  - the account to be shown as selected       *
 * Returns: an integer used to expand the tree up to the selected   *
 *          account                                                 *
 \*******************************************************************/
static int
gnc_ui_accWindow_tree_fill (GtkTree *tree, AccountGroup *accts) 
{
  GtkTreeItem *treeItem;
  GtkWidget *subtree;
  gint       totalAccounts = xaccGroupGetNumAccounts(accts);
  gint       currentAccount;
  int       retVal, myRetVal = -2; 
  /*
    -2 -- don't expand
    -1 -- expand
    >-1 -- expend and select ith item
  */
  
  /* Add each account to the tree */  
  for ( currentAccount = 0; 
        currentAccount < totalAccounts; 
        currentAccount++ )
  {
    Account      *acc = xaccGroupGetAccount(accts, currentAccount);
    AccountGroup *hasChildren;

    if(acc == selectedOne)
      myRetVal = currentAccount;
    
    treeItem = (GtkTreeItem*)
      gtk_tree_item_new_with_label(xaccAccountGetName(acc));
    gtk_object_set_user_data(GTK_OBJECT(treeItem), acc);
    gtk_tree_append(tree, GTK_WIDGET(treeItem));
    
    /* Check to see if this account has any children. If it
     * does then we need to build a subtree and fill it 
     */
    hasChildren = xaccAccountGetChildren(acc); 
    
    gtk_widget_show(GTK_WIDGET(treeItem));
    
    if(hasChildren)
    {
      subtree = gtk_tree_new();
      gtk_signal_connect(GTK_OBJECT(subtree), "select-child",
                         GTK_SIGNAL_FUNC(gnc_ui_accWindow_tree_select),
                         (gpointer)1);
      gtk_signal_connect(GTK_OBJECT(subtree), "unselect-child",
                         GTK_SIGNAL_FUNC(gnc_ui_accWindow_tree_select),
                         (gpointer)0);
      retVal = gnc_ui_accWindow_tree_fill(GTK_TREE(subtree), hasChildren);  
      gtk_tree_item_set_subtree(treeItem, GTK_WIDGET(subtree));
    } 
    else {
      retVal = -2;
    }
    
    /* the gtk tutorial sais not to show the subtree !!! */
    /* gtk_widget_show(subtree); */
    
    if(retVal > -2) /* this subtree contains the selected node */
    {
      gtk_tree_item_expand(treeItem);
      myRetVal = -1;
    }

    if(acc == selectedOne)
    {
      selected_tree = GTK_WIDGET(tree);
      selected_child = GTK_WIDGET(treeItem);
    }
  }
  return myRetVal;
}

/********************************************************************\
 *                                                                  * 
 * Args:   parent   - the parent of the window to be created        * 
 * Return: none                                                     *
\********************************************************************/
static void
gnc_ui_accWindow_cancelled_callback( GtkWidget *ignore, gpointer data )
{
  AccWindow  *accData = (AccWindow *)data;

  if(accData->newAccount) xaccFreeAccount (accData->newAccount);  
  
  gnome_dialog_close( GNOME_DIALOG(accData->dialog));
  g_free(accData);
}

/********************************************************************\
 * make_valid_name                                                  *
 *   tests whether the name has at least a non-whitespace           *
 *   character in it, used to prevent the user from entring         *
 *   only whitespace names. it also trims all leading and           *
 *   trailing whitespace                                            *
 *                                                                  *
 * Note: other parts of the program could also benefit from this    *
 *       function                                                   *
 *                                                                  *
 * Args: name - a character string representing the name            *
 * Return: a character string representing the new name, allocated  *
 *         with malloc                                              *
 \*******************************************************************/
char *make_valid_name(const char *name)
{
  int l;
  const char *s = NULL, *p = name;
  char *r;

  while(*p)
    if(isspace(*p)) p++;
    else { s = p; break; }

  if(!s) return NULL;

  p = name + strlen(name) -1;
  while(p != name)
    if(!isspace(*p)) break;
    else p--;

  l = p - s + 1;
  r = (char *)malloc((l + 1)*sizeof(char));
  strncpy(r, s, l);
  r[l] = '\0';
  return r;
}


/********************************************************************\
 * gnc_ui_accWindow_create_callback                                 *
 *    This function does the actually creating of the new account.  *
 *    It also validates the data entered first.                     *
 *                                                                  * 
 * Args:   dialog - button that was clicked                         * 
 *         data   - should be NULL                                  *
 * Return: none                                                     *
\********************************************************************/
static void 
gnc_ui_accWindow_create_callback(GtkWidget * dialog, gpointer data)
{
  Account      *account;
  AccWindow    *accData = (AccWindow *)data; 

  GtkWidget *w;
  char *n;
  
  /* we only check once if dialog is a GTK_WIDGET */
  GTK_WIDGET(dialog);

  account = accData->newAccount;

  xaccAccountBeginEdit (account, 0); /* were is xaccAccountRollbackEdit ? */

  if (accData->parentAccount) {
    xaccInsertSubAccount (accData->parentAccount, account);
  } else {
    xaccGroupInsertAccount(gncGetCurrentGroup(), account );
  }

  { /* the account name */
    w = gnc_ui_get_widget(dialog, "entryAccountName");
    n = make_valid_name(gtk_entry_get_text(GTK_ENTRY(w)));
    if(n)
      xaccAccountSetName(account, n);
    else
    {
      GtkWidget *msgbox;
      msgbox = gnome_message_box_new ( "You must enter a valid name for the account",
                                       GNOME_MESSAGE_BOX_ERROR, "Ok", NULL );
      gtk_window_set_modal(GTK_WINDOW(msgbox) ,TRUE );
      gtk_widget_show ( msgbox );
      return;
    }
  }

  if( accData->type != -1 ) 
    xaccAccountSetType (account, accData->type);
  else
  {
    GtkWidget *msgbox;
    msgbox = gnome_message_box_new ( "You must select a type for the account",
                                     GNOME_MESSAGE_BOX_ERROR, "Ok", NULL );
    gtk_window_set_modal(GTK_WINDOW(msgbox) ,TRUE );
    gtk_widget_show ( msgbox );
    return;
  }

  /* description */
  w = gnc_ui_get_widget(dialog, "entryDescription");
  n = make_valid_name(gtk_entry_get_text(GTK_ENTRY(w)));
  xaccAccountSetDescription (account, n);

  /* curency */
  w = gnc_ui_get_widget(dialog, "entryCurrency");
  n = make_valid_name(gtk_entry_get_text(GTK_ENTRY(w)));
  xaccAccountSetCurrency(account, n);

  /* security */
  w = gnc_ui_get_widget(dialog, "entrySecurity");
  n = make_valid_name(gtk_entry_get_text(GTK_ENTRY(w)));
  xaccAccountSetSecurity(account, n);

  /* account code */
  w = gnc_ui_get_widget(dialog, "entryCode");
  n = make_valid_name(gtk_entry_get_text(GTK_ENTRY(w)));
  if(n)
  { /* we should check whether the code is unique in the session! */
    xaccAccountSetCode(account, n);
  } else
  { /* default code */
    xaccAccountAutoCode(account, 4);
  }
  
  /* notes */
  w = gnc_ui_get_widget(GTK_WIDGET(dialog), "notesText");
  n = gtk_editable_get_chars(GTK_EDITABLE(w), 0, -1);
  xaccAccountSetNotes(account, n);
  
  { /* make a default transaction */
    Transaction  *trans = xaccMallocTransaction();
 
    xaccTransBeginEdit(trans, 1);
    xaccTransSetDateToday (trans);
    xaccTransSetDescription (trans, OPEN_BALN_STR);
    xaccTransCommitEdit(trans);
            
    xaccAccountInsertSplit (account, xaccTransGetSplit (trans, 0) );
  }

  xaccAccountCommitEdit (account);

  {
    GtkWidget *app = gnc_get_ui_data();
    GtkCTree *ctree = gtk_object_get_data(GTK_OBJECT(app), "ctree");
    GtkCTreeNode *ctreeParent = gtk_object_get_data(GTK_OBJECT(app), 
                                                    "ctree_parent");
    double dbalance = xaccAccountGetBalance(account);
    GtkCTreeNode *newRow = NULL;
    GtkCTreeNode *parentRow;
    gchar *text[3];
    gchar buf[BUFSIZE];

    xaccSPrintAmount (buf, dbalance, PRTSYM | PRTSEP);
    
    text[0] = xaccAccountGetName(account);
    text[1] = xaccAccountGetDescription(account);
    text[2] = buf;
    
    /* Find the parent account row & add our new account to it... */
    parentRow = gtk_ctree_find_by_row_data(GTK_CTREE(ctree), ctreeParent, 
                                           accData->parentAccount);
    newRow    = gtk_ctree_insert_node (ctree, parentRow, newRow, text, 0,
                                     NULL, NULL, NULL, NULL, /* PIXMAP INFO */
                                       TRUE, TRUE);
    
    gtk_ctree_node_set_row_data(GTK_CTREE(ctree), newRow, account);
  }

  /* FIXME: Maybe
   *   Should we refresh the statusbar here?  I don't think it should be
   *   needed because we are adding an account with a zero balance.
   */

  gnome_dialog_close(GNOME_DIALOG(gtk_widget_get_toplevel(dialog)));
}

/********************************************************************\
 * accWindow                                                        *
 *   opens up a window to create a new account... the account is    * 
 *   actually created in the "create" callback                      * 
 *                                                                  * 
 * Args:   grp - the account that is selected in the main window    *
 * Return: the created window                                       *
 \*******************************************************************/
AccWindow *
accWindow (AccountGroup *grp) 
{
  AccWindow *accData;
  GtkWidget *vbox, *hbox;
  gchar     *title      = SETUP_ACCT_STR;

  accData = (AccWindow *)g_malloc(sizeof(AccWindow));
  accData->parentAccount = (Account*)grp;
  

  /* if no account group specified, assume top-level group */
  if (!grp) grp = gncGetCurrentGroup();

  accData->parentAccount = (Account*)grp;
  accData->newAccount    = xaccMallocAccount();
  accData->type          = _accWindow_last_used_account_type;
  accData->dialog = GNOME_DIALOG( gnome_dialog_new ( title, 
                                                     CREATE_STR,
                                                     CANCEL_STR,
                                                     NULL));
  
  /* Make this dialog modal */
  gtk_window_set_modal(GTK_WINDOW( &(accData->dialog)->window ),TRUE );

  /* Add accData to the dialogs object data so we can get it from anywhere later */
  gtk_object_set_data (GTK_OBJECT (accData->dialog), "accData", accData );

  vbox = GNOME_DIALOG (accData->dialog)->vbox;
  gtk_widget_show (vbox);
  
  {
    /* the account name, description, curency security and code */
    GtkWidget *frame, *accInfoTable, *aWidget;
    
    frame = gtk_frame_new("Account Info");
    gtk_container_border_width(GTK_CONTAINER(frame), 5);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, TRUE, 0);
    
    accInfoTable = gtk_table_new(5, 2, TRUE);
    gtk_container_border_width(GTK_CONTAINER(accInfoTable), 5);
    gtk_table_set_row_spacings(GTK_TABLE(accInfoTable), 3);
    gtk_table_set_col_spacings(GTK_TABLE(accInfoTable), 5);
    gtk_widget_show(accInfoTable);
    gtk_container_add(GTK_CONTAINER(frame), accInfoTable);
    
    aWidget = gtk_label_new ("Account Name:");
    gtk_widget_show (aWidget);
    gtk_misc_set_alignment (GTK_MISC (aWidget), 0.95, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(accInfoTable), aWidget,
                              0, 1, 0, 1); /* attachment info */
    
    aWidget = gtk_entry_new ();
    gtk_object_set_data (GTK_OBJECT (accData->dialog), "entryAccountName", 
                         aWidget);
    gtk_widget_show (aWidget);
    gtk_table_attach_defaults(GTK_TABLE(accInfoTable), aWidget,
                              1, 2, 0, 1);
    
    aWidget = gtk_label_new ("Description:");
    gtk_widget_show(aWidget);
    gtk_misc_set_alignment (GTK_MISC (aWidget), 0.95, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(accInfoTable), aWidget,
                              0, 1, 1, 2);
    
    aWidget = gtk_entry_new();
    gtk_object_set_data (GTK_OBJECT (accData->dialog), "entryDescription", 
                         aWidget);
    gtk_widget_show(aWidget);
    gtk_table_attach_defaults(GTK_TABLE(accInfoTable), aWidget,
                              1, 2, 1, 2);
    
    aWidget = gtk_label_new("Currency:");
    gtk_widget_show(aWidget);
    gtk_misc_set_alignment (GTK_MISC (aWidget), 0.95, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(accInfoTable), aWidget,
                              0, 1, 2, 3);
    
    aWidget = gtk_entry_new();
    gtk_object_set_data (GTK_OBJECT (accData->dialog), "entryCurrency", 
                         aWidget);
    gtk_widget_show(aWidget);
    gtk_table_attach_defaults(GTK_TABLE(accInfoTable), aWidget,
                              1, 2, 2, 3);
    
    aWidget = gtk_label_new("Security:");
    gtk_widget_show(aWidget);
    gtk_misc_set_alignment (GTK_MISC (aWidget), 0.95, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(accInfoTable), aWidget,
                              0, 1, 3, 4);
    
    aWidget = gtk_entry_new();
    gtk_object_set_data (GTK_OBJECT (accData->dialog), "entrySecurity", 
                         aWidget);
    /* Set the security entry box to insensitive by default */
    gtk_widget_set_sensitive(GTK_WIDGET(aWidget), FALSE );
    gtk_widget_show(aWidget);
    gtk_table_attach_defaults(GTK_TABLE(accInfoTable), aWidget,
                              1, 2, 3, 4);

    aWidget = gtk_label_new("Code:");
    gtk_widget_show(aWidget);
    gtk_misc_set_alignment (GTK_MISC (aWidget), 0.95, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(accInfoTable), aWidget,
                              0, 1, 4, 5);
    
    aWidget = gtk_entry_new();
    gtk_object_set_data (GTK_OBJECT (accData->dialog), "entryCode", 
                         aWidget);
    gtk_widget_show(aWidget);
    gtk_table_attach_defaults(GTK_TABLE(accInfoTable), aWidget,
                              1, 2, 4, 5);
  }
  
  hbox = gtk_hbox_new (FALSE, 5);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (hbox), 5);
  
  {
    /* the "list of types" list box */
    GtkWidget *frame, *abox, *typesList, *scrollWin;
    
    frame = gtk_frame_new("Type of Account");
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(hbox), frame, FALSE, FALSE, 0);
    
    abox = gtk_hbox_new(TRUE, 0);
    gtk_widget_show(abox);
    gtk_container_border_width(GTK_CONTAINER(abox), 5);
    gtk_container_add(GTK_CONTAINER(frame), abox);
    
    typesList = gtk_list_new();
    gtk_container_border_width(GTK_CONTAINER(typesList), 3);    
    gtk_object_set_data (GTK_OBJECT (accData->dialog), "listOfTypes", 
                         typesList);    
    gtk_widget_show(typesList);
    gtk_signal_connect(GTK_OBJECT(typesList), "select-child",
                       GTK_SIGNAL_FUNC(gnc_ui_accWindow_list_cb),
                       (gpointer)1);
    gtk_signal_connect(GTK_OBJECT(typesList), "unselect-child",
                       GTK_SIGNAL_FUNC(gnc_ui_accWindow_list_cb),
                       (gpointer)0);
    gnc_ui_accWindow_list_fill (typesList);
    
    gtk_box_pack_start(GTK_BOX(abox), typesList, TRUE, TRUE, 0);
  }
  
  {
    /* the account tree */
    GtkWidget *frame, *accountTree, *scrollWin;
    
    frame = gtk_frame_new("Parent Account");
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
    
    accountTree = gtk_tree_new();
    
    gtk_signal_connect(GTK_OBJECT(accountTree), "select-child",
                       GTK_SIGNAL_FUNC(gnc_ui_accWindow_tree_select),
                       (gpointer)1);
    gtk_signal_connect(GTK_OBJECT(accountTree), "unselect-child",
                       GTK_SIGNAL_FUNC(gnc_ui_accWindow_tree_select),
                       (gpointer)0);
    
    scrollWin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollWin),
                                    GTK_POLICY_AUTOMATIC, 
                                    GTK_POLICY_AUTOMATIC);
    gtk_widget_show (scrollWin);
    
    gtk_container_add(GTK_CONTAINER(frame), scrollWin);
    gtk_container_border_width (GTK_CONTAINER (scrollWin), 5);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrollWin),
                                          accountTree);
    
    /* Add a toplevel tree item so we can add new top level accounts  */
    {
      GtkWidget *subtree, *treeItem;
      int retVal = -2;
      
      treeItem = gtk_tree_item_new_with_label("New Toplevel Account");
      gtk_tree_append(GTK_TREE(accountTree), GTK_WIDGET(treeItem));
      
      subtree = gtk_tree_new();
      gtk_signal_connect(GTK_OBJECT(subtree), "select-child",
                         GTK_SIGNAL_FUNC(gnc_ui_accWindow_tree_select),
                         (gpointer)1);
      gtk_signal_connect(GTK_OBJECT(subtree), "unselect-child",
                         GTK_SIGNAL_FUNC(gnc_ui_accWindow_tree_select),
                         (gpointer)0);
      /* so that we will know if we have to select something */
      selected_tree = NULL; 
      selectedOne = (Account*)grp;
      retVal = gnc_ui_accWindow_tree_fill(GTK_TREE(subtree),
                                          gncGetCurrentGroup());
      selectedOne = NULL;
      gtk_tree_item_set_subtree(GTK_TREE_ITEM(treeItem), GTK_WIDGET(subtree));

      gtk_widget_show(treeItem);
      gtk_widget_show(accountTree);
      gtk_widget_map(accountTree); /* i have no ideea why do I need the call, 
                                      but without it it coredumps! */
      
      if(retVal > -2) /* this subtree contains the selected node */
      {
        gtk_tree_item_expand(GTK_TREE_ITEM(treeItem));
      }

      if(selected_tree)
      {
        gtk_widget_map(GTK_WIDGET(selected_tree));
        gtk_tree_select_child(GTK_TREE(selected_tree), selected_child);
      }
      else
         gtk_tree_select_item(GTK_TREE(accountTree), 0);
    }
  }
  
  {
    /* the notes box */
    GtkWidget *frame, *text, *table, *vscr;
    
    frame = gtk_frame_new("Notes");
    gtk_container_border_width(GTK_CONTAINER(frame), 5);
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, TRUE, 0);
   
    table = gtk_table_new(1,2, FALSE);
    gtk_widget_show(table);
    gtk_container_add(GTK_CONTAINER(frame), table);
    gtk_container_border_width (GTK_CONTAINER (table), 3);
    
    text = gtk_text_new(NULL, NULL);
    gtk_object_set_data (GTK_OBJECT(accData->dialog), "notesText", text);
    gtk_widget_set_usize(text, 300, 50);
    gtk_widget_show(text);
    gtk_text_set_editable(GTK_TEXT(text), TRUE);
    
    gtk_table_attach (GTK_TABLE (table), text, 0, 1, 0, 1,
                      GTK_FILL | GTK_EXPAND,
                      GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

    vscr = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);
    gtk_table_attach(GTK_TABLE(table), vscr, 1, 2, 0, 1,
                     GTK_FILL, GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0);
    gtk_widget_show(vscr);
  }
  
  gnome_dialog_button_connect (GNOME_DIALOG (accData->dialog), 0,
                               GTK_SIGNAL_FUNC (gnc_ui_accWindow_create_callback), 
                               accData);
  
  gnome_dialog_button_connect (GNOME_DIALOG (accData->dialog), 1,
                               GTK_SIGNAL_FUNC (gnc_ui_accWindow_cancelled_callback),
                               accData);
                               
  gtk_widget_show(GTK_WIDGET(accData->dialog));  

  return accData;
}


/*  This is a no-op for the gnome code */
void
xaccDestroyEditNotesWindow (Account *acc)
{
  return;
}



/********************** END OF FILE *********************************\
\********************************************************************/
