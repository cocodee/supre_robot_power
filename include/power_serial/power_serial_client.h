#ifndef POWER_SERIAL_POWER_SERIAL_CLIENT_H
#define POWER_SERIAL_POWER_SERIAL_CLIENT_H

#include "power_serial/power_serial.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void *impl;
  unsigned int timeout_ms;
  uint8_t adr;
} ps_client_t;

ps_status_t ps_client_open(ps_client_t *client, const char *device, unsigned int baudrate, unsigned int timeout_ms, uint8_t adr);
void ps_client_close(ps_client_t *client);
int ps_client_is_open(const ps_client_t *client);

ps_status_t ps_client_request(ps_client_t *client, uint8_t cid2, const uint8_t *info, size_t info_len, ps_frame_t *response);
ps_status_t ps_client_get_protocol_version(ps_client_t *client, ps_frame_t *response);
ps_status_t ps_client_get_pack_count(ps_client_t *client, uint8_t *pack_count);
ps_status_t ps_client_get_device_info(ps_client_t *client, ps_device_info_t *out);
ps_status_t ps_client_set_baudrate(ps_client_t *client, uint8_t command, ps_frame_t *response);
ps_status_t ps_client_get_pack_data(ps_client_t *client, uint8_t command, ps_pack_response_t *out);
ps_status_t ps_client_control(ps_client_t *client, uint8_t command, ps_frame_t *response);
ps_status_t ps_client_set_buzzer(ps_client_t *client, int enabled, ps_frame_t *response);

#ifdef __cplusplus
}
#endif

#endif
