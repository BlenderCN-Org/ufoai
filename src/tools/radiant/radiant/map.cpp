/**
 * @file map.cpp
 */

/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

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

#include "map.h"
#include "radiant.h"

#include "debugging/debugging.h"

#include "imap.h"
#include "iselection.h"
#include "iundo.h"
#include "ibrush.h"
#include "ifilter.h"
#include "ireference.h"
#include "ifiletypes.h"
#include "ieclass.h"
#include "irender.h"
#include "ientity.h"
#include "editable.h"
#include "ifilesystem.h"
#include "namespace.h"
#include "moduleobserver.h"

#include <set>

#include "scenelib.h"
#include "transformlib.h"
#include "selectionlib.h"
#include "instancelib.h"
#include "traverselib.h"
#include "maplib.h"
#include "eclasslib.h"
#include "stream/textfilestream.h"
#include "os/path.h"
#include "uniquenames.h"
#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"
#include "stream/stringstream.h"
#include "signal/signal.h"

#include "gtkutil/filechooser.h"
#include "timer.h"
#include "select.h"
#include "plugin.h"
#include "filetypes.h"
#include "sidebar/mapinfo.h"
#include "qe3.h"
#include "camwindow.h"
#include "xywindow.h"
#include "mainframe.h"
#include "preferences.h"
#include "referencecache.h"
#include "lastused.h"
#include "commands.h"
#include "autosave.h"

class NameObserver
{
		UniqueNames& m_names;
		CopiedString m_name;

		void construct (void)
		{
			if (!empty()) {
				//globalOutputStream() << "construct " << makeQuoted(c_str()) << "\n";
				m_names.insert(name_read(c_str()));
			}
		}
		void destroy (void)
		{
			if (!empty()) {
				//globalOutputStream() << "destroy " << makeQuoted(c_str()) << "\n";
				m_names.erase(name_read(c_str()));
			}
		}

		NameObserver& operator= (const NameObserver& other);
	public:
		NameObserver (UniqueNames& names) :
			m_names(names)
		{
			construct();
		}
		NameObserver (const NameObserver& other) :
			m_names(other.m_names), m_name(other.m_name)
		{
			construct();
		}
		~NameObserver (void)
		{
			destroy();
		}
		bool empty () const
		{
			return string_empty(c_str());
		}
		const char* c_str () const
		{
			return m_name.c_str();
		}
		void nameChanged (const char* name)
		{
			destroy();
			m_name = name;
			construct();
		}
		typedef MemberCaller1<NameObserver, const char*, &NameObserver::nameChanged> NameChangedCaller;
};

class BasicNamespace: public Namespace
{
		typedef std::map<NameCallback, NameObserver> Names;
		Names m_names;
		UniqueNames m_uniqueNames;
	public:
		~BasicNamespace (void)
		{
			ASSERT_MESSAGE(m_names.empty(), "namespace: names still registered at shutdown");
		}
		void attach (const NameCallback& setName, const NameCallbackCallback& attachObserver)
		{
			std::pair<Names::iterator, bool> result = m_names.insert(Names::value_type(setName, m_uniqueNames));
			ASSERT_MESSAGE(result.second, "cannot attach name");
			attachObserver(NameObserver::NameChangedCaller((*result.first).second));
			//globalOutputStream() << "attach: " << reinterpret_cast<const unsigned int&>(setName) << "\n";
		}
		void detach (const NameCallback& setName, const NameCallbackCallback& detachObserver)
		{
			Names::iterator i = m_names.find(setName);
			ASSERT_MESSAGE(i != m_names.end(), "cannot detach name");
			//globalOutputStream() << "detach: " << reinterpret_cast<const unsigned int&>(setName) << "\n";
			detachObserver(NameObserver::NameChangedCaller((*i).second));
			m_names.erase(i);
		}

		void makeUnique (const char* name, const NameCallback& setName) const
		{
			char buffer[1024];
			name_write(buffer, m_uniqueNames.make_unique(name_read(name)));
			setName(buffer);
		}

		void mergeNames (const BasicNamespace& other) const
		{
			typedef std::list<NameCallback> SetNameCallbacks;
			typedef std::map<CopiedString, SetNameCallbacks> NameGroups;
			NameGroups groups;

			UniqueNames uniqueNames(other.m_uniqueNames);

			for (Names::const_iterator i = m_names.begin(); i != m_names.end(); ++i) {
				groups[(*i).second.c_str()].push_back((*i).first);
			}

			for (NameGroups::iterator i = groups.begin(); i != groups.end(); ++i) {
				name_t uniqueName(uniqueNames.make_unique(name_read((*i).first.c_str())));
				uniqueNames.insert(uniqueName);

				char buffer[1024];
				name_write(buffer, uniqueName);

				//globalOutputStream() << "renaming " << makeQuoted((*i).first.c_str()) << " to " << makeQuoted(buffer) << "\n";

				SetNameCallbacks& setNameCallbacks = (*i).second;

				for (SetNameCallbacks::const_iterator j = setNameCallbacks.begin(); j != setNameCallbacks.end(); ++j) {
					(*j)(buffer);
				}
			}
		}
};

BasicNamespace g_defaultNamespace;
BasicNamespace g_cloneNamespace;

class NamespaceAPI
{
		Namespace* m_namespace;
	public:
		typedef Namespace Type;
		STRING_CONSTANT(Name, "*")
		;

		NamespaceAPI (void)
		{
			m_namespace = &g_defaultNamespace;
		}
		Namespace* getTable (void)
		{
			return m_namespace;
		}
};

typedef SingletonModule<NamespaceAPI> NamespaceModule;
typedef Static<NamespaceModule> StaticNamespaceModule;
StaticRegisterModule staticRegisterDefaultNamespace (StaticNamespaceModule::instance ());

std::list<Namespaced*> g_cloned;

inline Namespaced* Node_getNamespaced (scene::Node& node)
{
	return NodeTypeCast<Namespaced>::cast(node);
}

