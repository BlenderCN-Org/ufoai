#include "RenderablePicoModel.h"

#include "texturelib.h"

namespace model
{
	// Constructor

	RenderablePicoModel::RenderablePicoModel (picoModel_t* mod)
	{
		// Get the number of surfaces to create
		const int nSurf = PicoGetModelNumSurfaces(mod);

		// Create a RenderablePicoSurface for each surface in the structure
		for (int n = 0; n < nSurf; ++n) {
			// Retrieve the surface, discarding it if it is null or non-triangulated (?)
			picoSurface_t* surf = PicoGetModelSurface(mod, n);
			if (surf == 0 || PicoGetSurfaceType(surf) != PICO_TRIANGLES)
				continue;

			// Fix the normals of the surface (?)
			PicoFixSurfaceNormals(surf);

			// Create the RenderablePicoSurface object and add it to the vector
			RenderablePicoSurface *rSurf = new RenderablePicoSurface(surf);
			_surfVec.push_back(*rSurf);

			// Extend the model AABB to include the surface's AABB
			aabb_extend_by_aabb(_localAABB, rSurf->getAABB());
		}
	}

	// Virtual render function

	void RenderablePicoModel::render (RenderStateFlags flags) const
	{
		glEnable(GL_VERTEX_ARRAY);
		glEnable(GL_NORMAL_ARRAY);
		glEnable(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
		glShadeModel(GL_SMOOTH);

		// Iterate over the surfaces, calling the render function on each one
		for (SurfaceList::const_iterator i = _surfVec.begin(); i != _surfVec.end(); ++i) {
			qtexture_t& tex = i->getShader()->getTexture();
			glBindTexture(GL_TEXTURE_2D, tex.texture_number);
			i->render(flags);
		}
	}
}
