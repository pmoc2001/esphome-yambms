# BatteryOps Mason deployment

Qualified local deployment for one 16S 280 Ah Seplos Mason, SP18B V3 firmware
1.5, on the Waveshare ESP32-S3-RS485-CAN. This is not a general-purpose
firmware for unknown Seplos models or other batteries.

See the [complete fork change log](CHANGELOG.md) for all customisations relative
to the pinned upstream baseline, their test evidence and remaining boundaries.

## Prepared Release - 20 October 2026

`yambms-reboot-timer.yaml` combines two focused corrections with the installed
reader and finishing guards. **It is not installed yet.** Publishing source or
passing simulated restart tests is not proof of a live controller restart.

- AutoCCL restores the last explicit ON/OFF choice, defaults ON without a saved
  choice, and immediately requests native flash persistence. AutoCVL already
  follows this policy and is unchanged.
- EOC deliberately starts ON after a restart. Turning it OFF is a supervised
  maintenance-session exception, not a permanent reboot policy.
- The finishing elapsed-time display now reports zero, not 600 seconds, when
  its timer start is just ahead of the last checked timestamp. This repairs
  unsigned subtraction in diagnostics; it does not shorten or bypass charging
  completion checks.

Saved master/directional stops, fresh-data admission, 55.2/53.6 V targets,
50/100 A ceilings and all cell/temperature/BMS safeguards remain unchanged.
No T-Connect transport or polling changes are included. See
[qualification and deployment boundaries](RELEASE_20261020.md).

## Installed Release - 19 October 2026

**Bounded finishing pause v2 is installed on the Waveshare.** The operator
confirmed supervision with mains available. One OTA completed, followed by
successful native GX recovery and 12 consecutive verification snapshots.
This confirms installation and restored control, not completed top balancing.

### Why The Change Was Needed

Our previous custom finishing guard cancelled an attempt whenever battery
current became negative, even briefly. That could discard progress during
small load-related reversals. The repair pauses an already-active attempt
within all three engineering bounds:

- Discharge no greater than **3 A**.
- A continuous reversal no longer than **60 seconds**.
- Cumulative discharge no greater than **0.05 Ah per attempt**. Positive
  charging does not refund this budget.

Paused time never counts towards completion, and discharge cannot start a
finishing attempt. Larger/longer discharge, unsafe cells or temperatures,
stale/invalid data, BMS prohibition and operator stops still cancel it. Raw
source and merged measurements remain checked. These are bounded trial
parameters, not cell-manufacturer limits or advice for another battery.

The normal **600-second** confirmation still requires the reported Equalizing
state to be OFF; balancing restarting resets that confirmation. EOC remains
OFF for this supervised session. No automatic maintenance timeout was added.
The optional EOC backstop is not evidence of a completed balance.

New web/API diagnostics show the finishing reason, qualified seconds, paused
seconds and discharge budget used. Code comments explain the non-obvious
safety rules, conservative interval accounting and cancellation conditions;
the tests cover their boundary cases.

### Unchanged Settings And Evidence

Bulk/float remain **55.2/53.6 V**, with **50 A charge / 100 A discharge** output
ceilings. AutoCCL and AutoCVL are ON. No Seplos or SmartShunt settings were
written, and no protection threshold was relaxed. SmartShunt had already
synchronised before this upload; the repair does not force Yam SOC to 100%.

Offline qualification passed 166 existing policy scenarios, 18 core and 26
raw-parser pause scenarios, 10 parser/safety executables, ESP32-S3 compilation
and 20 build/provenance checks. A 25-hour, 8,606-snapshot replay passed in both
callback orderings without false completion. **That recording still did not
finish**: balancing and other legitimate interruptions remain independent
conditions. Offline replay is not physical-battery proof.

The initial recovery check incorrectly assumed configured inverter power zero
would produce an effective discharge limit of zero in today's Keep Batteries
Charged session after discharge permission was restored. It stopped and saved
all three Yam controls OFF. This was an update-procedure check failure, not
evidence of a 5 kW discharge or a new BMS fault. No second OTA or rollback was
needed. Corrected admission kept Yam's Discharge enabled switch OFF until GX
CCL/DCL and effective discharge were zero and measured Multi/shunt current was
within 0.8 A. It then restored the exact captured operating settings.

