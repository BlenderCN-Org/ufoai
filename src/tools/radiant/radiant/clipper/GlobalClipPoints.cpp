#include "GlobalClipPoints.h"

#include "radiant_i18n.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "preferencesystem.h"
#include "stringio.h"

#include "../mainframe.h"
#include "../brush/csg/csg.h"
#include "../sidebar/texturebrowser.h"

ClipPointManager::ClipPointManager () :
	_movingClip(NULL), _switch(true), _useCaulk(false), _caulkShader("textures/tex_common/nodraw")
{
	GlobalPreferenceSystem().registerPreference("ClipNoDraw", BoolImportStringCaller(_useCaulk),
			BoolExportStringCaller(_useCaulk));
}

void ClipPointManager::constructPreferences (PreferencesPage& page)
{
	page.appendCheckBox("", _("Clipper tool uses nodraw"), _useCaulk);
}

void ClipPointManager::constructPreferencePage (PreferenceGroup& group)
{
	PreferencesPage page(group.createPage("Clipper", _("Clipper Tool Settings")));
	constructPreferences(page);
}

void ClipPointManager::registerPreferencesPage ()
{
	PreferencesDialog_addSettingsPage(PreferencePageConstructor(*this));
}

EViewType ClipPointManager::getViewType () const
{
	return _viewType;
}

void ClipPointManager::setViewType (EViewType viewType)
{
	_viewType = viewType;
}

ClipPoint* ClipPointManager::getMovingClip ()
{
	return _movingClip;
}

void ClipPointManager::setMovingClip (ClipPoint* clipPoint)
{
	_movingClip = clipPoint;
}

const std::string ClipPointManager::getShader () const
{
	return (_useCaulk) ? _caulkShader : GlobalTextureBrowser().getSelectedShader();
}

// greebo: Cycles through the three possible clip points and returns the nearest to point (for selectiontest)
ClipPoint* ClipPointManager::find (const Vector3& point, EViewType viewtype, float scale)
{
	double bestDistance = FLT_MAX;

	ClipPoint* bestClip = NULL;

	for (unsigned int i = 0; i < NUM_CLIP_POINTS; i++) {
		_clipPoints[i].testSelect(point, viewtype, scale, bestDistance, bestClip);
	}

	return bestClip;
}

// Returns true if at least two clip points are set
bool ClipPointManager::valid () const
{
	return _clipPoints[0].isSet() && _clipPoints[1].isSet();
}

void ClipPointManager::draw (float scale)
{
	// Draw clip points
	for (unsigned int i = 0; i < NUM_CLIP_POINTS; i++) {
		if (_clipPoints[i].isSet())
			_clipPoints[i].Draw(i, scale);
	}
}

void ClipPointManager::getPlanePoints (Vector3 planepts[3], const AABB& bounds) const
{
	ASSERT_MESSAGE(valid(), "clipper points not initialised");

	planepts[0] = _clipPoints[0]._coords;
	planepts[1] = _clipPoints[1]._coords;
	planepts[2] = _clipPoints[2]._coords;

	Vector3 maxs(bounds.origin + bounds.extents);
	Vector3 mins(bounds.origin - bounds.extents);

	if (!_clipPoints[2].isSet()) {
		int n = (_viewType == XY) ? 2 : (_viewType == YZ) ? 0 : 1;
		int x = (n == 0) ? 1 : 0;
		int y = (n == 2) ? 1 : 2;

		// on viewtype XZ, flip clip points
		if (n == 1) {
			planepts[0][n] = maxs[n];
			planepts[1][n] = maxs[n];
			planepts[2][x] = _clipPoints[0]._coords[x];
			planepts[2][y] = _clipPoints[0]._coords[y];
			planepts[2][n] = mins[n];
		} else {
			planepts[0][n] = mins[n];
			planepts[1][n] = mins[n];
			planepts[2][x] = _clipPoints[0]._coords[x];
			planepts[2][y] = _clipPoints[0]._coords[y];
			planepts[2][n] = maxs[n];
		}
	}
}

void ClipPointManager::update ()
{
	Vector3 planepts[3];
	if (!valid()) {
		planepts[0] = Vector3(0, 0, 0);
		planepts[1] = Vector3(0, 0, 0);
		planepts[2] = Vector3(0, 0, 0);
		Scene_BrushSetClipPlane(GlobalSceneGraph(), Plane3(0, 0, 0, 0));
	} else {
		AABB bounds(Vector3(0, 0, 0), Vector3(64, 64, 64));
		getPlanePoints(planepts, bounds);
		if (_switch) {
			std::swap(planepts[0], planepts[1]);
		}
		Scene_BrushSetClipPlane(GlobalSceneGraph(), Plane3(planepts));
	}
	ClipperChangeNotify();
}

void ClipPointManager::flipClip ()
{
	_switch = !_switch;
	update();
	ClipperChangeNotify();
}

void ClipPointManager::reset ()
{
	for (unsigned int i = 0; i < NUM_CLIP_POINTS; i++) {
		_clipPoints[i].reset();
	}
}

void ClipPointManager::clip ()
{
	if (clipMode() && valid()) {
		Vector3 planepts[3];
		AABB bounds(Vector3(0, 0, 0), Vector3(64, 64, 64));
		getPlanePoints(planepts, bounds);

		Scene_BrushSplitByPlane(GlobalSceneGraph(), planepts[0], planepts[1], planepts[2], getShader(),
				(!_switch) ? eFront : eBack);

		reset();
		update();
		ClipperChangeNotify();
	}
}

void ClipPointManager::splitClip ()
{
	if (clipMode() && valid()) {
		Vector3 planepts[3];
		AABB bounds(Vector3(0, 0, 0), Vector3(64, 64, 64));
		getPlanePoints(planepts, bounds);

		Scene_BrushSplitByPlane(GlobalSceneGraph(), planepts[0], planepts[1], planepts[2], getShader(), eFrontAndBack);

		reset();
		update();
		ClipperChangeNotify();
	}
}

bool ClipPointManager::clipMode () const
{
	return GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eClip;
}

void ClipPointManager::onClipMode (bool enabled)
{
	// Revert all clip points to <0,0,0> values
	reset();

	// Revert the _movingClip pointer, if clip mode to be disabled
	if (!enabled && _movingClip) {
		_movingClip = 0;
	}

	update();
	ClipperChangeNotify();
}

void ClipPointManager::newClipPoint (const Vector3& point)
{
	if (_clipPoints[0].isSet() == false) {
		_clipPoints[0]._coords = point;
		_clipPoints[0].Set(true);
	} else if (_clipPoints[1].isSet() == false) {
		_clipPoints[1]._coords = point;
		_clipPoints[1].Set(true);
	} else if (_clipPoints[2].isSet() == false) {
		_clipPoints[2]._coords = point;
		_clipPoints[2].Set(true);
	} else {
		// All three clip points were already set, restart with the first one
		reset();
		_clipPoints[0]._coords = point;
		_clipPoints[0].Set(true);
	}

	update();
	ClipperChangeNotify();
}

// --------------------------------------------------------------------------------------

// Accessor function for the clip points interface
ClipPointManager* GlobalClipPoints ()
{
	static ClipPointManager _clipPointManager;
	return &_clipPointManager;
}
