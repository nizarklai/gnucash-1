/*
 * gncEntry.h -- the Core Business Entry Interface
 * Copyright (C) 2001 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#ifndef GNC_ENTRY_H_
#define GNC_ENTRY_H_

typedef struct _gncEntry GncEntry;

#include "date.h"
#include "gncBusiness.h"
#include "gncOrder.h"
#include "gncInvoice.h"

#define GNC_ENTRY_MODULE_NAME "gncEntry"

/* How to interpret the Discount and Tax numbers..  You can interpret
 * the as a VALUE or a PERCENT.  Similarly, you can set the PRETAX
 * bit if you want the discount to be applied before a percentage-tax.
 */
#define GNC_ENTRY_INTERP_VALUE		0x00
#define GNC_ENTRY_INTERP_PERCENT	0x01
#define GNC_ENTRY_INTERP_PRETAX		0x02

#define GNC_ENTRY_INTERP_IS_VALUE(x)	(((x)&0x01) == GNC_ENTRY_INTERP_VALUE)
#define GNC_ENTRY_INTERP_IS_PERCENT(x)	(((x)&0x01) == GNC_ENTRY_INTERP_PERCENT)

/* Create/Destroy Functions */

GncEntry *gncEntryCreate (GncBusiness *business);
void gncEntryDestroy (GncEntry *entry);

/* Set Functions */

void gncEntrySetDate (GncEntry *entry, Timespec *date);
void gncEntrySetDescription (GncEntry *entry, const char *desc);
void gncEntrySetAction (GncEntry *entry, const char *action);
void gncEntrySetQuantity (GncEntry *entry, gnc_numeric quantity);
void gncEntrySetPrice (GncEntry *entry, gnc_numeric price);
void gncEntrySetTax (GncEntry *entry, gnc_numeric tax);
void gncEntrySetTaxType (GncEntry *entry, gint type);
void gncEntrySetDiscount (GncEntry *entry, gnc_numeric discount);
void gncEntrySetDiscountType (GncEntry *entry, gint type);

void gncEntrySetAccount (GncEntry *entry, Account *acc);
void gncEntrySetTaxAccount (GncEntry *entry, Account *acc);

/* Get Functions */

GncBusiness * gncEntryGetBusiness (GncEntry *entry);
const GUID * gncEntryGetGUID (GncEntry *entry);
Timespec gncEntryGetDate (GncEntry *entry);
const char * gncEntryGetDescription (GncEntry *entry);
const char * gncEntryGetAction (GncEntry *entry);
gnc_numeric gncEntryGetQuantity (GncEntry *entry);
gnc_numeric gncEntryGetPrice (GncEntry *entry);
gnc_numeric gncEntryGetTax (GncEntry *entry);
gint gncEntryGetTaxType (GncEntry *entry);
gnc_numeric gncEntryGetDiscount (GncEntry *entry);
gint gncEntryGetDiscountType (GncEntry *entry);


Account * gncEntryGetAccount (GncEntry *entry);
Account * gncEntryGetTaxAccount (GncEntry *entry);

GncOrder * gncEntryGetOrder (GncEntry *entry);
GncInvoice * gncEntryGetInvoice (GncEntry *entry);


void gncEntryCommitEdit (GncEntry *entry);

#endif /* GNC_ENTRY_H_ */
