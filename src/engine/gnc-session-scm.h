/********************************************************************\
 * gnc-session-scm.h -- session access (connection to backend)      *
 *    Scheme specific code.                                         *
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
 *                                                                  *
\********************************************************************/

/*
 * FILE:
 * gnc-session-scm.h
 *
 * FUNCTION:
 * Encapsulates a connection to a GnuCash backend.  That is, it
 * manages the connection to a persistant data store; whereas
 * the backend is the thing that performs the actual datastore 
 * access.
 *
 * HISTORY:
 * Created by David Hampton, September 2002
 * Copyright (c) 2002 Linas Vepstas <linas@linas.org>
 */

#ifndef GNC_SESSION_SCM_H
#define GNC_SESSION_SCM_H

#include <libguile.h>
#include "guile-mappings.h"

void gnc_session_scm_load (GNCSession *session);
gboolean gnc_session_scm_export (GNCSession *tmp_session,
				 GNCSession *real_session);
void     gnc_session_scm_save (GNCSession *session);
void gnc_session_scm_set_callback (SCM percentage_cb);

#endif /* GNC_SESSION_SCM_H */
