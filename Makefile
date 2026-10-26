# Team convention checks. Run from the repository root.
#   make format   rewrite the C files in the team style (.clang-format)
#   make lint     report style and naming violations; fail (nonzero exit) if any
#   make fast     run the whole vm test suite in parallel (devcontainer)
# The editor shows the same results live through clangd.
# Pintos tests are upstream files; `make check` in pintos/vm runs them.
# pcheck and ptest (week08) are gone: they wrote to the week08 team plan page.

# The file lists and output formatting live in scripts/convention.py.
format:
	@python3 scripts/convention.py format

lint:
	@python3 scripts/convention.py lint

# fast: one QEMU per logical CPU of this machine (nproc). -icount makes guest
# time follow executed instructions, not host load, so parallel runs keep
# timing tests stable. The final check is still a plain `make check`.
# Old results are deleted first: `make check` skips a test that has one.
#   make fast JOBS=4 SHIFT=6 TIMEOUT=60   override any default
JOBS    ?= $(shell nproc)
SHIFT   ?= 5
TIMEOUT ?= 20

fast:
	find pintos/vm/build/tests \( -name '*.output' -o -name '*.errors' \
	  -o -name '*.result' \) -delete 2>/dev/null || true
	cd pintos/vm && PATH="$(CURDIR)/pintos/utils:$$PATH" \
	  PINTOS_QEMU_EXTRA="-icount shift=$(SHIFT),sleep=off" \
	  $(MAKE) -s -j$(JOBS) check TIMEOUT=$(TIMEOUT)

.PHONY: format lint fast
