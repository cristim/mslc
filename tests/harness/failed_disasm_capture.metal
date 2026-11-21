// EXPECT: valid
// A CAPTURE regex that matches nothing must fail the probe, not capture nothing.
// DISASM-CAPTURE: (%[0-9]+) = OpNoSuchInstruction
kernel void k() {}
