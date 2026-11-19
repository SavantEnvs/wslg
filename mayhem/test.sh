#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# Known-answer test of the WSLDVCPlugin rdpapplist client (mayhem/wsldvc/wsldvc_test.cpp): caps
# reply bytes, accepted/rejected PDUs, the shortcut/icon paths and the icon file bytes. Every check
# prints PASS/FAIL; the expected list is fixed here so a binary that prints nothing fails them all.
EXPECTED=(create_instance caps_v4_accepted caps_reply_bytes update_with_icon_accepted update_link_path
  update_link_args update_link_target update_icon_path update_icon_file_bytes delete_known_accepted
  delete_unknown_rejected icon_size_overflow_rejected dotdot_appid_rejected missing_required_flag_rejected
  update_before_caps_rejected caps_v3_rejected caps_space_in_provider_rejected caps_bad_guid_rejected)
# rdpapplist server channel (mayhem/rdpapplist/rdpapplist_test.c): client caps decoding.
RDP_EXPECTED=(client_caps_delivered two_caps_pdus_in_one_read unknown_cmd_ignored truncated_caps_rejected)
BIN="$SRC/out/wsldvc_test"
RDP_BIN="$SRC/out/rdpapplist_test"
[ -x "$RDP_BIN" ] || { echo "missing $RDP_BIN — mayhem/build.sh must build it" >&2; emit_ctrf rdpapplist_test 0 "${#RDP_EXPECTED[@]}"; exit 1; }
[ -x "$BIN" ] || { echo "missing $BIN — mayhem/build.sh must build it" >&2; emit_ctrf wsldvc_test 0 "${#EXPECTED[@]}"; exit 1; }
log=$("$BIN" 2>&1); rc=$?
echo "$log"
passed=0; failed=0
for t in "${EXPECTED[@]}"; do
  if grep -qx "PASS $t" <<<"$log"; then passed=$((passed+1)); else echo "FAILED: $t"; failed=$((failed+1)); fi
done
[ "$rc" -eq 0 ] || [ "$failed" -gt 0 ] || { echo "wsldvc_test exited $rc"; failed=$((failed+1)); }
rlog=$("$RDP_BIN" 2>&1); rrc=$?
echo "$rlog"
rfailed=0
for t in "${RDP_EXPECTED[@]}"; do
  if grep -qx "PASS $t" <<<"$rlog"; then passed=$((passed+1)); else echo "FAILED: $t"; failed=$((failed+1)); rfailed=$((rfailed+1)); fi
done
[ "$rrc" -eq 0 ] || [ "$rfailed" -gt 0 ] || { echo "rdpapplist_test exited $rrc"; failed=$((failed+1)); }
emit_ctrf wslg-known-answer "$passed" "$failed"
