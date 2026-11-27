// EXPECT: error is a texture or sampler type
struct O { texture2d<float> d [[depth(any)]]; };
fragment O f() { O o; return o; }
