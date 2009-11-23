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

#include "shared.h"
#include "radiant_i18n.h"

#include "brushmanip.h"

#include "gtkutil/widget.h"
#include "gtkutil/menu.h"
#include "../gtkmisc.h"
#include "brushnode.h"
#include "../map/map.h"
#include "../sidebar/sidebar.h"
#include "../commands.h"
#include "../dialog.h"
#include "../xyview/xywindow.h"
#include "../settings/preferences.h"
#include "../mainframe.h"

#include "construct/Prism.h"
#include "construct/Cone.h"
#include "construct/Cuboid.h"
#include "construct/Rock.h"
#include "construct/Sphere.h"
#include "construct/Terrain.h"

#include <list>

static void Brush_ConstructPrefab (Brush& brush, EBrushPrefab type, const AABB& bounds, std::size_t sides,
		const TextureProjection& projection, const std::string& shader = "textures/tex_common/nodraw")
{
	brushconstruct::BrushConstructor *bc;
	switch (type) {
	case eBrushPrism:
		bc = &brushconstruct::Prism::getInstance();
		break;
	case eBrushCone:
		bc = &brushconstruct::Cone::getInstance();
		break;
	case eBrushSphere:
		bc = &brushconstruct::Sphere::getInstance();
		break;
	case eBrushRock:
		bc = &brushconstruct::Rock::getInstance();
		break;
	case eBrushTerrain:
		bc = &brushconstruct::Terrain::getInstance();
		break;
	default:
		return;
	}

	std::string command = bc->getName() + " -sides " + string::toString(sides);
	UndoableCommand undo(command);

	bc->generate(brush, bounds, sides, projection, shader);
}

class FaceSetTexdef
{
		const TextureProjection& m_projection;
	public:
		FaceSetTexdef (const TextureProjection& projection) :
			m_projection(projection)
		{
		}
		void operator() (Face& face) const
		{
			face.SetTexdef(m_projection);
		}
};

void Scene_BrushSetTexdef_Selected (scene::Graph& graph, const TextureProjection& projection)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetTexdef(projection));
	SceneChangeNotify();
}

void Scene_BrushSetTexdef_Component_Selected (scene::Graph& graph, const TextureProjection& projection)
{
	Scene_ForEachSelectedBrushFace(FaceSetTexdef(projection));
	SceneChangeNotify();
}

class FaceSetFlags
{
		const ContentsFlagsValue& m_projection;
	public:
		FaceSetFlags (const ContentsFlagsValue& flags) :
			m_projection(flags)
		{
		}
		void operator() (Face& face) const
		{
			face.SetFlags(m_projection);
		}
};

void Scene_BrushSetFlags_Selected (scene::Graph& graph, const ContentsFlagsValue& flags)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetFlags(flags));
	SceneChangeNotify();
}

void Scene_BrushSetFlags_Component_Selected (scene::Graph& graph, const ContentsFlagsValue& flags)
{
	Scene_ForEachSelectedBrushFace(FaceSetFlags(flags));
	SceneChangeNotify();
}

class FaceShiftTexdef
{
		float m_s, m_t;
	public:
		FaceShiftTexdef (float s, float t) :
			m_s(s), m_t(t)
		{
		}
		void operator() (Face& face) const
		{
			face.ShiftTexdef(m_s, m_t);
		}
};

void Scene_BrushShiftTexdef_Selected (scene::Graph& graph, float s, float t)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceShiftTexdef(s, t));
	SceneChangeNotify();
}

void Scene_BrushShiftTexdef_Component_Selected (scene::Graph& graph, float s, float t)
{
	Scene_ForEachSelectedBrushFace(FaceShiftTexdef(s, t));
	SceneChangeNotify();
}

class FaceScaleTexdef
{
		float m_s, m_t;
	public:
		FaceScaleTexdef (float s, float t) :
			m_s(s), m_t(t)
		{
		}
		void operator() (Face& face) const
		{
			face.ScaleTexdef(m_s, m_t);
		}
};

