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
		// Every type declaration in the core grammar sits in one contiguous
		// block, from OpTypeVoid to OpTypeForwardPointer, so the block is checked
		// as a range. Listing the opcodes one by one meant a type the emitter
		// had not used yet was emitted into whatever section the caller was last
		// in, which put it ahead of the ids it names and spirv-val rejected.
		if (opcode >= OpTypeVoid && opcode <= OpTypeForwardPointer) {
			return Section::TypesGlobals;
		}

		switch (opcode) {
			// An extended instruction set is imported before the memory model, and
			// a builtin is only discovered where it is called, which is inside a
			// function body. Left to the current section the import lands among
			// the body it is used in, and the logical layout rejects it as an
			// invalid section.
			case OpExtInstImport:
				return Section::ExtInstImports;

			// A capability has its own section, and which one is emitted depends on
			// the types a shader turns out to use rather than on where the emitter
			// is when it finds out. Float16, Int64 and Matrix are all discovered
			// mid-stream, so without this they land wherever the last instruction
			// went and spirv-val rejects the module as an invalid section.
			case OpCapability:
				return Section::Capabilities;

			case OpConstant:
			case OpConstantNull:
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
	};

}

// The section an opcode belongs in, given where the caller is emitting. A
// member because it reads the current section.
Section Builder::pickSection(uint16_t opcode, const std::vector<uint32_t>& operands) const {
	// A Function-storage variable is part of the body it is declared in: the
	// specification allows one nowhere else, so it stays in the section the
	// caller is emitting rather than joining the module's global variables. Its
	// storage class is its first operand, which is why the section is decided
	// before the result type and id are prepended.
	if (opcode == OpVariable && !operands.empty()
		&& operands.front() == static_cast<uint32_t>(StorageClass::Function)) {
		return _currentSection;
	}

	const Section forced = sectionForOpcode(opcode);
	return forced != Section::Invalid ? forced : _currentSection;
}

Id Builder::emit(uint16_t opcode, std::vector<uint32_t> operands) {
	const Section section = pickSection(opcode, operands);

	Instruction instruction;
	instruction.opcode = opcode;
	instruction.words = std::move(operands);

	_sections[section].push_back(std::move(instruction));

	// Ids are allocated independently of emission, so the caller decides which
	// value to return. Returning the next id keeps call sites terse.
	return _nextId++;
}

Id Builder::emitTyped(uint16_t opcode, Id resultType, std::vector<uint32_t> operands) {
	const Section section = pickSection(opcode, operands);

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
	place(section, std::move(instruction), opcode);
	_valueTypes[id] = resultType;
	return id;
}

Id Builder::emitDecl(uint16_t opcode, std::vector<uint32_t> operands) {
	const Section section = pickSection(opcode, operands);
	const Id id = _nextId++;

	// The result id is the first operand of every declaration.
	operands.insert(operands.begin(), id);

	Instruction instruction;
	instruction.opcode = opcode;
	instruction.words = std::move(operands);
	place(section, std::move(instruction), opcode);

	return id;
}

Id Builder::emitDeclTyped(uint16_t opcode, Id resultType, std::vector<uint32_t> operands) {
	const Section section = pickSection(opcode, operands);
	const Id id = _nextId++;

	operands.insert(operands.begin(), id);
	operands.insert(operands.begin(), resultType);

	Instruction instruction;
	instruction.opcode = opcode;
	instruction.words = std::move(operands);
	place(section, std::move(instruction), opcode);

	// The object a variable names is a value the body can load from, so record
	// its type the same way a value-producing instruction would.
	_valueTypes[id] = resultType;

	return id;
}

void Builder::emitDeclTypedAt(uint16_t opcode, Id resultType, Id resultId,
	std::vector<uint32_t> operands) {
	const Section section = pickSection(opcode, operands);

	operands.insert(operands.begin(), resultId);
	operands.insert(operands.begin(), resultType);

	Instruction instruction;
	instruction.opcode = opcode;
	instruction.words = std::move(operands);
	place(section, std::move(instruction), opcode);

	_valueTypes[resultId] = resultType;
}

void Builder::emitDeclAt(uint16_t opcode, Id resultId, std::vector<uint32_t> operands) {
	const Section section = pickSection(opcode, operands);

	operands.insert(operands.begin(), resultId);

	Instruction instruction;
	instruction.opcode = opcode;
	instruction.words = std::move(operands);
	place(section, std::move(instruction), opcode);
}

// A variable belongs at the top of the function's first block, and an
// instruction is written as [result type, result id, operands...], so an
// OpVariable's storage class is its third word. Only a Function-storage one is
// buffered: a descriptor or an interface variable is a module global and goes
// where it was emitted.
void Builder::place(Section section, Instruction instruction, uint16_t opcode) {
	if (_prologueOpen && opcode == OpVariable && instruction.words.size() > 2
		&& instruction.words[2] == static_cast<uint32_t>(StorageClass::Function)) {
		_prologue.push_back(std::move(instruction));
		return;
	}

	_sections[section].push_back(std::move(instruction));
}

// Opened with the function's opening label already emitted, so _prologueAt is
// that label's position in the Functions section and the buffer lands directly
// behind it.
void Builder::openPrologue() {
	_prologueOpen = true;
	_prologueAt = _sections[Section::Functions].size();
}

void Builder::closePrologue() {
	if (!_prologueOpen) {
		return;
	}

	_prologueOpen = false;

	std::vector<Instruction>& functions = _sections[Section::Functions];
	functions.insert(functions.begin() + static_cast<long>(_prologueAt),
		_prologue.begin(), _prologue.end());
	_prologue.clear();
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

	for (Section section: kSectionOrder) {
		auto it = _sections.find(section);
		if (it == _sections.end()) {
			continue;
		}

		for (const Instruction& instruction: it->second) {
			// The high 16 bits hold the total word count including this one.
			words.push_back((static_cast<uint32_t>(instruction.words.size() + 1) << 16)
				| instruction.opcode);
			words.insert(words.end(), instruction.words.begin(), instruction.words.end());
		}
	}

	words[3] = _nextId; // bound: one past the highest id handed out

	out.resize(words.size() * sizeof(uint32_t));
	std::memcpy(out.data(), words.data(), out.size());

	return true;
}

}
}
