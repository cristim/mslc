// EXPECT: error incompatible depth attribute combination
struct O { float d [[depth(any), point_size]]; float4 c [[color(0)]]; };
fragment O f() { O o; return o; }
