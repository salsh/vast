#ifndef VAST_LOGGER_HPP
#define VAST_LOGGER_HPP

#include <sstream>
#include <type_traits>

#include <caf/logger.hpp>
#include <caf/stateful_actor.hpp>
#include <caf/typed_actor.hpp>

#include "vast/config.hpp"
#include "vast/concept/printable/print.hpp"
#include "vast/detail/pp.hpp"

namespace vast {
namespace detail {

struct formatter {
  template <class Stream, class T>
  struct is_streamable {
    template <class S, class U>
    static auto test(U const* x)
    -> decltype(std::declval<S&>() << *x, std::true_type());

    template <class, class>
    static auto test(...) -> std::false_type;

    using type = decltype(test<Stream, T>(0));
    static constexpr auto value = type::value;
  };

  template <class T>
  auto operator<<(T const& x)
  -> std::enable_if_t<is_streamable<std::ostringstream, T>::value, formatter&> {
    message << x;
    return *this;
  }

  template <class T>
  auto operator<<(T const& x)
  -> std::enable_if_t<
       !is_streamable<std::ostringstream, T>::value
          && vast::is_printable<std::ostreambuf_iterator<char>, T>::value,
       formatter&
     > {
    using vast::print;
    if (!print(std::ostreambuf_iterator<char>{message}, x))
      message.setstate(std::ios_base::failbit);
    return *this;
  }

  template <class T, class... Ts>
  formatter& operator<<(caf::stateful_actor<T, Ts...>* a) {
    message << a->name();
    return *this;
  }

  template <class... Ts>
  formatter& operator<<(caf::typed_actor<Ts...> const& a) {
    return *this << a->address();
  }

  formatter& operator<<(caf::actor const& a) {
    return *this << a->address();
  }

  formatter& operator<<(caf::actor_addr const& a) {
    message << a.id();
    return *this;
  }

  // E.g., self->current_sender()
  formatter& operator<<(caf::strong_actor_ptr const& a) {
    if (a)
      message << a->id();
    else
      message << "invalid";
    return *this;
  }

  std::ostringstream message;
};

} // namespace detail
} // namespace vast

#if defined(CAF_LOG_LEVEL)
  #define VAST_LOG_IMPL(lvl, msg)                                              \
    do {                                                                       \
      vast::detail::formatter __vast_fmt;                                      \
      __vast_fmt << msg;                                                       \
      CAF_LOG_IMPL("vast", lvl, __vast_fmt.message.str());                     \
    } while (false)

  #define VAST_LOG_2(lvl, m1) VAST_LOG_IMPL(lvl, m1)
  #define VAST_LOG_3(lvl, m1, m2) VAST_LOG_2(lvl, m1 << ' ' << m2)
  #define VAST_LOG_4(lvl, m1, m2, m3) VAST_LOG_3(lvl, m1, m2 << ' ' << m3)
  #define VAST_LOG_5(lvl, m1, m2, m3, m4)                                      \
    VAST_LOG_4(lvl, m1, m2, m3 << ' ' << m4)
  #define VAST_LOG_6(lvl, m1, m2, m3, m4, m5)                                  \
    VAST_LOG_5(lvl, m1, m2, m3, m4 << ' ' << m5)
  #define VAST_LOG_7(lvl, m1, m2, m3, m4, m5, m6)                              \
    VAST_LOG_6(lvl, m1, m2, m3, m4, m5 << ' ' << m6)
  #define VAST_LOG_8(lvl, m1, m2, m3, m4, m5, m6, m7)                          \
    VAST_LOG_7(lvl, m1, m2, m3, m4, m5, m6 << ' ' << m7)
  #define VAST_LOG_9(lvl, m1, m2, m3, m4, m5, m6, m7, m8)                      \
    VAST_LOG_8(lvl, m1, m2, m3, m4, m5, m6, m7 << ' ' << m8)
  #define VAST_LOG_10(lvl, m1, m2, m3, m4, m5, m6, m7, m8, m9)                 \
    VAST_LOG_9(lvl, m1, m2, m3, m4, m5, m6, m7, m8 << ' ' << m9)
  #define VAST_LOG(...)                                                        \
    VAST_PP_OVERLOAD(VAST_LOG_, __VA_ARGS__)(__VA_ARGS__)

  #if VAST_LOG_LEVEL >= CAF_LOG_LEVEL_ERROR
    #define VAST_ERROR(...) VAST_LOG(CAF_LOG_LEVEL_ERROR, __VA_ARGS__)
  #else
    #define VAST_ERROR(...) CAF_VOID_STMT
  #endif

  #if VAST_LOG_LEVEL >= CAF_LOG_LEVEL_WARNING
    #define VAST_WARNING(...) VAST_LOG(CAF_LOG_LEVEL_WARNING, __VA_ARGS__)
  #else
    #define VAST_WARNING(...) CAF_VOID_STMT
  #endif

  #if VAST_LOG_LEVEL >= CAF_LOG_LEVEL_INFO
    #define VAST_INFO(...) VAST_LOG(CAF_LOG_LEVEL_INFO, __VA_ARGS__)
  #else
    #define VAST_INFO(...) CAF_VOID_STMT
  #endif

  #if VAST_LOG_LEVEL >= CAF_LOG_LEVEL_DEBUG
    #define VAST_DEBUG(...) VAST_LOG(CAF_LOG_LEVEL_DEBUG, __VA_ARGS__)
  #else
    #define VAST_DEBUG(...) CAF_VOID_STMT
  #endif

  #if VAST_LOG_LEVEL >= CAF_LOG_LEVEL_TRACE
    #define VAST_TRACE(...) VAST_LOG(CAF_LOG_LEVEL_TRACE, __VA_ARGS__)
  #else
    #define VAST_TRACE(...) CAF_VOID_STMT
  #endif
#else
  #define VAST_ERROR(...) CAF_VOID_STMT
  #define VAST_WARNING(...) CAF_VOID_STMT
  #define VAST_INFO(...) CAF_VOID_STMT
  #define VAST_DEBUG(...) CAF_VOID_STMT
  #define VAST_TRACE(...) CAF_VOID_STMT
#endif

#endif
