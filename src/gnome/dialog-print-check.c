/********************************************************************\
 * dialog-print-check.c : dialog to control check printing.         *
 * Copyright (C) 2000 Bill Gribble <grib@billgribble.com>           *
 * Copyright (C) 2006 David Hampton <hampton@employees.org>         *
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
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
\********************************************************************/

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <libguile.h>
#include <locale.h>
#include <math.h>

#include "glib-compat.h"

#include "qof.h"
#include "gnc-date.h"
#include "gnc-gconf-utils.h"
#include "gnc-numeric.h"
#include "gnc-plugin-page-register.h"
#include "dialog-print-check.h"
#include "dialog-utils.h"
#include "print-session.h"
#include "gnc-ui.h"
#include "gnc-date-format.h"
#include "gnc-ui-util.h"
#include "gnc-path.h"
#include "gnc-filepath-utils.h"
#include "gnc-gkeyfile-utils.h"
#include "Split.h"
#include "Transaction.h"

#define USE_GTKPRINT HAVE_GTK_2_10

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "gnc.printing.checks"

#define GCONF_SECTION 	       "dialogs/print_checks"
#define KEY_CHECK_FORMAT       "check_format"
#define KEY_CHECK_POSITION     "check_position"
#define KEY_DATE_FORMAT_USER   "date_format_custom"
#define KEY_CUSTOM_PAYEE       "custom_payee"
#define KEY_CUSTOM_DATE        "custom_date"
#define KEY_CUSTOM_WORDS       "custom_amount_words"
#define KEY_CUSTOM_NUMBER      "custom_amount_number"
#define KEY_CUSTOM_NOTES       "custom_memo" /* historically misnamed */
#define KEY_CUSTOM_TRANSLATION "custom_translation"
#define KEY_CUSTOM_ROTATION    "custom_rotation"
#define KEY_CUSTOM_UNITS       "custom_units"
#define KEY_SHOW_DATE_FMT      "show_date_format"


#define DEFAULT_FONT            "sans 12"
#define CHECK_FMT_DIR           "checks"
#define CUSTOM_CHECK_NAME       "custom.chk"
#define CHECK_NAME_FILTER       "*.chk"
#define DEGREES_TO_RADIANS      (G_PI / 180.0)

#define KF_GROUP_TOP       "Top"
#define KF_GROUP_POS       "Check Positions"
#define KF_GROUP_ITEMS     "Check Items"
#define KF_KEY_TITLE       "Title"
#define KF_KEY_ROTATION    "Rotation"
#define KF_KEY_TRANSLATION "Translation"
#define KF_KEY_FONT        "Font"
#define KF_KEY_ALIGN       "Align"
#define KF_KEY_SHOW_GRID   "Show_Grid"
#define KF_KEY_SHOW_BOXES  "Show_Boxes"
#define KF_KEY_NAME        "Name"
#define KF_KEY_HEIGHT      "Height"
#define KF_KEY_TYPE        "Type"
#define KF_KEY_COORDS      "Coords"
#define KF_KEY_TEXT        "Text"
#define KF_KEY_FILENAME    "Filename"

#if USE_GTKPRINT
#    define GncPrintContext GtkPrintContext
#else
#    define GncPrintContext GnomePrintContext
#endif


/* Used by glade_xml_signal_autoconnect_full */
void gnc_ui_print_check_response_cb(GtkDialog * dialog, gint response, PrintCheckDialog * pcd);
void gnc_print_check_format_changed(GtkComboBox *widget, PrintCheckDialog * pcd);
void gnc_print_check_position_changed(GtkComboBox *widget, PrintCheckDialog * pcd);
static void gnc_ui_print_save_dialog(PrintCheckDialog * pcd);
static void gnc_ui_print_restore_dialog(PrintCheckDialog * pcd);
void gnc_ui_print_restore_dialog(PrintCheckDialog * pcd);
void gnc_print_check_save_button_clicked(GtkButton *button, PrintCheckDialog *pcd);


/** This enum defines the types of items that gnucash knows how to
 *  print on checks.  Most refer to specific fields from a gnucash
 *  transaction and split, but some are generic items unrelated to
 *  gnucash. */
#define ENUM_CHECK_ITEM_TYPE(_) \
        _(NONE,) \
        _(PAYEE,) \
        _(DATE,) \
        _(NOTES,) \
        _(CHECK_NUMBER,) \
                  \
        _(MEMO,) \
        _(ACTION,) \
        _(AMOUNT_NUMBER,) \
        _(AMOUNT_WORDS,) \
                         \
        _(TEXT,) \
        _(PICTURE,)

DEFINE_ENUM(CheckItemType, ENUM_CHECK_ITEM_TYPE)
FROM_STRING_DEC(CheckItemType, ENUM_CHECK_ITEM_TYPE)
FROM_STRING_FUNC(CheckItemType, ENUM_CHECK_ITEM_TYPE)
AS_STRING_DEC(CheckItemType, ENUM_CHECK_ITEM_TYPE)
AS_STRING_FUNC(CheckItemType, ENUM_CHECK_ITEM_TYPE)

/** This data structure describes a single item printed on a check.
 *  It is build from a description in a text file. */
typedef struct _check_item {

    CheckItemType type;         /**< What type of item is this?  */

    gdouble x, y;               /**< The x/y coordinates where this item
                                 *   should be printed.  The origin for this
                                 *   coordinate is determined by the print
                                 *   system used.  */

    gdouble w, h;               /**< Optional. The width and height of this
                                 *   item.  Text will be clipped to this
                                 *   size. Pictures will be shrunk if
                                 *   necessary to fit.  */

    gchar *filename;            /**< The filename for picture items. Otherwise
                                 *   unused. */

    gchar *text;                /**< The text to be displayed (for text based
                                 *   items.) Otherwise unused.  */

    gchar *font;                /**< The font to use for text based item. This
                                 *   overrides any font in the check format.
                                 *   Unused for non-text items. */

    PangoAlignment align;       /**< The alignment of a text based item. Only
                                 *   used for text based items when a width is
                                 *   specified.  */
} check_item_t;

/** This data structure describes an entire page of checks.  Depending
 *  upon the check format, the page may contain multiple checks or
 *  only a single check.  The data structure is build from a
 *  description in a text file. */
typedef struct _check_format {

    gchar *title;               /**< Title of this check format. Displayed to
                                 *   user in the dialog box. */

    gboolean show_grid;         /**< Print a grid pattern on the page */

    gboolean show_boxes;        /**< Print boxes when width and height are
                                 *   known. */

    gdouble rotation;           /**< Rotate entire page by this amount  */

    gdouble trans_x;            /**< Move entire page by this X amount  */

    gdouble trans_y;            /**< Move entire page by this Y amount  */

    gchar *font;                /**< Default font for this page of checks */

    gdouble height;             /**< Height of one check on a page */

    GSList *positions;          /**< Names of the checks on the page. */

    GSList *items;              /**< List of items printed on each check  */
} check_format_t;

/** This data structure is used to manage the print check dialog, and
 *  the overall check printing process.  It contains pointers to many
 *  of the widgets in the dialog, pointers to the check descriptions
 *  that have been read, and also contains the data from the gnucash
 *  transaction/split that is to be printed.  */
struct _print_check_dialog {
  GladeXML * xml;
  GtkWidget * dialog;
  GtkWindow * caller_window;

  GncPluginPageRegister *plugin_page;
  Split *split;

  GtkWidget * format_combobox;
  gint format_max;
  GtkWidget * position_combobox;
  gint position_max;
  GtkWidget * custom_table;
  GtkSpinButton * payee_x,  * payee_y;
  GtkSpinButton * date_x,   * date_y;
  GtkSpinButton * words_x,  * words_y;
  GtkSpinButton * number_x, * number_y;
  GtkSpinButton * notes_x,   * notes_y;
  GtkSpinButton * translation_x, * translation_y;
  GtkSpinButton * check_rotation;
  GtkWidget * translation_label;

  GtkWidget * units_combobox;

  GtkWidget * date_format;

  gchar *format_string;

  GSList *formats_list;
  check_format_t *selected_format;
};


static void
save_float_pair (const char *section, const char *key, double a, double b)
{
  GSList *coord_list = NULL;

  coord_list = g_slist_append(coord_list, &a);
  coord_list = g_slist_append(coord_list, &b);
  gnc_gconf_set_list(section, key, GCONF_VALUE_FLOAT, coord_list, NULL);
  g_slist_free(coord_list);
}

