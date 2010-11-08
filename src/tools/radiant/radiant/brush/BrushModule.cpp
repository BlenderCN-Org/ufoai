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

#include "BrushModule.h"
#include "radiant_i18n.h"

#include "iradiant.h"

#include "TexDef.h"
#include "ibrush.h"
#include "ifilter.h"

#include "BrushNode.h"
#include "brushmanip.h"

#include "preferencesystem.h"
#include "stringio.h"

#include "../map/map.h"
#include "../qe3.h"
#include "../dialog.h"
#include "../mainframe.h"
#include "../settings/preferences.h"

BrushModuleClass::BrushModuleClass():_textureLockEnabled(GlobalRegistry().get(RKEY_ENABLE_TEXTURE_LOCK) == "1")
{
	GlobalRegistry().addKeyObserver(this, RKEY_ENABLE_TEXTURE_LOCK);

	// greebo: Register this class in the preference system so that the constructPreferencePage() gets called.
	GlobalPreferenceSystem().addConstructor(this);
}

void BrushModuleClass::constructPreferencePage (PreferenceGroup& group)
{
	PreferencesPage* page = group.createPage(_("Brush"), _("Brush Settings"));
	// Add the default texture scale preference and connect it to the according registryKey
	// Note: this should be moved somewhere else, I think
	page->appendEntry(_("Default texture scale"), "user/ui/textures/defaultTextureScale");

	// The checkbox to enable/disable the texture lock option
	page->appendCheckBox("", _("Enable Texture Lock"), "user/ui/brush/textureLock");
}

void BrushModuleClass::construct ()
{
	Brush_registerCommands();

	BrushClipPlane::constructStatic();
	BrushInstance::constructStatic();
	Brush::constructStatic();

	Brush::m_maxWorldCoord = GlobalRegistry().getFloat("game/defaults/maxWorldCoord");
	BrushInstance::m_counter = &g_brushCount;
}

void BrushModuleClass::destroy ()
{
	Brush::m_maxWorldCoord = 0;
	BrushInstance::m_counter = 0;

	Brush::destroyStatic();
	BrushInstance::destroyStatic();
	BrushClipPlane::destroyStatic();
}

void BrushModuleClass::clipperColourChanged ()
{
	BrushClipPlane::destroyStatic();
	BrushClipPlane::constructStatic();
}

void BrushFaceData_fromFace (const BrushFaceDataCallback& callback, Face& face)
{
	_QERFaceData faceData;
	faceData.m_p0 = face.getPlane().planePoints()[0];
	faceData.m_p1 = face.getPlane().planePoints()[1];
	faceData.m_p2 = face.getPlane().planePoints()[2];
	faceData.m_shader = face.GetShader();
	faceData.m_texdef = face.getTexdef().m_projection.m_texdef;
	faceData.contents = face.getShader().m_flags.m_contentFlags;
	faceData.flags = face.getShader().m_flags.m_surfaceFlags;
	faceData.value = face.getShader().m_flags.m_value;
	callback(faceData);
}
typedef ConstReferenceCaller1<BrushFaceDataCallback, Face&, BrushFaceData_fromFace> BrushFaceDataFromFaceCaller;
typedef Callback1<Face&> FaceCallback;

scene::Node& BrushModuleClass::createBrush ()
{
	return (new BrushNode)->node();
}
void BrushModuleClass::Brush_forEachFace (scene::Node& brush, const BrushFaceDataCallback& callback)
{
	::Brush_forEachFace(*Node_getBrush(brush), FaceCallback(BrushFaceDataFromFaceCaller(callback)));
}
bool BrushModuleClass::Brush_addFace (scene::Node& brush, const _QERFaceData& faceData)
{
	Node_getBrush(brush)->undoSave();
	return Node_getBrush(brush)->addPlane(faceData.m_p0, faceData.m_p1, faceData.m_p2, faceData.m_shader,
			TextureProjection(faceData.m_texdef)) != 0;
}

void BrushModuleClass::keyChanged() {
	_textureLockEnabled = (GlobalRegistry().get(RKEY_ENABLE_TEXTURE_LOCK) == "1");
}

bool BrushModuleClass::textureLockEnabled() const {
	return _textureLockEnabled;
}

void BrushModuleClass::setTextureLock(bool enabled) {
	// Write the value to the registry, the keyChanged() method is triggered automatically
	GlobalRegistry().set(RKEY_ENABLE_TEXTURE_LOCK, enabled ? "1" : "0");
}

void BrushModuleClass::toggleTextureLock() {
	setTextureLock(!textureLockEnabled());
}

BrushModuleClass* GlobalBrush ()
{
	static BrushModuleClass _brushModule;
	return &_brushModule;
}

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class BrushDependencies: public GlobalRadiantModuleRef,
		public GlobalSceneGraphModuleRef,
		public GlobalShaderCacheModuleRef,
		public GlobalSelectionModuleRef,
		public GlobalOpenGLModuleRef,
		public GlobalUndoModuleRef,
		public GlobalFilterModuleRef
{
};

class BrushUFOAPI: public TypeSystemRef
{
		BrushCreator* m_brushufo;
	public:
		typedef BrushCreator Type;
		STRING_CONSTANT(Name, "*");

		BrushUFOAPI ()
		{
			GlobalBrush()->construct();

			m_brushufo = GlobalBrush();
		}
		~BrushUFOAPI ()
		{
			GlobalBrush()->destroy();
		}
		BrushCreator* getTable ()
		{
			return m_brushufo;
		}
};

typedef SingletonModule<BrushUFOAPI, BrushDependencies> BrushUFOModule;
typedef Static<BrushUFOModule> StaticBrushUFOModule;
StaticRegisterModule staticRegisterBrushUFO(StaticBrushUFOModule::instance());
