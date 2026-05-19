# Power Serial SDK Implementation Result

Date: 2026-05-19

## Implemented

- Added a C SDK for the serial ASCII protocol described in `docs/power_serial.pdf`.
- Added frame encode/decode support, including `LENGTH` and `CHKSUM` validation.
- Added serial client support using `wjwwood/serial`.
- Added high-level APIs for protocol version, pack count, device info, baudrate setting, pack data, control commands, and buzzer control.
- Added pybind11 Python binding module named `power_serial`.
- Added a CMake/Ninja build system.
- Added a CLI example at `examples/power_serial_cli.c`.
- Added unit tests at `tests/test_power_serial.c`.
- Added Docker-based build environment using Ubuntu 22.04 arm64.

## Important Files

- `CMakeLists.txt`
- `Dockerfile`
- `include/power_serial/power_serial.h`
- `include/power_serial/power_serial_client.h`
- `src/power_serial.c`
- `src/power_serial_client.cpp`
- `bindings/power_serial_py.cpp`
- `tests/test_power_serial.c`
- `examples/power_serial_cli.c`
- `README.md`

## Docker Build Environment

The build image is:

```bash
supre-robot-power-builder
```

It is built with:

```bash
docker buildx build --platform linux/arm64 -t supre-robot-power-builder .
```

On non-arm64 hosts, register arm64 binfmt first if needed:

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

## Verification

The project was built and tested inside the arm64 Docker image, not with local host build tools.

Command used:

```bash
docker run --rm --platform linux/arm64 -v "$PWD:/workspace" -w /workspace supre-robot-power-builder \
  bash -lc 'rm -rf build && cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure'
```

Result:

```text
100% tests passed, 0 tests failed out of 1
```

Python binding smoke test was also run inside the same arm64 container:

```bash
python3 - <<'PY'
import sys
sys.path.insert(0, "build")
import power_serial
f = power_serial.make_command(0x01, 0x4f)
raw = power_serial.encode_frame(f)
assert power_serial.decode_frame(raw).cid2 == 0x4f
print(raw)
PY
```

Output:

```text
b'~2001464F0000FD99\r'
```

Docker image architecture was confirmed as:

```text
arm64 linux
```

## Serial Library Update

The first implementation used direct POSIX serial code. It was later changed to use:

```text
https://github.com/wjwwood/serial
```

Implementation details:

- CMake fetches the upstream source archive during configuration.
- The upstream catkin CMake project is not used directly.
- The project builds a local `wjwwood_serial` static library from upstream source files.
- `src/power_serial_client.cpp` wraps `serial::Serial` behind the existing C ABI in `power_serial_client.h`.
- The public `ps_client_t` now stores an opaque `void *impl` pointer instead of a raw file descriptor.
- The C protocol codec remains plain C and unchanged in purpose.

## Protocol Note

The charge relay OPEN/CLOSE frames on page 17 of `docs/power_serial.pdf` use checksum `FD35`, which does not match the general checksum formula described earlier in the document.

Implemented behavior:

- `CID2=0x99`, `INFO=0x0A`: encode/decode as the documented charge relay OPEN special frame.
- `CID2=0x99`, `INFO=0x0B`: encode/decode as the documented charge relay CLOSE special frame.
- All other frames use the standard checksum algorithm from the protocol.

## Remaining Notes

Some engineering units in the PDF are not explicit. For those fields, the SDK preserves raw integer values and only exposes decoded status bits where the PDF defines the bit meaning.
