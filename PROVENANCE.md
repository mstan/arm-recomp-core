# Provenance

Initial contents were extracted from `mstan/ndsrecomp`'s
`recompiler/armv4t` directory.

The implementation is project-authored clean-room code for ARM/Thumb decode,
IR, interpretation, and C code emission. Platform runtimes and generated game
code are intentionally outside this repository.

The reusable ALU primitives derive from this core's existing common and GBA
instruction helpers. The portable sign-extension, instruction-operand shift and unsigned arithmetic-shift
forms also retain Matthew Stanley's 3dsrecomp helper contract at
`be2179f592ea730d44f93c53be94dd0e46c94816`. Instruction descriptor tables,
privileged state, memory systems and device behavior remain consumer-owned.
