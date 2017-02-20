#ifndef CCNET_CASH_TYPE_H
#define CCNET_CASH_TYPE_H

#include <string>

namespace ccnet {

	struct cash_type {
		cash_type(
			const std::string& currency_code = "",
			std::uint64_t denomination = 0
		) :
			currency_code(currency_code),
			denomination(denomination) { }

		std::string currency_code;
		std::uint64_t denomination;
	};

	bool operator==(const cash_type& lhs, const cash_type& rhs);
	bool operator!=(const cash_type& lhs, const cash_type& rhs);
	bool operator<(const cash_type& lhs, const cash_type& rhs);
	bool operator>(const cash_type& lhs, const cash_type& rhs);

}

#endif // CCNET_CASH_TYPE_H
