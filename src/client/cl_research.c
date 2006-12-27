/**
 * @file cl_research.c
 * @brief Player research.
 *
 * Handles everything related to the research-tree.
 * Provides information if items/buildings/etc.. can be researched/used/displayed etc...
 * Implements the research-system (research new items/etc...)
 * See base/ufos/research.ufo and base/ufos/menu_research.ufo for the underlying content.
 * TODO: comment on used global variables.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#include "client.h"
#include "cl_global.h"

qboolean RS_TechIsResearchable(technology_t *t);

/* A (local) list of displayed technology-entries (the research list in the base) */
static technology_t *researchList[MAX_RESEARCHLIST];

/* The number of entries in the above list. */
static int researchListLength;

/* The currently selected entry in the above list. */
static int researchListPos;

static stringlist_t curRequiredList;

/**
 * @brief Sets a technology status to researched and updates the date.
 * @param[in] tech The technology that was researched.
 */
void RS_ResearchFinish (technology_t* tech)
{
	tech->statusResearch = RS_FINISH;
	CL_DateConvert(&ccs.date, &tech->researchedDateDay, &tech->researchedDateMonth);
	tech->researchedDateYear = ccs.date.day / 365;
	if (!tech->statusResearchable) {
		tech->statusResearchable = qtrue;
		CL_DateConvert(&ccs.date, &tech->preResearchedDateDay, &tech->preResearchedDateMonth);
		tech->preResearchedDateYear = ccs.date.day / 365;
	}
}

/**
 * @brief Marks one tech as researchable.
 * @param id unique id of a technology_t
 */
void RS_MarkOneResearchable(int tech_idx)
{
	technology_t *tech = &gd.technologies[tech_idx];
	Com_DPrintf("RS_MarkOneResearchable: \"%s\" marked as researchable.\n", tech->id);
	tech->statusResearchable = qtrue;
	CL_DateConvert(&ccs.date, &tech->preResearchedDateDay, &tech->preResearchedDateMonth);
	tech->preResearchedDateYear = ccs.date.day / 365;
}

/**
 * @brief Check if the item has been collected (in storage or quarantine) in the giveb base.
 * @param[in] item_idx The index of the item in the inv.
 * @param[in] base The base to searrch in.
 * @return amount of available items in base (TODO/FIXME: and on market)
 * @todo quarantine
 */
int RS_ItemInBase(int item_idx, base_t *base)
{
	equipDef_t *ed = NULL;

	if (!base)
		return -1;

	ed = &base->storage;

	if (!ed)
		return -1;

	/* Com_DPrintf("RS_ItemInBase: DEBUG idx %s\n",  csi.ods[item_idx].kurz); */

	/* FIXME/TODO: currently since all alien artifacts are added to the
	 * market, this check ensures market items are researchable too...
	 * otherwise the user must buy each item before researching it.
	 * Suggestion: if an unknown alien tech is found, sell all but the
	 * required number of items to perform research on that tech. Then
	 * the eMarket addition below would not be required */
	return ed->num[item_idx] + ccs.eMarket.num[item_idx];
}

/**
 * @brief Checks if all requirements of a tech have been met so that it becomes researchable.
 * @param[in] require_AND pointer to a list of AND-related requirements.
 * @param[in] require_OR pointer to a list of OR-related requirements.
 * @return Returns qtrue if all requirements are satisfied otherwise qfalse.
 */
static qboolean RS_RequirementsMet(requirements_t *required_AND, requirements_t *required_OR)
{
	int i;
	qboolean met_AND = qfalse;
	qboolean met_OR = qfalse;

	if (!required_AND && !required_AND) {
		Com_Printf("RS_RequirementsMet: No requirement list(s) given as parameter.\n");
		return qfalse;
	}

	if (required_AND->numLinks) {
		met_AND = qtrue;
		for (i = 0; i < required_AND->numLinks; i++) {
			switch (required_AND->type[i]) {
			case RS_LINK_TECH:
				Com_DPrintf("RS_RequirementsMet: ANDtech: %s / %i\n", required_AND->id[i], required_AND->idx[i]);
				if (!RS_TechIsResearched(required_AND->idx[i])
					&& Q_strncmp(required_AND->id[i], "nothing", MAX_VAR)) {
					Com_DPrintf("RS_RequirementsMet: this tech not researched ----> %s \n", required_AND->id[i]);
					met_AND = qfalse;
				}
				break;
			case RS_LINK_ITEM:
				Com_DPrintf("RS_RequirementsMet: ANDitem: %s / %i\n", required_AND->id[i], required_AND->idx[i]);
				/* TODO: required_AND->collected[i] was being used instead of RS_ItemInBase below,
				 * but the collected count never seemed to be incremented...
				 * so either remove this and the RS_CheckCollected method or fix them */
				if (RS_ItemInBase(required_AND->idx[i], baseCurrent) < required_AND->amount[i]) {
					met_AND = qfalse;
				}
				break;
			case RS_LINK_WEAPON:
				/* This is no real requirement, so no checks here. */
				break;
			case RS_LINK_EVENT:
				break;
			default:
				break;
			}

			if (!met_AND)
				break;
		}
	}

	if (required_OR->numLinks)
		for (i = 0; i < required_OR->numLinks; i++) {
			switch (required_OR->type[i]) {
			case RS_LINK_TECH:
				Com_DPrintf("RS_RequirementsMet: ORtech: %s / %i\n", required_OR->id[i], required_OR->idx[i]);
				if (RS_TechIsResearched(required_OR->idx[i]))
					met_OR = qtrue;
				break;
			case RS_LINK_ITEM:
				Com_DPrintf("RS_RequirementsMet: ORitem: %s / %i\n", required_OR->id[i], required_OR->idx[i]);
				/* TODO: required_OR->collected[i] should be used instead of RS_ItemInBase, see equivalent TODO above */
				if (RS_ItemInBase(required_OR->idx[i], baseCurrent) >= required_OR->amount[i])
					met_OR = qtrue;
				break;
			case RS_LINK_WEAPON:
				/* This is no real requirement, so no checks here. */
				break;
			case RS_LINK_EVENT:
				break;
			default:
				break;
			}

			if (met_OR)
				break;
		}
	Com_DPrintf("met_AND is %i, met_OR is %i\n", met_AND, met_OR);

	return (met_AND || met_OR);
}

/**
 * @brief Checks if any items have been collected (in the current base) and correct the value for each requirement.
 * @note Does not check if the collected items satisfy the needed "amount". This is done in RS_RequirementsMet.
 * @return Returns qtrue if there are any collected items otherwise qfalse.
 * @todo Extend to support require_OR (see RS_CheckAllCollected for more info)
 */
qboolean RS_CheckCollected(requirements_t *required)
{
	int i;
	int item_amount;
	qboolean all_collected = qtrue;
	technology_t *tech = NULL;

	if (!required)
		return qfalse;

	if (!baseCurrent)
		return qfalse;

	for (i = 0; i < required->numLinks; i++) {
		if (required->type[i] == RS_LINK_ITEM) {
			item_amount = RS_ItemInBase(required->idx[i], baseCurrent);
			if (item_amount > 0) {
				required->collected[i] = item_amount;
			} else {
				required->collected[i] = 0;
				all_collected = qfalse;
			}
		} else if (required->type[i] == RS_LINK_TECH) {
			tech = &gd.technologies[required->idx[i]];
			/* Check if it is a logic block (RS_LOGIC) and interate into it if so.*/
			if (tech->type == RS_LOGIC) {
				if (!RS_CheckCollected(&tech->require_AND)) {
					tech->statusCollected = qfalse;
					all_collected = qfalse;
				} else {
					tech->statusCollected = qtrue;
				}
			}
		}
	}
	return all_collected;
}