void Node_gatherNamespaced (scene::Node& node)
{
	Namespaced* namespaced = Node_getNamespaced(node);
	if (namespaced != 0) {
		g_cloned.push_back(namespaced);
	}
}

class GatherNamespaced: public scene::Traversable::Walker
{
	public:
		bool pre (scene::Node& node) const
		{
			Node_gatherNamespaced(node);
			return true;
		}
};

void Map_gatherNamespaced (scene::Node& root)
{
	Node_traverseSubgraph(root, GatherNamespaced());
}

void Map_mergeClonedNames (void)
{
	for (std::list<Namespaced*>::const_iterator i = g_cloned.begin(); i != g_cloned.end(); ++i) {
		(*i)->setNamespace(g_cloneNamespace);
	}
	g_cloneNamespace.mergeNames(g_defaultNamespace);
	for (std::list<Namespaced*>::const_iterator i = g_cloned.begin(); i != g_cloned.end(); ++i) {
		(*i)->setNamespace(g_defaultNamespace);
	}

	g_cloned.clear();
}

class WorldNode
{
		scene::Node* m_node;
	public:
		WorldNode () :
			m_node(0)
		{
		}
		void set (scene::Node* node)
		{
			if (m_node != 0)
				m_node->DecRef();
			m_node = node;
			if (m_node != 0)
				m_node->IncRef();
		}
		scene::Node* get () const
		{
			return m_node;
		}
};

class Map;
void Map_SetValid (Map& map, bool valid);
void Map_UpdateTitle (const Map& map);
void Map_SetWorldspawn (Map& map, scene::Node* node);

class Map: public ModuleObserver
{
	public:
		CopiedString m_name;
		Resource* m_resource;
		bool m_valid;

		bool m_modified;
		void (*m_modified_changed) (const Map&);

		Signal0 m_mapValidCallbacks;

		WorldNode m_world_node; // "classname" "worldspawn" !

		Map () :
			m_resource(0), m_valid(false), m_modified_changed(Map_UpdateTitle)
		{
		}

		void realise (void)
		{
			if (m_resource != 0) {
				if (Map_Unnamed(*this)) {
					g_map.m_resource->setNode(NewMapRoot("").get_pointer());
					MapFile* map = Node_getMapFile(*g_map.m_resource->getNode());
					if (map != 0) {
						map->save();
					}
				} else {
					m_resource->load();
				}

				GlobalSceneGraph().insert_root(*m_resource->getNode());

				AutoSave_clear();

				Map_SetValid(g_map, true);
			}
		}
		void unrealise (void)
		{
			if (m_resource != 0) {
				Map_SetValid(g_map, false);
				Map_SetWorldspawn(g_map, 0);

				GlobalUndoSystem().clear();

				GlobalSceneGraph().erase_root();
			}
		}
};

Map g_map;
Map* g_currentMap = 0;

void Map_addValidCallback (Map& map, const SignalHandler& handler)
{
	map.m_mapValidCallbacks.connectLast(handler);
}

bool Map_Valid (const Map& map)
{
	return map.m_valid;
}

void Map_SetValid (Map& map, bool valid)
{
	map.m_valid = valid;
	map.m_mapValidCallbacks();
}

const char* Map_Name (const Map& map)
{
	return map.m_name.c_str();
}

bool Map_Unnamed (const Map& map)
{
	return string_equal(Map_Name(map), "unnamed.map");
}

inline const MapFormat& MapFormat_forFile (const char* filename)
{
	const char* moduleName = findModuleName(GetFileTypeRegistry(), MapFormat::Name(), path_get_extension(filename));
	MapFormat* format = Radiant_getMapModules().findModule(moduleName);
	ASSERT_MESSAGE(format != 0, "map format not found for file " << makeQuoted(filename));
	return *format;
}

const MapFormat& Map_getFormat (const Map& map)
{
	return MapFormat_forFile(Map_Name(map));
}

bool Map_Modified (const Map& map)
{
	return map.m_modified;
}

void Map_SetModified (Map& map, bool modified)
{
	if (map.m_modified ^ modified) {
		map.m_modified = modified;

		map.m_modified_changed(map);
	}
}

/**
 * @sa Sys_SetTitle
 */
void Map_UpdateTitle (const Map& map)
{
	Sys_SetTitle(map.m_name.c_str(), Map_Modified(map));
}

scene::Node* Map_GetWorldspawn (const Map& map)
{
	return map.m_world_node.get();
}

void Map_SetWorldspawn (Map& map, scene::Node* node)
{
	map.m_world_node.set(node);
}

// TTimo
// need that in a variable, will have to tweak depending on the game
float g_MaxWorldCoord = 64 * 1024;
float g_MinWorldCoord = -64 * 1024;

static void AddRegionBrushes (void);
static void RemoveRegionBrushes (void);

/*
 ================
 Map_Free
 free all map elements, reinitialize the structures that depend on them
 ================
 */
void Map_Free (void)
{
	g_map.m_resource->detach(g_map);
	GlobalReferenceCache().release(g_map.m_name.c_str());
	g_map.m_resource = 0;

	FlushReferences();

	g_currentMap = 0;
}

class EntityFindByClassname: public scene::Graph::Walker
{
		const char* m_name;
		Entity*& m_entity;
	public:
		EntityFindByClassname (const char* name, Entity*& entity) :
			m_name(name), m_entity(entity)
		{
			m_entity = 0;
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (m_entity == 0) {
				Entity* entity = Node_getEntity(path.top());
				if (entity != 0 && string_equal(m_name, entity->getKeyValue("classname"))) {
					m_entity = entity;
				}
			}
			return true;
		}
};

Entity* Scene_FindEntityByClass (const char* name)
{
	Entity* entity;
	GlobalSceneGraph().traverse(EntityFindByClassname(name, entity));
	return entity;
}

