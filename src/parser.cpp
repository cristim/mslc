#include "parser.h"

#include <unordered_map>

namespace mslc {

namespace {

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

	// Keywords that may appear before a type and are not address spaces.
	bool isTypeQualifier(std::string_view text) {
		return text == "const" || text == "static" || text == "constexpr"
			|| text == "volatile" || text == "restrict" || text == "__restrict";
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
		if (name == "vertex_id") { return Builtin::VertexID; }
		if (name == "instance_id") { return Builtin::InstanceID; }
		if (name == "position") { return Builtin::Position; }
		if (name == "frag_coord") { return Builtin::FragCoord; }
		if (name == "front_facing") { return Builtin::FrontFacing; }

		return std::nullopt;
	}

	// Cast targets the subset accepts in an explicit cast expression.
	bool isCastTypeName(std::string_view name) {
		return scalarTypeNames().count(name) > 0;
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

const Token& Parser::lookahead(size_t offset) const {
	const size_t index = _position + offset;
	return index < _tokens.size() ? _tokens[index] : _tokens.back();
}

const Token& Parser::advance() {
	const Token& token = _tokens[_position];
	if (_position + 1 < _tokens.size()) {
		++_position;
	}
	return token;
}

void Parser::expect(TokenKind expected, const char* context) {
	if (!at(expected)) {
		throw CompileError(std::string("expected ") + tokenKindName(expected) + " " + context
			+ ", found " + tokenKindName(kind()) + " \"" + std::string(current().text) + "\"");
	}

	advance();
}

bool Parser::match(TokenKind expected) {
	if (!at(expected)) {
		return false;
	}

	advance();
	return true;
}

bool Parser::atKeyword(const char* text) const {
	return kind() == TokenKind::Identifier && current().text == text;
}

bool Parser::matchIdentifier(const char* text) {
	if (!atKeyword(text)) {
		return false;
	}

	advance();
	return true;
}

const Token& Parser::expectKeyword(const char* text, const char* context) {
	if (!atKeyword(text)) {
		throw CompileError(std::string("expected \"") + text + "\" " + context
			+ ", found \"" + std::string(current().text) + "\"");
	}

	return advance();
}

size_t Parser::line() const {
	return current().line;
}

void Parser::skipDirectiveLine(size_t directiveLine) {
	while (!at(TokenKind::EndOfFile) && current().line == directiveLine) {
		advance();
	}
}

TranslationUnit Parser::parse() {
	while (!at(TokenKind::EndOfFile)) {
		parseDeclaration();
	}

	return std::move(_unit);
}

// The corpus starts with a licence block, a #include <metal_stdlib> and a
// using-directive. None of those change the meaning of the code that follows,
// so the include and using are consumed and discarded. A directive that could
// change meaning, such as a conditional, is an error.
void Parser::parsePreprocessorDirective() {
	const size_t directiveLine = line();
	advance(); // '#'

	// A '#' with nothing after it on its own line is the null directive, which
	// C defines and Apple's compiler accepts. Reading a name here without the
	// line bound would take the next line's first token for the directive name
	// and blame it, so the name is only read from this line.
	if (at(TokenKind::EndOfFile) || current().line != directiveLine) {
		return;
	}

	if (atKeyword("include")) {
		advance();
		match(TokenKind::Less);

		// Skip the header name, which ends at '>' or at the end of the line. A
		// quoted include has no '>' at all, so the line bound is what keeps it
		// from consuming the declarations that follow.
		while (!at(TokenKind::EndOfFile) && !at(TokenKind::Greater)
			&& current().line == directiveLine) {
			advance();
		}

		// A '>' the line does not have is a truncated include, not a reason to
		// keep looking on the next line.
		match(TokenKind::Greater);
		return;
	}

	if (atKeyword("define")) {
		advance();
		advance(); // macro name
		// Parameter list, if present. Bounded by the line like everything else,
		// so an unclosed '(' cannot swallow the rest of the file.
		if (at(TokenKind::LParen)) {
			while (!at(TokenKind::EndOfFile) && !at(TokenKind::RParen)
				&& current().line == directiveLine) {
				advance();
			}
			expect(TokenKind::RParen, "to close a macro parameter list");
		}
		// Replacement list, to the end of the line. A backslash continuation is
		// not part of the subset and the lexer has no escape for it, so a
		// continuation is a lexer error rather than a joined line.
		skipDirectiveLine(directiveLine);
		return;
	}

	if (atKeyword("if") || atKeyword("ifdef") || atKeyword("ifndef") || atKeyword("elif")
		|| atKeyword("else") || atKeyword("endif")) {
		throw CompileError("conditional compilation is not supported; mslc needs the "
			"preprocessor to run before parsing, which is not implemented yet");
	}

	// A pragma is advice to the compiler and changes no meaning. #undef and
	// #line pair with what is already discarded, and #line only moves a
	// diagnostic. xcrun metal accepts all three, so does mslc, on the directive
	// name alone: neither #undef with no name nor #line with a non-numeric
	// argument is rejected here. Checking arguments is M1 item 8's preprocessor.
	if (atKeyword("pragma") || atKeyword("undef") || atKeyword("line")) {
		skipDirectiveLine(directiveLine);
		return;
	}

	// #error exists to fail the build. mslc cannot evaluate the conditional that
	// would have guarded one, so a reached #error is a real error rather than
	// something to discard.
	if (atKeyword("error")) {
		throw CompileError("#error directive in this source; mslc cannot evaluate "
			"conditionals, so it cannot be one that was meant to be skipped");
	}

	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a preprocessor directive after '#'");
	}

	// A preprocessor rejects a directive it does not know, and so does Apple's.
	// Discarding one means a shader whose intent was lost compiles anyway.
	throw CompileError("unknown preprocessor directive \"#" + std::string(advance().text)
		+ "\"; mslc handles #include, #define, #undef, #line and #pragma");
}

void Parser::parseDeclaration() {
	if (at(TokenKind::Semicolon)) {
		advance();
		return;
	}

	if (at(TokenKind::Hash)) {
		parsePreprocessorDirective();
		return;
	}

	if (matchIdentifier("using")) {
		// using namespace metal;
		advance(); // namespace
		advance(); // name
		expect(TokenKind::Semicolon, "after using directive");
		return;
	}

	if (matchIdentifier("struct")) {
		_unit.structs.push_back(parseStructDeclaration());
		return;
	}

	if (atKeyword("kernel")) {
		advance();
		_unit.functions.push_back(parseFunctionDeclaration(Stage::Kernel));
		return;
	}

	if (atKeyword("vertex")) {
		advance();
		_unit.functions.push_back(parseFunctionDeclaration(Stage::Vertex));
		return;
	}

	if (atKeyword("fragment")) {
		advance();
		_unit.functions.push_back(parseFunctionDeclaration(Stage::Fragment));
		return;
	}

	if (atKeyword("typedef")) {
		throw CompileError("typedef is not supported");
	}

	throw CompileError("unexpected \"" + std::string(current().text) + "\" at top level; "
		"expected a struct, kernel, vertex or fragment declaration");
}

StructDecl Parser::parseStructDeclaration() {
	StructDecl decl;

	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a struct name, found " + std::string(tokenKindName(kind())));
	}

