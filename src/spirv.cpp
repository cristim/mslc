#include "spirv.h"

#include <cstring>

namespace mslc {
namespace spirv {

namespace {

	// Opcodes that belong in a fixed section regardless of where the caller
	// thinks it is. A function body routinely needs a constant, and letting
	// that switch the section would emit the rest of the body into the types
	// block, ahead of the function. Routing here rather than at each call site
	// means the caller cannot get it wrong.
	Section sectionForOpcode(uint16_t opcode) {
		switch (opcode) {
			// Capabilities come first in the logical layout, whatever the
			// caller believes it is emitting. A capability raised while a type
			// was being declared landed in the graph-definitions section, and
			// spirv-val rejects the whole module for it.
			case OpCapability:
				return Section::Capabilities;

			case OpTypeVoid:
			case OpTypeBool:
			case OpTypeInt:
			case OpTypeFloat:
			case OpTypeVector:
			case OpTypeMatrix:
			case OpTypeArray:
			case OpTypeRuntimeArray:
			case OpTypeStruct:
			case OpTypePointer:
			case OpTypeFunction:
			case OpConstant:
			case OpConstantTrue:
			case OpConstantFalse:
			case OpConstantComposite:
			case OpSpecConstant:
			case OpSpecConstantTrue:
			case OpSpecConstantFalse:
			case OpVariable:
			case OpUndef:
				return Section::TypesGlobals;

			case OpFunction:
			case OpFunctionParameter:
			case OpFunctionEnd:
			case OpLabel:
				return Section::Functions;

			// Annotations have their own section, and it comes after the entry
			// points. Emitting a decoration into whatever section the caller
			// happens to be in puts it ahead of OpEntryPoint, which the logical
			// layout forbids and spirv-val rejects as an invalid section.
			case OpDecorate:
			case OpMemberDecorate:
			case OpDecorationGroup:
			case OpGroupDecorate:
			case OpGroupMemberDecorate:
			case OpDecorateId:
			case OpDecorateString:
			case OpMemberDecorateString:
				return Section::Annotations;

			case OpExtInstImport:
				return Section::ExtInstImports;

			// Debug names, which likewise have a fixed section.
			case OpName:
			case OpMemberName:
			case OpSource:
			case OpSourceContinued:
			case OpSourceExtension:
			case OpModuleProcessed:
				return Section::Debug;

			default:
				return Section::Invalid;
		}
	}

