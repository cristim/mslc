// EXPECT: valid
// DISASM-MATCH: OpFunctionParameter %_ptr_UniformConstant_[0-9]+
//
// A thread reference to a texture or sampler names the caller's descriptor, as a by-value texture parameter does,
// and lowers the same way. Once refused ("a reference to texture2d<float> is not valid"); Apple's compiler accepts it.
#include <metal_stdlib>
using namespace metal;
float4 look(thread texture2d<float>& t, thread sampler& s) { return t.sample(s, float2(0.25)); }
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return look(t, s); }
