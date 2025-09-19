# ---- Basics ----
# set pagination off
# set confirm off
set print pretty on
set disassemble-next-line on
set target-async on
set remotetimeout 120

set architecture riscv:rv32

# ---- Helpers ----
define oc
  target extended-remote localhost:3333
  monitor reset halt
  echo Connected and halted.\n
end

define restart
  # Set program counter to _start and continue
  set $pc = _start
  continue
end

echo \n[GDB] Use: oc \n
echo Available Shortcuts:\n

\n\n