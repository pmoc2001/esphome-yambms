# BatteryOps Fork Change Log

This records all BatteryOps YamBMS customisations captured in this fork through
20 October 2026, with separately prepared work identified explicitly. It does not describe unrelated BatteryOps host, charger, wiring
or OpenClaw changes as YamBMS source changes.

## 20 October: Reboot And Timer Correction Installed, Recovery Verified

- Added `yambms-reboot-timer.yaml`. AutoCCL restores saved ON/OFF intent,
  defaults ON and requests immediate native preference sync. Existing AutoCVL
  persistence is unchanged. EOC intentionally boots ON; maintenance OFF does
  not survive a restart.
- Fixed unsigned subtraction in the qualified elapsed-time diagnostic. A
  timer start just newer than the last checked timestamp displays zero instead
  of wrapping to the 600-second cap. Control timers and completion rules do
  not use this diagnostic and are unchanged.
- Added resolved-config and actual ESPHome restoration-method tests for saved
  ON, saved OFF and absent preferences, plus compiled-code checks. Simulated
  preferences do not prove physical flash persistence or GX recovery.
- Published-source candidate retains the installed single-node transaction
  reader byte-for-byte; unfinished T-Connect transport is not included.
  Imported source history and tests for the previously private operating fixes.
- No OTA, live controller restart, BMS/shunt write, forced SOC reset or charge
  target change is implied by source publication. See `RELEASE_20261020.md`.
- Replaced two inherited example SmartShunt BLE key literals with private
  `!secret` references. Those examples are outside the live Waveshare include
  chain. Public history was not rewritten; private installation keys, binaries
  and generated files are excluded from the published source.
- Subsequent authorized installation used one OTA under a verified zero-current
  hold. First boot restored an older AutoCCL OFF preference, so admission stayed
  stopped. Saved intended ON with the new firmware, then one controlled soft
  restart proved ON persistence without a post-boot command. Saved master and
  directional stops survived both restarts.
- Native admission and 12 consecutive recovery samples passed; exact captured
  ESS/DESS settings restored. Separate cloud-planning and active GX schedule
  acceptance passed. No BMS/shunt settings or charge thresholds changed.
- Qualified elapsed display was zero under the stopped hold. A future active
  finishing window and unattended mains-loss/cold-boot recovery are not proven.
  The new informational Restart Policy text label remains blank; actual switch
  states are readable. This cosmetic issue is recorded, not silently treated
  as verified or used to justify another immediate OTA.

## 19 October: Bounded Finishing Pause V2 Installed

- Added bounded pauses to an existing finishing attempt for reversals no larger
  than 3 A, no longer than 60 seconds and no more than 0.05 Ah cumulative.
  Paused time cannot count towards completion; positive current does not refund
  the discharge budget. Unsafe/stale data and operator stops still cancel.
- Normal 600-second completion still requires reported balancing OFF. EOC is
  an optional backstop, not proof of balanced cells. Added reason, elapsed,
  pause and budget diagnostics with comments and regressions.
- One authorized OTA and corrected native GX recovery passed. Recovery kept
  discharge permission OFF until effective GX discharge was proven zero;
  inverter-power configuration alone was insufficient in Keep Batteries Charged.
- Recorded short live pauses and resumption without false completion. The
  8,606-snapshot replay also did not finish: legitimate balancing interruptions
  remained. This is not a claim that every cell was balanced.
- Historical installed v2 still boots AutoCCL OFF and EOC ON. The 20 October
  candidate addresses AutoCCL persistence separately.

## 17 October: Separate Two-Board Candidate, Not Included Here

The following work remains in the original development working tree. It is
not part of this single-node operating release and is not installed.

- Added a direct read-only SEPLOS poller for T-Connect and a dedicated
  RS485 link to Waveshare. Native address 0 remains separate from node
  address 1. Polling defaults OFF on first installation.
- Added an opt-in transport entry to the existing qualified parser, retaining
  raw native limits, PIC status, cell/temperature guards and independent
  freshness. Boot identities, sequence counters and source ages prevent
  cached node replies from masquerading as new battery measurements.
- Added the standard summary register layout plus an installation-specific
  native-block extension; Waveshare requires the extension for control.
  This is custom BatteryOps code, not an unmodified upstream multi-node reader.
- Added native regressions, actual ESPHome host Modbus endpoint tests and
  a read-only API-to-MQTT observer candidate to preserve monitoring without
  putting bo-Pi back in the control path. No software was deployed to either Pi.