static void
get_float_pair (const char *section, const char *key, double *a, double *b)
{
  GSList *coord_list;
  
  coord_list = gnc_gconf_get_list (section, key, GCONF_VALUE_FLOAT, NULL);
  if (NULL == coord_list) {
    *a = 0;
    *b = 0;
    return;
  }

  *a = *(gdouble*)g_slist_nth_data(coord_list, 0);
  *b = *(gdouble*)g_slist_nth_data(coord_list, 1);
  g_slist_free(coord_list);
}

static void
gnc_ui_print_save_dialog(PrintCheckDialog * pcd)
{
  const gchar *format;
  gint active;

  /* Options page */
  active = gtk_combo_box_get_active(GTK_COMBO_BOX(pcd->format_combobox));
  gnc_gconf_set_int(GCONF_SECTION, KEY_CHECK_FORMAT, active, NULL);
  active = gtk_combo_box_get_active(GTK_COMBO_BOX(pcd->position_combobox));
  gnc_gconf_set_int(GCONF_SECTION, KEY_CHECK_POSITION, active, NULL);
  active = gnc_date_format_get_format (GNC_DATE_FORMAT(pcd->date_format));
  gnc_gconf_set_int(GCONF_SECTION, KEY_DATE_FORMAT, active, NULL);
  if (active == QOF_DATE_FORMAT_CUSTOM) {
    format = gnc_date_format_get_custom (GNC_DATE_FORMAT(pcd->date_format));
    gnc_gconf_set_string(GCONF_SECTION, KEY_DATE_FORMAT_USER, format, NULL);
  } else {
    gnc_gconf_unset (GCONF_SECTION, KEY_DATE_FORMAT_USER, NULL);
  }

  /* Custom format page */
  save_float_pair(GCONF_SECTION, KEY_CUSTOM_PAYEE,
		  gtk_spin_button_get_value(pcd->payee_x),
		  gtk_spin_button_get_value(pcd->payee_y));
  save_float_pair(GCONF_SECTION, KEY_CUSTOM_DATE,
		  gtk_spin_button_get_value(pcd->date_x),
		  gtk_spin_button_get_value(pcd->date_y));
  save_float_pair(GCONF_SECTION, KEY_CUSTOM_WORDS,
		  gtk_spin_button_get_value(pcd->words_x),
		  gtk_spin_button_get_value(pcd->words_y));
  save_float_pair(GCONF_SECTION, KEY_CUSTOM_NUMBER,
		  gtk_spin_button_get_value(pcd->number_x),
		  gtk_spin_button_get_value(pcd->number_y));
  save_float_pair(GCONF_SECTION, KEY_CUSTOM_NOTES,
		  gtk_spin_button_get_value(pcd->notes_x),
		  gtk_spin_button_get_value(pcd->notes_y));
  save_float_pair(GCONF_SECTION, KEY_CUSTOM_TRANSLATION,
		  gtk_spin_button_get_value(pcd->translation_x),
		  gtk_spin_button_get_value(pcd->translation_y));
  gnc_gconf_set_float(GCONF_SECTION, KEY_CUSTOM_ROTATION,
		      gtk_spin_button_get_value(pcd->check_rotation),
		      NULL);
  active = gtk_combo_box_get_active(GTK_COMBO_BOX(pcd->units_combobox));
  gnc_gconf_set_int(GCONF_SECTION, KEY_CUSTOM_UNITS, active, NULL);
}

void
gnc_ui_print_restore_dialog(PrintCheckDialog * pcd)
{
  gchar *format;
  gdouble x, y;
  gint active;

  /* Options page */
  active = gnc_gconf_get_int(GCONF_SECTION, KEY_CHECK_FORMAT, NULL);
  gtk_combo_box_set_active(GTK_COMBO_BOX(pcd->format_combobox), active);
  active = gnc_gconf_get_int(GCONF_SECTION, KEY_CHECK_POSITION, NULL);
  gtk_combo_box_set_active(GTK_COMBO_BOX(pcd->position_combobox), active);
  active = gnc_gconf_get_int(GCONF_SECTION, KEY_DATE_FORMAT, NULL);
  gnc_date_format_set_format(GNC_DATE_FORMAT(pcd->date_format), active);
  if (active == QOF_DATE_FORMAT_CUSTOM) {
    format = gnc_gconf_get_string(GCONF_SECTION, KEY_DATE_FORMAT_USER, NULL);
    if (format) {
      gnc_date_format_set_custom(GNC_DATE_FORMAT(pcd->date_format), format);
      g_free(format);
    }
  }

  /* Custom format page */
  get_float_pair(GCONF_SECTION, KEY_CUSTOM_PAYEE, &x, &y);
  gtk_spin_button_set_value(pcd->payee_x, x);
  gtk_spin_button_set_value(pcd->payee_y, y);

  get_float_pair(GCONF_SECTION, KEY_CUSTOM_DATE, &x, &y);
  gtk_spin_button_set_value(pcd->date_x, x);
  gtk_spin_button_set_value(pcd->date_y, y);
  get_float_pair(GCONF_SECTION, KEY_CUSTOM_WORDS, &x, &y);
  gtk_spin_button_set_value(pcd->words_x, x);
  gtk_spin_button_set_value(pcd->words_y, y);
  get_float_pair(GCONF_SECTION, KEY_CUSTOM_NUMBER, &x, &y);
  gtk_spin_button_set_value(pcd->number_x, x);
  gtk_spin_button_set_value(pcd->number_y, y);
  get_float_pair(GCONF_SECTION, KEY_CUSTOM_NOTES, &x, &y);
  gtk_spin_button_set_value(pcd->notes_x, x);
  gtk_spin_button_set_value(pcd->notes_y, y);
  get_float_pair(GCONF_SECTION, KEY_CUSTOM_TRANSLATION, &x, &y);
  gtk_spin_button_set_value(pcd->translation_x, x);
  gtk_spin_button_set_value(pcd->translation_y, y);
  x = gnc_gconf_get_float(GCONF_SECTION, KEY_CUSTOM_ROTATION, NULL);
  gtk_spin_button_set_value(pcd->check_rotation, x);
  active = gnc_gconf_get_int(GCONF_SECTION, KEY_CUSTOM_UNITS, NULL);
  gtk_combo_box_set_active(GTK_COMBO_BOX(pcd->units_combobox), active);
}


static gdouble
pcd_get_custom_multip(PrintCheckDialog * pcd)
{
    gint selected;

    selected = gtk_combo_box_get_active(GTK_COMBO_BOX(pcd->units_combobox));
    switch (selected) {
        default:
            return 72.0;        /* inches */
        case 1:
            return 28.346;      /* cm */
        case 2:
            return 2.8346;      /* mm */
        case 3:
            return 1.0;         /* points */
    }
}


/** Save a coordinate pair into a check description file.  This function
 *  extracts the values from the spin buttons, adjusts them for the units
 *  multiplies (inch, pixel, etc), and then adds them to the gKeyFile. */
static void
pcd_key_file_save_xy (GKeyFile *key_file, const gchar *group_name,
                      const gchar *key_name, gdouble multip,
                      GtkSpinButton *spin0, GtkSpinButton *spin1)
{
    gdouble dd[2];

    dd[0] = multip * gtk_spin_button_get_value(spin0);
    dd[1] = multip * gtk_spin_button_get_value(spin1);
    /* Clip the numbers to three decimal places. */
    dd[0] = round(dd[0] * 1000) / 1000;
    dd[1] = round(dd[1] * 1000) / 1000;
    g_key_file_set_double_list(key_file, group_name, key_name, dd, 2);
}


/** Save the information about a single printed item into a check description
 *  file.  This function uses a helper function to extracts and save the item
 *  coordinates. */
static void
pcd_key_file_save_item_xy (GKeyFile *key_file, int index,
                           CheckItemType type, gdouble multip,
                           GtkSpinButton *spin0, GtkSpinButton *spin1)
{
    gchar *key;
    key = g_strdup_printf("Type_%d", index);
    g_key_file_set_string(key_file, KF_GROUP_ITEMS, key,
                          CheckItemTypeasString(type));
    g_free(key);
    key = g_strdup_printf("Coords_%d", index);
    pcd_key_file_save_xy(key_file, KF_GROUP_ITEMS, key, multip, spin0, spin1);
    g_free(key);
}


/** Save all of the information from the custom check dialog into a check
 *  description file. */
