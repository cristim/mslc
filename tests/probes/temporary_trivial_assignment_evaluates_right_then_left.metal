// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %_struct_[0-9]+ %[0-9]+ %int_1.*= OpCompositeConstruct %_struct_[0-9]+ %[0-9]+ %int_0
// DISASM-MATCH: OpFunctionCall.*OpFunctionCall
// DISASM-NO-MATCH: OpFunctionCall.*OpFunctionCall.*OpFunctionCall
struct S{int a;int b;};
int f(thread int& x){ x=x*2+1; return x; }
int g(thread int& x){ x=x*3+2; return x; }
kernel void k(device int* out [[buffer(0)]]) { int k=0; (S{f(k),0}) = S{g(k),1}; out[0]=k; }
