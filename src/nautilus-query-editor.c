/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#include <config.h>
#include "nautilus-query-editor.h"
#include "nautilus-file-utilities.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <eel/eel-glib-extensions.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

typedef enum {
	NAUTILUS_QUERY_EDITOR_ROW_TYPE,
	
	NAUTILUS_QUERY_EDITOR_ROW_LAST
} NautilusQueryEditorRowType;

typedef struct {
	NautilusQueryEditorRowType type;
	NautilusQueryEditor *editor;
	GtkWidget *hbox;
	GtkWidget *combo;

	GtkWidget *type_widget;
	
	void *data;
} NautilusQueryEditorRow;


typedef struct {
	const char *name;
	GtkWidget * (*create_widgets)      (NautilusQueryEditorRow *row);
	void        (*add_to_query)        (NautilusQueryEditorRow *row,
					    NautilusQuery          *query);
	void        (*free_data)           (NautilusQueryEditorRow *row);
	void        (*add_rows_from_query) (NautilusQueryEditor *editor,
					    NautilusQuery *query);
} NautilusQueryEditorRowOps;

struct NautilusQueryEditorDetails {
	GtkWidget *entry;
	gboolean change_frozen;
	guint typing_timeout_id;
	gboolean is_visible;
	GtkWidget *vbox;

	GtkWidget *search_current_button;
	GtkWidget *search_all_button;
	char *current_uri;

	GList *rows;
	char *last_set_query_text;
	gboolean got_preedit;
};

enum {
	CHANGED,
	CANCEL,
	LAST_SIGNAL
}; 

static guint signals[LAST_SIGNAL];

static void entry_activate_cb (GtkWidget *entry, NautilusQueryEditor *editor);
static void entry_changed_cb  (GtkWidget *entry, NautilusQueryEditor *editor);
static void nautilus_query_editor_changed_force (NautilusQueryEditor *editor,
						 gboolean             force);
static void nautilus_query_editor_changed (NautilusQueryEditor *editor);
static NautilusQueryEditorRow * nautilus_query_editor_add_row (NautilusQueryEditor *editor,
							       NautilusQueryEditorRowType type);

static GtkWidget *type_row_create_widgets      (NautilusQueryEditorRow *row);
static void       type_row_add_to_query        (NautilusQueryEditorRow *row,
					        NautilusQuery          *query);
static void       type_row_free_data           (NautilusQueryEditorRow *row);
static void       type_add_rows_from_query     (NautilusQueryEditor    *editor,
					        NautilusQuery          *query);



static NautilusQueryEditorRowOps row_type[] = {
	{ N_("File Type"),
	  type_row_create_widgets,
	  type_row_add_to_query,
	  type_row_free_data,
	  type_add_rows_from_query
	},
};

G_DEFINE_TYPE (NautilusQueryEditor, nautilus_query_editor, GTK_TYPE_BOX);

/* taken from gtk/gtktreeview.c */
static void
send_focus_change (GtkWidget *widget,
                   GdkDevice *device,
		   gboolean   in)
{
	GdkDeviceManager *device_manager;
	GList *devices, *d;

	device_manager = gdk_display_get_device_manager (gtk_widget_get_display (widget));
	devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);
	devices = g_list_concat (devices, gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_SLAVE));
	devices = g_list_concat (devices, gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_FLOATING));

	for (d = devices; d; d = d->next) {
		GdkDevice *dev = d->data;
		GdkEvent *fevent;
		GdkWindow *window;

		if (gdk_device_get_source (dev) != GDK_SOURCE_KEYBOARD)
			continue;

		window = gtk_widget_get_window (widget);

		/* Skip non-master keyboards that haven't
		 * selected for events from this window
		 */
		if (gdk_device_get_device_type (dev) != GDK_DEVICE_TYPE_MASTER &&
		    !gdk_window_get_device_events (window, dev))
			continue;

		fevent = gdk_event_new (GDK_FOCUS_CHANGE);

		fevent->focus_change.type = GDK_FOCUS_CHANGE;
		fevent->focus_change.window = g_object_ref (window);
		fevent->focus_change.in = in;
		gdk_event_set_device (fevent, device);

		gtk_widget_send_focus_change (widget, fevent);

		gdk_event_free (fevent);
	}

	g_list_free (devices);
}

