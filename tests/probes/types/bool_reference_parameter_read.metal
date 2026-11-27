// EXPECT: valid
// DISASM-MATCH: OpLoad %uchar %[A-Za-z0-9_]+ Aligned 1
// DISASM-NOT: OpLoad %bool
// A bool is a byte in a buffer, so a reference to one reads that byte and maps it
// to a bool, as a bool pointer does; a bool-typed load would read the wrong width.
kernel void bool_reference_parameter_read(device float *o [[buffer(0)]], constant bool &b [[buffer(1)]]) {
  o[0] = b ? 1.0 : 0.0;
}