void Scene_BrushScaleTexdef_Selected (scene::Graph& graph, float s, float t)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceScaleTexdef(s, t));
	SceneChangeNotify();
}

void Scene_BrushScaleTexdef_Component_Selected (scene::Graph& graph, float s, float t)
{
	Scene_ForEachSelectedBrushFace(FaceScaleTexdef(s, t));
	SceneChangeNotify();
}

class FaceRotateTexdef
{
		float m_angle;
	public:
		FaceRotateTexdef (float angle) :
			m_angle(angle)
		{
		}
		void operator() (Face& face) const
		{
			face.RotateTexdef(m_angle);
		}
};

void Scene_BrushRotateTexdef_Selected (scene::Graph& graph, float angle)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceRotateTexdef(angle));
	SceneChangeNotify();
}

void Scene_BrushRotateTexdef_Component_Selected (scene::Graph& graph, float angle)
{
	Scene_ForEachSelectedBrushFace(FaceRotateTexdef(angle));
	SceneChangeNotify();
}

class FaceSetShader
{
		const char* m_name;
	public:
		FaceSetShader (const char* name) :
			m_name(name)
		{
		}
		void operator() (Face& face) const
		{
			face.SetShader(m_name);
		}
};

void Scene_BrushSetShader_Selected (scene::Graph& graph, const char* name)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetShader(name));
	SceneChangeNotify();
}

void Scene_BrushSetShader_Component_Selected (scene::Graph& graph, const char* name)
{
	Scene_ForEachSelectedBrushFace(FaceSetShader(name));
	SceneChangeNotify();
}

class FaceSetDetail
{
		bool m_detail;
	public:
		FaceSetDetail (bool detail) :
			m_detail(detail)
		{
		}
		void operator() (Face& face) const
		{
			face.setDetail(m_detail);
		}
};

void Scene_BrushSetDetail_Selected (scene::Graph& graph, bool detail)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetDetail(detail));
	SceneChangeNotify();
}

bool Face_FindReplaceShader (Face& face, const char* find, const char* replace)
{
	if (shader_equal(face.GetShader(), find)) {
		face.SetShader(replace);
		return true;
	}
	return false;
}

class FaceFindReplaceShader
{
		const char* m_find;
		const char* m_replace;
	public:
		FaceFindReplaceShader (const char* find, const char* replace) :
			m_find(find), m_replace(replace)
		{
		}
		void operator() (Face& face) const
		{
			Face_FindReplaceShader(face, m_find, m_replace);
		}
};

class FaceFindShader
{
		const char* m_find;
		const char* m_replace;
	public:
		FaceFindShader (const char* find) :
			m_find(find)
		{
		}
		void operator() (FaceInstance& faceinst) const
		{
			if (shader_equal(faceinst.getFace().GetShader(), m_find)) {
				faceinst.setSelected(SelectionSystem::eFace, true);
			}
		}
};

bool DoingSearch (const char *repl)
{
	return (repl == NULL || (strcmp("textures/", repl) == 0));
}

void Scene_BrushFindReplaceShader (scene::Graph& graph, const char* find, const char* replace)
{
	if (DoingSearch(replace)) {
		Scene_ForEachBrush_ForEachFaceInstance(graph, FaceFindShader(find));
	} else {
		Scene_ForEachBrush_ForEachFace(graph, FaceFindReplaceShader(find, replace));
	}
}

void Scene_BrushFindReplaceShader_Selected (scene::Graph& graph, const char* find, const char* replace)
{
	if (DoingSearch(replace)) {
		Scene_ForEachSelectedBrush_ForEachFaceInstance(graph, FaceFindShader(find));
	} else {
		Scene_ForEachSelectedBrush_ForEachFace(graph, FaceFindReplaceShader(find, replace));
	}
}

// TODO: find for components
// d1223m: dont even know what they are...
void Scene_BrushFindReplaceShader_Component_Selected (scene::Graph& graph, const char* find, const char* replace)
{
	if (DoingSearch(replace)) {

	} else {
		Scene_ForEachSelectedBrushFace(FaceFindReplaceShader(find, replace));
	}
}

