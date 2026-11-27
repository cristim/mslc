// EXPECT: valid
// OpConvertUToF zero-extends, so every negative int became a large positive
// float. A signed source needs OpConvertSToF, both through the explicit cast
// and through the implicit conversion at the store.
// DISASM: OpConvertSToF
// DISASM-NOT: OpConvertUToF
kernel void int_to_float(device const int* in [[buffer(0)]],
                         device const int* other [[buffer(1)]],
                         device float* out [[buffer(2)]],
                         uint i [[thread_position_in_grid]])
{
    out[i] = float(in[i]);
    out[i] = out[i] + other[i];
}
