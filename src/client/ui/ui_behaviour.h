/**
 * @file ui_behaviour.h
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CLIENT_UI_UI_BEHAVIOUR_H
#define CLIENT_UI_UI_BEHAVIOUR_H

#include "ui_nodes.h"
#include "node/ui_node_abstractnode.h"

struct value_s;
struct uiBehaviour_s;
struct uiNode_s;

/**
 * @brief node behaviour, how a node work
 * @sa virtualFunctions
 */
typedef struct uiBehaviour_s {
	/* behaviour attributes */
	const char* name;				/**< name of the behaviour: string type of a node */
	const char* extends;			/**< name of the extends behaviour */
	uiNode* manager;				/**< manager of the behaviour */
	qboolean registration;			/**< True if we can define the behavior with registration function */
	qboolean isVirtual;				/**< true, if the node doesn't have any position on the screen */
	qboolean isFunction;			/**< true, if the node is a function */
	qboolean isAbstract;			/**< true, if we can't instantiate the behaviour */
	qboolean isInitialized;			/**< cache if we already have initialized the node behaviour */
	qboolean focusEnabled;			/**< true if the node can win the focus (should be use when type TAB) */
	qboolean drawItselfChild;		/**< if true, the node draw function must draw child, the core code will not do it */

	const value_t** localProperties;	/**< list of properties of the node */
	int propertyCount;				/**< number of the properties into the propertiesList. Cache value to speedup search */
	intptr_t extraDataSize;			/**< Size of the extra data used (it come from "u" attribute) @note use intptr_t because we use the virtual inheritance function (see virtualFunctions) */
	struct uiBehaviour_s *super;	/**< link to the extended node */
#ifdef DEBUG
	int count;						/**< number of node allocated */
#endif
} uiBehaviour_t;

/**
 * @brief Signature of a function to bind a node method.
 */
typedef void (*uiNodeMethod_t)(struct uiNode_s* node, const struct uiCallContext_s *context);

/**
 * @brief Register a property to a behaviour.
 * It should not be used in the code
 * @param behaviour Target behaviour
 * @param name Name of the property
 * @param type Type of the property
 * @param pos position of the attribute (which store property memory) into the node structure
 * @param size size of the attribute (which store property memory) into the node structure
 * @see UI_RegisterNodeProperty
 * @see UI_RegisterExtradataNodeProperty
 * @return A link to the node property
 */
const struct value_s *UI_RegisterNodePropertyPosSize_(struct uiBehaviour_s *behaviour, const char* name, int type, size_t pos, size_t size);

/**
 * @brief Initialize a property
 * @param BEHAVIOUR behaviour Target behaviour
 * @param NAME Name of the property
 * @param TYPE Type of the property
 * @param OBJECTTYPE Object type containing the property
 * @param ATTRIBUTE Name of the attribute of the object containing data of the property
 */
#define UI_RegisterNodeProperty(BEHAVIOUR, NAME, TYPE, OBJECTTYPE, ATTRIBUTE) UI_RegisterNodePropertyPosSize_(BEHAVIOUR, NAME, TYPE, offsetof(OBJECTTYPE, ATTRIBUTE), MEMBER_SIZEOF(OBJECTTYPE, ATTRIBUTE))

/**
 * @brief Return the offset of an extradata node attribute
 * @param TYPE Extradata type
 * @param MEMBER Attribute name
 * @sa offsetof
 */
#define UI_EXTRADATA_OFFSETOF_(TYPE, MEMBER) ((size_t) &((TYPE *)(UI_EXTRADATA_POINTER(0, TYPE)))->MEMBER)

/**
 * @brief Initialize a property from extradata of node
 * @param BEHAVIOUR behaviour Target behaviour
 * @param NAME Name of the property
 * @param TYPE Type of the property
 * @param EXTRADATATYPE Object type containing the property
 * @param ATTRIBUTE Name of the attribute of the object containing data of the property
 */
#define UI_RegisterExtradataNodeProperty(BEHAVIOUR, NAME, TYPE, EXTRADATATYPE, ATTRIBUTE) UI_RegisterNodePropertyPosSize_(BEHAVIOUR, NAME, TYPE, UI_EXTRADATA_OFFSETOF_(EXTRADATATYPE, ATTRIBUTE), MEMBER_SIZEOF(EXTRADATATYPE, ATTRIBUTE))

/**
 * @brief Initialize a property which override an inherited property.
 * It is yet only used for the documentation.
 * @param BEHAVIOUR behaviour Target behaviour
 * @param NAME Name of the property
 */
#define UI_RegisterOveridedNodeProperty(BEHAVIOUR, NAME) ;

/**
 * @brief Register a node method to a behaviour.
 * @param behaviour Target behaviour
 * @param name Name of the property
 * @param function function to execute the node method
 * @return A link to the node property
 */
const struct value_s *UI_RegisterNodeMethod(struct uiBehaviour_s *behaviour, const char* name, uiNodeMethod_t function);

/**
 * @brief Return a property from a node behaviour
 * @return A property, else NULL if not found.
 */
const struct value_s *UI_GetPropertyFromBehaviour(const uiBehaviour_t *behaviour, const char* name) __attribute__ ((warn_unused_result));

/**
 * @brief Initialize a node behaviour memory, after registration, and before unsing it.
 * @param behaviour Behaviour to initialize
 */
void UI_InitializeNodeBehaviour(uiBehaviour_t* behaviour);

#endif
