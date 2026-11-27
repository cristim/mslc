// EXPECT: valid
// DISASM-MATCH: OpImageSampleImplicitLod
//
// A thread pointer local to a texture or sampler is another name for the descriptor whose address initialises it,
// as a texture local is: p->sample, (*p).sample, *q as the sampler and p->get_width(). Once refused ("a local texture
// "b" that is a pointer"); Apple's compiler accepts it.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]], constant int& c [[buffer(0)]]) {
  thread texture2d<float>* b = &t0;
  thread sampler* q = &s1;
  return b->sample(*q, float2(0.5)) + (*b).sample(s0, float2(0.25)) + float4(b->get_width());
}
