#include "power_serial/power_serial_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0) {
  fprintf(stderr, "usage: %s <serial-device> <command>\n", argv0);
  fprintf(stderr, "commands: version | pack-count | device-info | pack-data [all|1-8] | buzzer-on | buzzer-off\n");
}

int main(int argc, char **argv) {
  if (argc < 3) {
    usage(argv[0]);
    return 2;
  }

  ps_client_t client;
  ps_status_t st = ps_client_open(&client, argv[1], 9600, 1000, PS_DEFAULT_ADR);
  if (st != PS_OK) {
    fprintf(stderr, "open failed: %s\n", ps_status_string(st));
    return 1;
  }

  if (strcmp(argv[2], "version") == 0) {
    ps_frame_t response;
    st = ps_client_get_protocol_version(&client, &response);
    printf("version response info_len=%zu rtn=0x%02X\n", response.info_len, response.cid2);
  } else if (strcmp(argv[2], "pack-count") == 0) {
    uint8_t count = 0;
    st = ps_client_get_pack_count(&client, &count);
    if (st == PS_OK) printf("%u\n", count);
  } else if (strcmp(argv[2], "device-info") == 0) {
    ps_device_info_t info;
    st = ps_client_get_device_info(&client, &info);
    if (st == PS_OK) {
      printf("device=%s\nversion=%u\nmanufacturer=%s\n", info.device_name, info.software_version, info.manufacturer);
    }
  } else if (strcmp(argv[2], "pack-data") == 0) {
    uint8_t command = 0xff;
    if (argc >= 4 && strcmp(argv[3], "all") != 0) command = (uint8_t)strtoul(argv[3], NULL, 0);
    ps_pack_response_t data;
    st = ps_client_get_pack_data(&client, command, &data);
    if (st == PS_OK) {
      printf("packs=%u\n", data.pack_count);
      for (uint8_t i = 0; i < data.pack_count; ++i) {
        printf("pack=%u cells=%u temps=%u total_voltage_raw=%u\n",
               data.packs[i].pack_index,
               data.packs[i].cell_count,
               data.packs[i].temperature_count,
               data.packs[i].total_voltage_raw);
      }
    }
  } else if (strcmp(argv[2], "buzzer-on") == 0 || strcmp(argv[2], "buzzer-off") == 0) {
    ps_frame_t response;
    st = ps_client_set_buzzer(&client, strcmp(argv[2], "buzzer-on") == 0, &response);
    if (st == PS_OK) printf("rtn=0x%02X\n", response.cid2);
  } else {
    usage(argv[0]);
    ps_client_close(&client);
    return 2;
  }

  ps_client_close(&client);
  if (st != PS_OK) {
    fprintf(stderr, "command failed: %s\n", ps_status_string(st));
    return 1;
  }
  return 0;
}
