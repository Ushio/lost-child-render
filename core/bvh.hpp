#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <boost/optional.hpp>
#include "TracerType.hpp"

namespace tracer {
	// 
	struct TriangleIntersection : public Intersection {
		Vec2 uv;
	};
	inline boost::optional<TriangleIntersection> intersect_ray_triangle(const Ray &ray, Vec3 v0, Vec3 v1, Vec3 v2) {
		Vec3 orig = ray.o;
		Vec3 dir = ray.d;
		Vec3 baryPosition;

		Vec3 e1 = v1 - v0;
		Vec3 e2 = v2 - v0;

		Vec3 p = glm::cross(dir, e2);

		double a = glm::dot(e1, p);

		bool isback = a < 0.0;

		double f = 1.0 / a;

		Vec3 s = orig - v0;
		baryPosition.x = f * glm::dot(s, p);
		if (baryPosition.x < 0.0)
			return boost::none;
		if (baryPosition.x > 1.0)
			return boost::none;

		Vec3 q = glm::cross(s, e1);
		baryPosition.y = f * glm::dot(dir, q);
		if (baryPosition.y < 0.0)
			return boost::none;
		if (baryPosition.y + baryPosition.x > 1.0)
			return boost::none;

		baryPosition.z = f * glm::dot(e2, q);

		bool isIntersect = baryPosition.z >= 0.0;
		if (isIntersect == false) {
			return boost::none;
		}

		TriangleIntersection intersection;
		intersection.tmin = baryPosition.z;
		intersection.n = normalize(isback ? cross(e2, e1) : cross(e1, e2));
		if (glm::all(glm::isfinite(intersection.n)) == false) { abort(); }
		intersection.isback = isback;
		intersection.uv = Vec2(baryPosition.x, baryPosition.y);
		intersection.p = ray.o + ray.d * intersection.tmin;
		return intersection;
	}

	struct AABB {
		Vec3 min_position = Vec3(std::numeric_limits<double>::max());
		Vec3 max_position = Vec3(-std::numeric_limits<double>::max());

		void expand(Vec3 p) {
			min_position = glm::min(min_position, p);
			max_position = glm::max(max_position, p);
		}
		bool contains(Vec3 p) const {
			for (int dim = 0; dim < 3; ++dim) {
				if (p[dim] < min_position[dim] || max_position[dim] < p[dim]) {
					return false;
				}
			}
			return true;
		}
		bool intersect(Ray ray, double &tmin) const {
			auto p = ray.o;
			auto d = ray.d;

			tmin = 0.0f; // -FLT_MAXに設定して直線における最初の交差を得る
			double tmax = std::numeric_limits<double>::max(); // (線分に対して)光線が移動することのできる最大の距離に設定

															  // 3つのすべてスラブに対して
			for (int i = 0; i < 3; i++) {
				if (glm::abs(d[i]) < glm::epsilon<double>()) {
					// 光線はスラブに対して平行。原点がスラブの中になければ交差なし
					if (p[i] < min_position[i] || p[i] > max_position[i]) return false;
				}
				else {
					// スラブの近い平面および遠い平面と交差する光線のtの値を計算
					double ood = 1.0 / d[i];
					double t1 = (min_position[i] - p[i]) * ood;
					double t2 = (max_position[i] - p[i]) * ood;
					// t1が近い平面との交差、t2が遠い平面との交差となる
					if (t1 > t2) std::swap(t1, t2);
					// スラブの交差している間隔との交差を計算
					if (t1 > tmin) tmin = t1;
					if (t2 < tmax) tmax = t2;
					// スラブに交差がないことが分かれば衝突はないのですぐに終了
					if (tmin > tmax) return false;
				}
			}
			return true;
		}

		std::tuple<AABB, AABB> divide(double border, int dimension) const {
			AABB L = *this;
			AABB R = *this;
			L.max_position[dimension] = border;
			R.min_position[dimension] = border;
			return std::make_tuple(L, R);
		}
	};
	struct BVH {
		struct Node {
			// 空間
			AABB aabb;

			// 所属する三角形インデックス
			std::vector<int> indices;

			// 終端ノードか？
			bool isTerminal() const {
				return indices.empty() == false;
			}
		};

		// 二分木操作
		// 添え字は1から
		inline int left_child(int index) const {
			return index << 1;
		}
		inline int right_child(int index) const {
			return (index << 1) + 1;
		}
		inline int parent(int index) const {
			return index >> 1;
		}
		int depth_to_dimension(int depth) const {
			return depth % 3;
		}

		void build() {
			if (triangles.empty()) {
				return;
			}
			_depth_count = glm::log2((int)triangles.size());
			// _depth_count = 1;

			// ノードのメモリを確保
			// Sum[2^k, {k, 0, n - 1}]
			int nodeCount = (2 << (_depth_count - 1)) - 1;
			_nodes.resize(nodeCount);

			// 最初はルートノードにすべて分配
			_nodes[0].indices.resize(triangles.size());
			for (int i = 0; i < triangles.size(); ++i) {
				_nodes[0].indices[i] = i;
				_nodes[0].aabb.expand(triangles[i].v0);
				_nodes[0].aabb.expand(triangles[i].v1);
				_nodes[0].aabb.expand(triangles[i].v2);
			}

			this->build_recursive(0, 1);
		}

