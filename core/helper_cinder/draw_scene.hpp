#pragma once

#include "cinder/gl/gl.h"
#include "scene.hpp"

namespace lc {
	inline void draw_object(const ConelBoxObject &b) {
		cinder::gl::VertBatch vb_tri(GL_TRIANGLES);

		for (auto tri : b.triangles) {
			vb_tri.color(tri.color.r, tri.color.g, tri.color.b);
			for (int i = 0; i < 3; ++i) {
				vb_tri.vertex(tri.triangle.v[i]);
			}
		}

		vb_tri.draw();
	}
	inline void draw_object(const SphereObject &o) {
		cinder::gl::ScopedPolygonMode wire(GL_LINE);
		cinder::gl::ScopedColor c(0.5, 0.5, 0.5);
		cinder::gl::drawSphere(o.sphere.center, o.sphere.radius, 15);
	}
	inline void draw_object(const TriangleMeshObject &o) {
		//cinder::gl::ScopedPolygonMode wire(GL_LINE);
		//cinder::gl::ScopedColor c(0.5, 0.5, 0.5);
		//cinder::gl::drawSphere(o.sphere.center, o.sphere.radius, 15);
		// NOP
	}

	struct DrawObjectVisitor : public boost::static_visitor<> {
		template <class T>
		void operator() (const T &o) {
			draw_object(o);
		}
	};

	inline void draw_scene(const Scene &scene, int image_width, int image_height) {
		for (int i = 0; i < scene.objects.size(); ++i) {
			scene.objects[i].apply_visitor(DrawObjectVisitor());
		}

		for (int i = 0; i < scene.importances.size(); ++i) {
			auto shape = scene.importances[i].shape;
			cinder::gl::ScopedPolygonMode wire(GL_LINE);
			cinder::gl::ScopedColor c(1.0, 0.3, 0.3);
			cinder::gl::drawSphere(shape.center, shape.radius, 20);
		}

		cinder::gl::ScopedMatrices smat;
		cinder::gl::ScopedColor c(0.5, 0.5, 0.5);
		cinder::gl::multModelMatrix(scene.viewTransform.inverse_matrix());
		lc::draw_camera(scene.camera, image_width, image_height, 100.0);
	}
}
