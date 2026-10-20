# Pre-upload review and supervised live trial

## Deployment Follow-Up: 13:59 UTC

V3 was installed through the established grid-supported hold procedure. Saved
stops survived OTA and the exact original GX/DESS settings were restored.
The hold-induced low-battery warning cleared. Charging/BMS safety settings
were not changed.

The 13:39-13:59 UTC live recording passed: four settings cycles, 2,304 native
GX samples, 793 raw CAN limit frames, and 794 all-clear CAN alarm frames.
No readiness drop, unexplained zero allowance or unexpected restart occurred.
Current ranged from +18.1 A charging to -69.7 A discharging without a forced
stress test. Maximum observed limit-frame gap was 1.676 seconds. DESS cloud
planning and GX schedule delivery passed with 11.52 hours of schedule remaining.

Combined proof: private `live-acceptance.json`, with hashes of recorded streams,
qualification and OTA/restoration evidence. The first observer launch failed
because GX lacks the shell `timeout` utility; it changed no controls and was
replaced with a Python-alarm-bounded observer before the complete timed trial.
The old compact console formatter uses historic can1 and prints null fields;
the raw upgrade assertions and timed observer use verified active USB can0.

This clears the bounded live trial, not the 24-hour recurrence review or exact
attribution of the earlier 09:55 UK warning. Sections below retain the earlier
pre-upload review sequence.

Further review found two framing defects in transaction-reader v2 before
any upload. An offline pass and successful build were insufficient.

## Findings and corrections

1. After CRC failure, v2 discarded one byte and scanned inside the damaged
   message. A long damaged message containing a valid nested request/reply
   could therefore inject apparent telemetry. A regression against v2 failed
   at the assertion that accepted telemetry must not increase. The revised
   reader discards the complete damaged message and outstanding association.
2. A PIC reply can have an eight-byte prefix that is also a CRC-valid request.
   v2 classified that prefix first and lost the actual reply. The revised
   reader first validates the complete expected response, then considers a
   request/retry prefix if that complete candidate fails CRC. An explicit
   regression now covers this. These are protocol edge cases, not claims that
   these exact bytes occurred during the production alarm.
3. Added 286 single-byte corruption/drop cases across all three requests and
   responses, clean-cycle recovery, and a complete synthetic telemetry/settings
   cycle delivered byte by byte. Existing protection, freshness and startup
   regressions remain required.
4. Added `transaction_reader_test --wire-replay` to ingest actual timestamped
   hex chunks without fabricated startup data or extra requests. It reports
   accepted frames, admission and allowances after each chunk.

Current candidate identity: `transaction reader v3`. The build directory is
still named `transaction-reader-v2` to reuse compiler artifacts. The generated
identity, source hashes and separately named v3 image define the release.
The earlier v2 image must NOT be uploaded despite its earlier test pass.

Production firmware and controls remain unchanged. All real BMS protections,
cell/temperature thresholds and charging policy remain unchanged.

## Actual-recording gate passed at 12:15 UTC

The existing SSH key works with `casper@192.168.1.46` (`bo-charger-pi`).
A bounded six-minute strace of the existing poller's serial read/write
syscalls recorded actual requests and replies, including a complete settings
cycle. The observer did not open the serial device, issue requests, restart
the service or change controls. Syscall tracing adds brief scheduling overhead;
this is poller-side completion timing, not ESP32 UART arrival timing.

Replay preserved all 292 chunks (5,272 wire bytes): 34 complete telemetry
cycles and one settings cycle. An independent request/CRC oracle agrees with
every accepted telemetry frame. No requests or startup data were fabricated.

- Rejected reader: settings reply `0x1350` triggers a 10,637 ms admission block
  with zero charge/discharge allowances, recovering at the next telemetry poll.
- V3 reader: zero admission blocks or unintended zero allowances after startup
  on the exact same recording, including that settings reply.
- Capture supervision: 73 healthy samples, no alerts or MultiPlus alarms,
  live caps 35/100 A throughout. Poller PID 8751 unchanged and active.
- Post-capture check: `TracerPid=0`, with fresh successful polling logs.

The combined `release-status.json` now marks recorded-traffic replay and
pre-upload checks passed. The standalone offline qualification JSON retains
its narrower scope and does not itself claim recorded-traffic acceptance.
At that pre-upload checkpoint, supervised installation and physical candidate
control had not occurred. They subsequently passed the trial above; 24-hour
acceptance remains outstanding.
The separate 09:55 UK alarm is not fully attributed by this recording.

## Evidence and replay

Private fixer folder `output/yambms-reader-investigation-20261009/` contains:

- `transaction-v2-before-review/`: source before review.
- `qualification.json`: earlier v2 offline proof, not deployment approval.
- `review-qualification.json`: revised results with the open gate explicit.
- `capture-session-qualification.json`: repeated v3 offline qualification.
- `serial-full.proof.json`: complete actual-recording replay and trace hash.
- `serial-full-comparison.json`: rejected/v3 comparison on identical bytes.
- `serial-full-health.json`: bounded capture supervision and unchanged PID.
- `recorded-replay-manifest.json`: source, native binary and firmware hashes.
- `release-status.json`: combined pre-upload acceptance, explicitly not live.
- Earlier HA/settings exports and unchanged-device snapshots.

Run `python3 tests/qualify_transaction_reader.py --help` for qualification.
Native replay input is one line per captured chunk:
`<relative_monotonic_ms> <hex_bytes_without_spaces>`. Preserve order, timing and
provenance. Never manufacture missing requests and call it actual-wire replay.
