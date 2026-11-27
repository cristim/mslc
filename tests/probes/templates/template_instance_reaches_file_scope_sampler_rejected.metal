// EXPECT: error "smp" is not a sampler
// Refs #341: the scan that reserves a file-scope sampler for an entry point follows calls by name, so it
// does not see through an instance to the helper that samples with it (the same gap as one reached only
// through argument-dependent lookup). The sampler is then not bound, which is refused. With a second entry point that names
// the sampler too, the helper is refused by name (template_instance_helper_file_scope_sampler_two_entries_rejected).
#include <metal_stdlib>
using namespace metal;
constexpr sampler smp(coord::normalized, filter::linear);
float4 look(texture2d<float> t, float2 uv) { return t.sample(smp, uv); }
template <typename T> float4 via(texture2d<float> t, T x) { return look(t, float2(x)); }
kernel void k(device float4* out [[buffer(0)]], texture2d<float> t [[texture(0)]]) { out[0] = via(t, 0.5f); }
