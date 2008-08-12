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

#include "brushmanip.h"


#include "gtkutil/widget.h"
#include "gtkutil/menu.h"
#include "gtkmisc.h"
#include "brushnode.h"
#include "map.h"
#include "texwindow.h"
#include "gtkdlgs.h"
#include "commands.h"
#include "mainframe.h"
#include "dialog.h"
#include "xywindow.h"
#include "preferences.h"

#include <list>

void Brush_ConstructCuboid(Brush& brush, const AABB& bounds, const char* shader, const TextureProjection& projection) {
	const unsigned char box[3][2] = { { 0, 1 }, { 2, 0 }, { 1, 2 } };
	Vector3 mins(vector3_subtracted(bounds.origin, bounds.extents));
	Vector3 maxs(vector3_added(bounds.origin, bounds.extents));

	brush.clear();
	brush.reserve(6);

	{
		for (int i = 0; i < 3; ++i) {
			Vector3 planepts1(maxs);
			Vector3 planepts2(maxs);
			planepts2[box[i][0]] = mins[box[i][0]];
			planepts1[box[i][1]] = mins[box[i][1]];

			brush.addPlane(maxs, planepts1, planepts2, shader, projection);
		}
	}
	{
		for (int i = 0; i < 3; ++i) {
			Vector3 planepts1(mins);
			Vector3 planepts2(mins);
			planepts1[box[i][0]] = maxs[box[i][0]];
			planepts2[box[i][1]] = maxs[box[i][1]];

			brush.addPlane(mins, planepts1, planepts2, shader, projection);
		}
	}
}

inline float max_extent(const Vector3& extents) {
	return std::max(std::max(extents[0], extents[1]), extents[2]);
}

inline float max_extent_2d(const Vector3& extents, int axis) {
	switch (axis) {
	case 0:
		return std::max(extents[1], extents[2]);
	case 1:
		return std::max(extents[0], extents[2]);
	default:
		return std::max(extents[0], extents[1]);
	}
}

const std::size_t c_brushPrism_minSides = 3;
const std::size_t c_brushPrism_maxSides = c_brush_maxFaces - 2;
const char* const c_brushPrism_name = "brushPrism";

void Brush_ConstructPrism(Brush& brush, const AABB& bounds, std::size_t sides, int axis, const char* shader, const TextureProjection& projection) {
	if (sides < c_brushPrism_minSides) {
		globalErrorStream() << c_brushPrism_name << ": sides " << Unsigned(sides) << ": too few sides, minimum is " << Unsigned(c_brushPrism_minSides) << "\n";
		return;
	}
	if (sides > c_brushPrism_maxSides) {
		globalErrorStream() << c_brushPrism_name << ": sides " << Unsigned(sides) << ": too many sides, maximum is " << Unsigned(c_brushPrism_maxSides) << "\n";
		return;
	}

	brush.clear();
	brush.reserve(sides + 2);

	Vector3 mins(vector3_subtracted(bounds.origin, bounds.extents));
	Vector3 maxs(vector3_added(bounds.origin, bounds.extents));

	float radius = max_extent_2d(bounds.extents, axis);
	const Vector3& mid = bounds.origin;
	Vector3 planepts[3];

	planepts[2][(axis+1)%3] = mins[(axis+1)%3];
	planepts[2][(axis+2)%3] = mins[(axis+2)%3];
	planepts[2][axis] = maxs[axis];
	planepts[1][(axis+1)%3] = maxs[(axis+1)%3];
	planepts[1][(axis+2)%3] = mins[(axis+2)%3];
	planepts[1][axis] = maxs[axis];
	planepts[0][(axis+1)%3] = maxs[(axis+1)%3];
	planepts[0][(axis+2)%3] = maxs[(axis+2)%3];
	planepts[0][axis] = maxs[axis];

	brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);

	planepts[0][(axis+1)%3] = mins[(axis+1)%3];
	planepts[0][(axis+2)%3] = mins[(axis+2)%3];
	planepts[0][axis] = mins[axis];
	planepts[1][(axis+1)%3] = maxs[(axis+1)%3];
	planepts[1][(axis+2)%3] = mins[(axis+2)%3];
	planepts[1][axis] = mins[axis];
	planepts[2][(axis+1)%3] = maxs[(axis+1)%3];
	planepts[2][(axis+2)%3] = maxs[(axis+2)%3];
	planepts[2][axis] = mins[axis];

	brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);

	for (std::size_t i = 0 ; i < sides ; ++i) {
		double sv = sin (i * 3.14159265 * 2 / sides);
		double cv = cos (i * 3.14159265 * 2 / sides);

		planepts[0][(axis+1)%3] = static_cast<float>(floor(mid[(axis+1)%3] + radius * cv + 0.5));
		planepts[0][(axis+2)%3] = static_cast<float>(floor(mid[(axis+2)%3] + radius * sv + 0.5));
		planepts[0][axis] = mins[axis];

		planepts[1][(axis+1)%3] = planepts[0][(axis+1)%3];
		planepts[1][(axis+2)%3] = planepts[0][(axis+2)%3];
		planepts[1][axis] = maxs[axis];

		planepts[2][(axis+1)%3] = static_cast<float>(floor(planepts[0][(axis+1)%3] - radius * sv + 0.5));
		planepts[2][(axis+2)%3] = static_cast<float>(floor(planepts[0][(axis+2)%3] + radius * cv + 0.5));
		planepts[2][axis] = maxs[axis];

		brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
	}
}

