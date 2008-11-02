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

#include "plugin.h"

#include "autoptr.h"
#include "iscriplib.h"
#include "ibrush.h"
#include "ifiletypes.h"
#include "ieclass.h"
#include "qerplugin.h"

#include "scenelib.h"
#include "string/string.h"
#include "stringio.h"
#include "generic/constant.h"

#include "modulesystem/singletonmodule.h"

#include "parse.h"
#include "write.h"

class MapDependencies :
			public GlobalRadiantModuleRef,
			public GlobalBrushModuleRef,
			public GlobalFiletypesModuleRef,
			public GlobalScripLibModuleRef,
			public GlobalEntityClassManagerModuleRef,
			public GlobalSceneGraphModuleRef {
public:
	MapDependencies() :
			GlobalBrushModuleRef("ufo"),
			GlobalEntityClassManagerModuleRef("ufo") {
	}
};

class MapUFOAPI : public TypeSystemRef, public MapFormat, public PrimitiveParser {
public:
	typedef MapFormat Type;
	STRING_CONSTANT(Name, "mapufo");

	MapUFOAPI() {
		GlobalFiletypesModule::getTable().addType(Type::Name(), Name(), filetype_t("ufo maps", "*.map"));
		GlobalFiletypesModule::getTable().addType(Type::Name(), Name(), filetype_t("ufo region", "*.reg"));
	}
	MapFormat* getTable() {
		return this;
	}
	scene::Node& parsePrimitive(Tokeniser& tokeniser) const {
		const char* primitive = tokeniser.getToken();
		if (primitive != 0) {
			if (string_equal(primitive, "(")) {
				tokeniser.ungetToken(); // (
				return GlobalBrushModule::getTable().createBrush();
			}
		}

		Tokeniser_unexpectedError(tokeniser, primitive, "#ufo-primitive");
		return g_nullNode;
	}
	void readGraph(scene::Node& root, TextInputStream& inputStream, EntityCreator& entityTable) const {
		AutoPtr<Tokeniser> tokeniser(GlobalScripLibModule::getTable().m_pfnNewSimpleTokeniser(inputStream));
		Map_Read(root, *tokeniser, entityTable, *this);
	}
	void writeGraph(scene::Node& root, GraphTraversalFunc traverse, TextOutputStream& outputStream) const {
		AutoPtr<TokenWriter> writer(GlobalScripLibModule::getTable().m_pfnNewSimpleTokenWriter(outputStream));
		Map_Write(root, traverse, *writer);
	}
};

typedef SingletonModule<MapUFOAPI, MapDependencies> MapUFOModule;

MapUFOModule g_MapUFOModule;

extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules(ModuleServer& server) {
	initialiseModule(server);

	g_MapUFOModule.selfRegister();
}
