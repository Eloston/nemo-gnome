include $(top_srcdir)/Makefile.shared

bin_PROGRAMS=					\
	nemo				\
	nemo-autorun-software		\
	nemo-connect-server			\
    nemo-open-with \
	$(NULL)

libexec_PROGRAMS=				\
	nemo-convert-metadata		\
    nemo-extensions-list \
	$(NULL)

AM_CPPFLAGS =							\
	-I$(top_srcdir) 					\
	-I$(top_builddir)/libnemo-private 			\
	$(BASE_CFLAGS) 						\
	$(COMMON_CFLAGS)					\
	$(NEMO_CFLAGS)						\
	$(DESKTOP_CFLAGS)					\
	$(WARNING_CFLAGS)					\
	$(EXIF_CFLAGS)						\
	$(EXEMPI_CFLAGS)                                        \
	-DDATADIR=\""$(datadir)"\" 				\
	-DLIBDIR=\""$(libdir)"\" 				\
	-DLIBEXECDIR=\""$(libexecdir)"\" 			\
	-DNEMO_DATADIR=\""$(datadir)/nemo"\" 		\
	-DUIDIR=\""$(datadir)/nemo/ui"\" 		\
	-DNEMO_PIXMAPDIR=\""$(datadir)/pixmaps/nemo"\" 	\
	-DPREFIX=\""$(prefix)"\" 				\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DVERSION="\"$(VERSION)\"" 				\
    -DNEMO_EXTENSIONDIR=\""$(libdir)/nemo/extensions-3.0"\" \
	$(DISABLE_DEPRECATED) 		                        \
	$(UNITY_CFLAGS)                            \
	$(NULL)

LDADD =\
	$(top_builddir)/libnemo-private/libnemo-private.la  \
	$(BASE_LIBS) \
	$(COMMON_LIBS) \
	$(NEMO_LIBS) \
	$(DESKTOP_LIBS) \
	$(CORE_LIBS) \
	$(EXIF_LIBS) \
	$(EXEMPI_LIBS) \
	$(POPT_LIBS) \
	$(UNITY_LIBS) \
	$(NULL)

dbus_built_sources = 					\
	nemo-generated.c				\
	nemo-generated.h

$(dbus_built_sources) : Makefile.am $(top_srcdir)/data/dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.Nemo.					\
		--c-namespace NemoDBus						\
		--c-generate-object-manager						\
		--generate-c-code nemo-generated					\
		$(top_srcdir)/data/dbus-interfaces.xml					\
		$(NULL)

dbus_freedesktop_built_sources =			\
	nemo-freedesktop-generated.c		\
	nemo-freedesktop-generated.h

$(dbus_freedesktop_built_sources) : Makefile.am $(top_srcdir)/data/freedesktop-dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.freedesktop.					\
		--c-namespace NemoFreedesktop					\
		--c-generate-object-manager						\
		--generate-c-code nemo-freedesktop-generated			\
		$(top_srcdir)/data/freedesktop-dbus-interfaces.xml			\
		$(NULL)

dbus_shell_search_provider_built_sources =		\
	nemo-shell-search-provider-generated.c	\
	nemo-shell-search-provider-generated.h

$(dbus_shell_search_provider_built_sources) : Makefile.am $(top_srcdir)/data/shell-search-provider-dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.gnome.						\
		--c-namespace Nemo							\
		--generate-c-code nemo-shell-search-provider-generated		\
		$(top_srcdir)/data/shell-search-provider-dbus-interfaces.xml		\
		$(NULL)


headers =					\
	nemo-special-location-bar.h		\
	$(NULL)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/nemo.gresource.xml)
nemo-resources.c: nemo.gresource.xml $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name nemo $(srcdir)/nemo.gresource.xml
nemo-resources.h: nemo.gresource.xml $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name nemo $(srcdir)/nemo.gresource.xml

nemo-enum-types.h: stamp-nemo-enum-types.h
	$(AM_V_GEN)true

