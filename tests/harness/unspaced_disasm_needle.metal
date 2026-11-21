// EXPECT: valid
// Unspaced needle: checks nothing, so the runner must reject it.
// DISASM-NOT:OpEntryPoint
kernel void k() {}
