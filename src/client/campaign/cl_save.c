/**
 * @file cl_save.c
 * @brief Implements savegames
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../client.h"
#include "../cl_game.h"
#include "../cl_menu.h"
#include "../menu/m_popup.h"
#include "../mxml/mxml_ufoai.h"
#include "cl_campaign.h"
#include "cl_save.h"
#include "cp_hospital.h"
#include "cl_ufo.h"
#include "cl_alienbase.h"
#include "cp_time.h"

typedef struct saveFileHeaderXML_s {
	int version; /**< which savegame version */
	int compressed; /**< is this file compressed via zlib */
	int dummy[14]; /**< maybe we have to extend this later */
	char gameVersion[16]; /**< game version that was used to save this file */
	char name[32]; /**< savefile name */
	char gameDate[32]; /**< internal game date */
	char realDate[32]; /**< real datestring when the user saved this game */
	long xml_size;
} saveFileHeader_t;

typedef struct saveSubsystems_s {
	const char *name;
	qboolean (*save) (mxml_node_t *parent);	/**< return false if saving failed */
	qboolean (*load) (mxml_node_t *parent);	/**< return false if loading failed */
} saveSubsystems_t;

static saveSubsystems_t saveSubsystems[MAX_SAVESUBSYSTEMS];
static int saveSubsystemsAmount;
static cvar_t* save_compressed;
qboolean loading = qfalse;

/**
 * @brief Perform actions after loading a game for single player campaign
 * @sa SAV_GameLoad
 */
static qboolean SAV_GameActionsAfterLoad (char **error)
{
	RS_PostLoadInit();

	B_PostLoadInit();

	/* Make sure the date&time is displayed when loading. */
	CL_UpdateTime();

	/* Update number of UFO detected by radar */
	RADAR_SetRadarAfterLoading();

	return qtrue;
}

/**
 * @brief Tries to verify the Header of the Xml File
 * @param[in] header a pointer to the header to verify
 */
static qboolean SAV_VerifyXMLHeader (saveFileHeader_t const * const header)
{
	int len;
	/*check the length of the string*/
	len = strlen(header->name);
	if (len < 0 || len > sizeof(header->name)) {
		Com_DPrintf(DEBUG_CLIENT, "Name is %d Bytes long, max is "UFO_SIZE_T"\n", len, sizeof(header->name));
		return qfalse;
	}
	len = strlen(header->gameVersion);
	if (len < 0 || len > sizeof(header->gameVersion)) {
		Com_DPrintf(DEBUG_CLIENT, "gameVersion is %d Bytes long, max is "UFO_SIZE_T"\n", len, sizeof(header->gameVersion));
		return qfalse;
	}
	len = strlen(header->gameDate);
	if (len < 0 || len > sizeof(header->gameDate)) {
		Com_DPrintf(DEBUG_CLIENT, "gameDate is %d Bytes long, max is "UFO_SIZE_T"\n", len, sizeof(header->gameDate));
		return qfalse;
	}
	len = strlen(header->realDate);
	if (len < 0 || len > sizeof(header->realDate)) {
		Com_DPrintf(DEBUG_CLIENT, "gameVersion is %d Bytes long, max is "UFO_SIZE_T"\n", len, sizeof(header->realDate));
		return qfalse;
	}

	/* saved games should not be bigger than 15MB */
	if (header->xml_size < 0 || header->xml_size > 15 * 1024 * 1024) {
		Com_DPrintf(DEBUG_CLIENT, "Save size seems to be to large (over 15 MB) %ld.\n", header->xml_size);
		return qfalse;
	}
	if (header->version < 0) {
		Com_DPrintf(DEBUG_CLIENT, "Version is less than zero!\n");
		return qfalse;
	}
	if (header->version > SAVE_FILE_VERSION) {
		Com_Printf("Savefile is newer than the game!\n");
	}
	return qtrue;
}

/**
 * @brief Loads the given savegame from an xml File.
 * @return true on load success false on failures
 * @param[in] file The Filename to load from (without extension)
 * @param[out] error On failure an errormessage may be set.
 */
