/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Mr Jamie McCracken
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
 * Author: Jamie McCracken <jamiemcc@gnome.org>
 *
 */

#include <config.h>
#include "nautilus-search-engine-tracker.h"
#include <string.h>
#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

struct NautilusSearchEngineTrackerDetails {
	TrackerSparqlConnection *connection;
	NautilusQuery *query;

	gboolean       query_pending;
	GCancellable  *cancellable;
};

G_DEFINE_TYPE (NautilusSearchEngineTracker,
	       nautilus_search_engine_tracker,
	       NAUTILUS_TYPE_SEARCH_ENGINE);

static void
finalize (GObject *object)
{
	NautilusSearchEngineTracker *tracker;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (object);

	if (tracker->details->cancellable) {
		g_cancellable_cancel (tracker->details->cancellable);
		g_clear_object (&tracker->details->cancellable);
	}

	g_clear_object (&tracker->details->query);
	g_clear_object (&tracker->details->connection);

	G_OBJECT_CLASS (nautilus_search_engine_tracker_parent_class)->finalize (object);
}

static void cursor_callback (GObject      *object,
			     GAsyncResult *result,
			     gpointer      user_data);

static void
cursor_next (NautilusSearchEngineTracker *tracker,
             TrackerSparqlCursor    *cursor)
{
	tracker_sparql_cursor_next_async (cursor,
	                                  tracker->details->cancellable,
	                                  cursor_callback,
	                                  tracker);
}

static void
cursor_callback (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	NautilusSearchEngineTracker *tracker;
	GError *error = NULL;
	TrackerSparqlCursor *cursor;
	GList *hits;
	gboolean success;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (user_data);

	cursor = TRACKER_SPARQL_CURSOR (object);
	success = tracker_sparql_cursor_next_finish (cursor, result, &error);

	if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		tracker->details->query_pending = FALSE;
		nautilus_search_engine_error (NAUTILUS_SEARCH_ENGINE (tracker), error->message);
		g_error_free (error);
		g_object_unref (cursor);

		return;
	}

	g_clear_error (&error);

	if (!success) {
		tracker->details->query_pending = FALSE;
		nautilus_search_engine_finished (NAUTILUS_SEARCH_ENGINE (tracker));
		g_object_unref (cursor);

		return;
	}

	/* We iterate result by result, not n at a time. */
	hits = g_list_append (NULL, (gchar*) tracker_sparql_cursor_get_string (cursor, 0, NULL));
	nautilus_search_engine_hits_added (NAUTILUS_SEARCH_ENGINE (tracker), hits);
	g_list_free (hits);

	/* Get next */
	cursor_next (tracker, cursor);
}

static void
query_callback (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
	NautilusSearchEngineTracker *tracker;
	TrackerSparqlConnection *connection;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (user_data);

	connection = TRACKER_SPARQL_CONNECTION (object);
	cursor = tracker_sparql_connection_query_finish (connection,
	                                                 result,
	                                                 &error);

	if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		tracker->details->query_pending = FALSE;
		nautilus_search_engine_error (NAUTILUS_SEARCH_ENGINE (tracker), error->message);
		g_error_free (error);
		return;
	}

	g_clear_error (&error);

	if (!cursor) {
		tracker->details->query_pending = FALSE;
		nautilus_search_engine_finished (NAUTILUS_SEARCH_ENGINE (tracker));
		return;
	}

	cursor_next (tracker, cursor);
}