	decl.name = std::string(advance().text);

	if (at(TokenKind::Colon)) {
		// Inheritance is not part of the subset; consume the base list so the
		// error names something useful rather than a stray colon.
		advance();
		if (!atKeyword("public") && !atKeyword("private") && !atKeyword("protected")) {
			throw CompileError("struct inheritance is not supported (in \"" + decl.name + "\")");
		}
		advance();
		advance();
	}

	expect(TokenKind::LBrace, "to open a struct body");

	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		StructField field;
		field.type = parseType();
		expectFieldName(field.name);
		match(TokenKind::Semicolon);
		decl.fields.push_back(std::move(field));
	}

	expect(TokenKind::RBrace, "to close a struct body");
	match(TokenKind::Semicolon);

	return decl;
}

void Parser::expectFieldName(std::string& out) {
	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a field name, found " + std::string(tokenKindName(kind())));
	}

	out = std::string(advance().text);
}

bool Parser::parseAddressSpace(AddressSpace& space) {
	if (atKeyword("device")) { space = AddressSpace::Device; }
	else if (atKeyword("constant")) { space = AddressSpace::Constant; }
	else if (atKeyword("threadgroup")) { space = AddressSpace::Threadgroup; }
	else if (atKeyword("thread")) { space = AddressSpace::Thread; }
	else { return false; }

	advance();
	return true;
}

std::optional<uint32_t> Parser::tryParseArrayLength() {
	if (!at(TokenKind::LBracket)) {
		return std::nullopt;
	}

	advance();

	if (!at(TokenKind::IntegerLiteral)) {
		throw CompileError("only constant array lengths are supported");
	}

	auto length = static_cast<uint32_t>(advance().integerValue);
	expect(TokenKind::RBracket, "to close an array length");
	return length;
}

