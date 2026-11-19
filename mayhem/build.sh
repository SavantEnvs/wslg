#!/usr/bin/env bash
# mayhem/build.sh — build the wslg fuzz targets and the functional test.
#
# wsldvc_ondatareceived: WSLDVCPlugin (the Windows RDP client plugin) compiled unmodified on Linux
#   against a small Win32/COM/WRL shim (mayhem/wsldvc/shim/, mayhem/wsldvc/shim.cpp). The shim keeps
#   everything in memory, and stands in for utils.cpp (Windows-shell glue: IShellLink, registry,
#   known folders), which cannot be built here.
# rdpapplist_server: the rdpapplist server channel plugin (rdpapplist/rdpapplist_common.c +
#   rdpapplist/server/rdpapplist_main.c, as rdpapplist/server/meson.build builds them) against the
#   distro's FreeRDP 3 / WinPR 3, with the WTS* channel transport served from memory
#   (mayhem/rdpapplist/wts_channel.c).
# Outputs land in out/ (gitignored upstream).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
OUT="$SRC/out"
OBJ="$OUT/obj"
mkdir -p "$OBJ/fuzz" "$OBJ/test"

# UBSan "alignment" is relaxed for this target only: the plugin's ReadUINT16/ReadUINT32 macros
# (WSLDVCPlugin/rdpapplist.h) dereference unaligned pointers into the received buffer on every PDU,
# which is well-defined on the x86/x64/arm64 Windows targets it ships for. ASan and the rest of
# UBSan stay on and halting.
FUZZ_SAN="$SANITIZER_FLAGS"
case "$FUZZ_SAN" in *undefined*) FUZZ_SAN="$FUZZ_SAN -fno-sanitize=alignment" ;; esac

WSLDVC_INC=(-I"$SRC/mayhem/wsldvc/shim" -I"$SRC/WSLDVCPlugin" -I"$SRC/mayhem/wsldvc")
WSLDVC_SRCS=(WSLDVCPlugin/WSLDVCCallback.cpp WSLDVCPlugin/WSLDVCFileDB.cpp mayhem/wsldvc/shim.cpp)
CXXSTD=(-std=c++17 -Wno-everything)

# 1) project (+ shim) with sanitizers, coverage instrumentation and DWARF 3
fuzz_objs=()
for s in "${WSLDVC_SRCS[@]}"; do
  o="$OBJ/fuzz/$(basename "${s%.cpp}").o"
  $CXX "${CXXSTD[@]}" "${WSLDVC_INC[@]}" $FUZZ_SAN $DEBUG_FLAGS -fsanitize=fuzzer-no-link -c "$s" -o "$o"
  fuzz_objs+=("$o")
done
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$OBJ/fuzz/lsan_off.o"
$CXX "${CXXSTD[@]}" "${WSLDVC_INC[@]}" $FUZZ_SAN $DEBUG_FLAGS -fsanitize=fuzzer-no-link \
  -c mayhem/wsldvc/fuzz_wsldvc.cpp -o "$OBJ/fuzz/fuzz_wsldvc.o"
$CC $FUZZ_SAN $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OBJ/fuzz/standalone_main.o"

# 2) fuzzer + standalone reproducer
$CXX $FUZZ_SAN $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OBJ/fuzz/fuzz_wsldvc.o" "${fuzz_objs[@]}" \
  "$OBJ/fuzz/lsan_off.o" -lpthread -o "$OUT/wsldvc_ondatareceived"
$CXX $FUZZ_SAN $DEBUG_FLAGS "$OBJ/fuzz/standalone_main.o" "$OBJ/fuzz/fuzz_wsldvc.o" "${fuzz_objs[@]}" \
  "$OBJ/fuzz/lsan_off.o" -lpthread -o "$OUT/wsldvc_ondatareceived-standalone"

# 3) functional test, normal flags
test_objs=()
for s in "${WSLDVC_SRCS[@]}" mayhem/wsldvc/wsldvc_test.cpp; do
  o="$OBJ/test/$(basename "${s%.cpp}").o"
  $CXX "${CXXSTD[@]}" "${WSLDVC_INC[@]}" -O1 $COVERAGE_FLAGS -c "$s" -o "$o"
  test_objs+=("$o")
done
$CXX $COVERAGE_FLAGS "${test_objs[@]}" -lpthread -o "$OUT/wsldvc_test"

# ---- rdpapplist server ----
RDP_CFLAGS=($(pkg-config --cflags winpr3 freerdp3 freerdp-server3))
RDP_LIBS=($(pkg-config --libs winpr3 freerdp3 freerdp-server3) -lpthread)
RDP_INC=(-I"$SRC/rdpapplist" -I"$SRC/rdpapplist/server" -I"$SRC/mayhem/rdpapplist")
RDP_SRCS=(rdpapplist/rdpapplist_common.c rdpapplist/server/rdpapplist_main.c mayhem/rdpapplist/wts_channel.c)
mkdir -p "$OBJ/rdp-fuzz" "$OBJ/rdp-test"
rdp_fuzz_objs=()
for s in "${RDP_SRCS[@]}" mayhem/rdpapplist/fuzz_rdpapplist_server.c; do
  o="$OBJ/rdp-fuzz/$(basename "${s%.c}").o"
  $CC "${RDP_INC[@]}" "${RDP_CFLAGS[@]}" $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -c "$s" -o "$o"
  rdp_fuzz_objs+=("$o")
done
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "${rdp_fuzz_objs[@]}" "$OBJ/fuzz/lsan_off.o" \
  "${RDP_LIBS[@]}" -o "$OUT/rdpapplist_server"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OBJ/fuzz/standalone_main.o" "${rdp_fuzz_objs[@]}" "$OBJ/fuzz/lsan_off.o" \
  "${RDP_LIBS[@]}" -o "$OUT/rdpapplist_server-standalone"
rdp_test_objs=()
for s in "${RDP_SRCS[@]}" mayhem/rdpapplist/rdpapplist_test.c; do
  o="$OBJ/rdp-test/$(basename "${s%.c}").o"
  $CC "${RDP_INC[@]}" "${RDP_CFLAGS[@]}" -O1 $COVERAGE_FLAGS -c "$s" -o "$o"
  rdp_test_objs+=("$o")
done
$CC $COVERAGE_FLAGS "${rdp_test_objs[@]}" "${RDP_LIBS[@]}" -o "$OUT/rdpapplist_test"

echo "built: $(ls "$OUT" | grep -v obj | tr '\n' ' ')"