static void
entry_focus_hack (GtkWidget *entry,
		  GdkDevice *device)
{
	GtkEntryClass *entry_class;
	GtkWidgetClass *entry_parent_class;

	/* Grab focus will select all the text.  We don't want that to happen, so we
	 * call the parent instance and bypass the selection change.  This is probably
	 * really non-kosher. */
	entry_class = g_type_class_peek (GTK_TYPE_ENTRY);
	entry_parent_class = g_type_class_peek_parent (entry_class);
	(entry_parent_class->grab_focus) (entry);

	/* send focus-in event */
	send_focus_change (entry, device, TRUE);
}

static void
entry_preedit_changed_cb (GtkEntry            *entry,
			  gchar               *preedit,
			  NautilusQueryEditor *editor)
{
	editor->details->got_preedit = TRUE;
}

gboolean
nautilus_query_editor_handle_event (NautilusQueryEditor *editor,
				    GdkEventKey         *event)
{
	GdkEvent *new_event;
	gboolean handled = FALSE;
	gulong id;
	gboolean retval;
	gboolean text_changed;
	char *old_text;
	const char *new_text;

	editor->details->got_preedit = FALSE;

	old_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (editor->details->entry)));

	id = g_signal_connect (editor->details->entry, "preedit-changed",
			       G_CALLBACK (entry_preedit_changed_cb), editor);

	new_event = gdk_event_copy ((GdkEvent *) event);
	((GdkEventKey *) new_event)->window = gtk_widget_get_window (editor->details->entry);

	gtk_widget_realize (editor->details->entry);
	retval = gtk_widget_event (editor->details->entry, new_event);
	gdk_event_free (new_event);

	g_signal_handler_disconnect (editor->details->entry, id);

	new_text = gtk_entry_get_text (GTK_ENTRY (editor->details->entry));
	text_changed = strcmp (old_text, new_text) != 0;
	g_free (old_text);

	handled = (editor->details->got_preedit
		   || (retval && text_changed));

	editor->details->got_preedit = FALSE;

	return handled;
}

static void
nautilus_query_editor_dispose (GObject *object)
{
	NautilusQueryEditor *editor;

	editor = NAUTILUS_QUERY_EDITOR (object);

	if (editor->details->typing_timeout_id > 0) {
		g_source_remove (editor->details->typing_timeout_id);
		editor->details->typing_timeout_id = 0;
	}

	G_OBJECT_CLASS (nautilus_query_editor_parent_class)->dispose (object);
}

static gboolean
nautilus_query_editor_draw (GtkWidget *widget,
			    cairo_t *cr)
{
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (widget);

	gtk_render_background (context, cr, 0, 0,
			       gtk_widget_get_allocated_width (widget),
			       gtk_widget_get_allocated_height (widget));

	gtk_render_frame (context, cr, 0, 0,
			  gtk_widget_get_allocated_width (widget),
			  gtk_widget_get_allocated_height (widget));

	GTK_WIDGET_CLASS (nautilus_query_editor_parent_class)->draw (widget, cr);

	return FALSE;
}

static void
nautilus_query_editor_class_init (NautilusQueryEditorClass *class)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;
	GtkBindingSet *binding_set;

	gobject_class = G_OBJECT_CLASS (class);
        gobject_class->dispose = nautilus_query_editor_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->draw = nautilus_query_editor_draw;

	signals[CHANGED] =
		g_signal_new ("changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusQueryEditorClass, changed),
		              NULL, NULL,
		              g_cclosure_marshal_generic,
		              G_TYPE_NONE, 2, NAUTILUS_TYPE_QUERY, G_TYPE_BOOLEAN);

	signals[CANCEL] =
		g_signal_new ("cancel",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (NautilusQueryEditorClass, cancel),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "cancel", 0);

	g_type_class_add_private (class, sizeof (NautilusQueryEditorDetails));
}