Entity *Scene_FindPlayerStart (void)
{
	typedef const char* StaticString;
	StaticString strings[] = { "info_player_start", "info_player_deathmatch", "team_CTF_redplayer",
			"team_CTF_blueplayer", "team_CTF_redspawn", "team_CTF_bluespawn", };
	typedef const StaticString* StaticStringIterator;
	for (StaticStringIterator i = strings, end = strings + (sizeof(strings) / sizeof(StaticString)); i != end; ++i) {
		Entity* entity = Scene_FindEntityByClass(*i);
		if (entity != 0) {
			return entity;
		}
	}
	return 0;
}

/**
 * @brief move the view to a given position
 * @todo Use at mapstart
 */
void FocusViews (const Vector3& point, float angle)
{
	ASSERT_NOTNULL(g_pParentWnd);
	CamWnd& camwnd = *g_pParentWnd->GetCamWnd();
	Camera_setOrigin(camwnd, point);
	Vector3 angles(Camera_getAngles(camwnd));
	angles[CAMERA_PITCH] = 0;
	angles[CAMERA_YAW] = angle;
	Camera_setAngles(camwnd, angles);

	XYWnd* xywnd = g_pParentWnd->GetXYWnd();
	xywnd->SetOrigin(point);
}

#include "stringio.h"

inline bool node_is_worldspawn (scene::Node& node)
{
	Entity* entity = Node_getEntity(node);
	return entity != 0 && string_equal(entity->getKeyValue("classname"), "worldspawn");
}

// use first worldspawn
class entity_updateworldspawn: public scene::Traversable::Walker
{
	public:
		bool pre (scene::Node& node) const
		{
			if (node_is_worldspawn(node)) {
				if (Map_GetWorldspawn(g_map) == 0) {
					Map_SetWorldspawn(g_map, &node);
				}
			}
			return false;
		}
};

scene::Node* Map_FindWorldspawn (Map& map)
{
	Map_SetWorldspawn(map, 0);

	Node_getTraversable(GlobalSceneGraph().root())->traverse(entity_updateworldspawn());

	return Map_GetWorldspawn(map);
}

class CollectAllWalker: public scene::Traversable::Walker
{
		scene::Node& m_root;
		UnsortedNodeSet& m_nodes;
	public:
		CollectAllWalker (scene::Node& root, UnsortedNodeSet& nodes) :
			m_root(root), m_nodes(nodes)
		{
		}
		bool pre (scene::Node& node) const
		{
			m_nodes.insert(NodeSmartReference(node));
			Node_getTraversable(m_root)->erase(node);
			return false;
		}
};

void Node_insertChildFirst (scene::Node& parent, scene::Node& child)
{
	UnsortedNodeSet nodes;
	Node_getTraversable(parent)->traverse(CollectAllWalker(parent, nodes));
	Node_getTraversable(parent)->insert(child);

	for (UnsortedNodeSet::iterator i = nodes.begin(); i != nodes.end(); ++i) {
		Node_getTraversable(parent)->insert((*i));
	}
}

scene::Node& createWorldspawn (void)
{
	NodeSmartReference worldspawn(GlobalEntityCreator().createEntity(GlobalEntityClassManager().findOrInsert(
			"worldspawn", true)));
	Node_insertChildFirst(GlobalSceneGraph().root(), worldspawn);
	return worldspawn;
}

void Map_UpdateWorldspawn (Map& map)
{
	if (Map_FindWorldspawn(map) == 0) {
		Map_SetWorldspawn(map, &createWorldspawn());
	}
}

scene::Node& Map_FindOrInsertWorldspawn (Map& map)
{
	Map_UpdateWorldspawn(map);
	return *Map_GetWorldspawn(map);
}

class MapMergeAll: public scene::Traversable::Walker
{
		mutable scene::Path m_path;
	public:
		MapMergeAll (const scene::Path& root) :
			m_path(root)
		{
		}
		bool pre (scene::Node& node) const
		{
			Node_getTraversable(m_path.top())->insert(node);
			m_path.push(makeReference(node));
			selectPath(m_path, true);
			return false;
		}
		void post (scene::Node& node) const
		{
			m_path.pop();
		}
};

class MapMergeEntities: public scene::Traversable::Walker
{
		mutable scene::Path m_path;
	public:
		MapMergeEntities (const scene::Path& root) :
			m_path(root)
		{
		}
		bool pre (scene::Node& node) const
		{
			if (node_is_worldspawn(node)) {
				scene::Node* world_node = Map_FindWorldspawn(g_map);
				if (world_node == 0) {
					Map_SetWorldspawn(g_map, &node);
					Node_getTraversable(m_path.top().get())->insert(node);
					m_path.push(makeReference(node));
					Node_getTraversable(node)->traverse(SelectChildren(m_path));
				} else {
					m_path.push(makeReference(*world_node));
					Node_getTraversable(node)->traverse(MapMergeAll(m_path));
				}
			} else {
				Node_getTraversable(m_path.top())->insert(node);
				m_path.push(makeReference(node));
				if (node_is_group(node)) {
					Node_getTraversable(node)->traverse(SelectChildren(m_path));
				} else {
					selectPath(m_path, true);
				}
			}
			return false;
		}
		void post (scene::Node& node) const
		{
			m_path.pop();
		}
};

class BasicContainer: public scene::Node::Symbiot
{
		class TypeCasts
		{
				NodeTypeCastTable m_casts;
			public:
				TypeCasts (void)
				{
					NodeContainedCast<BasicContainer, scene::Traversable>::install(m_casts);
				}
				NodeTypeCastTable& get (void)
				{
					return m_casts;
				}
		};

		scene::Node m_node;
		TraversableNodeSet m_traverse;
	public:

		typedef LazyStatic<TypeCasts> StaticTypeCasts;

		scene::Traversable& get (NullType<scene::Traversable> )
		{
			return m_traverse;
		}

		BasicContainer () :
			m_node(this, this, StaticTypeCasts::instance().get())
		{
		}

