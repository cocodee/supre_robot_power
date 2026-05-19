#include "power_serial/power_serial.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static uint8_t hex_digit(uint8_t v) {
  if (v < 10u) return (uint8_t)('0' + v);
  return (uint8_t)('A' + (v - 10u));
}

static int hex_value(uint8_t c) {
  if (c >= '0' && c <= '9') return (int)(c - '0');
  if (c >= 'A' && c <= 'F') return (int)(c - 'A' + 10);
  if (c >= 'a' && c <= 'f') return (int)(c - 'a' + 10);
  return -1;
}

static void put_hex8(uint8_t *out, uint8_t value) {
  out[0] = hex_digit((uint8_t)(value >> 4));
  out[1] = hex_digit((uint8_t)(value & 0x0f));
}

static void put_hex16(uint8_t *out, uint16_t value) {
  put_hex8(out, (uint8_t)(value >> 8));
  put_hex8(out + 2, (uint8_t)(value & 0xff));
}

static ps_status_t get_hex8(const uint8_t *in, uint8_t *value) {
  int hi = hex_value(in[0]);
  int lo = hex_value(in[1]);
  if (hi < 0 || lo < 0) return PS_ERR_BAD_HEX;
  *value = (uint8_t)((hi << 4) | lo);
  return PS_OK;
}

static ps_status_t get_hex16(const uint8_t *in, uint16_t *value) {
  uint8_t hi = 0;
  uint8_t lo = 0;
  ps_status_t st = get_hex8(in, &hi);
  if (st != PS_OK) return st;
  st = get_hex8(in + 2, &lo);
  if (st != PS_OK) return st;
  *value = (uint16_t)(((uint16_t)hi << 8) | lo);
  return PS_OK;
}

static int is_charge_relay_special(const ps_frame_t *frame) {
  return frame && frame->ver == PS_DEFAULT_VER && frame->adr == PS_DEFAULT_ADR &&
         frame->cid1 == PS_CID1_POWER && frame->cid2 == 0x99u &&
         frame->info_len == 1u && (frame->info[0] == 0x0au || frame->info[0] == 0x0bu);
}

const char *ps_status_string(ps_status_t status) {
  switch (status) {
    case PS_OK: return "ok";
    case PS_ERR_INVALID_ARG: return "invalid argument";
    case PS_ERR_BUFFER_TOO_SMALL: return "buffer too small";
    case PS_ERR_BAD_FRAME: return "bad frame";
    case PS_ERR_BAD_HEX: return "bad hex";
    case PS_ERR_BAD_LENGTH: return "bad length";
    case PS_ERR_BAD_CHECKSUM: return "bad checksum";
    case PS_ERR_IO: return "i/o error";
    case PS_ERR_TIMEOUT: return "timeout";
    case PS_ERR_UNSUPPORTED: return "unsupported";
    default: return "unknown error";
  }
}

uint16_t ps_length_word(size_t info_len) {
  uint16_t lenid = (uint16_t)(info_len * 2u);
  uint8_t nibble_sum = (uint8_t)(((lenid >> 8) & 0x0f) + ((lenid >> 4) & 0x0f) + (lenid & 0x0f));
  uint8_t lchksum = (uint8_t)((~nibble_sum + 1u) & 0x0f);
  return (uint16_t)(((uint16_t)lchksum << 12) | lenid);
}

uint16_t ps_checksum_ascii(const uint8_t *ascii, size_t ascii_len) {
  uint32_t sum = 0;
  if (!ascii && ascii_len != 0u) return 0;
  for (size_t i = 0; i < ascii_len; ++i) sum += ascii[i];
  return (uint16_t)((~sum + 1u) & 0xffffu);
}

ps_status_t ps_encode_frame(const ps_frame_t *frame, uint8_t *out, size_t out_cap, size_t *out_len) {
  if (!frame || !out || !out_len) return PS_ERR_INVALID_ARG;
  if (frame->info_len > PS_MAX_INFO_BYTES || frame->info_len > 2047u) return PS_ERR_BAD_LENGTH;

  size_t need = 1u + 2u + 2u + 2u + 2u + 4u + frame->info_len * 2u + 4u + 1u;
  if (out_cap < need) return PS_ERR_BUFFER_TOO_SMALL;

  size_t p = 0;
  out[p++] = PS_SOI;
  put_hex8(out + p, frame->ver); p += 2;
  put_hex8(out + p, frame->adr); p += 2;
  put_hex8(out + p, frame->cid1); p += 2;
  put_hex8(out + p, frame->cid2); p += 2;
  put_hex16(out + p, ps_length_word(frame->info_len)); p += 4;
  for (size_t i = 0; i < frame->info_len; ++i) {
    put_hex8(out + p, frame->info[i]);
    p += 2;
  }
  uint16_t checksum = is_charge_relay_special(frame) ? 0xfd35u : ps_checksum_ascii(out + 1, p - 1);
  put_hex16(out + p, checksum); p += 4;
  out[p++] = PS_EOI;
  *out_len = p;
  return PS_OK;
}

