#include "GlobalXYWnd.h"

#include "radiant_i18n.h"

// Constructor
XYWndManager::XYWndManager() :
	_activeXY(NULL)
{
	// Connect self to the according registry keys
	GlobalRegistry().addKeyObserver(this, RKEY_CHASE_MOUSE);
	GlobalRegistry().addKeyObserver(this, RKEY_CAMERA_XY_UPDATE);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_CROSSHAIRS);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_GRID);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_SIZE_INFO);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_ENTITY_ANGLES);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_ENTITY_NAMES);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_BLOCKS);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_COORDINATES);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_OUTLINE);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_AXES);
	GlobalRegistry().addKeyObserver(this, RKEY_SHOW_WORKZONE);
	GlobalRegistry().addKeyObserver(this, RKEY_DEFAULT_BLOCKSIZE);

	// Trigger loading the values of the observed registry keys
	keyChanged();

	// greebo: Register this class in the preference system so that the constructPreferencePage() gets called.
	GlobalPreferenceSystem().addConstructor(this);
}

// Destructor
XYWndManager::~XYWndManager() {
	destroy();
}

void XYWndManager::constructPreferencePage(PreferenceGroup& group) {
	PreferencesPage* page(group.createPage(_("Orthographic"), _("Orthographic View Settings")));

	page->appendCheckBox("", _("View chases mouse cursor during drags"), RKEY_CHASE_MOUSE);
	page->appendCheckBox("", _("Update views on camera move"), RKEY_CAMERA_XY_UPDATE);
	page->appendCheckBox("", _("Show Crosshairs"), RKEY_SHOW_CROSSHAIRS);
	page->appendCheckBox("", _("Show Grid"), RKEY_SHOW_GRID);
	page->appendCheckBox("", _("Show Size Info"), RKEY_SHOW_SIZE_INFO);
	page->appendCheckBox("", _("Show Entity Angle Arrow"), RKEY_SHOW_ENTITY_ANGLES);
	page->appendCheckBox("", _("Show Entity Names"), RKEY_SHOW_ENTITY_NAMES);
	page->appendCheckBox("", _("Show Blocks"), RKEY_SHOW_BLOCKS);
	page->appendCheckBox("", _("Show Coordinates"), RKEY_SHOW_COORDINATES);
	page->appendCheckBox("", _("Show Axes"), RKEY_SHOW_AXES);
	page->appendCheckBox("", _("Show Window Outline"), RKEY_SHOW_OUTLINE);
	page->appendCheckBox("", _("Show Workzone"), RKEY_SHOW_WORKZONE);
}

// Load/Reload the values from the registry
void XYWndManager::keyChanged() {
	_chaseMouse = (GlobalRegistry().get(RKEY_CHASE_MOUSE) == "1");
	_camXYUpdate = (GlobalRegistry().get(RKEY_CAMERA_XY_UPDATE) == "1");
	_showCrossHairs = (GlobalRegistry().get(RKEY_SHOW_CROSSHAIRS) == "1");
	_showGrid = (GlobalRegistry().get(RKEY_SHOW_GRID) == "1");
	_showSizeInfo = (GlobalRegistry().get(RKEY_SHOW_SIZE_INFO) == "1");
	_showBlocks = (GlobalRegistry().get(RKEY_SHOW_BLOCKS) == "1");
	_showCoordinates = (GlobalRegistry().get(RKEY_SHOW_COORDINATES) == "1");
	_showOutline = (GlobalRegistry().get(RKEY_SHOW_OUTLINE) == "1");
	_showAxes = (GlobalRegistry().get(RKEY_SHOW_AXES) == "1");
	_showWorkzone = (GlobalRegistry().get(RKEY_SHOW_WORKZONE) == "1");
	_defaultBlockSize = (GlobalRegistry().getInt(RKEY_DEFAULT_BLOCKSIZE));

	updateAllViews();
}

bool XYWndManager::chaseMouse() const {
	return _chaseMouse;
}

bool XYWndManager::camXYUpdate() const {
	return _camXYUpdate;
}

bool XYWndManager::showCrossHairs() const {
	return _showCrossHairs;
}

void XYWndManager::toggleCrossHairs() {
	// Invert the registry value, the _showCrossHairs bool is updated automatically as this class observes the key
	GlobalRegistry().set(RKEY_SHOW_CROSSHAIRS, _showCrossHairs ? "0" : "1");
	updateAllViews();
}

bool XYWndManager::showBlocks() const {
	return _showBlocks;
}

