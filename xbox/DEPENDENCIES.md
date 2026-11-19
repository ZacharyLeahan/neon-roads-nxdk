# Native dependency pins

- NXDK: local checkout observed at `14d5ee97e73347c973f1f57b68b79ec08c9e77f2`
- PicoQUIC feasibility baseline: commit
  `0cd55f2952a62ce7d40e5d5f46e1b64cfa52de6d`
- mbed TLS: tag `mbedtls-3.6.2`, resolved commit
  `107ea89daaefb9867ea9121002fbbdf926780e98` (PicoQUIC crypto backend)
- Jitterentropy Library (BSD license), commit `ae2dff91ebd1cb9d306611eb261f5b7a060fc410` for CPU timing entropy on Xbox.
- QR Code generator: Project Nayuki C implementation, commit `9a217d443937f60cc56c1458c39a211d7f2d3af5`

PicoQUIC and Project Nayuki's QR generator are MIT-licensed. mbed TLS is
Apache-2.0. Release packaging must carry their license notices.

## Build the HTTPS game dependencies

From the repository root in an activated NXDK shell, with Node.js, CMake, Git and Make on PATH:

```sh
npm ci
git clone --branch mbedtls-3.6.2 --recursive https://github.com/Mbed-TLS/mbedtls.git third_party/mbedtls
git clone https://github.com/smuellerDD/jitterentropy-library.git third_party/jitterentropy
git -C third_party/jitterentropy checkout ae2dff91ebd1cb9d306611eb261f5b7a060fc410
git clone https://github.com/nayuki/QR-Code-generator.git third_party/qr-code-generator
git -C third_party/qr-code-generator checkout 9a217d443937f60cc56c1458c39a211d7f2d3af5
cmake -S third_party/mbedtls -B build-xbox/mbedtls -G "Unix Makefiles" -C xbox/cmake/mbedtls-nxdk-cache.cmake -DCMAKE_TOOLCHAIN_FILE="$NXDK_DIR/share/toolchain-nxdk.cmake" -DCMAKE_INSTALL_PREFIX="$PWD/build-xbox/mbedtls-install" -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF -DGEN_FILES=OFF -DMBEDTLS_FATAL_WARNINGS=OFF
cmake --build build-xbox/mbedtls --target install
make -C xbox/https
```
