#ifndef VAST_CONCEPT_HASHABLE_HASH_APPEND_HPP
#define VAST_CONCEPT_HASHABLE_HASH_APPEND_HPP

#include <cstddef>
#include <cstdint>

#include <array>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <type_traits>

#include <caf/detail/type_traits.hpp>
#include <caf/meta/save_callback.hpp>
#include <caf/meta/type_name.hpp>

#include "vast/detail/endian.hpp"
#include "vast/detail/type_traits.hpp"

// In order to be hashable, a type T must satisfy at least one of the following
// requirements:
//
//     1. There exists an overload with the following signature:
//
//         template <class Hasher, class T>
//         void hash_append(Hasher& h, T const& x);
//
//     2. T is contiguously hashable under. That is, for all combinations of
//        two instances of T, say `x` and `y`, if `x == y`, then it must also
//        be true that `memcmp(addressof(x), addressof(y), sizeof(T)) == 0`.
//        That is, if `x == y`, then `x` and `y` have the same bit pattern
//        representation.
//
// See https://isocpp.org/files/papers/n3980.html for details.

namespace vast {
namespace detail {

// -- is_uniquely_represented ------------------------------------------------

template <class T>
struct is_uniquely_represented
  : std::integral_constant<bool, std::is_integral<T>{}
                                 || std::is_enum<T> {}
                                 || std::is_pointer<T>{}> {};

template <class T>
struct is_uniquely_represented<T const> : is_uniquely_represented<T> {};

template <class T, class U>
struct is_uniquely_represented<std::pair<T, U>>
  : std::integral_constant<
      bool,
      is_uniquely_represented<T>{}
        && is_uniquely_represented<U>{}
        && sizeof(T) + sizeof(U) == sizeof(std::pair<T, U>)
     > {};

template <class ...T>
struct is_uniquely_represented<std::tuple<T...>>
  : std::integral_constant<
      bool,
      detail::conjunction<is_uniquely_represented<T>::value...>{}
        && detail::sum<sizeof(T)...>{} == sizeof(std::tuple<T...>)
    > {};

template <class T, size_t N>
struct is_uniquely_represented<T[N]> : is_uniquely_represented<T> {};

template <class T, size_t N>
struct is_uniquely_represented<std::array<T, N>>
  : std::integral_constant<
      bool,
      is_uniquely_represented<T>{} && sizeof(T) * N == sizeof(std::array<T, N>)
    > {};

// -- helpers ----------------------------------------------------------------

template <class T>
constexpr void reverse_bytes(T& x) {
  auto ptr = std::memmove(std::addressof(x), std::addressof(x), sizeof(T));
  unsigned char* bytes = static_cast<unsigned char*>(ptr);
  for (unsigned i = 0; i < sizeof(T) / 2; ++i)
    std::swap(bytes[i], bytes[sizeof(T) - 1 - i]);
}

template <class T, class Hasher>
constexpr std::enable_if_t<Hasher::endian == host_endian>
maybe_reverse_bytes(T&, Hasher&) {
  // nop
}

template <class T, class Hasher>
constexpr std::enable_if_t<Hasher::endian != host_endian>
maybe_reverse_bytes(T& x, Hasher&) {
  reverse_bytes(x);
}

// -- is_contiguously_hashable -----------------------------------------------

template <class T, class Hasher>
struct is_contiguously_hashable
  : std::integral_constant<
      bool,
      is_uniquely_represented<T>{}
        && (sizeof(T) == 1 || Hasher::endian == host_endian)
    > {};

template <class T, size_t N, class Hasher>
struct is_contiguously_hashable<T[N], Hasher>
  : std::integral_constant<
      bool,
      is_uniquely_represented<T[N]>{}
        && (sizeof(T) == 1 || Hasher::endian == host_endian)
    > {};

} // namespace detail

template <class Hasher, class T>
std::enable_if_t<detail::is_contiguously_hashable<T, Hasher>{}>
hash_append(Hasher& h, T const& x) noexcept {
  h(std::addressof(x), sizeof(x));
}

// -- Scalars -----------------------------------------------------------------

template <class Hasher, class T>
std::enable_if_t<
  !detail::is_contiguously_hashable<T, Hasher>{}
    && (std::is_integral<T>{} || std::is_pointer<T>{} || std::is_enum<T>{})
>
hash_append(Hasher& h, T x) noexcept {
  detail::reverse_bytes(x);
  h(std::addressof(x), sizeof(x));
}

template <class Hasher, class T>
std::enable_if_t<std::is_floating_point<T>{}>
hash_append(Hasher& h, T x) noexcept {
  // When hashing, we treat -0 and 0 the same.
  if (x == 0)
    x = 0;
  detail::maybe_reverse_bytes(x, h);
  h(&x, sizeof(x));
}

template <class Hasher>
void hash_append(Hasher& h, std::nullptr_t) noexcept {
  void const* p = nullptr;
  detail::maybe_reverse_bytes(p, h);
  h(&p, sizeof(p));
}

// -- Forward declarations to enable ADL --------------------------------------

template <class Hasher, class T, size_t N>
std::enable_if_t<!detail::is_contiguously_hashable<T, Hasher>{}>
hash_append(Hasher& h, T (&a)[N]) noexcept;

template <class Hasher, class CharT, class Traits, class Alloc>
std::enable_if_t<!detail::is_contiguously_hashable<CharT, Hasher>{}>
hash_append(Hasher& h, std::basic_string<CharT, Traits, Alloc> const& s) noexcept;

template <class Hasher, class CharT, class Traits, class Alloc>
std::enable_if_t<detail::is_contiguously_hashable<CharT, Hasher>{}>
hash_append(Hasher& h, std::basic_string<CharT, Traits, Alloc> const& s) noexcept;

template <class Hasher, class T, class U>
std::enable_if_t<!detail::is_contiguously_hashable<std::pair<T, U>, Hasher>{}>
hash_append (Hasher& h, std::pair<T, U> const& p) noexcept;

template <class Hasher, class T, class Alloc>
std::enable_if_t<!detail::is_contiguously_hashable<T, Hasher>{}>
hash_append(Hasher& h, std::vector<T, Alloc> const& v) noexcept;

template <class Hasher, class T, class Alloc>
std::enable_if_t<detail::is_contiguously_hashable<T, Hasher>{}>
hash_append(Hasher& h, std::vector<T, Alloc> const& v) noexcept;

template <class Hasher, class T, size_t N>
std::enable_if_t<!detail::is_contiguously_hashable<std::array<T, N>, Hasher>{}>
hash_append(Hasher& h, std::array<T, N> const& a) noexcept;

template <class Hasher, class ...T>
std::enable_if_t<!detail::is_contiguously_hashable<std::tuple<T...>, Hasher>{}>
hash_append(Hasher& h, std::tuple<T...> const& t) noexcept;

template <class Hasher, class T0, class T1, class ...T>
void hash_append(Hasher& h, T0 const& t0, T1 const& t1, T const& ...t) noexcept;

// -- C array -----------------------------------------------------------------

template <class Hasher, class T, size_t N>
std::enable_if_t<!detail::is_contiguously_hashable<T, Hasher>{}>
hash_append(Hasher& h, T (&a)[N]) noexcept {
  for (auto const& x : a)
    hash_append(h, x);
}

// -- string ------------------------------------------------------------------

template <class Hasher, class CharT, class Traits, class Alloc>
std::enable_if_t<!detail::is_contiguously_hashable<CharT, Hasher>{} >
hash_append(Hasher& h,
            std::basic_string<CharT, Traits, Alloc> const& s) noexcept {
  for (auto c : s)
    hash_append(h, c);
  hash_append(h, s.size());
}

template <class Hasher, class CharT, class Traits, class Alloc>
std::enable_if_t<detail::is_contiguously_hashable<CharT, Hasher>{}>
hash_append(Hasher& h,
            std::basic_string<CharT, Traits, Alloc> const& s) noexcept {
  h(s.data(), s.size() * sizeof(CharT));
  hash_append(h, s.size());
}

// -- pair --------------------------------------------------------------------


template <class Hasher, class T, class U>
std::enable_if_t<!detail::is_contiguously_hashable<std::pair<T, U>, Hasher>{}>
hash_append(Hasher& h, std::pair<T, U> const& p) noexcept {
  hash_append(h, p.first, p.second);
}

// -- vector ------------------------------------------------------------------

template <class Hasher, class T, class Alloc>
std::enable_if_t<!detail::is_contiguously_hashable<T, Hasher>{}>
hash_append(Hasher& h, std::vector<T, Alloc> const& v) noexcept {
  for (auto const& t : v)
    hash_append(h, t);
  hash_append(h, v.size());
}

template <class Hasher, class T, class Alloc>
std::enable_if_t<detail::is_contiguously_hashable<T, Hasher>{}>
hash_append(Hasher& h, std::vector<T, Alloc> const& v) noexcept {
  h(v.data(), v.size() * sizeof(T));
  hash_append(h, v.size());
}

// -- array -------------------------------------------------------------------

template <class Hasher, class T, size_t N>
std::enable_if_t<!detail::is_contiguously_hashable<std::array<T, N>, Hasher>{}>
hash_append(Hasher& h, std::array<T, N> const& a) noexcept {
  for (auto const& t : a)
    hash_append(h, t);
}

// -- tuple -------------------------------------------------------------------

namespace detail {

inline void for_each_item(...) noexcept {
}

template <class Hasher, class T>
int hash_one(Hasher& h, T const& t) noexcept {
  hash_append(h, t);
  return 0;
}

template <class Hasher, class ...T, size_t ...I>
void tuple_hash(Hasher& h, std::tuple<T...> const& t,
                std::index_sequence<I...>) noexcept {
  for_each_item(hash_one(h, std::get<I>(t))...);
}

} // namespace detail

template <class Hasher, class ...T>
std::enable_if_t<!detail::is_contiguously_hashable<std::tuple<T...>, Hasher>{}>
hash_append(Hasher& h, std::tuple<T...> const& t) noexcept {
  detail::tuple_hash(h, t, std::index_sequence_for<T...>{});
}

// -- variadic ----------------------------------------------------------------

template <class Hasher, class T0, class T1, class... Ts>
void hash_append(Hasher& h, T0 const& x0, T1 const& x1,
                 Ts const& ...xs) noexcept {
  hash_append(h, x0);
  hash_append(h, x1, xs...);
}

// -- inspectable -------------------------------------------------------------

namespace detail {

template <class Hasher>
struct hash_inspector {
  using result_type = void;

