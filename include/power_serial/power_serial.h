#ifndef POWER_SERIAL_POWER_SERIAL_H
#define POWER_SERIAL_POWER_SERIAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_SOI 0x7eu
#define PS_EOI 0x0du
#define PS_DEFAULT_VER 0x20u
#define PS_DEFAULT_ADR 0x01u
#define PS_CID1_POWER 0x46u

#define PS_MAX_INFO_BYTES 512u
#define PS_MAX_FRAME_BYTES (1u + 2u + 2u + 2u + 2u + 4u + (PS_MAX_INFO_BYTES * 2u) + 4u + 1u)

typedef enum {
  PS_OK = 0,
  PS_ERR_INVALID_ARG = -1,
  PS_ERR_BUFFER_TOO_SMALL = -2,
  PS_ERR_BAD_FRAME = -3,
  PS_ERR_BAD_HEX = -4,
  PS_ERR_BAD_LENGTH = -5,
  PS_ERR_BAD_CHECKSUM = -6,
  PS_ERR_IO = -7,
  PS_ERR_TIMEOUT = -8,
  PS_ERR_UNSUPPORTED = -9
} ps_status_t;

typedef struct {
  uint8_t ver;
  uint8_t adr;
  uint8_t cid1;
  uint8_t cid2;
  uint16_t length;
  uint8_t info[PS_MAX_INFO_BYTES];
  size_t info_len;
  uint16_t checksum;
} ps_frame_t;

typedef struct {
  char device_name[11];
  uint16_t software_version;
  char manufacturer[21];
} ps_device_info_t;

typedef struct {
  uint8_t cell_fault_1_8;
  uint8_t cell_fault_9_16;
  uint8_t protection;
  uint8_t fet;
  uint8_t misc;
  uint8_t raw[5];
} ps_status_flags_t;

typedef struct {
  uint8_t pack_index;
  uint8_t cell_count;
  uint16_t cell_mv[16];
  uint8_t temperature_count;
  int16_t temperatures_raw[16];
  int16_t charge_current_raw;
  uint16_t total_voltage_raw;
  int16_t discharge_current_raw;
  ps_status_flags_t status;
} ps_pack_data_t;

typedef struct {
  uint8_t command_or_pack_count;
  uint8_t pack_count;
  ps_pack_data_t packs[8];
  uint8_t warn_state[128];
  size_t warn_state_len;
} ps_pack_response_t;

const char *ps_status_string(ps_status_t status);

uint16_t ps_length_word(size_t info_len);
uint16_t ps_checksum_ascii(const uint8_t *ascii, size_t ascii_len);

ps_status_t ps_encode_frame(const ps_frame_t *frame, uint8_t *out, size_t out_cap, size_t *out_len);
ps_status_t ps_decode_frame(const uint8_t *data, size_t data_len, ps_frame_t *out);

ps_status_t ps_make_command(uint8_t adr, uint8_t cid2, const uint8_t *info, size_t info_len, ps_frame_t *out);

ps_status_t ps_parse_device_info(const uint8_t *info, size_t info_len, ps_device_info_t *out);
ps_status_t ps_parse_pack_response(const uint8_t *info, size_t info_len, ps_pack_response_t *out);

int ps_status_flag_total_voltage_low(const ps_status_flags_t *flags);
int ps_status_flag_charge_over_temp(const ps_status_flags_t *flags);
int ps_status_flag_discharge_over_temp(const ps_status_flags_t *flags);
int ps_status_flag_discharge_over_current(const ps_status_flags_t *flags);
int ps_status_flag_charge_over_current(const ps_status_flags_t *flags);
int ps_status_flag_cell_low_voltage(const ps_status_flags_t *flags);
int ps_status_flag_over_voltage(const ps_status_flags_t *flags);
int ps_status_flag_using_pack(const ps_status_flags_t *flags);
int ps_status_flag_dfet_on(const ps_status_flags_t *flags);
int ps_status_flag_cfet_on(const ps_status_flags_t *flags);
int ps_status_flag_prefet_on(const ps_status_flags_t *flags);
int ps_status_flag_fully_charged(const ps_status_flags_t *flags);
int ps_status_flag_buzzer_on(const ps_status_flags_t *flags);

#ifdef __cplusplus
}
#endif

#endif
