// EXPECT: error worked out after parsing
// In "float x[sizeof(x)]" Apple sizes the x of the enclosing block (a float4, 16), because the
// new x is not in scope until its declarator ends. The parser has the name but no type for it
// yet, so it refuses; a type recorded early would give 4 and one skipped past it the outer 16.
kernel void sizeof_name_declarator_incomplete_rejected(device float *out [[buffer(0)]])
{
    float4 x;
    {
        float x[sizeof(x)];
        out[0] = x[0];
    }
    out[1] = x.x;
}
