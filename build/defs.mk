# Some variables needed by the modules
SRCDIR=src

CFLAGS+=-DHAVE_CONFIG_H -Wall -pipe -Winline -Wcast-qual -Wcast-align -ansi \
	-Wdeclaration-after-statement -Wmissing-prototypes -Wmissing-declarations
#	-fstack-protector-all -Wstack-protector
#	-Wfloat-equal -Wconversion -Wunreachable-code -Wbad-function-cast \
#	-Wshadow -Wpointer-arith  \
#	-Wbad-function-cast -pedantic -std=c99
#-combine -fwhole-program

ifeq ($(MMX),1)
	CFLAGS+= -DUFO_MMX_ENABLED
endif

ifeq ($(HAVE_SHADERS),1)
	CFLAGS+= -DHAVE_SHADERS
endif

ifeq ($(HAVE_OPENAL),1)
	CFLAGS+= -DHAVE_OPENAL
endif

# Common things
_BUILDDIR=$(strip $(BUILDDIR))
_MODULE=$(strip $(MODULE))
_SRCDIR=$(strip $(SRCDIR))

LIBTOOL_LD=$(LIBTOOL) --silent --mode=link $(CC) -module -rpath / $(LDFLAGS) $(KIBS)
LIBTOOL_CC=$(LIBTOOL) --silent --mode=compile $(CC) -prefer-pic $(CFLAGS)

# Target options

ifeq ($(BUILD_DEBUG),1)
    BUILDDIR=debug-$(TARGET_OS)-$(TARGET_CPU)
    CFLAGS+=-ggdb -O0 -DDEBUG $(PROFILER_CFLAGS) -fno-inline
# -fvar-tracking
else
    BUILDDIR=release-$(TARGET_OS)-$(TARGET_CPU)
    CFLAGS+=-DNDEBUG $(RELEASE_CFLAGS) -O2
endif

ifeq ($(PROFILING),1)
    CFLAGS+=-pg -DPROFILING -fprofile-arcs -ftest-coverage
    LNKFLAGS+=-pg -lgcov
endif

ifeq ($(PARANOID),1)
    CFLAGS+=-DPARANOID
endif