- Retained the recorded 50/100 A release, voltage targets, finishing repair,
  saved controls and CAN identity. No BMS, GX, DESS or shunt settings changed.
- T-Connect is not yet in its final location. Physical ports, wiring,
  unattended power recovery, observer deployment and live GX acceptance
  remain separate mandatory gates. Only the existing one battery is configured.

## 16 October: Charge Ceiling 50 A Release Recorded

- Added the previously private `yambms-charge-ceiling-50a.yaml` source to this
  deployment. It matches the 16 October build input and retains the installed
  finishing-session and transaction-reader repairs. Discharge remains 100 A.
- Source of installation status: private `yambms-charge-rate-20261016/evidence/`
  release manifest and installation record. Historical 35 A trial values below
  are not the latest release setting; no new live charge verification is claimed.

## 15 October: Finishing Session Repair Installed, Recovery Verified

- Reproduced completion after permission loss, stale data and operator stop
  in the installed core. Added attempt identities, cancellation of both
  timers, expiry-time validation and preserved historical completion time.
- Added a deployment-only direct parser/pre-filter guard so current stops,
  discharge and balancing changes are checked before a delayed merge update.
  Normal short completion, the enabled balancing backstop and Float remain.
- Retained 600-second/30-minute timers and existing voltage/current/safety
  policy. Added finishing-outcome diagnostics and focused native regressions,
  plus compiled-build verification against the installed reader release.
- Subsequent authorized OTA passed under the established verified hold;
  saved stops and clean initial CAN state survived. Native GX limits passed
  under hold and captured settings were restored. GX then became unreachable
  during recovery checks; Seplos readings later became stale. Maintenance
  had been announced, but its start and the outage cause remain unconfirmed.
- All three Yam controls were saved OFF during that interruption; the GX hold
  could not then be reapplied. The failed initial acceptance record is retained.
- After operator-confirmed maintenance and approval for normal restoration,
  established a new GX hold, restored Yam communication and permissions under
  hold, then restored original GX settings at 11:15:38 UTC. All 12 native
  recovery samples and separate DESS cloud/delivered-schedule checks passed.
  MPPT 1 connection-loss error cleared and managed charging resumed. No
  active native alarms or new CAN errors; operating policy unchanged.
- Recorded separate cold-start fallback issue: with Yam master OFF and GX
  restarted, MultiPlus charged using local settings without a managed battery.
  Yam OFF is not a guaranteed charger stop. No fallback settings changed.
- No BMS/shunt changes, forced full charge or deliberate heavy-load test.
  Cell 4 was within a 16 mV spread during ordinary ~22 A charging; hardware
  repair and finishing-charge acceptance still need their specific tests.

## 9 October: Transaction Reader V3 Installed, Bounded Live Trial Passed

- Authorized OTA retained saved stops and restored exact GX/DESS settings.
  Temporary hold-induced warning cleared. The 20-minute trial passed four
  settings cycles, 2,304 native GX samples, 793 CAN limit frames and 794 clear
  CAN alarm frames. No false admission drop or unexplained zero allowance;
  natural current ranged +18.1 to -69.7 A. No charging/protection policy change.
  DESS planning and applied schedule verified. 24-hour recurrence review and
  exact earlier 09:55 warning attribution remain open.

- Follow-up pre-upload review withdrew transaction-reader v2. Corrected
  damaged-frame payload scanning and request-shaped PIC response handling in
  v3. Added 286 corruption/drop cases and a raw-chunk replay entry point.
  Subsequently captured six minutes of actual poller serial traffic, including
  settings. Replay reproduces a 10.637-second block in the rejected reader;
  v3 stays admitted throughout. Pre-upload checks passed before the later
  supervised installation above. Poller unchanged and tracing detached.

- Reconstructed the settings/cells response-length collision using recorded
  0x1350 settings words from the rejected trial. Reproduced same-poll numerical
  disagreement using the 9 October 04:45 UTC summary/cell values.
- Added opt-in request/response association, full unrelated-frame consumption,
  independent telemetry validation and diagnostic counters. No BMS writes.
- Retained real protection thresholds, stale-data stops, saved controls and
  charging policy. Fixed candidate extrema handling so no individual extreme
  is lost when considering summary extrema.
- Added transaction tests, sensor-filter simulation, offline qualification and
  an explicit supervised acceptance procedure. No OTA or live setting changes.
- Exact attribution of the separate 09:55 UK MultiPlus alarm remains limited
  by absent event-resolution CAN/native-limit history; do not claim full closure.

## 6 October: Measurement-Pair Repair Prepared, Not Installed

