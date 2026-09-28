// Reading a matrix: a column, a component of a column, and a swizzle of a
// column. SPIR-V has no pointer to a matrix's column, so all three take the
// element out of the value rather than chaining to an address, which is why
// this is one construct rather than three.
//
// The two-step forms are the ones a real shader writes: a normal matrix's
// basis is read as a column, and a texcoord or normal is a swizzle of one.
struct Uniforms
{
    float4x4 modelViewProjection;
    float4 pick;
};

kernel void read_matrix(device float4 *columns [[buffer(0)]],
                        device float *scalars [[buffer(1)]],
                        device float2 *swizzles [[buffer(2)]],
                        constant Uniforms &uniforms [[buffer(3)]],
                        uint index [[thread_position_in_grid]])
{
    columns[index] = uniforms.modelViewProjection[0];
    columns[index + 1] = uniforms.modelViewProjection[3];
    scalars[index] = uniforms.modelViewProjection[0][1];
    scalars[index + 1] = uniforms.modelViewProjection[2].x;
    swizzles[index] = uniforms.modelViewProjection[1].xy;
    swizzles[index + 1] = uniforms.modelViewProjection[1].zw;
}
