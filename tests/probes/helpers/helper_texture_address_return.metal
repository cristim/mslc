// EXPECT: valid
// DISASM-MATCH: OpImageSampleImplicitLod
// DISASM-NOT: OpFunctionCall
//
// A helper returning a thread pointer or reference to a texture or sampler parameter has no value at run time: the
// call is resolved while compiling from its return statement, so the pointer is the caller's descriptor and the
// helper is not emitted. Through a function returning a reference, a pointer to a pointer parameter's pointee, and
// a call inside the return of another. Apple's compiler accepts it.
#include <metal_stdlib>
using namespace metal;
thread texture2d<float>* addressOf(thread texture2d<float>& r) { return &r; }
thread sampler* samplerAddress(thread sampler& r) { return &r; }
thread texture2d<float>& same(thread texture2d<float>* p) { return *p; }
thread texture2d<float>* again(thread texture2d<float>& r) { return addressOf(r); }
fragment float4 f(texture2d<float> t [[texture(0)]], texture2d<float> u [[texture(1)]], sampler s [[sampler(0)]]) {
  thread texture2d<float>* p = addressOf(u);
  thread sampler* q = samplerAddress(s);
  return p->sample(*q, float2(0.5)) + same(&t).sample(s, float2(0.25)) + again(t)->sample(*samplerAddress(s), float2(0.75));
}