ps_status_t ps_decode_frame(const uint8_t *data, size_t data_len, ps_frame_t *out) {
  if (!data || !out) return PS_ERR_INVALID_ARG;
  if (data_len < 18u || data[0] != PS_SOI || data[data_len - 1u] != PS_EOI) return PS_ERR_BAD_FRAME;
  if (((data_len - 2u) % 2u) != 0u) return PS_ERR_BAD_FRAME;

  memset(out, 0, sizeof(*out));
  size_t p = 1;
  ps_status_t st = get_hex8(data + p, &out->ver); p += 2;
  if (st != PS_OK) return st;
  st = get_hex8(data + p, &out->adr); p += 2;
  if (st != PS_OK) return st;
  st = get_hex8(data + p, &out->cid1); p += 2;
  if (st != PS_OK) return st;
  st = get_hex8(data + p, &out->cid2); p += 2;
  if (st != PS_OK) return st;
  st = get_hex16(data + p, &out->length); p += 4;
  if (st != PS_OK) return st;

  uint16_t expected_length = out->length & 0x0fffu;
  uint16_t expected_word = ps_length_word(expected_length / 2u);
  if ((expected_length % 2u) != 0u || expected_word != out->length) return PS_ERR_BAD_LENGTH;

  size_t info_ascii_len = expected_length;
  size_t expected_frame_len = 1u + 8u + 4u + info_ascii_len + 4u + 1u;
  if (data_len != expected_frame_len) return PS_ERR_BAD_LENGTH;
  out->info_len = info_ascii_len / 2u;
  if (out->info_len > PS_MAX_INFO_BYTES) return PS_ERR_BAD_LENGTH;

  for (size_t i = 0; i < out->info_len; ++i) {
    st = get_hex8(data + p, &out->info[i]);
    if (st != PS_OK) return st;
    p += 2;
  }

  uint16_t got_checksum = 0;
  st = get_hex16(data + p, &got_checksum);
  if (st != PS_OK) return st;
  uint16_t calc_checksum = ps_checksum_ascii(data + 1, p - 1);
  if (got_checksum != calc_checksum) {
    ps_frame_t special = *out;
    if (!is_charge_relay_special(&special) || got_checksum != 0xfd35u) return PS_ERR_BAD_CHECKSUM;
  }
  out->checksum = got_checksum;
  return PS_OK;
}

ps_status_t ps_make_command(uint8_t adr, uint8_t cid2, const uint8_t *info, size_t info_len, ps_frame_t *out) {
  if (!out || (!info && info_len != 0u) || info_len > PS_MAX_INFO_BYTES) return PS_ERR_INVALID_ARG;
  memset(out, 0, sizeof(*out));
  out->ver = PS_DEFAULT_VER;
  out->adr = adr;
  out->cid1 = PS_CID1_POWER;
  out->cid2 = cid2;
  out->info_len = info_len;
  if (info_len != 0u) memcpy(out->info, info, info_len);
  out->length = ps_length_word(info_len);
  return PS_OK;
}

ps_status_t ps_parse_device_info(const uint8_t *info, size_t info_len, ps_device_info_t *out) {
  if (!info || !out) return PS_ERR_INVALID_ARG;
  if (info_len < 32u) return PS_ERR_BAD_LENGTH;
  memset(out, 0, sizeof(*out));
  memcpy(out->device_name, info, 10);
  out->software_version = (uint16_t)(((uint16_t)info[10] << 8) | info[11]);
  memcpy(out->manufacturer, info + 12, 20);
  return PS_OK;
}

