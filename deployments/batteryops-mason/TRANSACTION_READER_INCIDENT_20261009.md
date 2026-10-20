# Seplos reader incident: root cause and candidate repair

**Later review:** transaction-reader v2 had additional framing edge cases.
See [the pre-upload review](PREUPLOAD_REVIEW_20261009.md). Current candidate is
v3; actual poller-traffic replay passes, including the settings collision.
Authorized installation and the 20-minute live trial subsequently passed;
four settings cycles and natural load changes showed no false block. The
24-hour recurrence review and exact earlier 09:55 warning attribution remain open.

Status: v3 installed after offline and actual-traffic qualification. The
initial investigation made no control changes. Subsequent OTA used the standard
temporary grid-supported hold and restored captured settings unchanged.
No BMS writes, poller restarts or serial-port takeover occurred. A bounded
serial-syscall trace recorded real traffic and is now detached. The 5 October
persistent Auto CVL image remains the rollback target.
The rejected 6 October paired-measurements image must not be uploaded again.

## Findings

### 1. Replies were identified by length, not the requested register

The passive reader accepted every address-0, function-4 reply with 52 payload
bytes as cells/temperatures. bo-Pi reads live cells at 0x1100, 26 registers, but
also reads 106 settings registers at 0x1300 in chunks of 40, 40 and 26.
The last settings reply at 0x1350 has exactly the same address/function/length
and a valid CRC. It was decoded as cell voltages and temperatures.

