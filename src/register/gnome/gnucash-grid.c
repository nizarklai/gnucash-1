/********************************************************************\
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
 * The Gnucash Grid Canvas Item
 *
 *  Based heavily (i.e., cut and pasted from) on the Gnumeric ItemGrid.
 *
 * Author:
 *     Heath Martin <martinh@pegasus.cc.ucf.edu>
 */

#include "gnucash-sheet.h"
#include "gnucash-grid.h"
#include "gnucash-color.h"
#include "gnucash-style.h"

static GnomeCanvasItem *gnucash_grid_parent_class;

/* Our arguments */
enum {
        ARG_0,
        ARG_SHEET
};


static void
gnucash_grid_realize (GnomeCanvasItem *item)
{
        GdkWindow *window;
        GnucashGrid *gnucash_grid;
        GdkGC *gc;

        if (GNOME_CANVAS_ITEM_CLASS (gnucash_grid_parent_class)->realize)
                (GNOME_CANVAS_ITEM_CLASS
		 (gnucash_grid_parent_class)->realize)(item);

        gnucash_grid = GNUCASH_GRID (item);
        window = GTK_WIDGET (item->canvas)->window;

        /* Configure the default grid gc */
        gnucash_grid->grid_gc = gc = gdk_gc_new (window);
        gnucash_grid->fill_gc = gdk_gc_new (window);
        gnucash_grid->gc = gdk_gc_new (window);

        /* Allocate the default colors */
        gnucash_grid->background = gn_white;
        gnucash_grid->grid_color = gn_black;
        gnucash_grid->default_color = gn_black;

        gdk_gc_set_foreground (gc, &gnucash_grid->grid_color);
        gdk_gc_set_background (gc, &gnucash_grid->background);

        gdk_gc_set_foreground (gnucash_grid->fill_gc,
			       &gnucash_grid->background);
        gdk_gc_set_background (gnucash_grid->fill_gc,
			       &gnucash_grid->grid_color);
}


static void
gnucash_grid_unrealize (GnomeCanvasItem *item)
{
        GnucashGrid *gnucash_grid = GNUCASH_GRID (item);

	if (gnucash_grid->grid_gc != NULL) {
		gdk_gc_unref(gnucash_grid->grid_gc);
		gnucash_grid->grid_gc = NULL;
	}

	if (gnucash_grid->fill_gc != NULL) {
		gdk_gc_unref(gnucash_grid->fill_gc);
		gnucash_grid->fill_gc = NULL;
	}

	if (gnucash_grid->gc != NULL) {
		gdk_gc_unref(gnucash_grid->gc);
		gnucash_grid->gc = NULL;
	}

        if (GNOME_CANVAS_ITEM_CLASS (gnucash_grid_parent_class)->unrealize)
                (*GNOME_CANVAS_ITEM_CLASS
		 (gnucash_grid_parent_class)->unrealize)(item);
}


static void
gnucash_grid_update (GnomeCanvasItem *item, double *affine,
		     ArtSVP *clip_path, int flags)
{
        if (GNOME_CANVAS_ITEM_CLASS (gnucash_grid_parent_class)->update)
                (* GNOME_CANVAS_ITEM_CLASS (gnucash_grid_parent_class)->update)
			(item, affine, clip_path, flags);

        item->x1 = 0;
        item->y1 = 0;
        item->x2 = INT_MAX/2 -1;
        item->y2 = INT_MAX/2 -1;

        gnome_canvas_group_child_bounds (GNOME_CANVAS_GROUP (item->parent),
					 item);
}


/*
 * Sets virt_row, virt_col to the block coordinates for the
 * block containing pixel (x, y).  Also sets o_x, o_y, to the
 * pixel coordinates of the origin of the block.  Returns
 * TRUE if a block is found, FALSE if (x,y) is not
 * in any block.
 *
 * All coordinates are with respect to the canvas origin.
 */