GFile *
nautilus_query_editor_get_location (NautilusQueryEditor *editor)
{
	GFile *file = NULL;
	if (editor->details->current_uri != NULL)
		file = g_file_new_for_uri (editor->details->current_uri);
	return file;
}

static void
entry_activate_cb (GtkWidget *entry, NautilusQueryEditor *editor)
{
	if (editor->details->typing_timeout_id > 0) {
		g_source_remove (editor->details->typing_timeout_id);
		editor->details->typing_timeout_id = 0;
	}

	nautilus_query_editor_changed_force (editor, TRUE);
}

static gboolean
typing_timeout_cb (gpointer user_data)
{
	NautilusQueryEditor *editor;

	editor = NAUTILUS_QUERY_EDITOR (user_data);
	editor->details->typing_timeout_id = 0;

	nautilus_query_editor_changed (editor);

	return FALSE;
}

#define TYPING_TIMEOUT 750

static void
entry_changed_cb (GtkWidget *entry, NautilusQueryEditor *editor)
{
	if (editor->details->change_frozen) {
		return;
	}

	if (editor->details->typing_timeout_id > 0) {
		g_source_remove (editor->details->typing_timeout_id);
	}

	editor->details->typing_timeout_id =
		g_timeout_add (TYPING_TIMEOUT,
			       typing_timeout_cb,
			       editor);
}

/* Type */

static gboolean
type_separator_func (GtkTreeModel      *model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
	char *text;
	gboolean res;
	
	gtk_tree_model_get (model, iter, 0, &text, -1);

	res = text != NULL && strcmp (text, "---") == 0;
	
	g_free (text);
	return res;
}

struct {
	char *name;
	char *mimetypes[20];
} mime_type_groups[] = {
	{ N_("Documents"),
	  { "application/rtf",
	    "application/msword",
	    "application/vnd.sun.xml.writer",
	    "application/vnd.sun.xml.writer.global",
	    "application/vnd.sun.xml.writer.template",
	    "application/vnd.oasis.opendocument.text",
	    "application/vnd.oasis.opendocument.text-template",
	    "application/x-abiword",
	    "application/x-applix-word",
	    "application/x-mswrite",
	    "application/docbook+xml",
	    "application/x-kword",
	    "application/x-kword-crypt",
	    "application/x-lyx",
	    NULL
	  }
	},
	{ N_("Music"),
	  { "application/ogg",
	    "audio/x-vorbis+ogg",
	    "audio/ac3",
	    "audio/basic",
	    "audio/midi",
	    "audio/x-flac",
	    "audio/mp4",
	    "audio/mpeg",
	    "audio/x-mpeg",
	    "audio/x-ms-asx",
	    "audio/x-pn-realaudio",
	    NULL
	  }
	},
	{ N_("Video"),
	  { "video/mp4",
	    "video/3gpp",
	    "video/mpeg",
	    "video/quicktime",
	    "video/vivo",
	    "video/x-avi",
	    "video/x-mng",
	    "video/x-ms-asf",
	    "video/x-ms-wmv",
	    "video/x-msvideo",
	    "video/x-nsv",
	    "video/x-real-video",
	    NULL
	  }
	},
	{ N_("Picture"),
	  { "application/vnd.oasis.opendocument.image",
	    "application/x-krita",
	    "image/bmp",
	    "image/cgm",
	    "image/gif",
	    "image/jpeg",
	    "image/jpeg2000",
	    "image/png",
	    "image/svg+xml",
	    "image/tiff",
	    "image/x-compressed-xcf",
	    "image/x-pcx",
	    "image/x-photo-cd",
	    "image/x-psd",
	    "image/x-tga",
	    "image/x-xcf",
	    NULL
	  }
	},
	{ N_("Illustration"),
	  { "application/illustrator",
	    "application/vnd.corel-draw",
	    "application/vnd.stardivision.draw",
	    "application/vnd.oasis.opendocument.graphics",
	    "application/x-dia-diagram",
	    "application/x-karbon",
	    "application/x-killustrator",
	    "application/x-kivio",
	    "application/x-kontour",
	    "application/x-wpg",
	    NULL
	  }
	},
	{ N_("Spreadsheet"),
	  { "application/vnd.lotus-1-2-3",
	    "application/vnd.ms-excel",
	    "application/vnd.stardivision.calc",
	    "application/vnd.sun.xml.calc",
	    "application/vnd.oasis.opendocument.spreadsheet",
	    "application/x-applix-spreadsheet",
	    "application/x-gnumeric",
	    "application/x-kspread",
	    "application/x-kspread-crypt",
	    "application/x-quattropro",
	    "application/x-sc",
	    "application/x-siag",
	    NULL
	  }
	},
	{ N_("Presentation"),
	  { "application/vnd.ms-powerpoint",
	    "application/vnd.sun.xml.impress",
	    "application/vnd.oasis.opendocument.presentation",
	    "application/x-magicpoint",
	    "application/x-kpresenter",
	    NULL
	  }
	},
	{ N_("Pdf / Postscript"),
	  { "application/pdf",
	    "application/postscript",
	    "application/x-dvi",
	    "image/x-eps",
	    NULL
	  }
	},
	{ N_("Text File"),
	  { "text/plain",
	    NULL
	  }
	}
};

