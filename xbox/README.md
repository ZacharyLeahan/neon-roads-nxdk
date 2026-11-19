# NXDK direct phone-link proof

## Play Neon Roads on an original Xbox

`https/` builds a separate NXDK title. The Xbox serves the entire controller
page over TCP 443, displays a pairing QR and runs the campaign locally.
This hardware test uses a certificate provisioned for one console's IPv4 address.
The WebTransport experiment below is a separate target.

1. With the mbed TLS install and QR generator described in DEPENDENCIES.md
   available and the root npm dependencies installed, use an activated NXDK shell and run `make -C xbox/https`.
2. From the repository root, run
   `python xbox/https/provision.py <xbox-ip> build-xbox/https-rig` with OpenSSL
   on PATH. Choose a new output directory if provisioning another identity.
   The script discards the issuing CA key and writes a 14-day server certificate.
3. Place `xbox/https/bin/default.xbe`, `xbox/https/controller.html`,
   `server.pem` and that console's `key.pem` together in the Xbox title folder.
   Keep private keys out of release packages. Relaunch from XBMC.
4. Transfer only `NeonRoads.cer` to the iPhone through a trusted channel. For
   trusted HTTPS, install the profile and enable its root under Settings >
   General > About > Certificate Trust Settings. Remove the test profile after
   testing. Provision again if the IP changes or the server certificate expires.
5. Scan the TV code with Safari. With the phone screen-up and its top toward the nose, tap **Enable motion**. The initial resting pose is neutral. Side lean reaches full steering at 10°. Lifting either end controls ship height continuously. The first 0.12° around level is neutral and 0.5° reaches full height. Hold that angle to stay up. Level the deck to descend and land on the road. Before each start, retry or next road, rest the board flat for two seconds. The game zeros pitch and roll and starts automatically. Use **Recenter** if the mount shifts.

To return to the dashboard, tap **Exit to dashboard** on the phone and confirm.
For hardware testing, POST to `/dashboard` over the same HTTPS connection with
the current QR fragment in the `X-Pair` header. The title acknowledges the request
with HTTP 202 before rebooting. Wait for XBMC FTP before launching again.

Jump fills the lower phone screen in portrait and the right side in landscape.
The Xbox mixes world-themed music, a speed-reactive engine, and race effects into
48 kHz stereo through SDL. Pairing is quiet until the phone connects.

Easy Cruise opens each session with a slower, wide road. The middle lanes are clear, and the outside lanes have optional practice gaps. After a large kick, the controller scales descent to the measured peak and accepts a nearby stable resting pitch on landing.

The native target exports all 30 campaign roads from the TypeScript game at build
time. Progression lasts for the current session. The hardware-tested path covers
launch, tilt steering, jump, falling, and retry on First Light.

## Earlier WebTransport experiment

This directory tracks the first NXDK feasibility gate: a hosted secure phone
page connecting directly to an original Xbox over WebTransport. Nothing relays
or brokers controller data.

## Intended connection

```text
1. Xbox gets DHCP + current time.
2. Xbox creates a short-lived P-256 certificate.
3. TV shows https://<public-site>/controller.html#<local connection bundle>.
4. The phone page requests tilt permission.
5. WebTransport connects to the Xbox LAN IP and pins the certificate hash.
6. Phone sends the existing six-byte input packet as QUIC datagrams.
```

The connection bundle contains a version, LAN IPv4/port, certificate SHA-256
hash, and 128-bit random pairing token. It is encoded in the URL fragment so
the public web host never receives it.

## Why this is the distribution path

WebTransport certificate hashes let a browser explicitly trust an ephemeral
self-signed certificate for a private host. Players do not install a CA, edit
DNS, reserve an address, run Tailscale, or configure a PC proxy. The Xbox can
generate and rotate its own identity.

The certificate must be P-256 and valid for no more than two weeks. The Xbox
therefore needs credible current time before generation. The bring-up target
will use unauthenticated NTP with the dashboard RTC as a fallback; certificate
hash pinning, not the clock source, authenticates the connection.

## Feasibility spike

PicoQUIC already supplies a C HTTP/3/WebTransport server and an mbed TLS crypto
backend. The next gate is not application design; it is proving that its core,
HTTP/3 layer, P-256/TLS 1.3 path, and UDP socket loop compile under NXDK and fit
comfortably inside the 64 MiB limit.

The proof is complete only after all of these pass:

- NXDK compile and link;
- xemu UDP handshake;
- Safari 26.4+ connection using `serverCertificateHashes`;
- physical Xbox QR scan, tilt, jump, reconnect, and stale-input timeout;
- measured XBE size, working-set peak, packet rate, RTT, and loss behavior; and
- a stock-64-MiB hardware run, not only the developer's 128-MiB console.

## Build the xEMU bring-up image

From a MinGW64 shell with `NXDK_DIR` pointing at an initialized NXDK checkout:

```sh
make -C xbox NXDK_DIR=/path/to/nxdk
```

The first image is intentionally a narrow runtime gate. It runs an mbedTLS
SHA-256 known-answer test, initializes NXDK networking, and displays the DHCP
address. It does not yet claim a QUIC or WebTransport handshake.
