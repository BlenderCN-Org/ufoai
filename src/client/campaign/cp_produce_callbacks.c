/**
 * @file cp_produce_callbacks.c
 * @brief Menu related callback functions used for production.
 * @todo Remove direct access to nodes
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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
#include "../cl_menu.h"
#include "../menu/m_popup.h"
#include "cp_campaign.h"
#include "cp_market.h"
#include "cp_produce.h"
#include "cp_produce_callbacks.h"


/** Maximum number of produced/disassembled items. */
#define MAX_PRODUCTION_AMOUNT 500

/**
 * Holds the current active production category/filter type.
 * @sa itemFilterTypes_t
 */
static int produceCategory = FILTER_S_PRIMARY;

/** Holds the current active selected queue entry. */
static production_t *selectedProduction = NULL;

/** A list if all producable items. */
static linkedList_t *productionItemList;

/** Currently selected entry in the productionItemList (depends on content) */
static objDef_t *selectedItem = NULL;
static components_t *selectedDisassembly = NULL;
static aircraft_t *selectedAircraft = NULL;

/** @brief Number of blank lines between queued items and tech list. */
static const int QUEUE_SPACERS = 2;

/**
 * @brief Resets all "selected" pointers to NULL.
 */
static void PR_ClearSelected (void)
{
	selectedProduction = NULL;
	selectedAircraft = NULL;
	selectedItem = NULL;
	selectedDisassembly = NULL;
}

/**
 * @brief Conditions for disassembling.
 * @param[in] base Pointer to base where the component is stored.
 * @param[in] comp Pointer to components definition.
 * @return qtrue if disassembling is ready, qfalse otherwise.
 */
static qboolean PR_ConditionsDisassembly (const base_t *base, const components_t *comp)
{
	const objDef_t *od;

	assert(base);
	assert(comp);

	od = comp->assemblyItem;
	assert(od);

	if (RS_IsResearched_ptr(od->tech) && base->storage.num[od->idx] > 0)
		return qtrue;
	else
		return qfalse;
}

/**
 * @brief Checks if the production requirements are met for a defined amount.
 * @param[in] amount How many items are planned to be produced.
 * @param[in] reqs The production requirements of the item that is to be produced.
 * @param[in] base Pointer to base.
 * @return 0: If nothing can be produced. 1+: If anything can be produced. 'amount': Maximum.
 */
static int PR_RequirementsMet (int amount, requirements_t *reqs, base_t *base)
{
	int a, i;
	int producibleAmount = 0;

	for (a = 0; a < amount; a++) {
		qboolean producible = qtrue;
		for (i = 0; i < reqs->numLinks; i++) {
			const requirement_t *req = &reqs->links[i];
			if (req->type == RS_LINK_ITEM) {
				/* The same code is used in "RS_RequirementsMet" */
				Com_DPrintf(DEBUG_CLIENT, "PR_RequirementsMet: %s\n", req->id);
				if (B_ItemInBase(req->link, base) < req->amount) {
					producible = qfalse;
				}
			}
		}
		if (producible)
			producibleAmount++;
		else
			break;
	}
	return producibleAmount;
}

/**
 * @brief Add a new item to the bottom of the production queue.
 * @param[in] base Pointer to base, where the queue is.
 * @param[in] queue Pointer to the queue.
 * @param[in] item Item to add.
 * @param[in] aircraftTemplate aircraft to add.
 * @param[in] amount Desired amount to produce.
 * @param[in] disassembling True if this is disassembling, false if production.
 */
