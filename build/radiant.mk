########################################################################################################
# radiant
########################################################################################################

RADIANT_BASE = tools/radiant

RADIANT_CFLAGS+=-Isrc/$(RADIANT_BASE)/libs -Isrc/$(RADIANT_BASE)/include

RADIANT_SRCS = \
	$(RADIANT_BASE)/radiant/autosave.cpp \
	$(RADIANT_BASE)/radiant/brush.cpp \
	$(RADIANT_BASE)/radiant/brushmanip.cpp \
	$(RADIANT_BASE)/radiant/brushmodule.cpp \
	$(RADIANT_BASE)/radiant/brush_primit.cpp \
	$(RADIANT_BASE)/radiant/camwindow.cpp \
	$(RADIANT_BASE)/radiant/colorscheme.cpp \
	$(RADIANT_BASE)/radiant/commands.cpp \
	$(RADIANT_BASE)/radiant/console.cpp \
	$(RADIANT_BASE)/radiant/csg.cpp \
	$(RADIANT_BASE)/radiant/dialog.cpp \
	$(RADIANT_BASE)/radiant/eclass.cpp \
	$(RADIANT_BASE)/radiant/eclass_def.cpp \
	$(RADIANT_BASE)/radiant/entity.cpp \
	$(RADIANT_BASE)/radiant/environment.cpp \
	$(RADIANT_BASE)/radiant/error.cpp \
	$(RADIANT_BASE)/radiant/filetypes.cpp \
	$(RADIANT_BASE)/radiant/filters.cpp \
	$(RADIANT_BASE)/radiant/grid.cpp \
	$(RADIANT_BASE)/radiant/groupdialog.cpp \
	$(RADIANT_BASE)/radiant/gtkmisc.cpp \
	$(RADIANT_BASE)/radiant/image.cpp \
	$(RADIANT_BASE)/radiant/lastused.cpp \
	$(RADIANT_BASE)/radiant/main.cpp \
	$(RADIANT_BASE)/radiant/mainframe.cpp \
	$(RADIANT_BASE)/radiant/map.cpp \
	$(RADIANT_BASE)/radiant/multimon.cpp \
	$(RADIANT_BASE)/radiant/nullmodel.cpp \
	$(RADIANT_BASE)/radiant/parse.cpp \
	$(RADIANT_BASE)/radiant/pathfinding.cpp \
	$(RADIANT_BASE)/radiant/plugin.cpp \
	$(RADIANT_BASE)/radiant/pluginmanager.cpp \
	$(RADIANT_BASE)/radiant/pluginmenu.cpp \
	$(RADIANT_BASE)/radiant/plugintoolbar.cpp \
	$(RADIANT_BASE)/radiant/preferences.cpp \
	$(RADIANT_BASE)/radiant/qe3.cpp \
	$(RADIANT_BASE)/radiant/qgl.cpp \
	$(RADIANT_BASE)/radiant/referencecache.cpp \
	$(RADIANT_BASE)/radiant/renderstate.cpp \
	$(RADIANT_BASE)/radiant/scenegraph.cpp \
	$(RADIANT_BASE)/radiant/select.cpp \
	$(RADIANT_BASE)/radiant/selection.cpp \
	$(RADIANT_BASE)/radiant/server.cpp \
	$(RADIANT_BASE)/radiant/stacktrace.cpp \
	$(RADIANT_BASE)/radiant/texmanip.cpp \
	$(RADIANT_BASE)/radiant/textures.cpp \
	$(RADIANT_BASE)/radiant/texwindow.cpp \
	$(RADIANT_BASE)/radiant/timer.cpp \
	$(RADIANT_BASE)/radiant/treemodel.cpp \
	$(RADIANT_BASE)/radiant/undo.cpp \
	$(RADIANT_BASE)/radiant/url.cpp \
	$(RADIANT_BASE)/radiant/view.cpp \
	$(RADIANT_BASE)/radiant/winding.cpp \
	$(RADIANT_BASE)/radiant/windowobservers.cpp \
	$(RADIANT_BASE)/radiant/xywindow.cpp \
	\
	$(RADIANT_BASE)/radiant/sidebar/sidebar.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entitylist.cpp \
	$(RADIANT_BASE)/radiant/sidebar/entityinspector.cpp \
	\
	$(RADIANT_BASE)/radiant/dialogs/texteditor.cpp \
	$(RADIANT_BASE)/radiant/dialogs/about.cpp \
	$(RADIANT_BASE)/radiant/dialogs/findbrush.cpp \
	$(RADIANT_BASE)/radiant/dialogs/prism.cpp \
	$(RADIANT_BASE)/radiant/dialogs/light.cpp \
	$(RADIANT_BASE)/radiant/dialogs/maptools.cpp \
	$(RADIANT_BASE)/radiant/dialogs/particle.cpp \
	$(RADIANT_BASE)/radiant/dialogs/findtextures.cpp \
	\
	$(RADIANT_BASE)/radiant/surfaceinspector/surfaceinspector.cpp \
	\
	$(RADIANT_BASE)/libs/gtkutil/accelerator.cpp \
	$(RADIANT_BASE)/libs/gtkutil/button.cpp \
	$(RADIANT_BASE)/libs/gtkutil/clipboard.cpp \
	$(RADIANT_BASE)/libs/gtkutil/cursor.cpp \
	$(RADIANT_BASE)/libs/gtkutil/dialog.cpp \
	$(RADIANT_BASE)/libs/gtkutil/filechooser.cpp \
	$(RADIANT_BASE)/libs/gtkutil/frame.cpp \
	$(RADIANT_BASE)/libs/gtkutil/glfont.cpp \
	$(RADIANT_BASE)/libs/gtkutil/glwidget.cpp \
	$(RADIANT_BASE)/libs/gtkutil/image.cpp \
	$(RADIANT_BASE)/libs/gtkutil/menu.cpp \
	$(RADIANT_BASE)/libs/gtkutil/messagebox.cpp \
	$(RADIANT_BASE)/libs/gtkutil/paned.cpp \
	$(RADIANT_BASE)/libs/gtkutil/toolbar.cpp \
	$(RADIANT_BASE)/libs/gtkutil/window.cpp \
	\
	$(RADIANT_BASE)/libs/cmdlib/cmdlib.cpp \
	\
	$(RADIANT_BASE)/libs/profile/profile.cpp \
	$(RADIANT_BASE)/libs/profile/file.cpp