static void
nautilus_search_engine_tracker_start (NautilusSearchEngine *engine)
{
	NautilusSearchEngineTracker *tracker;
	gchar	*query_text, *search_text, *location_uri, *downcase;
	GString *sparql;
	GList *mimetypes, *l;
	gint mime_count;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (engine);

	if (tracker->details->query_pending) {
		return;
	}

	if (tracker->details->query == NULL) {
		return;
	}

	query_text = nautilus_query_get_text (tracker->details->query);
	downcase = g_utf8_strdown (query_text, -1);
	search_text = tracker_sparql_escape_string (downcase);
	g_free (query_text);
	g_free (downcase);

	location_uri = nautilus_query_get_location (tracker->details->query);
	mimetypes = nautilus_query_get_mime_types (tracker->details->query);

	mime_count = g_list_length (mimetypes);

	sparql = g_string_new ("SELECT DISTINCT nie:url(?urn) "
			       "WHERE {"
			       "  ?urn a nfo:FileDataObject ;"
			       "  tracker:available true ; ");

	if (mime_count > 0) {
		g_string_append (sparql, "nie:mimeType ?mime ;");
	}

	g_string_append_printf (sparql, " fts:match '%s*'", search_text);
	g_string_append (sparql, " . FILTER (");
	
	if (location_uri)  {
		g_string_append_printf (sparql, " fn:starts-with(nie:url(?urn), '%s')",
					location_uri);
	}

	g_string_append_printf (sparql, " && fn:contains(fn:lower-case(nfo:fileName(?urn)), '%s')",
				search_text);

	if (mime_count > 0) {
		g_string_append (sparql, " && (");

		for (l = mimetypes; l != NULL; l = l->next) {
			if (l != mimetypes) {
				g_string_append (sparql, " || ");
			}

			g_string_append_printf (sparql, "fn:contains(?mime, '%s')",
						(gchar *) l->data);
		}
		g_string_append (sparql, ")");
	}

	g_string_append (sparql, ")}");

	tracker->details->cancellable = g_cancellable_new ();
	tracker->details->query_pending = TRUE;
	tracker_sparql_connection_query_async (tracker->details->connection,
					       sparql->str,
					       tracker->details->cancellable,
					       query_callback,
					       tracker);
	g_string_free (sparql, TRUE);

	g_free (search_text);
	g_free (location_uri);
	g_list_free_full (mimetypes, g_free);
}

static void
nautilus_search_engine_tracker_stop (NautilusSearchEngine *engine)
{
	NautilusSearchEngineTracker *tracker;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (engine);
	
	if (tracker->details->query && tracker->details->query_pending) {
		g_cancellable_cancel (tracker->details->cancellable);
		g_clear_object (&tracker->details->cancellable);
		tracker->details->query_pending = FALSE;
	}
}

static void
nautilus_search_engine_tracker_set_query (NautilusSearchEngine *engine, NautilusQuery *query)
{
	NautilusSearchEngineTracker *tracker;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (engine);

	if (query) {
		g_object_ref (query);
	}

	if (tracker->details->query) {
		g_object_unref (tracker->details->query);
	}

	tracker->details->query = query;
}

static void
nautilus_search_engine_tracker_class_init (NautilusSearchEngineTrackerClass *class)
{
	GObjectClass *gobject_class;
	NautilusSearchEngineClass *engine_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	engine_class = NAUTILUS_SEARCH_ENGINE_CLASS (class);
	engine_class->set_query = nautilus_search_engine_tracker_set_query;
	engine_class->start = nautilus_search_engine_tracker_start;
	engine_class->stop = nautilus_search_engine_tracker_stop;

	g_type_class_add_private (class, sizeof (NautilusSearchEngineTrackerDetails));
}

static void
nautilus_search_engine_tracker_init (NautilusSearchEngineTracker *engine)
{
	engine->details = G_TYPE_INSTANCE_GET_PRIVATE (engine, NAUTILUS_TYPE_SEARCH_ENGINE_TRACKER,
						       NautilusSearchEngineTrackerDetails);
}


NautilusSearchEngine *
nautilus_search_engine_tracker_new (void)
{
	NautilusSearchEngineTracker *engine;
	TrackerSparqlConnection *connection;
	GError *error = NULL;

	connection = tracker_sparql_connection_get (NULL, &error);

	if (error) {
		g_warning ("Could not establish a connection to Tracker: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_TRACKER, NULL);
	engine->details->connection = connection;

	return NAUTILUS_SEARCH_ENGINE (engine);
}