static qboolean SAV_GameLoad (const char *file, char **error)
{
	uLongf len;
	char filename[MAX_OSPATH];
	qFILE f;
	byte *cbuf, *buf;
	int i, clen;
	mxml_node_t *top_node, *node;
	saveFileHeader_t header;

	Q_strncpyz(filename, file, sizeof(filename));

	/* open file */
	FS_OpenFile(va("save/%s.xml", filename), &f, FILE_READ);
	if (!f.f) {
		Com_Printf("Couldn't open file '%s'\n", filename);
		return qfalse;
	}

	clen = FS_FileLength(&f);
	cbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * clen, cl_genericPool, 0);
	if (FS_Read(cbuf, clen, &f) != clen)
		Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
	FS_CloseFile(&f);
	Com_Printf("Loading savegame xml (size %d)\n", clen);

	memcpy(&header, cbuf, sizeof(header));
	/* swap all int values if needed */
	header.compressed = LittleLong(header.compressed);
	header.version = LittleLong(header.version);
	/* doing some header verification */
	if (!SAV_VerifyXMLHeader(&header)) {
		/* our header is not valid, we MUST abort loading the game! */
		Com_Printf("The Header of the savegame '%s.xml' is corrupted. Loading aborted\n", filename);
		Mem_Free(cbuf);
		return qfalse;
	}

	Com_Printf("Loading savegame\n"
			"...version: %i\n"
			"...game version: %s\n"
			"...xml Size: %ld, compressed? %c\n"
			, header.version, header.gameVersion, header.xml_size, header.compressed?'y':'n');
	len = header.xml_size+50;
	buf = (byte *) Mem_PoolAlloc(sizeof(byte)*len, cl_genericPool, 0);

	if (header.compressed) {
		/* uncompress data, skipping comment header */
		const int res = uncompress(buf, &len, cbuf + sizeof(header), clen - sizeof(header));
		Mem_Free(cbuf);

		if (res != Z_OK) {
			Mem_Free(buf);
			*error = _("Error decompressing data");
			Com_Printf("Error decompressing data in '%s'.\n", filename);
			return qfalse;
		}
		top_node = mxmlLoadString(NULL, (char*)buf , mxml_ufo_type_cb);
		if (!top_node) {
			Mem_Free(buf);
			Com_Printf("Error: Failure in Loading the xml Data!");
			return qfalse;
		}
	} else {
		/*memcpy(buf, cbuf + sizeof(header), clen - sizeof(header));*/
		top_node = mxmlLoadString(NULL, (char*)(cbuf + sizeof(header)) , mxml_ufo_type_cb);
		Mem_Free(cbuf);
		if (!top_node) {
			Mem_Free(buf);
			Com_Printf("Error: Failure in Loading the xml Data!");
			return qfalse;
		}
	}

	/* doing a subsystem run ;) */
	GAME_RestartMode(GAME_CAMPAIGN);
	node = mxml_GetNode(top_node, "savegame");
	if (!node) {
		Com_Printf("Error: Failure in Loading the xml Data! (savegame node not found)");
		Mem_Free(buf);
		return qfalse;
	}

	Com_Printf("Load '%s' %d subsystems\n", filename, saveSubsystemsAmount);
	for (i = 0; i < saveSubsystemsAmount; i++) {
		Com_Printf("...Running subsystem '%s'\n", saveSubsystems[i].name);
		if (!saveSubsystems[i].load(node)) {
			Com_Printf("...subsystem '%s' returned false - savegame could not be loaded\n", saveSubsystems[i].name);
			loading = qfalse;
			return qfalse;
		} else
			Com_Printf("...subsystem '%s' - loaded.\n", saveSubsystems[i].name);
	}
	mxmlDelete(node);

	SAV_GameActionsAfterLoad(error);

	loading = qfalse;

	assert(GAME_IsCampaign());

	CL_Drop();

	Com_Printf("File '%s' successfully loaded from %s xml savegame.\n", filename, header.compressed?"compressed":"");
	Mem_Free(buf);
	return qtrue;
}

/**
 * @brief This is a savegame function which stores the game in xml-Format.
 */
