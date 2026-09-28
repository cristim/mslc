// More than one local in a function, which is what any shader with two
// statements in it has.
//
// A function's variables are only valid among the first instructions of its
// first block, and a variable is discovered at its declaration, which is after
// the instructions computing its initialiser. Two declarations in a row
// therefore put the second variable behind the first one's initialiser, which
// spirv-val rejects as "All OpVariable instructions in a function must be the
// first instructions in the first block".
//
// Every initialiser here reads a float, so the declarations exercise the
// placement and not a conversion. A local declared inside a body is the same
// case from further in, and is covered by structured_control_flow.metal.
struct Composed
{
    float4 doubled;
    float4 halved;
    float4 sum;
    float scale;
};

kernel void locals(device Composed *out [[buffer(0)]],
                   constant float4 *values [[buffer(1)]],
                   uint index [[thread_position_in_grid]])
{
    float4 doubled = values[0] * 2.0;
    float4 halved = values[1] / 2.0;
    float scale = doubled.x + halved.y;

    out[index].doubled = doubled;
    out[index].halved = halved;
    out[index].sum = doubled + halved;
    out[index].scale = scale;
}