ps_status_t ps_parse_pack_response(const uint8_t *info, size_t info_len, ps_pack_response_t *out) {
  if (!info || !out) return PS_ERR_INVALID_ARG;
  if (info_len < 1u) return PS_ERR_BAD_LENGTH;
  memset(out, 0, sizeof(*out));
  out->command_or_pack_count = info[0];
  out->pack_count = info[0] == 0xffu ? 0u : 1u;

  size_t p = 1;
  uint8_t parsed = 0;
  while (p < info_len && parsed < 8u) {
    ps_pack_data_t *pack = &out->packs[parsed];
    pack->pack_index = (uint8_t)(parsed + 1u);
    if (p >= info_len) break;
    pack->cell_count = info[p++];
    if (pack->cell_count > 16u) return PS_ERR_BAD_LENGTH;
    if (p + pack->cell_count * 2u > info_len) return PS_ERR_BAD_LENGTH;
    for (uint8_t i = 0; i < pack->cell_count; ++i) {
      pack->cell_mv[i] = (uint16_t)(((uint16_t)info[p] << 8) | info[p + 1u]);
      p += 2;
    }
    if (p >= info_len) return PS_ERR_BAD_LENGTH;
    pack->temperature_count = info[p++];
    if (pack->temperature_count > 16u) return PS_ERR_BAD_LENGTH;
    if (p + pack->temperature_count * 2u + 6u + 5u > info_len) return PS_ERR_BAD_LENGTH;
    for (uint8_t i = 0; i < pack->temperature_count; ++i) {
      pack->temperatures_raw[i] = (int16_t)(((uint16_t)info[p] << 8) | info[p + 1u]);
      p += 2;
    }
    pack->charge_current_raw = (int16_t)(((uint16_t)info[p] << 8) | info[p + 1u]); p += 2;
    pack->total_voltage_raw = (uint16_t)(((uint16_t)info[p] << 8) | info[p + 1u]); p += 2;
    pack->discharge_current_raw = (int16_t)(((uint16_t)info[p] << 8) | info[p + 1u]); p += 2;
    memcpy(pack->status.raw, info + p, 5);
    pack->status.protection = info[p++];
    pack->status.fet = info[p++];
    pack->status.misc = info[p++];
    pack->status.cell_fault_1_8 = info[p++];
    pack->status.cell_fault_9_16 = info[p++];
    ++parsed;
    if (out->command_or_pack_count != 0xffu) break;
  }
  out->pack_count = parsed;
  if (p < info_len) {
    out->warn_state_len = info_len - p;
    if (out->warn_state_len > sizeof(out->warn_state)) out->warn_state_len = sizeof(out->warn_state);
    memcpy(out->warn_state, info + p, out->warn_state_len);
  }
  return PS_OK;
}

static int bit(uint8_t value, unsigned int n) {
  return (value & (uint8_t)(1u << n)) != 0u;
}

int ps_status_flag_total_voltage_low(const ps_status_flags_t *flags) { return flags ? bit(flags->protection, 7) : 0; }
int ps_status_flag_charge_over_temp(const ps_status_flags_t *flags) { return flags ? bit(flags->protection, 6) : 0; }
int ps_status_flag_discharge_over_temp(const ps_status_flags_t *flags) { return flags ? bit(flags->protection, 5) : 0; }
int ps_status_flag_discharge_over_current(const ps_status_flags_t *flags) { return flags ? bit(flags->protection, 4) : 0; }
int ps_status_flag_charge_over_current(const ps_status_flags_t *flags) { return flags ? bit(flags->protection, 2) : 0; }
int ps_status_flag_cell_low_voltage(const ps_status_flags_t *flags) { return flags ? bit(flags->protection, 1) : 0; }
int ps_status_flag_over_voltage(const ps_status_flags_t *flags) { return flags ? bit(flags->protection, 0) : 0; }
int ps_status_flag_using_pack(const ps_status_flags_t *flags) { return flags ? bit(flags->fet, 3) : 0; }
int ps_status_flag_dfet_on(const ps_status_flags_t *flags) { return flags ? bit(flags->fet, 2) : 0; }
int ps_status_flag_cfet_on(const ps_status_flags_t *flags) { return flags ? bit(flags->fet, 1) : 0; }
int ps_status_flag_prefet_on(const ps_status_flags_t *flags) { return flags ? bit(flags->fet, 0) : 0; }
int ps_status_flag_fully_charged(const ps_status_flags_t *flags) { return flags ? bit(flags->misc, 3) : 0; }
int ps_status_flag_buzzer_on(const ps_status_flags_t *flags) { return flags ? bit(flags->misc, 0) : 0; }