static qboolean SAV_GameSave (const char *filename, const char *comment, char **error)
{
	mxml_node_t *top_node, *node;
	char savegame[MAX_OSPATH];
	int res;
	int requiredbuflen;
	byte *buf, *fbuf;
	uLongf bufLen;
	saveFileHeader_t header;
	char dummy[2];
	int i;
	dateLong_t date;
	char message[30];
#ifdef DEBUG
	char savegame_debug[MAX_OSPATH];
#endif

	if (!GAME_CP_IsRunning()) {
		*error = _("No campaign active.");
		Com_Printf("Error: No campaign active.\n");
		return qfalse;
	}

	if (!ccs.numBases) {
		*error = _("Nothing to save yet.");
		Com_Printf("Error: Nothing to save yet.\n");
		return qfalse;
	}

	Com_sprintf(savegame, sizeof(savegame), "save/%s.xml", filename);
#ifdef DEBUG
	Com_sprintf(savegame_debug, sizeof(savegame_debug), "save/%s.lint", filename);
#endif
	top_node = mxmlNewXML("1.0");
	node = mxml_AddNode(top_node, "savegame");
	/* writing  Header */
	mxml_AddInt(node, "saveversion", SAVE_FILE_VERSION);
	mxml_AddString(node, "comment", comment);
	mxml_AddString(node, "version", UFO_VERSION);
	CL_DateConvertLong(&ccs.date, &date);
	Com_sprintf(message, sizeof(message), _("%i %s %02i"),
		date.year, Date_GetMonthName(date.month - 1), date.day);
	mxml_AddString(node, "gamedate", message);
	/* working through all subsystems. perhaps we should redesign it, order is not important anymore */
	Com_Printf("Calling subsystems\n");
	for (i = 0; i < saveSubsystemsAmount; i++) {
		if (!saveSubsystems[i].save(node))
			Com_Printf("...subsystem '%s' failed to save the data\n", saveSubsystems[i].name);
		else
			Com_Printf("...subsystem '%s' - saved\n", saveSubsystems[i].name);
	}

	/* calculate the needed buffer size */

	memset(&header, 0, sizeof(header));
	header.compressed = LittleLong(save_compressed->integer);
	header.version = LittleLong(SAVE_FILE_VERSION);
	Q_strncpyz(header.name, comment, sizeof(header.name));
	Q_strncpyz(header.gameVersion, UFO_VERSION, sizeof(header.gameVersion));
	CL_DateConvertLong(&ccs.date, &date);
	Com_sprintf(header.gameDate, sizeof(header.gameDate), _("%i %s %02i"),
		date.year, Date_GetMonthName(date.month - 1), date.day);
	/** @todo fill real date string */

	requiredbuflen = mxmlSaveString(top_node, dummy, 2, MXML_NO_CALLBACK);

	/** @todo little/big endian */
	header.xml_size = requiredbuflen;
	buf = (byte *) Mem_PoolAlloc(sizeof(byte) * requiredbuflen+1, cl_genericPool, 0);
	if (!buf) {
		*error = _("Could not allocate enough memory to save this game");
		Com_Printf("Error: Could not allocate enough memory to save this game\n");
		return qfalse;
	}
	res = mxmlSaveString(top_node, (char*)buf, requiredbuflen+1, MXML_NO_CALLBACK);
	Com_Printf("XML Written to buffer (%d Bytes)\n", res);

	bufLen = (uLongf) (24 + 1.02 * requiredbuflen);
	fbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * bufLen + sizeof(header), cl_genericPool, 0);
	memcpy(fbuf, &header, sizeof(header));

#ifdef DEBUG
	/* In debugmode we will also write a uncompressed {filename}.lint file without header information */
	res = FS_WriteFile(buf, requiredbuflen, savegame_debug);
#endif
	if (header.compressed) {
		res = compress(fbuf + sizeof(header), &bufLen, buf, requiredbuflen + 1);
		Mem_Free(buf);

		if (res != Z_OK) {
			Mem_Free(fbuf);
			*error = _("Memory error compressing save-game data - set save_compressed cvar to 0");
			Com_Printf("Memory error compressing save-game data (%s) (Error: %i)!\n", comment, res);
			return qfalse;
		}
	} else {
		memcpy(fbuf + sizeof(header), buf, requiredbuflen + 1);
		Mem_Free(buf);
	}

	/* last step - write data */
	res = FS_WriteFile(fbuf, bufLen + sizeof(header), savegame);
	Mem_Free(fbuf);

	return qtrue;
}

/**
 * @brief Console command binding for save function
 * @sa SAV_GameSave
 */
