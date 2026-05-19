#include "power_serial/power_serial_client.h"

#include <serial/serial.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <string>

namespace {

struct ClientImpl {
  serial::Serial port;
};

ClientImpl *impl(ps_client_t *client) {
  return client ? static_cast<ClientImpl *>(client->impl) : nullptr;
}

const ClientImpl *impl(const ps_client_t *client) {
  return client ? static_cast<const ClientImpl *>(client->impl) : nullptr;
}

ps_status_t map_exception() {
  return PS_ERR_IO;
}

ps_status_t write_all(serial::Serial &port, const uint8_t *data, size_t len) {
  size_t written = 0;
  while (written < len) {
    size_t n = port.write(data + written, len - written);
    if (n == 0) return PS_ERR_TIMEOUT;
    written += n;
  }
  return PS_OK;
}

}  // namespace

extern "C" {

ps_status_t ps_client_open(ps_client_t *client, const char *device, unsigned int baudrate, unsigned int timeout_ms, uint8_t adr) {
  if (!client || !device) return PS_ERR_INVALID_ARG;
  std::memset(client, 0, sizeof(*client));
  client->timeout_ms = timeout_ms;
  client->adr = adr;

  try {
    std::unique_ptr<ClientImpl> holder(new ClientImpl());
    serial::Timeout timeout = serial::Timeout::simpleTimeout(timeout_ms);
    holder->port.setPort(device);
    holder->port.setBaudrate(baudrate);
    holder->port.setTimeout(timeout);
    holder->port.setBytesize(serial::eightbits);
    holder->port.setParity(serial::parity_none);
    holder->port.setStopbits(serial::stopbits_one);
    holder->port.setFlowcontrol(serial::flowcontrol_none);
    holder->port.open();
    holder->port.flush();
    client->impl = holder.release();
    return PS_OK;
  } catch (const std::invalid_argument &) {
    return PS_ERR_UNSUPPORTED;
  } catch (const std::exception &) {
    return map_exception();
  }
}

void ps_client_close(ps_client_t *client) {
  ClientImpl *p = impl(client);
  if (!p) return;
  try {
    if (p->port.isOpen()) p->port.close();
  } catch (const std::exception &) {
  }
  delete p;
  client->impl = nullptr;
}

int ps_client_is_open(const ps_client_t *client) {
  const ClientImpl *p = impl(client);
  if (!p) return 0;
  try {
    return p->port.isOpen() ? 1 : 0;
  } catch (const std::exception &) {
    return 0;
  }
}

ps_status_t ps_client_request(ps_client_t *client, uint8_t cid2, const uint8_t *info, size_t info_len, ps_frame_t *response) {
  ClientImpl *p = impl(client);
  if (!p || !response) return PS_ERR_INVALID_ARG;

  try {
    ps_frame_t request;
    ps_status_t st = ps_make_command(client->adr, cid2, info, info_len, &request);
    if (st != PS_OK) return st;

    uint8_t encoded[PS_MAX_FRAME_BYTES];
    size_t encoded_len = 0;
    st = ps_encode_frame(&request, encoded, sizeof(encoded), &encoded_len);
    if (st != PS_OK) return st;

    p->port.flushInput();
    st = write_all(p->port, encoded, encoded_len);
    if (st != PS_OK) return st;

    uint8_t buf[PS_MAX_FRAME_BYTES];
    size_t len = 0;
    bool seen_soi = false;
    for (;;) {
      uint8_t c = 0;
      size_t n = p->port.read(&c, 1);
      if (n == 0) return PS_ERR_TIMEOUT;
      if (!seen_soi) {
        if (c != PS_SOI) continue;
        seen_soi = true;
        len = 0;
      }
      if (len >= sizeof(buf)) return PS_ERR_BUFFER_TOO_SMALL;
      buf[len++] = c;
      if (c == PS_EOI) break;
    }
    return ps_decode_frame(buf, len, response);
  } catch (const std::exception &) {
    return map_exception();
  }
}

ps_status_t ps_client_get_protocol_version(ps_client_t *client, ps_frame_t *response) {
  return ps_client_request(client, 0x4f, nullptr, 0, response);
}

ps_status_t ps_client_get_pack_count(ps_client_t *client, uint8_t *pack_count) {
  if (!pack_count) return PS_ERR_INVALID_ARG;
  ps_frame_t response;
  ps_status_t st = ps_client_request(client, 0x90, nullptr, 0, &response);
  if (st != PS_OK) return st;
  if (response.info_len < 1u) return PS_ERR_BAD_LENGTH;
  *pack_count = response.info[0];
  return PS_OK;
}

ps_status_t ps_client_get_device_info(ps_client_t *client, ps_device_info_t *out) {
  if (!out) return PS_ERR_INVALID_ARG;
  ps_frame_t response;
  ps_status_t st = ps_client_request(client, 0x51, nullptr, 0, &response);
  if (st != PS_OK) return st;
  return ps_parse_device_info(response.info, response.info_len, out);
}

ps_status_t ps_client_set_baudrate(ps_client_t *client, uint8_t command, ps_frame_t *response) {
  return ps_client_request(client, 0x91, &command, 1, response);
}

ps_status_t ps_client_get_pack_data(ps_client_t *client, uint8_t command, ps_pack_response_t *out) {
  if (!out) return PS_ERR_INVALID_ARG;
  ps_frame_t response;
  ps_status_t st = ps_client_request(client, 0x42, &command, 1, &response);
  if (st != PS_OK) return st;
  return ps_parse_pack_response(response.info, response.info_len, out);
}

ps_status_t ps_client_control(ps_client_t *client, uint8_t command, ps_frame_t *response) {
  return ps_client_request(client, 0x99, &command, 1, response);
}

ps_status_t ps_client_set_buzzer(ps_client_t *client, int enabled, ps_frame_t *response) {
  return ps_client_control(client, enabled ? 0x0c : 0x0d, response);
}

}  // extern "C"
