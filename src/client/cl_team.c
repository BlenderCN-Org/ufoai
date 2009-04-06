/**
 * @file cl_team.c
 * @brief Team management, name generation and parsing.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"
#include "cl_game.h"
#include "cl_team.h"
#include "cl_le.h"
#include "cl_actor.h"
#include "cl_inventory.h"
#include "cl_ugv.h"
#include "cl_parse.h"
#include "cl_menu.h"
#include "menu/m_nodes.h"

/** @brief List of currently displayed or equipeable characters. */
chrList_t chrDisplayList;

/**
 * @brief Translate the skin id to skin name
 * @param[in] id The id of the skin
 * @return Translated skin name
 */
const char* CL_GetTeamSkinName (int id)
{
	switch(id) {
	case 0:
		return _("Urban");
	case 1:
		return _("Jungle");
	case 2:
		return _("Desert");
	case 3:
		return _("Arctic");
	case 4:
		return _("Yellow");
	case 5:
		return _("CCCP");
	}
	Sys_Error("CL_GetTeamSkinName: Unknown skin id %i - max is %i", id, NUM_TEAMSKINS - 1);
}

/**
 * @brief saves a character to a given xml node
 * @param[in] p The node to which we should save the character
 * @param[in] chr The charcter we should save
 */
qboolean CL_SaveCharacterXML (mxml_node_t *p, const character_t chr)
{
	/* Store the character data */
	mxml_node_t *s;
	int k;
	const int count = max(SKILL_NUM_TYPES + 1, KILLED_NUM_TYPES);

	mxml_AddString(p, "name", chr.name);
	mxml_AddString(p, "body", chr.body);
	mxml_AddString(p, "path", chr.path);
	mxml_AddString(p, "head", chr.head);
	mxml_AddInt(p, "skin", chr.skin);
	mxml_AddInt(p, "teamdefidx", chr.teamDef->idx);
	mxml_AddInt(p, "gender", chr.gender);
	mxml_AddInt(p, "ucn", chr.ucn);
	mxml_AddInt(p, "maxhp", chr.maxHP);
	mxml_AddInt(p, "hp", chr.HP);
	mxml_AddInt(p, "stun", chr.STUN);
	mxml_AddInt(p, "morale", chr.morale);
	mxml_AddInt(p, "fieldsize", chr.fieldSize);

	/* Store character stats/score */
	for (k = 0; k < count; k++) {
		s = mxml_AddNode(p, "score");
		if (k < SKILL_NUM_TYPES)
			mxml_AddInt(s, "skill", chr.score.skills[k]);
		if (k < SKILL_NUM_TYPES + 1) {
			mxml_AddInt(s, "experience", chr.score.experience[k]);
			/** @todo There is a Bug in storing the initial skills. Fix it in loading */
			mxml_AddInt(s, "initskill", chr.score.initialSkills[k]);
		}
		if (k < KILLED_NUM_TYPES) {
			mxml_AddInt(s, "kills", chr.score.kills[k]);
			mxml_AddInt(s, "stuns", chr.score.stuns[k]);
		}
	}

	mxml_AddInt(p, "score.assignedmissions", chr.score.assignedMissions);
	mxml_AddInt(p, "score.rank", chr.score.rank);

	/* Store inventories */
	s = mxml_AddNode(p, "inventory");
	CL_SaveInventoryXML(s, &chr.inv);

	return qtrue;
}

