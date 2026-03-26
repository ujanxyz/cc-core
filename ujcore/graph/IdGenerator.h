#pragma once

#include <cstdint>
#include <string>

namespace ujcore {

/**
* Stateless / seekable PRNG (aka random-access PRNG or counter-based RNG)
* Generates a deterministic sequence of alphanumeric id.
*/
class IdGenerator {
 public:
  IdGenerator();

  void SetIdLength(size_t length);

  std::string GetNextId();

 private:
  size_t id_length_;
  uint64_t internal_counter_{0ULL};
};

std::string GenSplitMix64OfLength(uint64_t counter, size_t id_length);

}  // namespace ujcore
