#include "bill_validator.h"
#include <exception>
#include "utility.h"

using namespace boost::asio;
using namespace ccnet;

#define POLYNOMIAL 0x08408

const std::uint8_t byte_size = 8;

// serial port parameters
const serial_port::baud_rate baud_rate(9600);
const serial_port::character_size char_size(8);
const serial_port::parity parity(serial_port::parity::none);
const serial_port::stop_bits stop_bits(serial_port::stop_bits::one);
const serial_port::flow_control flow_ctrl(serial_port::flow_control::none);

// acknowledge
const std::uint8_t ack = 0x00;
// negative acknowledge
const std::uint8_t nak = 0xff;
// illegal command
const std::uint8_t ill_cmd = 0x30;

const std::uint8_t sync = 0x02;
const std::uint8_t bill_validator_addr = 0x03;

// frame header structure
const std::uint8_t header_size = 3; // in bytes
const std::uint8_t sync_offset = 0;
const std::uint8_t adr_offset = 1;
const std::uint8_t lng_offset = 2;

const std::uint64_t currency_base = 10;
const std::uint8_t exponent_sign_bit_number = 7;

bool bill_validator::device_state::operator==(const bill_validator::device_state& other) const {
	return (this->code == other.code) && (this->info == other.info);
}

bool bill_validator::device_state::operator!=(const bill_validator::device_state& other) const {
	return !(*this == other);
}

bill_validator::bill_validator(const std::string& port_name, bill_validator_operator* bill_validator_operator) :
	cmd_handler_thread(),
	thread_is_working(false),
	cmd_queue(),
	cmd_queue_mutex(),
	io_service(),
	serial_port(io_service, port_name),
	connected_device_operator(bill_validator_operator),
	connected_device_info(),
	bill_types_by_numbers() {
	try {
		this->serial_port.set_option(baud_rate);
		this->serial_port.set_option(char_size);
		this->serial_port.set_option(parity);
		this->serial_port.set_option(stop_bits);
		this->serial_port.set_option(flow_ctrl);

		this->thread_is_working = true;
		this->cmd_handler_thread = std::thread(&bill_validator::operate, this);
	} catch (boost::system::system_error) {
		throw std::exception("serial port error");
	} catch (std::system_error) {
		throw std::exception("unable to create handler thread");
	}
}

bill_validator::~bill_validator() {
	this->thread_is_working = false;
	this->cmd_handler_thread.join();
}

std::future<device_info> bill_validator::get_device_info() {
	std::promise<device_info>* promised_result = new std::promise<device_info>();
	handler_command new_command(handler_command_code::get_device_info, std::vector<std::uint8_t>(), promised_result);

	this->cmd_queue_mutex.lock();
	this->cmd_queue.push(new_command);
	this->cmd_queue_mutex.unlock();

	return promised_result->get_future();
}

std::future<std::set<cash_type>> bill_validator::get_enabled_cash_types() {
	std::promise<std::set<cash_type>>* promised_result = new std::promise<std::set<cash_type>>();
	handler_command new_command(handler_command_code::get_enabled_bill_types, std::vector<std::uint8_t>(), promised_result);

	this->cmd_queue_mutex.lock();
	this->cmd_queue.push(new_command);
	this->cmd_queue_mutex.unlock();

	return promised_result->get_future();
}

std::future<void> bill_validator::set_enabled_cash_types(const std::set<cash_type>& enabled_bill_types) {
	std::vector<std::uint8_t> command_data(enable_bill_types_command_data_size);
	std::uint8_t bill_type_number = 0;

	for (std::set<cash_type>::const_iterator iter = enabled_bill_types.cbegin(); iter != enabled_bill_types.cend(); ++iter) {
		std::map<std::uint8_t, cash_type>::const_iterator cash_type_iter = std::find_if(this->bill_types_by_numbers.cbegin(), this->bill_types_by_numbers.cend(),
			[iter](std::pair<std::uint8_t, cash_type> p) { return p.second == *iter; });

		if (cash_type_iter == this->bill_types_by_numbers.cend()) {
			throw std::exception("specified cash type is not supported");
		}

		bill_type_number = cash_type_iter->first;
		set_bit(command_data[3 - (bill_type_number / byte_size)], bill_type_number % byte_size);
		set_bit(command_data[3 - (bill_type_number / byte_size) + 3], bill_type_number % byte_size);
	}

	std::promise<void>* promised_result = new std::promise<void>();
	handler_command new_command(handler_command_code::set_enabled_bill_types, command_data, promised_result);

	this->cmd_queue_mutex.lock();
	this->cmd_queue.push(new_command);
	this->cmd_queue_mutex.unlock();

	return promised_result->get_future();
}