class FaceFitTexture
{
		float m_s_repeat, m_t_repeat;
	public:
		FaceFitTexture (float s_repeat, float t_repeat) :
			m_s_repeat(s_repeat), m_t_repeat(t_repeat)
		{
		}
		void operator() (Face& face) const
		{
			face.FitTexture(m_s_repeat, m_t_repeat);
		}
};

void Scene_BrushFitTexture_Selected (scene::Graph& graph, float s_repeat, float t_repeat)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceFitTexture(s_repeat, t_repeat));
	SceneChangeNotify();
}

void Scene_BrushFitTexture_Component_Selected (scene::Graph& graph, float s_repeat, float t_repeat)
{
	Scene_ForEachSelectedBrushFace(FaceFitTexture(s_repeat, t_repeat));
	SceneChangeNotify();
}

TextureProjection g_defaultTextureProjection;
const TextureProjection& TextureTransform_getDefault ()
{
	TexDef_Construct_Default(g_defaultTextureProjection);
	return g_defaultTextureProjection;
}

void Scene_BrushConstructPrefab (scene::Graph& graph, EBrushPrefab type, std::size_t sides, const char* shader)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();

		Brush* brush = Node_getBrush(path.top());
		if (brush != 0) {
			AABB bounds = brush->localAABB(); // copy bounds because the brush will be modified
			Brush_ConstructPrefab(*brush, type, bounds, sides, TextureTransform_getDefault(), shader);
			SceneChangeNotify();
		}
	}
}

void Scene_BrushResize_Selected (scene::Graph& graph, const AABB& bounds, const char* shader)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();

		Brush* brush = Node_getBrush(path.top());
		if (brush != 0) {
			brushconstruct::Cuboid::getInstance().generate(*brush, bounds, 0, TextureTransform_getDefault(), shader);
			SceneChangeNotify();
		}
	}
}

static inline bool Brush_hasShader (const Brush& brush, const char* name)
{
	for (Brush::const_iterator i = brush.begin(); i != brush.end(); ++i) {
		if (shader_equal((*i)->GetShader(), name)) {
			return true;
		}
	}
	return false;
}

class BrushSelectByShaderWalker: public scene::Graph::Walker
{
		const char* m_name;
	public:
		BrushSelectByShaderWalker (const char* name) :
			m_name(name)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get().visible()) {
				Brush* brush = Node_getBrush(path.top());
				if (brush != 0 && Brush_hasShader(*brush, m_name)) {
					Instance_getSelectable(instance)->setSelected(true);
				}
			}
			return true;
		}
};

void Scene_BrushSelectByShader (scene::Graph& graph, const char* name)
{
	graph.traverse(BrushSelectByShaderWalker(name));
}

class FaceSelectByShader
{
		const std::string m_name;
	public:
		FaceSelectByShader (const std::string& name) :
			m_name(name)
		{
		}
		void operator() (FaceInstance& face) const
		{
			if (shader_equal(face.getFace().GetShader(), m_name)) {
				face.setSelected(SelectionSystem::eFace, true);
			}
		}
};

void Scene_BrushSelectByShader_Component (scene::Graph& graph, const std::string& name)
{
	Scene_ForEachSelectedBrush_ForEachFaceInstance(graph, FaceSelectByShader(name));
}

/**
 * @brief Select all faces in given graph that use given texture
 * @param graph scene graph to select faces from
 * @param name texture name
 */
void Scene_BrushFacesSelectByShader_Component (scene::Graph& graph, const char* name)
{
	Scene_ForEachBrush_ForEachFaceInstance(graph, FaceSelectByShader(name));
}

class FaceGetTexdef
{
		TextureProjection& m_projection;
		mutable bool m_done;
	public:
		FaceGetTexdef (TextureProjection& projection) :
			m_projection(projection), m_done(false)
		{
		}
		void operator() (Face& face) const
		{
			if (!m_done) {
				m_done = true;
				face.GetTexdef(m_projection);
			}
		}
};