static production_t *PR_QueueNew (base_t *base, production_queue_t *queue, objDef_t *item, aircraft_t *aircraftTemplate, signed int amount, qboolean disassembling)
{
	int numWorkshops = 0;
	production_t *prod;
	const technology_t *tech;

	assert((item && !aircraftTemplate) || (!item && aircraftTemplate));
	assert(base);

	if (queue->numItems >= MAX_PRODUCTIONS)
		return NULL;

	if (E_CountHired(base, EMPL_WORKER) <= 0) {
		MN_Popup(_("Not enough workers"), _("You cannot queue productions without workers hired in this base.\n\nHire workers."));
		return NULL;
	}

	numWorkshops = max(B_GetNumberOfBuildingsInBaseByBuildingType(base, B_WORKSHOP), 0);

	if (queue->numItems >= numWorkshops * MAX_PRODUCTIONS_PER_WORKSHOP) {
		MN_Popup(_("Not enough workshops"), _("You cannot queue more items.\nBuild more workshops.\n"));
		return NULL;
	}

	/* Initialize */
	prod = &queue->items[queue->numItems];
	memset(prod, 0, sizeof(*prod));

	/* self-reference. */
	prod->idx = queue->numItems;

	if (produceCategory != FILTER_AIRCRAFT)
		tech = item->tech;
	else
		tech = aircraftTemplate->tech;

	/* We cannot queue new aircraft if no free hangar space. */
	if (produceCategory == FILTER_AIRCRAFT) {
		if (!B_GetBuildingStatus(base, B_COMMAND)) {
			MN_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo command centre in this base.\n"));
			return NULL;
		} else if (!B_GetBuildingStatus(base, B_HANGAR) && !B_GetBuildingStatus(base, B_SMALL_HANGAR)) {
			MN_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo hangars in this base.\n"));
			return NULL;
		}
		/** @todo we should also count aircraft that are already in the queue list */
		if (AIR_CalculateHangarStorage(aircraftTemplate, base, 0) <= 0) {
			MN_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo free space in hangars.\n"));
			return NULL;
		}
	}

	prod->item = item;
	prod->aircraft = aircraftTemplate;
	prod->amount = amount;
	if (disassembling) {
		/* Disassembling. */
		prod->production = qfalse;

		/* We have to remove amount of items being disassembled from base storage. */
		base->storage.num[item->idx] -= amount;
		/* Now find related components definition. */
		prod->percentDone = 0.0f;
	} else {
		/* Production. */
		prod->production = qtrue;

		/* Don't try to add to queue an item which is not producible. */
		if (tech->produceTime < 0)
			return NULL;
		else
			prod->percentDone = 0.0f;
	}

	queue->numItems++;
	return prod;
}

/**
 * @brief update the list of queued and available items
 * @param[in] base Pointer to the base.
 */
static void PR_UpdateProductionList (const base_t* base)
{
	int i, j, counter;
	linkedList_t *productionList = NULL;
	linkedList_t *productionQueued = NULL;
	linkedList_t *productionAmount = NULL;
	const production_queue_t *queue;

	assert(base);

	queue = &ccs.productions[base->idx];

	/* First add all the queue items ... */
	for (i = 0; i < queue->numItems; i++) {
		const production_t *prod = &queue->items[i];
		if (!prod->aircraft) {
			const objDef_t *od = prod->item;
			LIST_AddString(&productionList, va("%s", _(od->name)));
			LIST_AddString(&productionAmount, va("%i", base->storage.num[prod->item->idx]));
			LIST_AddString(&productionQueued, va("%i", prod->amount));
		} else {
			const aircraft_t *aircraftTemplate = prod->aircraft;
			LIST_AddString(&productionList, va("%s", _(aircraftTemplate->name)));
			for (j = 0, counter = 0; j < ccs.numAircraft; j++) {
				const aircraft_t *aircraftBase = AIR_AircraftGetFromIDX(j);
				assert(aircraftBase);
				if (aircraftBase->homebase == base && aircraftBase->tpl == aircraftTemplate)
					counter++;
			}
			LIST_AddString(&productionAmount, va("%i", counter));
			LIST_AddString(&productionQueued, va("%i", prod->amount));
		}
	}

	/* Then spacers ... */
	for (i = 0; i < QUEUE_SPACERS; i++) {
		LIST_AddString(&productionList, "");
		LIST_AddString(&productionAmount, "");
		LIST_AddString(&productionQueued, "");
	}

	LIST_Delete(&productionItemList);

	/* Then go through all object definitions ... */
	if (produceCategory == FILTER_DISASSEMBLY) {
		for (i = 0; i < ccs.numComponents; i++) {
			const objDef_t *asOd = ccs.components[i].assemblyItem;
			components_t *comp = &ccs.components[i];
			if (!asOd)
				continue;
			if (PR_ConditionsDisassembly(base, comp)) {
				LIST_AddPointer(&productionItemList, comp);

				LIST_AddString(&productionList, va("%s", _(asOd->name)));
				LIST_AddString(&productionAmount, va("%i", base->storage.num[asOd->idx]));
				LIST_AddString(&productionQueued, "");
			}
		}
	} else if (produceCategory == FILTER_AIRCRAFT) {
		for (i = 0; i < ccs.numAircraftTemplates; i++) {
			aircraft_t *aircraftTemplate = &ccs.aircraftTemplates[i];
			/* don't allow producing ufos */
			if (aircraftTemplate->ufotype != UFO_MAX)
				continue;
			if (!aircraftTemplate->tech) {
				Com_Printf("PR_UpdateProductionList: no technology for craft %s!\n", aircraftTemplate->id);
				continue;
			}

			Com_DPrintf(DEBUG_CLIENT, "air: %s ufotype: %i tech: %s time: %i\n", aircraftTemplate->id,
					aircraftTemplate->ufotype, aircraftTemplate->tech->id, aircraftTemplate->tech->produceTime);

			if (aircraftTemplate->tech->produceTime > 0 && RS_IsResearched_ptr(aircraftTemplate->tech)) {
				LIST_AddPointer(&productionItemList, aircraftTemplate);

				LIST_AddString(&productionList, va("%s", _(aircraftTemplate->name)));
				for (j = 0, counter = 0; j < ccs.numAircraft; j++) {
					const aircraft_t *aircraftBase = AIR_AircraftGetFromIDX(j);
					assert(aircraftBase);
					if (aircraftBase->homebase == base && aircraftBase->tpl == aircraftTemplate)
						counter++;
				}
				LIST_AddString(&productionAmount, va("%i", counter));
				LIST_AddString(&productionQueued, "");
			}
		}
	} else {
		objDef_t *od;
		for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			assert(od->tech);
			/* We will not show items with producetime = -1 - these are not producible.
			 * We can produce what was researched before. */
			if (od->name[0] != '\0' && od->tech->produceTime > 0 && RS_IsResearched_ptr(od->tech)
			 && INV_ItemMatchesFilter(od, produceCategory)) {
				LIST_AddPointer(&productionItemList, od);

				LIST_AddString(&productionList, va("%s", _(od->name)));
				LIST_AddString(&productionAmount, va("%i", base->storage.num[i]));
				LIST_AddString(&productionQueued, "");
			}
		}
	}

	/* bind the menu text to our static char array */
	MN_RegisterLinkedListText(TEXT_PRODUCTION_LIST, productionList);
	/* bind the amount of available items */
	MN_RegisterLinkedListText(TEXT_PRODUCTION_AMOUNT, productionAmount);
	/* bind the amount of queued items */
	MN_RegisterLinkedListText(TEXT_PRODUCTION_QUEUED, productionQueued);
}

