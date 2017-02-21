#include "cash_type.h"

bool ccnet::operator==(const cash_type& lhs, const cash_type& rhs) {
	return (lhs.currency_code == rhs.currency_code)
		&& (lhs.denomination == rhs.denomination);
}

bool ccnet::operator!=(const cash_type& lhs, const cash_type& rhs) {
	return !(lhs == rhs);
}

bool ccnet::operator<(const cash_type& lhs, const cash_type& rhs) {
	if (lhs.currency_code != rhs.currency_code) {
		return lhs.currency_code < rhs.currency_code;
	}

	return lhs.denomination < rhs.denomination;
}

bool ccnet::operator>(const cash_type& lhs, const cash_type& rhs) {
	if (lhs.currency_code != rhs.currency_code) {
		return lhs.currency_code > rhs.currency_code;
	}

	return lhs.denomination > rhs.denomination;
}

bool ccnet::operator<=(const cash_type& lhs, const cash_type& rhs) {
	return (lhs < rhs) || (lhs == rhs);
}

bool ccnet::operator>=(const cash_type& lhs, const cash_type& rhs) {
	return (lhs > rhs) || (lhs == rhs);
}