unsigned int XYWndManager::defaultBlockSize() const {
	return _defaultBlockSize;
}

bool XYWndManager::showCoordinates() const {
	return _showCoordinates;
}

bool XYWndManager::showOutline() const  {
	return _showOutline;
}

bool XYWndManager::showAxes() const {
	return _showAxes;
}

bool XYWndManager::showWorkzone() const {
	return _showWorkzone;
}

bool XYWndManager::showGrid() const {
	return _showGrid;
}

void XYWndManager::toggleGrid() {
	// Invert the registry value, the _showCrossHairs bool is updated automatically as this class observes the key
	GlobalRegistry().set(RKEY_SHOW_GRID, _showGrid ? "0" : "1");
	updateAllViews();
}

void XYWndManager::updateAllViews() {
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); i++) {
		XYWnd* xyview = *i;

		// Pass the call
		xyview->queueDraw();
	}
}

void XYWndManager::zoomIn() {
	if (_activeXY != NULL) {
		_activeXY->zoomIn();
	}
}

void XYWndManager::zoomOut() {
	if (_activeXY != NULL) {
		_activeXY->zoomOut();
	}
}

void XYWndManager::resetZoom() {
	GlobalXYWnd().setScale(1);
}

// Free the allocated XYViews from the heap
void XYWndManager::destroy() {
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); i++) {
		// Free the view from the heap
		XYWnd* xyView = *i;
		delete xyView;

	}
	// Discard the whole list
	_XYViews.clear();
}

XYWnd* XYWndManager::getActiveXY() const {
	return _activeXY;
}

void XYWndManager::setOrigin(const Vector3& origin) {
	// Cycle through the list of views and set the origin
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); i++) {
		XYWnd* xyView = *i;

		if (xyView != NULL) {
			// Pass the call
			xyView->setOrigin(origin);
		}
	}
}

void XYWndManager::setScale(float scale) {
	// Cycle through the list of views and set the origin
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); i++) {
		XYWnd* xyView = *i;

		if (xyView != NULL) {
			// Pass the call
			xyView->setScale(scale);
		}
	}
}

void XYWndManager::positionAllViews(const Vector3& origin) {
	// Cycle through the list of views and set the origin
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); i++) {
		XYWnd* xyView = *i;

		if (xyView != NULL) {
			// Pass the call
			xyView->positionView(origin);
		}
	}
}

void XYWndManager::positionView(const Vector3& origin) {
	if (_activeXY != NULL) {
		return _activeXY->positionView(origin);
	}
}

EViewType XYWndManager::getActiveViewType() const {
	if (_activeXY != NULL) {
		return _activeXY->getViewType();
	}
	// Return at least anything
	return XY;
}

void XYWndManager::setActiveViewType(EViewType viewType) {
	if (_activeXY != NULL) {
		return _activeXY->setViewType(viewType);
	}
}

void XYWndManager::toggleActiveView() {
	if (_activeXY != NULL) {
		if (_activeXY->getViewType() == XY) {
			_activeXY->setViewType(XZ);
		}
		else if (_activeXY->getViewType() == XZ) {
			_activeXY->setViewType(YZ);
		}
		else {
			_activeXY->setViewType(XY);
		}
	}
}

XYWnd* XYWndManager::getView(EViewType viewType) {
	// Cycle through the list of views and get the one matching the type
	for (XYWndList::iterator i = _XYViews.begin(); i != _XYViews.end(); i++) {
		XYWnd* xyView = *i;

		if (xyView != NULL) {
			// If the view matches, return the pointer
			if (xyView->getViewType() == viewType) {
				return xyView;
			}
		}
	}

	return NULL;
}

void XYWndManager::setActiveXY(XYWnd* wnd) {
	// Notify the currently active XYView that is has been deactivated
	if (_activeXY != NULL) {
		_activeXY->setActive(false);
	}

	// Update the pointer
	_activeXY = wnd;

	// Notify the new active XYView about its activation
	if (_activeXY != NULL) {
		_activeXY->setActive(true);
	}
}

XYWnd* XYWndManager::createXY() {
	// Allocate a new window
	XYWnd* newWnd = new XYWnd();

	// Add it to the internal list and return the pointer
	_XYViews.push_back(newWnd);

	return newWnd;
}

// Accessor function returning a reference to the static instance
XYWndManager& GlobalXYWnd() {
	static XYWndManager _xyWndManager;
	return _xyWndManager;
}