Native recovery passed from **09:09:51 to 09:11:19 BST**: sole Yam battery
instance 512, fresh data, DVCC control, current/voltage limits, saved switch
intent and no active battery alarms. ESS remains Keep Batteries Charged.
The captured native DESS Mode=1 was restored, while DESS scheduling remained
inactive in this session; this was not a switch back to Optimised operation.

The subsequent **09:10-09:15 BST** GET-only watch recorded 86 sequential
samples. Natural reversals reached -0.71 A; Yam reported Paused: brief
discharge, retained its pause/budget counters, and returned to Waiting:
balancing active. Paused time reached 34 seconds and the recorded discharge
budget 0.005145 Ah. Normal qualified time remained zero; no new completion
or Float transition was reported. Data stayed fresh, CAN counters stayed zero
and no active battery alerts were recorded. This demonstrates the new pause
path on the running device, not the full 600-second completion path. The web
reads are sequential, not atomic serial captures.

**Historical v2 restart caveat:** inherited overrides boot EOC ON and AutoCCL OFF. This
installation explicitly restored EOC OFF and AutoCCL/AutoCVL ON before output
admission. The bounded-pause repair does not change that persistence behaviour.
Keep the current qualified 50 A image as rollback, not an older 35 A release.

The fitted active balancer is established. Its actual transfer current and
revision-specific timeout/reactivation state are not available through existing
telemetry. No speculative board-reset procedure or balance-threshold change
was applied. Successful installation is not proof that the pack is balanced.

### Publication And Hardware Status

This operating branch incorporates the previously private transaction-reader,
50 A and finishing-session source and tests. `yambms-finish-pause.yaml` includes
the timer-display correction; use the retained private v2 image for an exact
rollback, not a rebuild of the now-corrected file. The 20 October entry point
adds the reboot policy. Source publication does not install either change.
Credentials, compiled binaries, generated C++ and private telemetry are excluded.

The existing bo-Pi remains the sole Seplos poller. The separate T-Connect plus
Waveshare architecture is an offline candidate, not the live control path.
Do not replace the poller or connect a second native poller as part of this fix.

## Previous Release - 16 October 2026

The 16 October installation record identifies `yambms-charge-ceiling-50a.yaml`
as the installed entry point: 50 A charge / 100 A discharge, with the existing
55.2 V bulk ceiling, 53.6 V float, transaction reader and finishing-session fix.
This paragraph records that release; it is not a fresh live settings check.
The older 35 A values in the dated sections below describe those earlier tests.

The T-Connect plus Waveshare candidate is prepared in a separate working tree,
not included in this operating branch. T-Connect is not yet at the battery. Neither candidate has been
uploaded, and the existing bo-Pi poller remains in service.

## Previous Release - 9 October 2026

Transaction reader v3 was installed. The authorized grid-supported OTA retained
saved stops, then restored the exact captured GX/DESS settings and unchanged
charging policy. Initial native checks passed; the temporary maintenance-hold
low-battery warning cleared before normal operation was accepted.

The subsequent 20-minute live trial passed four complete settings cycles,
2,304 native GX samples, 793 CAN limit frames and 794 clear CAN alarm frames.
No readiness drops or unexplained zero limits occurred. Natural battery current
ranged from 18.1 A charging to 69.7 A discharging. DESS planning and delivered
schedule remained active. A 24-hour recurrence review is still required; this
is bounded live proof, not closure of every historical Victron warning.

Private evidence: `yambms-reader-investigation-20261009/live-acceptance.json`.
Installed image SHA256:
`65e88e795e313a872f69fa683912152f006b0d944f257a11d5fa31f307a7f06a`.
Rollback remains the 5 October persistent Auto CVL image, not the rejected
paired-measurements release or withdrawn transaction-reader v2.

## Previous Release - 15 October 2026, Recovery Verified