void Scene_BrushGetTexdef_Selected (scene::Graph& graph, TextureProjection& projection)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceGetTexdef(projection));
}

void Scene_BrushGetTexdef_Component_Selected (scene::Graph& graph, TextureProjection& projection)
{
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		faceInstance.getFace().GetTexdef(projection);
	}
}

void Scene_BrushGetShaderSize_Component_Selected (scene::Graph& graph, size_t& width, size_t& height)
{
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		width = faceInstance.getFace().getShader().width();
		height = faceInstance.getFace().getShader().height();
	}
}

void FaceGetFlags::operator ()(Face& face) const
{
	face.GetFlags(m_flags);
}

/**
 * @sa SurfaceInspector_SetCurrent_FromSelected
 */
void Scene_BrushGetFlags_Selected (scene::Graph& graph, ContentsFlagsValue& flags)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		Scene_ForEachSelectedBrush_ForEachFace(graph, FaceGetFlags(flags));
	}
}

/**
 * @sa SurfaceInspector_SetCurrent_FromSelected
 */
void Scene_BrushGetFlags_Component_Selected (scene::Graph& graph, ContentsFlagsValue& flags)
{
	if (GlobalSelectionSystem().countSelectedComponents() != 0) {
		Scene_ForEachSelectedBrushFace(FaceGetFlags(flags));
	}
}

class FaceGetShader
{
		std::string& m_shader;
		mutable bool m_done;
	public:
		FaceGetShader (std::string& shader) :
			m_shader(shader), m_done(false)
		{
		}
		void operator() (Face& face) const
		{
			if (!m_done) {
				m_done = true;
				m_shader = face.GetShader();
			}
		}
};

void Scene_BrushGetShader_Selected (scene::Graph& graph, std::string& shader)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		BrushInstance* brush = Instance_getBrush(GlobalSelectionSystem().ultimateSelected());
		if (brush != 0) {
			Brush_forEachFace(*brush, FaceGetShader(shader));
		}
	}
}

void Scene_BrushGetShader_Component_Selected (scene::Graph& graph, std::string& shader)
{
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		shader = faceInstance.getFace().GetShader();
	}
}

class filter_face_shader: public FaceFilter
{
		const char* m_shader;
	public:
		filter_face_shader (const char* shader) :
			m_shader(shader)
		{
		}
		bool filter (const Face& face) const
		{
			return shader_equal(face.GetShader(), m_shader);
		}
};

class filter_face_shader_prefix: public FaceFilter
{
		const char* m_prefix;
	public:
		filter_face_shader_prefix (const char* prefix) :
			m_prefix(prefix)
		{
		}
		bool filter (const Face& face) const
		{
			return shader_equal_n(face.GetShader(), m_prefix, strlen(m_prefix));
		}
};

class filter_face_flags: public FaceFilter
{
		int m_flags;
	public:
		filter_face_flags (int flags) :
			m_flags(flags)
		{
		}
		bool filter (const Face& face) const
		{
			return (face.getShader().shaderFlags() & m_flags) != 0;
		}
};

class filter_face_contents: public FaceFilter
{
		int m_contents;
	public:
		filter_face_contents (int contents) :
			m_contents(contents)
		{
		}
		bool filter (const Face& face) const
		{
			return (face.getShader().m_flags.m_contentFlags & m_contents) != 0;
		}
};

class filter_face_surface: public FaceFilter
{
		int m_surface;
	public:
		filter_face_surface (int surface) :
			m_surface(surface)
		{
		}
		bool filter (const Face& face) const
		{
			return (face.getShader().m_flags.m_surfaceFlags & m_surface) != 0;
		}
};

class FaceFilterAny
{
		FaceFilter* m_filter;
		bool& m_filtered;
	public:
		FaceFilterAny (FaceFilter* filter, bool& filtered) :
			m_filter(filter), m_filtered(filtered)
		{
			m_filtered = false;
		}
		void operator() (Face& face) const
		{
			if (m_filter->filter(face)) {
				m_filtered = true;
			}
		}
};

