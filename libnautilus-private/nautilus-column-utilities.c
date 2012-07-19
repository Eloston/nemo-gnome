/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-column-utilities.h - Utilities related to column specifications

   Copyright (C) 2004 Novell, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the column COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Dave Camp <dave@ximian.com>
*/

#include <config.h>
#include "nautilus-column-utilities.h"

#include <string.h>
#include <eel/eel-glib-extensions.h>
#include <glib/gi18n.h>
#include <libnautilus-extension/nautilus-column-provider.h>
#include <libnautilus-private/nautilus-module.h>

static const char *default_column_order[] = {
	"name",
	"size",
	"type",
	"date_modified",
	"owner",
	"group",
	"permissions",
	"octal_permissions",
	"mime_type",
	"selinux_context",
	"where",
	NULL
};

static GList *
get_builtin_columns (void)
{
	GList *columns;

	columns = g_list_append (NULL,
				 g_object_new (NAUTILUS_TYPE_COLUMN,
					       "name", "name",
					       "attribute", "name",
					       "label", _("Name"),
					       "description", _("The name and icon of the file."),
					       NULL));
	columns = g_list_append (columns,
				 g_object_new (NAUTILUS_TYPE_COLUMN,
					       "name", "size",
					       "attribute", "size",
					       "label", _("Size"),
					       "description", _("The size of the file."),
					       "xalign", 1.0,
					       NULL));
	columns = g_list_append (columns,
				 g_object_new (NAUTILUS_TYPE_COLUMN,
					       "name", "type",
					       "attribute", "type",
					       "label", _("Type"),
					       "description", _("The type of the file."),
					       NULL));
	columns = g_list_append (columns,
				 g_object_new (NAUTILUS_TYPE_COLUMN,
					       "name", "date_modified",
					       "attribute", "date_modified",
					       "label", _("Modified"),
					       "description", _("The date the file was modified."),
					       "default-sort-order", GTK_SORT_DESCENDING,
					       NULL));

	columns = g_list_append (columns,
				 g_object_new (NAUTILUS_TYPE_COLUMN,
					       "name", "owner",
					       "attribute", "owner",
					       "label", _("Owner"),
					       "description", _("The owner of the file."),
					       NULL));

	columns = g_list_append (columns,
				 g_object_new (NAUTILUS_TYPE_COLUMN,
					       "name", "group",
					       "attribute", "group",
					       "label", _("Group"),
					       "description", _("The group of the file."),
					       NULL));

	columns = g_list_append (columns,
				 g_object_new (NAUTILUS_TYPE_COLUMN,
					       "name", "permissions",
					       "attribute", "permissions",
					       "label", _("Permissions"),
					       "description", _("The permissions of the file."),
					       NULL));

	columns = g_list_append (columns,
				 g_object_new (NAUTILUS_TYPE_COLUMN,
					       "name", "octal_permissions",
					       "attribute", "octal_permissions",
					       "label", _("Octal Permissions"),
					       "description", _("The permissions of the file, in octal notation."),
					       NULL));

	columns = g_list_append (columns,
				 g_object_new (NAUTILUS_TYPE_COLUMN,
					       "name", "mime_type",
					       "attribute", "mime_type",
					       "label", _("MIME Type"),
					       "description", _("The mime type of the file."),
					       NULL));
#ifdef HAVE_SELINUX
	columns = g_list_append (columns,
				 g_object_new (NAUTILUS_TYPE_COLUMN,
					       "name", "selinux_context",
					       "attribute", "selinux_context",
					       "label", _("Security Context"),
					       "description", _("The security context of the file."),
					       NULL));
#endif
	columns = g_list_append (columns,
				 g_object_new (NAUTILUS_TYPE_COLUMN,
					       "name", "where",
					       "attribute", "where",
					       "label", _("Location"),
					       "description", _("The location of the file."),
					       NULL));

	return columns;
}

static GList *
get_extension_columns (void)
{
	GList *columns;
	GList *providers;
	GList *l;
	
	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_COLUMN_PROVIDER);
	
	columns = NULL;
	
	for (l = providers; l != NULL; l = l->next) {
		NautilusColumnProvider *provider;
		GList *provider_columns;
		
		provider = NAUTILUS_COLUMN_PROVIDER (l->data);
		provider_columns = nautilus_column_provider_get_columns (provider);
		columns = g_list_concat (columns, provider_columns);
	}

	nautilus_module_extension_list_free (providers);

	return columns;
}

