#pragma once

#include "render_type.hpp"

namespace lc {
	/*
	平行移動、回転、スケール
	*/
	struct Transform {
		Transform() {}
		Transform(const Mat4 &mat) {
			_o_transform_w = mat;
			_o_transform_w_i = glm::inverse(mat);
			_d_transform_w = glm::transpose(Mat3(_o_transform_w_i));
			_d_transform_w_i = Mat3(_o_transform_w_i);
		}
		Vec3 to_local_position(const Vec3 &p) const {
			lc::Vec4 tp = _o_transform_w_i * lc::Vec4(p, 1.0);
			return lc::Vec3(tp.x, tp.y, tp.z);
		}
		Vec3 to_local_normal(const Vec3 &p) const {
			return _d_transform_w_i * p;
		}
		Vec3 from_local_position(const Vec3 &p) const {
			lc::Vec4 tp = _o_transform_w * lc::Vec4(p, 1.0);
			return lc::Vec3(tp.x, tp.y, tp.z);
		}
		Vec3 from_local_normal(const Vec3 &p) const {
			return _d_transform_w * p;
		}
		Ray to_local_ray(const Ray &ray) const {
			return Ray(to_local_position(ray.o), to_local_normal(ray.d));
		}
		Ray from_local_ray(const Ray &ray) const {
			return Ray(from_local_position(ray.o), from_local_normal(ray.d));
		}
		
		Mat4 matrix() const {
			return _o_transform_w;
		}
		Mat4 inverse_matrix() const {
			return _o_transform_w_i;
		}
	private:
		Mat4 _o_transform_w;
		Mat3 _d_transform_w;
		Mat4 _o_transform_w_i;
		Mat3 _d_transform_w_i;
	};
	inline Transform operator*(const Transform &lhs, const Transform &rhs) {
		return Transform(lhs.matrix() * rhs.matrix());
	}

	// +y を軸とする半球を、任意のyaxisに向けて回転する
	struct HemisphereTransform {
	public:
		HemisphereTransform(const Vec3 &yaxis):_yaxis(yaxis){
			if (0.999 < glm::abs(yaxis.z)) {
				_xaxis = glm::normalize(glm::cross(Vec3(0.0, -1.0, 0.0), yaxis));
			}
			else {
				_xaxis = glm::normalize(glm::cross(Vec3(0.0, 0.0, 1.0), yaxis));
			}
			_zaxis = glm::cross(_xaxis, _yaxis);
		}
		Vec3 transform(const Vec3 &direction) const {
			return direction.x * _xaxis + direction.y * _yaxis + direction.z * _zaxis;
		}
		Vec3 _yaxis;
		Vec3 _xaxis;
		Vec3 _zaxis;
	};
}

