// A write to a local variable, which is what a loop's counter and any value a
// shader accumulates are.
//
// The target of an assignment is a place, and the place of a plain name is the
// variable itself. mslc emitted the target through the expression path, which
// loads a name and hands back its value rather than its address, so storing
// through what came back had nothing to store through and the assignment was
// reported as "cannot determine what \"x\" points at". An index and a member
// each have their own address path, and both are reached through addressOf now
// rather than through a chain of kinds here, so a name, a component and an
// element of a buffer are one case rather than three.
//
// The increment a loop writes is a write to a local, so the counter is updated
// by an assignment: mslc recognises ++ and -- as unary operators but does not
// lower them, and no shader in the corpus uses them.
struct Composed
{
    float4 accumulated;
    float4 last;
    float total;
    uint count;
};

kernel void assign(device Composed *out [[buffer(0)]],
                   constant float4 *values [[buffer(1)]],
                   uint index [[thread_position_in_grid]])
{
    float4 accumulated = values[0];

    accumulated = accumulated * 2.0;
    out[index].accumulated = accumulated;
    out[index].last = accumulated;

    float total = 0;
    total = total + accumulated.x;
    out[index].total = total;

    uint count = 0;
    count = count + 1;
    out[index].count = count;
}
