/********************************************************************\
 * qofinstance-p.h -- private fields common to all object instances *
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
 * Object instance holds many common fields that most
 * gnucash objects use.
 * 
 * Copyright (C) 2003 Linas Vepstas <linas@linas.org>
 */

/** @addtogroup Object
    @{ */
/** @addtogroup Object_Private
    Private interfaces, not meant to be used by applications.
    @{ */
/** @name  Instance_Private
    @{ */

#ifndef QOF_INSTANCE_P_H
#define QOF_INSTANCE_P_H

#include "qofinstance.h"

/**
 * UNDER CONSTRUCTION!
 * This is mostly scaffolding for now,
 * eventually, it may hold more fields, such as refrence counting...
 *
 */
struct QofInstance_s
{
   /** Globally unique id identifying this instance */
	QofEntity entity;

   /** The entity_table in which this instance is stored */
   QofBook * book;

  /** kvp_data is a key-value pair database for storing arbirtary
   * information associated with this instance.  
   * See src/engine/kvp_doc.txt for a list and description of the 
   * important keys. */
   KvpFrame *kvp_data;

   /** Keep track of nesting level of begin/end edit calls */
   int    editlevel;

   /** In process of being destroyed */
   gboolean  do_free;

   /** This instance has not been saved yet */
   gboolean  dirty;
};

/** reset the dirty flag */
void qof_instance_mark_clean (QofInstance *);

void qof_instance_set_slots (QofInstance *, KvpFrame *);

/* @} */
/* @} */
/* @} */

#endif /* QOF_INSTANCE_P_H */
