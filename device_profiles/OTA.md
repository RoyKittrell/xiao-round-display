# Wi-Fi firmware updates (S3)

Both Desktop and Gym use the provisioned certificate-pinned TLS pairing for
updates on port7444. Ordinary controls continue on7443. No plaintext ArduinoOTA
or unauthenticated web upload endpoint is enabled.

A board without this receiver needs one USB update. With Wi-Fi provisioned,
Firmware → Update Firmware over Wi-Fi installs the currently bundled build for
that board's existing profile. Rebuild the modules and reinstall the helper to
bundle new source changes. Profile changes remain explicit USB operations.

The client checks its saved pairing belongs to the selected USB identity. The
receiver reports its full MAC, profile, current ELF hash, partition-table hash and
inactive-slot capacity. The updater validates the application chip header,
profile marker, ELF hash, package hashes and partition layout before upload.
A per-device maintenance lease pauses only that device's worker during transfer.

Only the inactive OTA application partition is erased/written. A SHA256 of the
entire received file, expected profile marker, SDK image validation and application
ELF identity must pass before boot selection changes. Truncated/corrupt uploads
retain the existing boot selection. Successful uploads reboot; Gym flushes its
pedal log immediately beforehand. The helper confirms the new running ELF hash,
then resumes that device's worker. NVS and the filesystem are preserved.

The default Arduino bootloader does not provide automatic application-crash
rollback. Keep USB available for recovery if a newly compiled application boots
incorrectly. Transport integrity does not establish that arbitrary new source is
correct. S3 is currently qualified here; C6 OTA and Nordic OTA are not enabled.

USB updates after an OTA boot target the positively reported running slot rather
than assuming app0. Existing partition layouts are never changed automatically.

Offline coverage: real firmware receiver compiled with ASan/UBSan, real SHA256,
fragmented input/output, wrong profile/marker/layout, hash mismatch, partial/write
failure, invalid image and activation ordering; host transfer, identity refusal,
commit/readback and maintenance cleanup; real loopback TLS pinning/authentication.
Physical update results are recorded separately after hardware validation.

## Validation limits

An originating S3 Gym installation completed real certificate-pinned Wi-Fi
updates, changed OTA slots, and confirmed the expected running ELF hash. The
Desktop receiver boots and passes offline checks, but an actual Desktop Wi-Fi
firmware transfer remains unverified because that installation did not join its
network during the OTA check. USB power was present during the Gym transfers;
battery-only transfer is not separately qualified.

The public source is rebuilt and tested separately from that configured
installation. No saved pairing, network credential or recovery dump is included.
Recovery copies can contain user data; keep them local. A failed USB recovery
read aborts before writing rather than proceeding without a backup.
