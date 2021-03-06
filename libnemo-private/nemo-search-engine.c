/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include "nemo-search-provider.h"
#include "nemo-search-engine.h"
#include "nemo-search-engine-simple.h"
#include "nemo-search-engine-model.h"
#define DEBUG_FLAG NEMO_DEBUG_SEARCH
#include "nemo-debug.h"

#ifdef ENABLE_TRACKER
#include "nemo-search-engine-tracker.h"
#endif

struct NemoSearchEngineDetails
{
#ifdef ENABLE_TRACKER
	NemoSearchEngineTracker *tracker;
#endif
	NemoSearchEngineSimple *simple;
	NemoSearchEngineModel *model;

	GHashTable *uris;
	guint providers_running;
	guint providers_finished;
	guint providers_error;

	gboolean running;
	gboolean restart;
};

static void nemo_search_provider_init (NemoSearchProviderIface  *iface);

G_DEFINE_TYPE_WITH_CODE (NemoSearchEngine,
			 nemo_search_engine,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NEMO_TYPE_SEARCH_PROVIDER,
						nemo_search_provider_init))

static void
nemo_search_engine_set_query (NemoSearchProvider *provider,
				  NemoQuery          *query)
{
	NemoSearchEngine *engine = NEMO_SEARCH_ENGINE (provider);
#ifdef ENABLE_TRACKER
	nemo_search_provider_set_query (NEMO_SEARCH_PROVIDER (engine->details->tracker), query);
#endif
	nemo_search_provider_set_query (NEMO_SEARCH_PROVIDER (engine->details->model), query);
	nemo_search_provider_set_query (NEMO_SEARCH_PROVIDER (engine->details->simple), query);
}

static void
search_engine_start_real (NemoSearchEngine *engine)
{
	engine->details->providers_running = 0;
	engine->details->providers_finished = 0;
	engine->details->providers_error = 0;

	engine->details->restart = FALSE;

	DEBUG ("Search engine start real");

	g_object_ref (engine);

#ifdef ENABLE_TRACKER
	nemo_search_provider_start (NEMO_SEARCH_PROVIDER (engine->details->tracker));
	engine->details->providers_running++;
#endif
	if (nemo_search_engine_model_get_model (engine->details->model)) {
		nemo_search_provider_start (NEMO_SEARCH_PROVIDER (engine->details->model));
		engine->details->providers_running++;
	}

	nemo_search_provider_start (NEMO_SEARCH_PROVIDER (engine->details->simple));
	engine->details->providers_running++;
}

static void
nemo_search_engine_start (NemoSearchProvider *provider)
{
	NemoSearchEngine *engine = NEMO_SEARCH_ENGINE (provider);
	gint num_finished;

	DEBUG ("Search engine start");

	num_finished = engine->details->providers_error + engine->details->providers_finished;

	if (engine->details->running) {
		if (num_finished == engine->details->providers_running &&
		    engine->details->restart) {
			search_engine_start_real (engine);
		}

		return;
	}

	engine->details->running = TRUE;

	if (num_finished < engine->details->providers_running) {
		engine->details->restart = TRUE;
	} else {
		search_engine_start_real (engine);
	}
}

static void
nemo_search_engine_stop (NemoSearchProvider *provider)
{
	NemoSearchEngine *engine = NEMO_SEARCH_ENGINE (provider);

	DEBUG ("Search engine stop");

#ifdef ENABLE_TRACKER
	nemo_search_provider_stop (NEMO_SEARCH_PROVIDER (engine->details->tracker));
#endif
	nemo_search_provider_stop (NEMO_SEARCH_PROVIDER (engine->details->model));
	nemo_search_provider_stop (NEMO_SEARCH_PROVIDER (engine->details->simple));

	engine->details->running = FALSE;
	engine->details->restart = FALSE;
}

static void
search_provider_hits_added (NemoSearchProvider *provider,
			    GList                  *hits,
			    NemoSearchEngine   *engine)
{
	GList *added = NULL;
	GList *l;

	if (!engine->details->running || engine->details->restart) {
		DEBUG ("Ignoring hits-added, since engine is %s",
		       !engine->details->running ? "not running" : "waiting to restart");
		return;
	}

	for (l = hits; l != NULL; l = l->next) {
		NemoSearchHit *hit = l->data;
		int count;
		const char *uri;

		uri = nemo_search_hit_get_uri (hit);
		count = GPOINTER_TO_INT (g_hash_table_lookup (engine->details->uris, uri));
		if (count == 0)
			added = g_list_prepend (added, hit);
		g_hash_table_replace (engine->details->uris, g_strdup (uri), GINT_TO_POINTER (++count));
	}
	if (added != NULL) {
		added = g_list_reverse (added);
		nemo_search_provider_hits_added (NEMO_SEARCH_PROVIDER (engine), added);
		g_list_free (added);
	}
}

