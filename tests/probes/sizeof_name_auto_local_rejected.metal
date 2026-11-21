// EXPECT: error worked out after parsing
// Apple gives 8 (the pointer an auto deduces). The parser records no type for an auto, so an
// array length from its size is refused, and the file-scope q (20) it hides is not sized instead.
constant float q[5] = {1, 2, 3, 4, 5};
kernel void sizeof_name_auto_local_rejected(device float *out [[buffer(0)]])
{
    float table[6];
    auto q = table;
    float a[sizeof(q)];
    out[0] = a[0] + q[0];
}