qboolean CL_LoadCharacterXML (mxml_node_t *p, character_t *chr)
{
	mxml_node_t *s;
	int td, k, count;

	Q_strncpyz(chr->name, mxml_GetString(p, "name"), sizeof(chr->name));
	Q_strncpyz(chr->body, mxml_GetString(p, "body"), sizeof(chr->body));
	Q_strncpyz(chr->path, mxml_GetString(p, "path"), sizeof(chr->path));
	Q_strncpyz(chr->head, mxml_GetString(p, "head"), sizeof(chr->head));

	chr->skin = mxml_GetInt(p, "skin", 0);

	chr->teamDef = NULL;
	/* Team-definition index */
	td = mxml_GetInt(p, "teamdefidx", BYTES_NONE);

	if (td != BYTES_NONE) {
		assert(csi.numTeamDefs);
		if (td >= csi.numTeamDefs)
			return qfalse;
		chr->teamDef = &csi.teamDef[td];
	}

	/* Load the character data */
	chr->gender = mxml_GetInt(p, "gender", 0);
	chr->ucn = mxml_GetInt(p, "ucn", 0);
	chr->maxHP = mxml_GetInt(p, "maxhp", 0);
	chr->HP = mxml_GetInt(p, "hp", 0);
	chr->STUN = mxml_GetInt(p, "stun", 0);
	chr->morale = mxml_GetInt(p, "morale", 0);
	chr->fieldSize = mxml_GetInt(p, "fieldsize", 1);

	/** Load character stats/score
	 * @sa chrScoreGlobal_t */
	count = max(SKILL_NUM_TYPES + 1, KILLED_NUM_TYPES);
	for (k = 0, s = mxml_GetNode(p, "score"); s && k < count; k++, s = mxml_GetNextNode(s, p, "score")) {
		if (k < SKILL_NUM_TYPES)
			chr->score.skills[k] = mxml_GetInt(s, "skill", 0);
		if (k < SKILL_NUM_TYPES + 1) {
			chr->score.experience[k] = mxml_GetInt(s, "experience", 0);
			/** @todo There was a Bug in storing the initial skills. Zero Values will be set to max(hp,max_hp)*/
			chr->score.initialSkills[k] = mxml_GetInt(s, "initskill", 0);
			if (k == SKILL_NUM_TYPES && chr->score.initialSkills[k] == 0) {
				Com_DPrintf(DEBUG_CLIENT, "Skill was zero!");
				chr->score.initialSkills[k] = max(chr->HP, chr->maxHP);
			}
		}
		if (k < KILLED_NUM_TYPES) {
			chr->score.kills[k] = mxml_GetInt(s, "kills", 0);
			chr->score.stuns[k] = mxml_GetInt(s, "stuns", 0);
		}
	}
	chr->score.assignedMissions = mxml_GetInt(p, "score.assignedmissions", 0);
	chr->score.rank = mxml_GetInt(p, "score.rank", 0);
	chr->reservedTus.reserveReaction = STATE_REACTION_ONCE;

	/*memset(&chr->inv, 0, sizeof(inventory_t));*/
	INVSH_DestroyInventory(&chr->inv);
	s = mxml_GetNode(p, "inventory");
	CL_LoadInventoryXML(s, &chr->inv);

	return qtrue;
}

/**
 * @sa CL_LoadItemXML
 */
static void CL_SaveItemXML (mxml_node_t *p, item_t item, int container, int x, int y)
{
	assert(item.t);
	mxml_AddInt(p, "ammo", item.a);
	mxml_AddInt(p, "container", container);
	mxml_AddInt(p, "x", x);
	mxml_AddInt(p, "y", y);
	mxml_AddInt(p, "rotated", item.rotated);
	mxml_AddInt(p, "amount", item.amount);
	mxml_AddString(p, "weaponid", item.t->id);
	if (item.a > NONE_AMMO)
		mxml_AddString(p, "munitionid", item.m->id);
}

/**
 * @sa CL_LoadItem
 */
static void CL_SaveItem (sizebuf_t *buf, item_t item, int container, int x, int y)
{
	assert(item.t);
/*	Com_Printf("Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n", csi.ods[item.t].id, container, item.t, item.a, item.m, x, y);*/
	MSG_WriteFormat(buf, "bbbbbl", item.a, container, x, y, item.rotated, item.amount);
	MSG_WriteString(buf, item.t->id);
	if (item.a > NONE_AMMO)
		MSG_WriteString(buf, item.m->id);
}
/**
 * @sa CL_SaveItemXML
 * @sa CL_LoadInventoryXML
 */