class filter_brush_any_face: public BrushFilter
{
		FaceFilter* m_filter;
	public:
		filter_brush_any_face (FaceFilter* filter) :
			m_filter(filter)
		{
		}
		bool filter (const Brush& brush) const
		{
			bool filtered;
			Brush_forEachFace(brush, FaceFilterAny(m_filter, filtered));
			return filtered;
		}
};

class FaceFilterAll
{
		FaceFilter* m_filter;
		bool& m_filtered;
	public:
		FaceFilterAll (FaceFilter* filter, bool& filtered) :
			m_filter(filter), m_filtered(filtered)
		{
			m_filtered = true;
		}
		void operator() (Face& face) const
		{
			if (!m_filter->filter(face)) {
				m_filtered = false;
			}
		}
};

class FaceFilterOne
{
		FaceFilter* m_filter;
		bool& m_filtered;
	public:
		FaceFilterOne (FaceFilter* filter, bool& filtered) :
			m_filter(filter), m_filtered(filtered)
		{
			m_filtered = false;
		}
		void operator() (Face& face) const
		{
			if (m_filter->filter(face)) {
				m_filtered = true;
			}
		}
};

class filter_brush_all_faces: public BrushFilter
{
		FaceFilter* m_filter;
	public:
		filter_brush_all_faces (FaceFilter* filter) :
			m_filter(filter)
		{
		}
		bool filter (const Brush& brush) const
		{
			bool filtered;
			Brush_forEachFace(brush, FaceFilterAll(m_filter, filtered));
			return filtered;
		}
};

class filter_brush_one_face: public BrushFilter
{
		FaceFilter* m_filter;
	public:
		filter_brush_one_face (FaceFilter* filter) :
			m_filter(filter)
		{
		}
		bool filter (const Brush& brush) const
		{
			bool filtered;
			Brush_forEachFace(brush, FaceFilterOne(m_filter, filtered));
			return filtered;
		}
};

class filter_brush_no_face: public BrushFilter
{
		FaceFilter* m_filter;
	public:
		filter_brush_no_face (FaceFilter* filter) :
			m_filter(filter)
		{
		}
		bool filter (const Brush& brush) const
		{
			bool filtered;
			Brush_forEachFace(brush, FaceFilterOne(m_filter, filtered));
			return !filtered;
		}
};

filter_face_flags g_filter_face_clip(QER_CLIP);
filter_brush_all_faces g_filter_brush_clip(&g_filter_face_clip);

filter_face_surface g_filter_face_light(SURF_LIGHT);
filter_brush_one_face g_filter_brush_light(&g_filter_face_light);

filter_face_surface g_filter_face_phong(SURF_PHONG);
filter_brush_one_face g_filter_brush_phong(&g_filter_face_phong);

filter_face_surface g_filter_face_no_surflight(SURF_LIGHT);
filter_brush_no_face g_filter_brush_no_surflight(&g_filter_face_no_surflight);

filter_face_surface g_filter_face_no_footstep(SURF_FOOTSTEP);
filter_brush_no_face g_filter_brush_no_footstep(&g_filter_face_no_footstep);

filter_face_shader g_filter_face_weapclip("textures/tex_common/weaponclip");
filter_brush_all_faces g_filter_brush_weapclip(&g_filter_face_weapclip);

filter_face_shader g_filter_face_actorclip("textures/tex_common/actorclip");
filter_brush_all_faces g_filter_brush_actorclip(&g_filter_face_actorclip);

filter_face_shader_prefix g_filter_face_caulk("textures/tex_common/caulk");
filter_brush_all_faces g_filter_brush_caulk(&g_filter_face_caulk);

filter_face_shader_prefix g_filter_face_liquids("textures/liquids/");
filter_brush_any_face g_filter_brush_liquids(&g_filter_face_liquids);

filter_face_shader g_filter_face_hint("textures/tex_common/hint");
filter_brush_any_face g_filter_brush_hint(&g_filter_face_hint);