/**
 * @brief Prints information about the selected item (no aircraft) in production.
 * @param[in] base Pointer to the base where informations should be printed.
 * @sa PR_ProductionInfo
 */
static void PR_ItemProductionInfo (const base_t *base, const objDef_t *od, float percentDone)
{
	static char productionInfo[512];
	int time;
	float prodPerHour;

	assert(base);
	assert(od);
	assert(od->tech);

	/* Don't try to display an item which is not producible. */
	if (od->tech->produceTime < 0) {
		Com_sprintf(productionInfo, sizeof(productionInfo), _("No item selected"));
		Cvar_Set("mn_item", "");
	} else {
		prodPerHour = PR_CalculateProductionPercentDone(base, od->tech, NULL, qfalse);
		/* If you entered production menu, that means that prodPerHour > 0 (must not divide by 0) */
		assert(prodPerHour > 0);
		time = ceil((1.0f - percentDone) / prodPerHour);

		Com_sprintf(productionInfo, sizeof(productionInfo), "%s\n", _(od->name));
		Q_strcat(productionInfo, va(_("Costs per item\t%i c\n"), (od->price * PRODUCE_FACTOR / PRODUCE_DIVISOR)),
			sizeof(productionInfo));
		Q_strcat(productionInfo, va(_("Production time\t%ih\n"), time), sizeof(productionInfo));
		Q_strcat(productionInfo, va(_("Item size\t%i\n"), od->size), sizeof(productionInfo));
		Cvar_Set("mn_item", od->id);
	}
	MN_RegisterText(TEXT_PRODUCTION_INFO, productionInfo);
}

/**
 * @brief Prints information about the selected disassembly task
 * @param[in] base Pointer to the base where informations should be printed.
 * @sa PR_ProductionInfo
 */
