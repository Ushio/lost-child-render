#pragma once

#include "cinder/TriMesh.h"
#include "render_type.hpp"
#include "collision_triangle.hpp"

namespace lc {
	inline std::vector<Triangle> to_triangles(cinder::TriMeshRef mesh) {
		std::vector<Triangle> triangles;
		int numTriangle = mesh->getNumTriangles();
		triangles.reserve(numTriangle);
		for (int i = 0; i < numTriangle; i++) {
			cinder::vec3 v[3];
			Triangle triangle;
			mesh->getTriangleVertices(i, &v[0], &v[1], &v[2]);
			for (int j = 0; j < 3; ++j) {
				triangle.v[j] = v[j];
			}
			triangles.push_back(triangle);
		}
		return triangles;
	}
}
