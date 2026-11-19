# Original Xbox port plan

This document records the early WebTransport experiment. The shipped source in this fork uses an Xbox-hosted HTTPS controller. See the [playable NXDK target](../xbox/README.md#play-neon-roads-on-an-original-xbox) for current build and play instructions.

The Xbox port is a native NXDK application. It does not embed a browser and it
does not attempt to run the Three.js build. The portable simulation, road data,
and input protocol should move behind C/C++ interfaces; rendering, audio,
storage, networking, and platform input get Xbox implementations.

## First gate: direct phone control

The first hardware milestone is intentionally independent from the game:

```text
hosted HTTPS controller page -- WebTransport over the LAN --> original Xbox
```

The Xbox:

1. obtains an IPv4 address and current time (NTP, with RTC fallback);
2. generates or loads a short-lived P-256 certificate and private key;
3. starts a PicoQUIC HTTP/3 WebTransport endpoint;
4. displays a QR for the public controller page whose URL fragment contains
   the Xbox LAN address, certificate SHA-256 hash, and random pairing token;
5. accepts WebTransport datagrams authenticated by that token; and
6. visualizes steering and jump packets on the TV.

There is no PeerJS broker, relay, cloud signaling service, trusted local CA, or
Internet gameplay path. The phone and Xbox must be on the same LAN. URL
fragments are not sent to the web host, so the certificate hash and pairing
token stay on the phone.

### Why certificate-hash pinning changes the answer

Mobile Safari exposes motion/orientation sensors only in a secure context.
Ordinary HTTPS and WebSockets require Web PKI trust, which a private LAN Xbox
cannot obtain automatically. Shipping one public certificate and private key
inside every XBE would be insecure and must not be done.

WebTransport's `serverCertificateHashes` option is designed for ephemeral and
non-public hosts. The QR supplies the exact leaf-certificate hash, so the
browser can authenticate the Xbox without DNS or a certificate authority. The
certificate must be X.509v3, use P-256, be currently valid, and have a total
validity period of at most two weeks. A seven-day certificate generated on the
Xbox satisfies those constraints and can be rotated automatically.

This requires Safari/iOS 26.4 or newer (or another browser implementing
WebTransport certificate hashes). Unsupported phones remain unsupported for
tilt rather than silently routing controls through a relay. Normal Xbox
controllers continue to work.

## Port sequence after the phone-link proof

1. **Platform shell:** NXDK build, 640x480 presentation, controller input,
   logging, FATX save paths, and a 64 MiB memory budget.
2. **Simulation seam:** port deterministic ship physics, collision, road
   parsing, and replay code without rendering. Keep fixed-step behavior and
   cross-platform golden tests.
3. **Renderer:** follow the proven Neverball pattern: native C/C++ game code
   into an Xbox graphics backend (prefer nxdk-gles11 if the existing render
   abstraction maps cleanly; otherwise use pbkit directly). Do not attempt to
   port Three.js or a JavaScript runtime.
4. **Content pipeline:** convert road definitions and required procedural
   assets on the host at build time. Avoid runtime parsing and allocations
   where practical.
5. **Audio and saves:** replace Web Audio and localStorage with NXDK-native
   implementations.
6. **Integration:** feed the phone-link's six-byte input state into the same
   platform input mixer as the Duke/Controller S.
7. **Hardware qualification:** test on a stock 64 MiB console, not only xemu or
   a 128 MiB upgraded Xbox. Record exact XBE hashes and observed coverage.

The direct phone proof lives in [`xbox/`](../xbox/README.md).
