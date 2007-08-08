/**
 * @file cvar.h
 * @brief Cvar header file
 *
 * cvar_t variables are used to hold scalar or string variables that can be changed or displayed at the console or prog code as well as accessed directly
 * in C code.
 * The user can access cvars from the console in three ways:
 * r_draworder			prints the current value
 * r_draworder 0		sets the current value to 0
 * set r_draworder 0	as above, but creates the cvar if not present
 * Cvars are restricted from having the same names as commands to keep this
 * interface from being ambiguous.
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

/**
 * @brief creates the variable if it doesn't exist, or returns the existing one
 * if it exists, the value will not be changed, but flags will be ORed in
 * that allows variables to be unarchived without needing bitflags
 */
cvar_t *Cvar_Get(const char *var_name, const char *value, int flags, const char* desc);

/**
 * @brief will create the variable if it doesn't exist
 */
cvar_t *Cvar_Set(const char *var_name, const char *value);

/**
 * @brief will set the variable even if NOSET or LATCH
 */
cvar_t *Cvar_ForceSet(const char *var_name, const char *value);

/**
 * @brief
 */
cvar_t *Cvar_FullSet(const char *var_name, const char *value, int flags);

/**
 * @brief expands value to a string and calls Cvar_Set
 */
void Cvar_SetValue(const char *var_name, float value);

/**
 * @brief returns 0 if not defined or non numeric
 */
int Cvar_VariableInteger(const char *var_name);

/**
 * @brief returns 0.0 if not defined or non numeric
 */
float Cvar_VariableValue(const char *var_name);

/**
 * @brief returns an empty string if not defined
 */
const char *Cvar_VariableString(const char *var_name);

/**
 * @brief returns an empty string if not defined
 */
const char *Cvar_VariableStringOld(const char *var_name);

/**
 * @brief attempts to match a partial variable name for command line completion
 * returns NULL if nothing fits
 */
int Cvar_CompleteVariable(const char *partial, const char **match);

/**
 * @brief any CVAR_LATCHED variables that have been set will now take effect
 */
void Cvar_UpdateLatchedVars(void);

/**
 * @brief called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
 * command.  Returns true if the command was a variable reference that
 * was handled. (print or change)
 */
qboolean Cvar_Command(void);

/**
 * @brief appends lines containing "set variable value" for all variables
 * with the archive flag set to true.
 */
void Cvar_WriteVariables(const char *path);

/**
 * @brief
 */
void Cvar_Init(void);

/**
 * @brief returns an info string containing all the CVAR_USERINFO cvars
 */
char *Cvar_Userinfo(void);

/**
 * @brief returns an info string containing all the CVAR_SERVERINFO cvars
 */
char *Cvar_Serverinfo(void);

/**
 * @brief this is set each time a CVAR_USERINFO variable is changed
 * so that the client knows to send it to the server
 */
extern qboolean userinfo_modified;

/**
 * @brief this function checks cvar ranges and integral values
 */
qboolean Cvar_AssertValue(cvar_t * cvar, float minVal, float maxVal, qboolean shouldBeIntegral);

/**
 * @brief this function checks whether the cvar string is a valid string in char ** array
 */
qboolean Cvar_AssertString(cvar_t * cvar, char **array, int arraySize);

/**
 * @brief Sets the check functions for a cvar (e.g. Cvar_Assert)
 */
qboolean Cvar_SetCheckFunction(const char *var_name, qboolean (*check) (cvar_t* cvar));

/**
 * @brief Reset cheat cvar values to default
 */
void Cvar_FixCheatVars(void);

/**
 * @brief Function to remove the cvar and free the space
 */
qboolean Cvar_Delete(const char *var_name);
