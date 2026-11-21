// EXPECT: valid
// DIFFCHECK: writes-zero
// DISASM-MATCH: = OpCompositeConstruct %_struct_[0-9]+ %int_3[_0-9]* %int_4[_0-9]*.*= OpCompositeConstruct %_struct_[0-9]+ %int_1[_0-9]* %int_2[_0-9]*.*= OpCompositeConstruct %_struct_[0-9]+ %int_3[_0-9]* %int_4[_0-9]*.*= OpCompositeConstruct %_struct_[0-9]+ %int_1[_0-9]* %int_2[_0-9]*.*= OpCompositeConstruct %_struct_[0-9]+ %int_3[_0-9]* %int_4[_0-9]*.*= OpFunctionCall %_struct_[0-9]+
struct S{int a;int b;};
S mk(){ S s{1,1}; return s; }
kernel void k(device int* out [[buffer(0)]]) { (S{1,2}) = S{3,4}; S{1,2} = S{3,4}; mk() = S{3,4}; out[0] = 0; }
