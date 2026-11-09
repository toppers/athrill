#!/bin/sh
# Checks that the unit tests catch deliberate faults in athrill_jit.c:
#   test/jit/mutation_check.sh <build dir of test/jit>
# Each mutation is applied to a copy, built, and must make the test fail
# (or end on its 60 s watchdog). The source is restored at the end.
set -u
B=$1
SRC=$(cd "$(dirname "$0")/../.." && pwd)/src/main/jit/athrill_jit.c
cp "$SRC" "$SRC.orig"
trap 'cp "$SRC.orig" "$SRC"; rm -f "$SRC.orig"; cmake --build "$B" >/dev/null 2>&1' EXIT
RESULT=0
mutate() {	# name, from, to
	cp "$SRC.orig" "$SRC"
	python3 - "$SRC" "$2" "$3" <<'PY'
import sys
p, a, b = sys.argv[1:]
s = open(p).read()
assert a in s, a
open(p, "w").write(s.replace(a, b, 1))
PY
	cmake --build "$B" >/dev/null 2>&1
	if "$B/athrill_jit_test" >/dev/null 2>&1; then
		echo "  [MISSED] $1"
		RESULT=1
	else
		echo "  [caught] $1"
	fi
}
mutate "exit_if not inverted" 'athrill_jit_jump_if(b, (AthrillJitCond)(cond ^ 1U), a, c, stay);' 'athrill_jit_jump_if(b, cond, a, c, stay);'
mutate "32-bit operations done in 64 bit" '		return SLJIT_32;' '		return 0;'
mutate "generation ignored" '	if (slot->generation != generation) {' '	if (0) {'
mutate "signed less as unsigned" '		return SLJIT_SIG_LESS;' '		return SLJIT_LESS;'
# Not listed: SLJIT_MSHL -> SLJIT_SHL. ARM64 and x86-64 mask register shift
# counts in hardware, so on these hosts the two are the same code.
mutate "SUB as ADD" '		return SLJIT_SUB;' '		return SLJIT_ADD;'
mutate "widen without register" '		sljit_emit_op1(b->c, op, SLJIT_R0, 0, s, sw);
		sljit_emit_op1(b->c, SLJIT_MOV, d, dw, SLJIT_R0, 0);' '		sljit_emit_op1(b->c, op, d, dw, s, sw);'
mutate "remainder as quotient" '		result_reg = SLJIT_R1;' '		result_reg = SLJIT_R0;'
mutate "count ignores n" '(sljit_sw)n);' '(sljit_sw)1);'
mutate "call result not kept" '	sljit_emit_op1(b->c, SLJIT_MOV_S32, JIT_REG_RESULT, 0, SLJIT_R0, 0);' ''
exit $RESULT
