#include "RenderablePicoSurface.h"

namespace model
{
	// Constructor. Copy the provided picoSurface_t structure into this object
	RenderablePicoSurface::RenderablePicoSurface (picoSurface_t* surf) :
		_shaderName("")
	{
		// Get the shader from the picomodel struct. If this is a LWO model, use
		// the material name to select the shader, while for an ASE model the
		// bitmap path should be used.
		picoShader_t* shader = PicoGetSurfaceShader(surf);
		if (shader != 0) {
			_shaderName = PicoGetShaderName(shader);
		}
		globalOutputStream() << "  RenderablePicoSurface: using shader " << _shaderName.c_str() << "\n";

		// Capture the shader
		_shader = GlobalShaderCache().capture(_shaderName);

		// Get the number of vertices and indices, and reserve capacity in our vectors in advance
		// by populating them with empty structs.
		int nVerts = PicoGetSurfaceNumVertexes(surf);
		_nIndices = PicoGetSurfaceNumIndexes(surf);
		_vertices.resize(nVerts);
		_indices.resize(_nIndices);

		// Stream in the vertex data from the raw struct, expanding the local AABB to include
		// each vertex.
		for (int vNum = 0; vNum < nVerts; ++vNum) {
			Vertex3f vertex(PicoGetSurfaceXYZ(surf, vNum));
			_localAABB.includePoint(vertex);
			_vertices[vNum].vertex = vertex;
			_vertices[vNum].normal = Normal3f(PicoGetSurfaceNormal(surf, vNum));
			_vertices[vNum].texcoord = TexCoord2f(PicoGetSurfaceST(surf, 0, vNum));
		}

		// Stream in the index data
		picoIndex_t* ind = PicoGetSurfaceIndexes(surf, 0);
		for (unsigned int i = 0; i < _nIndices; i++)
			_indices[i] = ind[i];
	}

	// Render function
	void RenderablePicoSurface::render (RenderStateFlags flags) const
	{
		// Use Vertex Arrays to submit data to the GL. We will assume that it is
		// acceptable to perform pointer arithmetic over the elements of a std::vector,
		// starting from the address of element 0.
		glNormalPointer(GL_FLOAT, sizeof(ArbitraryMeshVertex), &_vertices[0].normal);
		glVertexPointer(3, GL_FLOAT, sizeof(ArbitraryMeshVertex), &_vertices[0].vertex);
		glTexCoordPointer(2, GL_FLOAT, sizeof(ArbitraryMeshVertex), &_vertices[0].texcoord);
		glDrawElements(GL_TRIANGLES, _nIndices, GL_UNSIGNED_INT, &_indices[0]);
	}

} // namespace model
