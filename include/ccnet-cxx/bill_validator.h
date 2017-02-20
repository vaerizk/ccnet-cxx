#ifndef CCNET_BILL_VALIDATOR_H
#define CCNET_BILL_VALIDATOR_H

#include <map>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include "ccnet.h"

namespace ccnet {

	class bill_validator {
		public:
			bill_validator(const std::string& port_name, bill_validator_operator* bill_validator_operator);

			bill_validator(const bill_validator& other) = delete;
			//bill_validator(bill_validator&& other);

			~bill_validator();

			bill_validator& operator=(const bill_validator& other) = delete;

			std::future<device_info> get_device_info();
			std::future<std::set<cash_type>> get_enabled_cash_types();
			std::future<void> set_enabled_cash_types(const std::set<cash_type>& enabled_cash_types);
			std::future<std::map<cash_type, bill_security_level>> get_cash_types_security_levels();
			std::future<void> set_cash_types_security_levels(const std::map<cash_type, bill_security_level>& security_levels);
			std::future<std::set<cash_type>> get_cash_types();

		private:
			typedef std::vector<std::uint8_t> frame;
			typedef std::uint16_t crc16;

			enum class device_state_code : std::uint8_t {
				unknown = 0x00,
				power_up = 0x10,
				power_up_with_bill_in_val = 0x11,
				power_up_with_bill_in_stack = 0x12,
				initialize = 0x13,
				idling = 0x14,
				accepting = 0x15,
				stacking = 0x17,
				returning = 0x18,
				unit_disabled = 0x19,
				holding = 0x1a,
				device_busy = 0x1b,
				rejecting = 0x1c,
				drop_cassette_full = 0x41,
				drop_cassette_out_of_pos = 0x42,
				validator_jammed = 0x43,
				drop_cassette_jammed = 0x44,
				cheated = 0x45,
				pause = 0x46,
				failure = 0x47,
				escrow_pos = 0x80,
				bill_stacked = 0x81,
				bill_returned = 0x82
			};

			typedef std::uint8_t device_state_info;

			//enum class device_state_info : std::uint8_t {
			//	absent = 0x00,
			//	reject_insertion = 0x60,
			//	reject_magnetic = 0x61,
			//	reject_remained_bill_in_head = 0x62,
			//	reject_multiplying = 0x63,
			//	reject_conveying = 0x64,
			//	reject_identification = 0x65,
			//	reject_verification = 0x66,
			//	reject_optic = 0x67,
			//	reject_inhibit = 0x68,
			//	reject_capacity = 0x69,
			//	reject_operation = 0x6a,
			//	reject_length = 0x6c,
			//	reject_uv = 0x6d,
			//	reject_unrecognised_barcode = 0x92,
			//	reject_barcode_char_num_error = 0x93,
			//	reject_barcode_start_seq_error = 0x94,
			//	reject_barcode_stop_seq_error = 0x95,
			//	failure_stack_motor = 0x50,
			//	failure_transport_motor_speed = 0x51,
			//	failure_transport_motor = 0x52,
			//	failure_aligning_motor = 0x53,
			//	failure_init_cassette_status = 0x54,
			//	failure_optic_canal = 0x55,
			//	failure_magnetic_canal = 0x56,
			//	failure_capacitance_canal = 0x5f
			//};

			struct device_state {
				device_state(device_state_code code = device_state_code::unknown, device_state_info info = 0) :
					code(code),
					info(info) { }

				bool operator==(const device_state& other) const;
				bool operator!=(const device_state& other) const;

				device_state_code code;
				device_state_info info;
			};

			enum class device_command_code : std::uint8_t {
				reset = 0x30,
				get_status = 0x31,
				set_security = 0x32,
				poll = 0x33,
				enable_bill_types = 0x34,
				stack_bill = 0x35,
				return_bill = 0x36,
				identification = 0x37,
				hold_bill = 0x38,
				set_barcode_parameters = 0x39,
				extract_barcode_data = 0x3a,
				get_bill_table = 0x41,
				download = 0x50,
				get_crc32 = 0x51,
				request_statistics = 0x60
			};

			struct device_command {
				device_command(device_command_code code, const std::vector<std::uint8_t>& data) :
					code(code),
					data(data) { }

				device_command_code code;
				std::vector<std::uint8_t> data;
			};

			enum class handler_command_code : std::uint8_t {
				get_bill_types,
				get_bill_types_security_levels,
				get_device_info,
				get_enabled_bill_types,
				set_bill_types_security_levels,
				set_enabled_bill_types
			};

			struct handler_command {
				handler_command(handler_command_code code, const std::vector<std::uint8_t>& data, void* result) :
					code(code),
					data(data),
					result(result) { }

				handler_command_code code;
				std::vector<std::uint8_t> data;
				void* result;
			};

		private:
			void operate();
			void reset();
			device_state poll();
			void stack_bill();
			void return_bill();
			device_info request_device_info();
			void hold_bill();
			std::map<std::uint8_t, cash_type> request_bill_table();
			void get_bill_types_handler(const std::vector<std::uint8_t>& data, void* untyped_result);
			void get_device_info_handler(const std::vector<std::uint8_t>& data, void* untyped_result);
			void get_enabled_bill_types_handler(const std::vector<std::uint8_t>& data, void* untyped_result);
			void set_enabled_bill_types_handler(const std::vector<std::uint8_t>& data, void* untyped_result);
			void get_bill_types_security_levels_handler(const std::vector<std::uint8_t>& data, void* untyped_result);
			void set_bill_types_security_levels_handler(const std::vector<std::uint8_t>& data, void* untyped_result);
			std::uint16_t read_uint16(const frame& frame) const;
			std::uint64_t read_uint64(const frame& frame) const;
			void write_uint16(frame& frame, std::uint16_t value) const;

			// constructs a frame
			// from a command, command data and service data
			// and adds the frame check sequence to the frame
			frame build_command_frame(const device_command& command) const;

			// accesses the bill validator
			// to process a command with an expected result
			std::vector<std::uint8_t> get_command_result(const device_command& command);
			// accesses the bill validator
			// to process a command without an expected result
			void send_command(const device_command& command);

			crc16 get_crc(const frame& frame) const;
			void send_ack(std::uint8_t device_address);
			void send_nak(std::uint8_t device_address);

		private:
			std::thread cmd_handler_thread;
			bool thread_is_working;
			std::queue<handler_command> cmd_queue;
			std::mutex cmd_queue_mutex;
			boost::asio::io_service io_service;
			boost::asio::serial_port serial_port;
			bill_validator_operator* connected_device_operator;
			device_info connected_device_info;
			std::map<std::uint8_t, cash_type> bill_types_by_numbers;

			static const std::uint8_t bill_types_count_max = 24;
			static const std::size_t bill_type_record_size = 5;

			// command data sizes in bytes
			static const std::size_t set_security_command_data_size = 3;
			static const std::size_t enable_bill_types_command_data_size = 6;

			// result data sizes in bytes
			static const std::size_t poll_min_result_data_size = 1;
			static const std::size_t poll_max_result_data_size = 2;
			static const std::size_t get_bill_table_result_data_size = bill_types_count_max * bill_type_record_size;
			static const std::size_t identification_result_data_size = 34;
			static const std::size_t get_status_result_data_size = 6;
	};

}

#endif // CCNET_BILL_VALIDATOR_H
