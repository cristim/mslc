// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpFunctionCall %int %[0-9]+ %[0-9]+[^=]*(%[0-9]+) = OpFunctionCall %int %[0-9]+ %[0-9]+
// DISASM-MATCH-CAPTURED: = OpCompositeConstruct %_struct_[0-9]+ $1 $2
// DISASM-MATCH: OpBranchConditional.*= OpFunctionCall %int %[0-9]+ %[0-9]+.*= OpFunctionCall %int %[0-9]+ %[0-9]+.*OpBranch %[0-9]+.*= OpFunctionCall %int %[0-9]+ %[0-9]+.*= OpCompositeConstruct %_struct_[0-9]+ %[0-9]+ %int_7.*OpPhi
// DISASM-NO-MATCH: OpFunctionCall.*OpFunctionCall.*OpFunctionCall.*OpFunctionCall
struct S{int a;int b;};
int f(thread int& x){ x=x*2+1; return x; }
kernel void k(device int* out [[buffer(0)]], constant int& nn [[buffer(1)]]) { bool c = nn > 0; int k=0; int r=(c ? S{f(k),f(k)} : S{f(k),7}).b; out[1]=r+k*100; }