		void build_recursive(int depth, int parent) {
			int dimension = this->depth_to_dimension(depth);
			int node_index = parent - 1;

			// 最大深度に達したら、終わりにする
			if (depth + 1 < _depth_count) {
				// continue
			}
			else {
				// done
				return;
			}

			auto aabb = _nodes[node_index].aabb;
			std::vector<int> &indices = _nodes[node_index].indices;

			// 適当だが部屋の中身が3個より少なくなったら、終わりにする
			if (indices.size() < 3) {
				// done
				return;
			}

			// 分配する
			// まあ、もっと高度なアルゴリズムはあるが、まずはもっとも単純に
			double border = 0.0f;
			for (int index : indices) {
				border += triangles[index].v0[dimension];
				border += triangles[index].v1[dimension];
				border += triangles[index].v2[dimension];
			}
			border /= (indices.size() * 3);

			//AABB aabb_L;
			//AABB aabb_R;
			//std::tie(aabb_L, aabb_R) = aabb.divide(border, dimension);

			int child_L = left_child(parent);
			int child_R = right_child(parent);
			int child_L_index = child_L - 1;
			int child_R_index = child_R - 1;

			_nodes[child_L_index].indices.reserve(indices.size() / 2);
			_nodes[child_R_index].indices.reserve(indices.size() / 2);

			for (int i = 0; i < indices.size(); ++i) {
				int index = indices[i];
				const Triangle &triangle = triangles[index];

				double value0 = triangles[index].v0[dimension];
				double value1 = triangles[index].v1[dimension];
				double value2 = triangles[index].v2[dimension];
				if (value0 <= border || value1 <= border || value2 <= border) {
					_nodes[child_L_index].indices.push_back(index);
					_nodes[child_L_index].aabb.expand(triangle.v0);
					_nodes[child_L_index].aabb.expand(triangle.v1);
					_nodes[child_L_index].aabb.expand(triangle.v2);
				}
				if (border <= value0 || border <= value1 || border <= value2) {
					_nodes[child_R_index].indices.push_back(index);
					_nodes[child_R_index].aabb.expand(triangle.v0);
					_nodes[child_R_index].aabb.expand(triangle.v1);
					_nodes[child_R_index].aabb.expand(triangle.v2);
				}
			}

			// 分配が完了したら自身の分を破棄する
			std::swap(_nodes[node_index].indices, std::vector<int>());

			if (_nodes[child_L_index].indices.empty() == false) {
				this->build_recursive(depth + 1, child_L);
			}
			if (_nodes[child_R_index].indices.empty() == false) {
				this->build_recursive(depth + 1, child_R);
			}
		}

		struct BVHIntersection : public TriangleIntersection {
			BVHIntersection(const TriangleIntersection& intersection, int index) :TriangleIntersection(intersection), triangle_index(index) {
			}
			int triangle_index = 0;
		};

		boost::optional<BVHIntersection> intersect(Ray ray, int parent = 1, int depth = 0, double tmin_already = std::numeric_limits<double>::max()) const {
			// 最大深度を超えたら終わりにする
			if (_depth_count <= depth) {
				return boost::none;
			}

			int node_index = parent - 1;
			double tmin;
			if (_nodes[node_index].aabb.intersect(ray, tmin) == false) {
				return boost::none;
			}

			// すでに判明しているtminが手前にあるなら、後半は判定する必要はない
			if (tmin_already < tmin) {
				return boost::none;
			}

			// 終端までやってきたので所属するポリゴンに総当たり
			if (_nodes[node_index].isTerminal()) {
				boost::optional<BVHIntersection> r;
				for (auto index : _nodes[node_index].indices) {
					const Triangle &triangle = triangles[index];
					if (auto intersection = intersect_ray_triangle(ray, triangle.v0, triangle.v1, triangle.v2)) {
						if (!r) {
							r = BVHIntersection(*intersection, index);
						}
						else {
							if (intersection->tmin < r->tmin) {
								r = BVHIntersection(*intersection, index);
							}
						}
						r->triangle_index = index;
					}
				}
				return r;
			}
			int child_L = left_child(parent);
			int child_R = right_child(parent);
			boost::optional<BVHIntersection> L = this->intersect(ray, child_L, depth + 1, tmin_already);
			if (L) {
				tmin_already = glm::min(tmin_already, L->tmin);
			}
			boost::optional<BVHIntersection> R = this->intersect(ray, child_R, depth + 1, tmin_already);

			if (!L) {
				return R;
			}
			if (!R) {
				return L;
			}
			return L->tmin < R->tmin ? L : R;
		}

		struct Triangle {
			Vec3 v0, v1, v2;
		};
		std::vector<Triangle> triangles;
	public:
		std::vector<Node> _nodes;
		int _depth_count = 0;
	};
}