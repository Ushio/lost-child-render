#pragma once

#include "render_type.hpp"
#include "collision.hpp"
#include "collision_aabb.hpp"
#include "collision_triangle.hpp"
#include <boost/optional.hpp>
#include <boost/range.hpp>

namespace lc {
	static const double kCOST_INTERSECT_AABB = 1.0;
	static const double kCOST_INTERSECT_TRIANGLE = 1.5;

	inline double surface_area(const AABB &aabb) {
		Vec3 size = aabb.max_position - aabb.min_position;
		return (size.x * size.z + size.x * size.y + size.z * size.y) * 2.0;
	}
	inline AABB expand(AABB aabb, const Triangle &triangle) {
		for (int j = 0; j < 3; ++j) {
			aabb = expand(aabb, triangle.v[j]);
		}
		return aabb;
	}

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

		void build() {
			if (_triangles.empty()) {
				return;
			}
			_depth_count = glm::log2((int)_triangles.size());

			// ノードのメモリを確保
			// Sum[2^k, {k, 0, n - 1}]
			int nodeCount = (2 << (_depth_count - 1)) - 1;
			_nodes.resize(nodeCount);

			// 最初はルートノードにすべて分配
			_nodes[0].indices.resize(_triangles.size());
			for (int i = 0; i < _triangles.size(); ++i) {
				_nodes[0].indices[i] = i;
				_nodes[0].aabb = expand(_nodes[0].aabb, _triangles[i]);
			}

			this->build_recursive(0, 1);
		}

		void build_recursive(int depth, int parent) {
			int node_index = parent - 1;

			// 最大深度に達したら、終わりにする
			if (depth + 1 < _depth_count) {
				// continue
			}
			else {
				// done
				return;
			}
			
			std::vector<int> &indices = _nodes[node_index].indices;
			// 数が少なくなったら、終わりにする
			if (indices.size() < 3) {
				// done
				return;
			}

			auto aabb = _nodes[node_index].aabb;
			auto area = surface_area(aabb);

			int child_L = left_child(parent);
			int child_R = right_child(parent);
			int child_L_index = child_L - 1;
			int child_R_index = child_R - 1;

			std::vector<double> compornents(indices.size() * 3);

			std::vector<int> indices_L;
			std::vector<int> indices_R;
			AABB aabb_L;
			AABB aabb_R;

			// 分割しなかった場合のコストが最小コストである
			double min_cost = indices.size() * kCOST_INTERSECT_TRIANGLE;

			bool is_separate = false;
			
			for (int dimension = 0; dimension < 3; ++dimension) {
				for (int i = 0; i < indices.size(); ++i) {
					int index = indices[i];
					const Triangle &triangle = _triangles[index];
					for (int j = 0; j < 3; ++j) {
						compornents[i * 3 + j] = triangle.v[j][dimension];
					}
				}

				// 最大最小
				std::vector<double>::iterator min_edge;
				std::vector<double>::iterator max_edge;
				std::tie(min_edge, max_edge) = std::minmax_element(compornents.begin(), compornents.end());

				// 根拠のある数字ではないが、あまり計算が爆発しない程度
				// int separation = std::max((int)indices.size() >> 6, 2);
				int separation = glm::max((int)glm::sqrt((double)(indices.size())), 2);
				double step = (*max_edge - *min_edge) / separation;

				for (int i = 0; i < separation - 1; ++i) {
					double border = *min_edge + (i + 1) * step;

					// クリア
					indices_L.clear();
					indices_R.clear();
					indices_L.reserve(indices.size() / 2);
					indices_R.reserve(indices.size() / 2);

					aabb_L = AABB();
					aabb_R = AABB();

					// ボーダーに基づいて振り分ける
					for (int i = 0; i < indices.size(); ++i) {
						int index = indices[i];
						const Triangle &triangle = _triangles[index];

						double value0 = _triangles[index].v[0][dimension];
						double value1 = _triangles[index].v[1][dimension];
						double value2 = _triangles[index].v[2][dimension];
						if (value0 <= border || value1 <= border || value2 <= border) {
							indices_L.push_back(index);
							aabb_L = expand(aabb_L, triangle);
						}
						if (border < value0 || border < value1 || border < value2) {
							indices_R.push_back(index);
							aabb_R = expand(aabb_R, triangle);
						}
					}

					// 分割した場合のコスト期待値
					double cost = 2.0 * kCOST_INTERSECT_AABB
						+ (surface_area(aabb_L) / area) * indices_L.size() * kCOST_INTERSECT_TRIANGLE +
						+ (surface_area(aabb_R) / area) * indices_R.size() * kCOST_INTERSECT_TRIANGLE;

					if (cost < min_cost) {
						_nodes[child_L_index].aabb = aabb_L;
						_nodes[child_R_index].aabb = aabb_R;
						std::swap(_nodes[child_L_index].indices, indices_L);
						std::swap(_nodes[child_R_index].indices, indices_R);
						min_cost = cost;

						is_separate = true;
					}
				}
			}

			// 分割は必要ない
			if (is_separate == false) {
				return;
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
			int triangle_index = -1;
		};

		boost::optional<BVHIntersection> intersect(Ray ray, int parent = 1, int depth = 0, double tmin_already = std::numeric_limits<double>::max()) const {
			// 最大深度を超えたら終わりにする
			if (_depth_count <= depth) {
				return boost::none;
			}

			int node_index = parent - 1;
			auto intersection = lc::intersect(ray, _nodes[node_index].aabb);
			if (!intersection) {
				return boost::none;
			}

			// すでに判明しているtminが手前にあるなら、後半は判定する必要はない
			if (tmin_already < intersection->tmin) {
				return boost::none;
			}

			// 終端までやってきたので所属するポリゴンに総当たりして終了
			if (_nodes[node_index].isTerminal()) {
				boost::optional<BVHIntersection> r;
				for (auto index : _nodes[node_index].indices) {
					const Triangle &triangle = _triangles[index];
					if (auto intersection = lc::intersect(ray, triangle)) {
						double tmin = r ? r->tmin : std::numeric_limits<double>::max();
						if (intersection->tmin < tmin) {
							r = BVHIntersection(*intersection, index);
							r->triangle_index = index;
						}
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

		int depth_count() const {
			return _depth_count;
		}
		void set_triangle(const std::vector<Triangle> &triangles) {
			_triangles = triangles;
		}

		std::vector<Triangle> _triangles;
		std::vector<Node> _nodes;
		int _depth_count = 0;
	};
}