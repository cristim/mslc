// EXPECT: error worked out after parsing
// Apple sizes sizeof(G) inside the struct as the member G above it (8, so h has 8 floats), not
// the file-scope G (20). A field of the struct being read hides the global, so the parser
// refuses instead of folding the global's 20.
constant float G[5] = {1, 2, 3, 4, 5};
struct W { float G[2]; float h[sizeof(G)]; };
kernel void sizeof_name_struct_field_hides_global_rejected(device float *out [[buffer(0)]])
{
    W w;
    out[0] = w.h[0];
}
