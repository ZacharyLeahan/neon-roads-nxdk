# Install Neon Roads on an original Xbox

1. Extract **neon-roads-nxdk-test.zip** on your computer. The game files are in the `NeonRoads` folder.
2. Find the Xbox's LAN IPv4 address in the dashboard network settings. On a computer with Python 3 and OpenSSL installed, open a terminal in the extracted folder containing `provision.py`. Replace `<xbox-ip>` with that address and run:

   ```sh
   python provision.py <xbox-ip> console-tls
   ```

   The script creates a new `console-tls` directory. If OpenSSL is outside your PATH, pass `--openssl` followed by the full path to the executable.
3. Copy `key.pem` and `server.pem` from `console-tls` into `NeonRoads` beside `default.xbe` and `controller.html`. Transfer those four files to the same game folder on your modded Xbox, for example `F:\Games\NeonRoads\`. Keep the private key with your installation.
4. For trusted HTTPS, transfer `NeonRoads.cer` from `console-tls` to the iPhone via a trusted channel. Install the profile and enable its root in **Settings → General → About → Certificate Trust Settings**. The server certificate is valid for 14 days. If it expires or the Xbox address changes, provision into a fresh directory, replace the Xbox's two PEM files, and update the iPhone's trusted profile.
5. Launch `default.xbe` from the dashboard. Put the iPhone on the same LAN and scan the TV code in Safari. Secure the phone screen-up with its top toward the nose, tap **Enable motion**, and rest the deck flat for two seconds. Easy Cruise starts automatically. Lean to steer, lift either end to raise the ship, and level the deck to land.