filter_face_shader g_filter_face_nodraw("textures/tex_common/nodraw");
filter_brush_all_faces g_filter_brush_nodraw(&g_filter_face_nodraw);

filter_face_flags g_filter_face_translucent(QER_TRANS);
filter_brush_all_faces g_filter_brush_translucent(&g_filter_face_translucent);

filter_face_flags g_filter_face_water(BRUSH_WATER_MASK);
filter_brush_all_faces g_filter_brush_water(&g_filter_face_water);

filter_face_contents g_filter_face_detail(BRUSH_DETAIL_MASK);
filter_brush_all_faces g_filter_brush_detail(&g_filter_face_detail);

void BrushFilters_construct ()
{
	add_brush_filter(g_filter_brush_clip, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_weapclip, EXCLUDE_WEAPONCLIP);
	add_brush_filter(g_filter_brush_phong, EXCLUDE_PHONG);
	add_brush_filter(g_filter_brush_no_footstep, EXCLUDE_NO_FOOTSTEPS);
	add_brush_filter(g_filter_brush_light, EXCLUDE_LIGHTS);
	add_brush_filter(g_filter_brush_no_surflight, EXCLUDE_NO_SURFLIGHTS);
	add_brush_filter(g_filter_brush_actorclip, EXCLUDE_ACTORCLIP);
	add_brush_filter(g_filter_brush_weapclip, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_actorclip, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_caulk, EXCLUDE_CAULK);
	add_face_filter(g_filter_face_caulk, EXCLUDE_CAULK);
	add_brush_filter(g_filter_brush_liquids, EXCLUDE_LIQUIDS);
	add_brush_filter(g_filter_brush_water, EXCLUDE_LIQUIDS);
	add_brush_filter(g_filter_brush_hint, EXCLUDE_HINTSSKIPS);
	add_brush_filter(g_filter_brush_translucent, EXCLUDE_TRANSLUCENT);
	add_brush_filter(g_filter_brush_detail, EXCLUDE_DETAILS);
	add_brush_filter(g_filter_brush_detail, EXCLUDE_STRUCTURAL, true);
	add_brush_filter(g_filter_brush_nodraw, EXCLUDE_NODRAW);
}

void Select_MakeDetail ()
{
	UndoableCommand undo("brushSetDetail");
	Scene_BrushSetDetail_Selected(GlobalSceneGraph(), true);
}

void Select_MakeStructural ()
{
	UndoableCommand undo("brushClearDetail");
	Scene_BrushSetDetail_Selected(GlobalSceneGraph(), false);
}

class BrushMakeSided
{
		std::size_t m_count;
	public:
		BrushMakeSided (std::size_t count) :
			m_count(count)
		{
		}
		void set ()
		{
			Scene_BrushConstructPrefab(GlobalSceneGraph(), eBrushPrism, m_count, TextureBrowser_GetSelectedShader(
					GlobalTextureBrowser()));
		}
		typedef MemberCaller<BrushMakeSided, &BrushMakeSided::set> SetCaller;
};

BrushMakeSided g_brushmakesided3(3);
BrushMakeSided g_brushmakesided4(4);
BrushMakeSided g_brushmakesided5(5);
BrushMakeSided g_brushmakesided6(6);
BrushMakeSided g_brushmakesided7(7);
BrushMakeSided g_brushmakesided8(8);
BrushMakeSided g_brushmakesided9(9);

#include "radiant_i18n.h"
#include <gdk/gdkkeysyms.h>
#include "gtkutil/dialog.h"
#include "scenelib.h"
#include "../brush/brushmanip.h"
#include "../sidebar/sidebar.h"
class BrushPrefab
{
	private:
		EBrushPrefab m_type;

