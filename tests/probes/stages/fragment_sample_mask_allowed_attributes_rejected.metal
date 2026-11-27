// EXPECT: error [[depth(mode)]], [[sample_mask]], [[point_size]], [[user(name)]]
struct O { uint m [[vertex_id]]; };
fragment O f() { O o; return o; }