RADIANT_OBJS=$(RADIANT_SRCS:%.cpp=$(BUILDDIR)/tools/radiant/%.o)
RADIANT_TARGET=radiant/radiant$(EXE_EXT)

# model plugin
RADIANT_PLUGIN_MODEL_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/model/plugin.cpp \
	$(RADIANT_BASE)/plugins/model/model.cpp
RADIANT_PLUGIN_MODEL_SRCS_C = \
	$(RADIANT_BASE)/libs/picomodel/picointernal.c \
	$(RADIANT_BASE)/libs/picomodel/picomodel.c \
	$(RADIANT_BASE)/libs/picomodel/picomodules.c \
	$(RADIANT_BASE)/libs/picomodel/pm_3ds.c \
	$(RADIANT_BASE)/libs/picomodel/pm_ase.c \
	$(RADIANT_BASE)/libs/picomodel/pm_md3.c \
	$(RADIANT_BASE)/libs/picomodel/pm_obj.c \
	$(RADIANT_BASE)/libs/picomodel/pm_ms3d.c \
	$(RADIANT_BASE)/libs/picomodel/pm_mdc.c \
	$(RADIANT_BASE)/libs/picomodel/pm_md2.c \
	$(RADIANT_BASE)/libs/picomodel/pm_terrain.c

