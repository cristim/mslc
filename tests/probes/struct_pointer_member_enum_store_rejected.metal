// EXPECT: error the target of the assignment has the enum type E
// An element of a pointer member to an enum has that enum type, which an integer does not convert to.
enum E { A = 2 };
struct S { device E *e; };
kernel void struct_pointer_member_enum_store_rejected(constant S &s [[buffer(0)]])
{ s.e[0] = 7; }
