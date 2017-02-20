#include <cstdint>
#include <string>

void set_bit(std::uint8_t& byte, std::uint8_t bit_number);

bool is_bit_set(std::uint8_t byte, std::uint8_t bit_number);

std::uint64_t power(std::uint64_t base, std::uint64_t exponent);

std::uint64_t get_abs_exponent(std::uint8_t byte);

std::string trim(const std::string& string);