void CL_SaveInventoryXML (mxml_node_t *p, const inventory_t *i)
{
	int j;

	for (j = 0; j < csi.numIDs; j++) {
		invList_t *ic = i->c[j];
		for (; ic; ic = ic->next) {
			mxml_node_t *s = mxml_AddNode(p, "item");
			CL_SaveItemXML(s, ic->item, j, ic->x, ic->y);
		}
	}
}
/**
 * @sa CL_SaveItem
 * @sa CL_LoadInventory
 */
void CL_SaveInventory (sizebuf_t *buf, const inventory_t *i)
{
	int j, nr = 0;
	invList_t *ic;

	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			nr++;

	assert(nr < MAX_INVLIST);
	Com_DPrintf(DEBUG_CLIENT, "CL_SaveInventory: Save %i items\n", nr);
	MSG_WriteShort(buf, nr);
	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			CL_SaveItem(buf, ic->item, j, ic->x, ic->y);
}

/**
 * @sa CL_SaveItemXML
 */
static void CL_LoadItemXML (mxml_node_t *n, item_t *item, int *container, int *x, int *y)
{
	const char *itemID;

	/* reset */
	memset(item, 0, sizeof(*item));
	item->a = mxml_GetInt(n, "ammo", NONE_AMMO);
	item->rotated = mxml_GetInt(n, "rotated", 0);
	item->amount = mxml_GetInt(n, "amount", 1);
	*x = mxml_GetInt(n, "x", 0);
	*y = mxml_GetInt(n, "y", 0);
	*container = mxml_GetInt(n, "container", 0);
	itemID = mxml_GetString(n, "weaponid");
	item->t = INVSH_GetItemByID(itemID);
	if (item->a > NONE_AMMO) {
		itemID = mxml_GetString(n, "munitionid");
		item->m = INVSH_GetItemByID(itemID);
	}
}

/**
 * @sa CL_SaveItem
 */
static void CL_LoadItem (sizebuf_t *buf, item_t *item, int *container, int *x, int *y)
{
	const char *itemID;

	/* reset */
	memset(item, 0, sizeof(*item));
	item->a = NONE_AMMO;

	MSG_ReadFormat(buf, "bbbbbl", &item->a, container, x, y, &item->rotated, &item->amount);
	itemID = MSG_ReadString(buf);
	item->t = INVSH_GetItemByID(itemID);
	if (item->a > NONE_AMMO) {
		itemID = MSG_ReadString(buf);
		item->m = INVSH_GetItemByID(itemID);
	}
}

/**
 * @sa CL_SaveInventoryXML
 * @sa CL_LoadItemXML
 * @sa Com_AddToInventory
  */
void CL_LoadInventoryXML (mxml_node_t *p, inventory_t *i)
{
	mxml_node_t *s;
	int count = 0;

	for (s = mxml_GetNode(p, "item"); s; s = mxml_GetNextNode(s, p, "item"), count++) {
		item_t item;
		int container, x, y;
		CL_LoadItemXML(s, &item, &container, &x, &y);
		if (!Com_AddToInventory(i, item, &csi.ids[container], x, y, 1))
			Com_Printf("Could not add item '%s' to inventory\n", item.t ? item.t->id : "NULL");
		assert(count < MAX_INVLIST);
	}
}

/**
 * @sa CL_SaveInventory
 * @sa CL_LoadItem
 * @sa Com_AddToInventory
 */