std::future<std::map<cash_type, bill_security_level>> bill_validator::get_cash_types_security_levels() {
	std::promise<std::map<cash_type, bill_security_level>>* promised_result = new std::promise<std::map<cash_type, bill_security_level>>();
	handler_command new_command(handler_command_code::get_bill_types_security_levels, std::vector<std::uint8_t>(), promised_result);

	this->cmd_queue_mutex.lock();
	this->cmd_queue.push(new_command);
	this->cmd_queue_mutex.unlock();

	return promised_result->get_future();
}

std::future<void> bill_validator::set_cash_types_security_levels(const std::map<cash_type, bill_security_level>& security_levels) {
	std::vector<std::uint8_t> command_data(set_security_command_data_size);
	std::uint8_t bill_type_number = 0;

	for (std::map<cash_type, bill_security_level>::const_iterator iter = security_levels.cbegin(); iter != security_levels.cend(); ++iter) {
		if (iter->second == bill_security_level::high) {
			std::map<std::uint8_t, cash_type>::const_iterator cash_type_iter = std::find_if(this->bill_types_by_numbers.cbegin(), this->bill_types_by_numbers.cend(),
				[iter](std::pair<std::uint8_t, cash_type> p) { return p.second == iter->first; });

			if (cash_type_iter == this->bill_types_by_numbers.cend()) {
				throw std::exception("specified cash type is not supported");
			}

			bill_type_number = cash_type_iter->first;
			set_bit(command_data[bill_type_number / byte_size], bill_type_number);
		}
	}

	std::promise<void>* promised_result = new std::promise<void>();
	handler_command new_command(handler_command_code::set_bill_types_security_levels, command_data, promised_result);

	this->cmd_queue_mutex.lock();
	this->cmd_queue.push(new_command);
	this->cmd_queue_mutex.unlock();

	return promised_result->get_future();
}

std::future<std::set<cash_type>> bill_validator::get_cash_types() {
	std::promise<std::set<cash_type>>* promised_result = new std::promise<std::set<cash_type>>();
	handler_command new_command(handler_command_code::get_bill_types, std::vector<std::uint8_t>(), promised_result);

	this->cmd_queue_mutex.lock();
	this->cmd_queue.push(new_command);
	this->cmd_queue_mutex.unlock();

	return promised_result->get_future();
}