const std::size_t c_brushCone_minSides = 3;
const std::size_t c_brushCone_maxSides = 32;
const char* const c_brushCone_name = "brushCone";

void Brush_ConstructCone(Brush& brush, const AABB& bounds, std::size_t sides, const char* shader, const TextureProjection& projection) {
	if (sides < c_brushCone_minSides) {
		globalErrorStream() << c_brushCone_name << ": sides " << Unsigned(sides) << ": too few sides, minimum is " << Unsigned(c_brushCone_minSides) << "\n";
		return;
	}
	if (sides > c_brushCone_maxSides) {
		globalErrorStream() << c_brushCone_name << ": sides " << Unsigned(sides) << ": too many sides, maximum is " << Unsigned(c_brushCone_maxSides) << "\n";
		return;
	}

	brush.clear();
	brush.reserve(sides + 1);

	Vector3 mins(vector3_subtracted(bounds.origin, bounds.extents));
	Vector3 maxs(vector3_added(bounds.origin, bounds.extents));

	float radius = max_extent(bounds.extents);
	const Vector3& mid = bounds.origin;
	Vector3 planepts[3];

	planepts[0][0] = mins[0];
	planepts[0][1] = mins[1];
	planepts[0][2] = mins[2];
	planepts[1][0] = maxs[0];
	planepts[1][1] = mins[1];
	planepts[1][2] = mins[2];
	planepts[2][0] = maxs[0];
	planepts[2][1] = maxs[1];
	planepts[2][2] = mins[2];

	brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);

	for (std::size_t i = 0 ; i < sides ; ++i) {
		double sv = sin (i * 3.14159265 * 2 / sides);
		double cv = cos (i * 3.14159265 * 2 / sides);

		planepts[0][0] = static_cast<float>(floor(mid[0] + radius * cv + 0.5));
		planepts[0][1] = static_cast<float>(floor(mid[1] + radius * sv + 0.5));
		planepts[0][2] = mins[2];

		planepts[1][0] = mid[0];
		planepts[1][1] = mid[1];
		planepts[1][2] = maxs[2];

		planepts[2][0] = static_cast<float>(floor(planepts[0][0] - radius * sv + 0.5));
		planepts[2][1] = static_cast<float>(floor(planepts[0][1] + radius * cv + 0.5));
		planepts[2][2] = maxs[2];

		brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
	}
}

const std::size_t c_brushSphere_minSides = 3;
const std::size_t c_brushSphere_maxSides = 7;
const char* const c_brushSphere_name = "brushSphere";

