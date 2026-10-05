# BatteryOps Mason deployment

Qualified local deployment for one 16S 280 Ah Seplos Mason, SP18B V3 firmware
1.5, on the Waveshare ESP32-S3-RS485-CAN. This is not a general-purpose
firmware for unknown Seplos models or other batteries.

See the [complete fork change log](CHANGELOG.md) for all customisations relative
to the pinned upstream baseline, their test evidence and remaining boundaries.

## Connections and ownership

- RS485 is a passive receiver: GPIO18 RX, 19200 baud, GPIO21 driver-enable
  held LOW. The existing bo-Pi remains the sole poller. No BMS writes.
- CAN: GPIO15 TX / GPIO16 RX, 500 kbit/s, Victron profile.
- YamBMS is the sole GX native CAN battery, device instance 512.
- Disconnect the original Seplos-to-GX CAN path before admitting YamBMS.
  Never import both native receivers at once.
- HA uses encrypted ESPHome API port 6053 for monitoring. It is not the
  charging controller. Disable HA number, switch, select, button and update
  entities for this integration. That is an operator guard, not API access
  control: protect the encryption key and the network.
- Wi-Fi and API reboot timeouts are zero. Losing HA or Wi-Fi must not
  repeatedly reboot the battery controller.

## Current operating policy

Bulk ceiling 55.2 V; float 53.6 V; rebulk below 85% SOC or 52.8 V.
Charge output ceiling 35 A; discharge output ceiling 100 A. These are
maximum permissions, not commands to force that current. GX/DESS, VEConfigure
and the BMS may enforce lower allowances. The upstream merge margin and
temperature table also remain in effect.

Retained independent protections include stale/missing/invalid telemetry,
raw Seplos faults and FET permissions, unknown status bits, cell voltage
limits (charge blocked at 3.55 V maximum cell; discharge at 2.90 V minimum
cell), temperature stops and cold charging/discharging reductions. Explicit
operator stops persist across restarts. Startup requires fresh telemetry.

The installed narrow status profile accepts the qualified SP18B firmware-1.5
FET-status extension (0x40) and the manufacturer-classified normal balancing
status bit (byte16 0x20). It does not ignore arbitrary unknown bits.
Reported balancing flags are not a measurement of active-balancer current.

## Balancing status fix - 5 October 2026

The previous 4 October image intermittently blocked a second reserved status
bit: PIC coil0x1285, byte16 mask0x20. This is not the earlier FET extension.
Seplos's official DevBMSStudio V2.0.0.4 SP, published 17 March 2026, classifies
PIC row134 as `attribute=Normal`, not Warn/Protect, while leaving its physical
meaning labelled Reservation. Do not claim this measures active-balancer current.

The opt-in `v3_sp18b_fw15_balancing` profile accepts exactly this additional
non-fault status bit. Strict and previous FET-only profiles remain unchanged.
Raw bytes/reserved counts remain visible; the portal separately reports
`accepted_balancing_status`. All documented protections, FET permissions,
reported zero allowances, freshness, CRC, metadata, cell/temperature guards
and the other 17 reserved bits remain enforced.

Use `yambms-balancing-status-fix.yaml` to select the new profile while inheriting
the existing HA operating policy without changing charging settings. Offline
qualification: eight native ASan/UBSan suites, all 37 legacy operating scenarios,
all 37 scenarios with the new status asserted, and 16 compiled boundary checks.
Installed on 5 October 2026 using a verified grid-connected zero-energy hold.
Saved stops survived OTA; original GX/DESS settings and the 35/100 A profile
were restored. Twelve healthy GX/MultiPlus samples passed with no battery
alarms and zero CAN errors. The intermittent additional bit did not recur in
that soak: its acceptance is qualified by captured-message offline replay,
not yet by observing its recurrence after installation. This repair does not
claim to fix separate bridge/metadata/SQLite symptoms.

