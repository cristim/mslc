// EXPECT: error abs of an unsigned integer is not lowered yet
// GLSL.std.450 has FAbs and SAbs and nothing for an unsigned operand.
kernel void math_abs_of_unsigned_rejected(
    device const uint* u [[buffer(0)]],
    device uint* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = abs(u[i]);
}
