#ifndef CCNET_CCNET_H
#define CCNET_CCNET_H

#include <future>
#include <string>
#include "cash_type.h"

namespace ccnet {

	struct device_info {
		device_info(
			const std::string& part_number = "",
			const std::string& serial_number = "",
			std::uint64_t asset_number = 0
		) :
			part_number(part_number),
			serial_number(serial_number),
			asset_number(asset_number) { }

		std::string part_number;
		std::string serial_number;
		std::uint64_t asset_number;
	};

	enum class bill_security_level : std::uint8_t {
		normal = 0,
		high = 1
	};

	enum class cash_action : std::uint8_t {
		hold_cash = 1,
		accept_cash = 2,
		return_cash = 3
	};

	class bill_validator_operator {
		public:
			virtual std::future<void> drop_cassette_full() = 0;
			virtual std::future<void> drop_cassette_installed() = 0;
			virtual std::future<void> drop_cassette_removed() = 0;
			virtual std::future<cash_action> request_cash_action(const cash_type& cash_type) = 0;
			virtual std::future<void> cash_accepted(const cash_type& cash_type) = 0;
			virtual std::future<void> cash_returned(const cash_type& cash_type) = 0;

		protected:
			bill_validator_operator() = default;
			~bill_validator_operator() = default;
	};

}

#endif // CCNET_CCNET_H