`yambms-finish-session-fix.yaml` adds guarded finishing timers to the installed
transaction-reader baseline. It was uploaded on 15 October with authorization;
**bounded live recovery passed after maintenance**. See the
[repair and acceptance procedure](FINISH_SESSION_REPAIR_20261015.md).
The 600-second confirmation and 30-minute backstop remain; an interruption
cancels the attempt instead of later declaring it complete. No safety limits
or battery/shunt settings change. Yam completion still does not prove shunt
synchronisation or that every cell is balanced.

Offline acceptance passed: 166 logic scenarios, 10 parser/safety suites,
ESP32-S3 compilation and 14 generated-build/rollback checks. The upload and
saved-stop restart passed. Native GX received the correct limits under hold,
then the GX became unreachable during normal-operation verification; Seplos
telemetry subsequently became stale. The operator had announced cell 4
maintenance, but the cause of the disconnection is not yet confirmed.

All three Yam controls were saved OFF during that interruption. After the
operator confirmed maintenance complete and authorized normal restoration,
a new GX zero-current hold was established. Yam communication and permissions
were restored under that hold, then the original GX settings were restored
at 11:15:38 UTC: DESS 1, max charge current -1, max discharge power -1.
Twelve native GX/Yam checks passed with sole instance 512, fresh Seplos data,
35/100 A limits, 55.2 V ceiling and no active alarms or new CAN errors.
DESS cloud planning and delivered schedule both passed separate checks.
MPPT 1's connection-loss error cleared; it resumed managed charging.

Normal operation is restored, not a supervised full-charge test. At 11:19 UTC,
SmartShunt read 53.55 V / +22 A / 53.1%, Seplos 57.7%, and Yam 58% (Bulk).
Cell 4 was 3.347 V within a 16 mV spread. This is encouraging at moderate
charge current, not proof of the maintenance repair at high load or full
charge. No new shunt sync was recorded. The finishing repair still needs its
separate attended charge acceptance.

**Maintenance boundary:** with Yam master OFF after GX reboot, the MultiPlus
was observed charging from its local settings while no managed battery was
selected. Yam OFF is not a guaranteed charger stop or physical isolation.
Investigate that cold-start fallback separately; no charger fallback settings
were changed during this recovery.

Local evidence retains both the interrupted upload report
`GX_BALANCING_STATUS_UPGRADE_20261015T092951Z.json` and subsequent
`yambms-finish-repair-20261015/post-maintenance-restoration.json` and
`post-maintenance-final-gx.json`. Historical CAN bus-error totals remained
unchanged through recovery; active TX/RX errors and failed/queued TX were zero.

The rollback for this release is the previously installed **9 October
transaction-reader-v3** image, preserving the reader repair. Earlier rollback
descriptions below are historical. Do not upload without a supervised hold.

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
Charge output ceiling 50 A; discharge output ceiling 100 A. These are
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

## Auto CVL restart persistence - 5 October 2026

`yambms-auto-cvl-persistent.yaml` inherits the complete top-charge trial.
It changes only Auto CVL persistence and the visible firmware identity:
`RESTORE_DEFAULT_ON` restores the last operator ON/OFF choice, defaults ON
when no saved choice exists, and immediately syncs changes to native flash
preferences. The historical handover wrapper's `ALWAYS_OFF` remains unchanged
for rollback builds. No BMS, SmartShunt or charging-limit changes are included.

Build and qualify locally with private credentials outside Git:

```bash
esphome compile yambms-auto-cvl-persistent.yaml
python3 tests/qualify_top_charge.py --auto-cvl-persistence
```

Installed and verified on 5 October 2026. Offline qualification passed eight
native suites, both 37-case operating regressions and 23 compiled checks.
Saved OFF and saved ON each survived a verified software reboot under the
grid-connected hold, with master/directional stops intact. Auto CVL remains
ON. DESS and captured GX settings were restored; all 12 native recovery
samples passed with no final alarms and zero CAN errors. A hardware power-loss
test is not claimed. This switch persistence does not persist Yam's charging
stage: upstream startup still reinitialises its charge state.

## Top-charge synchronisation trial - 5 October 2026

