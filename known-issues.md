# Known issues

## Vexing parse: unused function-looking declarations are rejected, `v(1.0)` calls are not modelled (#356)

`float3 v(float(a));` declares a function in C++ and in Apple's compiler, so a later `v.x` or
`v = ...` is an error there. mslc used to build a variable and accept the use. It now refuses the
declaration itself when every comma group inside the parentheses is `type(name)` or `type()` with a
name (`T v(U(x))`, `T v(U(x), V(y))`, `S s(S(a))`, with `thread` or `const`): "the most vexing
parse". Trade-off, a decision recorded for the owner to override:

- Apple accepts such a declaration when `v` is never used as a value, or is called as a function
  (`float3 v(float(a)); ... v(1.0)`), or tested in a bool context (`if (v)`, `bool v(bool(a)); o[0] = v ? 1 : 0;`,
  through function-to-pointer conversion). mslc refuses all of them, because it lowers no block-scope
  function declarations. A shader that relies on that is declaring an unused function by mistake.
- Empty parentheses (`float3 v();`) are not matched by this check; they still reach the "has no
  arguments" refusal, which Apple does not make for an unused declaration.
- `for (float3 v(float(a)); ...)` is refused by the for initialiser, as any parenthesised
  initialiser there already was. `for (auto v(float(a)); ...)` is refused as a function declaration;
  `for (auto v(a); ...)` is a variable, in mslc as in Apple.

## Apple fuses a * b + c inside one expression; mslc emits a separate multiply and add (#368)

**Status:** open, deliberate. Found 2026-11-15 while fixing #368.

mslc decorates `OpFMul`/`OpFAdd`/`OpFSub` and the float conversions `NoContraction`, which
forbids a driver from fusing `a * b + c` into an fma. Apple's compiler does fuse within one
expression (`llvm.fmuladd`) even in safe math mode, so a result under cancellation can differ by
a ULP or more. mslc matches the strict IEEE result, which is the only one that is the same on
every Vulkan driver; matching Apple would need an explicit `OpExtInst Fma` at each `a * b + c`
the front end can see (not across statements, where Apple does not fuse). `tools/diffcheck`
compiles the Apple side with `contract(off)` to compare like with like. Not scheduled: no
corpus kernel depends on the last bit.

Also open, tracked in #371:

- indium (`src/indium/indium.cpp`) never queries `VkPhysicalDeviceFloatControlsProperties`. The
  execution modes need `shaderSignedZeroInfNanPreserveFloat32` (VUID 06294) and, for mode 16,
  `shaderSignedZeroInfNanPreserveFloat16` (VUID 06293). Mode 16 is module-wide: every entry point
  gets it once any code declares half.
- Consumer side, not in this repo: `src/indium` does not exist here, so the property query lives in
  darlinghq/indium (the bullet above).

