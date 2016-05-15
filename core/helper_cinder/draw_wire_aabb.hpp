#pragma once

#include "cinder/gl/gl.h"
#include "render_type.hpp"
#include "collision_aabb.hpp"
namespace lc {
	inline void draw_wire_aabb(const lc::AABB &aabb, cinder::gl::VertBatch &vb) {
		lc::Vec3 x_axis(aabb.max_position.x - aabb.min_position.x, 0.0, 0.0);
		lc::Vec3 z_axis(0.0, 0.0, aabb.max_position.z - aabb.min_position.z);

		std::array<lc::Vec3, 4> top = {
			aabb.max_position - x_axis - z_axis,
			aabb.max_position - x_axis,
			aabb.max_position,
			aabb.max_position - z_axis
		};
		std::array<lc::Vec3, 4> bottom = {
			aabb.min_position,
			aabb.min_position + z_axis,
			aabb.min_position + x_axis + z_axis,
			aabb.min_position + x_axis
		};
		for (int i = 0; i < 4; ++i) {
			vb.vertex(top[i]);
			vb.vertex(top[(i + 1) % 4]);
		}

		for (int i = 0; i < 4; ++i) {
			vb.vertex(bottom[i]);
			vb.vertex(bottom[(i + 1) % 4]);
		}
		for (int i = 0; i < 4; ++i) {
			vb.vertex(top[i]);
			vb.vertex(bottom[i]);
		}
	}

}