static void
type_add_custom_type (NautilusQueryEditorRow *row,
		      const char *mime_type,
		      const char *description,
		      GtkTreeIter *iter)
{
	GtkTreeModel *model;
	GtkListStore *store;
	
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (row->type_widget));
	store = GTK_LIST_STORE (model);

	gtk_list_store_append (store, iter);
	gtk_list_store_set (store, iter,
			    0, description,
			    2, mime_type,
			    -1);
}


static void
type_combo_changed (GtkComboBox *combo_box, NautilusQueryEditorRow *row)
{
	GtkTreeIter iter;
	gboolean other;
	GtkTreeModel *model;

	if (!gtk_combo_box_get_active_iter  (GTK_COMBO_BOX (row->type_widget),
					     &iter)) {
		return;
	}

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (row->type_widget));
	gtk_tree_model_get (model, &iter, 3, &other, -1);

	if (other) {
		GList *mime_infos, *l;
		GtkWidget *dialog;
		GtkWidget *scrolled, *treeview;
		GtkListStore *store;
		GtkTreeViewColumn *column;
		GtkCellRenderer *renderer;
		GtkWidget *toplevel;
		GtkTreeSelection *selection;

		mime_infos = g_content_types_get_registered ();

		store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
		for (l = mime_infos; l != NULL; l = l->next) {
			GtkTreeIter iter;
			char *mime_type = l->data;
			char *description;

			description = g_content_type_get_description (mime_type);
			if (description == NULL) {
				description = g_strdup (mime_type);
			}
			
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,
					    0, description,
					    1, mime_type,
					    -1);
			
			g_free (mime_type);
			g_free (description);
		}
		g_list_free (mime_infos);
		

		
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo_box));
		dialog = gtk_dialog_new_with_buttons (_("Select type"),
						      GTK_WINDOW (toplevel),
						      0,
						      GTK_STOCK_OK, GTK_RESPONSE_OK,
						      NULL);
		gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 600);
			
		scrolled = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
						     GTK_SHADOW_IN);
		
		gtk_widget_show (scrolled);
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), scrolled, TRUE, TRUE, 6);

		treeview = gtk_tree_view_new ();
		gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
					 GTK_TREE_MODEL (store));
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), 0,
						      GTK_SORT_ASCENDING);
		
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
		gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);


		renderer = gtk_cell_renderer_text_new ();
		column = gtk_tree_view_column_new_with_attributes ("Name",
								   renderer,
								   "text",
								   0,
								   NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
		gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
		
		gtk_widget_show (treeview);
		gtk_container_add (GTK_CONTAINER (scrolled), treeview);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
			char *mimetype, *description;

			gtk_tree_selection_get_selected (selection, NULL, &iter);
			gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
					    0, &description,
					    1, &mimetype,
					    -1);

			type_add_custom_type (row, mimetype, description, &iter);
			gtk_combo_box_set_active_iter  (GTK_COMBO_BOX (row->type_widget),
							&iter);
		} else {
			gtk_combo_box_set_active (GTK_COMBO_BOX (row->type_widget), 0);
		}

		gtk_widget_destroy (dialog);
	}
	
	nautilus_query_editor_changed (row->editor);
}