	// The section order the specification mandates.
	const Section kSectionOrder[] = {
		Section::Capabilities,
		Section::Extensions,
		Section::ExtInstImports,
		Section::MemoryModel,
		Section::EntryPoints,
		Section::ExecutionModes,
		Section::Debug,
		Section::Annotations,
		Section::TypesGlobals,
		Section::Functions,
		Section::Literals,
	};

}

// The section an opcode belongs in, given where the caller is emitting. A
// member because it reads the current section.
Section Builder::pickSection(uint16_t opcode) const {
	const Section forced = sectionForOpcode(opcode);
	return forced != Section::Invalid ? forced : _currentSection;
}

Id Builder::emit(uint16_t opcode, std::vector<uint32_t> operands) {
	Instruction instruction;
	instruction.opcode = opcode;
	instruction.words = std::move(operands);

	_sections[pickSection(opcode)].push_back(std::move(instruction));

	// Ids are allocated independently of emission, so the caller decides which
	// value to return. Returning the next id keeps call sites terse.
	return _nextId++;
}

Id Builder::emitTyped(uint16_t opcode, Id resultType, std::vector<uint32_t> operands) {
	// A value-producing instruction is laid out as
	// [result type, result id, operands...]. Both leading words are needed:
	// omitting the id makes the first real operand be read as the result, which
	// shows up as ids defined twice or as a zero id.
	const Id id = _nextId++;
	operands.insert(operands.begin(), id);
	operands.insert(operands.begin(), resultType);

	Instruction instruction;
	instruction.opcode = opcode;
	instruction.words = std::move(operands);
	_sections[pickSection(opcode)].push_back(std::move(instruction));
	_valueTypes[id] = resultType;
	return id;
}

Id Builder::emitDecl(uint16_t opcode, std::vector<uint32_t> operands) {
	const Id id = _nextId++;

	// The result id is the first operand of every declaration.
	operands.insert(operands.begin(), id);

	Instruction instruction;
	instruction.opcode = opcode;
	instruction.words = std::move(operands);
	_sections[pickSection(opcode)].push_back(std::move(instruction));

	return id;
}

Id Builder::emitDeclTyped(uint16_t opcode, Id resultType, std::vector<uint32_t> operands) {
	const Id id = _nextId++;

	// A local lives in the function, unlike every other OpVariable.
	const bool isLocal = opcode == OpVariable && !operands.empty()
		&& operands[0] == static_cast<uint32_t>(StorageClass::Function);

	operands.insert(operands.begin(), id);
	operands.insert(operands.begin(), resultType);

	Instruction instruction;
	instruction.opcode = opcode;
	instruction.words = std::move(operands);

	_sections[isLocal ? Section::FunctionVariables : pickSection(opcode)].push_back(std::move(instruction));

	// The object a variable names is a value the body can load from, so record
	// its type the same way a value-producing instruction would.
	_valueTypes[id] = resultType;

	return id;
}

void Builder::emitDeclTypedAt(uint16_t opcode, Id resultType, Id resultId,
	std::vector<uint32_t> operands) {

	operands.insert(operands.begin(), resultId);
	operands.insert(operands.begin(), resultType);

	Instruction instruction;
	instruction.opcode = opcode;
	instruction.words = std::move(operands);
	_sections[pickSection(opcode)].push_back(std::move(instruction));

	_valueTypes[resultId] = resultType;
}

Id Builder::typeOf(Id value) const {
	auto it = _valueTypes.find(value);
	return it == _valueTypes.end() ? InvalidId : it->second;
}

void Builder::setType(Id value, Id type) {
	_valueTypes[value] = type;
}

void Builder::appendString(std::vector<uint32_t>& words, const std::string& text) {
	// Padded to a whole number of words, NUL-terminated, packed
	// little-endian regardless of host byte order.
	const size_t wordCount = (text.size() + 1 + 3) / 4;
	const size_t base = words.size();
	words.resize(base + wordCount, 0u);

	auto* bytes = reinterpret_cast<uint8_t*>(words.data() + base);
	std::memcpy(bytes, text.data(), text.size());
}

Id Builder::stringLiteral(const std::string& text) {
	std::vector<uint32_t> words;
	appendString(words, text);

	auto it = _literalIds.find(words);
	if (it != _literalIds.end()) {
		return it->second;
	}

	const Id id = _nextId++;

	Instruction instruction;
	instruction.opcode = OpString;
	instruction.words = std::move(words);
	_literals.push_back(std::move(instruction));

	// Keyed on the words before the move above.
	_literalIds.emplace(std::move(words), id);

	return id;
}

bool Builder::finalize(std::vector<uint8_t>& out) const {
	if (!hasEntryPoint()) {
		return false;
	}

	// Build the word stream first, since the header needs the total count.
	std::vector<uint32_t> words;
	words.push_back(kSpirvMagic);
	words.push_back(kSpirvVersion);
	words.push_back(0); // generator
	words.push_back(0); // bound
	words.push_back(0); // schema, patched below

	const auto append = [&words](const Instruction& instruction) {
		// The high 16 bits hold the total word count including this one.
		words.push_back((static_cast<uint32_t>(instruction.words.size() + 1) << 16)
			| instruction.opcode);
		words.insert(words.end(), instruction.words.begin(), instruction.words.end());
	};

	const auto locals = _sections.find(Section::FunctionVariables);
	bool localsPlaced = locals == _sections.end();

	for (Section section: kSectionOrder) {
		auto it = _sections.find(section);
		if (it == _sections.end()) {
			continue;
		}

		for (const Instruction& instruction: it->second) {
			append(instruction);

			// The module has one function, so its first label opens the
			// entry block the locals belong to.
			if (!localsPlaced && section == Section::Functions && instruction.opcode == OpLabel) {
				for (const Instruction& local: locals->second) {
					append(local);
				}
				localsPlaced = true;
			}
		}
	}

	// The literal section is emitted as instructions too, but it is built in a
	// separate list so it can be appended last.
	for (const Instruction& instruction: _literals) {
		append(instruction);
	}

	words[3] = _nextId; // bound: one past the highest id handed out

	out.resize(words.size() * sizeof(uint32_t));
	std::memcpy(out.data(), words.data(), out.size());

	return true;
}

}
}