		scene::Node& node (void)
		{
			return m_node;
		}
};

/// Merges the map graph rooted at \p node into the global scene-graph.
void MergeMap (scene::Node& node)
{
	Node_getTraversable(node)->traverse(MapMergeEntities(scene::Path(makeReference(GlobalSceneGraph().root()))));
}
void Map_ImportSelected (TextInputStream& in, const MapFormat& format)
{
	NodeSmartReference node((new BasicContainer)->node());
	format.readGraph(node, in, GlobalEntityCreator());
	Map_gatherNamespaced(node);
	Map_mergeClonedNames();
	MergeMap(node);
}

inline scene::Cloneable* Node_getCloneable (scene::Node& node)
{
	return NodeTypeCast<scene::Cloneable>::cast(node);
}

inline scene::Node& node_clone (scene::Node& node)
{
	scene::Cloneable* cloneable = Node_getCloneable(node);
	if (cloneable != 0) {
		return cloneable->clone();
	}

	return (new scene::NullNode)->node();
}

class CloneAll: public scene::Traversable::Walker
{
		mutable scene::Path m_path;
	public:
		CloneAll (scene::Node& root) :
			m_path(makeReference(root))
		{
		}
		bool pre (scene::Node& node) const
		{
			if (node.isRoot()) {
				return false;
			}

			m_path.push(makeReference(node_clone(node)));
			m_path.top().get().IncRef();

			return true;
		}
		void post (scene::Node& node) const
		{
			if (node.isRoot()) {
				return;
			}

			Node_getTraversable(m_path.parent())->insert(m_path.top());

			m_path.top().get().DecRef();
			m_path.pop();
		}
};

scene::Node& Node_Clone (scene::Node& node)
{
	scene::Node& clone = node_clone(node);
	scene::Traversable* traversable = Node_getTraversable(node);
	if (traversable != 0) {
		traversable->traverse(CloneAll(clone));
	}
	return clone;
}

class ScopeTimer
{
		Timer m_timer;
		const char* m_message;
	public:
		ScopeTimer (const char* message) :
			m_message(message)
		{
			m_timer.start();
		}
		~ScopeTimer (void)
		{
			double elapsed_time = m_timer.elapsed_msec() / 1000.f;
			g_message("%s timer: %f second(s) elapsed\n", m_message, elapsed_time);
		}
};

/**
 * @brief Loads a map file
 * @param[in] filename The filename of the map to load
 * @note The file must be checked for existence and readability already
 */
void Map_LoadFile (const char *filename)
{
	g_message("Loading map from %s\n", filename);
	ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Loading Map"));

	g_map.m_name = filename;
	Map_UpdateTitle(g_map);

	{
		g_map.m_resource = GlobalReferenceCache().capture(g_map.m_name.c_str());
		g_map.m_resource->attach(g_map);

		Node_getTraversable(GlobalSceneGraph().root())->traverse(entity_updateworldspawn());
	}

	globalOutputStream() << "--- LoadMapFile ---\n";
	globalOutputStream() << g_map.m_name.c_str() << "\n";

	globalOutputStream() << makeLeftJustified(Unsigned(g_brushCount.get()), 5) << " primitive\n";
	globalOutputStream() << makeLeftJustified(Unsigned(g_entityCount.get()), 5) << " entities\n";

	//GlobalEntityCreator().printStatistics();

			// move the view to a start position
			FocusViews(g_vector3_identity, 0);

			g_currentMap = &g_map;

			MapInfo_Update();
		}

void Map_Reload (void)
{
	if (!Map_Name(g_map))
		return;

	/* reload the map */
	Map_RegionOff();
	Map_Free();
	Map_LoadFile(Map_Name(g_map));
}

class Excluder
{
	public:
		virtual ~Excluder ()
		{
		}
		virtual bool excluded (scene::Node& node) const = 0;
};

class ExcludeWalker: public scene::Traversable::Walker
{
		const scene::Traversable::Walker& m_walker;
		const Excluder* m_exclude;
		mutable bool m_skip;
	public:
		ExcludeWalker (const scene::Traversable::Walker& walker, const Excluder& exclude) :
			m_walker(walker), m_exclude(&exclude), m_skip(false)
		{
		}
		bool pre (scene::Node& node) const
		{
			if (m_exclude->excluded(node) || node.isRoot()) {
				m_skip = true;
				return false;
			} else {
				m_walker.pre(node);
			}
			return true;
		}
		void post (scene::Node& node) const
		{
			if (m_skip) {
				m_skip = false;
			} else {
				m_walker.post(node);
			}
		}
};

class AnyInstanceSelected: public scene::Instantiable::Visitor
{
		bool& m_selected;
	public:
		AnyInstanceSelected (bool& selected) :
			m_selected(selected)
		{
			m_selected = false;
		}
		void visit (scene::Instance& instance) const
		{
			Selectable* selectable = Instance_getSelectable(instance);
			if (selectable != 0 && selectable->isSelected()) {
				m_selected = true;
			}
		}
};

bool Node_instanceSelected (scene::Node& node)
{
	scene::Instantiable* instantiable = Node_getInstantiable(node);
	ASSERT_NOTNULL(instantiable);
	bool selected;
	instantiable->forEachInstance(AnyInstanceSelected(selected));
	return selected;
}

class SelectedDescendantWalker: public scene::Traversable::Walker
{
		bool& m_selected;
	public:
		SelectedDescendantWalker (bool& selected) :
			m_selected(selected)
		{
			m_selected = false;
		}

		bool pre (scene::Node& node) const
		{
			if (node.isRoot()) {
				return false;
			}

			if (Node_instanceSelected(node)) {
				m_selected = true;
			}

			return true;
		}
};

static bool Node_selectedDescendant (scene::Node& node)
{
	bool selected;
	Node_traverseSubgraph(node, SelectedDescendantWalker(selected));
	return selected;
}

