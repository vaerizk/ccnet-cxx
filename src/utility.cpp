#include "utility.h"
#include <exception>

void set_bit(std::uint8_t& byte, std::uint8_t bit_number) {
	byte = byte | (1 << bit_number);
}

bool is_bit_set(std::uint8_t byte, std::uint8_t bit_number) {
	return (byte & (1 << bit_number)) != 0;
}

std::uint64_t power(std::uint64_t base, std::uint64_t exponent) {
	if ((base == 0) && (exponent == 0)) {
		throw std::exception("invalid arguments");
	}

	std::uint64_t result = 1;
	for (std::uint64_t i = 1; i <= exponent; ++i) {
		result *= base;
	}

	return result;
}

std::uint64_t get_abs_exponent(std::uint8_t byte) {
	return byte & 0x7F;
}

std::string trim(const std::string& string) {
	std::string result(string);
	result.erase(0, string.find_first_not_of(" \n\r\t"));
	result.erase(string.find_last_not_of(" \n\r\t") + 1);
	return result;
}
