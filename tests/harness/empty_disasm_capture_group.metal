// EXPECT: valid
// An optional group that matches nothing must fail the probe: skipping it would shift
// the numbering of every later group, so $1 would silently name a different capture.
// DISASM-CAPTURE: (OpNoSuch)?(OpMemoryModel)
// DISASM-CAPTURE: (OpReturn)
// DISASM-MATCH-CAPTURED: $1[^O]*OpFunctionEnd
kernel void k() {}
