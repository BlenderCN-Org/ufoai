/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "ifilter.h"
#include "scenelib.h"
#include "iregistry.h"
#include "iscenegraph.h"

#include "XMLFilter.h"

#include <list>
#include <set>

#include "gtkutil/widget.h"
#include "gtkutil/menu.h"
#include "../gtkmisc.h"
#include "../mainframe.h"
#include "../settings/preferences.h"

#include "radiant_i18n.h"

/** FilterSystem implementation class.
 */
class BasicFilterSystem
	: public FilterSystem
{
public:
	typedef FilterSystem Type;
	STRING_CONSTANT(Name, "*");

	FilterSystem* getTable ()
	{
		return this;
	}

private:

	// Flag to indicate initialisation status
	bool _initialised;

	// Hashtable of available filters, indexed by name
	typedef std::map<std::string, filters::XMLFilter> FilterTable;
	FilterTable _availableFilters;

	// Second table containing just the active filters
	FilterTable _activeFilters;

	// Cache of visibility flags for item names, to avoid having to
	// traverse the active filter list for each lookup
	typedef std::map<std::string, bool> StringFlagCache;
	StringFlagCache _visibilityCache;

private:

	// Initialise the filter system. This must be done after the main
	// Radiant module, hence cannot be done in the constructor.
	void initialise() {

		// Ask the XML Registry for the filter nodes
		xml::NodeList filters = GlobalRegistry().findXPath("game/filtersystem//filter");

		// Iterate over the list of nodes, adding filter objects onto the list
		for (xml::NodeList::iterator iter = filters.begin();
			 iter != filters.end();
			 ++iter)
		{
			// Initialise the XMLFilter object
			std::string filterName = iter->getAttributeValue("name");
			filters::XMLFilter filter(filterName);

			// Get all of the filterCriterion children of this node
			xml::NodeList critNodes = iter->getNamedChildren("filterCriterion");

			// Create XMLFilterRule objects for each criterion
			for (xml::NodeList::iterator critIter = critNodes.begin();
				 critIter != critNodes.end();
				 ++critIter)
			{
				filter.addRule(critIter->getAttributeValue("type"),
						critIter->getAttributeValue("match"),
						critIter->getAttributeValue("action") == "show");
			}

			// Add this XMLFilter to the list of available filters
			_availableFilters.insert(FilterTable::value_type(filterName, filter));
		}
	}

public:

	// Constructor
	BasicFilterSystem()
	: _initialised(false)
	{}

	// Filter system visit function
	void forEachFilter(IFilterVisitor& visitor) {
		// Initialise the filter system if not already
		if (!_initialised)
			initialise();

		// Visit each filter on the list, passing the name to the visitor
		for (FilterTable::iterator iter = _availableFilters.begin();
			iter != _availableFilters.end();
			++iter)
		{
			visitor.visit(iter->first);
		}
	}

	// Set the state of a filter
	void setFilterState(const std::string& filter, bool state) {
		if (state) {
			// Copy the filter to the active filters list
			_activeFilters.insert(
					FilterTable::value_type(
					filter, _availableFilters.find(filter)->second));
		}
		else {
			// Remove filter from active filters list
			_activeFilters.erase(filter);
		}

		// Invalidate the visibility cache to force new values to be
		// loaded from the filters themselves
		_visibilityCache.clear();

		// Trigger an immediate scene redraw
		GlobalSceneGraph().sceneChanged();
	}

	// Query whether a item is visible or filtered out
	bool isVisible(const std::string& item, const std::string& name) {

		// Check if this texture is in the texture flag cache, returning
		// its cached value if found
		StringFlagCache::iterator cacheIter = _visibilityCache.find(name);
		if (cacheIter != _visibilityCache.end())
			return cacheIter->second;

		// Otherwise, walk the list of active filters to find a value for
		// this item.
		bool visFlag = true; // default if no filters modify it

		for (FilterTable::iterator activeIter = _activeFilters.begin();
			 activeIter != _activeFilters.end();
			 ++activeIter)
		{
			// Delegate the check to the filter object. If a filter returns
			// false for the visibility check, then the item is filtered
			// and we don't need any more checks.
			if (!activeIter->second.isVisible(item, name)) {
				visFlag = false;
				break;
			}
		}

		// Cache the result and return to caller
		_visibilityCache.insert(StringFlagCache::value_type(name, visFlag));
		return visFlag;
	}

	bool isVisible(const std::string& item, int flags) {
		return isVisible(item, string::toString(flags));
	}
};

#include "preferencesystem.h"
#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

typedef SingletonModule<BasicFilterSystem> FilterModule;
typedef Static<FilterModule> StaticFilterModule;
StaticRegisterModule staticRegisterFilter(StaticFilterModule::instance());
