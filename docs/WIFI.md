# Wi-Fi setup for ESP32 displays

The original nRF54 board has no Wi-Fi. See HARDWARE.md for the S3/C6 support and
qualification limits. The S3 path was physically checked on the originating
installation; a new owner must verify their own network and panel.

1. Build/flash the matching ESP32 Wi-Fi firmware by USB. Attach the S3 antenna.
2. In helper **2.7.0 or later**, choose Connection → Choose USB Display. Its
   header should identify the correct MCU. An empty/unknown controller cannot
   be identified reliably from USB VID/PID alone.
3. While connected through USB, choose **Set Up Wi-Fi via USB…**. Enter a 2.4 GHz
   WPA2/WPA3 Personal network and password in the native secure dialog. No
   example/home SSID is provided. Enterprise/captive-portal setup is not included.
4. Allow local-network access if macOS asks. After joining, the helper selects
   Wi-Fi. A power cycle reconnects using the display’s saved NVS settings.
5. Test a scene change, brightness (KE2 ON), clock animation and live feeds.
   It can now be powered by a USB charger; the helper must stay running on the
   same reachable LAN for the Mac-mediated Codex/printer/weather/Sun updates.

No router port forwarding, cloud relay, open access point or firmware OTA
service is added. USB remains the recovery/configuration path. To change the
network, select USB and repeat setup. Settings/profiles/scenes are preserved.

## Connection and privacy

Each display receives its own ECDSA certificate/private key and a random
256-bit controller token. The Mac pins the certificate, checks its unique local
hostname and authenticates inside TLS. It never falls back to plaintext.
The display advertises `_xiao-display._tcp` on port7443. mDNS follows DHCP;
a saved IP is only a hint and still requires successful certificate checks.

The password arrives in the setup worker on stdin, not process arguments. It
is not saved on the Mac or sent in logs. The display stores credentials and its
key in ordinary NVS, which is not encrypted-flash protection against physical
extraction. Local private/wifi records contain pairing tokens/pins and must not
be shared. Rebuilding the helper can prompt macOS for permissions again.

## Reliability and validation

The setup sender drains the previous reply and paces USB writes in32-byte
chunks at15ms intervals. This was physically verified after reproducing CDC
RX-ring overflow with one long packet. “Display did not confirm Wi-Fi setup”
is a transport acknowledgement failure, not proof of an incorrect password.
A sanitized local wifi-setup-status.json records the final outcome.

TCP_NODELAY and complete-line TLS replies reduce packet delays. Wi-Fi image
updates use240 complete, CRC-checked rows, with control/health opportunities
after each row; USB/Bluetooth retain1920 small patches. A complete S3 Wi-Fi
integration verified all ten scenes, Weather12/24 switching, all57600 Sun
pixels and identical full-frame CRC after scene changes. No real user readings
or configured credentials are distributed here. The template remains unpaired.

Run `python tests/check_wifi.py` for the localhost TLS tests (no internet, printer,
account or hardware access). Loopback networking must be allowed. Offline tests
also cover identification, guarded setup parsing, finite CDC buffers, fragmented
replies and rejection of untrusted certificates/incorrect controller tokens.