		static void DoSides (EBrushPrefab type)
		{
			ModalDialog dialog;
			GtkEntry* sides_entry;

			GtkWindow* window = create_dialog_window(GlobalRadiant().getMainWindow(), _("Arbitrary sides"),
					G_CALLBACK(dialog_delete_callback), &dialog);

			GtkAccelGroup* accel = gtk_accel_group_new();
			gtk_window_add_accel_group(window, accel);

			{
				GtkHBox* hbox = create_dialog_hbox(4, 4);
				gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(hbox));
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new(_("Sides:")));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), FALSE, FALSE, 0);
				}
				{
					GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
					gtk_widget_show(GTK_WIDGET(entry));
					gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(entry), FALSE, FALSE, 0);
					sides_entry = entry;
					gtk_widget_grab_focus(GTK_WIDGET(entry));
				}
				{
					GtkVBox* vbox = create_dialog_vbox(4);
					gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), TRUE, TRUE, 0);
					{
						GtkButton* button = create_dialog_button(_("OK"), G_CALLBACK(dialog_button_ok), &dialog);
						gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
						widget_make_default(GTK_WIDGET(button));
						gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Return,
								(GdkModifierType) 0, (GtkAccelFlags) 0);
					}
					{
						GtkButton* button =
								create_dialog_button(_("Cancel"), G_CALLBACK(dialog_button_cancel), &dialog);
						gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
						gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Escape,
								(GdkModifierType) 0, (GtkAccelFlags) 0);
					}
				}
			}

			if (modal_dialog_show(window, dialog) == eIDOK) {
				const char *str = gtk_entry_get_text(sides_entry);

				Scene_BrushConstructPrefab(GlobalSceneGraph(), type, atoi(str), TextureBrowser_GetSelectedShader(
						GlobalTextureBrowser()));
			}

			gtk_widget_destroy(GTK_WIDGET(window));
		}

	public:
		BrushPrefab (EBrushPrefab type) :
			m_type(type)
		{
		}
		void set ()
		{
			if (m_type == eBrushTerrain)
				Scene_BrushConstructPrefab(GlobalSceneGraph(), m_type, 0, TextureBrowser_GetSelectedShader(
						GlobalTextureBrowser()));
			else
				DoSides(m_type);
		}
		typedef MemberCaller<BrushPrefab, &BrushPrefab::set> SetCaller;
};

static BrushPrefab g_brushprism(eBrushPrism);
static BrushPrefab g_brushcone(eBrushCone);
static BrushPrefab g_brushsphere(eBrushSphere);
static BrushPrefab g_brushrock(eBrushRock);
static BrushPrefab g_brushterrain(eBrushTerrain);

void ClipSelected ()
{
	if (ClipMode()) {
		UndoableCommand undo("clipperClip");
		Clip();
	}
}

void SplitSelected ()
{
	if (ClipMode()) {
		UndoableCommand undo("clipperSplit");
		SplitClip();
	}
}

void FlipClipper ()
{
	FlipClip();
}

Callback g_texture_lock_status_changed;
BoolExportCaller g_texdef_movelock_caller(g_brush_texturelock_enabled);
ToggleItem g_texdef_movelock_item(g_texdef_movelock_caller);

void Texdef_ToggleMoveLock ()
{
	g_brush_texturelock_enabled = !g_brush_texturelock_enabled;
	g_texdef_movelock_item.update();
	g_texture_lock_status_changed();
}