void CL_LoadInventory (sizebuf_t *buf, inventory_t *i)
{
	item_t item;
	int container, x, y;
	int nr = MSG_ReadShort(buf);

	Com_DPrintf(DEBUG_CLIENT, "CL_LoadInventory: Read %i items\n", nr);
	assert(nr < MAX_INVLIST);
	for (; nr-- > 0;) {
		CL_LoadItem(buf, &item, &container, &x, &y);
		if (!Com_AddToInventory(i, item, &csi.ids[container], x, y, 1))
			Com_Printf("Could not add item '%s' to inventory\n", item.t ? item.t->id : "NULL");
	}
}

/**
 * @sa G_WriteItem
 * @sa G_ReadItem
 * @note The amount of the item_t struct should not be needed here - because
 * the amount is only valid for idFloor and idEquip
 */
static void CL_NetSendItem (struct dbuffer *buf, item_t item, int container, int x, int y)
{
	const int ammoIdx = item.m ? item.m->idx : NONE;
	assert(item.t);
	Com_DPrintf(DEBUG_CLIENT, "CL_NetSendItem: Add item %s to container %i (t=%i:a=%i:m=%i) (x=%i:y=%i)\n",
		item.t->id, container, item.t->idx, item.a, ammoIdx, x, y);
	NET_WriteFormat(buf, ev_format[EV_INV_TRANSFER], item.t->idx, item.a, ammoIdx, container, x, y, item.rotated);
}

/**
 * @sa G_SendInventory
 */
void CL_NetSendInventory (struct dbuffer *buf, const inventory_t *i)
{
	int j, nr = 0;
	const invList_t *ic;

	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			nr++;

	NET_WriteShort(buf, nr * INV_INVENTORY_BYTES);
	for (j = 0; j < csi.numIDs; j++)
		for (ic = i->c[j]; ic; ic = ic->next)
			CL_NetSendItem(buf, ic->item, j, ic->x, ic->y);
}

/**
 * @sa G_WriteItem
 * @sa G_ReadItem
 * @note The amount of the item_t struct should not be needed here - because
 * the amount is only valid for idFloor and idEquip
 */
void CL_NetReceiveItem (struct dbuffer *buf, item_t *item, int *container, int *x, int *y)
{
	/* reset */
	int t, m;
	item->t = item->m = NULL;
	item->a = NONE_AMMO;
	NET_ReadFormat(buf, ev_format[EV_INV_TRANSFER], &t, &item->a, &m, container, x, y, &item->rotated);

	item->t = INVSH_GetItemByIDX(t);
	item->m = INVSH_GetItemByIDX(m);
}

/**
 * @brief Generates the skills and inventory for a character and for a 2x2 unit
 *
 * @param[in] employee The employee to create character data for.
 * @param[in] team Which team to use for creation.
 * @todo fix the assignment of ucn??
 * @todo fix the WholeTeam stuff
 */
void CL_GenerateCharacter (character_t *chr, const char *teamDefName, const ugv_t *ugvType)
{
	memset(chr, 0, sizeof(*chr));

	/* link inventory */
	INVSH_DestroyInventory(&chr->inv);

	/* get ucn */
	chr->ucn = cls.nextUniqueCharacterNumber++;

	/* Set default reaction-mode for all character-types to "once".
	 * AI actor (includes aliens if one doesn't play AS them) are set in @sa G_SpawnAIPlayer */
	chr->reservedTus.reserveReaction = STATE_REACTION_ONCE;

	CL_CharacterSetShotSettings(chr, -1, -1, NULL);

	chr->skin = Com_GetCharacterValues(teamDefName, chr);
	/* Create attributes. */
	CHRSH_CharGenAbilitySkills(chr, GAME_IsMultiplayer());
}

