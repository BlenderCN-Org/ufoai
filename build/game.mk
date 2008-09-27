GAME_CFLAGS+=-DCOMPILE_UFO

GAME_SRCS=\
	game/q_shared.c \
	game/g_ai.c \
	game/g_client.c \
	game/g_combat.c \
	game/g_cmds.c \
	game/g_phys.c \
	game/g_reaction.c \
	game/g_main.c \
	game/g_spawn.c \
	game/g_svcmds.c \
	game/g_utils.c \
	game/g_func.c \
	game/g_trigger.c \
	game/inv_shared.c \
	shared/mathlib.c \
	shared/shared.c \
	shared/infostring.c \
	game/lua/lapi.c \
	game/lua/lauxlib.c \
	game/lua/lbaselib.c \
	game/lua/lcode.c \
	game/lua/ldblib.c \
	game/lua/ldebug.c \
	game/lua/ldo.c \
	game/lua/ldump.c \
	game/lua/lfunc.c \
	game/lua/lgc.c \
	game/lua/linit.c \
	game/lua/liolib.c \
	game/lua/llex.c \
	game/lua/lmathlib.c \
	game/lua/lmem.c \
	game/lua/loadlib.c \
	game/lua/lobject.c \
	game/lua/lopcodes.c \
	game/lua/loslib.c \
	game/lua/lparser.c \
	game/lua/lstate.c \
	game/lua/lstring.c \
	game/lua/lstrlib.c \
	game/lua/ltable.c \
	game/lua/ltablib.c \
	game/lua/ltm.c \
	game/lua/lua.c \
	game/lua/lundump.c \
	game/lua/lvm.c \
	game/lua/lzio.c \
	game/lua/print.c


GAME_OBJS=$(GAME_SRCS:%.c=$(_BUILDDIR)/game/%.o)
GAME_TARGET=base/game.$(SHARED_EXT)

# Add the list of all project object files and dependencies
ALL_OBJS += $(GAME_OBJS)
TARGETS +=$(GAME_TARGET)

# Say how about to build the target
$(GAME_TARGET) : $(GAME_OBJS) $(BUILDDIR)/.dirs
	@echo " * [GAM] ... linking $(LNKFLAGS) ($(GAME_LIBS))"; \
		$(CC) $(LDFLAGS) $(SHARED_LDFLAGS) -o $@ $(GAME_OBJS) $(GAME_LIBS) $(LNKFLAGS)

# Say how to build .o files from .c files for this module
$(BUILDDIR)/game/%.o: $(SRCDIR)/%.c $(BUILDDIR)/.dirs
	@echo " * [GAM] $<"; \
		$(CC) $(CFLAGS) $(SHARED_CFLAGS) $(GAME_CFLAGS) -o $@ -c $< $(CFLAGS_M_OPTS)
