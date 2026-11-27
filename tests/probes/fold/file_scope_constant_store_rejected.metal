// EXPECT: error cannot store to "K", a constant declared at file scope
//
// Apple: cannot assign to variable 'K' with const-qualified type 'const constant float'.
constant float K = 1.0;

kernel void file_scope_constant_store_rejected(device float *out [[buffer(0)]])
{
    K = 2.0;
    out[0] = K;
}