static void PR_DisassemblyInfo (const base_t *base, const objDef_t *od, const components_t *comp, float percentDone)
{
	static char productionInfo[512];
	int time, i;
	float prodPerHour;

	assert(base);
	assert(od);
	assert(comp);
	assert(od->tech);

	prodPerHour = PR_CalculateProductionPercentDone(base, od->tech, comp, qtrue);
	/* If you entered production menu, that means that prodPerHour > 0 (must not divide by 0) */
	assert(prodPerHour > 0);
	time = ceil((1.0f - percentDone) / prodPerHour);

	Com_sprintf(productionInfo, sizeof(productionInfo), _("%s - disassembly\n"), _(od->name));
	Q_strcat(productionInfo, _("Components: "), sizeof(productionInfo));
	/* Print components. */
	for (i = 0; i < comp->numItemtypes; i++) {
		const objDef_t *compOd = comp->items[i];
		assert(compOd);
		Q_strcat(productionInfo, va(_("%s (%i) "), _(compOd->name), comp->itemAmount[i]),
			sizeof(productionInfo));
	}
	Q_strcat(productionInfo, "\n", sizeof(productionInfo));
	Q_strcat(productionInfo, va(_("Disassembly time\t%ih\n"), time), sizeof(productionInfo));
	Cvar_Set("mn_item", od->id);
	MN_RegisterText(TEXT_PRODUCTION_INFO, productionInfo);
}

/**
 * @brief Prints information about the selected aircraft in production.
 * @param[in] aircraftTemplate The aircraft to print the information for
 * @sa PR_ProductionInfo
 */
static void PR_AircraftInfo (const aircraft_t *aircraftTemplate)
{
	static char productionInfo[512];
	assert(aircraftTemplate);

	Com_sprintf(productionInfo, sizeof(productionInfo), "%s\n", _(aircraftTemplate->name));
	Q_strcat(productionInfo, va(_("Production costs\t%i c\n"), (aircraftTemplate->price * PRODUCE_FACTOR / PRODUCE_DIVISOR)),
		sizeof(productionInfo));
	assert(aircraftTemplate->tech);
	Q_strcat(productionInfo, va(_("Production time\t%ih\n"), aircraftTemplate->tech->produceTime), sizeof(productionInfo));
	MN_RegisterText(TEXT_PRODUCTION_INFO, productionInfo);
	Cvar_Set("mn_item", aircraftTemplate->id);
}

/**
 * @brief Prints information about the selected item in production.
 * @param[in] base Pointer to the base where informations should be printed.
 * @sa PR_AircraftInfo
 * @sa PR_ItemProductionInfo
 * @sa PR_DisassemblyInfo
 */
static void PR_ProductionInfo (const base_t *base)
{
	if (selectedProduction) {
		production_t *prod = selectedProduction;
		MN_ExecuteConfunc("prod_taskselected");
		if (prod->aircraft) {
			PR_AircraftInfo(prod->aircraft);
		} else if (prod->production) {
			PR_ItemProductionInfo(base, prod->item, prod->percentDone);
		} else {
			PR_DisassemblyInfo(base, prod->item, CL_GetComponentsByItem(prod->item), prod->percentDone);
		}
		Cvar_SetValue("mn_production_amount", selectedProduction->amount);
	} else {
		if (!(selectedAircraft || selectedItem || selectedDisassembly)) {
			MN_ExecuteConfunc("prod_nothingselected");
			if (produceCategory == FILTER_AIRCRAFT)
				MN_RegisterText(TEXT_PRODUCTION_INFO, _("No aircraft selected."));
			else
				MN_RegisterText(TEXT_PRODUCTION_INFO, _("No item selected"));
			Cvar_Set("mn_item", "");
		} else {
			MN_ExecuteConfunc("prod_availableselected");
			if (selectedAircraft) {
				PR_AircraftInfo(selectedAircraft);
			} else if (selectedItem) {
				PR_ItemProductionInfo(base, selectedItem, 0.0);
			} else if (selectedDisassembly) {
				PR_DisassemblyInfo(base, selectedDisassembly->assemblyItem, selectedDisassembly, 0.0);
			}
		}
	}
}

/**
 * @brief Click function for production list
 * @note Opens the UFOpaedia - by right clicking an item
 */
