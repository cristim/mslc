// Metal's packed vector types, packed_float4 and packed_float2, where no block
// layout is involved.
//
// A packed vector is a vector with no padding, which is what tells it apart from
// a plain one: float4 in Metal occupies four floats, where float3 also occupies
// four. SPIR-V has one vector type either way, so packed_float4 is the same
// v4float as float4 and the two are the same value.
//
// The struct here is a value rather than a buffer's element, which is the point.
// Inside a Block the layout rules apply to the *SPIR-V* type, and a v4float
// aligns to 16 there whatever Metal thought of the packed one: a struct of
// packed_float4, packed_float2, packed_float4 at Metal's own offsets has its
// last member at 24, which straddles a vector under relaxed block layout. That
// is the same conflict the corpus's own texturing fixture stands on, and it is
// Vulkan's to settle rather than mslc's, so this fixture keeps the type under
// test away from it.
struct Attributes
{
    packed_float4 position;
    packed_float2 texCoords;
};

kernel void packed(device float4 *out [[buffer(0)]],
                   constant float4 *values [[buffer(1)]],
                   uint index [[thread_position_in_grid]])
{
    packed_float4 whole = values[0];
    packed_float2 pair = packed_float2(values[1].x, values[1].y);

    // A packed vector reads as a plain one component at a time, since it is one.
    float3 widened = float3(pair, values[1].z);
    float4 assembled = float4(widened, whole.w);

    Attributes a;
    a.position = whole;
    a.texCoords = pair;

    out[index * 2] = a.position;
    out[index * 2 + 1] = float4(assembled.x, a.texCoords.x, a.texCoords.y, whole.w);
}