static void
check_providers_status (NemoSearchEngine *engine)
{
	gint num_finished = engine->details->providers_error + engine->details->providers_finished;

	if (num_finished < engine->details->providers_running) {
		return;
	}

	if (num_finished == engine->details->providers_error) {
		DEBUG ("Search engine error");
		nemo_search_provider_error (NEMO_SEARCH_PROVIDER (engine),
						_("Unable to complete the requested search"));
	} else {
		DEBUG ("Search engine finished");
		nemo_search_provider_finished (NEMO_SEARCH_PROVIDER (engine));
	}

	engine->details->running = FALSE;
	g_hash_table_remove_all (engine->details->uris);

	if (engine->details->restart) {
		DEBUG ("Restarting engine");
		nemo_search_engine_start (NEMO_SEARCH_PROVIDER (engine));
	}

	g_object_unref (engine);
}

static void
search_provider_error (NemoSearchProvider *provider,
		       const char             *error_message,
		       NemoSearchEngine   *engine)

{
	DEBUG ("Search provider error: %s", error_message);
	engine->details->providers_error++;

	check_providers_status (engine);
}

static void
search_provider_finished (NemoSearchProvider *provider,
			  NemoSearchEngine   *engine)

{
	DEBUG ("Search provider finished");
	engine->details->providers_finished++;

	check_providers_status (engine);
}

static void
connect_provider_signals (NemoSearchEngine   *engine,
			  NemoSearchProvider *provider)
{
	g_signal_connect (provider, "hits-added",
			  G_CALLBACK (search_provider_hits_added),
			  engine);
	g_signal_connect (provider, "finished",
			  G_CALLBACK (search_provider_finished),
			  engine);
	g_signal_connect (provider, "error",
			  G_CALLBACK (search_provider_error),
			  engine);
}

static void
nemo_search_provider_init (NemoSearchProviderIface *iface)
{
	iface->set_query = nemo_search_engine_set_query;
	iface->start = nemo_search_engine_start;
	iface->stop = nemo_search_engine_stop;
}

static void
nemo_search_engine_finalize (GObject *object)
{
	NemoSearchEngine *engine = NEMO_SEARCH_ENGINE (object);

	g_hash_table_destroy (engine->details->uris);

#ifdef ENABLE_TRACKER
	g_clear_object (&engine->details->tracker);
#endif
	g_clear_object (&engine->details->model);
	g_clear_object (&engine->details->simple);

	G_OBJECT_CLASS (nemo_search_engine_parent_class)->finalize (object);
}

static void
nemo_search_engine_class_init (NemoSearchEngineClass *class)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;

	object_class->finalize = nemo_search_engine_finalize;

	g_type_class_add_private (class, sizeof (NemoSearchEngineDetails));
}

static void
nemo_search_engine_init (NemoSearchEngine *engine)
{
	engine->details = G_TYPE_INSTANCE_GET_PRIVATE (engine,
						       NEMO_TYPE_SEARCH_ENGINE,
						       NemoSearchEngineDetails);

	engine->details->uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

#ifdef ENABLE_TRACKER
	engine->details->tracker = nemo_search_engine_tracker_new ();
	connect_provider_signals (engine, NEMO_SEARCH_PROVIDER (engine->details->tracker));
#endif
	engine->details->model = nemo_search_engine_model_new ();
	connect_provider_signals (engine, NEMO_SEARCH_PROVIDER (engine->details->model));

	engine->details->simple = nemo_search_engine_simple_new ();
	connect_provider_signals (engine, NEMO_SEARCH_PROVIDER (engine->details->simple));
}

NemoSearchEngine *
nemo_search_engine_new (void)
{
	NemoSearchEngine *engine;

	engine = g_object_new (NEMO_TYPE_SEARCH_ENGINE, NULL);

	return engine;
}

NemoSearchEngineModel *
nemo_search_engine_get_model_provider (NemoSearchEngine *engine)
{
	return engine->details->model;
}

NemoSearchEngineSimple *
nemo_search_engine_get_simple_provider (NemoSearchEngine *engine)
{
	return engine->details->simple;
}
