// EXPECT: valid
// DISASM: OpImageSampleExplicitLod
kernel void texture_and_sampler_thread_address_space_accepted(thread texture2d<float> t [[texture(0)]], thread sampler s [[sampler(0)]], device float4* o [[buffer(0)]])
{ o[0] = t.sample(s, float2(0.5)); }
