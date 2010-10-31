#ifndef CAMERASETTINGS_H_
#define CAMERASETTINGS_H_

#include <string>
#include "iregistry.h"
#include "gtkutil/widget.h"
#include "preferencesystem.h"

namespace {
const int MAX_CUBIC_SCALE = 23;

const std::string RKEY_MOVEMENT_SPEED = "user/ui/camera/movementSpeed";
const std::string RKEY_ROTATION_SPEED = "user/ui/camera/rotationSpeed";
const std::string RKEY_INVERT_MOUSE_VERTICAL_AXIS = "user/ui/camera/invertMouseVerticalAxis";
const std::string RKEY_DISCRETE_MOVEMENT = "user/ui/camera/discreteMovement";
const std::string RKEY_CUBIC_SCALE = "user/ui/camera/cubicScale";
const std::string RKEY_ENABLE_FARCLIP = "user/ui/camera/enableCubicClipping";
const std::string RKEY_DRAWMODE = "user/ui/camera/drawMode";
const std::string RKEY_SOLID_SELECTION_BOXES = "user/ui/xyview/solidSelectionBoxes";

enum CameraDrawMode
{
	drawWire, drawSolid, drawTexture
};

}
/* greebo: This is the home of all the camera settings. As this class derives
 * from a RegistryKeyObserver, it can be connected to the according registry keys
 * and gets notified if any of the observed keys are changed.*/

class CameraSettings: public RegistryKeyObserver, public PreferenceConstructor
{
		bool _callbackActive;

		int _movementSpeed;
		int _angleSpeed;

		bool _invertMouseVerticalAxis;
		bool _discreteMovement;

		CameraDrawMode _cameraDrawMode;

		int _cubicScale;
		bool _farClipEnabled;
		bool _solidSelectionBoxes;

		void farClipExport (const BoolImportCallback& importCallback);

		// Callback and callers for the ToggleItem stuff (farClipPlane)
		MemberCaller1<CameraSettings, const BoolImportCallback&, &CameraSettings::farClipExport> _farClipCaller;
		BoolExportCallback _farClipCallBack;
		ToggleItem _farClipItem;

	public:
		CameraSettings ();

		// The callback that gets called on registry key changes
		void keyChanged ();

		int movementSpeed () const;
		int angleSpeed () const;

		// Returns true if cubic clipping is on
		bool farClipEnabled () const;
		bool invertMouseVerticalAxis () const;
		bool discreteMovement () const;
		bool solidSelectionBoxes () const;

		// Sets/returns the draw mode (wireframe, solid, textured, lighting)
		CameraDrawMode getMode () const;
		void setMode (const CameraDrawMode& mode);

		// Gets/Sets the cubic scale member variable (is automatically constrained [1..MAX_CUBIC_SCALE])
		int cubicScale () const;
		void setCubicScale (const int& scale);

		// Enables/disables the cubic clipping
		void toggleFarClip ();
		void setFarClip (bool farClipEnabled);
		ToggleItem& farClipItem ();

		// PreferenceConstructor implementation, adds the elements to the according preference page
		void constructPreferencePage (PreferenceGroup& group);

	private:
		void importDrawMode (const int mode);
}; // class CameraSettings

CameraSettings* getCameraSettings ();

#endif /*CAMERASETTINGS_H_*/
