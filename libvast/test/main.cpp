#define CAF_TEST_NO_MAIN
#include <caf/test/unit_test_impl.hpp>

#include "vast/detail/adjust_resource_consumption.hpp"

int main(int argc, char** argv) {
  if (!vast::detail::adjust_resource_consumption())
    return 1;
  return caf::test::main(argc, argv);
}
