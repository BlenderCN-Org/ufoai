/**
 * @file camwindow.cpp
 * @author Leonardo Zide (leo@lokigames.com)
 * @brief Camera Window
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

#include "camwindow.h"
#include "radiant_i18n.h"

#include "debugging/debugging.h"

#include "ientity.h"
#include "iscenegraph.h"
#include "irender.h"
#include "igl.h"
#include "cullable.h"
#include "renderable.h"
#include "preferencesystem.h"

#include "container/array.h"
#include "scenelib.h"
#include "render.h"
#include "math/frustum.h"

#include "gtkutil/widget.h"
#include "gtkutil/button.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/GLWidgetSentry.h"
#include "gtkutil/xorrectangle.h"
#include "../gtkmisc.h"
#include "../selection/RadiantWindowObserver.h"
#include "../mainframe.h"
#include "../settings/preferences.h"
#include "../commands.h"
#include "../xyview/xywindow.h"
#include "../windowobservers.h"
#include "../ui/Icons.h"
#include "../render/RenderStatistics.h"
#include "CamRenderer.h"
#include "../ui/eventmapper/EventMapper.h"

#include "GlobalCamera.h"
#include "Camera.h"
#include "CameraSettings.h"

gboolean camera_keymove (gpointer data)
{
	Camera* cam = reinterpret_cast<Camera*> (data);
	cam->keyMove();
	return TRUE;
}
#include "CamWnd.h"

camwindow_globals_t g_camwindow_globals;

// =============================================================================
// CamWnd class

void CamWnd_registerShortcuts ()
{
	toggle_add_accelerator("ToggleCubicClip");
}

void GlobalCamera_Benchmark ()
{
	GlobalCamera().benchmark();
}

void Camera_constructPreferences (PreferencesPage* page)
{
	// Add the sliders for the movement and angle speed and connect them to the observer
	page->appendSlider(_("Movement Speed (game units)"), RKEY_MOVEMENT_SPEED, TRUE, 100, 50, 300, 1, 10, 10);
	page->appendSlider(_("Rotation Speed"), RKEY_ROTATION_SPEED, TRUE, 3, 1, 180, 1, 10, 10);

	// Add the checkboxes and connect them with the registry key and the according observer
	page->appendCheckBox("", _("Discrete movement (non-freelook mode)"), RKEY_DISCRETE_MOVEMENT);
	page->appendCheckBox("", _("Enable far-clip plane (hides distant objects)"), RKEY_ENABLE_FARCLIP);

	// Add the "inverse mouse vertical axis in free-look mode" preference
	page->appendCheckBox("", _("Invert mouse vertical axis (freelook mode)"), RKEY_INVERT_MOUSE_VERTICAL_AXIS);

	// Create the string list containing the render mode captions
	std::list<std::string> renderModeDescriptions;
	renderModeDescriptions.push_back(_("WireFrame"));
	renderModeDescriptions.push_back(_("Flatshade"));
	renderModeDescriptions.push_back(_("Textured"));

	page->appendCombo("Render Mode", RKEY_DRAWMODE, renderModeDescriptions);
}

void Camera_constructPage (PreferenceGroup& group)
{
	PreferencesPage* page = group.createPage(_("Camera"), _("Camera View Preferences"));
	Camera_constructPreferences(page);
}
void Camera_registerPreferencesPage ()
{
	PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, Camera_constructPage> ());
}

#include "preferencesystem.h"
#include "stringio.h"
#include "../dialog.h"

// greebo: this gets called when the main Radiant class is instantiated. This is _before_ a GlobalCamWnd actually exists.
/// \brief Initialisation for things that have the same lifespan as this module.
void CamWnd_Construct ()
{
	GlobalCommands_insert("CenterView", MemberCaller<GlobalCameraManager, &GlobalCameraManager::resetCameraAngles> (
			GlobalCamera()), Accelerator(GDK_End));
	GlobalToggles_insert("ToggleCubicClip", MemberCaller<CameraSettings, &CameraSettings::toggleFarClip> (
			*getCameraSettings()), ToggleItem::AddCallbackCaller(getCameraSettings()->farClipItem()), Accelerator('\\',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("CubicClipZoomIn", MemberCaller<GlobalCameraManager, &GlobalCameraManager::cubicScaleIn> (
			GlobalCamera()), Accelerator('[', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("CubicClipZoomOut", MemberCaller<GlobalCameraManager, &GlobalCameraManager::cubicScaleOut> (
			GlobalCamera()), Accelerator(']', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("UpFloor", MemberCaller<GlobalCameraManager, &GlobalCameraManager::changeFloorUp> (
			GlobalCamera()), Accelerator(GDK_Prior));
	GlobalCommands_insert("DownFloor", MemberCaller<GlobalCameraManager, &GlobalCameraManager::changeFloorDown> (
			GlobalCamera()), Accelerator(GDK_Prior));
	GlobalToggles_insert("ToggleCamera", ToggleShown::ToggleCaller(GlobalCamera().getToggleShown()),
			ToggleItem::AddCallbackCaller(GlobalCamera().getToggleShown().m_item), Accelerator('C',
					(GdkModifierType) (GDK_SHIFT_MASK | GDK_CONTROL_MASK)));
	GlobalCommands_insert("LookThroughSelected", MemberCaller<GlobalCameraManager,
			&GlobalCameraManager::lookThroughSelected> (GlobalCamera()));
	GlobalCommands_insert("LookThroughCamera", MemberCaller<GlobalCameraManager,
			&GlobalCameraManager::lookThroughCamera> (GlobalCamera()));

	// Insert movement commands
	GlobalCommands_insert("CameraForward",
			MemberCaller<GlobalCameraManager, &GlobalCameraManager::moveForwardDiscrete> (GlobalCamera()), Accelerator(
					GDK_Up));
	GlobalCommands_insert("CameraBack", MemberCaller<GlobalCameraManager, &GlobalCameraManager::moveBackDiscrete> (
			GlobalCamera()), Accelerator(GDK_Down));
	GlobalCommands_insert("CameraLeft", MemberCaller<GlobalCameraManager, &GlobalCameraManager::rotateLeftDiscrete> (
			GlobalCamera()), Accelerator(GDK_Left));
	GlobalCommands_insert("CameraRight", MemberCaller<GlobalCameraManager, &GlobalCameraManager::rotateRightDiscrete> (
			GlobalCamera()), Accelerator(GDK_Right));
	GlobalCommands_insert("CameraStrafeRight", MemberCaller<GlobalCameraManager,
			&GlobalCameraManager::moveRightDiscrete> (GlobalCamera()), Accelerator(GDK_period));
	GlobalCommands_insert("CameraStrafeLeft",
			MemberCaller<GlobalCameraManager, &GlobalCameraManager::moveLeftDiscrete> (GlobalCamera()), Accelerator(
					GDK_comma));
	GlobalCommands_insert("CameraUp", MemberCaller<GlobalCameraManager, &GlobalCameraManager::moveUpDiscrete> (
			GlobalCamera()), Accelerator('D'));
	GlobalCommands_insert("CameraDown", MemberCaller<GlobalCameraManager, &GlobalCameraManager::moveDownDiscrete> (
			GlobalCamera()), Accelerator('C'));
	GlobalCommands_insert("CameraAngleUp", MemberCaller<GlobalCameraManager, &GlobalCameraManager::pitchUpDiscrete> (
			GlobalCamera()), Accelerator('A'));
	GlobalCommands_insert("CameraAngleDown",
			MemberCaller<GlobalCameraManager, &GlobalCameraManager::pitchDownDiscrete> (GlobalCamera()), Accelerator(
					'Z'));

	CamWnd::captureStates();

	Camera_registerPreferencesPage();
}
void CamWnd_Destroy ()
{
	CamWnd::releaseStates();
}