void Brush_ConstructSphere(Brush& brush, const AABB& bounds, std::size_t sides, const char* shader, const TextureProjection& projection) {
	if (sides < c_brushSphere_minSides) {
		globalErrorStream() << c_brushSphere_name << ": sides " << Unsigned(sides) << ": too few sides, minimum is " << Unsigned(c_brushSphere_minSides) << "\n";
		return;
	}
	if (sides > c_brushSphere_maxSides) {
		globalErrorStream() << c_brushSphere_name << ": sides " << Unsigned(sides) << ": too many sides, maximum is " << Unsigned(c_brushSphere_maxSides) << "\n";
		return;
	}

	brush.clear();
	brush.reserve(sides*sides);

	float radius = max_extent(bounds.extents);
	const Vector3& mid = bounds.origin;
	Vector3 planepts[3];

	double dt = 2 * c_pi / sides;
	double dp = c_pi / sides;
	for (std::size_t i = 0; i < sides; i++) {
		for (std::size_t j = 0;j < sides - 1; j++) {
			double t = i * dt;
			double p = float(j * dp - c_pi / 2);

			planepts[0] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t, p), radius));
			planepts[1] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t, p + dp), radius));
			planepts[2] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t + dt, p + dp), radius));

			brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
		}
	}

	{
		double p = (sides - 1) * dp - c_pi / 2;
		for (std::size_t i = 0; i < sides; i++) {
			double t = i * dt;

			planepts[0] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t, p), radius));
			planepts[1] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t + dt, p + dp), radius));
			planepts[2] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t + dt, p), radius));

			brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
		}
	}
}

int GetViewAxis() {
	switch (GlobalXYWnd_getCurrentViewType()) {
	case XY:
		return 2;
	case XZ:
		return 1;
	case YZ:
		return 0;
	}
	return 2;
}

void Brush_ConstructPrefab(Brush& brush, EBrushPrefab type, const AABB& bounds, std::size_t sides, const char* shader, const TextureProjection& projection) {
	switch (type) {
	case eBrushCuboid: {
		UndoableCommand undo("brushCuboid");

		Brush_ConstructCuboid(brush, bounds, shader, projection);
	}
	break;
	case eBrushPrism: {
		int axis = GetViewAxis();
		StringOutputStream command;
		command << c_brushPrism_name << " -sides " << Unsigned(sides) << " -axis " << axis;
		UndoableCommand undo(command.c_str());

		Brush_ConstructPrism(brush, bounds, sides, axis, shader, projection);
	}
	break;
	case eBrushCone: {
		StringOutputStream command;
		command << c_brushCone_name << " -sides " << Unsigned(sides);
		UndoableCommand undo(command.c_str());

		Brush_ConstructCone(brush, bounds, sides, shader, projection);
	}
	break;
	case eBrushSphere: {
		StringOutputStream command;
		command << c_brushSphere_name << " -sides " << Unsigned(sides);
		UndoableCommand undo(command.c_str());

		Brush_ConstructSphere(brush, bounds, sides, shader, projection);
	}
	break;
	}
}


void ConstructRegionBrushes(scene::Node* brushes[6], const Vector3& region_mins, const Vector3& region_maxs) {
	{
		// set mins
		Vector3 mins(region_mins[0] - 32, region_mins[1] - 32, region_mins[2] - 32);

		// vary maxs
		for (std::size_t i = 0; i < 3; i++) {
			Vector3 maxs(region_maxs[0] + 32, region_maxs[1] + 32, region_maxs[2] + 32);
			maxs[i] = region_mins[i];
			Brush_ConstructCuboid(*Node_getBrush(*brushes[i]), aabb_for_minmax(mins, maxs), texdef_name_default(), TextureProjection());
		}
	}

	{
		// set maxs
		Vector3 maxs(region_maxs[0] + 32, region_maxs[1] + 32, region_maxs[2] + 32);

		// vary mins
		for (std::size_t i = 0; i < 3; i++) {
			Vector3 mins(region_mins[0] - 32, region_mins[1] - 32, region_mins[2] - 32);
			mins[i] = region_maxs[i];
			Brush_ConstructCuboid(*Node_getBrush(*brushes[i+3]), aabb_for_minmax(mins, maxs), texdef_name_default(), TextureProjection());
		}
	}
}


class FaceSetTexdef {
	const TextureProjection& m_projection;
public:
	FaceSetTexdef(const TextureProjection& projection) : m_projection(projection) {
	}
	void operator()(Face& face) const {
		face.SetTexdef(m_projection);
	}
};

void Scene_BrushSetTexdef_Selected(scene::Graph& graph, const TextureProjection& projection) {
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetTexdef(projection));
	SceneChangeNotify();
}

void Scene_BrushSetTexdef_Component_Selected(scene::Graph& graph, const TextureProjection& projection) {
	Scene_ForEachSelectedBrushFace(graph, FaceSetTexdef(projection));
	SceneChangeNotify();
}