static GtkWidget *
type_row_create_widgets (NautilusQueryEditorRow *row)
{
	GtkWidget *combo;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkTreeIter iter;
	int i;

	store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_BOOLEAN);
	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
					"text", 0,
					NULL);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo),
					      type_separator_func,
					      NULL, NULL);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, _("Any"), -1);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, "---",  -1);

	for (i = 0; i < G_N_ELEMENTS (mime_type_groups); i++) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    0, gettext (mime_type_groups[i].name),
				    1, mime_type_groups[i].mimetypes,
				    -1);
	}

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, "---",  -1);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, _("Other Type..."), 3, TRUE, -1);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
	
	g_signal_connect (combo, "changed",
			  G_CALLBACK (type_combo_changed),
			  row);

	gtk_widget_show (combo);
	
	gtk_box_pack_start (GTK_BOX (row->hbox), combo, FALSE, FALSE, 0);
	
	return combo;
}

static void
type_row_add_to_query (NautilusQueryEditorRow *row,
		       NautilusQuery          *query)
{
	GtkTreeIter iter;
	char **mimetypes;
	char *mimetype;
	GtkTreeModel *model;

	if (!gtk_combo_box_get_active_iter  (GTK_COMBO_BOX (row->type_widget),
					     &iter)) {
		return;
	}

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (row->type_widget));
	gtk_tree_model_get (model, &iter, 1, &mimetypes, 2, &mimetype, -1);

	if (mimetypes != NULL) {
		while (*mimetypes != NULL) {
			nautilus_query_add_mime_type (query, *mimetypes);
			mimetypes++;
		}
	}
	if (mimetype) {
		nautilus_query_add_mime_type (query, mimetype);
		g_free (mimetype);
	}
}

static void
type_row_free_data (NautilusQueryEditorRow *row)
{
}

static gboolean
all_group_types_in_list (char **group_types, GList *mime_types)
{
	GList *l;
	char **group_type;
	char *mime_type;
	gboolean found;

	group_type = group_types;
	while (*group_type != NULL) {
		found = FALSE;

		for (l = mime_types; l != NULL; l = l->next) {
			mime_type = l->data;

			if (strcmp (mime_type, *group_type) == 0) {
				found = TRUE;
				break;
			}
		}
		
		if (!found) {
			return FALSE;
		}
		group_type++;
	}
	return TRUE;
}

static GList *
remove_group_types_from_list (char **group_types, GList *mime_types)
{
	GList *l, *next;
	char **group_type;
	char *mime_type;

	group_type = group_types;
	while (*group_type != NULL) {
		for (l = mime_types; l != NULL; l = next) {
			mime_type = l->data;
			next = l->next;

			if (strcmp (mime_type, *group_type) == 0) {
				mime_types = g_list_remove_link (mime_types, l);
				g_free (mime_type);
				break;
			}
		}
		
		group_type++;
	}
	return mime_types;
}


