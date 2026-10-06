// EXPECT: valid
// DISASM: OpConvertSToF
namespace N { typedef float F; }
kernel void cast_to_a_namespace_qualified_type(device float* out [[buffer(0)]], device int* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    int x = in[i];
    out[i] = (N::F)x + ((metal::float3)1.0f).x + (::N::F)x;
}
