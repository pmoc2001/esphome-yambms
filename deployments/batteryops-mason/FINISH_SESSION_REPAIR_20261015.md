# Finishing Session Repair

Status: offline candidate, not installed. No charge test or live setting change.

## Fault And Scope

The installed core's two delayed completion scripts did not cancel when native
charging permission disappeared, data went offline or the operator stopped
charging. Both scripts unconditionally recorded completion at expiry. Local
source execution reproduced this; retained 14 October evidence includes a new
completion timestamp during Wait/discharge without a new SmartShunt sync.

The repair gives each finishing attempt an identity, cancels both timers on
lost qualification and rechecks conditions at expiry. A cancelled attempt
cannot update Last Complete Charge or release the completion-related SOC cap.
The existing timestamp is preserved; old false records are not retroactively
corrected. It is replaced only after a newly qualified completion.

The deployment-specific guard reads fresh parser permissions, saved control
intent and pre-filter pack current/maximum cell samples. Thus a new stop or
discharge packet cannot be hidden by the slower merged sensor update. Short
completion also checks newly reported balancing flags. These flags are still
not a measurement of the separate active board's balancing current.

Both timer durations remain unchanged: 600 seconds without reported balancing,
or the enabled 30-minute backstop during qualifying balancing. Qualification
must be continuous: losing permission, data, voltage/current qualification,
or starting to discharge restarts the attempt, rather than counting the pause
as charging time. New Finishing Session diagnostics distinguish cancellation,
short confirmation and the EOC backstop. Normal Balancing/Float states remain.

This does not change the 55.2/53.6 V targets, 35/100 A ceilings, Seplos parser,
cell/temperature/status guards, saved stops, BMS settings, or SmartShunt settings.
It does not cure cell/connection problems or override native CCL zero.
The source tests deliberately retain a case where Yam completes below the
SmartShunt's charged-voltage threshold: Yam completion, shunt sync and physical
balance remain separate outcomes.

## Offline Verification

Use the pinned toolchain and private credentials described in README.md.

```bash
python3 bootstrap_packages.py
python3 tests/qualify_finish_session.py --output-dir /private/evidence
esphome compile yambms-finish-session-fix.yaml
python3 tests/verify_finish_build.py --build-root . --evidence-dir /private/evidence --rollback /private/installed-reader-v3.ota.bin
```

Native tests use actual extracted source with simulated time and ESPHome
entities. Parser tests use real request/response frames, including cases where
expiry precedes the next merge loop. Compiled-image checks verify the actual
generated timer arguments, private wrapper, reader source hashes, safety policy,
control persistence and the retained installed-reader rollback image.
These are offline proofs, not a physical CAN/GX charge test.

## Installation Gate

Do not upload as part of building. Obtain separate approval with the operator
present and mains available. Capture live settings and use the existing
verified grid-connected zero-energy hold and persisted stops in README.md.

After OTA, verify saved stops, candidate identity, fresh Seplos data, correct
35/100 A and 55.2/53.6 V limits, existing operator AutoCCL/AutoCVL choices,
sole CAN authority, native GX alarms and MultiPlus recovery. Any failure stays
stopped; rollback uses the retained 9 October transaction-reader-v3 image,
not the older reader which predates the request/response repair.

Only then consider the separately approved attended charge: early 35 A,
proposed 10 A final approach at the first highest-cell reading of 3.45 V.
No automatic current-setting controller is included here. Preserve all stops;
do not restart repeatedly against CCL zero or raise voltage to force sync.
Observe the SmartShunt's actual voltage/current qualification and new sync
count independently, and restore the captured normal ESS/DESS settings after
the attempt. A restart requires rechecking any temporary current ceiling.

Reference: [ESPHome script parameters and cancellation](https://esphome.io/components/script/).
