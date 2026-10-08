// EXPECT: error [[depth(mode)]], [[sample_mask]], [[user(name)]]
struct O { uint m [[vertex_id]]; };
fragment O f() { O o; return o; }
