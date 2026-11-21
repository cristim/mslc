// EXPECT: error negative value
// -1 < sizeof(int) converts -1 to a huge unsigned value and is false, so Apple's length is 1;
// a signed evaluation would say true and give 2. mslc refuses a negative beside a sizeof.
kernel void sizeof_negative_operand_rejected(device float *out [[buffer(0)]])
{
    float a[(-1 < sizeof(int)) + 1];
    out[0] = a[0];
}
