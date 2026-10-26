# Team convention checks. Run from the repository root.
#   make format   rewrite the C files in the team style (.clang-format)
#   make lint     report style and naming violations; fail (nonzero exit) if any
#   make fast     run the whole vm test suite in parallel, with -icount (devcontainer)
#   make fcheck   the same, without -icount
# The editor shows the same results live through clangd.
# Pintos tests are upstream files; `make check` in pintos/vm runs them.
# pcheck and ptest (week08) are gone: they wrote to the week08 team plan page.

# The file lists and output formatting live in scripts/convention.py.
format:
	@python3 scripts/convention.py format

lint:
	@python3 scripts/convention.py lint

# fast, fcheck: one QEMU per logical CPU of this machine (nproc).
# fast adds -icount: guest time follows executed instructions, not host load,
# so parallel runs keep timing tests stable. fcheck runs without it; under
# parallel load a timing test may fail now and then.
# The final check is still a plain `make check`.
# Old results are deleted first: `make check` skips a test that has one.
#   make fast JOBS=4 SHIFT=6 TIMEOUT=60   override any default
JOBS    ?= $(shell nproc)
SHIFT   ?= 5
TIMEOUT ?= 20

fast: QEMU_EXTRA = -icount shift=$(SHIFT),sleep=off
fast fcheck:
	find pintos/vm/build/tests \( -name '*.output' -o -name '*.errors' \
	  -o -name '*.result' \) -delete 2>/dev/null || true
	cd pintos/vm && PATH="$(CURDIR)/pintos/utils:$$PATH" \
	  PINTOS_QEMU_EXTRA="$(QEMU_EXTRA)" \
	  $(MAKE) -s -j$(JOBS) check TIMEOUT=$(TIMEOUT)

.PHONY: format lint fast fcheck