static void PR_ProductionListRightClick_f (void)
{
	int num;
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();

	/* can be called from everywhere without a base set */
	if (!base)
		return;

	queue = &ccs.productions[base->idx];

	/* not enough parameters */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* clicked which item? */
	num = atoi(Cmd_Argv(1));

	/* Clicked the production queue or the item list? */
	if (num < queue->numItems && num >= 0) {
		selectedProduction = &queue->items[num];
		if (selectedProduction->aircraft) {
			assert(selectedProduction->aircraft->tech);
			UP_OpenWith(selectedProduction->aircraft->tech->id);
		} else {
			const objDef_t *od = selectedProduction->item;
			assert(od->tech);
			UP_OpenWith(od->tech->id);
		}
	} else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* Clicked in the item list. */
		const int idx = num - queue->numItems - QUEUE_SPACERS;

		if (produceCategory == FILTER_AIRCRAFT) {
			const aircraft_t *aircraftTemplate = (aircraft_t*)LIST_GetByIdx(productionItemList, idx);
			/* aircraftTemplate may be empty if rclicked below real entry.
			 * UFO research definition must not have a tech assigned,
			 * only RS_CRAFT types have */
			if (aircraftTemplate && aircraftTemplate->tech)
				UP_OpenWith(aircraftTemplate->tech->id);
		} else if (produceCategory == FILTER_DISASSEMBLY) {
			components_t *comp = (components_t*)LIST_GetByIdx(productionItemList, idx);
			if (comp && comp->assemblyItem && comp->assemblyItem->tech) {
				UP_OpenWith(comp->assemblyItem->tech->id);
			}
		} else {
			objDef_t *od = (objDef_t*)LIST_GetByIdx(productionItemList, idx);
#ifdef DEBUG
			if (!od) {
				Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListRightClick_f: No item found at the list-position %i!\n", idx);
				return;
			}

			if (!od->tech)
				Sys_Error("PR_ProductionListRightClick_f: No tech pointer for object '%s'\n", od->id);
#endif

			/* Open up UFOpaedia for this entry. */
			if (RS_IsResearched_ptr(od->tech) && INV_ItemMatchesFilter(od, produceCategory)) {
				PR_ClearSelected();
				selectedItem = od;
				UP_OpenWith(od->tech->id);
				return;
			}
		}
	}
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListRightClick_f: Click on spacer %i\n", num);
#endif
}

/**
 * @brief Click function for production list.
 * @note "num" is the entry in the visible production list (includes queued entries and spaces).
 * @todo left click on spacer should either delete current selection or do nothing, not update visible selection but show old info
 */
static void PR_ProductionListClick_f (void)
{
	int num;
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();

	/* can be called from everywhere without a base set */
	if (!base)
		return;

	queue = &ccs.productions[base->idx];

	/* Break if there are not enough parameters. */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* Clicked which item? */
	num = atoi(Cmd_Argv(1));

	/* Clicked the production queue or the item list? */
	if (num < queue->numItems && num >= 0) {
		selectedProduction = &queue->items[num];
		PR_ProductionInfo(base);
	} else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* Clicked in the item list. */
		const int idx = num - queue->numItems - QUEUE_SPACERS;

		if (produceCategory == FILTER_DISASSEMBLY) {
			components_t *comp = (components_t*)LIST_GetByIdx(productionItemList, idx);

			PR_ClearSelected();
			selectedDisassembly = comp;

			PR_ProductionInfo(base);
		} else if (produceCategory == FILTER_AIRCRAFT) {
			aircraft_t *aircraftTemplate = (aircraft_t*)LIST_GetByIdx(productionItemList, idx);
			if (!aircraftTemplate) {
				Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListClick_f: No item found at the list-position %i!\n", idx);
				return;
			}
			/* ufo research definition must not have a tech assigned
			 * only RS_CRAFT types have
			 * @sa RS_InitTree */
			if (aircraftTemplate->tech && aircraftTemplate->tech->produceTime >= 0
			 && RS_IsResearched_ptr(aircraftTemplate->tech)) {
				PR_ClearSelected();
				selectedAircraft = aircraftTemplate;
				PR_ProductionInfo(base);
			}
		} else {
			objDef_t *od = (objDef_t*)LIST_GetByIdx(productionItemList, idx);
			if (!od) {
				Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListClick_f: No item found at the list-position %i!\n", idx);
				return;
			}

			if (!od->tech)
				Com_Error(ERR_DROP, "PR_ProductionListClick_f: No tech pointer for object '%s'", od->id);
			/* We can only produce items that fulfill the following conditions... */
			if (RS_IsResearched_ptr(od->tech) && od->tech->produceTime >= 0			/* Item is producible */
			 && INV_ItemMatchesFilter(od, produceCategory)) {	/* Item is in the current inventory-category */
				assert(od->name[0] != '\0');

				PR_ClearSelected();
				selectedItem = od;
				PR_ProductionInfo(base);
			}
		}
	}
}

/**
 * @brief Will select a new tab on the production list.
 */