static void
pcd_save_custom_data(PrintCheckDialog *pcd, gchar *filename)
{
    GKeyFile *key_file;
    GError *error = NULL;
    GtkWidget *dialog;
    gdouble multip;
    gint i = 0;

    multip = pcd_get_custom_multip(pcd);

    key_file = g_key_file_new();
    g_key_file_set_string(key_file, KF_GROUP_TOP, KF_KEY_TITLE,
                          _("Custom Check"));
    g_key_file_set_boolean(key_file, KF_GROUP_TOP, KF_KEY_SHOW_GRID, FALSE);
    g_key_file_set_boolean(key_file, KF_GROUP_TOP, KF_KEY_SHOW_BOXES, FALSE);
    g_key_file_set_double(key_file, KF_GROUP_TOP, KF_KEY_ROTATION,
                          gtk_spin_button_get_value(pcd->check_rotation));
    pcd_key_file_save_xy(key_file, KF_GROUP_TOP, KF_KEY_TRANSLATION, multip,
                         pcd->translation_x, pcd->translation_y);

    pcd_key_file_save_item_xy(key_file, i++, PAYEE, multip,
                              pcd->payee_x, pcd->payee_y);
    pcd_key_file_save_item_xy(key_file, i++, DATE, multip,
                              pcd->date_x, pcd->date_y);
    pcd_key_file_save_item_xy(key_file, i++, AMOUNT_WORDS, multip,
                              pcd->words_x, pcd->words_y);
    pcd_key_file_save_item_xy(key_file, i++, AMOUNT_NUMBER, multip,
                              pcd->number_x, pcd->number_y);
    pcd_key_file_save_item_xy(key_file, i++, NOTES, multip,
                              pcd->notes_x, pcd->notes_y);

    if (!gnc_key_file_save_to_file(filename, key_file, &error)) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(pcd->dialog),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        _("Cannot save check format file."));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                 error->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_error_free(error);
    }
}


/** This function is called when the user clicks the "save format" button in
 *  the check printing dialog.  It presents another dialog to the user to get
 *  the filename for saving the data. */
void
gnc_print_check_save_button_clicked(GtkButton *button, PrintCheckDialog *pcd)
{
    GtkFileChooser *chooser;
    GtkFileFilter *filter;
    GtkWidget *dialog;
    gchar *check_dir, *filename;

    dialog = gtk_file_chooser_dialog_new(_("Save Check Description"),
                                         GTK_WINDOW(pcd->dialog),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                         NULL);
    chooser = GTK_FILE_CHOOSER(dialog);

    check_dir = g_build_filename(gnc_dotgnucash_dir(), CHECK_FMT_DIR, NULL);
    gtk_file_chooser_set_current_folder(chooser, check_dir);
    gtk_file_chooser_set_current_name(chooser, CUSTOM_CHECK_NAME);
    g_free(check_dir);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, CHECK_NAME_FILTER);
    gtk_file_filter_add_pattern(filter, CHECK_NAME_FILTER);
    gtk_file_chooser_add_filter(chooser, filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("All Files"));
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(chooser, filter);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        pcd_save_custom_data(pcd, filename);
        g_free(filename);
    }

    gtk_widget_destroy (dialog);
}


/** This is an auxiliary debugging function for converting an array of doubles
 *  into a printable string. */
static gchar *
doubles_to_string(gdouble * dd, gint len)
{
    GString *str;
    gint i;

    str = g_string_new_len(NULL, 50);
    for (i = 0; i < len; i++)
        g_string_append_printf(str, "%f ", dd[i]);
    return g_string_free(str, FALSE);
}


/** Read the information describing the placement for each item to be printed
 *  on the check.  This information is all relative to the upper left hand
 *  corner of a "check".  See the format_read_multicheck_info() function for
 *  determining if there are multiple checks on a single page of paper. This
 *  data is build into a linked list and saved as part of the check format
 *  information.  These items will be printed in the same order they are read,
 *  meaning that items listed later in the date file can be printed over top
 *  of items that appear earlier in the file. */
