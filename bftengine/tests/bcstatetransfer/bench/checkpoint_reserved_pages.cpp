#include "hayai/hayai.hpp"
#include "Serializable.h"

BENCHMARK(checkpoint, std_copy_4k, 10, 100) {
  std::string src(4 * 1024, 0);
  std::string dst(4 * 1024, 0);
  std::copy(src.begin(), src.end(), dst.begin());
}

BENCHMARK(checkpoint, serialize_4k, 10, 100) {
  std::string src(4 * 1024, 0);
  std::string dst(4 * 1024, 0);
  std::ostringstream oss;
  concord::serialize::Serializable::serialize(oss, src.data(), src.size());
  std::string a = oss.str();
}

int main() {
  hayai::ConsoleOutputter consoleOutputter;
  hayai::Benchmarker::AddOutputter(consoleOutputter);
  hayai::Benchmarker::RunAllTests();
  return 0;
}
