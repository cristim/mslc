// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// DISASM-MATCH: OpCompositeConstruct %_struct_[0-9]+ %[0-9]+ %float_1
struct Emp {}; struct OEm { Emp e; float c; };
kernel void k(device float* out [[buffer(0)]]) { OEm o{{},1.0f}; out[0]=o.c; }