`yambms-top-charge-trial.yaml` inherits the complete balancing-status release
and changes only `yambms_cutoff_timer` from 60 to 600 seconds. It also identifies
the trial in Firmware Role and exposes Cut-Off Confirmation Time for readback.
The 30-minute maximum EOC timer, float enable, voltage/current limits, disabled
80% automatic SOC limit and all cell/temperature/freshness/status guards remain.

The operator confirmed SmartShunt settings: 280 Ah, charged voltage 55.0 V,
tail current 1.5% (4.2 A), detection time 5 minutes, Peukert 1.01, efficiency
99%, current threshold 0.1 A, SOC-on-reset Keep SOC and time-to-go averaging
3 minutes. No SmartShunt or Seplos parameter writes are part of this trial.

600 seconds is an engineering test margin, not an upstream recommendation or
a guarantee of synchronisation. YamBMS and SmartShunt use different qualifying
conditions. Check actual shunt voltage/current over five uninterrupted minutes,
automatic-sync history, cell voltages/balancing and the native GX response to
the subsequent 53.6 V float request. A displayed YamBMS SOC of 100% alone is
not proof of physical full charge or SmartShunt synchronisation.

Offline preparation passed eight native suites, both sets of 37 operating
scenarios and 18 compiled-image checks. Those checks prove the timer value,
unchanged guards and matching prior-release rollback, not a live full charge.
Use `python3 tests/qualify_top_charge.py` after building the trial and running
the balancing-status native regression. Keep firmware and private evidence
outside public Git. Live installation follows the supervised hold below.

Installed on 5 October 2026 under that hold. The 600-second setting and trial
identity read back live; saved stops survived reboot. Twelve native GX/MultiPlus
recovery samples passed after the transient stop-related low-battery alarm
cleared. Captured DESS/GX settings were restored. The accepted byte16 bit5
recurred alongside reported balancing, with no warnings/protections/unknown-bit
block or CAN errors. Subsequent observation confirmed SmartShunt automatic
sync (counter14 to15) and Yam's53.6 V float request. A completed top balance
was not demonstrated: the upper-stage cell spread reached126 mV, and reduced
spread after settling is not proof of top balance. Timing strongly fits the
30-minute backstop; a direct timer-fired reason was not recorded.