class SelectionExcluder: public Excluder
{
	public:
		bool excluded (scene::Node& node) const
		{
			return !Node_selectedDescendant(node);
		}
};

class IncludeSelectedWalker: public scene::Traversable::Walker
{
		const scene::Traversable::Walker& m_walker;
		mutable std::size_t m_selected;
		mutable bool m_skip;

		bool selectedParent () const
		{
			return m_selected != 0;
		}
	public:
		IncludeSelectedWalker (const scene::Traversable::Walker& walker) :
			m_walker(walker), m_selected(0), m_skip(false)
		{
		}
		bool pre (scene::Node& node) const
		{
			// include node if:
			// node is not a 'root' AND ( node is selected OR any child of node is selected OR any parent of node is selected )
			if (!node.isRoot() && (Node_selectedDescendant(node) || selectedParent())) {
				if (Node_instanceSelected(node)) {
					++m_selected;
				}
				m_walker.pre(node);
				return true;
			} else {
				m_skip = true;
				return false;
			}
		}
		void post (scene::Node& node) const
		{
			if (m_skip) {
				m_skip = false;
			} else {
				if (Node_instanceSelected(node)) {
					--m_selected;
				}
				m_walker.post(node);
			}
		}
};

void Map_Traverse_Selected (scene::Node& root, const scene::Traversable::Walker& walker)
{
	scene::Traversable* traversable = Node_getTraversable(root);
	if (traversable != 0) {
		traversable->traverse(IncludeSelectedWalker(walker));
	}
}

void Map_ExportSelected (TextOutputStream& out, const MapFormat& format)
{
	format.writeGraph(GlobalSceneGraph().root(), Map_Traverse_Selected, out);
}

void Map_Traverse (scene::Node& root, const scene::Traversable::Walker& walker)
{
	scene::Traversable* traversable = Node_getTraversable(root);
	if (traversable != 0) {
		traversable->traverse(walker);
	}
}

class RegionExcluder: public Excluder
{
	public:
		bool excluded (scene::Node& node) const
		{
			return node.excluded();
		}
};

void Map_Traverse_Region (scene::Node& root, const scene::Traversable::Walker& walker)
{
	scene::Traversable* traversable = Node_getTraversable(root);
	if (traversable != 0) {
		traversable->traverse(ExcludeWalker(walker, RegionExcluder()));
	}
}

bool Map_SaveRegion (const char *filename)
{
	AddRegionBrushes();

	const bool success = MapResource_saveFile(MapFormat_forFile(filename), GlobalSceneGraph().root(),
			Map_Traverse_Region, filename);

	RemoveRegionBrushes();

	return success;
}

void Map_RenameAbsolute (const char* absolute)
{
	Resource* resource = GlobalReferenceCache().capture(absolute);
	NodeSmartReference clone(NewMapRoot(path_make_relative(absolute, GlobalFileSystem().findRoot(absolute))));
	resource->setNode(clone.get_pointer());

	{
		//ScopeTimer timer("clone subgraph");
		Node_getTraversable(GlobalSceneGraph().root())->traverse(CloneAll(clone));
	}

	g_map.m_resource->detach(g_map);
	GlobalReferenceCache().release(g_map.m_name.c_str());

	g_map.m_resource = resource;

	g_map.m_name = absolute;
	Map_UpdateTitle(g_map);

	g_map.m_resource->attach(g_map);
}

void Map_Rename (const char* filename)
{
	if (!string_equal(g_map.m_name.c_str(), filename)) {
		ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Saving Map"));

		Map_RenameAbsolute(filename);

		SceneChangeNotify();
	} else {
		SaveReferences();
	}
}

bool Map_Save (void)
{
	ScopeTimer timer("map save");
	SaveReferences();
	return true; // assume success..
}

/*
 ===========
 Map_New

 ===========
 */
void Map_New (void)
{
	//globalOutputStream() << "Map_New\n";

	g_map.m_name = "unnamed.map";
	Map_UpdateTitle(g_map);

	{
		g_map.m_resource = GlobalReferenceCache().capture(g_map.m_name.c_str());
		//    ASSERT_MESSAGE(g_map.m_resource->getNode() == 0, "bleh");
		g_map.m_resource->attach(g_map);

		SceneChangeNotify();
	}

	FocusViews(g_vector3_identity, 0);

	g_currentMap = &g_map;
}

extern void ConstructRegionBrushes (scene::Node* brushes[6], const Vector3& region_mins, const Vector3& region_maxs);

/*
 ===========================================================
 REGION
 ===========================================================
 */
static bool region_active;
Vector3 region_mins (g_MinWorldCoord, g_MinWorldCoord, g_MinWorldCoord);
Vector3 region_maxs (g_MaxWorldCoord, g_MaxWorldCoord, g_MaxWorldCoord);

static scene::Node* region_sides[6];
static scene::Node* region_startpoint = 0;

/**
 * @brief a regioned map will have temp walls put up at the region boundary
 * @todo old implementation of region brushes
 * we still add them straight in the worldspawn and take them out after the map is saved
 * with the new implementation we should be able to append them in a temporary manner
 * to the data we pass to the map module
 */
static void AddRegionBrushes (void)
{
	int i;

	for (i = 0; i < 6; i++) {
		region_sides[i] = &GlobalBrushCreator().createBrush();
		Node_getTraversable(Map_FindOrInsertWorldspawn(g_map))->insert(NodeSmartReference(*region_sides[i]));
	}

	region_startpoint = &GlobalEntityCreator().createEntity(GlobalEntityClassManager().findOrInsert(
			"info_player_start", false));

	ConstructRegionBrushes(region_sides, region_mins, region_maxs);

	Node_getTraversable(GlobalSceneGraph().root())->insert(NodeSmartReference(*region_startpoint));
}

static void RemoveRegionBrushes (void)
{
	for (std::size_t i = 0; i < 6; i++) {
		Node_getTraversable(*Map_GetWorldspawn(g_map))->erase(*region_sides[i]);
	}
	Node_getTraversable(GlobalSceneGraph().root())->erase(*region_startpoint);
}