static void SAV_GameSave_f (void)
{
	char comment[MAX_COMMENTLENGTH] = "";
	char *error = NULL;
	const char *arg = NULL;
	qboolean result;

	/* get argument */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <filename> <comment|*cvar>\n", Cmd_Argv(0));
		return;
	}

	if (!GAME_CP_IsRunning()) {
		Com_Printf("No running game - no saving...\n");
		return;
	}

	/* get comment */
	if (Cmd_Argc() > 2) {
		arg = Cmd_Argv(2);
		assert(arg);
		Q_strncpyz(comment, arg, sizeof(comment));
	}

	result = SAV_GameSave(Cmd_Argv(1), comment, &error);
	if (!result) {
		if (error)
			Com_sprintf(popupText, sizeof(popupText), "%s\n%s", _("Error saving game."), error);
		else
			Com_sprintf(popupText, sizeof(popupText), "%s\n", _("Error saving game."));
		MN_Popup(_("Note"), popupText);
	}
}

/**
 * @brief Init menu cvar for one savegame slot given by actual index.
 * @param[in] idx the savegame slot to retrieve gamecomment for
 * @sa SAV_GameReadGameComments_f
 */
static void SAV_GameReadGameComment (const int idx)
{
	saveFileHeader_t headerXML;
	qFILE f;

	FS_OpenFile(va("save/slot%i.xml", idx), &f, FILE_READ);
	if (f.f || f.z) {
		if (FS_Read(&headerXML, sizeof(headerXML), &f) != sizeof(headerXML))
			Com_Printf("Warning: SaveXMLfile header may be corrupted\n");
		if (!SAV_VerifyXMLHeader(&headerXML)) {
			Com_Printf("XMLSavegameheader for slot%d is corrupted!\n", idx);
		} else {
			char comment[MAX_VAR];
			Com_sprintf(comment, sizeof(comment), "%s - %s", headerXML.name, headerXML.gameDate);
			Cvar_Set(va("mn_slot%i", idx), comment);
			FS_CloseFile(&f);
		}
	}
}

/**
 * @brief Console commands to read the comments from savegames
 * @note The comment is the part of the savegame header that you type in at saving
 * for reidentifying the savegame
 * @sa SAV_GameLoad_f
 * @sa SAV_GameLoad
 * @sa SAV_GameSaveNameCleanup_f
 * @sa SAV_GameReadGameComment
 */
static void SAV_GameReadGameComments_f (void)
{
	int i;

	if (Cmd_Argc() == 2) {
		/* checks whether we plan to save without a running game */
		if (!GAME_CP_IsRunning() && !strncmp(Cmd_Argv(1), "save", 4)) {
			MN_PopMenu(qfalse);
			return;
		}
	}

	if (Cmd_Argc() == 2) {
		int idx = atoi(Cmd_Argv(1));
		SAV_GameReadGameComment(idx);
	} else {
		/* read all game comments */
		for (i = 0; i < 8; i++) {
			SAV_GameReadGameComment(i);
		}
	}
}

/**
 * @brief Console command to load a savegame
 * @sa SAV_GameLoad
 */
static void SAV_GameLoad_f (void)
{
	char *error = NULL;
	const cvar_t *gamedesc;

	/* get argument */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	/* try to get game description from "mn_<filename>" as indicator whether file will exist */
	gamedesc = Cvar_FindVar(va("mn_%s", Cmd_Argv(1)));
	if (!gamedesc || gamedesc->string[0] == '\0') {
		Com_DPrintf(DEBUG_CLIENT, "don't load file '%s', there is no description for it\n", Cmd_Argv(1));
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "load file '%s'\n", Cmd_Argv(1));

	/* load and go to map */
	if (!SAV_GameLoad(Cmd_Argv(1), &error)) {
		Cbuf_Execute(); /* wipe outstanding campaign commands */
		Com_sprintf(popupText, sizeof(popupText), "%s\n%s", _("Error loading game."), error ? error : "");
		MN_Popup(_("Error"), popupText);
	}
}

/**
 * @brief Loads the last saved game
 * @note At saving the archive cvar cl_lastsave was set to the latest savegame
 * @sa SAV_GameLoad
 */
static void SAV_GameContinue_f (void)
{
	char *error = NULL;

	if (cls.state == ca_active) {
		MN_PopMenu(qfalse);
		return;
	}

	if (!GAME_CP_IsRunning()) {
		/* try to load the last saved campaign */
		if (!SAV_GameLoad(cl_lastsave->string, &error)) {
			Cbuf_Execute(); /* wipe outstanding campaign commands */
			Com_sprintf(popupText, sizeof(popupText), "%s\n%s", _("Error loading game."), error ? error : "");
			MN_Popup(_("Error"), popupText);
		}
	} else {
		/* just continue the current running game */
		MN_PopMenu(qfalse);
	}
}

