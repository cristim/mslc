// EXPECT: valid
// The same in the other direction, where a wider value is truncated, and for
// floats, where OpFConvert truncates or denormalizes instead.
// DISASM: OpFConvert
// DISASM-NOT: OpBitcast
kernel void convert_float_width(device const float* f [[buffer(0)]],
                               device const half* h [[buffer(1)]],
                               device float* out [[buffer(2)]],
                               uint i [[thread_position_in_grid]])
{
    half narrow = half(f[i]);
    out[i] = float(narrow) + float(h[i]);
}
