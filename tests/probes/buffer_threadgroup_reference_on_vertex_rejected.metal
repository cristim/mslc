// EXPECT: error parameter "p" needs a device or constant address space to be a buffer binding, not threadgroup
//
// A threadgroup pointer is a kernel argument; a vertex function takes none.
struct Foo { float a; };
struct Out { float4 position [[position]]; };
vertex Out buffer_threadgroup_reference_on_vertex_rejected(threadgroup Foo& p)
{ Out o; o.position = float4(p.a); return o; }
