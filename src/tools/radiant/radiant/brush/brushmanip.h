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

#if !defined (INCLUDED_BRUSHWRAPPER_H)
#define INCLUDED_BRUSHWRAPPER_H

#include <cstddef>
#include <string>
#include "generic/callbackfwd.h"
#include "brush.h"

enum EBrushPrefab
{
	eBrushPrism, eBrushCone, eBrushSphere, eBrushRock, eBrushTerrain
};

class TextureProjection;
class ContentsFlagsValue;

class FaceGetFlags
{
		ContentsFlagsValue& m_flags;
	public:
		FaceGetFlags (ContentsFlagsValue& flags) :
			m_flags(flags)
		{
		}
		void operator() (Face& face) const;
};

namespace scene
{
	class Graph;
}
void Scene_BrushConstructPrefab (scene::Graph& graph, EBrushPrefab type, std::size_t sides, const std::string& shader);
class AABB;
void Scene_BrushResize_Selected (scene::Graph& graph, const AABB& bounds, const std::string& shader);
void Scene_BrushSetTexdef_Selected (scene::Graph& graph, const TextureProjection& projection);
void Scene_BrushSetTexdef_Component_Selected (scene::Graph& graph, const TextureProjection& projection);
void Scene_BrushGetTexdef_Selected (scene::Graph& graph, TextureProjection& projection);
void Scene_BrushGetTexdef_Component_Selected (scene::Graph& graph, TextureProjection& projection);
void Scene_BrushGetShaderSize_Component_Selected (scene::Graph& graph, size_t& width, size_t& height);
void Scene_BrushSetFlags_Selected (scene::Graph& graph, const ContentsFlagsValue& flags);
void Scene_BrushSetFlags_Component_Selected (scene::Graph& graph, const ContentsFlagsValue& flags);
void Scene_BrushGetFlags_Selected (scene::Graph& graph, ContentsFlagsValue& flags);
void Scene_BrushGetFlags_Component_Selected (scene::Graph& graph, ContentsFlagsValue& flags);
void Scene_BrushShiftTexdef_Selected (scene::Graph& graph, float s, float t);
void Scene_BrushShiftTexdef_Component_Selected (scene::Graph& graph, float s, float t);
void Scene_BrushScaleTexdef_Selected (scene::Graph& graph, float s, float t);
void Scene_BrushScaleTexdef_Component_Selected (scene::Graph& graph, float s, float t);
void Scene_BrushRotateTexdef_Selected (scene::Graph& graph, float angle);
void Scene_BrushRotateTexdef_Component_Selected (scene::Graph& graph, float angle);
void Scene_BrushSetShader_Selected (scene::Graph& graph, const std::string& name);
void Scene_BrushSetShader_Component_Selected (scene::Graph& graph, const std::string& name);
void Scene_BrushGetShader_Selected (scene::Graph& graph, std::string& shader);
void Scene_BrushGetShader_Component_Selected (scene::Graph& graph, std::string& shader);
void Scene_BrushSelectByShader (scene::Graph& graph, const std::string& shader);
void Scene_BrushSelectByShader_Component (scene::Graph& graph, const std::string& name);
void Scene_BrushFacesSelectByShader_Component (scene::Graph& graph, const std::string& shader);
void Scene_BrushFitTexture_Selected (scene::Graph& graph, float s_repeat, float t_repeat);
void Scene_BrushFitTexture_Component_Selected (scene::Graph& graph, float s_repeat, float t_repeat);
void Scene_BrushFlipTexture_Selected(unsigned int flipAxis);
void Scene_BrushFlipTexture_Component_Selected(unsigned int flipAxis);

void Brush_registerCommands ();

#endif