static SheetBlock *
gnucash_grid_find_block_by_pixel (GnucashGrid *grid,
                                  gint x, gint y,
                                  VirtualCellLocation *vcell_loc)
{
        SheetBlock *block;
        VirtualCellLocation vc_loc = { 1, 0 };

        g_return_val_if_fail(y >= 0, NULL);
        g_return_val_if_fail(x >= 0, NULL);

        do {
                block = gnucash_sheet_get_block (grid->sheet, vc_loc);
                if (!block)
                        return NULL;

                if (block->visible &&
                    y >= block->origin_y &&
                    y < block->origin_y + block->style->dimensions->height) {
                        if (vcell_loc)
                                vcell_loc->virt_row = vc_loc.virt_row;
                        break;
                }
                vc_loc.virt_row++;
        } while (vc_loc.virt_row < grid->sheet->num_virt_rows);

        if (vc_loc.virt_row == grid->sheet->num_virt_rows)
                return NULL;

        do {
                block = gnucash_sheet_get_block (grid->sheet, vc_loc);
                if (!block)
                        return NULL;

                if (block->visible &&
                    x >= block->origin_x &&
                    x < block->origin_x + block->style->dimensions->width) {
                        if (vcell_loc)
                                vcell_loc->virt_col = vc_loc.virt_col;
                        break;
                }
                vc_loc.virt_col++;
        } while (vc_loc.virt_col < grid->sheet->num_virt_cols);

        if (vc_loc.virt_col == grid->sheet->num_virt_cols)
                return NULL;

        return block;
}

static gboolean
gnucash_grid_find_cell_by_pixel (GnucashGrid *grid, gint x, gint y,
                                 VirtualLocation *virt_loc)
{
        SheetBlock *block;
        SheetBlockStyle *style;
        CellDimensions *cd;
        gint row = 0;
        gint col = 0;

        g_return_val_if_fail (virt_loc != NULL, FALSE);

        block = gnucash_sheet_get_block (grid->sheet, virt_loc->vcell_loc);
        if (block == NULL)
                return FALSE;

        /* now make x, y relative to the block origin */
        x -= block->origin_x;
        y -= block->origin_y;

        style = block->style;
        if (style == NULL)
                return FALSE;

        do {
                cd = gnucash_style_get_cell_dimensions (style, row, 0);

                if (y >= cd->origin_y && y < cd->origin_y + cd->pixel_height)
                        break;

                row++;
        } while (row < style->nrows);

        if (row == style->nrows)
                return FALSE;

        do {
                cd = gnucash_style_get_cell_dimensions (style, row, col);

                if (x >= cd->origin_x && x < cd->origin_x + cd->pixel_width) 
                        break;

                col++;
        } while (col < style->ncols);

        if (col == style->ncols)
                return FALSE;

        if (virt_loc)
                virt_loc->phys_row_offset = row;
        if (virt_loc)
                virt_loc->phys_col_offset = col;

        return TRUE;
}

gboolean
gnucash_grid_find_loc_by_pixel (GnucashGrid *grid, gint x, gint y,
                                VirtualLocation *virt_loc)
{
        SheetBlock *block;

        if (virt_loc == NULL)
                return FALSE;

        block = gnucash_grid_find_block_by_pixel (grid, x, y,
                                                  &virt_loc->vcell_loc);
        if (block == NULL)
                return FALSE;

        return gnucash_grid_find_cell_by_pixel (grid, x, y, virt_loc);
}


