﻿#pragma once
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/simd_vec4.hpp>

namespace lc {
	typedef glm::dvec4 Vec4;
	typedef glm::dvec3 Vec3;
	typedef glm::dvec2 Vec2;
	typedef glm::dmat4 Mat4;
	typedef glm::dmat3 Mat3;
	typedef glm::dquat Quat;

	/*
	光線
	常にdは正規化されていると約束する
	*/
	struct Ray {
		Vec3 o;
		Vec3 d;

		Ray() {}
		Ray(Vec3 origin, Vec3 direction) :o(origin), d(direction) {}

		template<class Archive>
		void serialize(Archive &ar) {
			ar(CEREAL_NVP(o), CEREAL_NVP(d));
		}
	};
}

/*
ファイルシステム選択
*/
#ifdef LC_USE_BOOST_FILESYSTEM
#include <boost/filesystem.hpp>
namespace lc {
	namespace fs = boost::filesystem;
}
#endif
#ifdef LC_USE_STD_FILESYSTEM
#include <filesystem>
namespace lc {
	namespace fs = std::tr2::sys;
}
#endif
