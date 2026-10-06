// EXPECT: error cannot store to "L", a constant declared at file scope
//
// Apple: cannot assign to variable 'L' with const-qualified type 'const constant S'.
struct S {
    float x;
    float y;
};

constant S L = { .x = 1.0, .y = 2.0 };

kernel void file_scope_constant_member_store_rejected(device float *out [[buffer(0)]])
{
    L.x = 2.0;
    out[0] = L.x;
}