static menuOption_t skinlist[] = {
	{"urban", N_("Urban"), "team_changeskin;", "0", NULL, qfalse, qfalse, NULL},
	{"jungle", N_("Jungle"), "team_changeskin;", "1", NULL, qfalse, qfalse, NULL},
	{"desert", N_("Desert"), "team_changeskin;", "2", NULL, qfalse, qfalse, NULL},
	{"arctic", N_("Arctic"), "team_changeskin;", "3", NULL, qfalse, qfalse, NULL},
	{"multionly_yellow", N_("Yellow"), "team_changeskin;", "4", NULL, qfalse, qfalse, NULL},
	{"multionly_cccp", N_("CCCP"), "team_changeskin;", "5", NULL, qfalse, qfalse, NULL},
};

/**
 * @brief Init skins into the GUI
 */
static void CL_InitSkin_f (void)
{
	menuNode_t *node;
	const char *nodeName;

	if (GAME_IsCampaign())
		nodeName = "equipment.skins";
	else
		nodeName = "mp_team_edit.skins";

	node = MN_GetNodeByPath(nodeName);

	if (!node)
		Sys_Error("CL_InitSkin_f: Could not find node '%s'", nodeName);

	/** link all elements */
	if (node->u.option.first == NULL) {
		int i;
		for (i = 0; i < NUM_TEAMSKINS - 1; i++)
			skinlist[i].next = &skinlist[i + 1];
		node->u.option.first = skinlist;
	}

	/** link/unlink multiplayer skins */
	if (GAME_IsSingleplayer()) {
		skinlist[NUM_TEAMSKINS_SINGLEPLAYER - 1].next = NULL;
		node->u.option.count = NUM_TEAMSKINS_SINGLEPLAYER;
	} else {
		skinlist[NUM_TEAMSKINS_SINGLEPLAYER - 1].next = &skinlist[NUM_TEAMSKINS_SINGLEPLAYER];
		node->u.option.count = NUM_TEAMSKINS;
	}
}

/**
 * @brief Change the skin of the selected actor.
 */
static void CL_ChangeSkin_f (void)
{
	const int sel = cl_selected->integer;

	if (sel >= 0 && sel < chrDisplayList.num) {
		int newSkin = Cvar_GetInteger("mn_skin");
		if (newSkin >= NUM_TEAMSKINS || newSkin < 0)
			newSkin = 0;

		/* don't allow all skins in singleplayer */
		if (GAME_IsSingleplayer() && newSkin >= NUM_TEAMSKINS_SINGLEPLAYER)
			newSkin = 0;

		if (chrDisplayList.chr[sel]) {
			chrDisplayList.chr[sel]->skin = newSkin;

			Cvar_SetValue("mn_skin", newSkin);
			Cvar_Set("mn_skinname", CL_GetTeamSkinName(newSkin));
		}
	}
}

/**
 * @brief Use current skin for all team members onboard.
 */
static void CL_ChangeSkinForWholeTeam_f (void)
{
	int newSkin, i;

	/* Get selected skin and fall back to default skin if it is not valid. */
	newSkin = Cvar_GetInteger("mn_skin");
	if (newSkin >= NUM_TEAMSKINS || newSkin < 0)
		newSkin = 0;

	/* don't allow all skins in singleplayer */
	if (GAME_IsSingleplayer() && newSkin >= NUM_TEAMSKINS_SINGLEPLAYER)
		newSkin = 0;

	/* Apply new skin to all (shown/dsiplayed) team-members. */
	/** @todo What happens if a model of a team member does not have the selected skin? */
	for (i = 0; i < chrDisplayList.num; i++) {
		assert(chrDisplayList.chr[i]);
		chrDisplayList.chr[i]->skin = newSkin;
	}
}

/**
 * @sa CL_CleanupAircraftCrew
 * @todo Move it to a better place - has nothing to do with team code imo
 */
