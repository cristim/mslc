// EXPECT: error float4 built from 4 values is not lowered yet
//
// The list form builds a vector from one value per component, which is a
// different operation from a broadcast: float4(a, b, c, 1) is not float4(1).
// test/lighting in the indium corpus needs this on the line
// "return float4(ambientTerm + diffuseTerm + specularTerm, 1);".
//
// The pin is that it is a diagnostic and not a module. This node is where the
// list arrives, so the argument count is already known here, and reporting the
// count tells the reader which shape mslc did take: one value broadcasts, so
// float4(0) works and float4(a, b, c, 1) does not yet. A module that quietly
// broadcast the first value and dropped the rest would read back as a valid
// shader producing the wrong colour.
kernel void construct_from_a_list_not_lowered(device float4 *out [[buffer(0)]],
                                             constant float4 *in [[buffer(1)]],
                                             constant float4 *other [[buffer(2)]],
                                             uint index [[thread_position_in_grid]])
{
    float4 combined = float4(in[index], other[index], 0.0, 1.0);
    out[index] = combined;
}