void bill_validator::operate() {
	device_state previous_device_state;
	device_state current_device_state;
	bool initialization_required = true;

	while (this->thread_is_working) {
		this->reset();
		this->connected_device_info = this->request_device_info();
		this->bill_types_by_numbers = this->request_bill_table();

		// init completed
		initialization_required = false;

		while ((this->thread_is_working) && (!initialization_required)) {
			previous_device_state = current_device_state;
			current_device_state = this->poll();

			if (previous_device_state.code != current_device_state.code) {
				switch (previous_device_state.code) {
					case device_state_code::drop_cassette_out_of_pos: {
						this->connected_device_operator->drop_cassette_installed();
						initialization_required = true;
						continue; // return to outer cycle
					}
				}

				switch (current_device_state.code) {
					case device_state_code::drop_cassette_full: {
						this->connected_device_operator->drop_cassette_full();
						break;
					}
					case device_state_code::drop_cassette_out_of_pos: {
						this->connected_device_operator->drop_cassette_removed();
						break;
					}
					case device_state_code::validator_jammed:
					case device_state_code::drop_cassette_jammed: {
						// TODO
					}
					case device_state_code::failure: {
						// TODO
					}
					case device_state_code::escrow_pos: {
						std::future<cash_action> future_result = this->connected_device_operator->request_cash_action(this->bill_types_by_numbers.at(current_device_state.info));

						if (future_result.wait_for(std::chrono::seconds(10)) == std::future_status::timeout) {
							this->return_bill();
						}

						switch (future_result.get()) {
							case cash_action::accept_cash: {
								this->stack_bill();
								break;
							}
							case cash_action::hold_cash: {
								// TODO: redesign escrow_pos state handling using inner poll() loop
								current_device_state = device_state(device_state_code::idling, 0); // small hack to repeat cash action request
								this->hold_bill();
								break;
							}
							case cash_action::return_cash: {
								this->return_bill();
								break;
							}
						}

						break;
					}
					case device_state_code::bill_stacked: {
						this->connected_device_operator->cash_accepted(this->bill_types_by_numbers.at(current_device_state.info));
						break;
					}
					case device_state_code::bill_returned: {
						this->connected_device_operator->cash_returned(this->bill_types_by_numbers.at(current_device_state.info));
						break;
					}
				}
			}

			this->cmd_queue_mutex.lock();
			if (!this->cmd_queue.empty()) {
				handler_command current_command = this->cmd_queue.front();
				this->cmd_queue.pop();
				this->cmd_queue_mutex.unlock();

				switch (current_command.code) {
					case handler_command_code::get_bill_types: {
						this->get_bill_types_handler(current_command.data, current_command.result);
						break;
					}
					case handler_command_code::get_bill_types_security_levels: {
						this->get_bill_types_security_levels_handler(current_command.data, current_command.result);
						break;
					}
					case handler_command_code::get_device_info: {
						this->get_device_info_handler(current_command.data, current_command.result);
						break;
					}
					case handler_command_code::get_enabled_bill_types: {
						this->get_enabled_bill_types_handler(current_command.data, current_command.result);
						break;
					}
					case handler_command_code::set_bill_types_security_levels: {
						this->set_bill_types_security_levels_handler(current_command.data, current_command.result);
						break;
					}
					case handler_command_code::set_enabled_bill_types: {
						this->set_enabled_bill_types_handler(current_command.data, current_command.result);
						break;
					}
				}
			} else {
				this->cmd_queue_mutex.unlock();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
}

void bill_validator::reset() {
	device_command reset_command(device_command_code::reset, std::vector<std::uint8_t>());

	this->send_command(reset_command);
}

bill_validator::device_state bill_validator::poll() {
	device_command poll_command(device_command_code::poll, std::vector<std::uint8_t>());

	std::vector<std::uint8_t> response = this->get_command_result(poll_command);

	if (response.size() == poll_min_result_data_size) {
		return device_state((device_state_code)response[0], 0);
	}

	if (response.size() == poll_max_result_data_size) {
		return device_state((device_state_code)response[0], (device_state_info)response[1]);
	}

	throw std::exception("invalid data received");
}

void bill_validator::stack_bill() {
	device_command stack_bill_command(device_command_code::stack_bill, std::vector<std::uint8_t>());

	this->send_command(stack_bill_command);
}

void bill_validator::return_bill() {
	device_command return_bill_command(device_command_code::return_bill, std::vector<std::uint8_t>());

	this->send_command(return_bill_command);
}

device_info bill_validator::request_device_info() {
	const device_command get_device_info_command(device_command_code::identification, std::vector<std::uint8_t>());

	const std::vector<std::uint8_t> response = this->get_command_result(get_device_info_command);

	if (response.size() != identification_result_data_size) {
		throw std::exception("invalid data received");
	}

	const std::string part_number = trim(std::string(response.cbegin(), response.cbegin() + 15));
	const std::string serial_number = trim(std::string(response.cbegin() + 15, response.cbegin() + 15 + 12));
	std::vector<std::uint8_t> asset_number_bytes(8);
	std::copy(response.cbegin() + 15 + 12, response.cend(), asset_number_bytes.begin() + 1);
	const std::uint64_t asset_number = this->read_uint64(asset_number_bytes);

	return device_info(part_number, serial_number, asset_number);
}

void bill_validator::hold_bill() {
	device_command hold_bill_command(device_command_code::hold_bill, std::vector<std::uint8_t>());

	this->send_command(hold_bill_command);
}

std::map<std::uint8_t, cash_type> bill_validator::request_bill_table() {
	const device_command get_bill_table_command(device_command_code::get_bill_table, std::vector<std::uint8_t>());

	const std::vector<std::uint8_t> response = this->get_command_result(get_bill_table_command);

	if (response.size() != get_bill_table_result_data_size) {
		throw std::exception("invalid data received");
	}

	std::map<std::uint8_t, cash_type> bill_types_by_numbers;

	for (std::uint8_t bill_type_number = 0; bill_type_number < bill_types_count_max; ++bill_type_number) {
		const std::size_t offset = bill_type_number * bill_type_record_size;

		if (response[offset] == 0) {
			continue;
		}

		std::string country_code;
		country_code += (char)(response[offset + 1]);
		country_code += (char)(response[offset + 2]);
		country_code += (char)(response[offset + 3]);

		// TODO: country to currency mapping required
		const std::string currency_code = country_code;

		const std::uint64_t minor_currency_units_per_major = 100; // currency: RUB
		std::uint64_t denomination = 0;
		if (is_bit_set(response[offset + 4], exponent_sign_bit_number)) {
			denomination = response[offset] * minor_currency_units_per_major;

			if (denomination % (power(currency_base, get_abs_exponent(response[offset + 4]))) != 0) {
				throw std::exception("invalid cash type");
			}

			denomination /= (power(currency_base, get_abs_exponent(response[offset + 4])));
		} else {
			denomination = response[offset] * minor_currency_units_per_major;
			denomination *= (power(currency_base, get_abs_exponent(response[offset + 4])));
		}

		bill_types_by_numbers[bill_type_number] = cash_type(currency_code, denomination);
	}

	return bill_types_by_numbers;
}

void bill_validator::get_bill_types_handler(const std::vector<std::uint8_t>& data, void* untyped_result) {
	std::set<cash_type> bill_types;

	std::transform(this->bill_types_by_numbers.begin(), this->bill_types_by_numbers.end(), std::inserter(bill_types, bill_types.end()),
		[](std::pair<std::uint8_t, cash_type> p) { return p.second; });

	std::promise<std::set<cash_type>>* result = reinterpret_cast<std::promise<std::set<cash_type>>*>(untyped_result);
	result->set_value(bill_types);
	delete result;
}

void bill_validator::get_device_info_handler(const std::vector<std::uint8_t>& data, void* untyped_result) {
	std::promise<device_info>* result = reinterpret_cast<std::promise<device_info>*>(untyped_result);
	result->set_value(this->connected_device_info);
	delete result;
}

void bill_validator::get_enabled_bill_types_handler(const std::vector<std::uint8_t>& data, void* untyped_result) {
	std::promise<std::set<cash_type>>* result = reinterpret_cast<std::promise<std::set<cash_type>>*>(untyped_result);

	try {
		device_command get_enabled_bill_types_command(device_command_code::get_status, data);

		std::vector<std::uint8_t> response = this->get_command_result(get_enabled_bill_types_command);

		if (response.size() != get_status_result_data_size) {
			throw std::exception("invalid data received");
		}

		// process the first 3 bytes of the response related to the enabled bill types info
		std::set<cash_type> enabled_bill_types;
		std::uint8_t bill_type_number = 0;

		for (std::vector<std::uint8_t>::const_reverse_iterator iter = response.crbegin() + 3; iter != response.crend(); ++iter) {
			for (std::size_t bit_number = 0; bit_number < byte_size; ++bit_number) {
				if (is_bit_set(*iter, bill_type_number % byte_size)) {
					if (this->bill_types_by_numbers.count(bill_type_number) > 0) {
						enabled_bill_types.insert(this->bill_types_by_numbers.at(bill_type_number));
					}
				}
				++bill_type_number;
			}
		}

		result->set_value(enabled_bill_types);
		delete result;
	} catch (std::exception) {
		result->set_exception(std::make_exception_ptr(std::exception("command processing error")));
		delete result;
		throw;
	}
}

void bill_validator::set_enabled_bill_types_handler(const std::vector<std::uint8_t>& data, void* untyped_result) {
	std::promise<void>* result = reinterpret_cast<std::promise<void>*>(untyped_result);

	try {
		device_command set_enabled_bill_types_command(device_command_code::enable_bill_types, data);

		this->send_command(set_enabled_bill_types_command);

		result->set_value();
		delete result;
	} catch (std::exception) {
		result->set_exception(std::make_exception_ptr(std::exception("command processing error")));
		delete result;
		throw;
	}
}

void bill_validator::get_bill_types_security_levels_handler(const std::vector<std::uint8_t>& data, void* untyped_result) {
	std::promise<std::map<cash_type, bill_security_level>>* result = reinterpret_cast<std::promise<std::map<cash_type, bill_security_level>>*>(untyped_result);

	try {
		device_command get_bill_types_security_levels_command(device_command_code::get_status, data);

		std::vector<std::uint8_t> response = this->get_command_result(get_bill_types_security_levels_command);

		if (response.size() != get_status_result_data_size) {
			throw std::exception("invalid data received");
		}

		// process the second 3 bytes of the response related to the bill types security levels info
		std::map<cash_type, bill_security_level> bill_types_security_levels;
		std::uint8_t bill_type_number = 0;

		for (std::vector<std::uint8_t>::const_reverse_iterator iter = response.crbegin(); iter != response.crbegin() + 3; ++iter) {
			for (std::size_t bit_number = 0; bit_number < byte_size; ++bit_number) {
				if (is_bit_set(*iter, bill_type_number % byte_size)) {
					bill_types_security_levels[this->bill_types_by_numbers.at(bill_type_number)] = bill_security_level::high;
				} else {
					bill_types_security_levels[this->bill_types_by_numbers.at(bill_type_number)] = bill_security_level::normal;
				}
				++bill_type_number;
			}
		}

		result->set_value(bill_types_security_levels);
		delete result;
	} catch (std::exception) {
		result->set_exception(std::make_exception_ptr(std::exception("command processing error")));
		delete result;
		throw;
	}
}

void bill_validator::set_bill_types_security_levels_handler(const std::vector<std::uint8_t>& data, void* untyped_result) {
	std::promise<void>* result = reinterpret_cast<std::promise<void>*>(untyped_result);

	try {
		device_command set_bill_types_security_levels_command(device_command_code::set_security, data);

		this->send_command(set_bill_types_security_levels_command);

		result->set_value();
		delete result;
	} catch (std::exception) {
		result->set_exception(std::make_exception_ptr(std::exception("command processing error")));
		delete result;
		throw;
	}
}

std::uint16_t bill_validator::read_uint16(const frame& frame) const {
	return ((std::uint16_t)(frame[1] << 8)) | frame[0];
}

std::uint64_t bill_validator::read_uint64(const frame& frame) const {
	assert(frame.size() >= sizeof(std::uint64_t));

	std::vector<std::uint8_t> bytes(sizeof(std::uint64_t));
	// assume little-endian byte order architecture
	std::copy(frame.crbegin(), frame.crbegin() + 8, bytes.begin());
	return *reinterpret_cast<const std::uint64_t*>(bytes.data());
}

void bill_validator::write_uint16(frame& frame, std::uint16_t value) const {
	frame.push_back((std::uint8_t)(value & 0xFF));
	frame.push_back((std::uint8_t)(value >> 8));
}

bill_validator::frame bill_validator::build_command_frame(const device_command& command) const {
	frame command_frame(command.data);
	// 0 is a reserved byte for the frame length
	command_frame.insert(command_frame.begin(), { sync, bill_validator_addr, 0, (std::uint8_t)command.code });
	// set the frame length considering the frame check sequence size
	command_frame[2] = command_frame.size() + sizeof(crc16);
	// add the frame check sequence
	this->write_uint16(command_frame, get_crc(command_frame));

	return command_frame;
}

std::vector<std::uint8_t> bill_validator::get_command_result(const device_command& command) {
	const frame command_frame = this->build_command_frame(command);

	bool response_received = false;
	std::vector<std::uint8_t> header(header_size);
	std::vector<std::uint8_t> payload;
	std::vector<std::uint8_t> crc(sizeof(crc16));

	try {
		for (int try_count = 3; (!response_received) && (try_count > 0); --try_count) {
			// try to receive not nak response
			write(this->serial_port, buffer(command_frame));
			// TODO: async variant (timeout handling)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			bool frame_received = false;
			for (int try_count = 5; (!frame_received) && (try_count > 0); --try_count) {
				// try to receive the frame intended for the bill validator controller
				read(this->serial_port, buffer(header, header_size));

				if (header[sync_offset] != sync) {
					throw std::exception("synchronisation error");
				}

				const std::size_t payload_size = header[lng_offset] - header_size - sizeof(crc16);
				payload.resize(payload_size);
				read(this->serial_port, buffer(payload, payload_size));
				assert(payload.size() == payload_size);
				read(this->serial_port, buffer(crc, sizeof(crc16)));

				std::vector<std::uint8_t> response;
				response.insert(response.end(), header.cbegin(), header.cend());
				response.insert(response.end(), payload.cbegin(), payload.cend());

				if (this->get_crc(response) != this->read_uint16(crc)) {
					this->send_nak(header[adr_offset]);
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
					throw std::exception("crc error");
				}

				if (header[adr_offset] == command_frame[adr_offset]) {
					frame_received = true;
				}
			}

			if (!frame_received) {
				throw std::exception("unable to receive data from bill validator");
			}

			assert(payload.size() > 0);
			if ((payload.size() == 1) && (payload[0] == ill_cmd)) {
				// process illegal command packet
				throw std::exception("illegal command");
			} else if ((payload.size() == 1) && (payload[0] == nak)) {
				// process nak packet
				// nothing to do
			} else {
				// process data packet
				this->send_ack(header[adr_offset]);
				response_received = true;
			}
		}
	} catch (boost::system::system_error) {
		throw std::exception("serial port read-write error");
	}

	if (!response_received) {
		// process nak response
		throw std::exception("command was not correctly received by bill validator");
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	return payload;
}

void bill_validator::send_command(const device_command& command) {
	const frame command_frame = this->build_command_frame(command);

	bool response_received = false;
	std::vector<std::uint8_t> header(header_size);
	std::vector<std::uint8_t> payload;
	std::vector<std::uint8_t> crc(sizeof(crc16));

	try {
		for (int try_count = 3; (!response_received) && (try_count > 0); --try_count) {
			// try to receive not nak response
			write(this->serial_port, buffer(command_frame));
			// TODO: async variant (timeout handling)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			bool frame_received = false;
			for (int try_count = 5; (!frame_received) && (try_count > 0); --try_count) {
				// try to receive the frame intended for the bill validator controller
				read(this->serial_port, buffer(header, header_size));

				if (header[sync_offset] != sync) {
					throw std::exception("synchronisation error");
				}

				const std::size_t payload_size = header[lng_offset] - header_size - sizeof(crc16);
				payload.resize(payload_size);
				read(this->serial_port, buffer(payload, payload_size));
				assert(payload.size() == payload_size);
				read(this->serial_port, buffer(crc, sizeof(crc16)));

				std::vector<std::uint8_t> response;
				response.insert(response.end(), header.cbegin(), header.cend());
				response.insert(response.end(), payload.cbegin(), payload.cend());

				if (this->get_crc(response) != this->read_uint16(crc)) {
					this->send_nak(header[adr_offset]);
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
					throw std::exception("crc error");
				}

				if (header[adr_offset] == command_frame[adr_offset]) {
					frame_received = true;
				}
			}

			if (!frame_received) {
				throw std::exception("unable to receive data from bill validator");
			}

			// only control packet is expected
			assert(payload.size() == 1);
			// process control packet
			if (payload[0] == ill_cmd) {
				throw std::exception("illegal command");
			}

			if (payload[0] == ack) {
				response_received = true;
			} else if (payload[0] != nak) {
					throw std::exception("invalid payload");
			}
		}
	} catch (boost::system::system_error) {
		throw std::exception("serial port read-write error");
	}

	if (!response_received) {
		// process nak response
		throw std::exception("command was not correctly received by bill validator");
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

bill_validator::crc16 bill_validator::get_crc(const frame& frame) const {
	std::uint16_t crc = 0;

	for (std::size_t i = 0; i < frame.size(); i++) {
		crc ^= frame[i];

		for (std::uint8_t j = 0; j < 8; j++) {
			if (crc & 0x0001) {
				crc >>= 1;
				crc ^= POLYNOMIAL;
			} else {
				crc >>= 1;
			}
		}
	}

	return crc;
}

void bill_validator::send_ack(std::uint8_t device_address) {
	frame ack_frame;
	// 0 is a reserved byte for the frame length
	ack_frame.insert(ack_frame.begin(), { sync, device_address, 0, ack });
	// set the frame length considering the frame check sequence size
	ack_frame[2] = ack_frame.size() + sizeof(crc16);
	// add the frame check sequence
	this->write_uint16(ack_frame, this->get_crc(ack_frame));

	write(this->serial_port, buffer(ack_frame));
}

void bill_validator::send_nak(std::uint8_t device_address) {
	frame nak_frame;
	// 0 is a reserved byte for the frame length
	nak_frame.insert(nak_frame.begin(), { sync, device_address, 0, nak });
	// set the frame length considering the frame check sequence size
	nak_frame[2] = nak_frame.size() + sizeof(crc16);
	// add the frame check sequence
	this->write_uint16(nak_frame, this->get_crc(nak_frame));

	write(this->serial_port, buffer(nak_frame));
}
