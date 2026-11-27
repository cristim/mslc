// EXPECT: error lowers a returned struct this source declares and nothing else
//
// Only a struct value is split into Output variables. Apple rejects this source
// too, for the pointer's missing address space.
struct Out { float4 p [[position]]; };

vertex Out* vertex_returning_a_pointer_rejected(device Out* outs [[buffer(0)]])
{
    return outs;
}