/**
 * @brief Adds a subsystem to the saveSubsystems array
 * @note The order is _not_ important
 * @sa SAV_Init
 */
static qboolean SAV_AddSubsystem (saveSubsystems_t *subsystem)
{
	if (saveSubsystemsAmount >= MAX_SAVESUBSYSTEMS)
		return qfalse;

	saveSubsystems[saveSubsystemsAmount].name = subsystem->name;
	saveSubsystems[saveSubsystemsAmount].load = subsystem->load;
	saveSubsystems[saveSubsystemsAmount].save = subsystem->save;
	saveSubsystemsAmount++;

	Com_Printf("added %s subsystem\n", subsystem->name);
	return qtrue;
}

/**
 * @brief Set the mn_slotX cvar to the comment (remove the date string) for clean
 * editing of the save comment
 * @sa SAV_GameReadGameComments_f
 */
static void SAV_GameSaveNameCleanup_f (void)
{
	int slotID;
	char cvar[16];
	qFILE f;
	saveFileHeader_t header;

	/* get argument */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <[0-7]>\n", Cmd_Argv(0));
		return;
	}

	slotID = atoi(Cmd_Argv(1));
	if (slotID < 0 || slotID > 7)
		return;

	Com_sprintf(cvar, sizeof(cvar), "mn_slot%i", slotID);

	FS_OpenFile(va("save/slot%i.xml", slotID), &f, FILE_READ);
	if (!f.f && !f.z)
		return;

	/* read the comment */
	if (FS_Read(&header, sizeof(header), &f) != sizeof(header))
		Com_Printf("Warning: Savefile header may be corrupted\n");
	Cvar_Set(cvar, header.name);
	FS_CloseFile(&f);
}

/**
 * @brief Quick save the current campaign
 * @sa CP_StartMissionMap
 */
qboolean SAV_QuickSave (void)
{
	char *error = NULL;
	qboolean result;

	if (CL_OnBattlescape())
		return qfalse;

	result = SAV_GameSave("slotquick", "QuickSave", &error);
	if (!result)
		Com_Printf("Error saving the xml game: %s\n", error ? error : "");

	return qtrue;
}

/**
 * @brief Checks whether there is a quicksave file at all - otherwise close
 * the quickload menu
 */
static void SAV_GameQuickLoadInit_f (void)
{
	qFILE f;

	/* only allow quickload while there is already a running campaign */
	if (!GAME_CP_IsRunning()) {
		MN_PopMenu(qfalse);
		return;
	}

	FS_OpenFile("save/slotquick.sav", &f, FILE_READ);
	if (!f.f && !f.z)
		MN_PopMenu(qfalse);
	else
		FS_CloseFile(&f);
}

/**
 * @brief Saves to the quick save slot
 * @sa SAV_GameQuickLoad_f
 */
static void SAV_GameQuickSave_f (void)
{
	if (!GAME_CP_IsRunning())
		return;

	if (!SAV_QuickSave())
		Com_Printf("Could not save the campaign\n");
	else
		MS_AddNewMessage(_("Quicksave"), _("Campaign was successfully saved."), qfalse, MSG_INFO, NULL);
}

/**
 * @brief Loads the quick save slot
 * @sa SAV_GameQuickSave_f
 */
static void SAV_GameQuickLoad_f (void)
{
	char *error = NULL;

	if (!GAME_CP_IsRunning())
		return;

	if (CL_OnBattlescape()) {
		Com_Printf("Could not load the campaign while you are on the battlefield\n");
		return;
	}

	if (!SAV_GameLoad("slotquick", &error)) {
		Cbuf_Execute(); /* wipe outstanding campaign commands */
		Com_sprintf(popupText, sizeof(popupText), "%s\n%s", _("Error loading game."), error ? error : "");
		MN_Popup(_("Error"), popupText);
	} else {
		MN_Popup(_("Campaign loaded"), _("Quicksave campaign was successfully loaded."));
	}
}

/**
 * @brief Register all save-subsystems and init some cvars and commands
 * @sa SAV_GameSave
 * @sa SAV_GameLoad
 */