/**
 * @brief Checks if any items have been collected in the current base and correct the values for each requirement.
 * @todo Add support for items in the require_OR list.
 */
void RS_CheckAllCollected(void)
{
	int i;
	technology_t *tech = NULL;
	requirements_t *required = NULL;

	if (!required)
		return;

	if (!baseCurrent)
		return;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];

		/* TODO: Add support for require_OR here. Change this to the following:
		 * if (RS_CheckCollected(&tech) - does return AND||OR
		 */
		if (RS_CheckCollected(&tech->require_AND)) {
			tech->statusCollected = qtrue;
		}
	}
}

/**
 * @brief Marks all the techs that can be researched.
 * Automatically researches 'free' techs such as ammo for a weapon.
 * Should be called when a new item is researched (RS_MarkResearched) and after
 * the tree-initialisation (RS_InitTree)
 */
void RS_MarkResearchable(void)
{
	int i;
	technology_t *tech = NULL;

	/* Set all entries to initial value. */
	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		tech->statusResearchable = qfalse;
	}
	RS_CheckAllCollected();

	for (i = 0; i < gd.numTechnologies; i++) {	/* i = tech-index */
		tech = &gd.technologies[i];
		if (!tech->statusResearchable) { /* In case we loopback we need to check for already marked techs. */
			/* Check for collected items/aliens/etc... */

			if (tech->statusResearch != RS_FINISH) {
				Com_DPrintf("RS_MarkResearchable: handling \"%s\".\n", tech->id);
				/* If required techs are all researched and all other requirements are met, mark this as researchable. */

				/* All requirements are met. */
				if (RS_RequirementsMet(&tech->require_AND, &tech->require_OR)) {
					Com_DPrintf("RS_MarkResearchable: \"%s\" marked researchable. reason:requirements.\n", tech->id);
					RS_MarkOneResearchable(tech->idx);
				}

				/* If the tech is a 'free' one (such as ammo for a weapon),
				   mark it as researched and loop back to see if it unlocks
				   any other techs */
				if (tech->statusResearchable && tech->time <= 0) {
					RS_ResearchFinish(tech);
					Com_DPrintf("RS_MarkResearchable: automatically researched \"%s\"\n", tech->id);
					/* Restart the loop as this may have unlocked new possibilities. */
					i = 0;
				}
			}
		}
	}
	Com_DPrintf("RS_MarkResearchable: Done.\n");
}

/**
 * @brief Assign required tech/item/etc... IDXs for a single requirements list.
 * @note A function with the same behaviour was formerly also known as RS_InitRequirementList
 */
void RS_AssignTechIdxs(requirements_t *req)
{
	int i;

	for (i = 0; i < req->numLinks; i++) {
		switch (req->type[i]) {
		case RS_LINK_TECH:
		case RS_LINK_WEAPON:
			/* Get the index in the techtree. */
			req->idx[i] = RS_GetTechIdxByName(req->id[i]);
			break;
		case RS_LINK_ITEM:
			/* Get index in item-list. */
			req->idx[i] = RS_GetItem(req->id[i]);
			break;
		case RS_LINK_EVENT:
			/* TODO: Get index of event in event-list. */
			break;
		default:
			break;
		}
	}
}

/**
 * @brief Assign IDXs to all required techs/items/etc...
 * @note This replaces the RS_InitRequirementList function (since the switch to the _OR and _AND list)
 */
void RS_RequiredIdxAssign(void)
{
	int i;
	technology_t *tech = NULL;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (&tech->require_AND.numLinks)
			RS_AssignTechIdxs(&tech->require_AND);
		if (&tech->require_OR.numLinks)
			RS_AssignTechIdxs(&tech->require_OR);
	}
}

/**
 * @brief Gets all needed names/file-paths/etc... for each technology entry.
 * Should be executed after the parsing of _all_ the ufo files and e.g. the
 * research tree/inventory/etc... are initialised.
 * @todo Add a function to reset ALL research-stati to RS_NONE; -> to be called after start of a new game.
 * @todo Enhance ammo model display (see comment in code).
 */
void RS_InitTree(void)
{
	int i, j, k;
	technology_t *tech = NULL;
	objDef_t *item = NULL;
	objDef_t *item_ammo = NULL;
	building_t *building = NULL;
	aircraft_t *air_samp = NULL;
	byte found;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];

		for (j = 0; j < tech->markResearched.numDefinitions; j++) {
			if (tech->markResearched.markOnly[j] && !Q_strncmp(tech->markResearched.campaign[j], curCampaign->id, MAX_VAR)) {
				Com_Printf("Mark '%s' as researched\n", tech->id);
				RS_ResearchFinish(tech);
				break;
			}
		}

		/* Save the idx to the id-names of the different requirement-types for quicker access. The id-strings themself are not really needed afterwards :-/ */
		RS_AssignTechIdxs(&tech->require_AND);
		RS_AssignTechIdxs(&tech->require_OR);

		/* Search in correct data/.ufo */
		switch (tech->type) {
		case RS_CRAFTWEAPON:
		case RS_CRAFTSHIELD:
			if (!*tech->name)
				Com_DPrintf("RS_InitTree: \"%s\" A type craftshield or craftweapon item needs to have a 'name\txxx' defined.", tech->id);
			break;
		case RS_NEWS:
			if (!*tech->name)
				Com_DPrintf("RS_InitTree: \"%s\" A 'type news' item needs to have a 'name\txxx' defined.", tech->id);
			break;
		case RS_TECH:
			if (!*tech->name)
				Com_DPrintf("RS_InitTree: \"%s\" A 'type tech' item needs to have a 'name\txxx' defined.", tech->id);
			break;
		case RS_WEAPON:
		case RS_ARMOR:
			found = qfalse;
			for (j = 0; j < csi.numODs; j++) {	/* j = item index */
				item = &csi.ods[j];

				/* This item has been 'provided' -> get the correct data. */
				if (!Q_strncmp(tech->provides, item->kurz, MAX_VAR)) {
					found = qtrue;
					if (!*tech->name)
						Com_sprintf(tech->name, MAX_VAR, item->name);
					if (!*tech->mdl_top)
						Com_sprintf(tech->mdl_top, MAX_VAR, item->model);
					if (!*tech->image_top)
						Com_sprintf(tech->image_top, MAX_VAR, item->image);
					if (!*tech->mdl_bottom) {
						if (tech->type == RS_WEAPON) {
							/* Find ammo for weapon. */
							/* TODO: Add code+structure to display several ammo-types (even reseachable ones). */
							for (k = 0; k < csi.numODs; k++) {
								item_ammo = &csi.ods[k];
								if ( INV_LoadableInWeapon(item_ammo, j) ) {
									Com_sprintf(tech->mdl_bottom, MAX_VAR, item_ammo->model);
									break;
								}
							}
						}
					}
					/* Should return to CASE RS_xxx. */
					break;
				}
			}
			/* No id found in csi.ods */
			if (!found) {
				Com_sprintf(tech->name, MAX_VAR, tech->id);
				Com_Printf("RS_InitTree: \"%s\" - Linked weapon or armor (provided=\"%s\") not found. Tech-id used as name.\n", tech->id, tech->provides);
			}
			break;
		case RS_BUILDING:
			found = qfalse;
			for (j = 0; j < gd.numBuildingTypes; j++) {
				building = &gd.buildingTypes[j];
				/* This building has been 'provided'  -> get the correct data. */
				if (!Q_strncmp(tech->provides, building->id, MAX_VAR)) {
					found = qtrue;
					if (!*tech->name)
						Com_sprintf(tech->name, MAX_VAR, building->name);
					if (!*tech->image_top)
						Com_sprintf(tech->image_top, MAX_VAR, building->image);

					/* Should return to CASE RS_xxx. */
					break;
				}
			}
			if (!found) {
				Com_sprintf(tech->name, MAX_VAR, tech->id);
				Com_DPrintf("RS_InitTree: \"%s\" - Linked building (provided=\"%s\") not found. Tech-id used as name.\n", tech->id, tech->provides);
			}
			break;
		case RS_CRAFT:
			found = qfalse;
			for (j = 0; j < numAircraft_samples; j++) {
				air_samp = &aircraft_samples[j];
				/* This aircraft has been 'provided'  -> get the correct data. */
				if (!Q_strncmp(tech->provides, air_samp->id, MAX_VAR)) {
					found = qtrue;
					if (!*tech->name)
						Com_sprintf(tech->name, MAX_VAR, air_samp->name);
					if (!*tech->mdl_top) {	/* DEBUG testing */
						Com_sprintf(tech->mdl_top, MAX_VAR, air_samp->model);
						Com_DPrintf("RS_InitTree: aircraft model \"%s\" \n", air_samp->model);
					}
					/* Should return to CASE RS_xxx. */
					break;
				}
			}
			if (!found)
				Com_DPrintf("RS_InitTree: \"%s\" - Linked aircraft or craft-upgrade (provided=\"%s\") not found.\n", tech->id, tech->provides);
			break;
		case RS_ALIEN:
			/* does nothing right now */
			break;
		case RS_UGV:
			/* TODO: Implement me */
			break;
		case RS_LOGIC:
			/* Does not need any additional data. */
			break;

		} /* switch */
	}
	/*
	for (i = 0; i < gd.numBases; i++)
		if (gd.bases[i].founded)
			RS_MarkCollected(&gd.bases[i].storage);
	*/
	RS_MarkResearchable();

	memset(&curRequiredList, 0, sizeof(stringlist_t));

	Com_DPrintf("RS_InitTree: Technology tree initialised. %i entries found.\n", i);
}

