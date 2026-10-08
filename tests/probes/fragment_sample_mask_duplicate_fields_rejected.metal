// EXPECT: error more than one [[sample_mask]] field
struct O { uint a [[sample_mask]]; uint b [[sample_mask]]; };
fragment O f() { O o; return o; }