class FaceSetFlags {
	const ContentsFlagsValue& m_projection;
public:
	FaceSetFlags(const ContentsFlagsValue& flags) : m_projection(flags) {
	}
	void operator()(Face& face) const {
		face.SetFlags(m_projection);
	}
};

void Scene_BrushSetFlags_Selected(scene::Graph& graph, const ContentsFlagsValue& flags) {
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetFlags(flags));
	SceneChangeNotify();
}

void Scene_BrushSetFlags_Component_Selected(scene::Graph& graph, const ContentsFlagsValue& flags) {
	Scene_ForEachSelectedBrushFace(graph, FaceSetFlags(flags));
	SceneChangeNotify();
}

class FaceShiftTexdef {
	float m_s, m_t;
public:
	FaceShiftTexdef(float s, float t) : m_s(s), m_t(t) {
	}
	void operator()(Face& face) const {
		face.ShiftTexdef(m_s, m_t);
	}
};

void Scene_BrushShiftTexdef_Selected(scene::Graph& graph, float s, float t) {
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceShiftTexdef(s, t));
	SceneChangeNotify();
}

void Scene_BrushShiftTexdef_Component_Selected(scene::Graph& graph, float s, float t) {
	Scene_ForEachSelectedBrushFace(graph, FaceShiftTexdef(s, t));
	SceneChangeNotify();
}

class FaceScaleTexdef {
	float m_s, m_t;
public:
	FaceScaleTexdef(float s, float t) : m_s(s), m_t(t) {
	}
	void operator()(Face& face) const {
		face.ScaleTexdef(m_s, m_t);
	}
};

void Scene_BrushScaleTexdef_Selected(scene::Graph& graph, float s, float t) {
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceScaleTexdef(s, t));
	SceneChangeNotify();
}

void Scene_BrushScaleTexdef_Component_Selected(scene::Graph& graph, float s, float t) {
	Scene_ForEachSelectedBrushFace(graph, FaceScaleTexdef(s, t));
	SceneChangeNotify();
}

class FaceRotateTexdef {
	float m_angle;
public:
	FaceRotateTexdef(float angle) : m_angle(angle) {
	}
	void operator()(Face& face) const {
		face.RotateTexdef(m_angle);
	}
};

void Scene_BrushRotateTexdef_Selected(scene::Graph& graph, float angle) {
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceRotateTexdef(angle));
	SceneChangeNotify();
}

void Scene_BrushRotateTexdef_Component_Selected(scene::Graph& graph, float angle) {
	Scene_ForEachSelectedBrushFace(graph, FaceRotateTexdef(angle));
	SceneChangeNotify();
}


class FaceSetShader {
	const char* m_name;
public:
	FaceSetShader(const char* name) : m_name(name) {}
	void operator()(Face& face) const {
		face.SetShader(m_name);
	}
};

void Scene_BrushSetShader_Selected(scene::Graph& graph, const char* name) {
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetShader(name));
	SceneChangeNotify();
}

void Scene_BrushSetShader_Component_Selected(scene::Graph& graph, const char* name) {
	Scene_ForEachSelectedBrushFace(graph, FaceSetShader(name));
	SceneChangeNotify();
}

class FaceSetDetail {
	bool m_detail;
public:
	FaceSetDetail(bool detail) : m_detail(detail) {
	}
	void operator()(Face& face) const {
		face.setDetail(m_detail);
	}
};

void Scene_BrushSetDetail_Selected(scene::Graph& graph, bool detail) {
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceSetDetail(detail));
	SceneChangeNotify();
}

bool Face_FindReplaceShader(Face& face, const char* find, const char* replace) {
	if (shader_equal(face.GetShader(), find)) {
		face.SetShader(replace);
		return true;
	}
	return false;
}

class FaceFindReplaceShader {
	const char* m_find;
	const char* m_replace;
public:
	FaceFindReplaceShader(const char* find, const char* replace) : m_find(find), m_replace(replace) {
	}
	void operator()(Face& face) const {
		Face_FindReplaceShader(face, m_find, m_replace);
	}
};

class FaceFindShader {
	const char* m_find;
	const char* m_replace;
public:
	FaceFindShader(const char* find) : m_find(find) {
	}
	void operator()(FaceInstance& faceinst) const {
		if (shader_equal(faceinst.getFace().GetShader(), m_find)) {
			faceinst.setSelected(SelectionSystem::eFace, true);
		}
	}
};