static void
type_add_rows_from_query (NautilusQueryEditor    *editor,
			  NautilusQuery          *query)
{
	GList *mime_types;
	char *mime_type;
	const char *desc;
	NautilusQueryEditorRow *row;
	GtkTreeIter iter;
	int i;
	GtkTreeModel *model;
	GList *l;

	mime_types = nautilus_query_get_mime_types (query);

	if (mime_types == NULL) {
		return;
	}
	
	for (i = 0; i < G_N_ELEMENTS (mime_type_groups); i++) {
		if (all_group_types_in_list (mime_type_groups[i].mimetypes,
					     mime_types)) {
			mime_types = remove_group_types_from_list (mime_type_groups[i].mimetypes,
								   mime_types);

			row = nautilus_query_editor_add_row (editor,
							     NAUTILUS_QUERY_EDITOR_ROW_TYPE);

			model = gtk_combo_box_get_model (GTK_COMBO_BOX (row->type_widget));

			gtk_tree_model_iter_nth_child (model, &iter, NULL, i + 2);
			gtk_combo_box_set_active_iter  (GTK_COMBO_BOX (row->type_widget),
							&iter);
		}
	}

	for (l = mime_types; l != NULL; l = l->next) {
		mime_type = l->data;

		desc = g_content_type_get_description (mime_type);
		if (desc == NULL) {
			desc = mime_type;
		}

		row = nautilus_query_editor_add_row (editor,
						     NAUTILUS_QUERY_EDITOR_ROW_TYPE);
		
		type_add_custom_type (row, mime_type, desc, &iter);
		gtk_combo_box_set_active_iter  (GTK_COMBO_BOX (row->type_widget),
						&iter);
	}

	g_list_free_full (mime_types, g_free);
}

/* End of row types */

static NautilusQueryEditorRowType
get_next_free_type (NautilusQueryEditor *editor)
{
	NautilusQueryEditorRow *row;
	NautilusQueryEditorRowType type;
	gboolean found;
	GList *l;

	
	for (type = 0; type < NAUTILUS_QUERY_EDITOR_ROW_LAST; type++) {
		found = FALSE;
		for (l = editor->details->rows; l != NULL; l = l->next) {
			row = l->data;
			if (row->type == type) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			return type;
		}
	}
	return NAUTILUS_QUERY_EDITOR_ROW_TYPE;
}

static void
remove_row_cb (GtkButton *clicked_button, NautilusQueryEditorRow *row)
{
	NautilusQueryEditor *editor;

	editor = row->editor;
	gtk_container_remove (GTK_CONTAINER (editor->details->vbox),
			      row->hbox);
	
	editor->details->rows = g_list_remove (editor->details->rows, row);

	row_type[row->type].free_data (row);
	g_free (row);

	nautilus_query_editor_changed (editor);
}

static void
create_type_widgets (NautilusQueryEditorRow *row)
{
	row->type_widget = row_type[row->type].create_widgets (row);
}

static void
row_type_combo_changed_cb (GtkComboBox *combo_box, NautilusQueryEditorRow *row)
{
	NautilusQueryEditorRowType type;

	type = gtk_combo_box_get_active (combo_box);

	if (type == row->type) {
		return;
	}

	if (row->type_widget != NULL) {
		gtk_widget_destroy (row->type_widget);
		row->type_widget = NULL;
	}

	row_type[row->type].free_data (row);
	row->data = NULL;

	row->type = type;
	
	create_type_widgets (row);

	nautilus_query_editor_changed (row->editor);
}

static NautilusQueryEditorRow *
nautilus_query_editor_add_row (NautilusQueryEditor *editor,
			       NautilusQueryEditorRowType type)
{
	GtkWidget *hbox, *button, *image, *combo;
	NautilusQueryEditorRow *row;
	int i;

	row = g_new0 (NautilusQueryEditorRow, 1);
	row->editor = editor;
	row->type = type;
	
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	row->hbox = hbox;
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (editor->details->vbox), hbox, FALSE, FALSE, 0);

	combo = gtk_combo_box_text_new ();
	row->combo = combo;
	for (i = 0; i < NAUTILUS_QUERY_EDITOR_ROW_LAST; i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), gettext (row_type[i].name));
	}
	gtk_widget_show (combo);
	gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), row->type);

	editor->details->rows = g_list_append (editor->details->rows, row);

	g_signal_connect (combo, "changed",
			  G_CALLBACK (row_type_combo_changed_cb), row);
	
	create_type_widgets (row);
	
	button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("list-remove-symbolic",
					      GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_container_add (GTK_CONTAINER (button), image);
	gtk_widget_show (image);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_show (button);

	g_signal_connect (button, "clicked",
			  G_CALLBACK (remove_row_cb), row);
	gtk_widget_set_tooltip_text (button,
				     _("Remove this criterion from the search"));
	
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	return row;
}