Float operations beyond add/sub/mul (#371), measured with `xcrun metal -fno-fast-math -S
-emit-llvm` (safe math; Apple's default is fast math, where every op below is `fast`). Rule: an
operation is decorated when Apple's safe AIR shows a plain op or call, and left alone when Apple
itself emits `llvm.fmuladd` or the op is exact. `a * b + c` is also `fmuladd` in Apple, so the
#368 decoration on add/mul is deliberately stricter than Apple (see above). Revisiting that
conservative choice is a separate decision from this table.
Open gap: Apple's `length`/`normalize`/`distance` rescale by powers of two before squaring, so
`length(float4(1e30))` is finite there, while a GLSL `Length` that squares first can return inf.


| Operation | Apple AIR | mslc |
|---|---|---|
| `/` | `fdiv`, no flags | `OpFDiv` NoContraction |
| `fmod` | `air.fmod` call, no flags | `OpFRem` NoContraction |
| `dot`, `vector * matrix` | `air.dot` per column | `OpDot`, `OpVectorTimesMatrix` NoContraction |
| `matrix * scalar` | `fmul`, no flags | `OpMatrixTimesScalar` NoContraction |
| `vector * scalar` | `fmul`, no flags | `OpFMul` on a splat, already decorated |
| unary `-` | `fneg`, exact | not decorated: nothing to contract |
| `matrix * vector`, `matrix * matrix` | `llvm.fmuladd` | not decorated: Apple allows fusing |
| `smoothstep` | inlined: `fdiv`, `fmul`, `llvm.fmuladd` | ext inst, not decorated: Apple fuses inside it |
| `mix` | opaque `air.mix` call | ext inst, not decorated |
| `length`, `normalize`, `distance` | inlined: `air.dot` + `air.sqrt`/`rsqrt` with power-of-two rescaling for range | GLSL.std.450 ext insts, not decorated; open gap (below) |

## lavapipe cannot execute any mslc module on this machine (RESOLVED, stale)

**Status:** resolved/stale as of 2026-10-13; kept for history, not current guidance.
Originally found 2026-10-10 while verifying #181.

This entry claimed every mslc-produced compute module failed at
`vkCreateComputePipelines` with `VK_ERROR_FEATURE_NOT_PRESENT`, or segfaulted
inside the driver. That is no longer true on this machine: fragment and
compute/kernel lavapipe readbacks have since run cleanly and produced correct,
mutation-sensitive numeric results for #184 (point_size), #220 (texture3d),
#221 (texture2d_array), and #173 (half fwidth), across several sessions
between 2026-10-10 and 2026-10-13. Whatever the original environment fault
was (driver update, ICD reinstall, or a transient local misconfiguration on
this shared machine, never isolated), it no longer reproduces. Left here
rather than deleted so a future reader who hits a *new* lavapipe failure has
the original evidence and knows this specific failure mode was already seen
and later resolved, not to be treated as still-current advice to skip
readbacks on this host.

Original evidence (2026-10-10):

- The device was found, reported Vulkan API 1.4.354, exposed `dynamicRendering`,
  `maintenance4` and `VK_KHR_synchronization2`, and created instance, device,
  queue, buffer, memory and descriptor set without complaint.
- `spirv-val --target-env vulkan1.3` accepted the same modules.
- It reproduced with `tests/compute_buffer_clear.metal` and with a four-line
  kernel that only stores a literal, so it was not a specific feature mslc
  emitted.
- `lldb` put the fault in `libvulkan_lvp.dylib`'s worker thread
  (`___lldb_unnamed_symbol14182 + 356`), with no mslc or harness frame below it.
- It was unaffected by `spirv-opt --freeze-spec-const`, so
  `OpExecutionModeId ... LocalSizeId` spec constants were not the cause.

**Do** attempt a lavapipe readback on this machine; it works as of 2026-10-13.
If a *new* failure reproduces, record fresh evidence rather than reviving this
entry, since the original cause was never identified.

## Framebuffer fetch ([[color(n)]] input) limits (#118)

mslc lowers a fragment `[[color(n)]]` parameter or stage-in field to a SubpassData input
attachment. What it does not cover:

- The image is always single-sample (MS 0). A consumer with a multisampled colour attachment
  gets an invalid pipeline, where Metal's fetch reads per sample.
- Same-subpass programmable blending, where the fragment reads the attachment it writes, needs
  a GENERAL feedback-loop layout and rasterization-order attachment access when primitives
  overlap. The module is the same; the consumer (indium) must provide both. indium binds no
  input attachment yet.
- `float`, `float2`, `float3`, `short4` and `[[color(n)]]` combined with another attribute
  (`[[color(0), flat]]`) are refused as policy: Apple accepts some of them, so these are not
  Apple's rules.
- A mismatched input and output type at one index is refused with Apple's wording, but only
  against the return type of the same function.

## half `fwidth` lowers via widen-to-float32, Apple parity unmeasured (#173)

`fwidth` on a `half`/`half2`/`half3`/`half4` argument lowers by converting the
operand to `float32`, emitting `OpFwidth` (core SPIR-V requires 32-bit float
components), and converting the result back to `half`. This is proven
internally correct by a lavapipe readback (exact affine-input derivative,
tolerance 0, two compiler mutants both caught), but **Apple's own half-fwidth
rounding policy is unmeasured on this machine** (no Apple toolchain is
reachable here). Apple is known to emit a native `air.fwidth.v2f16` for half,
which may compute the difference/sum in half precision directly rather than
widening to float32 first; the two approaches are not guaranteed to produce
bit-identical results. The project owner was asked directly and chose to land
this lowering with the gap stated rather than wait for hardware access to
measure it. If Apple hardware/toolchain access becomes available, measure
`air.fwidth.v2f16`'s actual output against mslc's widen-compute-narrow output
and update or remove this entry.

## mslc emits Vulkan-valid modules that this lavapipe rejects

`PhysicalStorageBuffer64` addressing with a binding-0 address block is correct
for indium and is validated by `spirv-val`, but a driver may not implement it.
Where a read-back is required for such a module, confirm the driver supports
the addressing model before treating a failure as an mslc defect.

## atomic_compare_exchange_weak_explicit accepts a narrower set than Apple (#249)

mslc lowers the weak form only for the shapes below and refuses the rest with a
diagnostic, so these are documented under-acceptances (valid Metal that mslc
rejects), not miscompiles. `atomic_compare_exchange_strong_explicit` does not
exist in Apple's toolchain and is rejected on purpose.

Expected side (`&expected`): only the address of a plain local scalar of the
atomic's own type (`uint` for `atomic_uint`, `int` for `atomic_int`) is taken.
Not supported: `&e[1]` (array element), `&e.y` (a struct member; a vector
element is an error in Apple too), a `thread` pointer local passed as the
expected argument, a builtin parameter, `&*&e`, and a `memory_order(0)` style
order spelled as anything but `memory_order_relaxed`.

Atomic side: only a device `atomic_uint`/`atomic_int` buffer pointer or element
(`&c[i]`, or the pointer itself), or `&local` of a scalar threadgroup atomic.
Not supported: atomic pointers received as helper parameters, atomics held in
struct members, a `device atomic_uint&` reference, pointer arithmetic on the
atomic pointer, threadgroup atomic arrays, `atomic<uint>`, and an alias
declared with `using A = atomic_uint` (a `typedef` alias is supported).

## Threadgroup memory arguments (#373)

`threadgroup T *p [[threadgroup(n)]]` (or no attribute: the lowest free index) lowers to a Workgroup array
whose length is a specialization constant, and the reflection gets one `ThreadgroupMemory` entry per
argument (`metal_index`, `spec_id`, `element_size`, `param_index`, `name`). The host must set that spec
constant to the element count, `max(1, bytes / element_size)` from the byte length Apple's
`setThreadgroupMemoryLength:atIndex:` would take, when it creates the pipeline. The default is 1 element.
Vulkan fixes the length at pipeline creation and Metal at encode time, so a consumer that changes the
length per dispatch needs a pipeline per length (or a cache keyed by it). `element_size` is Metal's, so
a `float3` element steps 16; a `packed_float3` or `bool` element may take more or fewer bytes in the
Vulkan driver, so a host that checks `maxComputeSharedMemorySize` should do it with some margin.

Refused, though Apple accepts it: a reference (`threadgroup T &`), an atomic or `void` pointee, passing
the argument to a helper function, and pointer arithmetic on it (`p + 1`, `*(p + 1)`). Apple itself rejects a duplicate index, an index above 30, `[[buffer]]` together with
`[[threadgroup]]`, an array-pointer pointee, and the argument in a vertex or fragment function.

## Implicit buffer indices are the lowest free index (#379)

An attribute-less `device`/`constant` buffer parameter takes the lowest `metal_index` that no
`[[buffer(n)]]` in the parameter list claims and no earlier attribute-less buffer took, as Apple does
(read from `air.location_index`; the same rule in kernel, vertex and fragment functions, with
`[[stage_in]]`, textures and samplers numbered apart). Before, it took its position among the
attribute-less buffers, so `(device float *out [[buffer(0)]], device float *in)` reflected `in` at 0
where Apple binds 1. Only the reflection's `metal_index` changes; the SPIR-V, the `member` and the
descriptors do not, because the buffers' members are numbered by parameter order. A consumer that
bound such buffers by the old `metal_index` placed them where Apple code never reads them. A program
needing an index above 30 is refused, as Apple refuses it.

Checked against Apple by name over `tests/**/*.metal` (a capped sweep that stopped early, through
`tests/probes/buffer_implicit_*`; `pp_macro_expansion_bomb_rejected.metal` makes Apple's compiler run
away and is skipped): before the fix the only mismatches were the new `buffer_implicit_*` probes
(8 buffers); after it, none. A byte-compare of reflection and SPIR-V over all `tests/**/*.metal`
against the previous build changed only those probes.

## An anonymous enum's enumerator converts to a named enum (#261)

`typedef enum { TA, TB } Anon; constant Mode km = TB;` (and the same through an
argument, a brace leaf, a conditional or a return) compiles. Apple rejects it:
"cannot initialize a variable of type 'const constant Mode' with an rvalue of
type 'Anon'". `requireEnumValue` lets `"<anonymous>"` through at every target,
because mslc cannot tell an untagged enumerator from the target enum's own one;
closing it needs anonymous-enum identity tracking, which does not exist.
Pinned by `tests/probes/enum_element_argument_and_brace_accepted.metal`.

A file-scope enum constant initialised with a conditional
(`constant Mode km = c ? MA : MB;`) is also refused as "not a value that can be
worked out", where Apple accepts it; the folder has no conditional.

## A float cast to an enum in a file-scope constant is refused when it does not fit (#261)

`constant Mode km = (Mode)1e30f;` (also `(Mode)2147483648.0f`, `(Mode)-2.7f`, NaN
and infinities) is refused: the truncated value is outside the enum's range,
which C++ leaves undefined. mslc takes the range as 0 up to INT_MAX (it records
an int underlying type unless an enumerator is past INT_MAX); Apple's enum
without a negative enumerator is unsigned, so the refusal is slightly wider than
Apple's range for values in [2^31, 2^32) and for enums with negative
enumerators. Apple accepts the out-of-range casts and folds them through LLVM's
undefined-behaviour rules to host- and pass-dependent values (`(Mode)9e18f` is
-1, `(Mode)-2.7f` is 0), not a language rule mslc could match. Truncations that
fit (`1.5f`, `-0.5f`, `1e-40f`) fold as Apple does. Pinned by the
`file_scope_constant_enum_cast_float_*` probes.

## An ignored nested scalar initializer element is emitted, then discarded (open)

**Status:** open, found 2026-11-15 while fixing #164. Nothing regressed: master
rejected every excess form.

`float x{{a, b}}` keeps `a` and Apple never evaluates `b`, but it does type-check
it. mslc type-checks it by emitting it and lifting the instructions out, so:

- An element mslc cannot lower as a value is rejected where Apple accepts it:
  `i++`/`i--`, `i += 1`, an assignment such as `out[1] = 5.0f` ("cannot be used
  as a value"), a void call `g()`, a bare buffer name `in`, `discard_fragment()`.
  `int x{{in[1], i++}}` leaves `i` unchanged in Apple. Pinned by
  `tests/probes/brace_local_scalar_nested_excess_increment_not_lowered.metal`.
- The lift leaves unused OpConstant and OpType declarations, a GLSL.std450
  import, and one unused OpVariable in the call case. A half operation in an
  ignored element adds OpCapability Float16 to a module that does not otherwise
  need it. A helper called only from an ignored element is still emitted as an
  uncalled OpFunction.

## Compound assignment reads its target before the right operand runs (found 2026-11-15, #232)

**Status:** open, pre-existing on master. For `n += (n = 10, 5);` Apple's AIR stores
15 (the right operand runs first, then `n` is read), while mslc loads `n` before the
right operand runs and stores the old `n` plus 5. Master does the same with
`n += g(n)` where `g(thread int &r) { r = 10; return 5; }`. The ordering lives in
`emitAssignment`'s compound-assignment path; the comma operator only opens another way
to reach it. Valid SPIR-V, wrong value, so a fix needs a produced-value read-back.

## Forms Apple accepts that the comma operator and void casts still reject (#232)

**Status:** open, all loud rejections, none a miscompile.

- `c ? f() : g();` with void helpers: `isVoidValued` treats only void casts, commas ending
  in one and conditionals of them as void, not calls.
- `return f();` with `f` void in a void function.
- A comma in a constant context: an array size `[(1, 2)]`, `case (1, 2):`,
  `constant int N = (1, 2);`.
- A comma whose right operand writes, used as a value: `x = (a, b = 3)` and `x = (a, n++)`.
- `(n = 1, s).b` on a struct: rejected with the misleading ".b swizzles a value that is
  not a vector", because `structOf` has no Comma case. Making `structOf` throw is wrong
  (it is called speculatively on any expression), so it needs a real case that returns the
  right operand's struct and lowers the member of a value.
- `(n, t) = 3;` is now rejected as "assigning to a comma expression is not lowered yet"
  (probe `comma_assignment_target_rejected`).

## An overloaded helper's Linkage name depends on the overload set this unit sees (#266)

A helper name with one signature in a unit keeps its plain Linkage name (`scaled`), as
before. A name that has two or more signatures in the unit exports each as
`name(parameter types)`, built from the canonical signature (`mixed(float3)`,
`mixed(float,thread const int&)`). The choice is made per unit, so two units of one link
have to see the same overloads of a name: a library that defines `f(float)` alone exports
`f`, and a caller whose own headers also declare `f(float3)` imports `f(float)`, which
does not meet it. Importing one overload of an overloaded name (declared here, defined in
another unit) is refused for the same reason. Apple's own AIR symbols are Itanium-mangled
for every function, which mslc does not follow.

## A conditional or comma argument that is an lvalue is refused when the call chooses among overloads (#306)

A conditional or comma with a place for an operand (`c ? la : lb`, `c ? la : 2.0f`, `(lw, lv)`) is an
lvalue-like value in Apple and can bind a non-const reference parameter; the overload ranking takes it for a
thread prvalue. Such an argument is refused whenever the call has more than one candidate by arity, qualified
or not (`N::f(c ? la : lb)`, `::f(...)` included). With one candidate it is accepted (by value or through ADL).

## A const member function's receiver aliases a member reference argument (found 2026-11-15, pre-existing)

`struct S { float a; float b; float m(thread float &x) const { x = 7.0f; return a; } };` and `s.m(s.a)` return 7
in Apple and 1 in mslc: the receiver is passed by value, so it is not the object the reference names.

## Helper parameters in the constant address space: what is taken, what is not (#232 f)

A `constant T &c` or `const constant T &c` helper parameter is read-only memory, so it is passed as the
value. Its argument has to be a place in constant memory (a constant buffer parameter or an element or member
of one, a file-scope constant, a constant reference parameter of the caller) of exactly T, with no
conversion; anything else is a diagnostic (Apple refuses a thread or device argument, a literal and a
converted value too). Not lowered, each a loud refusal: a conditional of two constant lvalues as the argument
(`c ? a : b`; Apple takes it as an lvalue), `constant T *` or `constant T *&` parameters, a `device T &`
parameter (still "does not take yet"), a `constant T &&`, and taking `&c` of the parameter. Overload ranking
treats it as an exact match that binds only a constant-space place; the same call with a by-value overload is
ambiguous, as in Apple. The argument is read once at the call, not bound, so a value read through two
aliasing constant references is the same value (nothing can write constant memory).

## Two reference parameters on overlapping places of one variable are refused (#210)

`f(s.a, s.b)`, `f(w.s.a, w.t.a)` and `f(a[0], a[2])` (different members, or different literal indices at
the first step they differ) are lowered: each place is copied in and out on its own. Still refused, though
Apple accepts them, because a pair of copies cannot reproduce a write seen through the other reference: the
same place twice (`f(s.a, s.a)`, `f(a[1], a[1])`), a member with its own struct (`f(s.a, s)`), and an index
that is not a literal (`f(a[0], a[i])`, even `f(a[i].x, a[i].y)`). Apple refuses a reference to a vector
component, element or matrix column element (`f(v.x)`, `f(v[1])`, `f(m[1][0])`, `float &r = v.y;`); mslc
refuses them too, and binds `const thread float &r = v.y;` to a copy, as Apple does. A vector element that
is reached through a call result, a conditional or a cast is not recognised as one.

## Two names for one object in a call that copies elements in and out (found 2026-11-15, #210)

`thread S &r = s; f(r.a, s.a)` and a helper `h(thread S &p, thread S &q) { return f(p.a, q.a); }` called
as `h(s, s)` pass two copies of storage that is the same object, so a write through one is lost or stale
(Apple: both references alias). Pre-existing; the check sees names, not objects.

## A constant-buffer struct argument never matches a constant reference in an overloaded call (#232 f)

`h(const constant A &)` against `h(const thread A &)` or `h(B)`, called with `ca[0]` of a `constant A *`, is
refused as "no matching function" (the buffer layout type is not the parameter's type), where Apple picks
the first, or `h(B)`. A loud over-rejection.

## Constant references: what is not modelled (#232 f, review follow-up)

A vector component or element of a constant vector chooses among overloads with a `const constant float &`
candidate: Apple finds the candidate viable and errors if it wins, mslc refuses the call. `const device float
&r = b[0].x;` and the constant equivalent are refused (the copy is thread memory; Apple refuses the mismatch).

## Overload sets, using declarations and argument-dependent lookup: what is not followed (#266)

- A using declaration names the overloads declared above it, and a later one is not found
  through it (Apple). mslc refuses a new overload declared after a using declaration of its
  name instead of modelling the cut-off.
- Argument-dependent lookup adds the same-named functions of the namespace of each struct or enum
  argument, found from the argument's type when it is a name, a member, an element, a call result,
  a comma or a conditional whose arms agree (#306; an assignment or a cast to a struct is not lowered as an argument at all, and `enumOf` has no case for an assignment). Not followed: the namespace of
  the pointee of a pointer argument (`g(&s)`; passing `&x` to a helper is refused on its own, so
  nothing valid is lost today), and a friend function defined in a struct (not parsed). The
  enclosing namespace of an inline one, and the inline members of a namespace, are followed. A
  struct the emitter cannot name before evaluating adds nothing.
  The passes that run before emission (the import scan, the reservation of file-scope samplers) read a
  call's names from the source and do not see what lookup adds: a function declared and defined
  elsewhere that only argument-dependent lookup reaches is refused, and a file-scope sampler
  named by a function only it reaches is not counted against its entry point.
  A file-scope sampler a function names, or one it reaches through a call, makes that function
  emitted once per entry point that reaches it (#167); the call from an instance member is
  followed by name, since the receiver's struct is known only when the call is emitted, so a
  same-named member of another struct that names a sampler reserves it too: listed, unused.
  A function that reaches one of those only through argument-dependent lookup is refused.
- A helper no entry point reaches is not checked for a call to an overloaded name (arity, no
  matching function, declared above); resolution happens where the call is emitted.
- Beside other overloads, an element or member of a local that a mutable reference parameter could
  bind is refused unless it is a local name or an element of a local array of scalars (a vector
  component cannot be bound, and Apple errors on it after choosing the reference). A device or
  constant lvalue is classified (it never binds a thread reference).
- An import of an overloaded name is refused, but the plain Linkage name of a function with one
  signature does not tell `f(E)` from `f(int)`: an import of one links to an export of the other.

## String literals, char pointers and pointer-to-bool are rejected on purpose (#156)

Apple's compiler accepts `const constant char *p = "abc"`, `constant char *p = "x"`,
`bool b = "x"`, `if ("x")`, `sizeof("abc")`, `"abc"[1]`, `"a" "b"`, `u8"abc"`, `R"(abc)"`, `L"x"` as a `bool`, in `sizeof` or as a `wchar_t` pointer
(only `const constant char *s = L"x"` is rejected there), `static_assert(1, "msg")`,
and accepts a kernel buffer pointer in a bool context (`bool b = p`, `if (p)`, `!p`; measured
with `xcrun -sdk macosx metal`). mslc rejects all of them, deliberately. The decision is a
rejection with a clear diagnostic ("a string literal is not lowered; mslc has no string type,
so char pointers, char arrays and bool-from-pointer are not lowered"; "a prefixed string
literal ..."; "the buffer ... is used as a value (a comparison, a conversion to bool or a copy); mslc does not lower pointer values yet"), not an accident.

Why: no corpus shader uses a string literal; a char pointer to one has no SPIR-V
representation without a constant char array plus a pointer into it; a pointer in a bool
context would need a null test, and the Logical and PhysicalStorageBuffer addressing mslc
emits give a pointer no meaningful null.

What accepting them would need: a constant char array lowering (a Private or UniformConstant
variable with an initializer), a pointer type for `const constant char *` as a value, array
decay and indexing of a literal, adjacent literal concatenation and `sizeof` folding in the
parser, prefix and raw-string lexing, and an explicit pointer-to-bool rule for buffer
pointers (a constant true, or an address compare against zero in PhysicalStorageBuffer64).
Related and separate: a helper function taking a `constant char *` parameter is refused by
the helper pointer-parameter limit, not by this one.

Also recorded under this entry, all measured against Apple and all pre-existing:

- `static_assert(1, "msg")` inside a function is rejected with the string-literal wording above,
  because mslc has no `static_assert` and reads it as a call with a string argument.
- Raw strings Apple accepts (`R"d(a;b)" c)d"` and a multi-line `R"(...)"`) still fail in the lexer
  with "newline in a string literal at offset N", so the prefixed-literal message never fires
  for them; it fires only for a single-line prefixed literal that the lexer keeps whole.