bool DoingSearch(const char *repl) {
	return (repl == NULL || (strcmp("textures/", repl) == 0));
}

void Scene_BrushFindReplaceShader(scene::Graph& graph, const char* find, const char* replace) {
	if (DoingSearch(replace)) {
		Scene_ForEachBrush_ForEachFaceInstance(graph, FaceFindShader(find));
	} else {
		Scene_ForEachBrush_ForEachFace(graph, FaceFindReplaceShader(find, replace));
	}
}

void Scene_BrushFindReplaceShader_Selected(scene::Graph& graph, const char* find, const char* replace) {
	if (DoingSearch(replace)) {
		Scene_ForEachSelectedBrush_ForEachFaceInstance(graph,
		        FaceFindShader(find));
	} else {
		Scene_ForEachSelectedBrush_ForEachFace(graph,
		                                       FaceFindReplaceShader(find, replace));
	}
}

// TODO: find for components
// d1223m: dont even know what they are...
void Scene_BrushFindReplaceShader_Component_Selected(scene::Graph& graph, const char* find, const char* replace) {
	if (DoingSearch(replace)) {

	} else {
		Scene_ForEachSelectedBrushFace(graph, FaceFindReplaceShader(find, replace));
	}
}


class FaceFitTexture {
	float m_s_repeat, m_t_repeat;
public:
	FaceFitTexture(float s_repeat, float t_repeat) : m_s_repeat(s_repeat), m_t_repeat(t_repeat) {
	}
	void operator()(Face& face) const {
		face.FitTexture(m_s_repeat, m_t_repeat);
	}
};

void Scene_BrushFitTexture_Selected(scene::Graph& graph, float s_repeat, float t_repeat) {
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceFitTexture(s_repeat, t_repeat));
	SceneChangeNotify();
}

void Scene_BrushFitTexture_Component_Selected(scene::Graph& graph, float s_repeat, float t_repeat) {
	Scene_ForEachSelectedBrushFace(graph, FaceFitTexture(s_repeat, t_repeat));
	SceneChangeNotify();
}

TextureProjection g_defaultTextureProjection;
const TextureProjection& TextureTransform_getDefault() {
	TexDef_Construct_Default(g_defaultTextureProjection);
	return g_defaultTextureProjection;
}

void Scene_BrushConstructPrefab(scene::Graph& graph, EBrushPrefab type, std::size_t sides, const char* shader) {
	if (GlobalSelectionSystem().countSelected() != 0) {
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();

		Brush* brush = Node_getBrush(path.top());
		if (brush != 0) {
			AABB bounds = brush->localAABB(); // copy bounds because the brush will be modified
			Brush_ConstructPrefab(*brush, type, bounds, sides, shader, TextureTransform_getDefault());
			SceneChangeNotify();
		}
	}
}

void Scene_BrushResize_Selected(scene::Graph& graph, const AABB& bounds, const char* shader) {
	if (GlobalSelectionSystem().countSelected() != 0) {
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();

		Brush* brush = Node_getBrush(path.top());
		if (brush != 0) {
			Brush_ConstructCuboid(*brush, bounds, shader, TextureTransform_getDefault());
			SceneChangeNotify();
		}
	}
}

bool Brush_hasShader(const Brush& brush, const char* name) {
	for (Brush::const_iterator i = brush.begin(); i != brush.end(); ++i) {
		if (shader_equal((*i)->GetShader(), name)) {
			return true;
		}
	}
	return false;
}

class BrushSelectByShaderWalker : public scene::Graph::Walker {
	const char* m_name;
public:
	BrushSelectByShaderWalker(const char* name)
			: m_name(name) {
	}
	bool pre(const scene::Path& path, scene::Instance& instance) const {
		if (path.top().get().visible()) {
			Brush* brush = Node_getBrush(path.top());
			if (brush != 0 && Brush_hasShader(*brush, m_name)) {
				Instance_getSelectable(instance)->setSelected(true);
			}
		}
		return true;
	}
};

void Scene_BrushSelectByShader(scene::Graph& graph, const char* name) {
	graph.traverse(BrushSelectByShaderWalker(name));
}

