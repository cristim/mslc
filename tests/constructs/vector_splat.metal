// A vector built from one scalar, which Metal spells as a constructor and mslc
// reads as a cast: "float4(0.5)" puts 0.5 in all four components.
//
// This was a bitcast before, which is a reinterpretation of the same bits and
// so has to be the same width as its result. A float into a float4 is four
// times the width, and spirv-val rejects it: a module that reads every
// component of a splat as four times the value it was given does not reach the
// validator at all, so this fixture is the guard as much as the feature.
struct Payload
{
    float4 scaled;
    float3 offset;
};

kernel void splat(device Payload *out [[buffer(0)]],
                  constant float4 *in [[buffer(1)]],
                  uint index [[thread_position_in_grid]])
{
    out[index].scaled = float4(0.5);
    out[index].offset = float3(-1.0) + in[index].xyz;
}
