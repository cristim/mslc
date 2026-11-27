// EXPECT: valid
// DISASM-MATCH: OpFunctionParameter %_ptr_UniformConstant_[0-9]+
//
// A helper taking thread pointers to a texture and a sampler is passed the descriptors their addresses name, as a
// texture parameter is, and passes them on: the module is the one the by-value form gives. Apple's compiler accepts
// it.
#include <metal_stdlib>
using namespace metal;
float4 look(thread texture2d<float>* p, thread sampler* q) { return p->sample(*q, float2(0.5)); }
float4 twice(thread texture2d<float>* p, thread sampler& s) { return look(p, &s) * 2; }
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  thread texture2d<float>* p = &t;
  return look(&t, &s) + twice(p, s);
}