class FaceSelectByShader {
	const char* m_name;
public:
	FaceSelectByShader(const char* name)
			: m_name(name) {
	}
	void operator()(FaceInstance& face) const {
		printf("checking %s = %s\n", face.getFace().GetShader(), m_name);
		if (shader_equal(face.getFace().GetShader(), m_name)) {
			face.setSelected(SelectionSystem::eFace, true);
		}
	}
};

void Scene_BrushSelectByShader_Component(scene::Graph& graph, const char* name) {
	Scene_ForEachSelectedBrush_ForEachFaceInstance(graph, FaceSelectByShader(name));
}

class FaceGetTexdef {
	TextureProjection& m_projection;
	mutable bool m_done;
public:
	FaceGetTexdef(TextureProjection& projection)
			: m_projection(projection), m_done(false) {
	}
	void operator()(Face& face) const {
		if (!m_done) {
			m_done = true;
			face.GetTexdef(m_projection);
		}
	}
};


void Scene_BrushGetTexdef_Selected(scene::Graph& graph, TextureProjection& projection) {
	Scene_ForEachSelectedBrush_ForEachFace(graph, FaceGetTexdef(projection));
}

void Scene_BrushGetTexdef_Component_Selected(scene::Graph& graph, TextureProjection& projection) {
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		faceInstance.getFace().GetTexdef(projection);
	}
}

void Scene_BrushGetShaderSize_Component_Selected(scene::Graph& graph, size_t& width, size_t& height) {
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		width = faceInstance.getFace().getShader().width();
		height = faceInstance.getFace().getShader().height();
	}
}


class FaceGetFlags {
	ContentsFlagsValue& m_flags;
	mutable bool m_done;
public:
	FaceGetFlags(ContentsFlagsValue& flags)
			: m_flags(flags), m_done(false) {
	}
	void operator()(Face& face) const {
		if (!m_done) {
			m_done = true;
			face.GetFlags(m_flags);
		}
	}
};


void Scene_BrushGetFlags_Selected(scene::Graph& graph, ContentsFlagsValue& flags) {
	if (GlobalSelectionSystem().countSelected() != 0) {
		BrushInstance* brush = Instance_getBrush(GlobalSelectionSystem().ultimateSelected());
		if (brush != 0) {
			Brush_forEachFace(*brush, FaceGetFlags(flags));
		}
	}
}

void Scene_BrushGetFlags_Component_Selected(scene::Graph& graph, ContentsFlagsValue& flags) {
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		faceInstance.getFace().GetFlags(flags);
	}
}


class FaceGetShader {
	CopiedString& m_shader;
	mutable bool m_done;
public:
	FaceGetShader(CopiedString& shader)
			: m_shader(shader), m_done(false) {
	}
	void operator()(Face& face) const {
		if (!m_done) {
			m_done = true;
			m_shader = face.GetShader();
		}
	}
};

void Scene_BrushGetShader_Selected(scene::Graph& graph, CopiedString& shader) {
	if (GlobalSelectionSystem().countSelected() != 0) {
		BrushInstance* brush = Instance_getBrush(GlobalSelectionSystem().ultimateSelected());
		if (brush != 0) {
			Brush_forEachFace(*brush, FaceGetShader(shader));
		}
	}
}

void Scene_BrushGetShader_Component_Selected(scene::Graph& graph, CopiedString& shader) {
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		shader = faceInstance.getFace().GetShader();
	}
}


class filter_face_shader : public FaceFilter {
	const char* m_shader;
public:
	filter_face_shader(const char* shader) : m_shader(shader) {
	}
	bool filter(const Face& face) const {
		return shader_equal(face.GetShader(), m_shader);
	}
};

class filter_face_shader_prefix : public FaceFilter {
	const char* m_prefix;
public:
	filter_face_shader_prefix(const char* prefix) : m_prefix(prefix) {
	}
	bool filter(const Face& face) const {
		return shader_equal_n(face.GetShader(), m_prefix, strlen(m_prefix));
	}
};

class filter_face_flags : public FaceFilter {
	int m_flags;
public:
	filter_face_flags(int flags) : m_flags(flags) {
	}
	bool filter(const Face& face) const {
		return (face.getShader().shaderFlags() & m_flags) != 0;
	}
};

