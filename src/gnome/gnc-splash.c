/********************************************************************\
 * gnc-splash.c -- splash screen for GnuCash                        *
 * Copyright (C) 2001 Gnumatic, Inc.                                *
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

#include <gnome.h>

#include "dialog-utils.h"
#include "gnc-splash.h"
#include "gnc-version.h"


static GtkWidget * splash = NULL;
static GtkWidget * progress = NULL;


static void
splash_destroy_cb (GtkObject *object, gpointer user_data)
{
  splash = NULL;
}

void
gnc_show_splash_screen (void)
{
  GtkWidget *pixmap;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *version;
  GtkWidget *separator;
  gchar ver_string[50];

  if (splash) return;

  splash = gtk_window_new (GTK_WINDOW_DIALOG);

  gtk_signal_connect (GTK_OBJECT (splash), "destroy",
                      GTK_SIGNAL_FUNC (splash_destroy_cb), NULL);

  gtk_window_set_title (GTK_WINDOW (splash), "GnuCash");
  gtk_window_set_position (GTK_WINDOW (splash), GTK_WIN_POS_CENTER);

  pixmap = gnc_get_pixmap ("gnucash_splash.png");

  if (!pixmap)
  {
    g_warning ("can't find splash pixmap");
    gtk_widget_destroy (splash);
    return;
  }

  frame = gtk_frame_new (NULL);
  vbox = gtk_vbox_new (FALSE, 3);
#ifdef GNUCASH_CVS
  sprintf(ver_string, _("Version: Gnucash-%s cvs (built %s)"),
	  VERSION, GNUCASH_BUILD_DATE);
#else
  sprintf(ver_string, _("Version: Gnucash-%s"), VERSION);
#endif

  version = gtk_label_new (ver_string);
  separator = gtk_hseparator_new();
  progress = gtk_label_new(_("Loading..."));

  gtk_container_add (GTK_CONTAINER (frame), pixmap);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), version, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), progress, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (splash), vbox);

  gtk_widget_show_all (splash);

  /* make sure splash is up */
  while (gtk_events_pending ())
    gtk_main_iteration ();
}

void
gnc_destroy_splash_screen (void)
{
  if (splash)
  {
    gtk_widget_destroy (splash);
    progress = NULL;
    splash = NULL;
  }
}

void
gnc_update_splash_screen (const gchar *string)
{
  if (progress)
  {
    gtk_label_set_text (GTK_LABEL(progress), string);

    /* make sure new text is up */
    while (gtk_events_pending ())
      gtk_main_iteration ();
  }
}
