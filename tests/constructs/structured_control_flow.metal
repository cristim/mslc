// The control flow a Metal function body is written with, which mslc emitted
// as an unstructured graph: a branch naming a merge block that was never
// declared, so spirv-val rejected every one of these with "Selection must be
// structured" or "Loop must be structured". No fixture exercised a branch until
// now, which is why it went unnoticed.
//
// The declarations go between the branch and the first block it joins, which is
// where the specification puts them. Zero is the loop or selection control,
// which is the enumerant for "none of it": a shader has nothing to say about
// unrolling or flat shading.
//
// The loop's increment runs in the block the header declared as the continue
// block and the back edge runs from there, which is what makes the continue
// happen rather than being skipped. A while loop is given the same shape, since
// a back edge has to run through the continue block the header declared.
//
// The locals inside the branches and the loop body are the same case as a local
// at the top level: a function's variables are all at the top of its first
// block, and only the store that initialises one stays where it was written.
struct Composed
{
    float4 positive;
    float4 negative;
    float4 total;
    float4 scale;
};

kernel void branch(device Composed *out [[buffer(0)]],
                   constant float4 *values [[buffer(1)]],
                   uint index [[thread_position_in_grid]])
{
    if (values[0].x > 0.0) {
        float4 taken = values[0] * 2.0;
        out[index].positive = taken;
    } else {
        float4 notTaken = values[1] * 2.0;
        out[index].negative = notTaken;
    }

    float4 accumulated = values[0];
    for (uint i = 0; i < 4; i = i + 1) {
        float4 scaled = accumulated * 2.0;
        accumulated = scaled;
    }
    out[index].total = accumulated;

    uint n = 0;
    while (n < 4) {
        float4 divided = values[0] / 2.0;
        out[index].scale = divided;
        n = n + 1;
    }
}
