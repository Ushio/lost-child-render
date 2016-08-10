#pragma once

#include <boost/type_traits.hpp>
#include <boost/type_traits/aligned_storage.hpp> 

namespace lc {
	template <class T, int MAX_COUNT>
	class fixed_vector {
	public:
		fixed_vector() {

		}
		~fixed_vector() {
			if (boost::has_trivial_destructor<T>::value == false) {
				for (size_t i = 0; i < _size; ++i) {
					static_cast<T *>(_storage.address())[i].~T();
				}
			}
		}
		fixed_vector(const fixed_vector &rhs) {
			_size = rhs._size;
			for (size_t i = 0; i < _size; ++i) {
				new (static_cast<T *>(_storage.address()) + i)T(rhs[i]);
			}
		}
		void operator=(const fixed_vector &rhs) {
			for (size_t i = 0; i < _size; ++i) {
				static_cast<T *>(_storage.address())[i].~T();
			}

			_size = rhs._size;
			for (size_t i = 0; i < _size; ++i) {
				new (static_cast<T *>(_storage.address()) + i)T(rhs[i]);
			}
		}
		void push_back(const T &value) {
			new (static_cast<T *>(_storage.address()) + _size)T(value);
			_size++;
			assert(_size <= MAX_COUNT);
		}
		void pop_back() {
			_size--;
			static_cast<T *>(_storage.address())[_size].~T();
			assert(_size <= MAX_COUNT);
		}
		T &operator[](size_t i) {
			return static_cast<T *>(_storage.address())[i];
		}
		const T &operator[](size_t i) const {
			return static_cast<const T *>(_storage.address())[i];
		}
		bool empty() const {
			return _size == 0;
		}
		size_t size() const {
			return _size;
		}

		T *begin() {
			return static_cast<T *>(_storage.address());
		}
		T *end() {
			return static_cast<T *>(_storage.address()) + _size;
		}
		const T *begin() const {
			return static_cast<const T *>(_storage.address());
		}
		const T *end() const {
			return static_cast<const T *>(_storage.address()) + _size;
		}
	private:
		typedef boost::aligned_storage<sizeof(T) * MAX_COUNT, 8> StorageType;
		StorageType _storage;
		std::size_t _size = 0;
	};
}