static void
add_new_row_cb (GtkButton *clicked_button, NautilusQueryEditor *editor)
{
	nautilus_query_editor_add_row (editor, get_next_free_type (editor));
	nautilus_query_editor_changed (editor);
}

static void
nautilus_query_editor_init (NautilusQueryEditor *editor)
{
	editor->details = G_TYPE_INSTANCE_GET_PRIVATE (editor, NAUTILUS_TYPE_QUERY_EDITOR,
						       NautilusQueryEditorDetails);
	editor->details->is_visible = FALSE;

	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (editor)),
				     GTK_STYLE_CLASS_TOOLBAR);
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (editor)),
				     GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (editor), GTK_ORIENTATION_VERTICAL);

	editor->details->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_no_show_all (editor->details->vbox, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (editor->details->vbox), 6);
	gtk_box_pack_start (GTK_BOX (editor), editor->details->vbox,
			    FALSE, FALSE, 0);
}

void
nautilus_query_editor_set_default_query (NautilusQueryEditor *editor)
{
	nautilus_query_editor_changed (editor);
}

static void
on_all_button_toggled (GtkToggleButton     *button,
		       NautilusQueryEditor *editor)
{
	nautilus_query_editor_changed (editor);
}

static void
on_current_button_toggled (GtkToggleButton     *button,
			   NautilusQueryEditor *editor)
{
	nautilus_query_editor_changed (editor);
}

static void
finish_first_line (NautilusQueryEditor *editor, GtkWidget *hbox, gboolean use_go)
{
	GtkWidget *button, *image;

	button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("list-add-symbolic",
					      GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_container_add (GTK_CONTAINER (button), image);
	gtk_widget_show (image);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_show (button);

	g_signal_connect (button, "clicked",
			  G_CALLBACK (add_new_row_cb), editor);
	
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	gtk_widget_set_tooltip_text (button,
				     _("Add a new criterion to this search"));

	editor->details->search_current_button = gtk_radio_button_new_with_label (NULL, _("Current"));
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (editor->details->search_current_button), FALSE);
	gtk_widget_show (editor->details->search_current_button);
	editor->details->search_all_button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (editor->details->search_current_button),
											  _("All Files"));
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (editor->details->search_all_button), FALSE);
	gtk_widget_show (editor->details->search_all_button);
	g_signal_connect (editor->details->search_all_button, "toggled",
			  G_CALLBACK (on_all_button_toggled), editor);
	g_signal_connect (editor->details->search_current_button, "toggled",
			  G_CALLBACK (on_current_button_toggled), editor);
	gtk_box_pack_start (GTK_BOX (hbox), editor->details->search_current_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), editor->details->search_all_button, FALSE, FALSE, 0);
}

static void
setup_widgets (NautilusQueryEditor *editor)
{
	GtkWidget *hbox;

	/* Create visible part: */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (editor->details->vbox), hbox, FALSE, FALSE, 0);

	editor->details->entry = gtk_search_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), editor->details->entry, TRUE, TRUE, 0);

	g_signal_connect (editor->details->entry, "activate",
			  G_CALLBACK (entry_activate_cb), editor);
	g_signal_connect (editor->details->entry, "changed",
			  G_CALLBACK (entry_changed_cb), editor);
	gtk_widget_show (editor->details->entry);

	finish_first_line (editor, hbox, TRUE);
}

void
nautilus_query_editor_set_visible (NautilusQueryEditor *editor,
				   gboolean visible)
{
	editor->details->is_visible = visible;
	if (visible) {
		gtk_widget_show (editor->details->vbox);
	} else {
		gtk_widget_hide (editor->details->vbox);
	}
}

static void
nautilus_query_editor_changed_force (NautilusQueryEditor *editor, gboolean force_reload)
{
	NautilusQuery *query;

	if (editor->details->change_frozen) {
		return;
	}

	query = nautilus_query_editor_get_query (editor);
	g_signal_emit (editor, signals[CHANGED], 0,
		       query, force_reload);
	g_object_unref (query);
}

