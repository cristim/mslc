// A matrix type, which MSL spells as one name and SPIR-V makes a type of its
// own: "float4x4" is four columns of four rows, not a float8. The fixture is
// about the type and the layout, so the matrix is only copied: reading a
// column, multiplying by a vector and building one are their own constructs.
struct Uniforms
{
    // A matrix in a uniform buffer, which is where a MatrixStride is required:
    // the stride between its columns is what tells the runtime how to step
    // through it.
    float4x4 modelViewProjection;
    float scale;
};

struct Written
{
    // The same matrix as a member of a struct in a storage buffer, so the
    // decoration is required on this form too rather than only the first.
    float4x4 modelViewProjection;
    float scale;
};

kernel void copy_matrix(device Written *written [[buffer(0)]],
                        constant Uniforms &uniforms [[buffer(1)]])
{
    written[0].modelViewProjection = uniforms.modelViewProjection;
    written[0].scale = uniforms.scale;
}
