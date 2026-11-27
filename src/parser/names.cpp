#include "parser/internal.h"

namespace mslc {

std::string joinParts(const std::vector<std::string>& parts, size_t from, size_t to) {
	std::string out;
	for (size_t i = from; i < to; ++i) {
		out += (out.empty() ? "" : "::") + parts[i];
	}
	return out;
}


StdlibLookup stdlibLookup(std::string_view scope, std::string_view name) {
	static const std::string_view table =
#include "stdlib_names.inc"
		;
	StdlibLookup result;
	const std::string lead = std::string(scope) + " ";
	const std::string needle = " " + std::string(name) + " ";
	for (size_t at = 0; at < table.size();) {
		const size_t end = std::min(table.find('\n', at), table.size());
		const std::string line = " " + std::string(table.substr(at, end - at)) + " ";
		if (line.compare(1, lead.size(), lead) == 0) {
			result.scopeExists = true;
			result.hasName = result.hasName || line.find(needle, lead.size()) != std::string::npos;
		}
		at = end + 1;
	}
	return result;
}

// Bare type keywords, mapped to their scalar kind. MSL spells these
// without a typedef, so they are lexed as identifiers and recognised here.
const std::unordered_map<std::string_view, ScalarKind>& scalarTypeNames() {
	static const std::unordered_map<std::string_view, ScalarKind> table = {
		{ "void", ScalarKind::Void },
		{ "bool", ScalarKind::Bool },
		{ "char", ScalarKind::Char },
		{ "uchar", ScalarKind::UChar },
		{ "ushort", ScalarKind::UShort },
		{ "short", ScalarKind::Short },
		{ "uint", ScalarKind::UInt },
		{ "int", ScalarKind::Int },
		{ "ulong", ScalarKind::ULong },
		{ "long", ScalarKind::Long },
		{ "half", ScalarKind::Half },
		{ "float", ScalarKind::Float },
		{ "double", ScalarKind::Double },

		// The fixed-width typedefs MSL supplies. Blender's shaders use these
		// spellings without including a header, since MSL makes <cstdint>
		// available on its own.
		{ "int8_t", ScalarKind::Char },
		{ "uint8_t", ScalarKind::UChar },
		{ "int16_t", ScalarKind::Short },
		{ "uint16_t", ScalarKind::UShort },
		{ "int32_t", ScalarKind::Int },
		{ "uint32_t", ScalarKind::UInt },
		{ "int64_t", ScalarKind::Long },
		{ "uint64_t", ScalarKind::ULong },
		{ "size_t", ScalarKind::ULong },
	};

	return table;
}

bool isScalarTypeName(std::string_view text, ScalarKind& outKind) {
	const auto& table = scalarTypeNames();
	auto it = table.find(text);
	if (it == table.end()) {
		return false;
	}

	outKind = it->second;
	return true;
}

// A scalar, vector or matrix type name, filling in the scalar, the vector
// width and the matrix column count. Metal spells a vector as its scalar type
// with the component count on the end, so "float4" is "float" and a 4, and
// the only thing to do with the name is split it. The count has to be 2 to 9
// and the rest has to name a scalar: a type with a width of 1 is not spelled
// that way, and "float0" is not a type. A matrix is "floatCxR" or "halfCxR"
// with C and R from 2 to 4; MSL has no matrix of any other scalar.
bool isUnpackedTypeName(std::string_view text, Type& outType) {
	ScalarKind kind;
	if (isScalarTypeName(text, kind)) {
		outType.scalar = kind;
		return true;
	}

	if (text.size() < 2) {
		return false;
	}

	const char last = text.back();
	if (last < '2' || last > '9') {
		return false;
	}

	if (text.size() > 3 && text[text.size() - 2] == 'x') {
		const char columns = text[text.size() - 3];
		if (columns < '2' || columns > '4' || last > '4'
			|| !isScalarTypeName(text.substr(0, text.size() - 3), kind)
			|| (kind != ScalarKind::Float && kind != ScalarKind::Half)) {
			return false;
		}

		outType.scalar = kind;
		outType.vectorWidth = static_cast<uint32_t>(last - '0');
		outType.matrixColumns = static_cast<uint32_t>(columns - '0');
		return true;
	}

	if (!isScalarTypeName(text.substr(0, text.size() - 1), kind)) {
		return false;
	}

	outType.scalar = kind;
	outType.vectorWidth = static_cast<uint32_t>(last - '0');
	return true;
}

// packed_float3 and its family: packed_<scalar><N> for the integer, half and
// float scalars and N from 2 to 4. Apple also spells packed_bool, which mslc
// reports by name where the type is resolved, and has no packed double.
bool isPackedVectorName(std::string_view text, Type& outType) {
	constexpr std::string_view prefix = "packed_";
	if (text.size() <= prefix.size() || text.substr(0, prefix.size()) != prefix) {
		return false;
	}

	Type vector;
	if (!isUnpackedTypeName(text.substr(prefix.size()), vector) || !vector.isVector()
		|| vector.isMatrix() || vector.vectorWidth > 4 || vector.scalar == ScalarKind::Double) {
		return false;
	}

	outType.scalar = vector.scalar;
	outType.vectorWidth = vector.vectorWidth;
	outType.isPacked = true;
	return true;
}

bool isTypeName(std::string_view text, Type& outType) {
	return isUnpackedTypeName(text, outType) || isPackedVectorName(text, outType);
}

bool isMetalBuiltinTypeName(std::string_view text) {
	ScalarKind kind;
	if (isScalarTypeName(text, kind)) {
		return text.front() == 'u' || text.substr(text.size() - 2) == "_t";
	}
	if (text == "atomic_int" || text == "atomic_uint") {
		return true;
	}
	const bool packed = text.substr(0, 7) == "packed_";
	if (packed) {
		text.remove_prefix(7);
	}
	if (text.size() < 2 || text.back() < '2' || text.back() > '4') {
		return false;
	}
	if (text.size() > 3 && text[text.size() - 2] == 'x') {
		const auto leaf = text.substr(0, text.size() - 3);
		return !packed && (leaf == "float" || leaf == "half")
			&& text[text.size() - 3] >= '2' && text[text.size() - 3] <= '4';
	}
	const auto leaf = text.substr(0, text.size() - 1);
	return leaf.find('_') == std::string_view::npos && isScalarTypeName(leaf, kind)
		&& kind != ScalarKind::Void && kind != ScalarKind::Double;
}

// The vector typedefs Apple's compiler has without any include: vector_float4,
// simd_uint2 and the rest. They name a vector of 2 to 4 components only, so
// vector_float and vector_float4x4 are not types, and neither are the double
// and 8 or 16 wide ones, which Apple declares as incomplete.
bool isSimdVectorName(std::string_view text, Type& outType) {
	for (const std::string_view prefix: { std::string_view("vector_"), std::string_view("simd_") }) {
		if (text.size() > prefix.size() && text.substr(0, prefix.size()) == prefix) {
			Type vector;
			if (isUnpackedTypeName(text.substr(prefix.size()), vector) && vector.isVector() && !vector.isMatrix()
				&& vector.vectorWidth <= 4 && vector.scalar != ScalarKind::Double
				&& vector.scalar != ScalarKind::Void) {
				outType.scalar = vector.scalar;
				outType.vectorWidth = vector.vectorWidth;
				return true;
			}
		}
	}

	return false;
}

// Whether a name is a scalar, vector or matrix type, for a caller that only
// needs to know which it is.
bool isTypeName(std::string_view text) {
	Type type;
	return isTypeName(text, type);
}

// The words C++ combines, in any order, into one integer type name:
// "unsigned long int" is a ulong, and so is "int long unsigned".
bool isIntegerSpecifier(std::string_view text) {
	return text == "unsigned" || text == "signed" || text == "short" || text == "long"
		|| text == "int" || text == "char";
}

// Keywords that may appear before a type and are not address spaces.
bool isTypeQualifier(std::string_view text) {
	return text == "const" || text == "static" || text == "constexpr"
		|| text == "volatile" || text == "__restrict";
}

// The texture and sampler type names. Only texture1d, texture2d, texturecube, texture3d,
// texture2d_array and sampler are lowered; the rest are recognised so they are
// reported by name rather than read as a struct type and failing later.
bool isResourceTypeName(std::string_view text) {
	static const std::set<std::string_view> names = {
		"sampler", "texture1d", "texture1d_array", "texture2d", "texture2d_array",
		"texture2d_ms", "texture2d_ms_array", "texture3d", "texturecube",
		"texturecube_array", "texture_buffer", "depth2d", "depth2d_array",
		"depth2d_ms", "depth2d_ms_array", "depthcube", "depthcube_array",
	};
	return names.count(text) > 0;
}

std::optional<Interpolation> interpolationFromName(std::string_view name) {
	static const std::map<std::string_view, Interpolation> table = {
		{ "flat", Interpolation::Flat },
		{ "center_perspective", Interpolation::CenterPerspective },
		{ "center_no_perspective", Interpolation::CenterNoPerspective },
		{ "centroid_perspective", Interpolation::CentroidPerspective },
		{ "centroid_no_perspective", Interpolation::CentroidNoPerspective },
		{ "sample_perspective", Interpolation::SamplePerspective },
		{ "sample_no_perspective", Interpolation::SampleNoPerspective },
	};
	const auto found = table.find(name);
	return found == table.end() ? std::nullopt : std::optional<Interpolation>(found->second);
}

// MSL builtin attributes the subset can map onto a SPIR-V BuiltIn. An
// attribute outside this table is rejected rather than ignored, so an
// unsupported builtin can never be dropped without a diagnostic.
std::optional<ParameterAttributes::Builtin> builtinFromName(std::string_view name) {
	using Builtin = ParameterAttributes::Builtin;

	if (name == "thread_position_in_grid") { return Builtin::ThreadPositionInGrid; }
	if (name == "threadgroup_position_in_grid") { return Builtin::ThreadgroupPositionInGrid; }
	if (name == "thread_position_in_threadgroup") { return Builtin::ThreadPositionInThreadgroup; }
	if (name == "thread_index_in_threadgroup") { return Builtin::ThreadIndexInThreadgroup; }
	if (name == "threads_per_threadgroup") { return Builtin::ThreadsPerThreadgroup; }
	if (name == "vertex_id") { return Builtin::VertexID; }
	if (name == "instance_id") { return Builtin::InstanceID; }
	if (name == "base_vertex") { return Builtin::BaseVertex; }
	if (name == "base_instance") { return Builtin::BaseInstance; }
	if (name == "position") { return Builtin::Position; }
	if (name == "frag_coord") { return Builtin::FragCoord; }
	if (name == "front_facing") { return Builtin::FrontFacing; }

	return std::nullopt;
}

// Cast targets the subset accepts in an explicit cast expression.
bool isCastTypeName(std::string_view name) {
	return scalarTypeNames().count(name) > 0;
}

// The address space a name spells, or nothing when it spells none. Shared by
// the type parser and the top-level declaration dispatch, so the two agree on
// which names are address spaces.
std::optional<AddressSpace> addressSpaceFor(std::string_view name) {
	if (name == "device") { return AddressSpace::Device; }
	if (name == "constant") { return AddressSpace::Constant; }
	if (name == "threadgroup") { return AddressSpace::Threadgroup; }
	if (name == "thread") { return AddressSpace::Thread; }

	return std::nullopt;
}

// A variable in a function lives in the thread address space, or in
// threadgroup, and is never static; Apple reports anything else. On a
// pointer the address space is the pointee's, which is free to be device.
// A reference is valid MSL that mslc does not lower, so it gets its own diagnostic
// rather than an address-space one.
void rejectLocalQualifiers(const Type& type, bool isReference, bool referenceAllowed) {
	if (isReference && !referenceAllowed) {
		throw CompileError("a reference in function scope is not lowered yet");
	}
	if (type.isStatic) {
		throw CompileError("variables in function scope cannot be declared static");
	}
	// A reference names a place that is already in its own address space, so
	// "const device S &r = buffer[i];" is the one way to spell it.
	if (!isReference && !type.isPointer && (type.addressSpace == AddressSpace::Device || type.addressSpace == AddressSpace::Constant)) {
		throw CompileError(std::string("variables in function scope cannot be in the ")
			+ addressSpaceName(type.addressSpace) + " address space");
	}
}


bool isMSLBuiltinName(std::string_view name) {
	static const char* const names[] = {
		"thread_position_in_grid",
		"threadgroup_position_in_grid",
		"thread_position_in_threadgroup",
		"thread_index_in_threadgroup",
		"vertex_id",
		"instance_id",
		"base_vertex",
		"base_instance",
		"position",
		"frag_coord",
		"front_facing",
		"threadgroups_per_grid",
		"threads_per_threadgroup",
	};

	for (const char* candidate: names) {
		if (name == candidate) {
			return true;
		}
	}

	return false;
}

} // namespace mslc
