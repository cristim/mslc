// EXPECT: error a struct converts to another form of itself only
//
// An int literal is not a struct to return. Apple rejects this source.
struct Out { float4 p [[position]]; };

vertex Out returned_int_literal_rejected() { return 0; }