Type Parser::parseType() {
	Type type;

	AddressSpace space;
	if (parseAddressSpace(space)) {
		type.addressSpace = space;
	}

	while (kind() == TokenKind::Identifier && isTypeQualifier(current().text)) {
		if (current().text == "const") {
			type.isConst = true;
		}
		advance();
	}

	ScalarKind scalarKind;
	if (kind() == TokenKind::Identifier && isScalarTypeName(current().text, scalarKind)) {
		std::string_view base = advance().text;
		type.scalar = scalarKind;

		// Vector suffix: float4, uint2, half3. The count is part of the name
		// in MSL, so it is split off here rather than being a separate token.
		if (base.size() > 1) {
			const char last = base.back();
			if (last >= '2' && last <= '9') {
				type.vectorWidth = static_cast<uint32_t>(last - '0');
				type.scalar = scalarKind;
				// Re-derive the base name without the digits.
				std::string_view trimmed = base.substr(0, base.size() - 1);
				if (!isScalarTypeName(trimmed, scalarKind)) {
					throw CompileError("unknown type \"" + std::string(base) + "\"");
				}
				type.scalar = scalarKind;
			}
		}
	} else if (kind() == TokenKind::Identifier) {
		type.namedType = std::string(advance().text);
	} else {
		throw CompileError("expected a type, found " + std::string(tokenKindName(kind()))
			+ " \"" + std::string(current().text) + "\"");
	}

	while (at(TokenKind::Star)) {
		advance();
		type.isPointer = true;
	}

	if (auto length = tryParseArrayLength()) {
		type.arrayLength = length;
	}

	return type;
}

Parameter Parser::parseParameter() {
	Parameter param;

	param.type = parseType();

	// "constant BufferClearParams &params" is a reference, which MSL allows in
	// place of a pointer for constant address space.
	if (at(TokenKind::Ampersand)) {
		advance();
		param.isConstReference = true;
	}

	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a parameter name, found " + std::string(tokenKindName(kind())));
	}

	param.name = std::string(advance().text);

	// "float values[4]" is an array suffix, "[[buffer(0)]]" is an attribute
	// list, and both start with a bracket, so the second one has to be
	// checked before the first is consumed.
	if (at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket) {
		advance();
		expect(TokenKind::RBracket, "to close an array parameter");
	}

	if (at(TokenKind::LBracket)) {
		param.attributes = parseParameterAttributes();
	}

	return param;
}

ParameterAttributes Parser::parseParameterAttributes() {
	ParameterAttributes attributes;

	expect(TokenKind::LBracket, "to open an attribute list");
	expect(TokenKind::LBracket, "to open an attribute");

	while (!at(TokenKind::EndOfFile)) {
		if (match(TokenKind::RBracket)) {
			break;
		}
		match(TokenKind::RBracket);

		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected an attribute name, found " + std::string(tokenKindName(kind())));
		}

		const std::string name(advance().text);

		std::optional<uint32_t> argument;
		if (at(TokenKind::LParen)) {
			advance();
			if (!at(TokenKind::IntegerLiteral)) {
				throw CompileError("attribute \"" + name + "\" needs a constant integer argument");
			}
			argument = static_cast<uint32_t>(advance().integerValue);
			expect(TokenKind::RParen, "to close an attribute argument");
		}

		if (name == "buffer") {
			if (!argument) {
				throw CompileError("[[buffer]] needs an index");
			}
			attributes.bufferIndex = argument;
		} else if (name == "texture") {
			if (!argument) {
				throw CompileError("[[texture]] needs an index");
			}
			attributes.textureIndex = argument;
		} else if (name == "sampler") {
			if (!argument) {
				throw CompileError("[[sampler]] needs an index");
			}
			attributes.samplerIndex = argument;
		} else if (name == "stage_in") {
			attributes.stageIn = true;
		} else if (auto builtin = builtinFromName(name)) {
			attributes.builtin = builtin;
		} else if (isMSLBuiltinName(name)) {
			throw CompileError("builtin attribute \"" + name + "\" is recognised but not "
				"supported yet; mslc reports it rather than ignoring it");
		} else {
			throw CompileError("unsupported attribute \"" + name + "\"");
		}

		match(TokenKind::Comma);
	}

	expect(TokenKind::RBracket, "to close an attribute list");

	return attributes;
}