void SAV_Init (void)
{
	static saveSubsystems_t b_subsystemXML = {"base", B_SaveXML, B_LoadXML};
	static saveSubsystems_t cp_subsystemXML = {"campaign", CP_SaveXML, CP_LoadXML};
	static saveSubsystems_t hos_subsystemXML = {"hospital", HOS_SaveXML, HOS_LoadXML};
	static saveSubsystems_t bs_subsystemXML = {"market", BS_SaveXML, BS_LoadXML};
	static saveSubsystems_t rs_subsystemXML = {"research", RS_SaveXML, RS_LoadXML};
	static saveSubsystems_t e_subsystemXML = {"employee", E_SaveXML, E_LoadXML};
	static saveSubsystems_t ac_subsystemXML = {"aliencont", AC_SaveXML, AC_LoadXML};
	static saveSubsystems_t pr_subsystemXML = {"production", PR_SaveXML, PR_LoadXML};
	static saveSubsystems_t air_subsystemXML = {"aircraft", AIR_SaveXML, AIR_LoadXML};
	static saveSubsystems_t ms_subsystemXML = {"messagesystem", MS_SaveXML, MS_LoadXML};
	static saveSubsystems_t stats_subsystemXML = {"stats", STATS_SaveXML, STATS_LoadXML};
	static saveSubsystems_t na_subsystemXML = {"nations", NAT_SaveXML, NAT_LoadXML};
	static saveSubsystems_t trans_subsystemXML = {"transfer", TR_SaveXML, TR_LoadXML};
	static saveSubsystems_t ab_subsystemXML = {"alien base", AB_SaveXML, AB_LoadXML};
	static saveSubsystems_t xvi_subsystemXML = {"xvirate", XVI_SaveXML, XVI_LoadXML};
	static saveSubsystems_t ins_subsystemXML = {"installation", INS_SaveXML, INS_LoadXML};
	static saveSubsystems_t mso_subsystemXML = {"messageoptions", MSO_SaveXML, MSO_LoadXML};

	saveSubsystemsAmount = 0;
	memset(&saveSubsystems, 0, sizeof(saveSubsystems));

	Com_Printf("\n--- save subsystem initialization --\n");

	/* don't mess with the order */
	SAV_AddSubsystem(&b_subsystemXML);
	SAV_AddSubsystem(&cp_subsystemXML);
	SAV_AddSubsystem(&hos_subsystemXML);
	SAV_AddSubsystem(&bs_subsystemXML);
	SAV_AddSubsystem(&rs_subsystemXML);
	SAV_AddSubsystem(&e_subsystemXML);
	SAV_AddSubsystem(&ac_subsystemXML);
	SAV_AddSubsystem(&pr_subsystemXML);
	SAV_AddSubsystem(&air_subsystemXML);
	SAV_AddSubsystem(&ms_subsystemXML);
	SAV_AddSubsystem(&stats_subsystemXML);
	SAV_AddSubsystem(&na_subsystemXML);
	SAV_AddSubsystem(&trans_subsystemXML);
	SAV_AddSubsystem(&ab_subsystemXML);
	SAV_AddSubsystem(&xvi_subsystemXML);
	SAV_AddSubsystem(&ins_subsystemXML);
	SAV_AddSubsystem(&mso_subsystemXML);

	Cmd_AddCommand("game_quickloadinit", SAV_GameQuickLoadInit_f, "Check whether there is a quicksave at all");
	Cmd_AddCommand("game_quicksave", SAV_GameQuickSave_f, _("Saves to the quick save slot"));
	Cmd_AddCommand("game_quickload", SAV_GameQuickLoad_f, "Loads the quick save slot");
	Cmd_AddCommand("game_save", SAV_GameSave_f, "Saves to a given filename");
	Cmd_AddCommand("game_load", SAV_GameLoad_f, "Loads a given filename");
	Cmd_AddCommand("game_comments", SAV_GameReadGameComments_f, "Loads the savegame names");
	Cmd_AddCommand("game_continue", SAV_GameContinue_f, "Continue with the last saved game");
	Cmd_AddCommand("game_savenamecleanup", SAV_GameSaveNameCleanup_f, "Remove the date string from mn_slotX cvars");
	save_compressed = Cvar_Get("save_compressed", "1", CVAR_ARCHIVE, "Save the savefiles compressed if set to 1");
}