static void
draw_cell (GnucashGrid *grid,
           SheetBlock *block,
           VirtualLocation virt_loc,
           GdkDrawable *drawable,
           int x, int y, int width, int height)
{
        Table *table = grid->sheet->table;
        const char *text;
        GdkFont *font;
        GdkColor *bg_color;
        GdkColor *fg_color;
        gint x_offset, y_offset;
        GdkRectangle rect;
        guint32 argb;
        gint borders;

        gdk_gc_set_background (grid->gc, &gn_white);

        argb = gnc_table_get_bg_color (table, virt_loc);
        bg_color = gnucash_color_argb_to_gdk (argb);

        gdk_gc_set_foreground (grid->gc, bg_color);
        gdk_draw_rectangle (drawable, grid->gc, TRUE, x, y, width, height);

        gdk_gc_set_foreground (grid->gc, &gn_black);

        borders = gnucash_sheet_get_borders (grid->sheet, virt_loc);
        /* top */
        if (borders & STYLE_BORDER_TOP)
                gdk_draw_line (drawable, grid->gc, x, y, x+width, y);

        /* right */
        if (borders & STYLE_BORDER_RIGHT)
                gdk_draw_line (drawable, grid->gc, x+width, y,
                               x+width, y+height);

        /* bottom */
        if (borders & STYLE_BORDER_BOTTOM)
                gdk_draw_line (drawable, grid->gc, x+width,
                               y+height, x, y+height);

        /* left */
        if (borders & STYLE_BORDER_LEFT)
                gdk_draw_line (drawable, grid->gc, x, y+height, x, y);

        /* dividing line */
        if ((virt_loc.phys_row_offset == 0) && (table->dividing_row >= 0))
        {
                if (virt_loc.vcell_loc.virt_row == table->dividing_row)
                {
                        gdk_gc_set_foreground (grid->gc, &gn_blue);
                        gdk_draw_line (drawable, grid->gc, x, y, x + width, y);
                }
        }

        if ((virt_loc.phys_row_offset == (block->style->nrows - 1)) &&
            (table->dividing_row >= 0))
        {
                if (virt_loc.vcell_loc.virt_row == (table->dividing_row - 1))
                {
                        gdk_gc_set_foreground (grid->gc, &gn_blue);
                        gdk_draw_line (drawable, grid->gc, x, y + height,
                                       x + width, y + height);
                }
        }

        text = gnc_table_get_entry (table, virt_loc);

        font = grid->normal_font;

        argb = gnc_table_get_fg_color (table, virt_loc);
        fg_color = gnucash_color_argb_to_gdk (argb);

        gdk_gc_set_foreground (grid->gc, fg_color);

        if ((table->current_cursor_loc.vcell_loc.virt_row ==
             virt_loc.vcell_loc.virt_row) &&
	    (!text || strlen(text) == 0)) {
                font = grid->italic_font;
                gdk_gc_set_foreground (grid->gc, &gn_light_gray);
                text = gnc_table_get_label (table, virt_loc);
        }

        if ((text == NULL) || (*text == '\0'))
                return;

        y_offset = height - MAX(CELL_VPADDING, font->descent + 4);

        switch (gnc_table_get_align (table, virt_loc)) {
                default:
                case CELL_ALIGN_LEFT:
                        x_offset = CELL_HPADDING;
                        break;
                case CELL_ALIGN_RIGHT:
                        x_offset = width - CELL_HPADDING
                                - gdk_string_measure (font, text);
                        break;
                case CELL_ALIGN_CENTER:
                        if (width < gdk_string_measure (font, text))
                                x_offset = CELL_HPADDING;
                        else {
                                x_offset = width / 2;
                                x_offset -= gdk_string_measure (font, text) / 2;
                        }
                        break;
                }

        rect.x = x + CELL_HPADDING;
        rect.y = y + CELL_VPADDING;
        rect.width = width - 2*CELL_HPADDING;
        rect.height = height;

        gdk_gc_set_clip_rectangle (grid->gc, &rect);

        gdk_draw_string (drawable,
                         font,
                         grid->gc,
                         x + x_offset,
                         y + y_offset,
                         text);

        gdk_gc_set_clip_rectangle (grid->gc, NULL);
}

static void
draw_block (GnucashGrid *grid,
            SheetBlock *block,
            VirtualLocation virt_loc,
            GdkDrawable *drawable,
            int x, int y, int width, int height)
{
        CellDimensions *cd;
        gint x_paint;
        gint y_paint;
        gint w, h;

        virt_loc.phys_row_offset = 0;
        virt_loc.phys_col_offset = 0;

        for ( ; virt_loc.phys_row_offset < block->style->nrows ;
              virt_loc.phys_row_offset++ )
        {
                for ( ; virt_loc.phys_col_offset < block->style->ncols ;
                      virt_loc.phys_col_offset++ )
                {
                        cd = gnucash_style_get_cell_dimensions
                                (block->style,
                                 virt_loc.phys_row_offset,
                                 virt_loc.phys_col_offset);

                        x_paint = block->origin_x + cd->origin_x;
                        if (x_paint > x + width)
                                break;

                        y_paint = block->origin_y + cd->origin_y;
                        if (y_paint > y + height)
                                return;

                        h = cd->pixel_height;
                        w = cd->pixel_width;

                        if (x_paint + w < x)
                                continue;

                        if (y_paint + h < y)
                                continue;

                        draw_cell (grid, block, virt_loc, drawable,
                                   x_paint - x, y_paint - y, w, h);
                }
        }
}