This is a protocol identity error, not noisy wiring or the qualified reserved
PIC status bit. Modbus read responses contain a byte count and register data,
but not the starting register address. That address belongs to the request.
See the [Modbus application protocol, section 6.4](https://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf).

On 8 October, Metadata Ready became OFF at 20:51:11.873407 UTC and recovered
at 20:51:22.371996. The settings snapshot was received at 20:51:12.972665 UTC.
The corresponding 0x1350 words begin 3400,50,30,960,...,28000,28000,18676.
Misdecoded, these include supposed cells at 0.050 V, 0.030 V and 28 V.
Native tests using those recorded words reproduce metadata OFF and zero
charge/discharge allowances in the original and rejected readers.

HA still displayed plausible cells during the rejected candidate's failure:
52.703 V cell sum versus 52.70 V summary, extrema 3.265/3.303 V versus
3.267/3.302 V summary. The 3-second sensor throttle suppresses the second
cell-shaped packet in the displayed sensor states, but the rejected candidate
committed it into its new unfiltered internal measurements. The filter-model
regression demonstrates why a good-looking HA display did not clear the fault.

The original incident evidence above is time-correlated reconstruction, not a
recording of its exact serial bytes. A subsequent 9 October actual poller
recording independently reproduces a 10.637-second block at the settings reply
in the rejected reader, with no block in v3. See the pre-upload review.

### 2. Independently sampled blocks were required to agree

At 04:45:06 UTC on 9 October, the installed old reader rejected a normal
current transition. Pack summary: 53.93 V, maximum cell 3.435 V. The cell
reply about 240 ms later: sum 53.637 V, maximum 3.373 V, minimum 3.320 V.
The maximum differs by 62 mV, exceeding the reader's 50 mV comparison.
Both replies can be valid because the BMS samples them at different times.
Metadata stayed OFF for 10.487 seconds until the next poll, despite fresh data
and no decoded protection or blocking-unknown bit. Local reconstruction
reproduces the block in both readers. A one-second pairing grace cannot fix it.

The original reader also evaluates partially published filtered sensor states,
which explains shorter inter-packet readiness drops during load changes.

The adapter responds to invalid metadata with 0/0 allowances and exclusion
from aggregation. Thus software validation can interrupt battery operation
without a genuine empty battery. The CAN loop also requires a combined pack.
This investigation does NOT establish the exact CAN/GX causal chain for the
separate 09:55 UK MultiPlus warning: five-second GX history did not capture
zero allowances during that particular warning. Do not claim every alarm
has now been attributed, or suppress genuine BMS alarms.

### 3. A candidate-only extrema bug was exposed by safety regression

The rejected candidate inserted summary extrema into array positions 0/1
before checking all individual extrema. This could overwrite the only high
or low cell/temperature value. The new code first calculates both extrema
across the complete arrays and then includes the summary extrema. Tests now
exercise every one of the 16 cell positions and all six temperature positions.
This is a protection defect in the uninstalled candidate, not the demonstrated
cause of this morning's alarm.

## Candidate solution

`yambms-transaction-reader.yaml` explicitly enables request tracking; older
wrappers retain their legacy behavior for reproducible comparison. This is
installation-specific: one Seplos V3, address 0, bo-Pi as the only poller,
passive requests and responses visible on RS485-2. It is not a general-purpose
multi-battery driver or a replacement for bo-Pi polling.

- Associate CRC-valid responses with a preceding CRC-valid request, matching
  address, function and byte count. Admit only 0x1000 pack, 0x1100 cells and
  0x1200 PIC telemetry. Consume all other replies whole, including long identity
  replies; never scan their payload as independent telemetry.
- Ignore unsolicited/expired/mismatched replies. A request expires at 1500 ms;
  incomplete buffered fragments clear after a 100 ms processing gap. The latter
  is recovery from a stalled fragment, not proof of Modbus wire timing.
- Commit complete identified packets before sensor callbacks. Require valid,
  finite fields and independent freshness of all three telemetry streams.
  Remove cross-message numerical agreement as a permission gate in this mode.
- Retain 30-second freshness expiry, immediate BMS zero-current/FET/protection
  handling, unknown-bit fail-closed behavior and all actual cell/temperature
  guards. Always check the worst extrema from both independently sampled blocks.
- Expose admission reason and request/telemetry/ignored/unassociated/exception
  counters. These distinguish reader problems from actual BMS protections.

No changes to charging policy, bulk/float voltage, 600-second cutoff, Auto CVL,
SOC controls, CAN speed, 35/100 A ceilings or saved user stop intentions.

## Proof and limits

Run `python3 tests/qualify_transaction_reader.py --help` for the offline gate.
The qualification reproduces both defects against the original and rejected
source, executes ten sanitizer-backed native suites, runs both 37-scenario
operating-policy suites, and checks the compiled component hashes, enabled
transaction mode, unchanged control settings and rollback image hash.

Private artifacts: `output/yambms-reader-investigation-20261009/` in the fixer
workspace contains the bounded read-only HA export, settings words, reproduction
source snapshot, qualification JSON and live read-only snapshots. This folder
is outside the fork. The image is in the private Waveshare build workspace,
not public Git; it contains deployment credentials and must remain private.

Reconstructed tests do not establish actual new-firmware UART timing, request
visibility or inverter control. Historical passive-link observations counted
143 bytes per normal poll, consistent with three 8-byte requests plus the
39/57/23-byte responses, and documented requests on this bus. The new counters
must confirm that association on the actual device before acceptance.

## Supervised Acceptance Procedure

1. Capture current GX/ESS/DESS and device controls and verify the known rollback
   image. With the operator present and mains available, use the existing
   grid-supported hold procedure. Verify low battery current before OTA.
2. Upload only the new transaction-reader image. Confirm exact identity,
   passive UART, observed requests, three admitted telemetry responses per
   normal poll, correct values and sole GX battery identity. If requests are
   not visible, STOP and roll back; never silently fall back to length matching.
3. Observe at least three scheduled settings-read cycles (allow 20 minutes).
   Ignored-response counts should advance without readiness or allowance drops.
   Check normal load/charge transitions within existing safe operating limits.
4. Capture admission and transaction counters alongside native GX limits,
   alarms and actual CAN output at sub-second/event resolution. Do not rely
   solely on filtered HA values or five-second history. Missing source data and
   actual protection inputs must retain their qualified stop behavior; do not
   deliberately create an electrical fault to test this.
5. Restore the captured operational settings, including DESS, and verify both
   planning and actual schedule delivery. Roll back on an unexplained block,
   restart, incorrect allowance, alarm, or failed request association.
6. Continue 24-hour observation before calling the intermittent fault resolved.

Steps 1-5 passed the 9 October trial documented in the pre-upload review.
Step 6 remains outstanding. This is a live-tested repair for demonstrated
reader defects, not proof that every reported Victron warning is resolved.
