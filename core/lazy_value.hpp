#pragma once

#include <boost/type_traits/aligned_storage.hpp> 

template <class T, int MAX_SIZE>
struct LazyValue {
	LazyValue() {}
	LazyValue(const LazyValue &) = delete;
	void operator=(const LazyValue &) = delete;

	~LazyValue() {
		if (_hasValue) {
			static_cast<HolderErase *>(_storage.address())->~HolderErase();
		}
	}

	struct HolderErase {
		virtual ~HolderErase() {}
		virtual T evaluate() const = 0;
	};

	template <class F>
	struct Holder : public HolderErase {
		Holder(const F &f) :_f(f) {}
		T evaluate() const {
			return _f();
		}
		F _f;
	};

	template <class F>
	void operator=(const F &f) {
		static_assert(sizeof(F) <= StorageType::size, "low memory");
		if (_hasValue) {
			static_cast<HolderErase *>(_storage.address())->~HolderErase();
		}
		new (_storage.address()) Holder<F>(f);
		_hasValue = true;
	}
	bool hasValue() const {
		return _hasValue;
	}
	T evaluate() const {
		return static_cast<const HolderErase *>(_storage.address())->evaluate();
	}

	typedef boost::aligned_storage<MAX_SIZE, 8> StorageType;

	bool _hasValue = false;
	StorageType _storage;
};
