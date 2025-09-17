# ---- Basics ----
# set pagination off
# set confirm off
set print pretty on
set disassemble-next-line on
set target-async on
set remotetimeout 120

set architecture riscv:rv32

# ---- Helpers ----
define oc-host
  target extended-remote 127.0.0.1:3333
  monitor reset halt
  echo Connected and halted.\n
end

define oc-docker
  target extended-remote host.docker.internal:3333
  monitor reset halt
  echo Connected and halted.\n
end


echo \n[GDB] Use: oc-host | oc-docker \n\n