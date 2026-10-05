// EXPECT: error typedef redefinition with different types
//
// packed_float3 and float3 are different types in Metal, so a name cannot be a
// typedef of both.
typedef packed_float3 Position;
typedef float3 Position;

kernel void packed_typedef_redefined_as_unpacked_rejected(device float *out [[buffer(0)]])
{
}
