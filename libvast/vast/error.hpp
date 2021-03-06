#ifndef VAST_ERROR_HPP
#define VAST_ERROR_HPP

#include <caf/error.hpp>
#include <caf/make_message.hpp>

namespace vast {

using caf::error;

/// VAST's error codes.
enum class ec : uint8_t {
  /// The unspecified default error code.
  unspecified = 1,
  /// An error while accessing the filesystem.
  filesystem_error,
  /// Expected a different type.
  type_clash,
  /// The operation does not support the given operator.
  unsupported_operator,
  /// Failure during parsing.
  parse_error,
  /// Failure during printing.
  print_error,
  /// Malformed query expression.
  invalid_query,
  /// An error with an input/output format.
  format_error,
  /// Exhausted the input.
  end_of_input,
  /// Encountered two incompatible versions.
  version_error,
};

/// @relates ec
const char* to_string(ec x);

/// @relates ec
template <class... Ts>
error make_error(ec x, Ts&&... xs) {
  return error{static_cast<uint8_t>(x), caf::atom("vast"),
               caf::make_message(std::forward<Ts>(xs)...)};
}

} // namespace vast

#endif
