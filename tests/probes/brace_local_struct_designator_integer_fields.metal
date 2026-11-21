// EXPECT: valid
// DISASM-MATCH: OpCompositeConstruct %_struct_[0-9]+ %int_7 %uint_3 %true %half_0x1p_0
struct I { int i; uint u; bool b; half h; };
kernel void k(device float* out [[buffer(0)]]) { I v{.u=2u,.i=7,.h=1.0h,.b=true,.u=3u}; out[0]=float(v.i); out[1]=float(v.u); out[2]=v.b ? 1.0f : 0.0f; out[3]=float(v.h); }