/**
 * @brief Return "name" if present, otherwise enter the correct .ufo file and get it from the definition there.
 * @param[in] id unique id of a technology_t
 * @param[out] name Full name of this technology_t (technology_t->name) - defaults to id if nothing is found.
 * @note name has a maxlength of MAX_VAR
 */
void RS_GetName(char *id, char *name)
{
	technology_t *tech = NULL;

	tech = RS_GetTechByID(id);
	if (!tech) {
		/* set the name to the id. */
		Com_sprintf(name, MAX_VAR, id);
		return;
	}

	if (*tech->name) {
		Com_sprintf(name, MAX_VAR, _(tech->name));
		return;
	} else {
		/* FIXME: Do we need to translate the id? */
		/* set the name to the id. */
		Com_sprintf(name, MAX_VAR, _(id));
		return;
	}
}

/**
 * @brief Displays the informations of the current selected technology in the description-area.
 * See menu_research.ufo for the layout/called functions.
 */
static void RS_ResearchDisplayInfo(void)
{
	technology_t *tech = NULL;
	base_t *base = NULL;

	/* we are not in base view */
	if (!baseCurrent)
		return;

	if (researchListLength <= 0 || researchListPos >= researchListLength)
		return;

	tech = researchList[researchListPos];

	/* Display total number of free labs in current base. */
	Cvar_Set("mn_research_scis", va(_("Available scientists in this base: %i"), E_CountUnassigned(baseCurrent, EMPL_SCIENTIST)));
	Cvar_Set("mn_research_selbase", _("Not researched in any base."));

	/* Display the base this tech is researched in. */
	if (tech->scientists >= 0) {
		if (tech->base_idx != baseCurrent->idx) {
			base = &gd.bases[tech->base_idx];
			Cvar_Set("mn_research_selbase", va(_("Researched in %s"), base->name));
		} else
			Cvar_Set("mn_research_selbase", _("Researched in this base."));
	}

	Cvar_Set("mn_research_selname", _(tech->name));
	if (tech->overalltime) {
		if (tech->time > tech->overalltime) {
			Com_Printf("RS_ResearchDisplayInfo: \"%s\" - 'time' (%f) was larger than 'overall-time' (%f). Fixed. Please report this.\n", tech->id, tech->time,
					tech->overalltime);
			/* just in case the values got messed up */
			tech->time = tech->overalltime;
		}
		Cvar_Set("mn_research_seltime", va(_("Progress: %.1f%%"), 100 - (tech->time * 100 / tech->overalltime)));
	} else
		Cvar_Set("mn_research_seltime", _("Progress: Not available."));

	switch (tech->statusResearch) {
	case RS_RUNNING:
		Cvar_Set("mn_research_selstatus", _("Status: Under research"));
		break;
	case RS_PAUSED:
		Cvar_Set("mn_research_selstatus", _("Status: Research paused"));
		break;
	case RS_FINISH:
		Cvar_Set("mn_research_selstatus", _("Status: Research finished"));
		break;
	case RS_NONE:
		Cvar_Set("mn_research_selstatus", _("Status: Unknown technology"));
		break;
	default:
		break;
	}
}

/**
 * @brief Changes the active research-list entry to the currently selected.
 * See menu_research.ufo for the layout/called functions.
 */
static void CL_ResearchSelectCmd(void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: research_select <num>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num >= researchListLength) {
		menuText[TEXT_STANDARD] = NULL;
		return;
	}

	/* call researchselect function from menu_research.ufo */
	researchListPos = num;
	Cbuf_AddText(va("researchselect%i\n", researchListPos));

	/*RS_ResearchDisplayInfo(); */
	RS_UpdateData();
}

/**
 * @brief Assigns scientist to the selected research-project.
 * @note The lab will be automatically selected (the first one that has still free space).
 * @param[in] tech What technology you want to assign the scientist to.
 * @sa RS_AssignScientist_f
 */
void RS_AssignScientist(technology_t* tech)
{
	building_t *building = NULL;
	employee_t *employee = NULL;
	base_t *base = NULL;

	Com_DPrintf("RS_AssignScientist: %i | %s \n", tech->idx, tech->name);

	if  (tech->base_idx >= 0) {
		base = &gd.bases[tech->base_idx];
	} else {
		base = baseCurrent;
	}

	employee = E_GetUnassignedEmployee(base, EMPL_SCIENTIST);

	if (!employee) {
		/* No scientists are free in this base. */
		return;
	}

	if (tech->statusResearchable) {
		/* Get a free lab from the base. */
		building = B_GetLab(base->idx);
		if (building) {
			/* Assign the tech to a lab&base. */
			tech->scientists++;
			tech->base_idx = building->base_idx;
			employee->buildingID = building->idx;
			/* TODO: use
			E_AssignEmployeeToBuilding(employee, building);
			instead. */
		} else {
			MN_Popup(_("Notice"),
				_("There is no free lab available.\nYou need to build one or free another\nin order to assign scientists to research this technology.\n"));
			return;
		}

		tech->statusResearch = RS_RUNNING;

		/* Update display-list and display-info. */
		RS_ResearchDisplayInfo();
		RS_UpdateData();
	}
}

