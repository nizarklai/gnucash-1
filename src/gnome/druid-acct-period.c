/********************************************************************\
 * druid-acct-period.c -- accouting period druid for GnuCash        *
 * Copyright (C) 2001 Gnumatic, Inc.                                *
 * Copyright (C) 2001 Dave Peticolas <dave@krondo.com>              *
 * Copyright (C) 2003 Linas Vepstas <linas@linas.org>               *
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
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
\********************************************************************/

#include "config.h"

#include <glib.h>
#include <gnome.h>
#include <libgnomeui/gnome-window-icon.h>

#include "FreqSpec.h"
#include "Group.h"
#include "Query.h"
#include "Transaction.h"
#include "dialog-utils.h"
#include "druid-acct-period.h"
#include "druid-utils.h"
#include "gnc-component-manager.h"
#include "gnc-date.h"
#include "gnc-frequency.h"
#include "gnc-ui-util.h"
#include "misc-gnome-utils.h"
#include "messages.h"
#include "qofbook.h"


#define DRUID_ACCT_PERIOD_CM_CLASS "druid-acct-period"


/** structures *********************************************************/
typedef struct
{
  GtkWidget * window;
  GnomeDruid * druid;
  GnomeDruidPage *menu_page;
  GnomeDruidPage *book_page;
  GNCFrequency *period_menu;
  GtkLabel  * period_remarks;
  GtkLabel  * close_results;
  GtkLabel  * book_details;
  GtkEntry  * book_title;
  GtkText   * book_notes;

  time_t earliest;
  char * earliest_str;
  GDate closing_date;
  GDate prev_closing_date;
  FreqSpec *period;
  int close_status;

} AcctPeriodInfo;


/* =============================================================== */
/* Find the earliest date occuring in the book.  Do this by making
 * a query and sorting by date. Since the truncated sort returns
 * only the *last* search results, sort in decreasing order.
 */
static time_t
get_earliest_in_book (QofBook *book)
{
  QofQuery *q;
  GSList *p1, *p2;
  GList *res;
  time_t earliest;

  q = qof_query_create_for(GNC_ID_SPLIT);
  qof_query_set_max_results(q, 1);
  qof_query_set_book (q, book);

  /* Sort by transaction date */
  p1 = g_slist_prepend (NULL, TRANS_DATE_POSTED);
  p1 = g_slist_prepend (p1, SPLIT_TRANS);
  p2 = g_slist_prepend (NULL, QUERY_DEFAULT_SORT);
  qof_query_set_sort_order (q, p1, p2, NULL);

  /* Reverse the sort order */
  qof_query_set_sort_increasing (q, FALSE, FALSE, FALSE);

  /* Run the query, find the earliest transaction date */
  res = qof_query_run (q);
  earliest = xaccQueryGetEarliestDateFound (q);

  qof_query_destroy (q);
  return earliest;
}

/* =============================================================== */
/* Find the number of transactions occuring before the indicated date.  
 * Do this by making a query and counting the results. 
 */

static int
get_num_xactions_before_date(QofBook *book, time_t close_date)
{
  QofQuery *q;
  GSList *param;
  QofQueryPredData *pred;
  Timespec ts;
  GList *res, *n;
  int cnt = 0;

  q = qof_query_create_for(GNC_ID_TRANS);
  qof_query_set_max_results(q, -1);
  qof_query_set_book (q, book);

  /* Look for transactions earlier than the closing date */
  param = g_slist_prepend (NULL, TRANS_DATE_POSTED);
  // param = g_slist_prepend (param, SPLIT_TRANS);   // ????
  timespecFromTime_t (&ts, close_date);
  pred = qof_query_date_predicate (QOF_COMPARE_LTE, QOF_DATE_MATCH_NORMAL, ts);
  qof_query_add_term (q,  param, pred, QOF_QUERY_FIRST_TERM);

  /* Run the query, find how many transactions there are */
  res = qof_query_run (q);

  cnt = 0;
  for (n=res; n; n=n->next) cnt ++;

  qof_query_destroy (q);
  return cnt;
}

