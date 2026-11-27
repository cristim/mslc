// EXPECT: valid
// DISASM: OpTypeStruct %float
// explicit, constexpr and inline do not change an empty constructor.
struct S { float x; explicit constexpr S() {} };
struct T { float y; inline T() {} };
kernel void struct_explicit_constexpr_empty_constructor(device float *out [[buffer(0)]], device S *s [[buffer(1)]], device T *t [[buffer(2)]]) { out[0] = s[0].x + t[0].y; }
