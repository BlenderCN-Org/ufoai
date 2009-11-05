#include "iufoscript.h"
#include "iradiant.h"
#include "modulesystem.h"
#include "modulesystem/moduleregistry.h"
#include "modulesystem/singletonmodule.h"
#include "os/path.h"
#include "terrain/Terrain.h"
#include "mapdef/MapDef.h"

#include <string>
#include <set>

namespace
{
	class UFOScriptCollector
	{
		private:

			std::set<std::string>& _list;

		public:
			typedef const char* first_argument_type;

			UFOScriptCollector (std::set<std::string>& list) :
				_list(list)
			{
				// don't parse the menus
				GlobalFileSystem().forEachFile("ufos/", "*", makeCallback1(*this), 0);
				std::size_t size = _list.size();
				globalOutputStream() << "Found " << size << " ufo files\n";
			}

			// Functor operator needed for the forEachFile() call
			void operator() (const char* file)
			{
				std::string rawPath(file);
				std::string extension = os::getExtension(rawPath);

				if (extension == "ufo") {
					_list.insert(rawPath);
				}
			}
	};
}

UFOScriptSystem::UFOScriptSystem ()
{
}

void UFOScriptSystem::editTerrainDefinition ()
{
	scripts::Terrain terrain;
	terrain.showTerrainDefinitionForTexture();
}

void UFOScriptSystem::editMapDefinition ()
{
	scripts::MapDef mapDef;
	mapDef.showMapDefinition();
}

const std::string UFOScriptSystem::getUFOScriptDir () const
{
	return "ufos/";
}

void UFOScriptSystem::init ()
{
	UFOScriptCollector collector(_ufoFiles);
}

class UFOScriptSystemAPI
{
	private:

		UFOScriptSystem* _ufoScriptSystem;

	public:
		typedef UFOScriptSystem Type;
		STRING_CONSTANT(Name, "*");

		UFOScriptSystemAPI () :
			_ufoScriptSystem(0)
		{
			_ufoScriptSystem = new UFOScriptSystem();
		}
		~UFOScriptSystemAPI ()
		{
			delete _ufoScriptSystem;
		}

		UFOScriptSystem* getTable ()
		{
			return _ufoScriptSystem;
		}
};

typedef SingletonModule<UFOScriptSystemAPI> UFOScriptSystemModule;
typedef Static<UFOScriptSystemModule> StaticUFOScriptSystemModule;
StaticRegisterModule staticRegisterUFOScript(StaticUFOScriptSystemModule::instance());

void EditTerrainDefinition ()
{
	GlobalUFOScriptSystem()->editTerrainDefinition();
}

void EditMapDefinition ()
{
	GlobalUFOScriptSystem()->editMapDefinition();
}

void UFOScript_Construct ()
{
	GlobalRadiant().commandInsert("EditTerrainDefinition", FreeCaller<EditTerrainDefinition> (), accelerator_null());
	GlobalRadiant().commandInsert("EditMapDefinition", FreeCaller<EditMapDefinition> (), accelerator_null());
	GlobalUFOScriptSystem()->init();
}

void UFOScript_Destroy ()
{
}