class filter_face_contents : public FaceFilter {
	int m_contents;
public:
	filter_face_contents(int contents) : m_contents(contents) {
	}
	bool filter(const Face& face) const {
		return (face.getShader().m_flags.m_contentFlags & m_contents) != 0;
	}
};



class FaceFilterAny {
	FaceFilter* m_filter;
	bool& m_filtered;
public:
	FaceFilterAny(FaceFilter* filter, bool& filtered) : m_filter(filter), m_filtered(filtered) {
		m_filtered = false;
	}
	void operator()(Face& face) const {
		if (m_filter->filter(face)) {
			m_filtered = true;
		}
	}
};

class filter_brush_any_face : public BrushFilter {
	FaceFilter* m_filter;
public:
	filter_brush_any_face(FaceFilter* filter) : m_filter(filter) {
	}
	bool filter(const Brush& brush) const {
		bool filtered;
		Brush_forEachFace(brush, FaceFilterAny(m_filter, filtered));
		return filtered;
	}
};

class FaceFilterAll {
	FaceFilter* m_filter;
	bool& m_filtered;
public:
	FaceFilterAll(FaceFilter* filter, bool& filtered) : m_filter(filter), m_filtered(filtered) {
		m_filtered = true;
	}
	void operator()(Face& face) const {
		if (!m_filter->filter(face)) {
			m_filtered = false;
		}
	}
};

class filter_brush_all_faces : public BrushFilter {
	FaceFilter* m_filter;
public:
	filter_brush_all_faces(FaceFilter* filter) : m_filter(filter) {
	}
	bool filter(const Brush& brush) const {
		bool filtered;
		Brush_forEachFace(brush, FaceFilterAll(m_filter, filtered));
		return filtered;
	}
};


filter_face_flags g_filter_face_clip(QER_CLIP);
filter_brush_all_faces g_filter_brush_clip(&g_filter_face_clip);

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


