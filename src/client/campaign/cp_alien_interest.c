/**
 * @file cp_alien_interest.c
 * @brief Alien interest values influence the campaign actions
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

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../client.h"
#include "cp_campaign.h"
#include "cp_alien_interest.h"

/**
 * @brief Typical value of the overall alien interest at the end of the game.
 */
static const int FINAL_OVERALL_INTEREST = 1000;

/**
 * @brief Initialize alien interest values and mission cycle
 * @note Should be used when a new single player campaign starts
 * @sa CP_CampaignInit
 */
void CL_ResetAlienInterest (void)
{
	int i;

	ccs.lastInterestIncreaseDelay = 0;
	ccs.lastMissionSpawnedDelay = 0;
	ccs.overallInterest = 20;

	for (i = 0; i < INTERESTCATEGORY_MAX; i++)
		ccs.interest[i] = 0;
	ccs.interest[INTERESTCATEGORY_NONE] = 6;
	ccs.interest[INTERESTCATEGORY_RECON] = 20;
}

/**
 * @brief Change individual interest value
 * @param[in] percentage Maybe pe > 0 or < 0
 * @param[in] category Category of interest for aliens that should increase / decrease
 * @note Should be used when a mission is over (success or failure)
 */
void CL_ChangeIndividualInterest (float percentage, interestCategory_t category)
{
	const float nonOccurrencePercent = 0.3f;		/**< Non occurence probability */

	if (category == INTERESTCATEGORY_MAX) {
		Com_Printf("CL_ChangeIndividualInterest: Unsupported value of category\n");
		return;
	}

	if (percentage > 0.0f) {
		const int gain = (int) (percentage * ccs.overallInterest);
		const int diff = ccs.overallInterest - ccs.interest[category];
		const float slowerIncreaseFraction = 0.5f; /**< Fraction of individual interest that will be won if
									individal interest becomes higher than overall interest. 0 means no increase */
		/* Value increases: percentage is taken from the overall interest value
			But it increase slower if the individual interest becomes higher than the overall interest value */
		if (diff > gain)
			/* Final value of individual interest is below overall interest */
			ccs.interest[category] += gain;
		else if (diff > 0)
			/* Initial value of individual interest is below overall interest */
			ccs.interest[category] = ccs.overallInterest + (int) (slowerIncreaseFraction * (gain - diff));
		else
			/* Final value of individual interest is above overall interest */
			ccs.interest[category] += (int) (slowerIncreaseFraction * gain);
	} else {
		/* Value decreases: percentage is taken from the individual interest value */
		ccs.interest[category] += (int) (percentage * ccs.interest[category]);
		if (ccs.interest[category] < 0) {
			/* this may be reached if percentage is below -1 */
			ccs.interest[category] = 0;
		}
	}

	/* Set new non-occurence value. The aim is to have roughly a steady number
	 * of none occurrence during the game, in order to include some randomness.
	 * If overall alien interest becomes higher than FINAL_OVERALL_INTEREST,
	 * then the none occurence goes to zero, to make more pressure on the player */
	if (ccs.overallInterest < FINAL_OVERALL_INTEREST)
		ccs.interest[INTERESTCATEGORY_NONE] = (int) (nonOccurrencePercent * ccs.overallInterest);
	else
		ccs.interest[INTERESTCATEGORY_NONE] = (int) (nonOccurrencePercent * FINAL_OVERALL_INTEREST * exp((ccs.overallInterest - FINAL_OVERALL_INTEREST) / 30));
}

/**
 * @brief Increase alien overall interest.
 * @sa CL_CampaignRun
 * @note hourly called
 */
void CP_IncreaseAlienInterest (void)
{
	const int delayBetweenIncrease = 28 - ccs.curCampaign->difficulty;	/**< Number of hours between 2 overall interest increase */

	ccs.lastInterestIncreaseDelay++;

	if (ccs.lastInterestIncreaseDelay > delayBetweenIncrease) {
		ccs.overallInterest++;
		ccs.lastInterestIncreaseDelay %= delayBetweenIncrease;
	}
}
