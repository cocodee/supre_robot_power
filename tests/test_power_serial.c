#include "power_serial/power_serial.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "assert failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

static int test_checksum_example(void) {
  const uint8_t body[] = "1203400456ABCEFE";
  ASSERT_EQ(ps_checksum_ascii(body, sizeof(body) - 1u), 0xfc71u);
  return 0;
}

static int test_encode_decode_no_info(void) {
  ps_frame_t frame;
  ASSERT_EQ(ps_make_command(0x01, 0x4f, NULL, 0, &frame), PS_OK);
  uint8_t encoded[PS_MAX_FRAME_BYTES];
  size_t encoded_len = 0;
  ASSERT_EQ(ps_encode_frame(&frame, encoded, sizeof(encoded), &encoded_len), PS_OK);
  ASSERT_TRUE(encoded_len > 0);
  ASSERT_EQ(encoded[0], PS_SOI);
  ASSERT_EQ(encoded[encoded_len - 1u], PS_EOI);

  ps_frame_t decoded;
  ASSERT_EQ(ps_decode_frame(encoded, encoded_len, &decoded), PS_OK);
  ASSERT_EQ(decoded.ver, 0x20);
  ASSERT_EQ(decoded.adr, 0x01);
  ASSERT_EQ(decoded.cid1, 0x46);
  ASSERT_EQ(decoded.cid2, 0x4f);
  ASSERT_EQ(decoded.info_len, 0u);
  return 0;
}

static int test_special_relay_examples(void) {
  uint8_t info_open[] = {0x0a};
  uint8_t info_close[] = {0x0b};
  uint8_t encoded[PS_MAX_FRAME_BYTES];
  size_t encoded_len = 0;
  ps_frame_t frame;

  ASSERT_EQ(ps_make_command(0x01, 0x99, info_open, sizeof(info_open), &frame), PS_OK);
  ASSERT_EQ(ps_encode_frame(&frame, encoded, sizeof(encoded), &encoded_len), PS_OK);
  const uint8_t expected_open[] = {
      0x7e, 0x32, 0x30, 0x30, 0x31, 0x34, 0x36, 0x39, 0x39, 0x45,
      0x30, 0x30, 0x32, 0x30, 0x41, 0x46, 0x44, 0x33, 0x35, 0x0d};
  ASSERT_EQ(encoded_len, sizeof(expected_open));
  ASSERT_TRUE(memcmp(encoded, expected_open, sizeof(expected_open)) == 0);

  ASSERT_EQ(ps_make_command(0x01, 0x99, info_close, sizeof(info_close), &frame), PS_OK);
  ASSERT_EQ(ps_encode_frame(&frame, encoded, sizeof(encoded), &encoded_len), PS_OK);
  const uint8_t expected_close[] = {
      0x7e, 0x32, 0x30, 0x30, 0x31, 0x34, 0x36, 0x39, 0x39, 0x45,
      0x30, 0x30, 0x32, 0x30, 0x42, 0x46, 0x44, 0x33, 0x35, 0x0d};
  ASSERT_EQ(encoded_len, sizeof(expected_close));
  ASSERT_TRUE(memcmp(encoded, expected_close, sizeof(expected_close)) == 0);
  return 0;
}

static int test_bad_checksum(void) {
  const uint8_t frame[] = "~2001464F0000FFFF\r";
  ps_frame_t decoded;
  ASSERT_EQ(ps_decode_frame(frame, sizeof(frame) - 1u, &decoded), PS_ERR_BAD_CHECKSUM);
  return 0;
}

static int test_device_info_parse(void) {
  uint8_t info[32] = {
      'S','m','a','r','t','P','a','c','k',' ',
      0x01, 0x02,
      'S','L','E','C',' ','P','o','w','e','r',' ',' ',' ',' ',' ',' ',' ',' '};
  ps_device_info_t parsed;
  ASSERT_EQ(ps_parse_device_info(info, sizeof(info), &parsed), PS_OK);
  ASSERT_EQ(parsed.software_version, 0x0102);
  ASSERT_TRUE(strncmp(parsed.device_name, "SmartPack ", 10) == 0);
  return 0;
}

static int test_pack_parse(void) {
  uint8_t info[] = {
      0x01,
      0x02, 0x0c, 0xe4, 0x0c, 0xe5,
      0x01, 0x00, 0xfa,
      0x00, 0x64, 0x18, 0x38, 0x00, 0x32,
      0x85, 0x0f, 0x09, 0x01, 0x02};
  ps_pack_response_t parsed;
  ASSERT_EQ(ps_parse_pack_response(info, sizeof(info), &parsed), PS_OK);
  ASSERT_EQ(parsed.pack_count, 1u);
  ASSERT_EQ(parsed.packs[0].cell_count, 2u);
  ASSERT_EQ(parsed.packs[0].cell_mv[0], 3300u);
  ASSERT_EQ(parsed.packs[0].cell_mv[1], 3301u);
  ASSERT_EQ(ps_status_flag_total_voltage_low(&parsed.packs[0].status), 1);
  ASSERT_EQ(ps_status_flag_charge_over_temp(&parsed.packs[0].status), 0);
  ASSERT_EQ(ps_status_flag_discharge_over_temp(&parsed.packs[0].status), 0);
  ASSERT_EQ(ps_status_flag_discharge_over_current(&parsed.packs[0].status), 0);
  ASSERT_EQ(ps_status_flag_charge_over_current(&parsed.packs[0].status), 1);
  ASSERT_EQ(ps_status_flag_over_voltage(&parsed.packs[0].status), 1);
  ASSERT_EQ(ps_status_flag_using_pack(&parsed.packs[0].status), 1);
  ASSERT_EQ(ps_status_flag_dfet_on(&parsed.packs[0].status), 1);
  ASSERT_EQ(ps_status_flag_cfet_on(&parsed.packs[0].status), 1);
  ASSERT_EQ(ps_status_flag_prefet_on(&parsed.packs[0].status), 1);
  ASSERT_EQ(ps_status_flag_fully_charged(&parsed.packs[0].status), 1);
  ASSERT_EQ(ps_status_flag_buzzer_on(&parsed.packs[0].status), 1);
  return 0;
}

int main(void) {
  int rc = 0;
  rc |= test_checksum_example();
  rc |= test_encode_decode_no_info();
  rc |= test_special_relay_examples();
  rc |= test_bad_checksum();
  rc |= test_device_info_parse();
  rc |= test_pack_parse();
  return rc;
}