item_t CL_AddWeaponAmmo (equipDef_t * ed, item_t item)
{
	int i;
	objDef_t *type = item.t;

	assert(ed->num[type->idx] > 0);
	ed->num[type->idx]--;

	if (type->weapons[0]) {
		/* The given item is ammo or self-contained weapon (i.e. It has firedefinitions. */
		if (type->oneshot) {
			/* "Recharge" the oneshot weapon. */
			item.a = type->ammo;
			item.m = item.t; /* Just in case this hasn't been done yet. */
			Com_DPrintf(DEBUG_CLIENT, "CL_AddWeaponAmmo: oneshot weapon '%s'.\n", type->id);
			return item;
		} else {
			/* No change, nothing needs to be done to this item. */
			return item;
		}
	} else if (!type->reload) {
		/* The given item is a weapon but no ammo is needed,
		 * so fire definitions are in t (the weapon). Setting equal. */
		item.m = item.t;
		return item;
	} else if (item.a) {
		assert(item.m);
		/* The item is a weapon and it was reloaded one time. */
		if (item.a == type->ammo) {
			/* Fully loaded, no need to reload, but mark the ammo as used. */
			if (ed->num[item.m->idx] > 0) {
				ed->num[item.m->idx]--;
				return item;
			} else {
				/* Your clip has been sold; give it back. */
				item.a = NONE_AMMO;
				return item;
			}
		}
	}

	/* Check for complete clips of the same kind */
	if (item.m && ed->num[item.m->idx] > 0) {
		ed->num[item.m->idx]--;
		item.a = type->ammo;
		return item;
	}

	/* Search for any complete clips. */
	/** @todo We may want to change this to use the type->ammo[] info. */
	for (i = 0; i < csi.numODs; i++) {
		if (INVSH_LoadableInWeapon(&csi.ods[i], type)) {
			if (ed->num[i] > 0) {
				ed->num[i]--;
				item.a = type->ammo;
				item.m = &csi.ods[i];
				return item;
			}
		}
	}

	/** @todo on return from a mission with no clips left
	 * and one weapon half-loaded wielded by soldier
	 * and one empty in equip, on the first opening of equip,
	 * the empty weapon will be in soldier hands, the half-full in equip;
	 * this should be the other way around. */

	/* Failed to find a complete clip - see if there's any loose ammo
	 * of the same kind; if so, gather it all in this weapon. */
	if (item.m && ed->numLoose[item.m->idx] > 0) {
		item.a = ed->numLoose[item.m->idx];
		ed->numLoose[item.m->idx] = 0;
		return item;
	}

	/* See if there's any loose ammo */
	/** @todo We may want to change this to use the type->ammo[] info. */
	item.a = NONE_AMMO;
	for (i = 0; i < csi.numODs; i++) {
		if (INVSH_LoadableInWeapon(&csi.ods[i], type) && (ed->numLoose[i] > item.a)) {
			if (item.a > 0) {
				/* We previously found some ammo, but we've now found other
				 * loose ammo of a different (but appropriate) type with
				 * more bullets.  Put the previously found ammo back, so
				 * we'll take the new type. */
				assert(item.m);
				ed->numLoose[item.m->idx] = item.a;
				/* We don't have to accumulate loose ammo into clips
				 * because we did it previously and we create no new ammo */
			}
			/* Found some loose ammo to load the weapon with */
			item.a = ed->numLoose[i];
			ed->numLoose[i] = 0;
			item.m = &csi.ods[i];
		}
	}
	return item;
}

static void CL_ActorEquipmentSelect_f (void)
{
	int num;
	character_t *chr;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num >= chrDisplayList.num)
		return;

	/* update menu inventory */
	if (menuInventory && menuInventory != &chrDisplayList.chr[num]->inv) {
		chrDisplayList.chr[num]->inv.c[csi.idEquip] = menuInventory->c[csi.idEquip];
		/* set 'old' idEquip to NULL */
		menuInventory->c[csi.idEquip] = NULL;
	}
	menuInventory = &chrDisplayList.chr[num]->inv;
	chr = chrDisplayList.chr[num];

	/* deselect current selected soldier and select the new one */
	MN_ExecuteConfunc("equipdeselect %i", cl_selected->integer);
	MN_ExecuteConfunc("equipselect %i", num);

	/* now set the cl_selected cvar to the new actor id */
	Cvar_ForceSet("cl_selected", va("%i", num));

	/* set info cvars */
	if (chr->teamDef->race == RACE_ROBOT)
		CL_UGVCvars(chr);
	else
		CL_CharacterCvars(chr);
}