/**
 * @brief Script function to add a scientist to  the technology entry in the research-list.
 * @sa RS_AssignScientist
 * @sa RS_RemoveScientist_f
 */
static void RS_AssignScientist_f(void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_rs_add <num_in_list>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num > researchListLength)
		return;

	Com_DPrintf("RS_AssignScientist_f: num %i\n", num);
	RS_AssignScientist(researchList[num]);
}


/**
 * @brief Remove a scientist from a technology.
 * @param[in] tech The technology you want to remove the scientist from.
 * @sa RS_RemoveScientist_f
 * @sa RS_AssignScientist
 */
static void RS_RemoveScientist(technology_t* tech)
{
	employee_t *employee = NULL;

	assert(tech);

	if (tech->scientists > 0) {
		employee = E_GetAssignedEmployee(&gd.bases[tech->base_idx], EMPL_SCIENTIST);
		if (employee) {
			employee->buildingID = -1; /* See also E_RemoveEmployeeFromBuilding */
			tech->scientists--;
		}
	}

	if (tech->scientists == 0) {
		tech->base_idx = -1;
	}
}

/**
 * @brief Script function to remove a scientist from the technology entry in the research-list.
 * @sa RS_AssignScientist_f
 * @sa RS_RemoveScientist
 */
static void RS_RemoveScientist_f(void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_rs_remove <num_in_list>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num > researchListLength)
		return;

	RS_RemoveScientist(researchList[num]);

	/* Update display-list and display-info. */
	RS_ResearchDisplayInfo();
	RS_UpdateData();
}

/**
 * @brief Starts the research of the selected research-list entry.
 * TODO: Check if laboratory is available.
 */
static void RS_ResearchStart(void)
{
	technology_t *tech = NULL;

	/* We are not in base view. */
	if (!baseCurrent)
		return;

#if 0
	/* Check if laboratory is available. */
	if (!baseCurrent->hasLab)
		return;
#endif

	/* get the currently selected research-item */
	tech = researchList[researchListPos];

	/************
		TODO:
		Check for collected items that are needed by the tech to be researchable.
		If there are enough items add them to the tech, otherwise pop an errormessage telling the palyer what is missing.
	*/
	if (!tech->statusResearchable) {
		if (RS_CheckCollected(&tech->require_AND) && RS_CheckCollected(&tech->require_OR))
			RS_MarkOneResearchable(tech->idx);
		RS_MarkResearchable();
	}
	/************/

	if (tech->statusResearchable) {
		switch (tech->statusResearch) {
		case RS_RUNNING:
			MN_Popup(_("Notice"), _("This item is already under research by your scientists."));
			break;
		case RS_PAUSED:
			MN_Popup(_("Notice"), _("The research on this item continues."));
			tech->statusResearch = RS_RUNNING;
			break;
		case RS_FINISH:
			MN_Popup(_("Notice"), _("The research on this item is complete."));
			break;
		case RS_NONE:
			if (tech->scientists <= 0) {
				/* Add scientists to tech. */
				RS_AssignScientist(tech);
			}
			tech->statusResearch = RS_RUNNING;
			break;
		default:
			break;
		}
	} else
		MN_Popup(_("Notice"), _("The research on this item is not yet possible.\nYou need to research the technologies it's based on first."));

	RS_UpdateData();
}

/**
 * @brief Pauses the research of the selected research-list entry.
 * TODO: Check if laboratory is available
 */
static void RS_ResearchStop(void)
{
	technology_t *tech = NULL;

	/* we are not in base view */
	if (!baseCurrent)
		return;

	/* get the currently selected research-item */
	tech = researchList[researchListPos];

	switch (tech->statusResearch) {
	case RS_RUNNING:
		/* TODO: remove lab from technology and scientists from lab */
		tech->statusResearch = RS_PAUSED;
		break;
	case RS_PAUSED:
		tech->statusResearch = RS_RUNNING;
		break;
	case RS_FINISH:
		MN_Popup(_("Notice"), _("The research on this item is complete."));
		break;
	case RS_NONE:
		Com_Printf("Can't pause research. Research not started.\n");
		break;
	default:
		break;
	}
	RS_UpdateData();
}

/**
 * @brief Loops trough the research-list and updates the displayed text+color of each research-item according to it's status.
 * @note See menu_research.ufo for the layout/called functions.
 * @todo Display free space in all labs in the current base for each item.
 */
void RS_UpdateData(void)
{
	char name[MAX_VAR];
	int i, j;
	int available[MAX_BASES];
	technology_t *tech = NULL;

	/* Make everything the same (predefined in the ufo-file) color. */
	Cbuf_AddText("research_clear\n");

	for (i=0; i < gd.numBases; i++) {
		available[i] = E_CountUnassigned(&gd.bases[i], EMPL_SCIENTIST);
	}
	RS_CheckAllCollected();
	RS_MarkResearchable();
	for (i = 0, j = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		Com_sprintf(name, MAX_VAR, tech->name);

		/* TODO: add check for collected items */

		if (tech->statusCollected && !tech->statusResearchable && (tech->statusResearch != RS_FINISH)) {
			/* An unresearched collected item that cannot yet be researched. */
			Q_strcat(name, _(" [not yet researchable]"), MAX_VAR);
			/* Color the item 'unresearchable' */
			Cbuf_AddText(va("researchunresearchable%i\n", j));
			/* Display the concated text in the correct list-entry. */
			Cvar_Set(va("mn_researchitem%i", j), name);

			Cvar_Set(va("mn_researchassigned%i", j), "--");
			Cvar_Set(va("mn_researchavailable%i", j), "--");
			Cvar_Set(va("mn_researchmax%i", j), "--");

			/* Assign the current tech in the global list to the correct entry in the displayed list. */
			researchList[j] = &gd.technologies[i];
			/* counting the numbers of display-list entries. */
			j++;
		} else if ((tech->statusResearch != RS_FINISH) && (tech->statusResearchable)) {
			/* An item that can be researched. */
			/* How many scis are assigned to this tech. */
			Cvar_SetValue(va("mn_researchassigned%i", j), tech->scientists);
			if ((tech->base_idx == baseCurrent->idx) || (tech->base_idx < 0) ) {
				/* Maximal available scientists in the base the tech is researched. */
				Cvar_SetValue(va("mn_researchavailable%i", j), available[baseCurrent->idx]);
			} else {
				/* Display available scientists of other base here. */
				Cvar_SetValue(va("mn_researchavailable%i", j), available[tech->base_idx]);
			}
			/* TODO: Free space in all labs in this base. */
			/* Cvar_SetValue(va("mn_researchmax%i", j), available); */
			Cvar_Set(va("mn_researchmax%i", j), "mx.");
			/* Set the text of the research items and mark them if they are currently researched. */
			switch (tech->statusResearch) {
			case RS_RUNNING:
				/* Color the item with 'research running'-color. */
				Cbuf_AddText(va("researchrunning%i\n", j));
				break;
			case RS_PAUSED:
				/* Color the item with 'research paused'-color. */
				Cbuf_AddText(va("researchpaused%i\n", j));
				break;
			case RS_NONE:
				/* The color is defined in menu research.ufo by  "confunc research_clear". See also above. */
				break;
			case RS_FINISH:
			default:
				break;
			}

			/* Display the concated text in the correct list-entry. */
			Cvar_Set(va("mn_researchitem%i", j), _(name));
			/* Assign the current tech in the global list to the correct entry in the displayed list. */
			researchList[j] = &gd.technologies[i];
			/* counting the numbers of display-list entries. */
			j++;
		}
	}

	researchListLength = j;

	/* Set rest of the list-entries to have no text at all. */
	for (; j < MAX_RESEARCHDISPLAY; j++) {
		Cvar_Set(va("mn_researchitem%i", j), "");
		Cvar_Set(va("mn_researchassigned%i", j), "");
		Cvar_Set(va("mn_researchavailable%i", j), "");
		Cvar_Set(va("mn_researchmax%i", j), "");
	}

	/* Select last selected item if possible or the very first one if not. */
	if (researchListLength) {
		Com_DPrintf("RS_UpdateData: Pos%i Len%i\n", researchListPos, researchListLength);
		if ((researchListPos < researchListLength) && (researchListLength < MAX_RESEARCHDISPLAY)) {
			Cbuf_AddText(va("researchselect%i\n", researchListPos));
		} else {
			Cbuf_AddText("researchselect0\n");
		}
	} else {
		/* No display list abailable (zero items) - > Reset description. */
		Cvar_Set("mn_researchitemname", "");
		Cvar_Set("mn_researchitem", "");
		Cvar_Set("mn_researchweapon", "");
		Cvar_Set("mn_researchammo", "");
		menuText[TEXT_STANDARD] = NULL;
	}

	/* Update the description field/area. */
	RS_ResearchDisplayInfo();
}

