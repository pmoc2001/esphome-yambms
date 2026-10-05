# BatteryOps Fork Change Log

This records all BatteryOps YamBMS customisations captured in this fork through
5 October 2026. It does not describe unrelated BatteryOps host, charger, wiring
or OpenClaw changes as YamBMS source changes.

## Baseline And Ownership

- Branch: `batteryops/mason-waveshare-ha`; upstream-derived `main` remains separate.
- Pinned upstream: `e183d206bc3b041fdd39dc2ee331e6ef22f4b84d`.
- Initial deployment capture: `575f9395cbef85b4ba61b0f83642591e3a1aaa5f`.
- One Seplos Mason 16S 280 Ah, SP18B V3 firmware 1.5, on Waveshare ESP32-S3-RS485-CAN.
- Our package changes live under `package-overrides/`; upstream packages are
  materialized by `bootstrap_packages.py`, not edited in place in public Git.
- Installed entry point: `yambms-balancing-status-fix.yaml`. Earlier wrappers
  are retained for history/testing, not all simultaneously installed.

Compare every committed fork change with our pinned baseline:

```bash
git diff --stat e183d206bc3b041fdd39dc2ee331e6ef22f4b84d batteryops/mason-waveshare-ha
git diff e183d206bc3b041fdd39dc2ee331e6ef22f4b84d batteryops/mason-waveshare-ha -- README.md deployments/batteryops-mason
git log --oneline e183d206bc3b041fdd39dc2ee331e6ef22f4b84d..batteryops/mason-waveshare-ha
```

## 2-3 October: Reader And Isolated Qualification

- Added `wifi-test.yaml` and the Waveshare board configuration, web diagnostics
  and Wi-Fi provisioning. API/Wi-Fi loss must not repeatedly reboot the controller.
- Added custom `shadow-components/seplos_parser`: receive-only RS485 on GPIO18,
  19200 baud, driver enable GPIO21 held LOW. bo-Pi generates the traffic being
  observed; this is not standalone polling and does not write BMS parameters.
- Decode real pack voltage/current/SOC, reported allowances, capacity metadata,
  all 16 cells, six temperatures and raw PIC status. Validate CRC, addressing,
  frame shape and independent freshness of pack/cell/status channels.
- Publish raw status, warning/protection counts, FET permissions, balancing
  flags, freshness, adapter decisions and alarm masks to the diagnostics portal.
  A reported balancing flag is not a measured active-balancer current.
- Add the opt-in `v3_sp18b_fw15` profile for byte15 FET-status extension `0x40`.
  Preserve the strict default, raw bytes and every other unknown-bit stop.
- Add `yambms-shadow.yaml`, status-compatible, live-data bench and handover
  wrappers separating observation, isolated CAN testing and live admission.
- Add local adapter/core/merge overrides for validated real Seplos data,
  unsmoothed permissions/protections, missing-data stops and conservative wire caps.
- Preserve metadata, per-cell and temperature checks before CAN admission.
  Independent guards stop charge at highest cell >=3.55 V and discharge at
  lowest cell <=2.90 V. Cell-temperature charge stop <=5 or >=45 C, discharge
  stop <=0 or >=45 C; cold-current reduction remains in addition to BMS limits.
- Add source-extracted YamBMS lambda tests, parser/adapter C++ scenarios and
  mock ESPHome entities. Isolated/native tests are not physical MultiPlus proof.

## 4 October: GX Migration And Physical Tests

- Admit YamBMS as the sole GX native CAN battery, instance512, via the CANable
  path at500 kbit/s. The original Seplos CAN receiver/path remains disconnected
  while Yam operates; do not import two native instance512 batteries together.
- SmartShunt remains wired to GX. No SmartShunt, Seplos or VEConfigure settings
  were written by the migration helpers.
- Supervised migration used captured GX settings, mains availability and
  bounded temporary allowances; failed qualification entered a zero-energy hold.
- Physical charge stop/recovery passed using a small grid-assisted charge.
- Physical discharge stop/recovery passed using a bounded household-load test.
- Full CAN-loss test demonstrated actual MultiPlus charge cessation and recovery;
  the earlier short-loss attempt was inconclusive and is not counted as proof.
  CAN-loss inverter shutdown under battery-only operation was not established.
- Expected bridge-offline/low-battery notifications during intentional stops
  were retained, not suppressed. Failed/interrupted private reports remain
  distinct from subsequent passing reports.

## 4 October: Persisted Stops And Normal Operation

- Add `yambms-restart-safe.yaml`: master and both directional permissions retain
  explicit intent through native ESPHome preferences. Immediately sync changes;
  failed intent persistence blocks admission. No stale-data auto-arm.
- Enabled and stopped soft reboots were tested, including fresh data and native
  GX recovery. A full power-cycle/GX cold-boot test remains outstanding.
- Add `yambms-operating.yaml` and `operating_limits()` to replace commissioning
  current constraints while retaining metadata/fault/cell/temperature protections.
- Normal wire/operator maxima:35 A charge and100 A discharge. These are allowed
  ceilings, not commands forcing that current; GX/DESS/BMS can request less.