References: [YamBMS timer behaviour](https://github.com/Sleeper85/esphome-yambms/blob/main/documents/README/YamBMS_functions.md),
[reported SmartShunt timer coordination](https://github.com/Sleeper85/esphome-yambms/issues/24#issuecomment-2618593499),
[Victron solar synchronisation guidance](https://www.victronenergy.com/media/pg/SmartShunt/en/troubleshooting.html).

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

## Measurement-Pair Repair - Trial Rejected 8 October 2026

`yambms-measurement-pair-fix.yaml` inherits the Auto CVL persistence
release. It changes our reader's measurement admission, not upstream YamBMS
charging policy, BMS settings, SmartShunt settings or the 35/100 A wire limits.

On 6 October a load transition produced two brief metadata-readiness failures
(256 ms and 148 ms), zero current allowances and a subsequent MultiPlus
low-battery alarm despite 43% SOC and no documented BMS protection. The old
reader compared separately updated pack and cell sensor states immediately.
The exact atomic inputs from that incident were not retained.

The candidate decodes each entire CRC-valid packet into internal measurements
before publishing sensor callbacks. Numerical sum/min/max tolerances remain
0.5 V / 0.05 V. A coherent pair also requires reception-time skew below one
second. After an established coherent pair, cross-packet disagreement alone
can wait less than 1000 ms for its counterpart. Duplicate or faulty messages
cannot restart that deadline. Startup has no grace admission, and a mismatch
that persists to the deadline blocks both allowances.

Fresh BMS limits, FET permissions, documented protections, unknown-bit stops,
invalid fields and the 30-second independent channel expiry remain immediate.
Cell-voltage and cell-temperature guards use conservative extremes from BOTH
current packets, so a new unsafe summary cannot wait for old detailed cells
to catch up. This is bounded packet assembly, not a general alarm debounce.
The one-second window is an engineering bound above the observed sub-300 ms
events, not a manufacturer charging recommendation.

```bash
python3 tests/qualify_measurement_pair.py
esphome compile yambms-measurement-pair-fix.yaml
python3 tests/qualify_measurement_pair.py --compiled
```

The native regression reproduces the load-step failure against the original
reader, then tests both message orders, deadlines, invalid data, immediate
stops, packet callbacks, clock wrap, CRC rejection and repeated load changes.
The existing safety and source-extracted operating tests are retained.
`tests/measurement_pair_live_readonly.py --output <private-evidence.json>`
checks the existing device and locally replays decoded live web samples.
Those sequential observations are not atomic serial captures or proof that
the candidate controls a physical GX. No OTA is performed by either script.
Upload and supervised native GX/MultiPlus verification remain separate gates.

The repair was installed for a supervised trial on 8 October under a verified
grid-supported hold (MultiPlus 0.5 A, SmartShunt 0.6 A). The running firmware
role ends `paired measurements v1`. Saved master/directional stops survived
the restart; Auto CVL remained ON. Twelve native GX/MultiPlus samples passed
after restoring the captured settings. CAN counters remained zero, battery
alarms cleared, and the 35/100 A ceilings and voltage/timer settings were
unchanged. DESS cloud planning and GX schedule delivery were verified separately.

The initial observation recorded bounded packet-wait diagnostics without a
metadata-readiness OFF transition after startup. This is live evidence
of the new admission path; the original high-load incident remains qualified
by offline regression, not deliberately reproduced on the battery. Cell 4's
separate load-dependent voltage sag remains undiagnosed and is NOT repaired
by this firmware. Do not weaken the cell-voltage guards to hide it.

GX reboot enumeration had swapped interface names: the USB adapter is currently
`can0`, not historical `can1`. Deployment verification now checks the physical
USB VID/PID/serial, active YamBMS identity and sole native CAN battery rather
than assuming interface numbering. No GX CAN-service configuration was changed.
Installation evidence and private recovery archives remain outside public Git.

**Final acceptance failed:** subsequent HA recording shows Metadata Ready OFF
from 20:51:11.873407 to 20:51:22.371996 UTC, while Complete Data Fresh stayed
ON. Both operating allowances became zero. The early twelve-sample pass and
60-sample observation do not override this later failure. The running controller
was rolled back to the previous Auto CVL persistence image, with the captured
settings restored. Do not install this
candidate again until the real packet timing/invalid-data cause is captured,
added to regression coverage and qualified by a longer supervised trial.
The original timing/alarm fault remains unresolved. Its qualified recovery
archive remains verified locally and on the NAS; retain that archive separately
from the rejected candidate's source, trial reports and backup.

### 9 October: Request-Identified Reader Candidate

The deep dive found two reproducible reader defects: a 26-register settings
reply was mistaken for cell telemetry, and independently sampled summary/cell
blocks were incorrectly required to agree during current changes. This also
explains why the one-second pairing candidate was insufficient. See
[the incident analysis and acceptance procedure](TRANSACTION_READER_INCIDENT_20261009.md).

`yambms-transaction-reader.yaml` prepares request/response association and
independent packet validation, with immediate real protections retained.
The additional extrema-overwrite bug in the rejected candidate is corrected.
`tests/qualify_transaction_reader.py` verifies the reconstruction, safety
regressions, operating policy and optional compiled artifact.

**Installed and bounded live trial passed on 9 October.** The earlier
paired-measurements image remains rejected, not the rollback target.
Credentials and compiled images stay outside Git.

Subsequent [pre-upload review](PREUPLOAD_REVIEW_20261009.md) found and corrected
damaged-frame resynchronization and request-shaped response-prefix defects.
The candidate is now **transaction reader v3**; v2 is withdrawn from upload
consideration. Actual poller traffic has now passed replay: 34 telemetry cycles
and a complete settings cycle. Identical recorded bytes block the rejected
reader for 10.637 seconds at the settings reply but do not block v3. This clears
the pre-upload check. Subsequent physical ESP32/CAN/GX acceptance passed the
20-minute trial summarized above. Longer-term recurrence review remains open.