/**
 * @brief Checks whether there are items in the research list and there is a base
 * otherwise leave the research menu again
 * @note if there is a base but no lab a popup appears
 * @sa RS_UpdateData
 * @sa MN_ResearchInit
 */
void CL_ResearchType(void)
{
	/* Update and display the list. */
	RS_UpdateData();

	/* Nothing to research here. */
	if (!researchListLength || !gd.numBases) {
		Cbuf_ExecuteText(EXEC_NOW, "mn_pop");
	} else if (baseCurrent && !baseCurrent->hasLab) {
		Cbuf_ExecuteText(EXEC_NOW, "mn_pop");
		MN_Popup(_("Notice"), _("Build a laboratory first"));
	}
}

#if 0
/**
 * @brief Checks if the research item id1 depends on (requires) id2
 * @param[in] id1 Unique id of a technology_t that may or may not depend on id2.
 * @param[in] id2 Unique id of a technology_t
 * @return qboolean
 */
static qboolean RS_DependsOn(char *id1, char *id2)
{
	int i;
	technology_t *tech = NULL;
	stringlist_t required;

	tech = RS_GetTechByID(id1);
	if (!tech)
		return qfalse;

	/* research item found */
	required = tech->requires;
	for (i = 0; i < required.numEntries; i++) {
		/* Current item (=id1) depends on id2. */
		if (!Q_strncmp(required.string[i], id2, MAX_VAR))
			return qtrue;
	}
	return qfalse;
}
#endif

/**
 * @brief Mark technologies as researched. This includes techs that depends in "id" and have time=0
 * @param[in] id Unique id of a technology_t
 */
void RS_MarkResearched(char *id)
{
	int i;
	technology_t *tech = NULL;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (!Q_strncmp(id, tech->id, MAX_VAR)) {
			RS_ResearchFinish(tech);
			Com_DPrintf("Research of \"%s\" finished.\n", tech->id);
#if 0
		} else if (RS_DependsOn(tech->id, id) && (tech->time <= 0) && RS_TechIsResearchable(tech)) {
			RS_ResearchFinish(tech);
			Com_DPrintf("Depending tech \"%s\" has been researched as well.\n", tech->id);
#endif
		}
	}
	RS_MarkResearchable();
}

/**
 * @brief Checks the research status
 * @todo Needs to check on the exact time that elapsed since the last check fo the status.
 *
 */
void CL_CheckResearchStatus(void)
{
	int i, newResearch = 0;
	technology_t *tech = NULL;

	if (!researchListLength)
		return;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (tech->statusResearch == RS_RUNNING) {
			if ( ( tech->time > 0 ) && ( tech->scientists >= 0 ) ) {
				Com_DPrintf("timebefore %.2f\n", tech->time);
				Com_DPrintf("timedelta %.2f\n", tech->scientists * 0.8);
				/* TODO: Just for testing, better formular may be needed. */
				tech->time -= tech->scientists * 0.8;
				Com_DPrintf("timeafter %.2f\n", tech->time);
				/* TODO include employee-skill in calculation. */
				/* Will be a good thing (think of percentage-calculation) once non-integer values are used. */
				if (tech->time <= 0) {
					Com_sprintf(messageBuffer, MAX_MESSAGE_TEXT, _("Research of %s finished\n"), tech->name);
					MN_AddNewMessage(_("Research finished"), messageBuffer, qfalse, MSG_RESEARCH, tech);

					/* Remove all scientists from the technology. */
					while (tech->scientists > 0)
						RS_RemoveScientist(tech);

					RS_MarkResearched(tech->id);
					researchListLength = 0;
					researchListPos = 0;
					newResearch++;
					tech->time = 0;
				}
			}
		}
	}

	if (newResearch) {
		CL_GameTimeStop();
		RS_UpdateData();
	}
}

/**
 * @brief Returns a list of technologies for the given type
 * @note this list is terminated by a NULL pointer
 */
static char *RS_TechTypeToName(researchType_t type)
{
	switch(type) {
	case RS_TECH:
		return "tech";
	case RS_WEAPON:
		return "weapon";
	case RS_ARMOR:
		return "armor";
	case RS_CRAFT:
		return "craft";
	case RS_CRAFTWEAPON:
		return "craftweapon";
	case RS_CRAFTSHIELD:
		return "craftshield";
	case RS_BUILDING:
		return "building";
	case RS_ALIEN:
		return "alien";
	case RS_UGV:
		return "ugv";
	case RS_NEWS:
		return "news";
	case RS_LOGIC:
		return "logic";
	default:
		return "unknown";
	}
}

#ifdef DEBUG
/**
 * @brief List all parsed technologies and their attributes in commandline/console.
 * Command to call this: techlist
 */
static void RS_TechnologyList_f(void)
{
	int i, j;
	technology_t *tech = NULL;
	requirements_t *req = NULL;

	Com_Printf("#techs: %i\n", gd.numTechnologies);
	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		Com_Printf("Tech: %s\n", tech->id);
		Com_Printf("... time      -> %.2f\n", tech->time);
		Com_Printf("... name      -> %s\n", tech->name);
		req = &tech->require_AND;
		Com_Printf("... requires ALL  ->");
		for (j = 0; j < req->numLinks; j++)
			Com_Printf(" %s (%s) %i", req->id[j], RS_TechTypeToName(req->type[j]), req->idx[j]);
		req = &tech->require_OR;
		Com_Printf("\n");
		Com_Printf("... requires ANY  ->");
		for (j = 0; j < req->numLinks; j++)
			Com_Printf(" %s (%s) %i", req->id[j], RS_TechTypeToName(req->type[j]), req->idx[j]);
		Com_Printf("\n");
		Com_Printf("... provides  -> %s", tech->provides);
		Com_Printf("\n");

		Com_Printf("... type      -> ");
		Com_Printf("%s\n", RS_TechTypeToName(tech->type));

		Com_Printf("... researchable -> %i\n", tech->statusResearchable);
		if (tech->statusResearchable) {
			Com_Printf("... researchable date: %02i %02i %i\n", tech->preResearchedDateDay, tech->preResearchedDateMonth,
				tech->preResearchedDateYear);
		}

		Com_Printf("... research  -> ");
		switch (tech->statusResearch) {
		case RS_NONE:
			Com_Printf("nothing\n");
			break;
		case RS_RUNNING:
			Com_Printf("running\n");
			break;
		case RS_PAUSED:
			Com_Printf("paused\n");
			break;
		case RS_FINISH:
			Com_Printf("done\n");
			Com_Printf("... research date: %02i %02i %i\n", tech->researchedDateDay, tech->researchedDateMonth,
				tech->researchedDateYear);
			break;
		default:
			Com_Printf("unknown\n");
			break;
		}
	}
}
#endif /* DEBUG */