static inline void exclude_node (scene::Node& node, bool exclude)
{
	if (exclude)
		node.enable(scene::Node::eExcluded);
	else
		node.disable(scene::Node::eExcluded);
}

class ExcludeAllWalker: public scene::Graph::Walker
{
		bool m_exclude;
	public:
		ExcludeAllWalker (bool exclude) :
			m_exclude(exclude)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			exclude_node(path.top(), m_exclude);

			return true;
		}
};

void Scene_Exclude_All (bool exclude)
{
	GlobalSceneGraph().traverse(ExcludeAllWalker(exclude));
}

bool Instance_isSelected (const scene::Instance& instance)
{
	const Selectable* selectable = Instance_getSelectable(instance);
	return selectable != 0 && selectable->isSelected();
}

class ExcludeSelectedWalker: public scene::Graph::Walker
{
		bool m_exclude;
	public:
		ExcludeSelectedWalker (bool exclude) :
			m_exclude(exclude)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			exclude_node(path.top(), (instance.isSelected() || instance.childSelected() || instance.parentSelected())
					== m_exclude);
			return true;
		}
};

void Scene_Exclude_Selected (bool exclude)
{
	GlobalSceneGraph().traverse(ExcludeSelectedWalker(exclude));
}

class ExcludeRegionedWalker: public scene::Graph::Walker
{
		bool m_exclude;
	public:
		ExcludeRegionedWalker (bool exclude) :
			m_exclude(exclude)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			exclude_node(path.top(), !((aabb_intersects_aabb(instance.worldAABB(), aabb_for_minmax(region_mins,
					region_maxs)) != 0) ^ m_exclude));

			return true;
		}
};

void Scene_Exclude_Region (bool exclude)
{
	GlobalSceneGraph().traverse(ExcludeRegionedWalker(exclude));
}

/**
 * @note Other filtering options may still be on
 */
void Map_RegionOff (void)
{
	region_active = false;

	region_maxs[0] = g_MaxWorldCoord - 64;
	region_mins[0] = g_MinWorldCoord + 64;
	region_maxs[1] = g_MaxWorldCoord - 64;
	region_mins[1] = g_MinWorldCoord + 64;
	region_maxs[2] = g_MaxWorldCoord - 64;
	region_mins[2] = g_MinWorldCoord + 64;

	Scene_Exclude_All(false);
}

void Map_ApplyRegion (void)
{
	region_active = true;

	Scene_Exclude_Region(false);
}

void Map_RegionSelectedBrushes (void)
{
	Map_RegionOff();

	if (GlobalSelectionSystem().countSelected() != 0 && GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive) {
		region_active = true;
		Select_GetBounds(region_mins, region_maxs);

		Scene_Exclude_Selected(false);

		GlobalSelectionSystem().setSelectedAll(false);
	}
}

void Map_RegionXY (float x_min, float y_min, float x_max, float y_max)
{
	Map_RegionOff();

	region_mins[0] = x_min;
	region_maxs[0] = x_max;
	region_mins[1] = y_min;
	region_maxs[1] = y_max;
	region_mins[2] = g_MinWorldCoord + 64;
	region_maxs[2] = g_MaxWorldCoord - 64;

	Map_ApplyRegion();
}

void Map_RegionBounds (const AABB& bounds)
{
	Map_RegionOff();

	region_mins = vector3_subtracted(bounds.origin, bounds.extents);
	region_maxs = vector3_added(bounds.origin, bounds.extents);

	deleteSelection();

	Map_ApplyRegion();
}

void Map_RegionBrush (void)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
		Map_RegionBounds(instance.worldAABB());
	}
}

bool Map_ImportFile (const char* filename)
{
	ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Loading Map"));

	bool success = false;
	{
		Resource* resource = GlobalReferenceCache().capture(filename);
		resource->refresh(); /* avoid loading old version if map has changed on disk since last import */
		if (resource->load()) {
			NodeSmartReference clone(NewMapRoot(""));
			Node_getTraversable(*resource->getNode())->traverse(CloneAll(clone));

			Map_gatherNamespaced(clone);
			Map_mergeClonedNames();
			MergeMap(clone);
			success = true;
		}
		GlobalReferenceCache().release(filename);
	}

	SceneChangeNotify();

	return success;
}

bool Map_SaveFile (const char* filename)
{
	ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Saving Map"));
	return MapResource_saveFile(MapFormat_forFile(filename), GlobalSceneGraph().root(), Map_Traverse, filename);
}

	/**
	 * @brief Saves selected world brushes and whole entities with partial/full selections
	 */
bool Map_SaveSelected (const char* filename)
{
	return MapResource_saveFile(MapFormat_forFile(filename), GlobalSceneGraph().root(), Map_Traverse_Selected, filename);
}

class ParentSelectedBrushesToEntityWalker: public scene::Graph::Walker
{
		scene::Node& m_parent;
	public:
		ParentSelectedBrushesToEntityWalker (scene::Node& parent) :
			m_parent(parent)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get_pointer() != &m_parent && Node_isPrimitive(path.top())) {
				Selectable* selectable = Instance_getSelectable(instance);
				if (selectable != 0 && selectable->isSelected() && path.size() > 1) {
					return false;
				}
			}
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get_pointer() != &m_parent && Node_isPrimitive(path.top())) {
				Selectable* selectable = Instance_getSelectable(instance);
				if (selectable != 0 && selectable->isSelected() && path.size() > 1) {
					scene::Node& parent = path.parent();
					if (&parent != &m_parent) {
						NodeSmartReference node(path.top().get());
						Node_getTraversable(parent)->erase(node);
						Node_getTraversable(m_parent)->insert(node);
					}
				}
			}
		}
};

void Scene_parentSelectedBrushesToEntity (scene::Graph& graph, scene::Node& parent)
{
	graph.traverse(ParentSelectedBrushesToEntityWalker(parent));
}

