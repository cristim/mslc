// A matrix times a vector, the product every modelViewProjectionMatrix exists
// for. The operand order is what picks the operation: SPIR-V has a separate
// instruction for a matrix on the left and for a vector on the left, because
// the two products are not the same one, so "v * m" is not "m * v" here.
//
// The struct puts a float4 before the matrix on purpose. A MatrixStride on a
// member that starts at a non-zero offset is what the number depends on, and
// a member at offset 0 would hide a wrong stride behind the right answer.
struct Uniforms
{
    float4 tint;
    float4x4 modelViewProjection;
    float4 position;
};

// The product of two matrices is a matrix, so it goes back out through a
// struct. An array of matrices would need an ArrayStride computed for a matrix
// element, which is a separate thing from a member's MatrixStride and is not
// something mslc lays out yet.
struct Composed
{
    float4x4 product;
    float scale;
};

kernel void project(device Composed *composed [[buffer(0)]],
                    device float4 *out [[buffer(1)]],
                    constant Uniforms &uniforms [[buffer(2)]],
                    uint index [[thread_position_in_grid]])
{
    out[index * 2] = uniforms.modelViewProjection * uniforms.position;
    out[index * 2 + 1] = uniforms.position * uniforms.modelViewProjection;
    composed[index].product = uniforms.modelViewProjection * uniforms.modelViewProjection;
}

// A matrix rebuilt from its columns, which is the constructor form of the same
// value: a matrix is built one column at a time, not one scalar at a time, so
// "float4x4(c0, c1, c2, c3)" is the whole of it.
kernel void rebuild(device Composed *out [[buffer(0)]],
                    constant Uniforms &uniforms [[buffer(1)]],
                    uint index [[thread_position_in_grid]])
{
    out[index].product = float4x4(uniforms.modelViewProjection[0],
        uniforms.modelViewProjection[1],
        uniforms.modelViewProjection[2],
        uniforms.modelViewProjection[3]);
    out[index].scale = 1.0;
}