/**
 * @brief Research menu init function binding
 * @note Command to call this: research_init
 *
 * @note Should be called whenever the research menu gets active
 * @sa CL_ResearchType
 */
void MN_ResearchInit(void)
{
	CL_ResearchType();
}

/**
 * @brief Mark everything as researched
 * @sa CL_Connect_f
 * @sa MN_StartServer
 */
void RS_MarkResearchedAll(void)
{
	int i;

	for (i = 0; i < gd.numTechnologies; i++) {
		Com_DPrintf("...mark %s as researched\n", gd.technologies[i].id);
		RS_MarkOneResearchable(i);
		RS_ResearchFinish(&gd.technologies[i]);
		/* TODO: Set all "collected" entries in the requirements to the "amount" value. */
	}
}

#ifdef DEBUG
/**
 * @brief Set all item to researched
 * @note Just for debugging purposes
 */
static void RS_DebugResearchAll(void)
{
	technology_t *tech = NULL;

	if (Cmd_Argc() != 2) {
		RS_MarkResearchedAll();
	} else {
		tech= RS_GetTechByID(Cmd_Argv(1));
		Com_DPrintf("...mark %s as researched\n", tech->id);
		RS_MarkOneResearchable(tech->idx);
		RS_ResearchFinish(tech);
	}
}

/**
 * @brief Set all item to researched
 * @note Just for debugging purposes
 */
static void RS_DebugResearchableAll(void)
{
	int i;
	technology_t *tech = NULL;

	if (Cmd_Argc() != 2) {
		for (i = 0; i < gd.numTechnologies; i++) {
			tech = &gd.technologies[i];
			Com_Printf("...mark %s as researchable\n", tech->id);
			RS_MarkOneResearchable(i);
			tech->statusCollected = qtrue;
		}
	} else {
		tech = RS_GetTechByID(Cmd_Argv(1));
		if (tech) {
				Com_Printf("...mark %s as researchable\n", tech->id);
				RS_MarkOneResearchable(tech->idx);
				tech->statusCollected = qtrue;
		}
	}
}
#endif

/**
 * @brief Opens UFOpedia by clicking dependency list
 */
void RS_DependenciesClick_f (void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: research_dependencies_click <num>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num >= curRequiredList.numEntries)
		return;

	UP_OpenWith(curRequiredList.string[num]);
}

/**
 * @brief This is more or less the initial
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from MN_ResetMenus resp. CL_InitLocal
 */
void RS_ResetResearch(void)
{
	researchListLength = 0;
	/* add commands and cvars */
	Cmd_AddCommand("research_init", MN_ResearchInit, "Research menu init function binding");
	Cmd_AddCommand("research_select", CL_ResearchSelectCmd, NULL);
	Cmd_AddCommand("research_type", CL_ResearchType, NULL);
	Cmd_AddCommand("mn_start_research", RS_ResearchStart, NULL);
	Cmd_AddCommand("mn_stop_research", RS_ResearchStop, NULL);
	Cmd_AddCommand("mn_rs_add", RS_AssignScientist_f, NULL);
	Cmd_AddCommand("mn_rs_remove", RS_RemoveScientist_f, NULL);
	Cmd_AddCommand("research_update", RS_UpdateData, NULL);
	Cmd_AddCommand("invlist", Com_InventoryList_f, NULL);
	Cmd_AddCommand("research_dependencies_click", RS_DependenciesClick_f, NULL);
#ifdef DEBUG
	Cmd_AddCommand("techlist", RS_TechnologyList_f, NULL);
	Cmd_AddCommand("research_all", RS_DebugResearchAll, NULL);
	Cmd_AddCommand("researchable_all", RS_DebugResearchableAll, NULL);
#endif
}

/**
 * @brief The valid definition names in the research.ufo file.
 */
static value_t valid_tech_vars[] = {
	/*name of technology */
	{"name", V_TRANSLATION2_STRING, offsetof(technology_t, name)},
	{"description", V_TRANSLATION2_STRING, offsetof(technology_t, description)},
	{"pre_description", V_TRANSLATION2_STRING, offsetof(technology_t, pre_description)},
	/*what does this research provide */
	{"provides", V_STRING, offsetof(technology_t, provides)},
	/* ("require")	Handled in parser below. */
	/* ("delay", V_INT, offsetof(technology_t, require->delay)}, can this work? */

	{"producetime", V_INT, offsetof(technology_t, produceTime)},
	{"time", V_FLOAT, offsetof(technology_t, time)},
	{"image_top", V_STRING, offsetof(technology_t, image_top)},
	{"image_bottom", V_STRING, offsetof(technology_t, image_bottom)},
	{"mdl_top", V_STRING, offsetof(technology_t, mdl_top)},
	{"mdl_bottom", V_STRING, offsetof(technology_t, mdl_bottom)},
	{NULL, 0, 0}
};

/**
 * @brief Parses one "tech" entry in the research.ufo file and writes it into the next free entry in technologies (technology_t).
 * @param[in] id Unique id of a technology_t. This is parsed from "tech xxx" -> id=xxx
 * @param[in] text the whole following text that is part of the "tech" item definition in research.ufo.
 */