/* FIXME:  this only does the first virtual column */
static void
gnucash_grid_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
                   int x, int y, int width, int height)
{
        GnucashGrid *grid = GNUCASH_GRID (item);
        VirtualLocation virt_loc;
        SheetBlock *sheet_block;

        g_return_if_fail(x >= 0);
        g_return_if_fail(y >= 0);

        /* The default background */
        gdk_draw_rectangle (drawable, grid->fill_gc, TRUE,
                            0, 0, width, height);

        /* compute our initial values where we start drawing */
        sheet_block = gnucash_grid_find_block_by_pixel (grid, x, y,
                                                        &virt_loc.vcell_loc);
        if (!sheet_block || !sheet_block->style)
                return;

        for ( ; virt_loc.vcell_loc.virt_row < grid->sheet->num_virt_rows;
              virt_loc.vcell_loc.virt_row++ )
        {
                while (TRUE)
                {
                        sheet_block = gnucash_sheet_get_block
                                (grid->sheet, virt_loc.vcell_loc);

                        if (!sheet_block || !sheet_block->style)
                                return;

                        if (sheet_block->visible)
                                break;

                        virt_loc.vcell_loc.virt_row++;
                }

                if (y + height < sheet_block->origin_y)
                        return;

                draw_block (grid, sheet_block, virt_loc, drawable,
                            x, y, width, height);
        }
}


static void
gnucash_grid_destroy (GtkObject *object)
{
	GNUCASH_GRID(object);

        if (GTK_OBJECT_CLASS (gnucash_grid_parent_class)->destroy)
                (*GTK_OBJECT_CLASS
		 (gnucash_grid_parent_class)->destroy)(object);
}


static void
gnucash_grid_init (GnucashGrid *grid)
{
        GnomeCanvasItem *item = GNOME_CANVAS_ITEM (grid);

        item->x1 = 0;
        item->y1 = 0;
        item->x2 = 0;
        item->y2 = 0;

        grid->top_block  = 0;
        grid->top_offset = 0;
        grid->left_offset = 0;

        grid->normal_font = gnucash_register_font;
        grid->italic_font = gnucash_register_hint_font;
}


static void
gnucash_grid_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
        GnomeCanvasItem *item;
        GnucashGrid *grid;

        item = GNOME_CANVAS_ITEM (o);
        grid = GNUCASH_GRID (o);

        switch (arg_id){
        case ARG_SHEET:
                grid->sheet = GTK_VALUE_POINTER (*arg);
                break;
        }
}


static void
gnucash_grid_class_init (GnucashGridClass *grid_class)
{
        GtkObjectClass  *object_class;
        GnomeCanvasItemClass *item_class;

        gnucash_grid_parent_class =
		gtk_type_class (gnome_canvas_item_get_type());

        object_class = (GtkObjectClass *) grid_class;
        item_class = (GnomeCanvasItemClass *) grid_class;

        gtk_object_add_arg_type ("GnucashGrid::Sheet", GTK_TYPE_POINTER,
                                 GTK_ARG_WRITABLE, ARG_SHEET);

        object_class->set_arg = gnucash_grid_set_arg;
        object_class->destroy = gnucash_grid_destroy;

        /* GnomeCanvasItem method overrides */
        item_class->update      = gnucash_grid_update;
        item_class->realize     = gnucash_grid_realize;
        item_class->unrealize   = gnucash_grid_unrealize;
        item_class->draw        = gnucash_grid_draw;
}


GtkType
gnucash_grid_get_type (void)
{
        static GtkType gnucash_grid_type = 0;

        if (!gnucash_grid_type) {
                GtkTypeInfo gnucash_grid_info = {
                        "GnucashGrid",
                        sizeof (GnucashGrid),
                        sizeof (GnucashGridClass),
                        (GtkClassInitFunc) gnucash_grid_class_init,
                        (GtkObjectInitFunc) gnucash_grid_init,
                        NULL, /* reserved_1 */
                        NULL, /* reserved_2 */
                        (GtkClassInitFunc) NULL
                };

                gnucash_grid_type =
			gtk_type_unique (gnome_canvas_item_get_type (),
					 &gnucash_grid_info);
        }

        return gnucash_grid_type;
}


/*
  Local Variables:
  c-basic-offset: 8
  End:
*/