```bash
python3 bootstrap_packages.py
python3 tests/qualify_balancing_status.py
esphome compile yambms-balancing-status-fix.yaml
```

Native tests need g++, Python and PyYAML, but no device access or secrets.
They compare policy hashes against the tracked `qualification.json` baseline.
The optional `--compiled` mode also needs local ESPHome build output and
private release/rollback evidence from the deployment backup; it never uploads.

Official source: [Seplos software download](https://www.seplos.com/bms-3.0-battery-management-software-devbmsstudio-v2.0.0.4--sp-2026.03.17.html).
Package SHA256: `a40f325b7bdd06b5269642cf393782e3b75eed4ef5627121d3f2b1fe2f781e6c`.

## Reproduce offline

Use Python 3.12+, ESPHome 2026.9.1 and the pinned ESP-IDF 5.5.5 toolchain.
The source baseline is YamBMS commit
`e183d206bc3b041fdd39dc2ee331e6ef22f4b84d`; do not silently update packages.

```bash
python3 bootstrap_packages.py
python3 tests/run_yambms_logic.py --operating
esphome compile yambms-ha.yaml
```

Before compiling, supply private `secrets.yaml` using the example schema
and private `ota-password.yaml` containing a YAML scalar OTA password.
Generate an API key using `openssl rand -base64 32`. Wi-Fi station credentials
are retained in device NVS; provision a new device using its setup portal.
Do not publish NVS dumps, generated C++, firmware binaries or secret files.
The repository-root `secrets.yaml` is an inherited upstream template, not our
installation credentials; never put real credentials into that tracked file.
Use this deployment's ignored private secret files and review staged changes
with `gitleaks git --staged --redact=100 --no-banner` before publishing.
The YAML include chain retains historical qualification names, but the
operating build admits one real battery only, not a synthetic bench battery.

Tests execute actual extracted YamBMS lambdas with mocked ESPHome entities,
plus parser/adapter and persistence guards under ASan/UBSan: 37 scenarios.
They are not proof of physical CAN or MultiPlus behaviour.

## HA views

`HA_BATTERYOPS_DASHBOARD.json` is the exported native dashboard configuration:
Overview and Cells & Balancing, using the existing installation's entity IDs.
It includes 16 cell voltages, cell spread/history, temperature sensors,
reported balancing flags, freshness, protection reasons and received GX limits.
Adjust entity IDs for another installation. `make_ha_dashboard.py` regenerates
it from a local `HA_ENTITY_INVENTORY.json` containing verified HA entity IDs;
that live inventory is kept in the private deployment backup, not public Git.

HA recorder starts collecting these new ESPHome entities when the integration
is connected. Old data from other entities is not backfilled. Some inherited
device/entity names still say Shadow or Trial; the Firmware Role and signed-off
qualification scope identify the operating build. This is not a second BMS.

## Deployment and recovery

Live updates require a fresh GX settings snapshot and verified grid-connected
zero-energy hold: DESS off, system charge allowance zero, discharge power
zero, both directional permissions off, actual MultiPlus/shunt current near
zero, then master off. Keep all three stops persisted throughout OTA.
After reboot, verify fresh battery metadata, sole native receiver, fault-free
CAN and the unchanged operating profile before restoring the captured GX
settings. Do not infer recovery from a web page or firmware version alone.

Keep the previous qualified OTA image and private keys off-host. To roll back,
use the same verified hold, upload that image over authenticated OTA, check
the saved stops and fresh data, then restore the saved settings only after
native GX/MultiPlus limits are proven. A failed update must remain stopped
with mains available; never automatically enable control after an error.

Private NAS archives contain source, pinned packages, release/rollback images,
credentials and live settings snapshots. HA native backups contain its
integration registry, encryption key and dashboards. Neither belongs in
public Git. Restore the matching configuration and firmware, not just the
latest-looking file.

Documentation: [ESPHome API](https://esphome.io/components/api/) and
[HA ESPHome integration](https://www.home-assistant.io/integrations/esphome/).
