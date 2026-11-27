// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpFunctionCall %int %[0-9]+ %[0-9]+[^=]*(%[0-9]+) = OpFunctionCall %int %[0-9]+ %[0-9]+
// DISASM-MATCH-CAPTURED: = OpCompositeConstruct %_struct_[0-9]+ $1 $2
// DISASM-NOT: OpCompositeExtract
// DISASM-NO-MATCH: OpFunctionCall.*OpFunctionCall.*OpFunctionCall
struct I{int x;int y;};
int f(thread int& x){ x=x*2+1; return x; }
kernel void k(device int* out [[buffer(0)]]) { int k=0; I b{I{f(k),f(k)}}; out[0]=b.y+k; }