- Reproduced a load-step metadata failure in the original reader. Recorded
  live metadata blips were 256/148 ms; the exact incident inputs were not
  retained, so the native reproduction models that observed timing pattern.
- Decode complete pack/cell packets internally before sensor callbacks.
  Retain existing numerical agreement tolerances; require a sub-second
  reception-time pair and allow at most 1000 ms for cross-packet assembly
  only after a coherent baseline. No startup grace or sliding deadline.
- Invalid fields, stale channels, new BMS current/FET limits, documented
  protections and other unknown bits remain blocking. Conservative extrema
  from either packet retain immediate cell-voltage and temperature stops.
- Added measurement_pair_test.cpp, qualify_measurement_pair.py and the
  diagnostic-only yambms-measurement-pair-fix.yaml wrapper. Charging policy,
  saved stop intent, Auto CVL persistence, 600-second cut-off and 35/100 A
  wire ceilings are unchanged.
- Added read-only current-device checks and local replay of decoded web
  samples; these are not candidate physical-control proof. OTA and native
  GX/MultiPlus verification remain pending. Installed entry point below
  remains yambms-auto-cvl-persistent.yaml until that separate rollout.

## Baseline And Ownership

- Branch: `batteryops/mason-reboot-timer-20261020`; upstream-derived `main` remains separate.
- Pinned upstream: `e183d206bc3b041fdd39dc2ee331e6ef22f4b84d`.
- Initial deployment capture: `575f9395cbef85b4ba61b0f83642591e3a1aaa5f`.
- One Seplos Mason 16S 280 Ah, SP18B V3 firmware 1.5, on Waveshare ESP32-S3-RS485-CAN.
- Our package changes live under `package-overrides/`; upstream packages are
  materialized by `bootstrap_packages.py`, not edited in place in public Git.
- Latest recorded installed release: reboot/timer v4, entry point
  `yambms-reboot-timer.yaml`. Earlier wrappers are
  retained for history/testing, not all simultaneously installed.

Compare every committed fork change with our pinned baseline:

```bash
git diff --stat e183d206bc3b041fdd39dc2ee331e6ef22f4b84d batteryops/mason-reboot-timer-20261020
git diff e183d206bc3b041fdd39dc2ee331e6ef22f4b84d batteryops/mason-reboot-timer-20261020 -- README.md deployments/batteryops-mason
git log --oneline e183d206bc3b041fdd39dc2ee331e6ef22f4b84d..batteryops/mason-reboot-timer-20261020
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

## 5 October: Auto CVL Persistence

- Operator enabled Auto CVL after the supervised charge review. The inherited
  commissioning `ALWAYS_OFF` override would lose that choice on restart.
- Add `yambms-auto-cvl-persistent.yaml` over the existing top-charge trial;
  restore the saved ON/OFF choice with defaultON and immediately sync changes.
  Keep upstream/pinned packages and historical rollback wrappers unchanged.
- Retain zero voltage boost,55.2 V ceiling,53.6 V float,35/100 A ceilings,
  600-second confirmation and30-minute backstop plus all existing guards.
- Eight native suites, both37-case operating regressions and23 compiled checks
  pass. Installed under a mains-connected GX hold; savedOFF and savedON each
  survived a verified software reboot with all saved stops retained. Restored
  DESS/GX settings, left Auto CVLON, and passed12 native GX/MultiPlus samples
  with no final alarms and zero CAN errors. Temporary stop-related low-battery
  warning cleared before qualification passed.
- No new full-charge, closed-loop Auto CVL or physical power-loss qualification
  is claimed. Reboot still reinitialises the upstream charging stage.

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

## 5 October: SmartShunt Synchronisation Timing Trial

- Operator authorised a supervised timer-only firmware change after checking
  upstream timer documentation, a first-hand SmartShunt timing report and
  Victron guidance about fluctuating solar. Ten minutes is a trial margin,
  not an official recommended duration or proof of full charge.
- Add `yambms-top-charge-trial.yaml`: inherit the complete balancing-status
  release, override cut-off confirmation from60s to600s, identify the trial
  in Firmware Role and expose a read-only confirmation-time sensor.
- Keep maximum EOC30min, bulk55.2V, float53.6V,35/100A ceilings, automatic
  SOC-limit disabled, rebulk85%/52.8V, saved stops and all raw BMS/temperature/
  cell/freshness guards. No BMS, SmartShunt or VEConfigure parameter writes.
- Operator readback confirms SmartShunt280Ah, charged55.0V, tail1.5%=4.2A,
  detection5min, Peukert1.01, efficiency99%, threshold0.1A, Keep SOC on reset,
  discharge floor20% and time-to-go averaging3min.
- Eight native suites and37 legacy+37 status-profile scenarios pass. Add18
  compiled-image checks, including actual600000ms delay,1800000ms maximum,
  matching parser/guards and checksum-verified previous-release rollback.
- Upload under verified mains-connected zero-energy hold; saved stops survive,
  timer600s and trial role read back live. Restore captured DESS/GX settings.
  Temporary MultiPlus low-battery alarm clears during recovery, not suppressed.
- Twelve native GX/MultiPlus recovery samples pass with sole instance512,
  restored DESS/GX settings, unchanged35/100A native permissions and CAN errors0.
- Manufacturer-classified normal byte16 bit5 recurs live alongside reported
  balancing and is accepted with warnings/protections/blocking-unknown0 and
  CAN errors0. This confirms acceptance on recurrence, not balancer-current
  measurement or proof of completed cell balancing.
- Full-charge synchronisation and the subsequent float transition are still
  awaiting the physical charging cycle. Do not equate installation proof with
  those outcomes; confirm SmartShunt automatic-sync history and actual GX limits.
- Matching source/keys, installed and rollback images, snapshots and evidence
  are retained in checksum-verified private local/NAS backups. Trial source
  and this documentation are local changes, not yet committed or pushed.

## 8 October: Measurement-Pair Trial Rejected

- Operator authorised supervised OTA after the 6 October qualification and
  renewed 8 October low-battery investigation. Re-ran all nine native suites,
  both 37-scenario operating-policy suites and 19 compiled checks successfully.
- Installed `yambms-measurement-pair-fix-20261006.ota.bin`, SHA256
  `8b745d7f931f96f7f12f140786c7eb97c123596ab4809c1090018986406fd2b0`.
  Verified previous Auto CVL persistence image as rollback before upload.
- Grid-supported hold verified actual MultiPlus 0.5 A / SmartShunt 0.6 A,
  zero operational allowances and persisted master/directional stops.
  Firmware returned with fresh telemetry, saved stops, Auto CVL ON and CAN
  counters zero. No BMS, SmartShunt, VEConfigure or MPPT settings were written.
- Restored captured DESS mode1, system charge limit-1 and discharge power-1.
  Twelve native GX/MultiPlus recovery samples passed; temporary hold-induced
  low-battery warning cleared. Normal 35/100 A ceilings and all existing
  protection, voltage and timer settings remain unchanged.
- HA recorded brief bounded measurement-pair waits without metadata readiness
  dropping after initial startup. Sixty post-restoration observations have no
  metadata drops, CAN errors or sampled battery alarms. One queued TX frame
  drained on the next sample; preserve the original overly strict empty-queue
  verdict separately from the successful error/freshness assessment.
  DESS cloud ON / GX active with approximately11.18 hours of schedule remaining
  was verified; this does not prove tonight's charging or notification delivery.
- Physical USB CAN identity, not old interface names, is now checked by the
  private update wrapper: USB currently enumerates as can0. Original bus-error
  counter was accumulated and stable; restarted controller counters are zero.
  No GX CAN-service or interface configuration was changed.
- Cell4's 2.949 V reading under approximately73.4 A earlier tonight is a
  separate unresolved cell/connection/sensing issue. Firmware does not cure
  it; no deliberate heavy-load reproduction or full-discharge test was run.
- Final acceptance supersedes the initial passing samples: HA records
  Metadata Ready OFF20:51:11.873407 toON20:51:22.371996 UTC, with Complete
  Data Fresh continuouslyON and operating allowances0/0. Exact packet/field
  cause is not captured. Performed a controlled rollback to the previous
  Auto CVL persistence image under a verified mains-connected zero-current
  hold. Candidate remains unqualified; preserve the initial evidence rather
  than relabeling its narrower historical result as final acceptance.
- Previous running role and restored35/100 A allowances verified live after
  rollback; original alarm fault is still unresolved. Prior matching source/
  firmware recovery archive20261005T151043Z verified locally, SHA256
  `2c958e2a1553107beaa78d816b2c7c3e08f77ba5330c50e2c3c822dc1641c17e`.
  Preserve failed-candidate evidence alongside, not instead of, that recovery.
- Rollback verification completed: twelve native GX/MultiPlus samples passed;
  DESS cloud/GX planning acceptance passed with11.81h remaining. No final
  battery alarms or CAN errors; no claim of overnight charging completion.

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
