#include "power_serial/power_serial.h"
#include "power_serial/power_serial_client.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;

static void throw_if_error(ps_status_t status) {
  if (status != PS_OK) throw std::runtime_error(ps_status_string(status));
}

static std::vector<uint8_t> frame_info(const ps_frame_t &frame) {
  return std::vector<uint8_t>(frame.info, frame.info + frame.info_len);
}

class PyClient {
 public:
  PyClient(const std::string &device, unsigned int baudrate, unsigned int timeout_ms, uint8_t adr) {
    client_.impl = nullptr;
    throw_if_error(ps_client_open(&client_, device.c_str(), baudrate, timeout_ms, adr));
  }

  ~PyClient() {
    ps_client_close(&client_);
  }

  PyClient(const PyClient &) = delete;
  PyClient &operator=(const PyClient &) = delete;

  ps_frame_t request(uint8_t cid2, const std::vector<uint8_t> &info) {
    ps_frame_t response;
    throw_if_error(ps_client_request(&client_, cid2, info.empty() ? nullptr : info.data(), info.size(), &response));
    return response;
  }

  ps_frame_t get_protocol_version() {
    ps_frame_t response;
    throw_if_error(ps_client_get_protocol_version(&client_, &response));
    return response;
  }

  uint8_t get_pack_count() {
    uint8_t count = 0;
    throw_if_error(ps_client_get_pack_count(&client_, &count));
    return count;
  }

  ps_device_info_t get_device_info() {
    ps_device_info_t info;
    throw_if_error(ps_client_get_device_info(&client_, &info));
    return info;
  }

  ps_pack_response_t get_pack_data(uint8_t command) {
    ps_pack_response_t data;
    throw_if_error(ps_client_get_pack_data(&client_, command, &data));
    return data;
  }

  ps_frame_t set_baudrate(uint8_t command) {
    ps_frame_t response;
    throw_if_error(ps_client_set_baudrate(&client_, command, &response));
    return response;
  }

  ps_frame_t control(uint8_t command) {
    ps_frame_t response;
    throw_if_error(ps_client_control(&client_, command, &response));
    return response;
  }

  ps_frame_t set_buzzer(bool enabled) {
    ps_frame_t response;
    throw_if_error(ps_client_set_buzzer(&client_, enabled ? 1 : 0, &response));
    return response;
  }

 private:
  ps_client_t client_;
};