static void PR_ProductionType_f (void)
{
	int cat;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <category>\n", Cmd_Argv(0));
		return;
	}

	cat = INV_GetFilterTypeID(Cmd_Argv(1));

	/* Check if the given category index is valid. */
	if (cat < 0 || cat >= MAX_FILTERTYPES)
		cat = FILTER_S_PRIMARY;

	/* Can be called from everywhere without a base set */
	if (!base)
		return;

	produceCategory = cat;
	Cvar_Set("mn_itemtype", INV_GetFilterType(produceCategory));

	/* Update list of entries for current production tab. */
	PR_UpdateProductionList(base);

	/* Reset selected entry, if it was not from the queue */
	selectedItem = NULL;
	selectedDisassembly = NULL;
	selectedAircraft = NULL;

	/* Select first entry in the list (if any). */
	if (LIST_Count(productionItemList) > 0) {
		if (produceCategory == FILTER_AIRCRAFT)
			selectedAircraft = (aircraft_t*)LIST_GetByIdx(productionItemList, 0);
		else if (produceCategory == FILTER_DISASSEMBLY)
			selectedDisassembly = (components_t*)LIST_GetByIdx(productionItemList, 0);
		else
			selectedItem = (objDef_t*)LIST_GetByIdx(productionItemList, 0);
	}
	/* update selection index if first entry of actual list was chosen */
	if (!selectedProduction)
		MN_ExecuteConfunc("prod_selectline %i", ccs.productions[base->idx].numItems + QUEUE_SPACERS);

	/* Update displayed info about selected entry (if any). */
	PR_ProductionInfo(base);
}

/**
 * @brief Will fill the list of producible items.
 * @note Some of Production Menu related cvars are being set here.
 */
static void PR_ProductionList_f (void)
{
	char tmpbuf[MAX_VAR];
	int numWorkshops;
	base_t *base = B_GetCurrentSelectedBase();

	/* can be called from everywhere without a started game */
	if (!base)
		return;

	numWorkshops = max(0, B_GetNumberOfBuildingsInBaseByBuildingType(base, B_WORKSHOP));

	Cvar_SetValue("mn_production_limit", MAX_PRODUCTIONS_PER_WORKSHOP * numWorkshops);
	Cvar_SetValue("mn_production_basecap", base->capacities[CAP_WORKSPACE].max);

	/* Set amount of workers - all/ready to work (determined by base capacity. */
	PR_UpdateProductionCap(base);

	Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i",
		base->capacities[CAP_WORKSPACE].cur,
		E_CountHired(base, EMPL_WORKER));
	Cvar_Set("mn_production_workers", tmpbuf);

	Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i",
		base->capacities[CAP_ITEMS].cur,
		base->capacities[CAP_ITEMS].max);
	Cvar_Set("mn_production_storage", tmpbuf);

	PR_ClearSelected();
}

/**
 * @brief Increases the production amount by given parameter.
 */
