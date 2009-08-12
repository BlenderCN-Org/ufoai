#ifndef RENDERABLEPICOMODEL_H_
#define RENDERABLEPICOMODEL_H_

#include <string>
#include <vector>
#include "RenderablePicoSurface.h"
#include "imodel.h"
#include "picomodel.h"
#include "math/aabb.h"
#include "igl.h"

namespace model
{
	/* Renderable class containing a model loaded via the picomodel library. A
	 * RenderablePicoModel is made up of one or more RenderablePicoSurface objects,
	 * each of which contains a number of polygons with the same texture. Rendering
	 * a RenderablePicoModel involves rendering all of its surfaces, each of which
	 * binds its texture(s) and submits its geometry via OpenGL calls.
	 */
	class RenderablePicoModel: public IModel
	{
			// Vector of renderable surfaces for this model
			typedef std::vector<RenderablePicoSurface> SurfaceList;
			SurfaceList _surfVec;

			// Local AABB for this model
			AABB _localAABB;

		public:

			/** Constructor. Accepts a picoModel_t struct containing the raw model data
			 * loaded from picomodel
			 */
			RenderablePicoModel (picoModel_t* mod);

			/** Virtual render function from OpenGLRenderable.
			 */
			void render (RenderStateFlags flags) const;

			/** Return the number of surfaces in this model.
			 */
			int getSurfaceCount () const
			{
				return _surfVec.size();
			}

			/** Return the number of vertices in this model, by summing the vertex
			 * counts for each surface
			 */
			int getVertexCount () const
			{
				int sum = 0;
				for (SurfaceList::const_iterator i = _surfVec.begin(); i != _surfVec.end(); ++i) {
					sum += i->getVertexCount();
				}
				return sum;
			}

			/** Return the polycount (tricount) of this model by summing the surface
			 * polycounts.
			 */
			int getPolyCount () const
			{
				int sum = 0;
				for (SurfaceList::const_iterator i = _surfVec.begin(); i != _surfVec.end(); ++i) {
					sum += i->getPolyCount();
				}
				return sum;
			}

			/** Return the enclosing AABB for this model.
			 */
			const AABB& getAABB () const
			{
				return _localAABB;
			}
	};
}

#endif /*RENDERABLEPICOMODEL_H_*/
