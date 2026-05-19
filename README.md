# supre_robot_power

C SDK and Python binding for the SmartPack power serial protocol described in `docs/power_serial.pdf`.

The protocol codec is implemented in C. The serial transport client uses
[`wjwwood/serial`](https://github.com/wjwwood/serial), fetched by CMake during configuration.

## Build with Docker

The build environment is an Ubuntu 22.04 arm64 image.

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm64
docker buildx build --platform linux/arm64 -t supre-robot-power-builder .
docker run --rm --platform linux/arm64 -v "$PWD:/workspace" -w /workspace supre-robot-power-builder \
  bash -lc 'cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure'
```

The `binfmt` command is only needed on non-arm64 hosts that have not already registered QEMU for arm64 containers.

The Python extension is built as `build/power_serial*.so`.

Run the detailed Python test script directly after building:

```bash
docker run --rm --platform linux/arm64 -v "$PWD:/workspace" -w /workspace supre-robot-power-builder \
  bash -lc 'PYTHONPATH=build python3 tests/test_power_serial_py.py'
```

For an optional live serial smoke test, pass a device into the container and set `POWER_SERIAL_TEST_PORT`:

```bash
docker run --rm --platform linux/arm64 --device=/dev/ttyUSB0 -v "$PWD:/workspace" -w /workspace \
  -e POWER_SERIAL_TEST_PORT=/dev/ttyUSB0 supre-robot-power-builder \
  bash -lc 'PYTHONPATH=build python3 tests/test_power_serial_py.py optional_live_serial'
```

## C API

Public headers are under `include/power_serial/`.

- `power_serial.h`: frame encode/decode, checksum, command helpers, device/pack parsers.
- `power_serial_client.h`: serial client using `wjwwood/serial`, configured as 9600 8N1 by default.

The protocol implementation uses ASCII HEX frames:

```text
SOI VER ADR CID1 CID2 LENGTH INFO CHKSUM EOI
```

`SOI` is `0x7E`, `EOI` is `0x0D`, and power commands use `CID1=0x46`.

## Python API

The pybind11 module name is `power_serial`.

```python
import power_serial

frame = power_serial.make_command(0x01, 0x4f)
raw = power_serial.encode_frame(frame)
decoded = power_serial.decode_frame(raw)

client = power_serial.Client("/dev/ttyUSB0", baudrate=9600, timeout_ms=1000)
count = client.get_pack_count()
data = client.get_pack_data(0xff)
```

## CLI Example

After building:

```bash
./build/power_serial_cli /dev/ttyUSB0 pack-count
./build/power_serial_cli /dev/ttyUSB0 device-info
./build/power_serial_cli /dev/ttyUSB0 pack-data all
```

## Notes

Some engineering units in the PDF are not explicit. The SDK therefore preserves raw integer values for pack measurements and exposes decoded status bits where the PDF defines them.
