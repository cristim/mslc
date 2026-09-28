// A sampler passed as a parameter, which is how a Metal shader receives one
// rather than declaring a sampler state in its own source.
//
// A sampler is a descriptor and not a value, so the parameter becomes an
// OpVariable in the UniformConstant storage class pointing at SPIR-V's sampler
// type. It goes in a descriptor set of its own: [[texture(0)]] and
// [[sampler(0)]] are both index 0 in Metal, since a texture and the sampler that
// reads it are indexed together, while a Vulkan set has one binding per index.
// spirv-val does not notice two variables claiming the same set and binding, so
// putting a sampler beside its texture would be a module that validates and a
// pipeline that does not behave.
//
// The sample is in a fragment stage because a read is implicit-LOD, and a
// compute stage has no derivatives to choose a level with.
fragment float4 sampler_parameter(texture2d<float> image [[texture(0)]],
                                  sampler imageSampler [[sampler(0)]])
{
    // A coordinate rather than a constant, so the sample is a read at a place the
    // shader computed rather than a fold.
    float2 uv = float2(0.375, 0.625);
    return image.sample(imageSampler, uv);
}
