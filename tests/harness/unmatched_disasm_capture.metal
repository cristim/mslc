// EXPECT: valid
// A capture reference no CAPTURE line produced would substitute nothing and match
// loosely, so the runner must reject the probe.
// DISASM-CAPTURE: (%[_0-9a-zA-Z]+) = OpTypeVoid
// DISASM-MATCH-CAPTURED: OpTypeFunction $2
kernel void k() {}