PYBIND11_MODULE(power_serial, m) {
  m.doc() = "Supre robot power serial protocol SDK";

  py::enum_<ps_status_t>(m, "Status")
      .value("OK", PS_OK)
      .value("INVALID_ARG", PS_ERR_INVALID_ARG)
      .value("BUFFER_TOO_SMALL", PS_ERR_BUFFER_TOO_SMALL)
      .value("BAD_FRAME", PS_ERR_BAD_FRAME)
      .value("BAD_HEX", PS_ERR_BAD_HEX)
      .value("BAD_LENGTH", PS_ERR_BAD_LENGTH)
      .value("BAD_CHECKSUM", PS_ERR_BAD_CHECKSUM)
      .value("IO", PS_ERR_IO)
      .value("TIMEOUT", PS_ERR_TIMEOUT)
      .value("UNSUPPORTED", PS_ERR_UNSUPPORTED);

  py::class_<ps_frame_t>(m, "Frame")
      .def(py::init([](uint8_t ver, uint8_t adr, uint8_t cid1, uint8_t cid2, const std::vector<uint8_t> &info) {
        ps_frame_t frame;
        if (info.size() > PS_MAX_INFO_BYTES) throw std::runtime_error("info too large");
        frame.ver = ver;
        frame.adr = adr;
        frame.cid1 = cid1;
        frame.cid2 = cid2;
        frame.info_len = info.size();
        frame.length = ps_length_word(info.size());
        frame.checksum = 0;
        std::copy(info.begin(), info.end(), frame.info);
        return frame;
      }),
      py::arg("ver") = PS_DEFAULT_VER,
      py::arg("adr") = PS_DEFAULT_ADR,
      py::arg("cid1") = PS_CID1_POWER,
      py::arg("cid2") = 0,
      py::arg("info") = std::vector<uint8_t>{})
      .def_readwrite("ver", &ps_frame_t::ver)
      .def_readwrite("adr", &ps_frame_t::adr)
      .def_readwrite("cid1", &ps_frame_t::cid1)
      .def_readwrite("cid2", &ps_frame_t::cid2)
      .def_readwrite("length", &ps_frame_t::length)
      .def_readwrite("checksum", &ps_frame_t::checksum)
      .def_property_readonly("info", &frame_info);

  py::class_<ps_device_info_t>(m, "DeviceInfo")
      .def_readonly("device_name", &ps_device_info_t::device_name)
      .def_readonly("software_version", &ps_device_info_t::software_version)
      .def_readonly("manufacturer", &ps_device_info_t::manufacturer);

  py::class_<ps_status_flags_t>(m, "StatusFlags")
      .def_readonly("cell_fault_1_8", &ps_status_flags_t::cell_fault_1_8)
      .def_readonly("cell_fault_9_16", &ps_status_flags_t::cell_fault_9_16)
      .def_readonly("protection", &ps_status_flags_t::protection)
      .def_readonly("fet", &ps_status_flags_t::fet)
      .def_readonly("misc", &ps_status_flags_t::misc)
      .def_property_readonly("total_voltage_low", [](const ps_status_flags_t &f) { return ps_status_flag_total_voltage_low(&f) != 0; })
      .def_property_readonly("charge_over_temp", [](const ps_status_flags_t &f) { return ps_status_flag_charge_over_temp(&f) != 0; })
      .def_property_readonly("discharge_over_temp", [](const ps_status_flags_t &f) { return ps_status_flag_discharge_over_temp(&f) != 0; })
      .def_property_readonly("discharge_over_current", [](const ps_status_flags_t &f) { return ps_status_flag_discharge_over_current(&f) != 0; })
      .def_property_readonly("charge_over_current", [](const ps_status_flags_t &f) { return ps_status_flag_charge_over_current(&f) != 0; })
      .def_property_readonly("cell_low_voltage", [](const ps_status_flags_t &f) { return ps_status_flag_cell_low_voltage(&f) != 0; })
      .def_property_readonly("over_voltage", [](const ps_status_flags_t &f) { return ps_status_flag_over_voltage(&f) != 0; })
      .def_property_readonly("using_pack", [](const ps_status_flags_t &f) { return ps_status_flag_using_pack(&f) != 0; })
      .def_property_readonly("dfet_on", [](const ps_status_flags_t &f) { return ps_status_flag_dfet_on(&f) != 0; })
      .def_property_readonly("cfet_on", [](const ps_status_flags_t &f) { return ps_status_flag_cfet_on(&f) != 0; })
      .def_property_readonly("prefet_on", [](const ps_status_flags_t &f) { return ps_status_flag_prefet_on(&f) != 0; })
      .def_property_readonly("fully_charged", [](const ps_status_flags_t &f) { return ps_status_flag_fully_charged(&f) != 0; })
      .def_property_readonly("buzzer_on", [](const ps_status_flags_t &f) { return ps_status_flag_buzzer_on(&f) != 0; });

  py::class_<ps_pack_data_t>(m, "PackData")
      .def_readonly("pack_index", &ps_pack_data_t::pack_index)
      .def_readonly("cell_count", &ps_pack_data_t::cell_count)
      .def_readonly("temperature_count", &ps_pack_data_t::temperature_count)
      .def_readonly("charge_current_raw", &ps_pack_data_t::charge_current_raw)
      .def_readonly("total_voltage_raw", &ps_pack_data_t::total_voltage_raw)
      .def_readonly("discharge_current_raw", &ps_pack_data_t::discharge_current_raw)
      .def_readonly("status", &ps_pack_data_t::status)
      .def_property_readonly("cell_mv", [](const ps_pack_data_t &p) {
        return std::vector<uint16_t>(p.cell_mv, p.cell_mv + p.cell_count);
      })
      .def_property_readonly("temperatures_raw", [](const ps_pack_data_t &p) {
        return std::vector<int16_t>(p.temperatures_raw, p.temperatures_raw + p.temperature_count);
      });

  py::class_<ps_pack_response_t>(m, "PackResponse")
      .def_readonly("command_or_pack_count", &ps_pack_response_t::command_or_pack_count)
      .def_readonly("pack_count", &ps_pack_response_t::pack_count)
      .def_property_readonly("packs", [](const ps_pack_response_t &r) {
        return std::vector<ps_pack_data_t>(r.packs, r.packs + r.pack_count);
      })
      .def_property_readonly("warn_state", [](const ps_pack_response_t &r) {
        return std::vector<uint8_t>(r.warn_state, r.warn_state + r.warn_state_len);
      });

  py::class_<PyClient>(m, "Client")
      .def(py::init<const std::string &, unsigned int, unsigned int, uint8_t>(),
           py::arg("device"),
           py::arg("baudrate") = 9600,
           py::arg("timeout_ms") = 1000,
           py::arg("adr") = PS_DEFAULT_ADR)
      .def("request", &PyClient::request, py::arg("cid2"), py::arg("info") = std::vector<uint8_t>{})
      .def("get_protocol_version", &PyClient::get_protocol_version)
      .def("get_pack_count", &PyClient::get_pack_count)
      .def("get_device_info", &PyClient::get_device_info)
      .def("get_pack_data", &PyClient::get_pack_data, py::arg("command") = 0xff)
      .def("set_baudrate", &PyClient::set_baudrate)
      .def("control", &PyClient::control)
      .def("set_buzzer", &PyClient::set_buzzer);

  m.def("status_string", &ps_status_string);
  m.def("length_word", &ps_length_word);
  m.def("checksum_ascii", [](const py::bytes &data) {
    std::string s = data;
    return ps_checksum_ascii(reinterpret_cast<const uint8_t *>(s.data()), s.size());
  });
  m.def("make_command", [](uint8_t adr, uint8_t cid2, const std::vector<uint8_t> &info) {
    ps_frame_t frame;
    throw_if_error(ps_make_command(adr, cid2, info.empty() ? nullptr : info.data(), info.size(), &frame));
    return frame;
  }, py::arg("adr"), py::arg("cid2"), py::arg("info") = std::vector<uint8_t>{});
  m.def("encode_frame", [](const ps_frame_t &frame) {
    uint8_t out[PS_MAX_FRAME_BYTES];
    size_t out_len = 0;
    throw_if_error(ps_encode_frame(&frame, out, sizeof(out), &out_len));
    return py::bytes(reinterpret_cast<const char *>(out), out_len);
  });
  m.def("decode_frame", [](const py::bytes &data) {
    std::string s = data;
    ps_frame_t frame;
    throw_if_error(ps_decode_frame(reinterpret_cast<const uint8_t *>(s.data()), s.size(), &frame));
    return frame;
  });
  m.def("parse_device_info", [](const std::vector<uint8_t> &info) {
    ps_device_info_t parsed;
    throw_if_error(ps_parse_device_info(info.empty() ? nullptr : info.data(), info.size(), &parsed));
    return parsed;
  });
  m.def("parse_pack_response", [](const std::vector<uint8_t> &info) {
    ps_pack_response_t parsed;
    throw_if_error(ps_parse_pack_response(info.empty() ? nullptr : info.data(), info.size(), &parsed));
    return parsed;
  });
}