static GList *
get_trash_columns (void)
{
	static GList *columns = NULL;

	if (columns == NULL) {
		columns = g_list_append (columns,
					 g_object_new (NAUTILUS_TYPE_COLUMN,
						       "name", "trashed_on",
						       "attribute", "trashed_on",
						       "label", _("Trashed On"),
						       "description", _("Date when file was moved to the Trash"),
						       NULL));
		columns = g_list_append (columns,
			                 g_object_new (NAUTILUS_TYPE_COLUMN,
			                               "name", "trash_orig_path",
			                               "attribute", "trash_orig_path",
			                               "label", _("Original Location"),
			                               "description", _("Original location of file before moved to the Trash"),
			                               NULL));
	}

	return nautilus_column_list_copy (columns);
}

static GList *
get_search_columns (void)
{
	static GList *columns = NULL;

	if (columns == NULL) {
		columns = g_list_append (columns,
					 g_object_new (NAUTILUS_TYPE_COLUMN,
						       "name", "search_relevance",
						       "attribute", "search_relevance",
						       "label", _("Relevance"),
						       "description", _("Relevance rank for search"),
						       NULL));
	}

	return nautilus_column_list_copy (columns);
}

GList *
nautilus_get_common_columns (void)
{
	static GList *columns = NULL;

	if (!columns) {
		columns = g_list_concat (get_builtin_columns (),
		                         get_extension_columns ());
	}

	return nautilus_column_list_copy (columns);
}

GList *
nautilus_get_all_columns (void)
{
	GList *columns = NULL;

	columns = g_list_concat (nautilus_get_common_columns (),
	                         get_trash_columns ());
	columns = g_list_concat (columns,
				 get_search_columns ());

	return columns;
}

GList *
nautilus_get_columns_for_file (NautilusFile *file)
{
	GList *columns;

	columns = nautilus_get_common_columns ();

	if (file != NULL && nautilus_file_is_in_trash (file)) {
		columns = g_list_concat (columns,
		                         get_trash_columns ());
	}

	return columns;
}

GList *
nautilus_column_list_copy (GList *columns) 
{
	GList *ret;
	GList *l;
	
	ret = g_list_copy (columns);
	
	for (l = ret; l != NULL; l = l->next) {
		g_object_ref (l->data);
	}

	return ret;
}

void
nautilus_column_list_free (GList *columns)
{
	GList *l;
	
	for (l = columns; l != NULL; l = l->next) {
		g_object_unref (l->data);
	}
	
	g_list_free (columns);
}

static int
strv_index (char **strv, const char *str)
{
	int i;

	for (i = 0; strv[i] != NULL; ++i) {
		if (strcmp (strv[i], str) == 0)
			return i;
	}

	return -1;
}

static int
column_compare (NautilusColumn *a, NautilusColumn *b, char **column_order)
{
	int index_a;
	int index_b;
	char *name_a;
	char *name_b;
	int ret;

	g_object_get (G_OBJECT (a), "name", &name_a, NULL);
	index_a = strv_index (column_order, name_a);

	g_object_get (G_OBJECT (b), "name", &name_b, NULL);
	index_b = strv_index (column_order, name_b);

	if (index_a == index_b) {
		int pos_a;
		int pos_b;

		pos_a = strv_index ((char **)default_column_order, name_a);
		pos_b = strv_index ((char **)default_column_order, name_b);

		if (pos_a == pos_b) {
			char *label_a;
			char *label_b;
		
			g_object_get (G_OBJECT (a), "label", &label_a, NULL);
			g_object_get (G_OBJECT (b), "label", &label_b, NULL);
			ret = strcmp (label_a, label_b);
			g_free (label_a);
			g_free (label_b);
		} else if (pos_a == -1) {
			ret = 1;
		} else if (pos_b == -1) {
			ret = -1;
		} else {
			ret = index_a - index_b;
		}
	} else if (index_a == -1) {
		ret = 1;
	} else if (index_b == -1) {
		ret = -1;
	} else {
		ret = index_a - index_b;
	}

	g_free (name_a);
	g_free (name_b);

	return ret;
}

GList *
nautilus_sort_columns (GList  *columns, 
		       char  **column_order)
{
	if (!column_order) {
		return NULL;
	}

	return g_list_sort_with_data (columns,
				      (GCompareDataFunc)column_compare,
				      column_order);
}
		       
