NULL =

public_icons_themes = \
	hicolor	\
	$(NULL)

public_icons = \
	hicolor_apps_16x16_nemo.png \
	hicolor_apps_22x22_nemo.png \
	hicolor_apps_24x24_nemo.png \
	hicolor_apps_32x32_nemo.png \
	hicolor_apps_scalable_nemo.svg \
	hicolor_actions_16x16_nemo-eject.png \
	hicolor_actions_32x32_nemo-eject.png \
	hicolor_actions_scalable_view-compact-symbolic.svg \
	hicolor_actions_scalable_location-symbolic.svg \
	hicolor_actions_scalable_sidebar-hide-symbolic.svg \
	hicolor_actions_scalable_sidebar-show-symbolic.svg \
	hicolor_actions_scalable_sidebar-tree-symbolic.svg \
	hicolor_actions_scalable_sidebar-places-symbolic.svg \
	hicolor_actions_scalable_expand-menu-symbolic.svg \
	hicolor_actions_scalable_expand-menu-hover-symbolic.svg \
	hicolor_actions_scalable_collapse-menu-symbolic.svg \
	hicolor_actions_scalable_collapse-menu-hover-symbolic.svg \
	hicolor_status_48x48_progress-0.png \
	hicolor_status_48x48_progress-10.png \
	hicolor_status_48x48_progress-20.png \
	hicolor_status_48x48_progress-30.png \
	hicolor_status_48x48_progress-40.png \
	hicolor_status_48x48_progress-50.png \
	hicolor_status_48x48_progress-60.png \
	hicolor_status_48x48_progress-70.png \
	hicolor_status_48x48_progress-80.png \
	hicolor_status_48x48_progress-90.png \
	hicolor_status_48x48_progress-100.png \
	$(NULL)

private_icons = \
	$(NULL)

noinst_DATA = \
	hicolor_apps_16x16_nemo.svg \
	hicolor_apps_22x22_nemo.svg \
	hicolor_apps_32x32_nemo.svg \
	$(NULL)

EXTRA_DIST = \
	$(public_icons)		\
	$(private_icons)	\
	$(noinst_DATA)		\
	$(NULL)


###############################################################################

gtk_update_icon_cache = gtk-update-icon-cache -f -t

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		for theme in $(public_icons_themes); do \
			$(gtk_update_icon_cache) $(datadir)/icons/$$theme; \
		done; \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		for theme in $(public_icons_themes); do \
			echo "***   $(gtk_update_icon_cache) $(datadir)/icons/$$theme"; \
		done; \
	fi

install-icons:
	for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

uninstall-icons:
	-for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

install-data-local: install-icons update-icon-cache

uninstall-local: uninstall-icons update-icon-cache