class CountSelectedBrushes: public scene::Graph::Walker
{
		std::size_t& m_count;
		mutable std::size_t m_depth;
	public:
		CountSelectedBrushes (std::size_t& count) :
			m_count(count), m_depth(0)
		{
			m_count = 0;
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (++m_depth != 1 && path.top().get().isRoot()) {
				return false;
			}
			Selectable* selectable = Instance_getSelectable(instance);
			if (selectable != 0 && selectable->isSelected() && Node_isPrimitive(path.top())) {
				++m_count;
			}
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			--m_depth;
		}
};

std::size_t Scene_countSelectedBrushes (scene::Graph& graph)
{
	std::size_t count;
	graph.traverse(CountSelectedBrushes(count));
	return count;
}

enum ENodeType
{
	eNodeUnknown, eNodeMap, eNodeEntity, eNodePrimitive,
};

static inline const char* nodetype_get_name (ENodeType type)
{
	if (type == eNodeMap)
		return "map";
	if (type == eNodeEntity)
		return "entity";
	if (type == eNodePrimitive)
		return "primitive";
	return "unknown";
}

ENodeType node_get_nodetype (scene::Node& node)
{
	if (Node_isEntity(node)) {
		return eNodeEntity;
	}
	if (Node_isPrimitive(node)) {
		return eNodePrimitive;
	}
	return eNodeUnknown;
}

bool contains_entity (scene::Node& node)
{
	return Node_getTraversable(node) != 0 && !Node_isBrush(node) && !Node_isEntity(node);
}

bool contains_primitive (scene::Node& node)
{
	return Node_isEntity(node) && Node_getTraversable(node) != 0 && Node_getEntity(node)->isContainer();
}

ENodeType node_get_contains (scene::Node& node)
{
	if (contains_entity(node)) {
		return eNodeEntity;
	}
	if (contains_primitive(node)) {
		return eNodePrimitive;
	}
	return eNodeUnknown;
}

static void Path_parent (const scene::Path& parent, const scene::Path& child)
{
	ENodeType contains = node_get_contains(parent.top());
	ENodeType type = node_get_nodetype(child.top());

	if (contains != eNodeUnknown && contains == type) {
		NodeSmartReference node(child.top().get());
		Path_deleteTop(child);
		Node_getTraversable(parent.top())->insert(node);
		SceneChangeNotify();
	} else {
		globalErrorStream() << "failed - " << nodetype_get_name(type) << " cannot be parented to "
				<< nodetype_get_name(contains) << " container.\n";
	}
}

void Scene_parentSelected (void)
{
	UndoableCommand undo("parentSelected");

	if (GlobalSelectionSystem().countSelected() > 1) {
		class ParentSelectedBrushesToEntityWalker: public SelectionSystem::Visitor
		{
				const scene::Path& m_parent;
			public:
				ParentSelectedBrushesToEntityWalker (const scene::Path& parent) :
					m_parent(parent)
				{
				}
				void visit (scene::Instance& instance) const
				{
					if (&m_parent != &instance.path()) {
						Path_parent(m_parent, instance.path());
					}
				}
		};

		ParentSelectedBrushesToEntityWalker visitor(GlobalSelectionSystem().ultimateSelected().path());
		GlobalSelectionSystem().foreachSelected(visitor);
	} else {
		g_warning("Failed - did not find two selected nodes.\n");
	}
}

/**
 * @sa Map_Reload
 * @sa Map_New
 * @sa Map_Load
 */
void NewMap (void)
{
	if (ConfirmModified("New Map")) {
		Map_RegionOff();
		Map_Free();
		Map_New();
	}
}

static CopiedString g_mapsPath;

const char* getMapsPath (void)
{
	return g_mapsPath.c_str();
}

const char* map_open (const char* title)
{
	return file_dialog(GTK_WIDGET(MainFrame_getWindow()), TRUE, title, getMapsPath(), MapFormat::Name());
}

const char* map_save (const char* title)
{
	return file_dialog(GTK_WIDGET(MainFrame_getWindow()), FALSE, title, getMapsPath(), MapFormat::Name());
}

void OpenMap (void)
{
	if (!ConfirmModified("Open Map"))
		return;

	const char* filename = map_open("Open Map");

	if (filename != 0) {
		MRU_AddFile(filename);
		Map_RegionOff();
		Map_Free();
		Map_LoadFile(filename);
	}
}

void ImportMap (void)
{
	const char* filename = map_open("Import Map");

	if (filename != 0) {
		UndoableCommand undo("mapImport");
		Map_ImportFile(filename);
	}
}

bool Map_SaveAs (void)
{
	const char* filename = map_save("Save Map");

	if (filename != 0) {
		MRU_AddFile(filename);
		Map_Rename(filename);
		return Map_Save();
	}
	return false;
}

void SaveMapAs (void)
{
	Map_SaveAs();
}

void SaveMap (void)
{
	if (Map_Unnamed(g_map)) {
		SaveMapAs();
	} else if (Map_Modified(g_map)) {
		Map_Save();
	}
}

void ExportMap (void)
{
	const char* filename = map_save("Export Selection");

	if (filename != 0) {
		Map_SaveSelected(filename);
	}
}

void SaveRegion (void)
{
	const char* filename = map_save("Export Region");

	if (filename != 0) {
		Map_SaveRegion(filename);
	}
}

void RegionOff (void)
{
	Map_RegionOff();
	SceneChangeNotify();
}

void RegionXY (void)
{
	Map_RegionXY(g_pParentWnd->GetXYWnd()->GetOrigin()[0] - 0.5f * g_pParentWnd->GetXYWnd()->Width()
			/ g_pParentWnd->GetXYWnd()->Scale(), g_pParentWnd->GetXYWnd()->GetOrigin()[1] - 0.5f
			* g_pParentWnd->GetXYWnd()->Height() / g_pParentWnd->GetXYWnd()->Scale(),
			g_pParentWnd->GetXYWnd()->GetOrigin()[0] + 0.5f * g_pParentWnd->GetXYWnd()->Width()
					/ g_pParentWnd->GetXYWnd()->Scale(), g_pParentWnd->GetXYWnd()->GetOrigin()[1] + 0.5f
					* g_pParentWnd->GetXYWnd()->Height() / g_pParentWnd->GetXYWnd()->Scale());
	SceneChangeNotify();
}