FunctionDecl Parser::parseFunctionDeclaration(Stage stage) {
	FunctionDecl decl;
	decl.stage = stage;
	decl.line = line();

	decl.returnType = parseType();

	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a function name, found " + std::string(tokenKindName(kind())));
	}

	decl.name = std::string(advance().text);

	expect(TokenKind::LParen, "to open a parameter list");

	while (!at(TokenKind::RParen) && !at(TokenKind::EndOfFile)) {
		decl.parameters.push_back(parseParameter());
		if (!match(TokenKind::Comma)) {
			break;
		}
	}

	expect(TokenKind::RParen, "to close a parameter list");

	// parseCompoundStatement consumes the opening brace itself, so consuming it
	// here as well would make it look for a second one.
	decl.body = parseCompoundStatement();

	return decl;
}

StatementPtr Parser::parseCompoundStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::Compound;
	statement->line = line();

	expect(TokenKind::LBrace, "to open a block");

	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		statement->children.push_back(parseStatement());
	}

	expect(TokenKind::RBrace, "to close a block");

	return statement;
}

StatementPtr Parser::parseStatement() {
	auto statement = std::make_unique<Statement>();
	statement->line = line();

	if (at(TokenKind::LBrace)) {
		return parseCompoundStatement();
	}

	if (at(TokenKind::Semicolon)) {
		advance();
		statement->kind = StatementKind::ExpressionStatement;
		return statement;
	}

	if (atKeyword("if")) {
		return parseIfStatement();
	}

	if (atKeyword("for")) {
		return parseForStatement();
	}

	if (atKeyword("while")) {
		return parseWhileStatement();
	}

	if (atKeyword("return")) {
		return parseReturnStatement();
	}

	if (atKeyword("break") || atKeyword("continue") || atKeyword("discard_fragment")) {
		const std::string keyword(advance().text);
		statement->kind = keyword == "break" ? StatementKind::Break
			: keyword == "continue" ? StatementKind::Continue
			: StatementKind::Discard;
		expect(TokenKind::Semicolon, "after a jump statement");
		return statement;
	}

	// A declaration statement starts with a type. Distinguish it from an
	// expression by looking for a bare type keyword or a known address space
	// before anything that could begin an expression.
	{
		ScalarKind scalarKind;
		const bool looksLikeType =
			(kind() == TokenKind::Identifier && (isScalarTypeName(current().text, scalarKind)
				|| isTypeQualifier(current().text)))
			|| atKeyword("device") || atKeyword("constant")
			|| atKeyword("threadgroup") || atKeyword("thread");

		if (looksLikeType) {
			statement->kind = StatementKind::DeclarationStatement;
			VariableDeclaration declaration;
			declaration.type = parseType();

			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
			}
			declaration.name = std::string(advance().text);

			if (match(TokenKind::Assign)) {
				declaration.initializer = parseExpression();
			}

			expect(TokenKind::Semicolon, "after a declaration");
			statement->declaration = std::move(declaration);
			return statement;
		}
	}

	statement->kind = StatementKind::ExpressionStatement;
	statement->expression = parseExpression();
	expect(TokenKind::Semicolon, "after an expression statement");

	return statement;
}

StatementPtr Parser::parseIfStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::If;
	statement->line = line();

	expectKeyword("if", "at the start of an if");
	expect(TokenKind::LParen, "after 'if'");
	statement->expression = parseExpression();
	expect(TokenKind::RParen, "after an if condition");

	statement->thenBranch = parseStatement();

	if (matchIdentifier("else")) {
		statement->elseBranch = parseStatement();
	}

	return statement;
}

StatementPtr Parser::parseForStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::For;
	statement->line = line();

	expectKeyword("for", "at the start of a for");
	expect(TokenKind::LParen, "after 'for'");

	{
		ScalarKind scalarKind;
		const bool looksLikeType =
			(kind() == TokenKind::Identifier && (isScalarTypeName(current().text, scalarKind)
				|| isTypeQualifier(current().text)));

		if (looksLikeType) {
			VariableDeclaration declaration;
			declaration.type = parseType();
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a loop variable name in a for initialiser");
			}
			declaration.name = std::string(advance().text);
			if (match(TokenKind::Assign)) {
				declaration.initializer = parseExpression();
			}
			statement->forInitializer = std::move(declaration);
		} else {
			statement->expression = parseExpression();
		}
	}

	expect(TokenKind::Semicolon, "after a for initialiser");

	if (!at(TokenKind::Semicolon)) {
		statement->forCondition = parseExpression();
	}
	expect(TokenKind::Semicolon, "after a for condition");

	if (!at(TokenKind::RParen)) {
		statement->forIncrement = parseExpression();
	}
	expect(TokenKind::RParen, "after a for header");

	statement->forBody = parseStatement();

	return statement;
}

