// EXPECT: error a struct converts to another form of itself only
//
// A scalar is not a struct to return. Apple rejects this source.
struct Out { float4 p [[position]]; };

vertex Out returned_scalar_rejected() { return 1.0f; }