static void PR_ProductionIncrease_f (void)
{
	int amount = 1, amountTemp = 0;
	production_queue_t *queue;
	production_t *prod;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (!(selectedProduction || selectedAircraft || selectedItem || selectedDisassembly))
		return;

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

	queue = &ccs.productions[base->idx];

	if (selectedProduction) {
		prod = selectedProduction;
		if (prod->production) {		/* Production. */
			if (prod->aircraft) {
				/* Don't allow to queue more aircraft if there is no free space. */
				if (AIR_CalculateHangarStorage(prod->aircraft, base, 0) <= 0) {
					MN_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo free space in hangars.\n"));
					return;
				}
			}

			/* Check if we can add more items. */
			if (prod->amount + amount > MAX_PRODUCTION_AMOUNT) {
				if (MAX_PRODUCTION_AMOUNT - prod->amount >= 0) {
					/* Add as many items as allowed. */
					prod->amount = MAX_PRODUCTION_AMOUNT;
				} else {
					return;
				}
			} else {
				prod->amount += amount;
			}
		} else {	/* Disassembling. */
			/* We can disassembly only as many items as we have in base storage. */
			if (base->storage.num[prod->item->idx] > amount)
				amountTemp = amount;
			else
				amountTemp = base->storage.num[prod->item->idx];
			Com_DPrintf(DEBUG_CLIENT, "PR_ProductionIncrease_f: amounts: storage: %i, param: %i, temp: %i\n", base->storage.num[prod->item->idx], amount, amountTemp);

			/* Now check if we can add more items and
			 * remove the amount we just added to queue from base storage. */
			if (prod->amount + amountTemp > MAX_PRODUCTION_AMOUNT) {
				if (MAX_PRODUCTION_AMOUNT - prod->amount >= 0) {
					/* Calculate the maximum allowed amount to be added. */
					amountTemp = MAX_PRODUCTION_AMOUNT - prod->amount;
				} else {
					return;
				}
			}

			base->storage.num[prod->item->idx] -= amountTemp;
			prod->amount += amountTemp;
		}
	} else {
		if (!selectedDisassembly) {
			if (selectedAircraft && AIR_CalculateHangarStorage(selectedAircraft, base, 0) <= 0) {
				MN_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo free space in hangars.\n"));
				return;
			}
			prod = PR_QueueNew(base, queue, selectedItem, selectedAircraft, amount, qfalse);	/* Production. (only of of the "selected" pointer can be nonNULL) */
		} else {
			/* We can disassemble only as many items as we have in base storage. */
			assert(selectedDisassembly && selectedDisassembly->assemblyItem);

			if (base->storage.num[selectedDisassembly->assemblyItem->idx] > amount)
				amountTemp = amount;
			else
				amountTemp = base->storage.num[selectedDisassembly->assemblyItem->idx];

			/* Check if we can add more items. */
			if (amountTemp > MAX_PRODUCTION_AMOUNT) {
				/* Add as many items as possible. */
				amountTemp = MAX_PRODUCTION_AMOUNT;
			}

			prod = PR_QueueNew(base, queue, selectedDisassembly->assemblyItem, NULL, amountTemp, qtrue);	/* Disassembling. */
		}

		/** prod is NULL when queue limit is reached
		 * @todo this popup hides any previous popup, like popup created in PR_QueueNew */
		if (!prod) {
			/* Oops! Too many items! */
			MN_Popup(_("Queue full!"), _("You cannot queue any more items!"));
			return;
		} else
			MN_ExecuteConfunc("prod_selectline %i", prod->idx);

		if (produceCategory != FILTER_AIRCRAFT) {
			/* Get technology of the item in the selected queue-entry. */
			const objDef_t *od = prod->item;
			int producibleAmount = amount;
			if (od->tech)
				producibleAmount = PR_RequirementsMet(amount, &od->tech->requireForProduction, base);

			if (producibleAmount > 0) {	/* Check if production requirements have been (even partially) met. */
				if (od->tech) {
					/* Remove the additionally required items (multiplied by 'producibleAmount') from base-storage.*/
					PR_UpdateRequiredItemsInBasestorage(base, -amount, &od->tech->requireForProduction);
					prod->itemsCached = qtrue;
				}

				if (producibleAmount < amount) {
					/** @todo make the numbers work here. */
					MN_Popup(_("Not enough material!"), va(_("You don't have enough material to produce all (%i) items. Production will continue with a reduced (%i) number."),
						amount, producibleAmount));
				}

				if (!selectedDisassembly) {
					Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Production of %s started"), _(od->name));
					MSO_CheckAddNewMessage(NT_PRODUCTION_STARTED, _("Production started"), cp_messageBuffer, qfalse, MSG_PRODUCTION, od->tech);
				} else {
					Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Disassembling of %s started"), _(od->name));
					MSO_CheckAddNewMessage(NT_PRODUCTION_STARTED, _("Production started"), cp_messageBuffer, qfalse, MSG_PRODUCTION, od->tech);
				}

				/* Now we select the item we just created. */
				PR_ClearSelected();
				selectedProduction = &queue->items[queue->numItems - 1];
			} else { /* requirements are not met => producibleAmount <= 0 */
				/** @todo better messages needed */
				MN_Popup(_("Not enough material!"), _("You don't have enough of the needed material to produce this item."));
				/** @todo
				 *  -) need to popup something like: "You need the following items in order to produce more of ITEM:   x of ITEM, x of ITEM, etc..."
				 *     This info should also be displayed in the item-info.
				 *  -) can can (if possible) change the 'amount' to a vlalue that _can_ be produced (i.e. the maximum amount possible).*/
			}
		} else {
			const aircraft_t *aircraftTemplate = prod->aircraft;
			assert(aircraftTemplate);
			assert(aircraftTemplate == aircraftTemplate->tpl);

			Com_DPrintf(DEBUG_CLIENT, "Increasing production for '%s'\n", aircraftTemplate->id);
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Production of %s started"), _(aircraftTemplate->name));
			MSO_CheckAddNewMessage(NT_PRODUCTION_STARTED, _("Production started"), cp_messageBuffer, qfalse, MSG_PRODUCTION, NULL);
			/* Now we select the item we just created. */
			PR_ClearSelected();
			selectedProduction = &queue->items[queue->numItems - 1];
		}
	}

	PR_ProductionInfo(base);
	PR_UpdateProductionList(base);
}

/**
 * @brief Stops the current running production
 */