stamp-nemo-enum-types.h: $(headers) Makefile
	$(AM_V_GEN)(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#ifndef __NEMO_ENUM_TYPES_H__\n" \
			--fhead "#define __NEMO_ENUM_TYPES_H__ 1\n\n" \
			--fhead "#include <glib-object.h>\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __NEMO_ENUM_TYPES_H__ */\n" \
			--fprod "#include \"@filename@\"\n" \
			--eprod "#define @ENUMPREFIX@_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(headers) ) > xgen-gth \
	&& (cmp -s xgen-gth nemo-enum-types.h || cp xgen-gth nemo-enum-types.h) \
	&& rm -f xgen-gth \
	&& echo timestamp > $(@F)

nemo-enum-types.c: $(headers) Makefile
	$(AM_V_GEN)(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#include <config.h>\n" \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"nemo-enum-types.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(headers) ) > xgen-gtc \
	&& cp xgen-gtc $(@F) \
	&& rm -f xgen-gtc

nemo_built_sources = \
	$(dbus_built_sources) \
	$(dbus_freedesktop_built_sources) \
	$(dbus_shell_search_provider_built_sources) \
	nemo-resources.c \
	nemo-resources.h \
	nemo-enum-types.h \
	nemo-enum-types.c \
	$(NULL)

nemo_SOURCES = \
	nemo-actions.h			\
    nemo-action-config-widget.c         \
    nemo-action-config-widget.h         \
	nemo-application.c			\
	nemo-application.h			\
	nemo-bookmark-list.c		\
	nemo-bookmark-list.h		\
	nemo-bookmarks-window.c		\
	nemo-bookmarks-window.h		\
    nemo-config-base-widget.c           \
    nemo-config-base-widget.h           \
	nemo-canvas-view.c			\
	nemo-canvas-view.h			\
	nemo-canvas-view-container.c		\
	nemo-canvas-view-container.h		\
	nemo-connect-server-dialog.c	\
	nemo-connect-server-dialog.h	\
	nemo-dbus-manager.c 		\
	nemo-dbus-manager.h 		\
	nemo-desktop-canvas-view.c		\
	nemo-desktop-canvas-view.h		\
	nemo-desktop-item-properties.c	\
	nemo-desktop-item-properties.h	\
	nemo-desktop-window.c		\
	nemo-desktop-window.h		\
	nemo-error-reporting.c		\
	nemo-error-reporting.h		\
    nemo-extension-config-widget.c      \
    nemo-extension-config-widget.h      \
	nemo-file-management-properties.c	\
	nemo-file-management-properties.h	\
	nemo-floating-bar.c			\
	nemo-floating-bar.h			\
	nemo-freedesktop-dbus.c		\
	nemo-freedesktop-dbus.h		\
	nemo-image-properties-page.c	\
	nemo-image-properties-page.h	\
    nemo-interesting-folder-bar.c  \
    nemo-interesting-folder-bar.h  \
	nemo-list-model.c			\
	nemo-list-model.h			\
	nemo-list-view-private.h 		\
	nemo-list-view.c			\
	nemo-list-view.h 			\
	nemo-location-entry.c               \
	nemo-location-entry.h               \
	nemo-main.c 			\
	nemo-mime-actions.c 		\
	nemo-mime-actions.h 		\
	nemo-navigation-action.c		\
	nemo-navigation-action.h		\
	nemo-navigation-state.c		\
	nemo-navigation-state.h		\
	nemo-notebook.c			\
	nemo-notebook.h			\
	nemo-pathbar.c			\
	nemo-pathbar.h			\
	nemo-places-sidebar.c		\
	nemo-places-sidebar.h		\
    nemo-plugin-manager.c        \
    nemo-plugin-manager.h        \
	nemo-previewer.c			\
	nemo-previewer.h			\
	nemo-progress-info-widget.c		\
	nemo-progress-info-widget.h		\
	nemo-progress-ui-handler.c		\
	nemo-progress-ui-handler.h		\
	nemo-properties-window.c		\
	nemo-properties-window.h		\
	nemo-query-editor.c			\
	nemo-query-editor.h			\
    nemo-script-config-widget.c         \
    nemo-script-config-widget.h         \
	nemo-self-check-functions.c 	\
	nemo-self-check-functions.h 	\
	nemo-shell-search-provider.h	\
	nemo-shell-search-provider.c	\
	nemo-special-location-bar.c		\
	nemo-special-location-bar.h		\
    nemo-statusbar.c    \
    nemo-statusbar.h    \
    nemo-thumbnail-problem-bar.c  \
    nemo-thumbnail-problem-bar.h  \
	nemo-toolbar.c			\
	nemo-toolbar.h			\
	nemo-tree-sidebar.c			\
	nemo-tree-sidebar.h			\
	nemo-tree-sidebar-model.c		\
	nemo-tree-sidebar-model.h		\
	nemo-trash-bar.c			\
	nemo-trash-bar.h			\
	nemo-view.c				\
	nemo-view.h				\
	nemo-view-dnd.c			\
	nemo-view-dnd.h			\
	nemo-view-factory.c 		\
	nemo-view-factory.h 		\
	nemo-window-bookmarks.c		\
	nemo-window-bookmarks.h		\
	nemo-window-menus.c         	\
	nemo-window-menus.h         \
	nemo-window-pane.c	\
	nemo-window-pane.h	\
	nemo-window-private.h		\
	nemo-window-slot.c			\
	nemo-window-slot.h			\
	nemo-window-slot-dnd.c		\
	nemo-window-slot-dnd.h		\
	nemo-window.c			\
	nemo-window.h			\
	nemo-x-content-bar.c		\
	nemo-x-content-bar.h		\
	$(NULL)

UNITY_SOURCES = \
	unity-quicklist-handler.c	\
	unity-quicklist-handler.h	\
	unity-bookmarks-handler.c	\
	unity-bookmarks-handler.h	\
	$(NULL)

if HAVE_UNITY
nemo_SOURCES += $(UNITY_SOURCES)
endif

nodist_nemo_SOURCES = \
	$(nemo_built_sources) \
	$(NULL)

EMPTY_VIEW_SOURCES = \
	nemo-empty-view.c 			\
	nemo-empty-view.h

if ENABLE_EMPTY_VIEW
nemo_SOURCES += $(EMPTY_VIEW_SOURCES)
endif

nemo_autorun_software_SOURCES= 			\
	nemo-autorun-software.c			\
	$(NULL)

nemo_connect_server_SOURCES= \
	nemo-connect-server-dialog.c        \
	nemo-connect-server-dialog.h        \
	nemo-connect-server.c		\
	$(NULL)

nemo_open_with_SOURCES = \
    nemo-open-with-main.c \
    $(NULL)

nemo_convert_metadata_SOURCES= \
	nemo-convert-metadata.c	\
	$(NULL)

nemo_extensions_list_SOURCES= \
    nemo-extensions-list.c \
    $(NULL)

BUILT_SOURCES = 					\
	$(nemo_built_sources) 			\
	$(NULL)

polkitdir = $(datadir)/polkit-1/actions
polkit_in_files = org.nemo.root.policy.in

@INTLTOOL_POLICY_RULE@
polkit_DATA = $(polkit_in_files:.policy.in=.policy)

TESTS=check-nemo

@INTLTOOL_SERVER_RULE@

CLEANFILES = \
	$(BUILT_SOURCES)			\
	$(server_DATA)				\
	stamp-nemo-enum-types.h		\
	$(NULL)

EXTRA_DIST =					\
	$(resource_files)			\
	$(server_in_files)			\
	$(polkit_in_files)      \
	nemo.gresource.xml			\
	check-nemo				\
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