static GSList *
format_read_item_placement(const gchar * file,
                           GKeyFile * key_file, check_format_t * format)
{
    check_item_t *data = NULL;
    GError *error = NULL;
    GSList *list = NULL;
    gchar *key, *value, *name;
    int item_num;
    gdouble *dd;
    gsize dd_len;

    /* Read until failure. */
    for (item_num = 1;; item_num++) {

        /* Create the per-item data structure */
        data = g_new0(check_item_t, 1);
        if (NULL == data)
            return list;

        /* Get the item type */
        key = g_strdup_printf("%s_%d", KF_KEY_TYPE, item_num);
        value = g_key_file_get_string(key_file, KF_GROUP_ITEMS, key, &error);
        if (error) {
            if ((error->domain == G_KEY_FILE_ERROR)
                && (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
                /* This is the expected exit from this function. */
                goto cleanup;
            }
            goto failed;
        }
        g_debug("Check file %s, group %s, key %s, value: %s",
                file, KF_GROUP_ITEMS, key, value);
        g_free(key);

        /* Convert the type from a string to an enum, ignoring case. */
        name = g_utf8_strup(value, -1);
        data->type = CheckItemTypefromString(name);
        g_free(name);
        g_free(value);


        /* Get the item location */
        key = g_strdup_printf("%s_%d", KF_KEY_COORDS, item_num);
        dd = g_key_file_get_double_list(key_file, KF_GROUP_ITEMS,
                                        key, &dd_len, &error);
        if (error)
            goto failed;
        value = doubles_to_string(dd, dd_len);
        g_debug("Check file %s, group %s, key %s, length %zd; values: %s",
                file, KF_GROUP_ITEMS, key, dd_len, value);
        g_free(value);

        /* Must have "x;y" or "x;y;w;h". */
        switch (dd_len) {
            case 4:
                data->w = dd[2];
                data->h = dd[3];
                /* fall through */
            case 2:
                data->x = dd[0];
                data->y = dd[1];
                break;
            default:
                g_warning
                    ("Check file %s, group %s, key %s, error: 2 or 4 values only",
                     file, KF_GROUP_ITEMS, key);
                goto cleanup;
        }
        g_free(dd);
        g_free(key);

        /* Any text item can specify a font, and can also an alignment if a
         * width was provided for the item. These values are optional and do
         * not cause a failure if they are missing. */
        if (data->type != PICTURE) {
            key = g_strdup_printf("%s_%d", KF_KEY_FONT, item_num);
            data->font =
                g_key_file_get_string(key_file, KF_GROUP_ITEMS, key, &error);
            if (!error) {
                g_debug("Check file %s, group %s, key %s, value: %s",
                        file, KF_GROUP_ITEMS, key, data->font);
            } else {
                if (!((error->domain == G_KEY_FILE_ERROR)
                      && (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)))
                    g_warning("Check file %s, group %s, key %s, error: %s",
                              file, KF_GROUP_ITEMS, key, error->message);
                g_clear_error(&error);
            }
            g_free(key);

            value =
                g_key_file_get_string(key_file, KF_GROUP_ITEMS, KF_KEY_ALIGN,
                                      &error);
            if (!error) {
                g_debug("Check file %s, group %s, key %s, value: %s",
                        file, KF_GROUP_ITEMS, key, value);
                name = g_utf8_strdown(value, -1);
                if (strcmp(name, "right") == 0)
                    data->align = PANGO_ALIGN_RIGHT;
                else if (strcmp(name, "center") == 0)
                    data->align = PANGO_ALIGN_CENTER;
                else
                    data->align = PANGO_ALIGN_LEFT;
                g_free(name);
                g_free(value);
            } else {
                if (!((error->domain == G_KEY_FILE_ERROR)
                      && (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)))
                    g_warning("Check file %s, group %s, key %s, error: %s",
                              file, KF_GROUP_ITEMS, key, error->message);
                data->align = PANGO_ALIGN_LEFT;
                g_clear_error(&error);
            }
        }
        /* Get any extra data for specific items. */
        switch (data->type) {
            case PICTURE:
                key = g_strdup_printf("%s_%d", KF_KEY_FILENAME, item_num);
                data->filename =
                    g_key_file_get_string(key_file, KF_GROUP_ITEMS, key,
                                          &error);
                if (error)
                    goto failed;
                g_debug("Check file %s, group %s, key %s, value: %s",
                        file, KF_GROUP_ITEMS, key, data->filename);
                g_free(key);
                break;
            case TEXT:
                key = g_strdup_printf("%s_%d", KF_KEY_TEXT, item_num);
                data->text =
                    g_key_file_get_string(key_file, KF_GROUP_ITEMS, key,
                                          &error);
                if (error)
                    goto failed;
                g_debug("Check file %s, group %s, key %s, value: %s",
                        file, KF_GROUP_ITEMS, key, data->text);
                g_free(key);
                break;
            default:
                break;
        }

        list = g_slist_append(list, data);
        data = NULL;
    }

    /* Should never be reached. */
    return list;

  failed:
    g_warning("Check file %s, group %s, key %s, error: %s",
              file, KF_GROUP_ITEMS, key, error->message);
  cleanup:
    if (error)
        g_error_free(error);
    if (data)
        g_free(data);
    if (key)
        g_free(key);
    return list;
}


/** Free the data describing the placement of a single item on a check. */
static void
format_free_item_placement(check_item_t * data)
{
    if (data->font)
        g_free(data->font);
    if (data->text)
        g_free(data->text);
    if (data->filename)
        g_free(data->filename);
    g_free(data);
}


/** Read the information describing whether a page contains multiple checks or
 *  a single check.  If there are multiple checks on a page, this functions
 *  builds a linked list of the position names and their offsets (from the
 *  upper left corner of the page). */
static GSList *
format_read_multicheck_info(const gchar * file,
                            GKeyFile * key_file, check_format_t * format)
{
    GError *error = NULL;
    GSList *list = NULL;
    gchar *key, *name;
    int i;

    key = g_strdup_printf("%s", KF_KEY_HEIGHT);
    format->height = g_key_file_get_double(key_file, KF_GROUP_POS, key, &error);
    if (error) {
        if ((error->domain == G_KEY_FILE_ERROR)
            && ((error->code == G_KEY_FILE_ERROR_GROUP_NOT_FOUND)
                || (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND))) {
            g_clear_error(&error);
            format->height = 0.0;
        } else {
            g_warning("Check file %s, error reading group %s, key %s: %s",
                      file, KF_GROUP_POS, key, error->message);
            g_free(key);
            return list;
        }
    }

    for (i = 1;; i++) {
        key = g_strdup_printf("%s_%d", KF_KEY_NAME, i);
        name = g_key_file_get_string(key_file, KF_GROUP_POS, key, &error);
        if (error) {
            if ((error->domain == G_KEY_FILE_ERROR)
                && ((error->code == G_KEY_FILE_ERROR_GROUP_NOT_FOUND)
                    || (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND))) {
                /* This is the expected exit from this function. */
                g_free(key);
                return list;
            }
            g_warning("Check file %s, error reading group %s, key %s: %s",
                      file, KF_GROUP_POS, key, error->message);
            g_free(key);
            return list;
        }
        g_free(key);

        list = g_slist_append(list, name);
    }

    /* Should never be reached. */
    return list;
}


/** Free the data describing the placement of multiple checks on a page. */
static void
free_check_position(gchar * name)
{
    g_free(name);
}


/** Read the information describing the general layout of a page of checks.
 *  All items in this section are optional except or the name of the check
 *  style. */
static gboolean
format_read_general_info(const gchar * file,
                         GKeyFile * key_file, check_format_t * format)
{
    GError *error = NULL;
    gchar *value;
    double *dd;
    gsize dd_len;

    format->title =
        g_key_file_get_string(key_file, KF_GROUP_TOP, KF_KEY_TITLE, &error);
    if (!error) {
        g_debug("Check file %s, group %s, key %s, value: %s",
                file, KF_GROUP_TOP, KF_KEY_TITLE, format->title);
    } else {
        g_warning("Check file %s, group %s, key %s, error: %s",
                  file, KF_GROUP_TOP, KF_KEY_TITLE, error->message);
        return FALSE;
    }

    format->show_grid =
        g_key_file_get_boolean(key_file, KF_GROUP_TOP, KF_KEY_SHOW_GRID,
                               &error);
    if (!error) {
        g_debug("Check file %s, group %s, key %s, value: %d",
                file, KF_GROUP_TOP, KF_KEY_SHOW_GRID, format->show_grid);
    } else {
        if (!((error->domain == G_KEY_FILE_ERROR)
              && (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)))
            g_warning("Check file %s, group %s, key %s, error: %s",
                      file, KF_GROUP_TOP, KF_KEY_SHOW_GRID, error->message);
        format->show_grid = FALSE;
        g_clear_error(&error);
    }

    format->show_boxes =
        g_key_file_get_boolean(key_file, KF_GROUP_TOP, KF_KEY_SHOW_BOXES,
                               &error);
    if (!error) {
        g_debug("Check file %s, group %s, key %s, value: %d",
                file, KF_GROUP_TOP, KF_KEY_SHOW_BOXES, format->show_boxes);
    } else {
        if (!((error->domain == G_KEY_FILE_ERROR)
              && (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)))
            g_warning("Check file %s, group %s, key %s, error: %s",
                      file, KF_GROUP_TOP, KF_KEY_SHOW_BOXES, error->message);
        format->show_boxes = FALSE;
        g_clear_error(&error);
    }

    format->font =
        g_key_file_get_string(key_file, KF_GROUP_TOP, KF_KEY_FONT, &error);
    if (!error) {
        g_debug("Check file %s, group %s, key %s, value: %s",
                file, KF_GROUP_TOP, KF_KEY_FONT, format->font);
    } else {
        if (!((error->domain == G_KEY_FILE_ERROR)
              && (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)))
            g_warning("Check file %s, group %s, key %s, error: %s",
                      file, KF_GROUP_TOP, KF_KEY_FONT, error->message);
        format->font = g_strdup(DEFAULT_FONT);
        g_clear_error(&error);
    }

    format->rotation =
        g_key_file_get_double(key_file, KF_GROUP_TOP, KF_KEY_ROTATION, &error);
    if (!error) {
        g_debug("Check file %s, group %s, key %s, value: %f",
                file, KF_GROUP_TOP, KF_KEY_ROTATION, format->rotation);
    } else {
        if (!((error->domain == G_KEY_FILE_ERROR)
              && (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)))
            g_warning("Check file %s, group %s, key %s, error: %s",
                      file, KF_GROUP_TOP, KF_KEY_ROTATION, error->message);
        format->rotation = 0.0;
        g_clear_error(&error);
    }

    dd = g_key_file_get_double_list(key_file, KF_GROUP_TOP, KF_KEY_TRANSLATION,
                                    &dd_len, &error);
    if (!error) {
        value = doubles_to_string(dd, dd_len);
        g_debug("Check file %s, group %s, key %s, length %zd; values: %s",
                file, KF_GROUP_TOP, KF_KEY_TRANSLATION, dd_len, value);
        g_free(value);

        if (dd_len == 2) {
            format->trans_x = dd[0];
            format->trans_y = dd[1];
        } else {
            g_warning("Check file %s, error top %s, key %s: 2 values only",
                      file, KF_GROUP_TOP, KF_KEY_TRANSLATION);
        }
        g_free(dd);
    } else {
        if (!((error->domain == G_KEY_FILE_ERROR)
              && (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)))
            g_warning("Check file %s, group top %s, key %s: %s",
                      file, KF_GROUP_ITEMS, KF_KEY_TRANSLATION, error->message);
        g_clear_error(&error);
    }

    return TRUE;
}


/** Free all of the information describing a page of checks. */
static void
free_check_format(check_format_t * data)
{
    g_free(data->title);
    g_free(data->font);
    g_slist_foreach(data->positions, (GFunc) free_check_position, NULL);
    g_slist_free(data->positions);
    g_slist_foreach(data->items, (GFunc) format_free_item_placement, NULL);
    g_slist_free(data->items);
    g_free(data);
}


/** Read a single check format file and append the resulting format to the
 *  list of all known formats.  This function calls other functions to read
 *  each section of the data file. */
static void
read_one_check_format(PrintCheckDialog * pcd,
                      const gchar * dirname, const gchar * file)
{
    gchar *pathname;
    GKeyFile *key_file;
    check_format_t *format;

    pathname = g_build_filename(dirname, file, (char *)NULL);
    key_file = gnc_key_file_load_from_file(pathname, FALSE, FALSE);
    g_free(pathname);
    if (!key_file) {
        g_warning("Check file %s, cannot load file", file);
        return;
    }

    format = g_new0(check_format_t, 1);
    if (format_read_general_info(file, key_file, format)) {
        format->positions = format_read_multicheck_info(file, key_file, format);
        format->items = format_read_item_placement(file, key_file, format);
    }

    g_key_file_free(key_file);
    if ((NULL == format->title) || (NULL == format->items)) {
        g_warning("Check file %s, no items read. Dropping file.", file);
        free_check_format(format);
        return;
    }

    pcd->formats_list = g_slist_append(pcd->formats_list, format);
}


/** Iterate over a single check directory, throwing out any backup files and
 *  then calling a helper function to read and parse the check format withing
 *  the file. */
static void
read_one_check_directory(PrintCheckDialog * pcd, const gchar * dirname)
{
    GDir *dir;
    const gchar *filename;

    dir = g_dir_open(dirname, 0, NULL);
    if (dir) {
        while ((filename = g_dir_read_name(dir)) != NULL) {
            if (g_str_has_prefix(filename, "#"))
                continue;
            if (g_str_has_suffix(filename, ".chk"))
                read_one_check_format(pcd, dirname, filename);
        }
        g_dir_close(dir);
    }
}


/** Read all check formats.  This function first looks in the system directory
 *  for check files, and then looks in the user's .gnucash directory for any
 *  custom check files. */
static void
read_formats(PrintCheckDialog * pcd)
{
    gchar *dirname, *pkgdatadir;

    pkgdatadir = gnc_path_get_pkgdatadir();
    dirname = g_build_filename(pkgdatadir, CHECK_FMT_DIR, (char *)NULL);
    read_one_check_directory(pcd, dirname);
    g_free(dirname);
    g_free(pkgdatadir);

    dirname = gnc_build_dotgnucash_path(CHECK_FMT_DIR);
    read_one_check_directory(pcd, dirname);
    g_free(dirname);
}


/********************************************************************\
 * gnc_ui_print_check_dialog_create
 * make a new print check dialog and wait for it.
\********************************************************************/

void
gnc_ui_print_check_dialog_create(GncPluginPageRegister *plugin_page,
                                 Split *split)
{
  PrintCheckDialog * pcd;
  GladeXML *xml;
  GtkWidget *table;
  GtkWindow *window;
  GSList *elem;
  GtkListStore *store;

  pcd = g_new0(PrintCheckDialog, 1);
  pcd->plugin_page = plugin_page;
  pcd->split = split;

  xml = gnc_glade_xml_new ("print.glade", "Print Check Dialog");
  glade_xml_signal_autoconnect_full(xml, gnc_glade_autoconnect_full_func, pcd);

  pcd->xml = xml;
  pcd->dialog = glade_xml_get_widget (xml, "Print Check Dialog");

  read_formats(pcd);

  /* now pick out the relevant child widgets */
  pcd->format_combobox = glade_xml_get_widget (xml, "check_format_combobox");
  pcd->position_combobox = glade_xml_get_widget (xml, "check_position_combobox");

  pcd->custom_table = glade_xml_get_widget (xml, "custom_table");
  pcd->payee_x = GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "payee_x_entry"));
  pcd->payee_y = GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "payee_y_entry"));
  pcd->date_x = GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "date_x_entry"));
  pcd->date_y = GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "date_y_entry"));
  pcd->words_x =
    GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "amount_words_x_entry"));
  pcd->words_y =
    GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "amount_words_y_entry"));
  pcd->number_x =
    GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "amount_numbers_x_entry"));
  pcd->number_y =
    GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "amount_numbers_y_entry"));
  pcd->notes_x = GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "notes_x_entry"));
  pcd->notes_y = GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "notes_y_entry"));
  pcd->translation_x = GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "translation_x_entry"));
  pcd->translation_y = GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "translation_y_entry"));
  pcd->translation_label = glade_xml_get_widget (xml, "translation_label");
  pcd->check_rotation =
    GTK_SPIN_BUTTON(glade_xml_get_widget (xml, "check_rotation_entry"));
  pcd->units_combobox = glade_xml_get_widget (xml, "units_combobox");

  window = GTK_WINDOW(GNC_PLUGIN_PAGE(plugin_page)->window);
  gtk_window_set_transient_for(GTK_WINDOW(pcd->dialog), window);
  pcd->caller_window = GTK_WINDOW(window);

  /* Create and attach the date-format chooser */
  table = glade_xml_get_widget (xml, "options_table");
  pcd->date_format = gnc_date_format_new_without_label();
  gtk_table_attach_defaults(GTK_TABLE(table), pcd->date_format, 1, 3, 2, 7);

  /* Update the combo boxes bases on the available check formats */
  store = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_combo_box_set_model(GTK_COMBO_BOX(pcd->format_combobox),
                          GTK_TREE_MODEL(store));
  for (elem = pcd->formats_list; elem; elem = g_slist_next(elem)) {
    gtk_combo_box_append_text(GTK_COMBO_BOX(pcd->format_combobox),
                              ((check_format_t*)elem->data)->title);
  }
  gtk_combo_box_append_text(GTK_COMBO_BOX(pcd->format_combobox), _("Custom"));
  pcd->format_max = g_slist_length(pcd->formats_list); /* -1 for 0 base, +1 for custom entry*/