  static constexpr bool reads_state = true;

  hash_inspector(Hasher& h) : h_{h} {
  }

  result_type operator()() const noexcept {
    // End of recursion.
  }

  template <class... Ts>
  result_type operator()(caf::meta::type_name_t x, Ts&&... xs) const {
    // Figure out the actual bytes to hash.
    auto ptr = x.value;
    while (*ptr != '\0')
      ++ptr;
    h_(x.value, ptr - x.value);
    (*this)(std::forward<Ts>(xs)...);
  }

  template <class F, class... Ts>
  result_type operator()(caf::meta::save_callback_t<F> x, Ts&&... xs) const {
    x.fun();
    (*this)(std::forward<Ts>(xs)...);
  }

  template <class T, class... Ts>
  std::enable_if_t<caf::meta::is_annotation<T>::value, result_type>
  operator()(T&&, Ts&&... xs) const noexcept {
    (*this)(std::forward<Ts>(xs)...);
  }

  template <class T, class... Ts>
  std::enable_if_t<!caf::meta::is_annotation<T>::value, result_type>
  operator()(T&& x, Ts&&... xs) const noexcept {
    hash_append(h_, x);
    (*this)(std::forward<Ts>(xs)...);
  }

  Hasher& h_;
};

} // namespace detail

template <class Hasher, class T>
std::enable_if_t<
  caf::detail::is_inspectable<detail::hash_inspector<Hasher>, T>::value
>
hash_append(Hasher& h, T const& x) noexcept {
  detail::hash_inspector<Hasher> f{h};
  inspect(f, const_cast<T&>(x));
}

} // namespace vast

#endif
