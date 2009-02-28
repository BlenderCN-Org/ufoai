########################################################################################################
# ufo2map
########################################################################################################

UFO2MAP_CFLAGS+=-DCOMPILE_MAP -ffloat-store

UFO2MAP_SRCS = \
	tools/ufo2map/ufo2map.c \
	tools/ufo2map/lighting.c \
	tools/ufo2map/bsp.c \
	tools/ufo2map/brushbsp.c \
	tools/ufo2map/csg.c \
	tools/ufo2map/faces.c \
	tools/ufo2map/levels.c \
	tools/ufo2map/lightmap.c \
	tools/ufo2map/map.c \
	tools/ufo2map/patches.c \
	tools/ufo2map/portals.c \
	tools/ufo2map/routing.c \
	tools/ufo2map/textures.c \
	tools/ufo2map/tree.c \
	tools/ufo2map/threads.c \
	tools/ufo2map/writebsp.c \
	tools/ufo2map/check/checklib.c \
	tools/ufo2map/check/check.c \
	tools/ufo2map/common/aselib.c \
	tools/ufo2map/common/bspfile.c \
	tools/ufo2map/common/cmdlib.c \
	tools/ufo2map/common/polylib.c \
	tools/ufo2map/common/scriplib.c \
	tools/ufo2map/common/trace.c \
	tools/ufo2map/common/imagelib.c \
	\
	shared/mathlib.c \
	shared/byte.c \
	shared/parse.c \
	shared/shared.c \
	shared/entitiesdef.c \
	\
	common/unzip.c \
	common/tracing.c \
	common/routing.c \
	common/ioapi.c

UFO2MAP_OBJS=$(UFO2MAP_SRCS:%.c=$(BUILDDIR)/tools/ufo2map/%.o)
UFO2MAP_TARGET=ufo2map$(EXE_EXT)

ifeq ($(BUILD_UFO2MAP),1)
	ALL_OBJS+=$(UFO2MAP_OBJS)
	TARGETS+=$(UFO2MAP_TARGET)
endif

# Say how to link the exe
$(UFO2MAP_TARGET): $(UFO2MAP_OBJS) $(BUILDDIR)/.dirs
	@echo " * [MAP] ... linking $(LNKFLAGS) ($(TOOLS_LIBS) $(SDL_LIBS))"; \
		$(CC) $(LDFLAGS) -o $@ $(UFO2MAP_OBJS) $(TOOLS_LIBS) $(SDL_LIBS) $(LNKFLAGS)

# Say how to build .o files from .c files for this module
# -ffloat-store option to ensure that maps are the same on every plattform
# store the float values in buffers, not in cpu registers, maybe slower
$(BUILDDIR)/tools/ufo2map/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [MAP] $<"; \
		$(CC) $(CFLAGS) $(UFO2MAP_CFLAGS) $(SDL_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)
