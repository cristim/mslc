// EXPECT: valid
// DISASM-MATCH: OpStore %[A-Za-z0-9_]+ %[A-Za-z0-9_]+ Aligned 1
kernel void bool_reference_parameter_store(device float *o [[buffer(0)]], device bool &b [[buffer(1)]], constant bool &c [[buffer(2)]]) {
  b = c;
  b = b && !c;
  o[0] = b ? 1.0 : 0.0;
}
