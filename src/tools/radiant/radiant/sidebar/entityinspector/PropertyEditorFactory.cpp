#include "PropertyEditorFactory.h"

#include "Vector3PropertyEditor.h"
#include "BooleanPropertyEditor.h"
#include "EntityPropertyEditor.h"
#ifdef USE_CLASSNAME_PROPERTY_EDITOR
#include "ClassnamePropertyEditor.h"
#endif
#include "ColourPropertyEditor.h"

#include "gtkutil/image.h"

namespace ui {

// Initialisation

PropertyEditorFactory::PropertyEditorMap PropertyEditorFactory::_peMap;

// Register the classes

void PropertyEditorFactory::registerClasses ()
{
	_peMap["vector3"] = new Vector3PropertyEditor();
	_peMap["boolean"] = new BooleanPropertyEditor();
	_peMap["entity"] = new EntityPropertyEditor();
	_peMap["colour"] = new ColourPropertyEditor();
#ifdef USE_CLASSNAME_PROPERTY_EDITOR
	_peMap["classname"] = new ClassnamePropertyEditor();
#endif
}

// Create a PropertyEditor from the given name.

PropertyEditor* PropertyEditorFactory::create (const std::string& className, Entity* entity, const std::string& key,
		const std::string& options)
{
	// Register the PropertyEditors if the map is empty
	if (_peMap.empty()) {
		registerClasses();
	}

#ifdef ALL_PROPERTY_EDITORS_ARE_TEXT
	PropertyEditorMap::iterator iter(_peMap.find("text"));
#else
	PropertyEditorMap::iterator iter(_peMap.find(className));
#endif

	if (iter == _peMap.end()) {
		return NULL;
	} else {
		PropertyEditor* pe = iter->second->createNew(entity, key, options);
		pe->refresh();
		return pe;
	}
}

// Return a GdkPixbuf containing the icon for the given property type

GdkPixbuf* PropertyEditorFactory::getPixbufFor (std::string type)
{
	std::string iconName(std::string("icon_") + type + ".png");
	return gtkutil::getLocalPixbuf(iconName);
}

}
