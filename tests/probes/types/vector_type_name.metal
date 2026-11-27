// EXPECT: valid
// DISASM: OpTypeVector %float 3
// DISASM: OpTypeVector %float 4
// DISASM-MATCH: OpDecorate %_runtimearr_v3float ArrayStride 16
// DISASM-MATCH: OpDecorate %_runtimearr_v4float ArrayStride 16
//
// A vector type name: "float3" is "float" and a count, and mslc's parser has a
// branch that splits a count off the end of a type name. That branch used to sit
// behind a test for the whole name being a scalar type, which "float3" is not,
// so it never ran, and every vector type name fell through to "this names a
// type" -- which is how "float4 position = ..." came to be read as an expression
// and reported as one.
//
// Four places have to agree on the spelling, and all four are here: a
// declaration, a cast, a buffer element, and an arithmetic operand. A kernel
// rather than a graphics stage, so that the types are the only thing in it.
//
// The ArrayStride pins are the layout a vector element gets. A float3 is 16
// bytes and 16-aligned where its three floats are 12, and the two widths pin
// that a float4 and a float3 stride the same, which they only do if the count
// and the padding are handled separately.
kernel void vector_type_name(device float3 *vectors [[buffer(0)]],
                             device float3 *narrow [[buffer(1)]],
                             device float4 *wide [[buffer(2)]],
                             constant float4 *in [[buffer(3)]],
                             uint index [[thread_position_in_grid]])
{
    // A declaration, and a cast, which is the same spelling.
    float3 copied = vectors[index];
    float3 rebuilt = float3(copied);

    // A scalar beside a vector. This is a broadcast and not a conversion: the
    // scalar is put in every component. Converting it to the vector's type
    // instead is an OpFConvert into a vector, which no convert opcode accepts,
    // and it is what mslc emitted until this.
    narrow[index] = rebuilt * 2.0;

    // A vector of the other width, so both the stride and the declaration
    // dispatch are covered.
    wide[index] = in[index] * 2.0;
}