static void
nautilus_query_editor_changed (NautilusQueryEditor *editor)
{
	nautilus_query_editor_changed_force (editor, TRUE);
}

void
nautilus_query_editor_grab_focus (NautilusQueryEditor *editor)
{
	if (editor->details->is_visible) {
		entry_focus_hack (editor->details->entry, gtk_get_current_event_device ());
	}
}

static void
add_location_to_query (NautilusQueryEditor *editor,
		       NautilusQuery       *query)
{
	char *uri;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->details->search_all_button))) {
		uri = nautilus_get_home_directory_uri ();
	} else {
		uri = g_strdup (editor->details->current_uri);
	}

	nautilus_query_set_location (query, uri);
	g_free (uri);
}

NautilusQuery *
nautilus_query_editor_get_query (NautilusQueryEditor *editor)
{
	const char *query_text;
	NautilusQuery *query;
	GList *l;
	NautilusQueryEditorRow *row;

	if (editor == NULL || editor->details == NULL || editor->details->entry == NULL) {
		return NULL;
	}

	query_text = gtk_entry_get_text (GTK_ENTRY (editor->details->entry));

	query = nautilus_query_new ();
	nautilus_query_set_text (query, query_text);

	add_location_to_query (editor, query);

	for (l = editor->details->rows; l != NULL; l = l->next) {
		row = l->data;
		
		row_type[row->type].add_to_query (row, query);
	}
	
	return query;
}

void
nautilus_query_editor_clear_query (NautilusQueryEditor *editor)
{
	editor->details->change_frozen = TRUE;
	gtk_entry_set_text (GTK_ENTRY (editor->details->entry), "");

	g_free (editor->details->last_set_query_text);
	editor->details->last_set_query_text = g_strdup ("");

	editor->details->change_frozen = FALSE;
}

GtkWidget *
nautilus_query_editor_new (void)
{
	GtkWidget *editor;

	editor = g_object_new (NAUTILUS_TYPE_QUERY_EDITOR, NULL);
	setup_widgets (NAUTILUS_QUERY_EDITOR (editor));

	return editor;
}

static void
update_location (NautilusQueryEditor *editor)
{
	NautilusFile *file;

	file = nautilus_file_get_by_uri (editor->details->current_uri);

	if (file != NULL) {
		char *name;
		if (nautilus_file_is_home (file)) {
			name = g_strdup (_("Home"));
		} else {
			char *filename;
			filename = nautilus_file_get_display_name (file);
			name = g_strdup_printf ("\342\200\234%s\342\200\235", filename);
			g_free (filename);
		}
		gtk_button_set_label (GTK_BUTTON (editor->details->search_current_button),
				      name);
		g_free (name);

		nautilus_file_unref (file);
	}
}

void
nautilus_query_editor_set_location (NautilusQueryEditor *editor,
				    GFile               *location)
{
	g_free (editor->details->current_uri);
	editor->details->current_uri = g_file_get_uri (location);
	update_location (editor);
}

void
nautilus_query_editor_set_query (NautilusQueryEditor *editor, NautilusQuery *query)
{
	NautilusQueryEditorRowType type;
	char *text;

	if (!query) {
		nautilus_query_editor_clear_query (editor);
		return;
	}

	text = nautilus_query_get_text (query);

	if (!text) {
		text = g_strdup ("");
	}

	editor->details->change_frozen = TRUE;

	gtk_entry_set_text (GTK_ENTRY (editor->details->entry), text);

	g_free (editor->details->current_uri);
	editor->details->current_uri = nautilus_query_get_location (query);

	update_location (editor);

	for (type = 0; type < NAUTILUS_QUERY_EDITOR_ROW_LAST; type++) {
		row_type[type].add_rows_from_query (editor, query);
	}
	
	editor->details->change_frozen = FALSE;

	g_free (editor->details->last_set_query_text);
	editor->details->last_set_query_text = text;
}