- Remove the inherited trial discharge filter; update the operating discharge
  temperature table to allow warm100 A operation while retaining cool derating.
  The charging temperature table and independent temperature stops are unchanged.
- Retain bulk55.2 V, float53.6 V, rebulk below85% SOC or52.8 V and rebulk-days0.
  Retain upstream end-of-charge/float logic; no forced ongoing top-charge mode.
- Restore captured DESS/GX settings after proof. A CAN transient during earlier
  commissioning remains an observation, not a conclusively explained fault.
-37 operating/source/parser/persistence/temperature scenarios passed under
  ASan/UBSan; native GX and MultiPlus35/100 A allowances were observed live.

## 4 October: Home Assistant And Recovery Capture

- Add `yambms-ha.yaml` with encrypted native ESPHome API and cell diagnostics.
  HA/Wi-Fi reboot timeouts remain zero. HA is a monitor, not charging authority.
- Export the native HA dashboard and its generator: Overview and Cells &
  Balancing, including16 cell voltages, spread/history, temperatures, balancing
  flags, freshness, limits and protection reasons.
- Locally disable44 HA control entities; dashboard uses54 unique readings,
  including existing GX readings. No HA charge automations were introduced.
  Disabled entities are an operator guard, not API authorization.
- Preserve inherited Shadow/Trial entity IDs to avoid breaking monitoring.
  Firmware Role identifies the operating build. New history is not backfilled.
- Capture pinned bootstrap, package overrides, fake-entity tests, sanitized
  `qualification.json`, example secrets and public recovery instructions.
- Keep matching release/rollback images, credentials, live settings snapshots,
  Git bundle and native HA backups in private off-host recovery archives.
  Backup hashes/contents were verified; no full live restore rehearsal claimed.

## 5 October: Second Normal-Status Bit Fix

- Diagnose repeated false BMS-internal-failure alarms: PIC byte16 mask`0x20`,
  coil`0x1285`, appears alongside balancing-module/cell flags. The existing
  accepted byte15`0x40` handling was intact. This is a different status bit.
- Manufacturer DevBMSStudio V2.0.0.4 SP PIC row134 classifies this reserved
  status as `Normal`, not `Warn`/`Protect`. Its physical trigger/meaning is not
  established; do not claim the bit itself proves active balancing current.
- Add explicit opt-in `v3_sp18b_fw15_balancing` and the installed wrapper.
  Accept only this additional bit; retain strict/FET-only profiles and raw data.
  Diagnostics separately report `accepted_balancing_status`.
- Keep all30 documented protections,17 other reserved-bit stops, FET permissions,
  reported zero allowances, CRC, freshness, metadata and cell/temperature guards.
  No charging-policy changes, BMS parameter writes or alarm-rule suppression.
- Add captured-message regression coverage, all four FET combinations,
  zero/invalid limits, startup/stale data, warnings, protections and other
  unknown bits. Run all37 operating scenarios with and without the new status.
- Eight native ASan/UBSan suites,37 legacy +37 new-profile scenarios and16
  compiled checks passed. Install once over authenticated OTA under a verified
  grid-connected zero-energy hold; saved stops survive reboot.
- Restore DESS/GX settings;12 healthy GX/MultiPlus samples pass, alarms0,
  CAN errors0. HA and BatteryOps independently receive healthy data.
- Exact intermittent-bit recurrence was not seen in the installation soak:
  handling is captured-message replay-qualified, not yet live recurrence-proven.
  The live symptom check must not intentionally force high cells or defeat guards.
- Fix the test wrapper's excessive live-recurrence requirement separately from
  firmware behavior; restore safely without a second OTA. Preserve earlier
  interrupted/failed records rather than relabeling them as passing.
- Checksum-verify a matching private NAS backup of firmware/source/keys,
  release/rollback evidence and fresh GX settings. No full restore test claimed.
- Include all eight native fixtures in the public fork and use tracked baseline
  hashes so native qualification works without private historical reports.
- Document secret-file exclusions and staged secret scanning. The inherited
  root secret template must never receive real installation credentials.

## Planned Or Not Yet Proven

- T-Connect direct polling: investigate/test when hardware arrives; current
  Waveshare still depends on bo-Pi polling. Moving the listener alone does not
  remove that dependency. Keep one active poller per Seplos bus.
- Multiple batteries/multi-node operation: not deployed or qualified yet.
- Optional Yam SmartShunt data integration: not configured; wired GX connection
  retained. Any future SOC/current-source change needs separate qualification.
- Full100 A discharge stress, full-system cold boot and prolonged end-of-charge/
  float transitions remain outside the completed qualification scope.
- Separate bridge-offline, metadata coherence and BatteryOps SQLite-locking
  observations are not all proven to share the repaired status-bit cause.

## Keeping This Record Current

Add a dated entry for each future change with files/policy affected, reason,
offline tests, actual live checks, installed/prepared status and recovery path.
Do not publish private keys, NVS, binaries or raw private host snapshots.
Source commits do not prove deployment: update the installation/recovery notes
only after the running controller and native GX/MultiPlus behavior are checked.