void RegionBrush (void)
{
	Map_RegionBrush();
	SceneChangeNotify();
}

void RegionSelected (void)
{
	Map_RegionSelectedBrushes();
	SceneChangeNotify();
}

class BrushFindByIndexWalker: public scene::Traversable::Walker
{
		mutable std::size_t m_index;
		scene::Path& m_path;
	public:
		BrushFindByIndexWalker (std::size_t index, scene::Path& path) :
			m_index(index), m_path(path)
		{
		}
		bool pre (scene::Node& node) const
		{
			if (Node_isPrimitive(node) && m_index-- == 0) {
				m_path.push(makeReference(node));
			}
			return false;
		}
};

class EntityFindByIndexWalker: public scene::Traversable::Walker
{
		mutable std::size_t m_index;
		scene::Path& m_path;
	public:
		EntityFindByIndexWalker (std::size_t index, scene::Path& path) :
			m_index(index), m_path(path)
		{
		}
		bool pre (scene::Node& node) const
		{
			if (Node_isEntity(node) && m_index-- == 0) {
				m_path.push(makeReference(node));
			}
			return false;
		}
};

static void Scene_FindEntityBrush (std::size_t entity, std::size_t brush, scene::Path& path)
{
	path.push(makeReference(GlobalSceneGraph().root()));
	{
		Node_getTraversable(path.top())->traverse(EntityFindByIndexWalker(entity, path));
	}
	if (path.size() == 2) {
		scene::Traversable* traversable = Node_getTraversable(path.top());
		if (traversable != 0) {
			traversable->traverse(BrushFindByIndexWalker(brush, path));
		}
	}
}

static inline bool Node_hasChildren (scene::Node& node)
{
	scene::Traversable* traversable = Node_getTraversable(node);
	return traversable != 0 && !traversable->empty();
}

/**
 * @brief Selects a brush given by entity- and brushnumber
 * @param[in] entitynum The entity number (0 for worldspawn, every other for
 * bmodels)
 * @param[in] brushnum The brush number to select
 */
void SelectBrush (int entitynum, int brushnum, int select)
{
	scene::Path path;
	Scene_FindEntityBrush(entitynum, brushnum, path);
	if (path.size() == 3 || (path.size() == 2 && !Node_hasChildren(path.top()))) {
		scene::Instance* instance = GlobalSceneGraph().find(path);
		ASSERT_MESSAGE(instance != 0, "SelectBrush: path not found in scenegraph");
		Selectable* selectable = Instance_getSelectable(*instance);
		ASSERT_MESSAGE(selectable != 0, "SelectBrush: path not selectable");
		selectable->setSelected(select);
		g_pParentWnd->GetXYWnd()->PositionView(instance->worldAABB().origin);
	}
}

class MapEntityClasses: public ModuleObserver
{
		std::size_t m_unrealised;
	public:
		MapEntityClasses () :
			m_unrealised(1)
		{
		}
		void realise (void)
		{
			if (--m_unrealised == 0) {
				if (g_map.m_resource != 0) {
					ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Loading Map"));
					g_map.m_resource->realise();
				}
			}
		}
		void unrealise (void) {
			if (++m_unrealised == 1) {
				if (g_map.m_resource != 0) {
					g_map.m_resource->flush();
					g_map.m_resource->unrealise();
				}
			}
		}
	};

MapEntityClasses g_MapEntityClasses;

class MapModuleObserver: public ModuleObserver
{
		std::size_t m_unrealised;
	public:
		MapModuleObserver () :
			m_unrealised(1)
		{
		}
		void realise (void)
		{
			if (--m_unrealised == 0) {
				ASSERT_MESSAGE(!string_empty(g_qeglobals.m_userGamePath.c_str()), "maps_directory: user-game-path is empty");
				StringOutputStream buffer(256);
				buffer << g_qeglobals.m_userGamePath.c_str() << "maps/";
				g_mkdir(buffer.c_str(), 0775);
				g_mapsPath = buffer.c_str();
			}
		}
		void unrealise (void)
		{
			if (++m_unrealised == 1) {
				g_mapsPath = "";
			}
		}
};

MapModuleObserver g_MapModuleObserver;

#include "preferencesystem.h"

CopiedString g_strLastMap;
bool g_bLoadLastMap = false;

void Map_Construct (void)
{
	GlobalCommands_insert("RegionOff", FreeCaller<RegionOff> ());
	GlobalCommands_insert("RegionSetXY", FreeCaller<RegionXY> ());
	GlobalCommands_insert("RegionSetBrush", FreeCaller<RegionBrush> ());
	GlobalCommands_insert("RegionSetSelection", FreeCaller<RegionSelected> (), Accelerator('R',
			(GdkModifierType) (GDK_SHIFT_MASK | GDK_CONTROL_MASK)));

	GlobalPreferenceSystem().registerPreference("LastMap", CopiedStringImportStringCaller(g_strLastMap),
			CopiedStringExportStringCaller(g_strLastMap));
	GlobalPreferenceSystem().registerPreference("LoadLastMap", BoolImportStringCaller(g_bLoadLastMap),
			BoolExportStringCaller(g_bLoadLastMap));

	GlobalEntityClassManager().attach(g_MapEntityClasses);
	Radiant_attachHomePathsObserver(g_MapModuleObserver);
}

void Map_Destroy (void)
{
	Radiant_detachHomePathsObserver(g_MapModuleObserver);
	GlobalEntityClassManager().detach(g_MapEntityClasses);
}