StatementPtr Parser::parseWhileStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::While;
	statement->line = line();

	expectKeyword("while", "at the start of a while");
	expect(TokenKind::LParen, "after 'while'");
	statement->whileCondition = parseExpression();
	expect(TokenKind::RParen, "after a while condition");

	statement->whileBody = parseStatement();

	return statement;
}

StatementPtr Parser::parseReturnStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::Return;
	statement->line = line();

	expectKeyword("return", "at the start of a return");

	if (!at(TokenKind::Semicolon)) {
		statement->expression = parseExpression();
	}

	expect(TokenKind::Semicolon, "after a return");

	return statement;
}

ExpressionPtr Parser::parseExpression() {
	return parseAssignment();
}

ExpressionPtr Parser::parseAssignment() {
	auto left = parseLogicalOr();

	if (at(TokenKind::Assign)) {
		advance();
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Assign;
		expression->line = left->line;
		expression->left = std::move(left);
		expression->right = parseAssignment();
		return expression;
	}

	return left;
}











ExpressionPtr Parser::parseLogicalOr() {
	return parseBinaryLevel([](Parser& p) { return p.parseLogicalAnd(); }, { { TokenKind::OrOr, BinaryOperator::LogicalOr } });
}

ExpressionPtr Parser::parseLogicalAnd() {
	return parseBinaryLevel([](Parser& p) { return p.parseBitwiseOr(); }, { { TokenKind::AndAnd, BinaryOperator::LogicalAnd } });
}

ExpressionPtr Parser::parseBitwiseOr() {
	return parseBinaryLevel([](Parser& p) { return p.parseBitwiseXor(); }, { { TokenKind::Pipe, BinaryOperator::BitOr } });
}

ExpressionPtr Parser::parseBitwiseXor() {
	return parseBinaryLevel([](Parser& p) { return p.parseBitwiseAnd(); }, { { TokenKind::Caret, BinaryOperator::BitXor } });
}

ExpressionPtr Parser::parseBitwiseAnd() {
	return parseBinaryLevel([](Parser& p) { return p.parseEquality(); }, { { TokenKind::Ampersand, BinaryOperator::BitAnd } });
}

ExpressionPtr Parser::parseEquality() {
	return parseBinaryLevel([](Parser& p) { return p.parseRelational(); }, { { TokenKind::Equal, BinaryOperator::Equal }, { TokenKind::NotEqual, BinaryOperator::NotEqual } });
}

ExpressionPtr Parser::parseRelational() {
	return parseBinaryLevel([](Parser& p) { return p.parseShift(); }, { { TokenKind::Less, BinaryOperator::Less }, { TokenKind::LessEqual, BinaryOperator::LessEqual },
		  { TokenKind::Greater, BinaryOperator::Greater }, { TokenKind::GreaterEqual, BinaryOperator::GreaterEqual } });
}

ExpressionPtr Parser::parseShift() {
	return parseBinaryLevel([](Parser& p) { return p.parseAdditive(); }, { { TokenKind::ShiftLeft, BinaryOperator::ShiftLeft }, { TokenKind::ShiftRight, BinaryOperator::ShiftRight } });
}

ExpressionPtr Parser::parseAdditive() {
	return parseBinaryLevel([](Parser& p) { return p.parseMultiplicative(); }, { { TokenKind::Plus, BinaryOperator::Add }, { TokenKind::Minus, BinaryOperator::Subtract } });
}

ExpressionPtr Parser::parseMultiplicative() {
	return parseBinaryLevel([](Parser& p) { return p.parseUnary(); }, { { TokenKind::Star, BinaryOperator::Multiply }, { TokenKind::Slash, BinaryOperator::Divide },
		  { TokenKind::Percent, BinaryOperator::Modulo } });
}

