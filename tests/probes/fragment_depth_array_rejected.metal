// EXPECT: error has [[depth]], which has to be a scalar float
struct O { float d[2] [[depth(any)]]; };
fragment O f() { O o; return o; }