static void PR_ProductionStop_f (void)
{
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base || !selectedProduction)
		return;

	queue = &ccs.productions[base->idx];

	PR_QueueDelete(base, queue, selectedProduction->idx);

	if (queue->numItems == 0) {
		selectedProduction = NULL;
		MN_ExecuteConfunc("prod_selectline -1");
	} else if (selectedProduction->idx >= queue->numItems) {
		selectedProduction = &queue->items[queue->numItems - 1];
		MN_ExecuteConfunc("prod_selectline %i", selectedProduction->idx);
	}

	PR_ProductionInfo(base);
	PR_UpdateProductionList(base);
}

/**
 * @brief Decrease the production amount by given parameter
 */
static void PR_ProductionDecrease_f (void)
{
	int amount = 1, amountTemp = 0;
	production_queue_t *queue;
	production_t *prod;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

	if (!base || !selectedProduction)
		return;

	queue = &ccs.productions[base->idx];
	prod = selectedProduction;
	if (prod->amount >= amount)
		amountTemp = amount;
	else
		amountTemp = prod->amount;

	prod->amount -= amountTemp;
	/* We need to readd items being disassembled to base storage. */
	if (!prod->production)
		base->storage.num[prod->item->idx] += amountTemp;

	if (prod->amount <= 0) {
		PR_ProductionStop_f();
	} else {
		PR_ProductionInfo(base);
		PR_UpdateProductionList(base);
	}
}

/**
 * @brief Change the production amount by given diff.
 */
static void PR_ProductionChange_f (void)
{
	int amount;

	if (!(selectedProduction || selectedAircraft || selectedItem || selectedDisassembly))
		return;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <diff> : change the production amount\n", Cmd_Argv(0));
		return;
	}

	amount = atoi(Cmd_Argv(1));
	if (amount > 0) {
		Cbuf_AddText(va("prod_inc %i\n", amount));
	} else {
		Cbuf_AddText(va("prod_dec %i\n", -amount));
	}
}

/**
 * @brief shift the current production up the list
 */
static void PR_ProductionUp_f (void)
{
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base || !selectedProduction)
		return;

	/* first position already */
	if (selectedProduction->idx == 0)
		return;

	queue = &ccs.productions[base->idx];
	PR_QueueMove(queue, selectedProduction->idx, -1);

	selectedProduction = &queue->items[selectedProduction->idx - 1];
	MN_ExecuteConfunc("prod_selectline %i", selectedProduction->idx);
	PR_UpdateProductionList(base);
}

/**
 * @brief shift the current production down the list
 */
static void PR_ProductionDown_f (void)
{
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base || !selectedProduction)
		return;

	queue = &ccs.productions[base->idx];

	if (selectedProduction->idx >= queue->numItems - 1)
		return;

	PR_QueueMove(queue, selectedProduction->idx, 1);

	selectedProduction = &queue->items[selectedProduction->idx + 1];
	MN_ExecuteConfunc("prod_selectline %i", selectedProduction->idx);
	PR_UpdateProductionList(base);
}

void PR_InitCallbacks (void)
{
	Cmd_AddCommand("prod_init", PR_ProductionList_f, NULL);
	Cmd_AddCommand("prod_type", PR_ProductionType_f, NULL);
	Cmd_AddCommand("prod_up", PR_ProductionUp_f, "Move production item up in the queue");
	Cmd_AddCommand("prod_down", PR_ProductionDown_f, "Move production item down in the queue");
	Cmd_AddCommand("prod_change", PR_ProductionChange_f, "Change production amount");
	Cmd_AddCommand("prod_inc", PR_ProductionIncrease_f, "Increase production amount");
	Cmd_AddCommand("prod_dec", PR_ProductionDecrease_f, "Decrease production amount");
	Cmd_AddCommand("prod_stop", PR_ProductionStop_f, "Stop production");
	Cmd_AddCommand("prodlist_rclick", PR_ProductionListRightClick_f, NULL);
	Cmd_AddCommand("prodlist_click", PR_ProductionListClick_f, NULL);
}

void PR_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("prod_init");
	Cmd_RemoveCommand("prod_type");
	Cmd_RemoveCommand("prod_up");
	Cmd_RemoveCommand("prod_down");
	Cmd_RemoveCommand("prod_change");
	Cmd_RemoveCommand("prod_inc");
	Cmd_RemoveCommand("prod_dec");
	Cmd_RemoveCommand("prod_stop");
	Cmd_RemoveCommand("prodlist_rclick");
	Cmd_RemoveCommand("prodlist_click");
}