RADIANT_PLUGIN_MODEL_C_OBJS=$(RADIANT_PLUGIN_MODEL_SRCS_C:%.c=$(BUILDDIR)/tools/radiant/plugins_c/%.o)
RADIANT_PLUGIN_MODEL_CPP_OBJS=$(RADIANT_PLUGIN_MODEL_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_MODEL_TARGET=radiant/modules/model.$(SHARED_EXT)

#entity plugin
RADIANT_PLUGIN_ENTITY_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/entity/plugin.cpp \
	$(RADIANT_BASE)/plugins/entity/entity.cpp \
	$(RADIANT_BASE)/plugins/entity/eclassmodel.cpp \
	$(RADIANT_BASE)/plugins/entity/generic.cpp \
	$(RADIANT_BASE)/plugins/entity/group.cpp \
	$(RADIANT_BASE)/plugins/entity/miscmodel.cpp \
	$(RADIANT_BASE)/plugins/entity/miscparticle.cpp \
	$(RADIANT_BASE)/plugins/entity/filters.cpp \
	$(RADIANT_BASE)/plugins/entity/light.cpp \
	$(RADIANT_BASE)/plugins/entity/skincache.cpp \
	$(RADIANT_BASE)/plugins/entity/targetable.cpp

RADIANT_PLUGIN_ENTITY_CPP_OBJS=$(RADIANT_PLUGIN_ENTITY_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_ENTITY_TARGET=radiant/modules/entity.$(SHARED_EXT)

#image plugin
RADIANT_PLUGIN_IMAGE_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/image/image.cpp \
	$(RADIANT_BASE)/plugins/image/jpeg.cpp \
	$(RADIANT_BASE)/plugins/image/tga.cpp

RADIANT_PLUGIN_IMAGE_CPP_OBJS=$(RADIANT_PLUGIN_IMAGE_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_IMAGE_TARGET=radiant/modules/image.$(SHARED_EXT)

#map plugin
RADIANT_PLUGIN_MAP_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/map/plugin.cpp \
	$(RADIANT_BASE)/plugins/map/parse.cpp \
	$(RADIANT_BASE)/plugins/map/write.cpp

RADIANT_PLUGIN_MAP_CPP_OBJS=$(RADIANT_PLUGIN_MAP_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_MAP_TARGET=radiant/modules/map.$(SHARED_EXT)

#shaders plugin
RADIANT_PLUGIN_SHADERS_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/shaders/shaders.cpp \
	$(RADIANT_BASE)/plugins/shaders/plugin.cpp \
	\
	$(RADIANT_BASE)/libs/cmdlib/cmdlib.cpp

RADIANT_PLUGIN_SHADERS_CPP_OBJS=$(RADIANT_PLUGIN_SHADERS_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_SHADERS_TARGET=radiant/modules/shaders.$(SHARED_EXT)

#vfspk3 plugin
RADIANT_PLUGIN_VFSPK3_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/vfspk3/vfspk3.cpp \
	$(RADIANT_BASE)/plugins/vfspk3/vfs.cpp \
	$(RADIANT_BASE)/plugins/vfspk3/archive.cpp

RADIANT_PLUGIN_VFSPK3_CPP_OBJS=$(RADIANT_PLUGIN_VFSPK3_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_VFSPK3_TARGET=radiant/modules/vfspk3.$(SHARED_EXT)

#archivezip plugin
RADIANT_PLUGIN_ARCHIVEZIP_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/archivezip/plugin.cpp \
	$(RADIANT_BASE)/plugins/archivezip/archive.cpp \
	\
	$(RADIANT_BASE)/libs/cmdlib/cmdlib.cpp

RADIANT_PLUGIN_ARCHIVEZIP_CPP_OBJS=$(RADIANT_PLUGIN_ARCHIVEZIP_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_ARCHIVEZIP_TARGET=radiant/modules/archivezip.$(SHARED_EXT)

# PLUGINS

#ufoai plugin
RADIANT_PLUGIN_UFOAI_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/ufoai/ufoai.cpp \
	$(RADIANT_BASE)/plugins/ufoai/ufoai_filters.cpp \
	$(RADIANT_BASE)/plugins/ufoai/ufoai_gtk.cpp

RADIANT_PLUGIN_UFOAI_CPP_OBJS=$(RADIANT_PLUGIN_UFOAI_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_UFOAI_TARGET=radiant/plugins/ufoai.$(SHARED_EXT)

RADIANT_PLUGIN_BRUSHEXPORT_SRCS_CPP = \
	$(RADIANT_BASE)/plugins/brushexport/callbacks.cpp \
	$(RADIANT_BASE)/plugins/brushexport/export.cpp \
	$(RADIANT_BASE)/plugins/brushexport/interface.cpp \
	$(RADIANT_BASE)/plugins/brushexport/plugin.cpp \
	$(RADIANT_BASE)/plugins/brushexport/support.cpp

RADIANT_PLUGIN_BRUSHEXPORT_CPP_OBJS=$(RADIANT_PLUGIN_BRUSHEXPORT_SRCS_CPP:%.cpp=$(BUILDDIR)/tools/radiant/plugins_cpp/%.o)
RADIANT_PLUGIN_BRUSHEXPORT_TARGET=radiant/plugins/brushexport.$(SHARED_EXT)

ifeq ($(BUILD_GTKRADIANT),1)
	ALL_RADIANT_OBJS+=$(RADIANT_OBJS) $(RADIANT_PLUGIN_MODEL_C_OBJS) $(RADIANT_PLUGIN_MODEL_CPP_OBJS) \
		$(RADIANT_PLUGIN_IMAGE_CPP_OBJS) $(RADIANT_PLUGIN_MAP_CPP_OBJS) $(RADIANT_PLUGIN_ENTITY_CPP_OBJS) \
		$(RADIANT_PLUGIN_SHADERS_CPP_OBJS) $(RADIANT_PLUGIN_VFSPK3_CPP_OBJS) $(RADIANT_PLUGIN_ARCHIVEZIP_CPP_OBJS) \
		$(RADIANT_PLUGIN_UFOAI_CPP_OBJS) $(RADIANT_PLUGIN_BRUSHEXPORT_CPP_OBJS)

RADIANT_DEPS = $(patsubst %.o, %.d, $(ALL_RADIANT_OBJS))

clean-radiant:
	@echo "Making clean radiant"
	@rm -f $(ALL_RADIANT_OBJS) $(RADIANT_DEPS)

radiant: $(RADIANT_PLUGIN_MODEL_TARGET) $(RADIANT_PLUGIN_ENTITY_TARGET) $(RADIANT_PLUGIN_IMAGE_TARGET) \
		$(RADIANT_PLUGIN_MAP_TARGET) $(RADIANT_PLUGIN_SHADERS_TARGET) $(RADIANT_PLUGIN_VFSPK3_TARGET) \
		$(RADIANT_PLUGIN_ARCHIVEZIP_TARGET) $(RADIANT_PLUGIN_UFOAI_TARGET) $(RADIANT_PLUGIN_BRUSHEXPORT_TARGET) \
		$(RADIANT_TARGET)
else
radiant:
	@echo "Radiant is not enabled - use './configure --enable-gtkradiant'"

clean-radiant:
	@echo "Radiant is not enabled - use './configure --enable-gtkradiant'"

endif

# Say how to build .o files from .cpp files for this module
$(BUILDDIR)/tools/radiant/%.o: $(SRCDIR)/%.cpp $(BUILDDIR)/.dirs
	@echo " * [RAD] $<"; \
		$(CC) $(CPPFLAGS) $(RADIANT_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)

# Say how to build .o files from .cpp/.c files for this module
$(BUILDDIR)/tools/radiant/plugins_c/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [RAD] $<"; \
		$(CC) $(CFLAGS) $(SHARED_CFLAGS) $(RADIANT_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)
$(BUILDDIR)/tools/radiant/plugins_cpp/%.o: $(SRCDIR)/%.cpp $(BUILDDIR)/.dirs
	@echo " * [RAD] $<"; \
		$(CC) $(CPPFLAGS) $(SHARED_CFLAGS) $(RADIANT_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)

# Say how about to build the modules
$(RADIANT_PLUGIN_MODEL_TARGET) : $(RADIANT_PLUGIN_MODEL_CPP_OBJS) $(RADIANT_PLUGIN_MODEL_C_OBJS) $(BUILDDIR)/.dirs
	@echo " * [RAD] ... linking $(LNKFLAGS)"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_MODEL_C_OBJS) $(RADIANT_PLUGIN_MODEL_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)
$(RADIANT_PLUGIN_ENTITY_TARGET) : $(RADIANT_PLUGIN_ENTITY_CPP_OBJS) $(BUILDDIR)/.dirs
	@echo " * [RAD] ... linking $(LNKFLAGS)"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_ENTITY_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)
$(RADIANT_PLUGIN_IMAGE_TARGET) : $(RADIANT_PLUGIN_IMAGE_CPP_OBJS) $(BUILDDIR)/.dirs
	@echo " * [RAD] ... linking $(LNKFLAGS)"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_IMAGE_CPP_OBJS) $(LNKFLAGS) -ljpeg
$(RADIANT_PLUGIN_MAP_TARGET) : $(RADIANT_PLUGIN_MAP_CPP_OBJS) $(BUILDDIR)/.dirs
	@echo " * [RAD] ... linking $(LNKFLAGS)"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_MAP_CPP_OBJS) $(LNKFLAGS)
$(RADIANT_PLUGIN_SHADERS_TARGET) : $(RADIANT_PLUGIN_SHADERS_CPP_OBJS) $(BUILDDIR)/.dirs
	@echo " * [RAD] ... linking $(LNKFLAGS)"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_SHADERS_CPP_OBJS) $(LNKFLAGS)
$(RADIANT_PLUGIN_VFSPK3_TARGET) : $(RADIANT_PLUGIN_VFSPK3_CPP_OBJS) $(BUILDDIR)/.dirs
	@echo " * [RAD] ... linking $(LNKFLAGS)"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_VFSPK3_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)
$(RADIANT_PLUGIN_ARCHIVEZIP_TARGET) : $(RADIANT_PLUGIN_ARCHIVEZIP_CPP_OBJS) $(BUILDDIR)/.dirs
	@echo " * [RAD] ... linking $(LNKFLAGS)"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_ARCHIVEZIP_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)

# and now link the plugins
$(RADIANT_PLUGIN_UFOAI_TARGET) : $(RADIANT_PLUGIN_UFOAI_CPP_OBJS) $(BUILDDIR)/.dirs
	@echo " * [RAD] ... linking $(LNKFLAGS)"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_UFOAI_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)
$(RADIANT_PLUGIN_BRUSHEXPORT_TARGET) : $(RADIANT_PLUGIN_BRUSHEXPORT_CPP_OBJS) $(BUILDDIR)/.dirs
	@echo " * [RAD] ... linking $(LNKFLAGS)"; \
		$(CPP) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(RADIANT_PLUGIN_BRUSHEXPORT_CPP_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)

# Say how to link the exe
$(RADIANT_TARGET): $(RADIANT_OBJS) $(BUILDDIR)/.dirs
	@echo " * [RAD] ... linking $(LNKFLAGS) ($(RADIANT_LIBS))"; \
		$(CPP) $(LDFLAGS) -o $@ $(RADIANT_OBJS) $(RADIANT_LIBS) $(LNKFLAGS)