- Character literals (`'a'`, `u8'a'`, `L'a'`) are not lexed at all ("unexpected character"),
  although Apple accepts them.
- A prefix separated by a space (`L "x"`) is an identifier and a literal, and keeps the
  "expected ; after a declaration" error (Apple rejects it too).
- The buffer-as-value message ("the buffer ... is used as a value") covers every use of a
  kernel buffer pointer as a value: a comparison, a conversion to bool, a copy. Apple accepts
  comparisons and bool tests and rejects `float x = p`; mslc rejects all, with one wording.

## Brace elision: forms Apple accepts that mslc refuses (#161)

Refusals, not miscompiles; each fails with a diagnostic.

- A designator over a struct member with a bare value, `Outer o{.p = 1.0f, .c = 3.0f}`: Apple elides
  under the designator (p.a = 1, p.b = 0); mslc reports "a struct converts to another form of itself only".
- A member with a user constructor, `struct OC { Ctor k; float c; }; OC o{1.0f, 2.0f};`: Apple calls
  `Ctor(float)`; mslc rejects brace initialisation involving declared constructors.
- A member struct with an array member reached by elision is rejected as not lowered (arrays: #131).

## [[fallthrough]] placement: two shapes still differ from Apple (#355)

The placement check follows a marker through nested blocks, `if` branches and nested
switches (see `endsWithFallthrough` in `src/parser.cpp`). Two shapes still differ from
Apple, both the same as before that change:

- A marker before the first case label, inside a loop (`switch (i) { for (...) { [[fallthrough]]; } case 9: ... }`):
  Apple rejects, mslc accepts. Statements in a switch preamble are not checked at all.
- A marker after something that always jumps, but is not a plain `return`/`break`/`continue`
  (`if (c) { return; } else { return; } [[fallthrough]]; x = 1u;`, or `while (true) { ... } [[fallthrough]]; x = 1u;`):
  Apple accepts because the marker is unreachable, mslc rejects. Only a direct `return`,
  `break` or `continue` earlier in the same block makes a marker unreachable here.

Also unchanged from before #355, found by comparing 224 generated switch programs with Apple:

- Shapes where a marker is made unreachable inside a nested block or an `if` and a statement
  follows the block: `case 0: if (c) { break; [[fallthrough]]; } x = 1u;`,
  `return; if (c) { [[fallthrough]]; } x = 1u;`, `return; [[fallthrough]]; if (c) { [[fallthrough]]; }`.
  Apple rejects, mslc accepts.
- `return; switch (y) { case 1: x = 1u; [[fallthrough]]; }` before an outer label (the inner
  switch's marker after a jump): Apple accepts, mslc rejects.

## `using precise::sqrt;` beside `using namespace metal;` is accepted (Apple rejects the call as ambiguous)

`using namespace metal; using precise::sqrt;` followed by a call `sqrt(x)` is rejected by Apple
(`metal::sqrt` and `metal::precise::sqrt` are both visible), but mslc accepts it: the using
declaration records the bare name `sqrt` as the library's, and mslc does not model the library's
overload sets across `metal`, `metal::precise` and `metal::fast`. Only invalid source is accepted
(both spellings lower to the same GLSL `Sqrt`), so no valid program changes value. Found in review
of the #354 fix.

## Constant-expression narrowing: over-accepts and gaps (#160)

Apple rejects these and mslc accepts them. Both predate the #160 change (the reviewer found
them on master as well), so neither is a regression.

- `const int k = 3; const thread int& r = k; float x{r};` is accepted; Apple does not treat a
  reference to a const as a constant, so it rejects the narrowing.
- Narrowing in dead code after `return;` is not checked: `return; constexpr int n = 300; char x{n};`
  is accepted, Apple rejects it.

Forms Apple accepts that the validity proof still leaves as the unsupported-proof diagnostic:
char literals such as `'a'` in a constant (parser limit), arithmetic on a bool
(`float x{true + 1}`; every arithmetic or bitwise operator on a bool is refused as "not lowered yet", so
this is a lowering gap and not a proof gap), a float-to-integer cast inside a vector constructor of a constexpr list
(`constexpr int3 v{int3(1.5f)}`), and a file-scope `constant` initialised with a float division by
zero that is then narrowed to half. A file-scope `constant int g = true ? 1 : 5 / 0;` (an unchosen arm that
does not fold, outside a function) is refused with "not known outside a function"; Apple accepts it.
A condition that is a constant but not provable (`constant float c = 1.0f / 0.0f; char z{c ? 1 : 300};`) is
refused as an unsupported proof; Apple accepts it.

## Nested field designators are rejected by the parser (#163)

Apple accepts `O o{.p.a = 1.0f, .c = 2.0f};` (a designator that names a member of a
member). mslc stops in the parser with "expected = after a field name". This is a
separate parser limit; the duplicate and reordered single-level designators of #163 are
supported. Dropped or type-probed initialisers no longer leave dead function locals.

## S{...} temporary forms still refused (#251)

Apple accepts the first five, which are refused loudly. It also rejects the last two, so for those only the wording differs.

- `using T = S; T{1,2}`: every alias declaration is rejected ("a using declaration has to name a
  member of a namespace"). It needs the parser's using and lookup code; `typedef S T; T{1,2}` works,
  at statement start too.
- A struct with an array member is brace-initialised with the array in its own braces (`A x{{1,2},3}`, designators
  included). Still refused where Apple accepts: brace elision into the array member (`A x{1,2,3}`), a
  struct member that is an array of structs ("an array of structs field in value struct is not lowered yet"),
  indexing a temporary (`S{{1,2},3}.a[1]`), a braced return from an entry point (a helper's `return {1,2};` works, #362), and the
  parenthesised copy `A z(A{{1},2})` ("constructing a A is not lowered yet", as for any struct).
- File-scope `constant` structs with a positional or array-member initialiser (a plain `constant P g = {1, 4};` gets the same "initialised by field name" error; designators on a plain struct work), which Apple accepts: `constant Q g =
  {{1,2,3},4};` ("initialised by field name, so a has to be named"), `constant Q g{{1,2,3},4};` ("needs an
  initialiser, found {") and `constant Q g = {.a = {1,2,3}, .s = 4};` ("a constant of \"\" is not a struct this
  source declares").
- Assigning to a conditional or comma that yields a place, including `(c ? s : t) = S{3,4}`,
  `(c ? S{1,2} : S{3,4}) = S{3,4}`, `(c ? s : t).a = 9` and a reference bound to a member of one;
  a store, `++`, reference binding or by-reference argument on such a member is refused with a
  message that names the conditional or comma. A member of a conditional with an indexed array,
  read as `(c ? s : t).arr[1]`, says "a member or an index has to start at a name mslc knows".
- `(&S{1,2})->a`: Apple rejects it too ("taking the address of a temporary object", an error); mslc
  says `&` is not supported.
- `S{1,2} += S{1,2}`: Apple rejects it too and names the operator ("no viable overloaded '+='");
  mslc says "no viable overloaded compound assignment" for a S temporary.

## A function or variable named like a struct or enum: forms still refused or not hidden (#348)

A function or variable of the same name now hides the struct or enum tag before `(` and `{`, and
in a declaration, as in Apple, so the silent wrong-callee cases are gone. Still different:

- A function template named like a struct (`#348` says "or function template, see #341"): rejected
  loudly by the #341 diagnostic ("the function template S shares its name with a type"); it is
  tracked under #341.
- A local or block-scope struct beside an outer function of the same name (`struct S{..}; S s;`),
  `struct n::S s(..)` beside a variable `n::S`, `struct S s(..)` inside a namespace, `sizeof(struct
  S)` beside a function `S`, and a typedef'd struct construction `typedef struct S T; T(1.0f)`:
  Apple accepts them, mslc refuses them loudly (identical on master).
- `(S)x`, a C-style cast to an enum `S` that a function or variable hides: mslc accepts it where
  Apple rejects it; it picks no wrong callee.
- Wording: where Apple says "must use 'struct' tag to refer to type 'S'", mslc says "expected ;
  after an expression statement, found identifier s"; for a variable it says "S is not a builtin
  mslc recognises" where Apple says "called object type float is not a function".
- No probe pins that a function declared AFTER a use leaves the struct visible at the use (the
  behaviour is correct; the guard against hiding by a later declaration is untested).

## Wording gaps on references to members of temporaries (#251)

None of these is a wrong accept, but some are forms Apple accepts that mslc still refuses. Each row was
measured with `xcrun metal` and `mslc` on the same source.

- Refused where Apple accepts, because a matrix `operator[]` gives an lvalue even on a prvalue (`Q` holds a
  `float2x2 m`, `mkm()` returns a `float2x2`, `sw` takes `thread float2&`): `sw(Q{}.m[0])`, `sw(mkq().m[1])`
  and `sw(mkm()[0])`. mslc says "a member or an index has to start at a name mslc knows" for the first two
  and "assigning to an element of a value that is not a local, a struct member or a buffer element" for
  the third. The reference declarations (`thread float2& r = Q{}.m[0];`, `= mkm()[0];`) say "a place mslc
  cannot name", and through a conditional (`(c ? q1 : Q{}).m[0]`, `(c ? m1 : mkm())[0]`) they name the
  conditional, in a declaration and as an argument.
- `thread float& r = (c ? s : S{1,2}).v.x;` and `(c ? S{1,2} : s).v.x`: Apple says it cannot bind a
  temporary; mslc says the reference is bound to a component of a vector.
- `constant int& r = S{1,2}.a;` (y10): Apple says the binding "changes address space"; mslc says "a place
  mslc cannot name". `constant int& r = (c ? s : t).a;` and the `(c ? S{1,2} : t)` form (y31, y11): Apple
  says the same; mslc names the conditional.
- `one((c ? cs[0] : cs[1]).a)` for a `constant` buffer `cs` (x15): Apple says "no matching function" (cannot
  bind a reference in address space 'constant'); mslc names the conditional.
- `two((c ? s : t).a, S{1,2}.b)` (x27): Apple refuses the second argument ("expects an lvalue for 2nd
  argument") and accepts the first; mslc refuses the first, as a member of a conditional.
- `one(ds[0].a)` for a `device` buffer `ds` (x14): Apple says "no matching function" (cannot bind a reference
  in address space 'device'); mslc says the argument is not a variable of this function.

## enable_if defaults and type traits (#341, slice 3)

- Folded, measured against Apple: `is_same`, `is_floating_point`, `is_integral`, `is_arithmetic`, `is_signed`,
  `is_unsigned`, `is_scalar`, `is_vector`, `is_class`, `is_enum`, `is_const` (as `::value` or `_v`) and the types
  `enable_if`, `enable_if_t`, `make_scalar`, `make_scalar_t`. Metal's traits differ from C++'s (a vector is
  floating point, integral, signed or unsigned by its element; a matrix is only a class). Any other `is_*<...>`,
  `is_convertible` included, and a trait of a pointer, array, texture, sampler, atomic or address-space type
  is a diagnostic.
- An unsatisfied `enable_if` in a default, a return type or a parameter drops the template from the call's
  candidates. A condition that names a type parameter still being deduced is assumed satisfied while the
  signature is read for deduction and decided when it is read again with the type.
- Refused: `enable_if` as a non-type parameter's type (`typename enable_if<C, int>::type = 0`), as a
  statement-level local's type (`enable_if_t<C, int> y = 1;`), and a non-type default that reads a type
  parameter (`bool B = is_same<T, float>::value`), which is evaluated before the type is deduced.
- Not diagnosed (mslc is more lenient than Apple, no wrong value): two templates of one name and signature
  that differ only in their defaults, or a default repeated on a redeclaration (Apple: "template parameter
  redefines default argument"); an `enable_if<false>` or an undeclared trait in a template that is never
  instantiated, and an enable_if whose condition does not name a template parameter and is false
  (`enable_if_t<false, int>`, `is_same_v<float, int>`), which drops the template silently where Apple rejects
  it at the definition.
- `volatile` is not part of a type in mslc, so a trait of a volatile type and a volatile template argument
  (`f<volatile float>`) are diagnostics; Apple treats `volatile float` as a type of its own.
- A trait of a struct declared after the template that uses it, inside a namespace, is "not a type mslc
  knows" (an error, no wrong value).
- A function attribute ahead of a helper's return type is dropped for `always_inline`, `noinline`, `unused` and
  `used` (METAL_FUNC) and refused for any other, on a template when it is instantiated.

## Function template instantiation: what is refused or approximated (#341, slice 2)

Each of these is a diagnostic, not a silent pick, except where it says "approximated".

- A call whose template parameter would be deduced as an address-space-qualified type is refused: a
  reference parameter (`thread T&`, `const thread T&`) given an lvalue in device, constant or threadgroup
  memory makes Apple deduce `T = device float` and bind it, where a function taking `thread float&` cannot
  bind that lvalue. mslc has no qualified `T`. A by-value `T x` is unaffected (T is `float`).
- Partial ordering is not modelled: two templates that rank alike are reported ambiguous even where C++
  prefers the more specialised one.
- The result type of `f(x)` is known only once T is deduced, so `f(s).field` on a struct result is refused
  (it reads as a swizzle), and an unqualified call whose argument is such a call and whose name a function in
  a namespace shares is refused (argument-dependent lookup would need the struct). Assign the result to a
  variable first.
- Approximated: a `packed_float3` argument deduces `T = float3` (Apple deduces `packed_float3`). The two
  differ only in storage layout, so the instance computes the same values; mslc cannot tell them apart
  anywhere else either.
- Deduction from an enum, from a pointer that is not a local array, and from a texture is refused.
- Not lowered: `vec<T, N>` and other template-id types in a signature, `typename T::type`, trailing return
  types, default function arguments, enum-typed non-type parameters.
- A constant used in a function body above its declaration (`float g() { return K; } constant float K = 5.0f;`)
  is accepted for a plain helper and for an instance, where Apple reports an undeclared identifier. This is
  older than templates (the file-scope constants a function sees are not limited to the ones above it).
- Inside an instance, a call whose arguments name a parameter of the instance's template type is looked up
  through its struct arguments' namespaces at the end of the file (clang's point of instantiation). A call
  whose arguments do not (a local, a literal) is bound where the template is declared in Apple; when a
  function it could take is declared below the template, mslc refuses it. A local that has a dependent type
  (`T y = x; g(y)`) is treated as not dependent, so that case is refused too when such a function exists.
- An instance is an internal function: a `--compile-library` unit never exports or imports it (Apple makes it
  `linkonce_odr`), and an instance that calls a function the unit only declares is refused.
- A helper reached only through an instance that names a file-scope sampler is not reserved that sampler (the
  scan follows calls by name), so the sampler is unbound and the compile fails with `"smp" is not a sampler`.
  Pass the sampler as an argument. An instance that names one itself is refused earlier, with the same advice.
- Older gap, refused and not miscompiled: `f(sa, 1)` where `::f` takes one argument and `A::f(SA, int)` is found
  through the struct argument's namespace is refused by validateHelpers' by-name arity check before lookup
  runs ("call to f passes 2 arguments, and it takes 1"); Apple calls `A::f`.
- A float literal is parsed to a double and then rounded to float, so a literal just above a float
  neighbour double-rounds: `1.0000000596046448` gives mslc 1.0 where Apple gives 0x3F800001. Seen in
  braced, scalar and function-body positions, and the same on master. Parsing the literal text
  directly to float (as `strtof`) would fix it.
- `const volatile int N = 3; float a[N];` as a local: Apple refuses it ("array size is not a
  constant expression"); mslc accepts it, because the parser does not record `volatile` on a type.
  The length it takes is the right one, so this accepts invalid source and miscompiles nothing.


## Class template instantiation: what is refused or approximated (#341, slice 5)

A `template <...> struct S { ... };` is kept as tokens and re-read, with its arguments bound, the first time
`S<args>` is named; the instance is an ordinary struct called `S<args>` with its member functions and
constructor as helpers. Checked against Apple by execution on about 900 of 2,100 generated programs that both compilers accept
(member and free-function choice, non-type and default arguments, nested instances, buffer layout and `sizeof`):
no value differed in those, but two shapes found by review differed and are now refused (a using declaration
below the template that rebinds a name, and a later overload found by argument-dependent lookup).

- Refused, with a diagnostic naming the template and the arguments: partial and explicit specialisation,
  `class` and `union` templates, a base list, a forward declaration, static data members (also refused in a
  plain struct), a member template (slice 5c), overloaded constructors (also refused in a plain struct),
  instantiation deeper than 64 (Apple: 1024), and any error inside the instance.
- An instance first named inside a function template, or built from its parameters (`template <class T> float
  f(Box<T> b)`), is refused (slice 5d). An instance named at file scope above the function template is usable
  inside it.
- Every member function of an instance is read when the instance is made, so an unused member whose body does
  not compile for that argument is an error here and not in Apple. Apple reads a member only when it is used.
- Member functions count as declared where the template is. A function written after the template whose name
  the template calls is refused when an instance has a struct template argument (Apple finds it by
  argument-dependent lookup where the instance is made). Not covered: a struct that reaches the call another
  way (a member's own struct type, a parameter), where the older order rule still applies and can pick a
  different overload than Apple. A using declaration of a name the template spells, or any using directive
  not naming the template's own namespaces, made after it in a scope it reads from (including namespaces those
  nominate, transitively) is refused. A late `using` that Apple would not let change the result (a same-namespace
  `using X::K;` below a template reading K, `using metal::precise::sqrt;`) is refused too: false refusals.
  The argument-dependent lookup check also runs at the end of the file, since clang instantiates there.
- Not covered, silent wrong overload (Apple 2.0, mslc 1.0):
  `namespace N { struct S { float a; }; float h(S, float) { return 1.0f; } }
  template <typename T> struct P { N::S s; T v; float f() const { return h(s, 1); } };
  namespace N { float h(S, int) { return 2.0f; } }` with `P<float>`: the struct reaches the call through a member
  type, not a template argument, so the check does not run. Declare every overload above the template.
- A name an instance reads (typedef, constant, struct, default argument) that is declared after the template is
  refused ("is declared after the class template that uses it"): clang reads it from the definition, so using
  the later one would change the value (a shadowing `N::K`) and Apple rejects the plain case.
- False refusals: `A<(8>>1)>` (a `>>` inside parentheses in an argument), a using directive that makes a member's
  constant look ambiguous where Apple binds the file-scope one, and `BH::f()` through a typedef of an instance.
- The restore of a parameter binding that was absent before an instance (erase afterwards) cannot be observed
  from source: the block that names it is gone, so no probe pins it.
- An unused template is accepted without being read, so an undeclared type in its body is accepted where
  Apple reports `unknown type name`.
- `S<args>` as written is canonicalised: `S<float>` and `S<F>` for `typedef float F` are one struct, `S<const
  float>` another; a struct argument's namespace is spelled `$` in the instance's name (never shown).
- Older gap (master, not templates): calling a non-device member function on a `device const` object is accepted
  where Apple rejects it.
- A struct that contains an instance by value inherits the older gap that a struct field of struct type is not
  represented in a buffer struct.

## Library namespace lookup: leftovers found while fixing #344, #345 and #357

All of these predate those fixes; counts are from generated differentials against Apple.

- A user namespace `precise` or `fast` (at file scope, or inside `A` with `using namespace A;`) beside
  `using namespace metal;` is silently bound, where Apple reports the name ambiguous with
  `metal::precise` or `metal::fast`. This is the mechanism #345 fixed for `metal` itself, not yet applied
  to the library's sub-namespaces (about 29 cases).
- Unqualified `abs` or `sqrt` with no directive for `metal` in scope is accepted, where Apple reports an
  undeclared identifier (about 34 cases).
- The library's names are visible without `#include <metal_stdlib>` (about 11 cases).
- `namespace NB { typedef int metal; }` followed by `metal::abs` is accepted (2 cases).
- A file-scope vector constant folds only from a braced list or a constructor of its own type
  (#347, #363). Still refused though Apple accepts them: another constant (`constant int4 k = a;`),
  an expression on one (`-int4(1)`), a vector that narrows to a shorter one (`int3(int4(...))`), a
  wider scalar-from-vector mix Apple also takes, `'a'` as a component (parser limit), and a float
  too large for the integer (`int4(2147483648.0f)`, which Apple saturates). Apple rejects, and so
  does mslc, a vector piece of another scalar type beside other arguments (`int4(uint2(1, 2), 3, 4)`).

## sizeof: what is refused (#361)

`sizeof(type)` is folded by the parser with Metal's layout, and `sizeof expr` is sized by sema from
the operand's declared type; every accepted size was compared with `xcrun metal`. A refusal stands in
for a size mslc cannot give with certainty. Apple accepts, and mslc refuses:

- `sizeof(x)` as an array length or attribute index (#369) where `x` is a name the parser holds no
  type for: an `auto` variable, a texture or sampler, a parameter of a function template instance,
  a member (`sizeof(s.a)`), a name a struct body's own field hides, or an element of a vector or
  matrix. `x`, `x[i]` and `*x` of a declared local, parameter or file-scope object work and are
  sized from the innermost declaration the name finds.
- Operands whose type sema cannot read from the source (#370). It types an operand without building
  it, so nothing in it runs (`sizeof(i++)` leaves `i` alone and emits nothing), and gives the same
  size as Apple (compared by value) for names, elements, members, literals, casts, unary and binary
  operators (integer promotion, the usual conversions, a packed vector keeping its type as the left
  operand or beside a scalar), `?:` of scalars or of equal vectors, assignments, `++`/`--`, comma,
  `S{...}`, and a call of the one function its name finds. Still refused: a call whose name has more
  than one function, a template or a library function beside it; `?:` of arrays or of different
  structs; pointer arithmetic other than `p + n`, `n + p`, `p - n`; `&x`; a vector with a scalar
  of another type (`f3 * 2`), vector comparisons, shifts, `!` and `&&`, and every matrix arithmetic
  form (`m * m`, `m * v`, `v * m`, `m * 2.0f`); before #370 these were built and discarded and sized
  correctly, so they are a regression in reach, kept refused until their typing is modelled; a shift,
  `%` or bitwise operator of a float (Apple rejects them too); a vector assignment from a scalar
  (`f3 = f`) or a vector compound assignment (`f3 *= 2`); a pointer parameter of a helper function in an operand that is
  more than the bare name (it is bound like a local array).
- Operands Apple rejects are refused too, not sized: a compound assignment whose operator the operands
  do not take (`i <<= f`), `++`/`--` of a bool, a write to a const object, a struct assigned to an
  int, a call whose argument is not typable or does not convert to the parameter, a comma whose left
  side is not an expression, and a swizzle past the vector's width (`f2.z`) or mixing `xyzw` with `rgba`.
- A call to a function declared but never defined, inside sizeof (Apple does not need the body).
- A constant expression where the unsigned sizeof would wrap: `-sizeof(int) + 8`, `sizeof(int) - 8`
  (Apple rejects the latter), a negative operand beside it, or a cast that truncates it.


## A helper with a local `constexpr sampler`, reached by two entry points, is invalid SPIR-V (#167)

A local `constexpr sampler L(...)` declared inside a helper (not the entry point) is reserved only
for the entry point whose own body names it, and the helper is emitted once per stage. Two entry
points of one stage that both call the helper share one copy, which uses the first entry's sampler
variable: spirv-val reports "Interface variable ... is used by entry point 'g' ... but is not
listed as an interface". Apple accepts it. Identical on master; found while reviewing #167.
Reproduction: a helper `look(texture2d<float> t) { constexpr sampler L(filter::linear); return
t.sample(L, float2(0.25)); }` called from two fragment entries. A fix has to reserve helper-local
samplers per entry and mark the helper sampler-dependent (`_samplerDependent`), as for a file-scope
sampler; the single-entry case is valid today and must stay so.

## Empty structs (#376)

An empty struct as a value (parameter, argument, return, local, member, array element, program-scope
constant) is lowered with one placeholder `uint` member so Mesa's SPIR-V front end accepts it; sizeof
and buffer layout keep Metal's size 1, and a runtime array of them has ArrayStride 1. Not covered:
inheritance from an empty base, a class template and `return {};` are refused by the parser, a struct
with a struct member inside a buffer is refused, and a buffer of empty structs is only validated with
spirv-val, not run (diffcheck has no scalar layout for it).


## texture_buffer is compile-checked only, not run against Apple (#359)

`texture_buffer<float|half|int|uint|short|ushort, access::read|write|read_write>` lowers to a Dim
Buffer image (read: sampled, SampledBuffer, OpImageFetch; write and read_write: storage, ImageBuffer,
format-less OpImageRead and OpImageWrite) with a scalar uint or ushort index and OpImageQuerySize for
get_width. Probes pin the SPIR-V and the `TextureBuffer` reflection kind (descriptor_type
UniformTexelBuffer or StorageTexelBuffer). Neither diffcheck host (metalhost.mm, vkhost.c) binds a
texel buffer, and kernel mode skips textures altogether (#366; texel buffers are #378), so no value has been compared with
Apple's. Refused, though Apple accepts them: texture atomics on a texture_buffer, and an array of
them. Refused like Apple: access::sample, an int or float index, a lod, get_height,
get_num_mip_levels, sample and the other sampling methods, and a char or vector element type.

## Texture and sampler locals are names, not values (#358)

`texture2d<float> b = a;`, `auto b = a;` and `sampler t = s;` (also `const`, `thread`, an alias of an
alias, a local named like a file-scope sampler, an alias of a helper's own parameter) bind the new name
to the descriptor the old one has: no variable, no binding and no reflection entry is added. The
initialiser has to be a bare name, of the same kind, element type, depth, multisampling and access
(Apple refuses a mismatch too). Apple treats a texture or sampler as a run-time value, so what it
accepts and mslc refuses is everything that would make the descriptor a run-time choice, each with a
diagnostic: assigning to a texture or sampler local or parameter (`b = a2;`, also `sampler t; t = s;`), an initialiser that is not a name (`c ? a : a2`), a
texture local with no initialiser or a default-constructed one, a self-initialised one, a pointer, an
array or a reference to a texture, a texture as a struct field, a texture local in a `for` initialiser
or beside a second declarator, `auto` from a conditional, a copy of a sampler at file scope, and a
template parameter deduced from a texture (`template T q = t;`). Other Apple forms refused:
brace or paren initialisation of a texture or sampler local (`b{t}`, `b(t)`, `sampler u(s)`,
`= {s}`), `thread auto& b = t;`, and a struct texture member picked by a conditional. A dead chain (`sampler u = t;` never sampled) still lists
the embedded sampler it names, which Apple would not record.

## `metal::array<T, N>` (#362)

`array<T, N>` (also `metal::array`, and a `typedef` of it) is read by the parser as the array `T[N]` with
`Type::isStdArray` set, an array of arrays as a multi-dimensional one. Layout is `T[N]`'s (sizeof, member
offsets and buffer round trips were compared with Apple's compiler by value, with a negative control). What it adds
over a C array is value semantics, all compared by readback against Apple: a copy, `a = b`, `a = {1,2,3}`,
`array<T,N>{...}`, `= {{1,2,3}}`, a by-value parameter, a `thread` reference parameter, a return (also a
braced `return {...};` for any helper), an element of a returned array (`f()[i]`), a struct member as a whole, a
file-scope `constant` array, a threadgroup local, `size()`, `max_size()`, `empty()`, `front()`, `back()`
(folded by the parser for a declared array, so `a.size()` is an array length or a loop bound).

Apple accepts, and mslc refuses (each is a loud error):

- `device`, `constant` or `threadgroup` pointers and references to an array (`device array<float,4>* b`), and any
  `array<T,N>` entry point parameter: a buffer holds the array in a struct (which works).
- `thread array<T,N>*` helper parameters, `array<T,N> a[2]` (an array of them), and a member of that kind.
- `auto b = a;`, `a.data()`, `a.fill()` (Apple has none), `a == b` (Apple rejects it too), `array<T,0>` (Apple
  gives it the size of one element), a constant index past the end (Apple takes it, as undefined behaviour).
- `array<T,N>` in a function template signature (class template argument deduction is #341), a `using` alias,
  class template argument deduction (`array a = {1.0f}`: Apple rejects it too).
- `array<bool,N>` and `array<packed_float3,N>` as a member of a struct that lives in a buffer (the existing
  array-member layout limit; a struct used only as a local value with one compiles), an array
  of structs as a member of a struct, brace elision into the array member of a struct (`W w = {1,2}`), and a
  member of a temporary indexed (`mk().a[i]`).
- A matrix element of an array of matrices written through (`c[1][0][0] = x`, as for a C array).
- `alignof(array<...>)`; `f().size()`, `f().back()` and the other member calls on anything but a declared
  object (the parser folds only a declared name, element or member); `array<A,2> n = {a, b}` (arrays as
  elements of a braced list); `typedef array<float,3> v3; v3 m[2];`; `array<float,3> a{...}` and `b(a)`
  (brace and paren initialisation of a declaration); the chained assignment `b = a = c`.

Accepted though Apple rejects or reports them differently: an unqualified `array<float,3>` without
`using namespace metal`, `metal::array` without the include, `volatile array<...>`, and a file's own
`struct array` or `constant int array` beside `using namespace metal` (Apple reports the name ambiguous).

mslc zero-fills `array<T,N> a;` where Apple leaves it uninitialised; a read before a write differs only where
Apple's value is undefined.