void RS_ParseTechnologies(char *id, char **text)
{
	value_t *var = NULL;
	technology_t *tech = NULL;
	int tech_old;
	char *errhead = "RS_ParseTechnologies: unexptected end of file.";
	char *token = NULL;
	requirements_t *required_temp = NULL;

	int i;

	/* get body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("RS_ParseTechnologies: \"%s\" technology def without body ignored.\n", id);
		return;
	}
	if (gd.numTechnologies >= MAX_TECHNOLOGIES) {
		Com_Printf("RS_ParseTechnologies: too many technology entries. limit is %i.\n", MAX_TECHNOLOGIES);
		return;
	}

	/* New technology (next free entry in global tech-list) */
	tech = &gd.technologies[gd.numTechnologies];
	gd.numTechnologies++;

	memset(tech, 0, sizeof(technology_t));

	/*set standard values */
	tech->idx = gd.numTechnologies - 1;
	Com_sprintf(tech->id, MAX_VAR, id);
	Com_sprintf(tech->description, MAX_VAR, _("No description available."));
	tech->type = RS_TECH;
	tech->statusResearch = RS_NONE;
	tech->statusResearchable = qfalse;
	tech->time = 0;
	tech->overalltime = 0;
	tech->scientists = 0;
	tech->prev = -1;
	tech->next = -1;
	tech->base_idx = -1;
	tech->up_chapter = -1;

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, id);
		if (!*text)
			break;
		if (*token == '}')
			break;
		/* get values */
		if (!Q_strncmp(token, "type", MAX_VAR)) {
			/* what type of tech this is */
			token = COM_EParse(text, errhead, id);
			if (!*text)
				return;
			/* redundant, but oh well. */
			if (!Q_strncmp(token, "tech", MAX_VAR))
				tech->type = RS_TECH;
			else if (!Q_strncmp(token, "weapon", MAX_VAR))
				tech->type = RS_WEAPON;
			else if (!Q_strncmp(token, "news", MAX_VAR))
				tech->type = RS_NEWS;
			else if (!Q_strncmp(token, "armor", MAX_VAR))
				tech->type = RS_ARMOR;
			else if (!Q_strncmp(token, "craft", MAX_VAR))
				tech->type = RS_CRAFT;
			else if (!Q_strncmp(token, "craftweapon", MAX_VAR))
				tech->type = RS_CRAFTWEAPON;
			else if (!Q_strncmp(token, "craftshield", MAX_VAR))
				tech->type = RS_CRAFTSHIELD;
			else if (!Q_strncmp(token, "building", MAX_VAR))
				tech->type = RS_BUILDING;
			else if (!Q_strncmp(token, "alien", MAX_VAR))
				tech->type = RS_ALIEN;
			else if (!Q_strncmp(token, "ugv", MAX_VAR))
				tech->type = RS_UGV;
			else if (!Q_strncmp(token, "logic", MAX_VAR))
				tech->type = RS_LOGIC;
			else
				Com_Printf("RS_ParseTechnologies: \"%s\" unknown techtype: \"%s\" - ignored.\n", id, token);
		} else {

			if ((!Q_strncmp(token, "require_AND", MAX_VAR)) || (!Q_strncmp(token, "require_OR", MAX_VAR))) {
				/* Link to correct list. */
				if (!Q_strncmp(token, "require_AND", MAX_VAR)) {
					required_temp = &tech->require_AND;
				} else {
					required_temp = &tech->require_OR;
				}

				token = COM_EParse(text, errhead, id);
				if (!*text)
					break;
				if (*token != '{')
					break;
				if (*token == '}')
					break;

				do {	/* Loop through all 'require' entries.*/
					token = COM_EParse(text, errhead, id);
					if (!*text)
						return;
					if (*token == '}')
						break;

					if ( (!Q_strcmp(token, "tech")) ||  (!Q_strcmp(token, "weapon")) ) {
						if (required_temp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							if (!Q_strcmp(token, "tech")) {
								required_temp->type[required_temp->numLinks] = RS_LINK_TECH;
							} else {	/* weapon */
								/* Ammo only: Defines what weapon can use this ammo. */
								required_temp->type[required_temp->numLinks] = RS_LINK_WEAPON;
							}
							/* Set requirement-name (id). */
							token = COM_Parse(text);
							Q_strncpyz(required_temp->id[required_temp->numLinks], token, MAX_VAR);
							Com_DPrintf("RS_ParseTechnologies: tech - %s\n", required_temp->id[required_temp->numLinks]);
							required_temp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", id, MAX_TECHLINKS);
						}
					} else if (!Q_strcmp(token, "item")) {
						/* Defines what items need to be collected for this item to be researchable. */
						if (required_temp->numLinks < MAX_TECHLINKS) {
							/* Set requirement-type. */
							required_temp->type[required_temp->numLinks] = RS_LINK_ITEM;
							/* Set requirement-name (id). */
							token = COM_Parse(text);
							Q_strncpyz(required_temp->id[required_temp->numLinks], token, MAX_VAR);
							/* Set requirement-amount of item. */
							token = COM_Parse(text);
							required_temp->amount[required_temp->numLinks] = atoi(token);
							Com_DPrintf("RS_ParseTechnologies: item - %s - %i\n", required_temp->id[required_temp->numLinks], required_temp->amount[required_temp->numLinks]);
							required_temp->numLinks++;
						} else {
							Com_Printf("RS_ParseTechnologies: \"%s\" Too many 'required' defined. Limit is %i - ignored.\n", id, MAX_TECHLINKS);
						}
					} else if (!Q_strcmp(token, "event")) {
						token = COM_Parse(text);
						Com_DPrintf("RS_ParseTechnologies: event - %s\n", token);
						/* Get name/id & amount of required item. */
						/* TODO: Implement final event esystem, so this can work 100% */
					} else {
						Com_Printf("RS_ParseTechnologies: \"%s\" unknown requirement-type: \"%s\" - ignored.\n", id, token);
					}
				} while (*text);
			} else if (!Q_strncmp(token, "delay", MAX_VAR)) {
				/* TODO: Move this to the default parser? See valid_tech_vars. */
				token = COM_Parse(text);
				Com_DPrintf("RS_ParseTechnologies: delay - %s\n", token);
			} else if (!Q_strncmp(token, "up_chapter", MAX_VAR)) {
				/* ufopedia chapter */
				token = COM_EParse(text, errhead, id);
				if (!*text)
					return;

				if (*token) {
					/* find chapter */
					for (i = 0; i < gd.numChapters; i++) {
						if (!Q_strncmp(token, gd.upChapters[i].id, MAX_VAR)) {
							/* add entry to chapter */
							tech->up_chapter = i;
							if (!gd.upChapters[i].first) {
								gd.upChapters[i].first = tech->idx;
								gd.upChapters[i].last = tech->idx;
								tech->prev = -1;
								tech->next = -1;
							} else {
								/* get "last entry" in chapter */
								tech_old = gd.upChapters[i].last;
								gd.upChapters[i].last = tech->idx;
								gd.technologies[tech_old].next = tech->idx;
								gd.technologies[gd.upChapters[i].last].prev = tech_old;
								gd.technologies[gd.upChapters[i].last].next = -1;
							}
							break;
						}
						if (i == gd.numChapters)
							Com_Printf("RS_ParseTechnologies: \"%s\" - chapter \"%s\" not found.\n", id, token);
					}
				}
			} else {
				for (var = valid_tech_vars; var->string; var++)
					if (!Q_strncmp(token, var->string, sizeof(var->string))) {
						/* found a definition */
						token = COM_EParse(text, errhead, id);
						if (!*text)
							return;

						if (var->ofs && var->type != V_NULL)
							Com_ParseValue(tech, token, var->type, var->ofs);
						else
							/* NOTE: do we need a buffer here? for saving or something like that? */
							Com_Printf("RS_ParseTechnologies Error: - no buffer for technologies - V_NULL not allowed\n");
						break;
					}
				/*TODO: escape "type weapon/tech/etc.." here */
				if (!var->string)
					Com_Printf("RS_ParseTechnologies: unknown token \"%s\" ignored (entry %s)\n", token, id);
			}
		}
	} while (*text);

	/* set the overall reseach time to the one given in the ufo-file. */
	tech->overalltime = tech->time;
}

/**
 * @brief Checks whether an item is already researched
 * @sa RS_IsResearched_ptr
 */
qboolean RS_IsResearched_idx(int idx)
{
	if (ccs.singleplayer == qfalse)
		return qtrue;
	if (idx >= 0 && gd.technologies[idx].statusResearch == RS_FINISH)
		return qtrue;
	return qfalse;
}

/**
 * @brief Checks whether an item is already researched
 * @sa RS_IsResearched_idx
 * Call this function if you already hold a tech pointer
 */