/**
 * @brief Selects a soldier while we are on battlescape
 */
static void CL_ActorSoldierSelect_f (void)
{
	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	/* check whether we are connected (tactical mission) */
	if (CL_OnBattlescape()) {
		const int num = atoi(Cmd_Argv(1));
		CL_ActorSelectList(num);
	}
}

/**
 * @brief implements the "nextsoldier" command
 * @todo Move into cl_actor.c
 */
static void CL_NextSoldier_f (void)
{
	if (CL_OnBattlescape()) {
		CL_ActorSelectNext();
	}
}

/**
 * @brief implements the reselect command
 * @todo Move into cl_actor.c
 */
static void CL_ThisSoldier_f (void)
{
	if (CL_OnBattlescape()) {
		CL_ActorSelectList(cl_selected->integer);
	}
}

/**
 * @brief Update the GUI with the selected item
 * @todo Doesn't belong into cl_team.c
 * @todo function is used for multiplayer, too - should be splitted
 */
static void CL_UpdateObject_f (void)
{
	int num;
	const objDef_t *obj;
	qboolean changeTab;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <objectid> <mustwechangetab>\n", Cmd_Argv(0));
		return;
	}

	if (Cmd_Argc() == 3)
		changeTab = atoi(Cmd_Argv(2)) >= 1;
	else
		changeTab = qtrue;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= csi.numODs) {
		Com_Printf("Id %i out of range 0..%i\n", num, csi.numODs);
		return;
	}
	obj = &csi.ods[num];

	/* update item description */
	INV_ItemDescription(obj);

	/* update tab */
	if (changeTab) {
		const cvar_t *var = Cvar_FindVar("mn_equiptype");
		const int filter = INV_GetFilterFromItem(obj);
		if (var->integer != filter) {
			Cvar_SetValue("mn_equiptype", filter);
			MN_ExecuteConfunc("update_item_list");
		}
	}
}

/**
 * @brief Update the skin of the current soldier
 */
static void CL_UpdateSoldier_f (void)
{
	const int num = cl_selected->integer;

	/* We are in the base or multiplayer inventory */
	if (num < chrDisplayList.num) {
		assert(chrDisplayList.chr[num]);
		if (chrDisplayList.chr[num]->teamDef->race == RACE_ROBOT)
			CL_UGVCvars(chrDisplayList.chr[num]);
		else
			CL_CharacterCvars(chrDisplayList.chr[num]);
	}
}

void TEAM_InitStartup (void)
{
	Cmd_AddCommand("team_initskin", CL_InitSkin_f, "Init skin according to the game mode");
	Cmd_AddCommand("team_changeskin", CL_ChangeSkin_f, "Change the skin of the soldier");
	Cmd_AddCommand("team_changeskinteam", CL_ChangeSkinForWholeTeam_f, "Change the skin for the whole current team");
	Cmd_AddCommand("equip_select", CL_ActorEquipmentSelect_f, "Select a soldier in the equipment menu");
	Cmd_AddCommand("soldier_select", CL_ActorSoldierSelect_f, _("Select a soldier from list"));
	Cmd_AddCommand("soldier_reselect", CL_ThisSoldier_f, _("Reselect the current soldier"));
	Cmd_AddCommand("soldier_updatecurrent", CL_UpdateSoldier_f, _("Update a soldier"));
	Cmd_AddCommand("object_update", CL_UpdateObject_f, _("Update the GUI with the selected item"));
	Cmd_AddCommand("nextsoldier", CL_NextSoldier_f, _("Toggle to next soldier"));
}