/* =============================================================== */
static void
ap_window_destroy_cb (GtkObject *object, gpointer data)
{
  AcctPeriodInfo *info = data;

  gnc_unregister_gui_component_by_data (DRUID_ACCT_PERIOD_CM_CLASS, info);

  // gnc_frequency_destory ??
  xaccFreqSpecFree (info->period);
  g_free (info->earliest_str);
  g_free (info);
}

static void
ap_finish (GnomeDruidPage *druidpage,
                    GtkWidget *druid,
                    gpointer user_data)
{
  AcctPeriodInfo *info = user_data;

  gnc_suspend_gui_refresh ();
  printf ("finished with acct periods\n");

  gnc_resume_gui_refresh ();
  gnc_close_gui_component_by_data (DRUID_ACCT_PERIOD_CM_CLASS, info);
}

static void
ap_druid_cancel (GnomeDruid *druid, gpointer user_data)
{
  AcctPeriodInfo *info = user_data;
  gnc_close_gui_component_by_data (DRUID_ACCT_PERIOD_CM_CLASS, info);
}

/* =============================================================== */

static void
prepare_remarks (AcctPeriodInfo *info)
{
  int nperiods;
  GDate period_begin, period_end, date_now;
  const char *remarks_text;
  char * str;

  remarks_text = 
    _("The earliest transaction date found in this book is %s.\n"
      "Based on the selection made above, this book will be split\n"
      "into %d books.  Click on 'Next' to start closing the\n"
      "earliest book.\n");

  /* Pull info from widget, push into freq spec */
  gnc_frequency_save_state (info->period_menu, info->period, &info->closing_date);

  /* Count the number of periods that would be generated */
  g_date_clear (&period_begin, 1);
  g_date_clear (&period_end, 1);
  g_date_clear (&date_now, 1);
  nperiods = 1;
  period_begin = info->closing_date;
  g_date_set_time (&date_now, time(0));
  do 
  {
    xaccFreqSpecGetNextInstance (info->period, &period_begin, &period_end);
    nperiods ++;
printf ("duude np=%d ender=%d/%d/%d\n", nperiods,
g_date_month(&period_end),
g_date_day(&period_end),
g_date_year(&period_end));
    period_begin = period_end;
  } 
  while (0 > g_date_compare(&period_begin, &date_now ));

  /* Display the results */
  str = g_strdup_printf (remarks_text, info->earliest_str, nperiods);
  gtk_label_set_text (info->period_remarks, str);
  g_free (str);
}

/* =============================================================== */

static void
show_book_details (AcctPeriodInfo *info)
{
  QofBook *currbook;
  char close_date_str[MAX_DATE_LENGTH];
  char prev_close_date_str[MAX_DATE_LENGTH];
  const char *period_text;
  char *str;
  int ntrans, nacc;
  printf ("duude gwanna show period info=%p\n", info);

  /* Tell user about how the previous book closing went. */
  switch (info->close_status)
  {
    case -1: str = ""; break;
    case 0: str = _("The book was closed suscessfully"); break;
    default: str = "";
  }
  gtk_label_set_text (info->close_results, str);
  info->close_status = -1;

  /* Pull info from widget, push into freq spec */
  gnc_frequency_save_state (info->period_menu, info->period, &info->closing_date);

  period_text = 
    _("You have asked for a book to be created.  This book\n"
      "will contain all transactions up to midnight %s\n"
      "(for a total of %d transactions spread over %d accounts).\n"
      "Click on 'Next' to create this book.\n"
      "Click on 'Back' to adjust the dates.\n");

  qof_print_date_dmy_buff (close_date_str, MAX_DATE_LENGTH, 
                           g_date_day(&info->closing_date),
                           g_date_month(&info->closing_date),
                           g_date_year(&info->closing_date));

  currbook = gnc_get_current_book();
  ntrans = get_num_xactions_before_date(currbook,
                   gnc_timet_get_day_end_gdate (&info->closing_date));

  nacc = xaccGroupGetNumSubAccounts (xaccGetAccountGroup (currbook));

  /* Display the book info */
  str = g_strdup_printf (period_text, close_date_str, ntrans, nacc);
  gtk_label_set_text (info->book_details, str);
  g_free (str);

  /* Weird bug fix ! */
  gtk_widget_show (GTK_WIDGET (info->book_details));

  /* Create default settings for the title, notes fields */
  qof_print_date_dmy_buff (prev_close_date_str, MAX_DATE_LENGTH, 
                           g_date_day(&info->prev_closing_date),
                           g_date_month(&info->prev_closing_date),
                           g_date_year(&info->prev_closing_date));

  str = g_strdup_printf (_("Period %s - %s"), prev_close_date_str, close_date_str);
  gtk_entry_set_text (info->book_title, str);
  xxxgtk_text_set_text (info->book_notes, str);
  g_free (str);

}