ExpressionPtr Parser::parseUnary() {
	if (at(TokenKind::Minus) || at(TokenKind::Plus) || at(TokenKind::Bang) || at(TokenKind::Tilde)
		|| at(TokenKind::Increment) || at(TokenKind::Decrement)) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Unary;
		expression->line = line();

		switch (kind()) {
			case TokenKind::Minus: expression->unaryOperator = UnaryOperator::Negate; break;
			case TokenKind::Plus: expression->unaryOperator = UnaryOperator::Plus; break;
			case TokenKind::Bang: expression->unaryOperator = UnaryOperator::Not; break;
			case TokenKind::Tilde: expression->unaryOperator = UnaryOperator::BitNot; break;
			case TokenKind::Increment: expression->unaryOperator = UnaryOperator::PreIncrement; break;
			default: expression->unaryOperator = UnaryOperator::PreDecrement; break;
		}

		advance();
		expression->left = parseUnary();
		return expression;
	}

	return parsePostfix();
}

ExpressionPtr Parser::parsePostfix() {
	auto expression = parsePrimary();

	while (true) {
		if (at(TokenKind::LBracket)) {
			advance();
			auto index = std::make_unique<Expression>();
			index->kind = ExpressionKind::Index;
			index->line = expression->line;
			index->left = std::move(expression);
			index->arguments.push_back(parseExpression());
			expect(TokenKind::RBracket, "to close an index");
			expression = std::move(index);
			continue;
		}

		if (at(TokenKind::Dot)) {
			advance();
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a member name after '.'");
			}
			auto member = std::make_unique<Expression>();
			member->kind = ExpressionKind::Member;
			member->line = expression->line;
			member->left = std::move(expression);
			member->memberName = std::string(advance().text);
			expression = std::move(member);
			continue;
		}

		if (at(TokenKind::LParen)) {
			advance();
			auto call = std::make_unique<Expression>();
			call->kind = ExpressionKind::Call;
			call->line = expression->line;
			call->left = std::move(expression);
			while (!at(TokenKind::RParen) && !at(TokenKind::EndOfFile)) {
				call->arguments.push_back(parseExpression());
				if (!match(TokenKind::Comma)) {
					break;
				}
			}
			expect(TokenKind::RParen, "to close an argument list");
			expression = std::move(call);
			continue;
		}

		return expression;
	}
}

ExpressionPtr Parser::parsePrimary() {
	if (at(TokenKind::IntegerLiteral)) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::IntLiteral;
		expression->line = line();
		expression->intValue = advance().integerValue;
		return expression;
	}

	if (at(TokenKind::FloatLiteral)) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::FloatLiteral;
		expression->line = line();
		expression->floatValue = advance().floatValue;
		return expression;
	}

	if (atKeyword("true") || atKeyword("false")) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::BoolLiteral;
		expression->line = line();
		expression->boolValue = atKeyword("true");
		advance();
		return expression;
	}

	if (at(TokenKind::LParen)) {
		advance();
		auto expression = parseExpression();
		expect(TokenKind::RParen, "to close a parenthesised expression");
		return expression;
	}

	if (kind() == TokenKind::Identifier) {
		std::string_view text = current().text;

		ScalarKind scalarKind;
		if (isScalarTypeName(text, scalarKind)) {
			// A type name in expression position is a cast: float(x), uint3(y).
			std::string_view name = advance().text;
			Type type;
			type.scalar = scalarKind;

			if (name.size() > 1) {
				const char last = name.back();
				if (last >= '2' && last <= '9') {
					type.vectorWidth = static_cast<uint32_t>(last - '0');
					if (!isScalarTypeName(name.substr(0, name.size() - 1), scalarKind)) {
						throw CompileError("unknown cast type \"" + std::string(name) + "\"");
					}
					type.scalar = scalarKind;
				}
			}

			if (!at(TokenKind::LParen)) {
				throw CompileError("expected '(' after cast type \"" + std::string(name) + "\"");
			}

			advance();
			auto expression = std::make_unique<Expression>();
			expression->kind = ExpressionKind::Cast;
			expression->line = line();
			expression->castType = type;
			expression->left = parseExpression();
			expect(TokenKind::RParen, "to close a cast");
			return expression;
		}

		// A name that is also an MSL builtin is a plain identifier here. Whether
		// it is the builtin or a parameter that shadows one is a question about
		// scope, which the parser has no answer for, so the emitter decides.
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Identifier;
		expression->line = line();
		expression->name = std::string(advance().text);
		return expression;
	}

	throw CompileError("unexpected " + std::string(tokenKindName(kind())) + " \""
		+ std::string(current().text) + "\" in an expression");
}

}