qboolean RS_IsResearched_ptr(technology_t * tech)
{
	if (ccs.singleplayer == qfalse)
		return qtrue;
	if (tech && tech->statusResearch == RS_FINISH)
		return qtrue;
	return qfalse;
}

/**
 * @brief Checks if the item (as listed in "provides") has been researched
 * @param[in] id_provided Unique id of an item/building/etc.. that is provided by a technology_t
 * @return qboolean
 * @sa RS_IsResearched_ptr
 */
qboolean RS_ItemIsResearched(char *id_provided)
{
	int i;
	technology_t *tech = NULL;

	/* in multiplaer everyting is researched */
	if (!ccs.singleplayer)
		return qtrue;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		/* Provided item found. */
		if (!Q_strncmp(id_provided, tech->provides, MAX_VAR))
			return RS_IsResearched_ptr(tech);
	}
	/* no research needed */
	return qtrue;
}

/**
 * @brief
 * @sa RS_ItemCollected
 * Call this function if you already hold a tech pointer.
 */
int RS_Collected_(technology_t * tech)
{
	if (tech)
		return tech->statusCollected;

	Com_DPrintf("RS_Collected_: NULL technology given.\n");
	return -1;
}

/**
 * @brief Checks if the technology (tech-id) has been researched.
 * @param[in] tech_idx index of the technology.
 * @return qboolean Returns qtrue if the technology has been researched, otherwise qfalse;
 */
qboolean RS_TechIsResearched(int tech_idx)
{
	if (tech_idx < 0)
		return qfalse;

	/* research item found */
	if (gd.technologies[tech_idx].statusResearch == RS_FINISH)
		return qtrue;

	return qfalse;
}

/**
 * @brief Checks if the technology (tech-id) is researchable.
 * @param[in] tech pointer to technology_t.
 * @return qboolean
 * @sa RS_TechIsResearched
 */
qboolean RS_TechIsResearchable(technology_t * tech)
{
	if (!tech)
		return qfalse;

	/* Research item found */
	if (tech->statusResearch == RS_FINISH)
		return qfalse;

	if (tech->statusResearchable)
		return qtrue;

	return RS_RequirementsMet(&tech->require_AND, &tech->require_OR);
}

/**
 * @brief Returns a list of .ufo items that are produceable when this item has been researched (=provided)
 * This list also incldues other items that "require" this one (id) and have a reseach_time of 0.
 */
#if 0
void RS_GetProvided(char *id, char *provided)
{
	int i, j;
	technology_t *tech = NULL;

	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (!strcmp(id, tech->id)) {
			for (j = 0; j < MAX_TECHLINKS; j++)
				Com_sprintf(provided[j], MAX_VAR, tech->provides);
			/*TODO: search for dependent items. */
			for (j = 0; j < gd.numTechnologies; j++) {
				if (RS_DependsOn(tech->id, id)) {
					/* TODO: append researchtree[j]->provided to *provided */
				}
			}
			return;
		}
	}
	Com_Printf("RS_GetProvided: research item \"%s\" not found.\n", id);
}
#endif

/**
 * @brief Returns the index of this item in the inventory.
 * @todo This function should be located in a inventory-related file.
 * @note id may not be null or empty
 * @param[in] id the item id in our object definition array (csi.ods)
 */
int RS_GetItem(char *id)
{
	int i;
	objDef_t *item = NULL;

#ifdef DEBUG
	if (!id || !*id)
		Com_Printf("RS_GetItem: Called with empty id\n");
#endif
	for (i = 0; i < csi.numODs; i++) {	/* i = item index */
		item = &csi.ods[i];
		if (!Q_strncmp(id, item->kurz, MAX_VAR)) {
			return i;
		}
	}

	Com_Printf("RS_GetItem: Item \"%s\" not found.\n", id);
	return -1;
}


/**
 * @brief Returns the tech pointer
 * @param id unique id of a technology_t
 */
technology_t* RS_GetTechByIDX(int tech_idx)
{
	if (tech_idx < 0 || tech_idx >= gd.numTechnologies)
		return NULL;
	else
		return &gd.technologies[tech_idx];
}


/**
 * @brief return a pointer to the technology identified by given id string
 */
technology_t *RS_GetTechByID(const char *id)
{
	int i = 0;

	if (!id || !*id)
		return NULL;

	if (!Q_strncmp((char *) id, "nothing", MAX_VAR))
		return NULL;

	for (; i < gd.numTechnologies; i++) {
		if (!Q_strncmp((char *) id, gd.technologies[i].id, MAX_VAR))
			return &gd.technologies[i];
	}
	Com_DPrintf("RS_GetTechByID: Could not find a technology with id \"%s\"\n", id);
	return NULL;
}

/**
 * @brief returns a pointer to the item tech (as listed in "provides")
 */
technology_t *RS_GetTechByProvided(const char *id_provided)
{
	int i;

	for (i = 0; i < gd.numTechnologies; i++)
		if (!Q_strncmp((char *) id_provided, gd.technologies[i].provides, MAX_VAR))
			return &gd.technologies[i];

	/* if a building, probably needs another building */
	/* if not a building, catch NULL where function is called! */
	return NULL;
}

/**
 * @brief Returns a list of technologies for the given type
 * @note this list is terminated by a NULL pointer
 */
technology_t **RS_GetTechsByType(researchType_t type)
{
	static technology_t *techList[MAX_TECHNOLOGIES];
	int i, j = 0;

	for (i=0; i<gd.numTechnologies;i++) {
		if (gd.technologies[i].type == type ) {
			techList[j] = &gd.technologies[i];
			j++;
			/* j+1 because last item have to be NULL */
			if ( j+1 >= MAX_TECHNOLOGIES ) {
				Com_Printf("RS_GetTechsByType: MAX_TECHNOLOGIES limit hit\n");
				break;
			}
		}
	}
	techList[j] = NULL;
	Com_DPrintf("techlist with %i entries\n", j);
	return techList;
}

/**
 * @brief Searches for the technology that has teh most scientists assigned in a given base.
 * @param[in] base_idx In what base the tech shoudl be researched.
 * @sa E_RemoveEmployeeFromBuilding
 */
technology_t *RS_GetTechWithMostScientists( int base_idx )
{
	technology_t *tech = NULL;
	technology_t *tech_temp = NULL;
	int i = 0;
	int max = 0;

	for (i=0; i<gd.numTechnologies;i++) {
		tech_temp = &gd.technologies[i];
		if ( (tech_temp->statusResearch == RS_RUNNING) && (tech_temp->base_idx == base_idx) ) {
			if (tech_temp->scientists > max) {
				tech = tech_temp;
				max = tech->scientists;
			}
		}
	}

	return tech;
}

/**
 * @brief Returns the index (idx) of a "tech" entry given it's name.
 * @param[in] name the name of the tech
 * TODO: this method is extremely inefficient... it could be dramatically improved
 */
int RS_GetTechIdxByName(char *name)
{
	int i;
	technology_t *tech;

	/* return -1 if tech matches "nothing" */
	if (!Q_strncmp(name, "nothing", MAX_VAR))
		return -1;

	/* search through all techs for a match */
	for (i = 0; i < gd.numTechnologies; i++) {
		tech = &gd.technologies[i];
		if (!Q_strncmp(name, tech->id, MAX_VAR))
			return i;
	}

	/* return -1 if not found */
	return -1;
}