void Brush_registerCommands ()
{
	GlobalToggles_insert("TogTexLock", FreeCaller<Texdef_ToggleMoveLock> (), ToggleItem::AddCallbackCaller(
			g_texdef_movelock_item), Accelerator('T', (GdkModifierType) GDK_SHIFT_MASK));

	GlobalCommands_insert("BrushPrism", BrushPrefab::SetCaller(g_brushprism));
	GlobalCommands_insert("BrushCone", BrushPrefab::SetCaller(g_brushcone));
	GlobalCommands_insert("BrushSphere", BrushPrefab::SetCaller(g_brushsphere));
	GlobalCommands_insert("BrushRock", BrushPrefab::SetCaller(g_brushrock));
	GlobalCommands_insert("BrushTerrain", BrushPrefab::SetCaller(g_brushterrain));

	GlobalRadiant().commandInsert("Brush3Sided", BrushMakeSided::SetCaller(g_brushmakesided3), Accelerator('3',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("Brush4Sided", BrushMakeSided::SetCaller(g_brushmakesided4), Accelerator('4',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("Brush5Sided", BrushMakeSided::SetCaller(g_brushmakesided5), Accelerator('5',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("Brush6Sided", BrushMakeSided::SetCaller(g_brushmakesided6), Accelerator('6',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("Brush7Sided", BrushMakeSided::SetCaller(g_brushmakesided7), Accelerator('7',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("Brush8Sided", BrushMakeSided::SetCaller(g_brushmakesided8), Accelerator('8',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("Brush9Sided", BrushMakeSided::SetCaller(g_brushmakesided9), Accelerator('9',
			(GdkModifierType) GDK_CONTROL_MASK));

	GlobalRadiant().commandInsert("ClipSelected", FreeCaller<ClipSelected> (), Accelerator(GDK_Return));
	GlobalRadiant().commandInsert("SplitSelected", FreeCaller<SplitSelected> (), Accelerator(GDK_Return,
			(GdkModifierType) GDK_SHIFT_MASK));
	GlobalRadiant().commandInsert("FlipClip", FreeCaller<FlipClipper> (), Accelerator(GDK_Return,
			(GdkModifierType) GDK_CONTROL_MASK));

	GlobalRadiant().commandInsert("MakeDetail", FreeCaller<Select_MakeDetail> (), Accelerator('M',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalRadiant().commandInsert("MakeStructural", FreeCaller<Select_MakeStructural> (), Accelerator('S',
			(GdkModifierType) (GDK_SHIFT_MASK | GDK_CONTROL_MASK)));
}

void Brush_constructMenu (GtkMenu* menu)
{
	create_menu_item_with_mnemonic(menu, _("Cone..."), "BrushCone");
	create_menu_item_with_mnemonic(menu, _("Prism..."), "BrushPrism");
	create_menu_item_with_mnemonic(menu, _("Sphere..."), "BrushSphere");
	create_menu_item_with_mnemonic(menu, _("Rock..."), "BrushRock");
	create_menu_item_with_mnemonic(menu, _("Terrain..."), "BrushTerrain");
	menu_separator(menu);
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic(menu, C_("Constructive Solid Geometry", "CSG"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(menu_in_menu);
		create_menu_item_with_mnemonic(menu_in_menu, _("Make Hollow"), "CSGHollow");
		create_menu_item_with_mnemonic(menu_in_menu, C_("Constructive Solid Geometry", "CSG Subtract"), "CSGSubtract");
		create_menu_item_with_mnemonic(menu_in_menu, C_("Constructive Solid Geometry", "CSG Merge"), "CSGMerge");
	}
	menu_separator(menu);
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic(menu, _("Clipper"));
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff(menu_in_menu);

		create_menu_item_with_mnemonic(menu_in_menu, _("Clip selection"), "ClipSelected");
		create_menu_item_with_mnemonic(menu_in_menu, _("Split selection"), "SplitSelected");
		create_menu_item_with_mnemonic(menu_in_menu, _("Flip Clip orientation"), "FlipClip");
	}
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("Make detail"), "MakeDetail");
	create_menu_item_with_mnemonic(menu, _("Make structural"), "MakeStructural");

	create_check_menu_item_with_mnemonic(menu, _("Texture Lock"), "TogTexLock");
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("Copy Face Texture"), "FaceCopyTexture");
	create_menu_item_with_mnemonic(menu, _("Paste Face Texture"), "FacePasteTexture");

	command_connect_accelerator("Brush3Sided");
	command_connect_accelerator("Brush4Sided");
	command_connect_accelerator("Brush5Sided");
	command_connect_accelerator("Brush6Sided");
	command_connect_accelerator("Brush7Sided");
	command_connect_accelerator("Brush8Sided");
	command_connect_accelerator("Brush9Sided");
}