/* =============================================================== */

static void
ap_changed (GtkWidget *widget, gpointer user_data)
{
  AcctPeriodInfo *info = user_data;

  printf ("duude something changed info=%p\n", info);
  prepare_remarks (info);
}


static void
ap_show_menu (GnomeDruidPage *druidpage,
                GtkWidget *druid,
                gpointer user_data)
{
  AcctPeriodInfo *info = user_data;

  printf ("duude show menu info=%p\n", info);

  /* Find the date of the earliest transaction in the current book.
   * Note that this could have changed since last time, since 
   * we may have closed books since last time. */
  info->earliest = get_earliest_in_book (gnc_get_current_book());
  info->earliest_str = qof_print_date(info->earliest); 
printf ("duude the new earliest is %ld %s\n", info->earliest, ctime (&info->earliest));

  prepare_remarks (info);
}

/* =============================================================== */

static void
ap_show_book (GnomeDruidPage *druidpage,
                GtkWidget *druid,
                gpointer user_data)
{
  AcctPeriodInfo *info = user_data;

  printf ("duude gwanna show period info=%p\n", info);
  show_book_details (info);
}

/* =============================================================== */

static gboolean
ap_close_period (GnomeDruidPage *druidpage,
                GtkWidget *druid,
                gpointer user_data)
{
  AcctPeriodInfo *info = user_data;

  printf ("duude gwanna close period info=%p\n", info);
  gnome_druid_page_next (info->book_page);

  info->close_status = 0;  /* XXX fixme */

  info->prev_closing_date = info->closing_date;
  xaccFreqSpecGetNextInstance (info->period, &info->prev_closing_date, &info->closing_date);
  gnc_frequency_setup (info->period_menu, NULL, &info->closing_date);

  show_book_details (info);
  return TRUE;
}

/* =============================================================== */

