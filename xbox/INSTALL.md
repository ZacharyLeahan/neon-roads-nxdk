# Install Neon Roads on an original Xbox

1. Extract the game ZIP. Copy the entire `NeonRoads` folder to a writable location on your modded Xbox hard drive, such as `F:\Games\NeonRoads\`. It contains `default.xbe` and `controller.html`.
2. Launch `default.xbe` from the dashboard. The Xbox creates its own HTTPS key and certificate, saves them beside the game, and displays a QR code on the TV.
3. Connect your iPhone to the same LAN and scan the code. Safari may ask you to continue because the Xbox signed its own certificate. Check that the address matches the Xbox before continuing, then tap **Enable motion**.
4. Mount the phone screen-up with its top toward the nose. Rest the deck flat for two seconds to start Easy Cruise. Lean to steer, lift either end to raise the ship, and level the deck to land.

Keep `auto-key.pem` on your console. Leave it and `auto-cert.der` in place when updating the game. On launch the game refreshes the certificate if its LAN address has changed or the certificate has expired. When distributing the game, copy only the original two game files.

**Upgrading from alpha 1.** The game continues to use an existing `key.pem` and `server.pem` pair. Install the new build in a fresh folder to switch to automatic setup.
