/*
 * gncVendorP.h -- the Core Vendor Interface
 * Copyright (C) 2001 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#ifndef GNC_VENDORP_H_
#define GNC_VENDORP_H_

#include "gncBusiness.h"
#include "gncVendor.h"

gboolean gncVendorRegister (void);
gint gncVendorNextID (GncBusiness *business);
void gncVendorSetGUID (GncVendor *vendor, const GUID *guid);

#endif /* GNC_VENDORP_H_ */