static void
ap_druid_create (AcctPeriodInfo *info)
{
  GladeXML *xml;
  GtkWidget *w, *page;

  xml = gnc_glade_xml_new ("acctperiod.glade", "Acct Period Druid");

  info->window = glade_xml_get_widget (xml, "Acct Period Druid");

  info->druid = GNOME_DRUID (glade_xml_get_widget (xml, "acct_period_druid"));
  gnc_druid_set_colors (info->druid);

  info->menu_page = 
        GNOME_DRUID_PAGE(glade_xml_get_widget (xml, "menu page"));
  info->book_page = 
        GNOME_DRUID_PAGE(glade_xml_get_widget (xml, "book page"));

  info->close_status = -1;

  /* Find the date of the earliest transaction in the book.
   * Add a year minus a day as the first guess for book closing,
   * and use that to set up the freq spec widget. */
  info->earliest = get_earliest_in_book (gnc_get_current_book());
  info->earliest_str = qof_print_date(info->earliest); 
printf ("duude the earliest is %ld %s\n", info->earliest, ctime (&info->earliest));

  g_date_clear (&info->closing_date, 1);
  g_date_set_time (&info->closing_date, info->earliest);
  g_date_clear (&info->prev_closing_date, 1);
  info->prev_closing_date = info->closing_date;
  g_date_add_years (&info->closing_date, 1);

  info->period = xaccFreqSpecMalloc( gnc_get_current_book() );
  xaccFreqSpecSetMonthly (info->period, &info->closing_date, 12);
  xaccFreqSpecSetUIType (info->period, UIFREQ_YEARLY);

  info->period_menu = GNC_FREQUENCY (
          gnc_frequency_new (info->period, &info->closing_date));

  /* Change the text so that its more mainingful for this druid */
  gnc_frequency_set_frequency_label_text(info->period_menu, _("Period:"));
  gnc_frequency_set_date_label_text(info->period_menu, _("Closing Date:"));

  /* Reparent to the correct location */
  w = glade_xml_get_widget (xml, "period box");
  gtk_box_pack_start (GTK_BOX (w), GTK_WIDGET (info->period_menu),
         TRUE, TRUE, 0);

  /* Get handles to all of the other widgets we'll need */
  info->period_remarks = 
        GTK_LABEL (glade_xml_get_widget (xml, "remarks label"));

  info->close_results = 
        GTK_LABEL (glade_xml_get_widget (xml, "results label"));

  info->book_details = 
        GTK_LABEL (glade_xml_get_widget (xml, "book label"));

  info->book_title = 
        GTK_ENTRY (glade_xml_get_widget (xml, "book title entry"));

  info->book_notes = 
        GTK_TEXT (glade_xml_get_widget (xml, "book notes text"));

  /* generic finished/close/abort signals */
  gtk_signal_connect (GTK_OBJECT (info->window), "destroy",
                      GTK_SIGNAL_FUNC (ap_window_destroy_cb), info);

  gtk_signal_connect (GTK_OBJECT (info->druid), "cancel",
                      GTK_SIGNAL_FUNC (ap_druid_cancel), info);

  gtk_signal_connect (GTK_OBJECT (info->menu_page), "prepare",
                      GTK_SIGNAL_FUNC (ap_show_menu), info);

  gtk_signal_connect (GTK_OBJECT (info->book_page), "prepare",
                      GTK_SIGNAL_FUNC (ap_show_book), info);

  gtk_signal_connect (GTK_OBJECT (info->book_page), "next",
                      GTK_SIGNAL_FUNC (ap_close_period), info);

  page = glade_xml_get_widget (xml, "finish page");
  gtk_signal_connect (GTK_OBJECT (page), "finish",
                      GTK_SIGNAL_FUNC (ap_finish), info);

  /* User changes the accouting period or date signals */
  gtk_signal_connect (GTK_OBJECT (info->period_menu), "changed",
                      GTK_SIGNAL_FUNC (ap_changed), info);
}


static void
ap_close_handler (gpointer user_data)
{
  AcctPeriodInfo *info = user_data;

  gtk_widget_destroy (info->window);
}

/********************************************************************\
 * gnc_acct_period_dialog                                           *
 *   opens up a druid to configure accounting periods               *
 *                                                                  * 
 * Args:   none                                                     *
 * Return: nothing                                                  *
\********************************************************************/

void
gnc_acct_period_dialog (void)
{
  AcctPeriodInfo *info;
  gint component_id;

  info = g_new0 (AcctPeriodInfo, 1);

  ap_druid_create (info);

  component_id = gnc_register_gui_component (DRUID_ACCT_PERIOD_CM_CLASS,
                                             NULL, ap_close_handler,
                                             info);

  gnome_window_icon_set_from_default(GTK_WINDOW(info->window));
  gtk_widget_show_all (info->window);

  gnc_window_adjust_for_screen (GTK_WINDOW(info->window));
}
