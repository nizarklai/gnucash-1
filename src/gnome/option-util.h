/********************************************************************\
 * option-util.h -- GNOME<->guile option interface                  *
 * Copyright (C) 1998,1999 Linas Vepstas                            *
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
\********************************************************************/

#ifndef __OPTION_UTIL_H__
#define __OPTION_UTIL_H__

#include <gnome.h>
#include <guile/gh.h>


typedef struct _GNCOption GNCOption;
struct _GNCOption
{
  /* Handle to the scheme-side option */
  SCM guile_option;

  /* Identifier for unregistering */
  SCM guile_option_id;

  /* Flag to indicate change by the UI */
  gboolean changed;

  /* The widget which is holding this option */
  GtkWidget *widget;
};

typedef struct _GNCOptionSection GNCOptionSection;
struct _GNCOptionSection
{
  char * section_name;

  GSList * options;
};


/***** Prototypes ********************************************************/

void gnc_options_init();
void gnc_options_shutdown();

void gnc_register_option_change_callback(void (*callback) (void));

char * gnc_option_section(GNCOption *option);
char * gnc_option_name(GNCOption *option);
char * gnc_option_type(GNCOption *option);
char * gnc_option_sort_tag(GNCOption *option);
char * gnc_option_documentation(GNCOption *option);
SCM    gnc_option_getter(GNCOption *option);
SCM    gnc_option_setter(GNCOption *option);
SCM    gnc_option_default_getter(GNCOption *option);
SCM    gnc_option_value_validator(GNCOption *option);

guint gnc_num_option_sections();
guint gnc_option_section_num_options(GNCOptionSection *section);

GNCOptionSection * gnc_get_option_section(gint i);

GNCOption * gnc_get_option_section_option(GNCOptionSection *section, int i);
GNCOption * gnc_get_option_by_name(char *section_name, char *name);
GNCOption * gnc_get_option_by_SCM(SCM guile_option);

gboolean gnc_options_dirty();
void     gnc_options_clean();

void gnc_options_commit();

gboolean gnc_lookup_boolean_option(char *section, char *name,
				   gboolean default_value);
char * gnc_lookup_string_option(char *section, char *name,
				char *default_value);


/* private */

void _gnc_register_option_ui(SCM guile_option);


#endif /* __OPTION_UTIL_H__ */