#if USE_GTKPRINT
  gtk_widget_destroy(glade_xml_get_widget (xml, "lower_left"));
#else
  gtk_widget_destroy(glade_xml_get_widget (xml, "upper_left"));
#endif

  gnc_ui_print_restore_dialog(pcd);
  gnc_restore_window_size(GCONF_SECTION, GTK_WINDOW(pcd->dialog));
  gtk_widget_show_all(pcd->dialog);
}

/********************************************************************\
 * Print check contents to the page.
\********************************************************************/


/** Draw a grid pattern on the page to be printed.  This grid is helpful when
 *  figuring out the offsets for where to print various items on the page. */
static void
draw_grid(GncPrintContext * context, gint width, gint height)
{
    const double dash_pattern[2] = { 1.0, 5.0 };
#if USE_GTKPRINT
    PangoFontDescription *desc;
    PangoLayout *layout;
    cairo_t *cr;
#endif
    gchar *text;
    gint i;

#if USE_GTKPRINT
    /* Initialize for printing text */
    layout = gtk_print_context_create_pango_layout(context);
    desc = pango_font_description_from_string("sans 12");
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
    pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
    pango_layout_set_width(layout, -1);
#endif

    /* Set up the line to draw with. */
#if USE_GTKPRINT
    cr = gtk_print_context_get_cairo_context(context);
    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_set_line_cap(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_dash(cr, dash_pattern, 2, 0);
#else
    gnome_print_gsave(context);
    gnome_print_setlinewidth(context, 1.0);
    gnome_print_setlinecap(context, 2);
    gnome_print_setdash(context, 2, dash_pattern, 0);
#endif

    /* Draw horizontal lines */
    for (i = -200; i < (height + 200); i += 50) {
        text = g_strdup_printf("%d", (int)i);
#if USE_GTKPRINT
        cairo_move_to(cr, -200, i);
        cairo_line_to(cr, width + 200, i);
        cairo_stroke(cr);
        pango_layout_set_text(layout, text, -1);
        cairo_move_to(cr, 0, i);
        pango_cairo_show_layout(cr, layout);
#else
        gnome_print_line_stroked(context, -200, i, width + 200, i);
        gnome_print_moveto(context, 0, i);
        gnome_print_show(context, text);
#endif
        g_free(text);
    }

    /* Draw vertical lines */
    for (i = -200; i < (width + 200); i += 50) {
        text = g_strdup_printf("%d", (int)i);
#if USE_GTKPRINT
        cairo_move_to(cr, i, -200);
        cairo_line_to(cr, i, height + 200);
        cairo_stroke(cr);
        pango_layout_set_text(layout, text, -1);
        cairo_move_to(cr, i, 0);
        pango_cairo_show_layout(cr, layout);
#else
        gnome_print_line_stroked(context, i, -200, i, height + 200);
        gnome_print_moveto(context, i, 0);
        gnome_print_show(context, text);
#endif
        g_free(text);
    }

    /* Clean up after ourselves */
#if USE_GTKPRINT
    cairo_restore(cr);
    g_object_unref(layout);
#else
    gnome_print_gsave(context);
#endif
}


/** Print a single line of text to the printed page.  If a width and height
 *  are specified, the line will be wrapped at the specified width, and the
 *  resulting text will be clipped if it does not fit in the space
 *  available. */
static gdouble
draw_text(GncPrintContext * context, const gchar * text, check_item_t * data,
          PangoFontDescription *default_desc)
{
#if USE_GTKPRINT
    PangoFontDescription *desc;
    PangoLayout *layout;
    cairo_t *cr;
    gint layout_height, layout_width;
    gdouble width, height;

    /* Initialize for printing text */
    layout = gtk_print_context_create_pango_layout(context);
    if (data->font) {
      desc = pango_font_description_from_string(data->font);
      pango_layout_set_font_description(layout, desc);
      pango_font_description_free(desc);
    } else {
      pango_layout_set_font_description(layout, default_desc);
    }
    pango_layout_set_alignment(layout,
                               data->w ? data->align : PANGO_ALIGN_LEFT);
    pango_layout_set_width(layout, data->w ? data->w * PANGO_SCALE : -1);
    pango_layout_set_text(layout, text, -1);
    pango_layout_get_size(layout, &layout_width, &layout_height);
    width = (gdouble) layout_width / PANGO_SCALE;
    height = (gdouble) layout_height / PANGO_SCALE;

    cr = gtk_print_context_get_cairo_context(context);
    cairo_save(cr);

    /* Clip text to the enclosing rectangle */
    if (data->w && data->h) {
        g_debug("Text clip rectangle, coords %f,%f, size %f,%f",
                data->x, data->y - data->h, data->w, data->h);
        cairo_rectangle(cr, data->x, data->y - data->h, data->w, data->h);
        cairo_clip_preserve(cr);
    }

    /* Draw the text */
    g_debug("Text move to %f,%f, print '%s'", data->x, data->y, text);
    cairo_move_to(cr, data->x, data->y - height);
    pango_cairo_show_layout(cr, layout);

    /* Clean up after ourselves */
    cairo_restore(cr);
    g_object_unref(layout);
    return width;
#else
    g_debug("Text move to %f,%f, print '%s'", data->x, data->y, text);
    gnome_print_moveto(context, data->x, data->y);
    gnome_print_show(context, text);
    return 0.0;
#endif
}


#if USE_GTKPRINT
/** Find and load the specified image.  If the specified filename isn't an
 *  absolute path name, this code will also look in the gnucash system check
 *  format directory, and then in the user's private check format
 *  directory.
 *
 *  NOTE: The gtk_image_new_from_file() function never fails.  If it can't
 *  find the specified file, it returs the "broken image" icon.  This function
 *  takes advantage of that.
*/
static GtkWidget *
read_image (const gchar *filename)
{
    GtkWidget *image;
    gchar *pkgdatadir, *dirname, *tmp_name;

    if (g_path_is_absolute(filename))
        return gtk_image_new_from_file(filename);

    pkgdatadir = gnc_path_get_pkgdatadir();
    tmp_name = g_build_filename(pkgdatadir, CHECK_FMT_DIR, filename, (char *)NULL);
    if (!g_file_exists(tmp_name)) {
        g_free(tmp_name);
	dirname = gnc_build_dotgnucash_path(CHECK_FMT_DIR);
	tmp_name = g_build_filename(dirname, filename, (char *)NULL);
	g_free(dirname);
    }
    image = gtk_image_new_from_file(tmp_name);
    g_free(tmp_name);
    return image;
}


/** Print a single image to the printed page.  This picture will be scaled
 *  down to fit in the specified size rectangle.  Scaling is done with the
 *  proportions locked 1:1 so as not to distort the image. */
static void
draw_picture(GtkPrintContext * context, check_item_t * data)
{
    cairo_t *cr;
    GdkPixbuf *pixbuf, *scaled_pixbuf;
    GtkImage *image;
    gint pix_w, pix_h;
    gdouble scale_w, scale_h, scale;

    cr = gtk_print_context_get_cairo_context(context);
    cairo_save(cr);

    /* Get the picture. */
    image = GTK_IMAGE(read_image(data->filename));
    pixbuf = gtk_image_get_pixbuf(image);
    if (pixbuf) {
        g_object_ref(pixbuf);
    } else {
        g_warning("Filename '%s' cannot be read or understood.",
                  data->filename);
        pixbuf = gtk_widget_render_icon(GTK_WIDGET(image),
                                        GTK_STOCK_MISSING_IMAGE,
                                        -1, NULL);
    }
    pix_w = gdk_pixbuf_get_width(pixbuf);
    pix_h = gdk_pixbuf_get_height(pixbuf);

    /* Draw the enclosing rectangle */
    if (data->w && data->h) {
        cairo_rectangle(cr, data->x, data->y - data->h, data->w, data->h);
        g_debug("Picture clip rectangle, user coords %f,%f, user size %f,%f",
                data->x, data->y - data->h, data->w, data->h);
    } else {
        cairo_rectangle(cr, data->x, data->y - pix_h, pix_w, pix_h);
        g_debug("Picture clip rectangle, user coords %f,%f, pic size %d,%d",
                data->x, data->y - data->h, pix_w, pix_h);
    }
    cairo_clip_preserve(cr);

    /* Scale down to fit.  Never scale up. */
    scale_w = scale_h = 1;
    if (data->w && (pix_w > data->w))
        scale_w = data->w / pix_w;
    if (data->h && (pix_h > data->h))
        scale_h = data->h / pix_h;
    scale = MIN(scale_w, scale_h);

    if (scale != 1) {
        scaled_pixbuf = gdk_pixbuf_scale_simple(pixbuf, pix_w * scale,
                                                pix_h * scale,
                                                GDK_INTERP_BILINEAR);
        pix_h = gdk_pixbuf_get_height(scaled_pixbuf);
        gdk_cairo_set_source_pixbuf(cr, scaled_pixbuf, data->x,
                                    data->y - pix_h);
        g_object_unref(scaled_pixbuf);
    } else {
        gdk_cairo_set_source_pixbuf(cr, pixbuf, data->x, data->y - pix_h);
    }
    g_object_unref(pixbuf);
    cairo_paint(cr);

    /* Clean up after ourselves */
    cairo_restore(cr);
    gtk_widget_destroy(GTK_WIDGET(image));
}
#endif


#define DATE_FMT_HEIGHT 8
#define DATE_FMT_SLOP   2

/* There is a new Canadian requirement that all software that prints the date
 * on a check must also print the format of that date underneath using a 6-8
 * point font.  This function implements that requirement.  It requires the
 * font description used in printing the date so that it can print in the same
 * font using a smaller point size.  It also requires width of the printed
 * date as an argument, allowing it to center the format string under the
 * date.
 *
 * Note: This code only prints a date if the user has explicitly requested it
 * via a preference (gconf) setting.  This is because gnucash has no way of
 * knowing if the user's checks already have a date format printed on them.
 */
static void
draw_date_format(GncPrintContext * context, const gchar *date_format,
                 check_item_t * data, PangoFontDescription *default_desc,
                 gdouble width)
{
#if USE_GTKPRINT
    PangoFontDescription *date_desc;
    check_item_t date_item;
    gchar *text = NULL, *expanded = NULL;
    const gchar *thislocale, *c;
    GString *cdn_fmt;

    thislocale = setlocale(LC_ALL, NULL);
    if (!gnc_gconf_get_bool(GCONF_SECTION, KEY_SHOW_DATE_FMT, NULL))
        return;

    date_desc = pango_font_description_copy_static(default_desc);
    pango_font_description_set_size(date_desc, DATE_FMT_HEIGHT * PANGO_SCALE);
    date_item = *data;
    date_item.y += (DATE_FMT_HEIGHT + DATE_FMT_SLOP);
    date_item.w = width;
    date_item.h = DATE_FMT_HEIGHT + DATE_FMT_SLOP;
    date_item.align = PANGO_ALIGN_CENTER;

    /* This is a date format string. It should only contain ascii. */
    cdn_fmt = g_string_new_len(NULL, 50);
    for (c = date_format; c && *c; ) {
        if ((c[0] != '%') || (c[1] == '\0')) {
            c += 1;
            continue;
        }
        switch (c[1]) {
            case 'F':
                cdn_fmt = g_string_append(cdn_fmt, "YYYYMMDD");
                break;
            case 'Y':
                cdn_fmt = g_string_append(cdn_fmt, "YYYY");
                break;
            case 'y':
                cdn_fmt = g_string_append(cdn_fmt, "YY");
                break;
            case 'm':
                cdn_fmt = g_string_append(cdn_fmt, "MM");
                break;
            case 'd':
            case 'e':
                cdn_fmt = g_string_append(cdn_fmt, "DD");
                break;
            case 'x':
                expanded = g_strdup_printf("%s%s",
                                           qof_date_format_get_string(QOF_DATE_FORMAT_LOCALE),
                                           c + 2);
                c = expanded;
                continue;
            default:
                break;
        }
        c += 2;
    }

    text = g_string_free(cdn_fmt, FALSE);
    draw_text(context, text, &date_item, date_desc);
    g_free(text);     
    if (expanded)
        g_free(expanded);
    pango_font_description_free(date_desc);
#endif
}


/** Print each of the items that in the description of a single check.  This
 *  function uses helper functions to print text based and picture based
 *  items. */
static void
draw_page_items(GncPrintContext * context,
                gint page_nr, check_format_t * format, gpointer user_data)
{
    PrintCheckDialog *pcd = (PrintCheckDialog *) user_data;
    PangoFontDescription *default_desc;
    Transaction *trans;
    gnc_numeric amount;
    GNCPrintAmountInfo info;
    const gchar *date_format;
    gchar *text = NULL, buf[100];
    GSList *elem;
    check_item_t *item;
    gdouble width;
    GDate *date;

    trans = xaccSplitGetParent(pcd->split);
    /* This was valid when the check printing dialog was instantiated. */
    g_return_if_fail(trans);
    amount = gnc_numeric_abs(xaccSplitGetAmount(pcd->split));

    default_desc = pango_font_description_from_string(format->font);

    /* Now put the actual data onto the page. */
    for (elem = format->items; elem; elem = g_slist_next(elem)) {
        item = elem->data;

        switch (item->type) {
            case DATE:
                date = g_date_new();
                g_date_set_time_t(date, xaccTransGetDate(trans));
                date_format =
                    gnc_date_format_get_custom(GNC_DATE_FORMAT
                                               (pcd->date_format));
                g_date_strftime(buf, 100, date_format, date);
                width = draw_text(context, buf, item, default_desc);
		draw_date_format(context, date_format, item, default_desc, width);
                g_date_free(date);
                break;

            case PAYEE:
                draw_text(context, xaccTransGetDescription(trans), item, default_desc);
                break;

            case NOTES:
                draw_text(context, xaccTransGetNotes(trans), item, default_desc);
                break;

            case MEMO:
                draw_text(context, xaccSplitGetMemo(pcd->split), item, default_desc);
                break;

            case ACTION:
                draw_text(context, xaccSplitGetAction(pcd->split), item, default_desc);
                break;

            case CHECK_NUMBER:
                draw_text(context, xaccTransGetNum(trans), item, default_desc);
                break;

            case AMOUNT_NUMBER:
                info = gnc_default_print_info(FALSE);
                draw_text(context, xaccPrintAmount(amount, info),
                          item, default_desc);
                break;

            case AMOUNT_WORDS:
                text = numeric_to_words(amount);
                draw_text(context, text, item, default_desc);
                g_free(text);
                break;

            case TEXT:
                draw_text(context, item->text, item, default_desc);
                break;

#if USE_GTKPRINT
            case PICTURE:
                draw_picture(context, item);
                break;
#endif

            default:
                text = g_strdup_printf("(unknown check field %d)", item->type);
                draw_text(context, text, item, default_desc);
                g_free(text);
        }
    }

    pango_font_description_free(default_desc);
}


#if USE_GTKPRINT
/** Print each of the items that in the description of a single check.  This
 *  function uses helper functions to print text based and picture based
 *  items. */
static void
draw_page_boxes(GncPrintContext * context,
                gint page_nr, check_format_t * format, gpointer user_data)
{
    cairo_t *cr;
    GSList *elem;
    check_item_t *item;

    cr = gtk_print_context_get_cairo_context(context);

    /* Now put the actual data onto the page. */
    for (elem = format->items; elem; elem = g_slist_next(elem)) {
        item = elem->data;
        if (!item->w || !item->h)
            continue;
        cairo_rectangle(cr, item->x, item->y - item->h, item->w, item->h);
        cairo_stroke(cr);
    }
}


/** Print an entire page based upon the layout in a check description file.
 *  This function takes care of translating/rotating the page, calling the function to print the grid
 *  pattern (if requested), an

each of the items that in the description of a single check.  This
 *  function uses helper functions to print text based and picture based
 *  items. */
static void
draw_page_format(GncPrintContext * context,
                 gint page_nr, check_format_t * format, gpointer user_data)
{
    PrintCheckDialog *pcd = (PrintCheckDialog *) user_data;
    cairo_t *cr;
    gint i;
    gdouble x, y, multip;

    cr = gtk_print_context_get_cairo_context(context);
    cairo_translate(cr, format->trans_x, format->trans_y);
    g_debug("Page translated by %f,%f", format->trans_x, format->trans_y);
    cairo_rotate(cr, format->rotation * DEGREES_TO_RADIANS);
    g_debug("Page rotated by %f degrees", format->rotation);

    /* The grid is useful when determining check layouts */
    if (format->show_grid) {
        draw_grid(context,
                  gtk_print_context_get_width(context),
                  gtk_print_context_get_height(context));
    }

    /* Translate all subsequent check items if requested. */
    i = gtk_combo_box_get_active(GTK_COMBO_BOX(pcd->position_combobox));
    if ((i >= 0) && (i < pcd->position_max)) {
        y = format->height * i;
        cairo_translate(cr, 0, y);
        g_debug("Position translated by %f (pre-defined)", y);
    } else {
        multip = pcd_get_custom_multip(pcd);
        x = multip * gtk_spin_button_get_value(pcd->translation_x);
        y = multip * gtk_spin_button_get_value(pcd->translation_y);
        cairo_translate(cr, x, y);
        g_debug("Position translated by %f (custom)", y);
    }

    /* Draw layout boxes if requested. Also useful when determining check
     * layouts. */
    if (format->show_boxes)
      draw_page_boxes(context, page_nr, format, user_data);

    /* Draw the actual check data. */
    draw_page_items(context, page_nr, format, user_data);
}

#else

static void
draw_page_format(PrintSession * ps, check_format_t * format, gpointer user_data)
{
    PrintCheckDialog *pcd = (PrintCheckDialog *) user_data;
    GnomePrintConfig *config;
    gint i;
    GSList *elem;
    check_item_t *item;
    gdouble width, height, x, y, multip;

    config = gnome_print_job_get_config(ps->job);
    gnome_print_job_get_page_size_from_config(config, &width, &height);

    gnome_print_gsave(ps->context);
    gnome_print_translate(ps->context, format->trans_x, format->trans_y);
    g_debug("Page translated by %f,%f", format->trans_x, format->trans_y);
    gnome_print_rotate(ps->context, format->rotation);
    g_debug("Page rotated by %f degrees", format->rotation);

    /* The grid is useful when determining check layouts */
    if (format->show_grid) {
        draw_grid(ps->context, width, height);
    }

    /* Translate all subsequent check items if requested. */
    i = gtk_combo_box_get_active(GTK_COMBO_BOX(pcd->position_combobox));
    if ((i >= 0) && (i < pcd->position_max)) {
        y = height - (format->height * (i + 1));
        gnome_print_translate(ps->context, 0, y);
        g_debug("Check translated by %f (pre-defined)", y);
    } else {
        multip = pcd_get_custom_multip(pcd);
        x = multip * gtk_spin_button_get_value(pcd->translation_x);
        y = multip * gtk_spin_button_get_value(pcd->translation_y);
        gnome_print_translate(ps->context, x, y);
        g_debug("Check translated by %f (custom)", y);
    }

    /* Draw layout boxes if requested. Also useful when determining check
     * layouts. */
    if (format->show_boxes) {
        for (elem = format->items; elem; elem = g_slist_next(elem)) {
            item = elem->data;
            if (!item->w || !item->h)
                continue;
            gnome_print_rect_stroked(ps->context, item->x, item->y, item->w,
                                     item->h);
        }
    }

    draw_page_items(ps->context, 1, format, user_data);
}
#endif

static void
draw_page_custom(GncPrintContext * context, gint page_nr, gpointer user_data)
{
    PrintCheckDialog *pcd = (PrintCheckDialog *) user_data;
    GNCPrintAmountInfo info;
    PangoFontDescription *desc;
    Transaction *trans;
    gnc_numeric amount;
#if USE_GTKPRINT
    cairo_t *cr;
#endif
    const gchar *date_format;
    gchar *text = NULL, buf[100];
    check_item_t item = { 0 };
    gdouble x, y, multip, degrees;
    GDate *date;

    trans = xaccSplitGetParent(pcd->split);
    /* This was valid when the check printing dialog was instantiated. */
    g_return_if_fail(trans);

    desc = pango_font_description_from_string("sans 12");

    multip = pcd_get_custom_multip(pcd);
    degrees = gtk_spin_button_get_value(pcd->check_rotation);
#if USE_GTKPRINT
    cr = gtk_print_context_get_cairo_context(context);
    cairo_rotate(cr, degrees * DEGREES_TO_RADIANS);
#else
    gnome_print_rotate(context, degrees);
#endif
    g_debug("Page rotated by %f degrees", degrees);

    x = multip * gtk_spin_button_get_value(pcd->translation_x);
    y = multip * gtk_spin_button_get_value(pcd->translation_y);
#if USE_GTKPRINT
    cairo_translate(cr, x, y);
#else
    gnome_print_translate(context, x, y);
#endif
    g_debug("Page translated by %f,%f", x, y);

    item.x = multip * gtk_spin_button_get_value(pcd->payee_x);
    item.y = multip * gtk_spin_button_get_value(pcd->payee_y);
    draw_text(context, xaccTransGetDescription(trans), &item, desc);

    item.x = multip * gtk_spin_button_get_value(pcd->date_x);
    item.y = multip * gtk_spin_button_get_value(pcd->date_y);
    date = g_date_new();
    g_date_set_time_t(date, xaccTransGetDate(trans));
    date_format = gnc_date_format_get_custom(GNC_DATE_FORMAT(pcd->date_format));
    g_date_strftime(buf, 100, date_format, date);
    draw_text(context, buf, &item, desc);
    g_date_free(date);

    item.x = multip * gtk_spin_button_get_value(pcd->words_x);
    item.y = multip * gtk_spin_button_get_value(pcd->words_y);
    info = gnc_default_print_info(FALSE);
    amount = gnc_numeric_abs(xaccSplitGetAmount(pcd->split));
    draw_text(context, xaccPrintAmount(amount, info), &item, desc);

    item.x = multip * gtk_spin_button_get_value(pcd->number_x);
    item.y = multip * gtk_spin_button_get_value(pcd->number_y);
    text = numeric_to_words(amount);
    draw_text(context, text, &item, desc);
    g_free(text);

    item.x = multip * gtk_spin_button_get_value(pcd->notes_x);
    item.y = multip * gtk_spin_button_get_value(pcd->notes_y);
    draw_text(context, xaccTransGetNotes(trans), &item, desc);

    pango_font_description_free(desc);
}

#if USE_GTKPRINT
/* Print a page of checks. Today, check printing only prints one check at a
 * time.  When its extended to print multiple checks, this will need to take
 * into account the number of checks to print, the number of checks on a page,
 * and the starting check position on the page. This function is called once
 * by the GtkPrint code once for each page to be printed. */
static void
draw_page(GtkPrintOperation * operation,
          GtkPrintContext * context, gint page_nr, gpointer user_data)
{
    PrintCheckDialog *pcd = (PrintCheckDialog *) user_data;
    check_format_t *format;

    format = pcd->selected_format;
    if (format)
        draw_page_format(context, page_nr, format, user_data);
    else
        draw_page_custom(context, page_nr, user_data);
}


/* Compute the number of pages required to complete this print operation.
 * Today, check printing only prints one check at a time.  When its extended to 
 * print multiple checks, this will need to take into account the number of
 * checks to print, the number of checks on a page, and the starting check
 * position on the page. This function is called once by the GtkPrint code to
 * determine the number of pages required to complete the print operation. */
static void
begin_print(GtkPrintOperation * operation,
            GtkPrintContext * context, gpointer user_data)
{
    gtk_print_operation_set_n_pages(operation, 1);
}

/********************************************************************\
 * gnc_ui_print_check_dialog_ok_cb
\********************************************************************/

static void
gnc_ui_print_check_dialog_ok_cb(PrintCheckDialog * pcd)
{
    GtkPrintOperation *print;
    GtkPrintOperationResult res;

    print = gtk_print_operation_new();

    gnc_restore_print_settings(print);
    gtk_print_operation_set_unit(print, GTK_UNIT_POINTS);
    gtk_print_operation_set_use_full_page(print, TRUE);
    g_signal_connect(print, "begin_print", G_CALLBACK(begin_print), NULL);
    g_signal_connect(print, "draw_page", G_CALLBACK(draw_page), pcd);

    res = gtk_print_operation_run(print,
                                  GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                  pcd->caller_window, NULL);

    if (res == GTK_PRINT_OPERATION_RESULT_APPLY)
        gnc_save_print_settings(print);

    g_object_unref(print);
}

#else

static void
gnc_ui_print_check_dialog_ok_cb(PrintCheckDialog * pcd)
{
  PrintSession *ps;
  check_format_t *format;

  ps = gnc_print_session_create(TRUE);

  format = pcd->selected_format;
  if (format) {
    draw_page_format(ps, format, pcd);
  } else {
    draw_page_custom(ps->context, 1, pcd);
  }

  gnc_print_session_done(ps);
  gnc_print_session_destroy(ps);
}
#endif



static void
gnc_print_check_set_sensitive (GtkWidget *widget, gpointer data)
{
  gboolean sensitive = GPOINTER_TO_INT(data);
  gtk_widget_set_sensitive(widget, sensitive);
}


void
gnc_print_check_position_changed (GtkComboBox *widget,
				  PrintCheckDialog * pcd)
{
  gboolean sensitive;
  gint value;

  value = gtk_combo_box_get_active(GTK_COMBO_BOX(pcd->position_combobox));
  if (-1 == value)
    return;
  sensitive = (value == pcd->position_max);
  gtk_widget_set_sensitive(GTK_WIDGET(pcd->translation_label), sensitive);
  gtk_widget_set_sensitive(GTK_WIDGET(pcd->translation_x), sensitive);
  gtk_widget_set_sensitive(GTK_WIDGET(pcd->translation_y), sensitive);
}

void
gnc_print_check_format_changed (GtkComboBox *widget,
                                PrintCheckDialog * pcd)
{
  GtkListStore *store;
  gboolean sensitive;
  gint fnum, pnum;
  check_format_t *format;
  GSList *elem;

  fnum = gtk_combo_box_get_active(GTK_COMBO_BOX(pcd->format_combobox));
  if (-1 == fnum)
    return;
  pnum = gtk_combo_box_get_active(GTK_COMBO_BOX(pcd->position_combobox));

  /* Update the positions combobox */
  format = g_slist_nth_data(pcd->formats_list, fnum);
  pcd->selected_format = format;
  g_signal_handlers_block_by_func(pcd->position_combobox,
                                  gnc_print_check_position_changed, pcd);
  store = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_combo_box_set_model(GTK_COMBO_BOX(pcd->position_combobox),
                          GTK_TREE_MODEL(store));
  if (format) {
    pcd->position_max = g_slist_length(format->positions); /* -1 for 0 base, +1 for custom entry */
    for (elem = format->positions; elem; elem = g_slist_next(elem)) {
      gtk_combo_box_append_text(GTK_COMBO_BOX(pcd->position_combobox), elem->data);
    }
  } else {
    pcd->position_max = 0;
  }
  gtk_combo_box_append_text(GTK_COMBO_BOX(pcd->position_combobox), _("Custom"));
  pnum = MIN(pnum, pcd->position_max);
  gtk_combo_box_set_active(GTK_COMBO_BOX(pcd->position_combobox), pnum);
  g_signal_handlers_unblock_by_func(pcd->position_combobox,
                                    gnc_print_check_position_changed, pcd);

  /* If there's only one thing in the position combobox, make it insensitive */
  sensitive = (pcd->position_max > 0);
  gtk_widget_set_sensitive(GTK_WIDGET(pcd->position_combobox), sensitive);
  
  /* Update the custom page */
  sensitive = (fnum == pcd->format_max);
  gtk_container_foreach(GTK_CONTAINER(pcd->custom_table),
			gnc_print_check_set_sensitive,
			GINT_TO_POINTER(sensitive));
  if (sensitive == TRUE)
    return;
  
  gnc_print_check_position_changed(widget, pcd);
}

void
gnc_ui_print_check_response_cb(GtkDialog * dialog,
			       gint response,
			       PrintCheckDialog *pcd)
{
  switch (response) {
    case GTK_RESPONSE_HELP:
      gnc_gnome_help(HF_HELP, HL_PRINTCHECK);
      return;

    case GTK_RESPONSE_OK:
      gnc_ui_print_check_dialog_ok_cb(pcd);
      gnc_ui_print_save_dialog(pcd);
      gnc_save_window_size(GCONF_SECTION, GTK_WINDOW(dialog));
      break;

    case GTK_RESPONSE_CANCEL:
      gnc_save_window_size(GCONF_SECTION, GTK_WINDOW(dialog));
      break;
  }

  gtk_widget_destroy(pcd->dialog);
  g_object_unref(pcd->xml);
  g_slist_foreach(pcd->formats_list, (GFunc)free_check_format, NULL);
  g_slist_free(pcd->formats_list);
  g_free(pcd);
}