void BrushFilters_construct() {
	/// @todo Move the ufoai filters
	add_brush_filter(g_filter_brush_clip, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_weapclip, EXCLUDE_CLIP);
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

void Select_MakeDetail() {
	UndoableCommand undo("brushSetDetail");
	Scene_BrushSetDetail_Selected(GlobalSceneGraph(), true);
}

void Select_MakeStructural() {
	UndoableCommand undo("brushClearDetail");
	Scene_BrushSetDetail_Selected(GlobalSceneGraph(), false);
}

class BrushMakeSided {
	std::size_t m_count;
public:
	BrushMakeSided(std::size_t count)
			: m_count(count) {
	}
	void set() {
		Scene_BrushConstructPrefab(GlobalSceneGraph(), eBrushPrism, m_count, TextureBrowser_GetSelectedShader(GlobalTextureBrowser()));
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

inline int axis_for_viewtype(int viewtype) {
	switch (viewtype) {
	case XY:
		return 2;
	case XZ:
		return 1;
	case YZ:
		return 0;
	}
	return 2;
}

class BrushPrefab {
	EBrushPrefab m_type;
public:
	BrushPrefab(EBrushPrefab type)
			: m_type(type) {
	}
	void set() {
		DoSides(m_type, axis_for_viewtype(GetViewAxis()));
	}
	typedef MemberCaller<BrushPrefab, &BrushPrefab::set> SetCaller;
};

BrushPrefab g_brushprism(eBrushPrism);
BrushPrefab g_brushcone(eBrushCone);
BrushPrefab g_brushsphere(eBrushSphere);


void FlipClip();
void SplitClip();
void Clip();
void OnClipMode(bool enable);
bool ClipMode();


void ClipSelected() {
	if (ClipMode()) {
		UndoableCommand undo("clipperClip");
		Clip();
	}
}

void SplitSelected() {
	if (ClipMode()) {
		UndoableCommand undo("clipperSplit");
		SplitClip();
	}
}

void FlipClipper() {
	FlipClip();
}


Callback g_texture_lock_status_changed;
BoolExportCaller g_texdef_movelock_caller(g_brush_texturelock_enabled);
ToggleItem g_texdef_movelock_item(g_texdef_movelock_caller);

void Texdef_ToggleMoveLock() {
	g_brush_texturelock_enabled = !g_brush_texturelock_enabled;
	g_texdef_movelock_item.update();
	g_texture_lock_status_changed();
}





void Brush_registerCommands() {
	GlobalToggles_insert("TogTexLock", FreeCaller<Texdef_ToggleMoveLock>(), ToggleItem::AddCallbackCaller(g_texdef_movelock_item), Accelerator('T', (GdkModifierType)GDK_SHIFT_MASK));

	GlobalCommands_insert("BrushPrism", BrushPrefab::SetCaller(g_brushprism));
	GlobalCommands_insert("BrushCone", BrushPrefab::SetCaller(g_brushcone));
	GlobalCommands_insert("BrushSphere", BrushPrefab::SetCaller(g_brushsphere));

	GlobalCommands_insert("Brush3Sided", BrushMakeSided::SetCaller(g_brushmakesided3), Accelerator('3', (GdkModifierType)GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush4Sided", BrushMakeSided::SetCaller(g_brushmakesided4), Accelerator('4', (GdkModifierType)GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush5Sided", BrushMakeSided::SetCaller(g_brushmakesided5), Accelerator('5', (GdkModifierType)GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush6Sided", BrushMakeSided::SetCaller(g_brushmakesided6), Accelerator('6', (GdkModifierType)GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush7Sided", BrushMakeSided::SetCaller(g_brushmakesided7), Accelerator('7', (GdkModifierType)GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush8Sided", BrushMakeSided::SetCaller(g_brushmakesided8), Accelerator('8', (GdkModifierType)GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush9Sided", BrushMakeSided::SetCaller(g_brushmakesided9), Accelerator('9', (GdkModifierType)GDK_CONTROL_MASK));

	GlobalCommands_insert("ClipSelected", FreeCaller<ClipSelected>(), Accelerator(GDK_Return));
	GlobalCommands_insert("SplitSelected", FreeCaller<SplitSelected>(), Accelerator(GDK_Return, (GdkModifierType)GDK_SHIFT_MASK));
	GlobalCommands_insert("FlipClip", FreeCaller<FlipClipper>(), Accelerator(GDK_Return, (GdkModifierType)GDK_CONTROL_MASK));

	GlobalCommands_insert("MakeDetail", FreeCaller<Select_MakeDetail>(), Accelerator('M', (GdkModifierType)GDK_CONTROL_MASK));
	GlobalCommands_insert("MakeStructural", FreeCaller<Select_MakeStructural>(), Accelerator('S', (GdkModifierType)(GDK_SHIFT_MASK | GDK_CONTROL_MASK)));
}

void Brush_constructMenu(GtkMenu* menu) {
	create_menu_item_with_mnemonic(menu, "Prism...", "BrushPrism");
	create_menu_item_with_mnemonic(menu, "Cone...", "BrushCone");
	create_menu_item_with_mnemonic(menu, "Sphere...", "BrushSphere");
	menu_separator (menu);
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic (menu, "CSG");
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff (menu_in_menu);
		create_menu_item_with_mnemonic(menu_in_menu, "Make _Hollow", "CSGHollow");
		create_menu_item_with_mnemonic(menu_in_menu, "CSG _Subtract", "CSGSubtract");
		create_menu_item_with_mnemonic(menu_in_menu, "CSG _Merge", "CSGMerge");
	}
	menu_separator(menu);
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic (menu, "Clipper");
		if (g_Layout_enableDetachableMenus.m_value)
			menu_tearoff (menu_in_menu);

		create_menu_item_with_mnemonic(menu_in_menu, "Clip selection", "ClipSelected");
		create_menu_item_with_mnemonic(menu_in_menu, "Split selection", "SplitSelected");
		create_menu_item_with_mnemonic(menu_in_menu, "Flip Clip orientation", "FlipClip");
	}
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, "Make detail", "MakeDetail");
	create_menu_item_with_mnemonic(menu, "Make structural", "MakeStructural");

	create_check_menu_item_with_mnemonic(menu, "Texture Lock", "TogTexLock");
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, "Copy Face Texture", "FaceCopyTexture");
	create_menu_item_with_mnemonic(menu, "Paste Face Texture", "FacePasteTexture");

	command_connect_accelerator("Brush3Sided");
	command_connect_accelerator("Brush4Sided");
	command_connect_accelerator("Brush5Sided");
	command_connect_accelerator("Brush6Sided");
	command_connect_accelerator("Brush7Sided");
	command_connect_accelerator("Brush8Sided");
	command_connect_accelerator("Brush9Sided");
}
