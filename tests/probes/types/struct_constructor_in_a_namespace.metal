// EXPECT: valid
// DISASM: OpFunctionCall
namespace n { struct S { float x; S(float a) : x(a) {} float twice() const { return x * 2.0; } }; }
kernel void k(device float *out [[buffer(0)]]) { n::S s(4.0); out[0] = s.twice(); }
