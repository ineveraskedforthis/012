// main.cpp
#include"clang-c/Index.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>

// struct function {
// 	std::string symbol;
// 	std::vector<std::string> dependency;
// };

struct delayed_function_call {
	std::string function;
	std::vector<std::string> params;
};

struct cursor_data {
	std::string reference;
	std::vector<std::string> params;
	std::vector<std::string> inputs;
	std::vector<std::string> side_effects;
	std::vector<delayed_function_call> function_calls;
};

struct sequence_of_cursors {
	std::vector<cursor_data> data;
};

void handle_function (CXCursor cursor);

void parse_namespace_body() {

}

CXChildVisitResult handle_generic_cursor(CXCursor cursor, cursor_data* current_scope);


static std::map<std::string, cursor_data> effects_map;

cursor_data handle_function_call(CXCursor cursor) {
	CXString spelling = clang_getCursorDisplayName(cursor);
	auto shape = clang_getCString(spelling);
	std::cout << "Function call " << shape << "\n";
	clang_disposeString(spelling);

	cursor_data result {};
	sequence_of_cursors inputs {};

	clang_visitChildren(
		cursor,
		[](CXCursor current_cursor, CXCursor parent, CXClientData client_data) {
			sequence_of_cursors* data = (sequence_of_cursors*) client_data;
			cursor_data new_data {};
			auto res = handle_generic_cursor(current_cursor, &new_data);
			data->data.push_back(new_data);
			return res;
		},
		&inputs
	);

	for (auto& item : inputs.data) {
		for (auto& effect : item.side_effects) {
			result.side_effects.push_back(effect);
			std::cout << "Operation side effects: " << effect << "\n";
		}
		for (auto& input : item.inputs) {
			result.inputs.push_back(input);
			std::cout << "Operation input: " << input << "\n";
		}
	}
	return result;
}

cursor_data handle_binary_operaion(CXCursor cursor) {
	CXString spelling = clang_getCursorDisplayName(cursor);
	auto shape = clang_getCString(spelling);

	std::cout << "Operation " << shape << "\n";

	bool mutates_left = false;
	auto binary = clang_getCursorBinaryOperatorKind(cursor);
	if (
		binary == CXBinaryOperator_Assign
		|| binary == CXBinaryOperator_AddAssign
		|| binary == CXBinaryOperator_MulAssign
		|| binary == CXBinaryOperator_SubAssign
		|| binary == CXBinaryOperator_DivAssign
		|| binary == CXBinaryOperator_AndAssign
		|| binary == CXBinaryOperator_OrAssign
	) {
		mutates_left = true;
	}

	auto unary = clang_getCursorUnaryOperatorKind(cursor);
	if (unary == CXUnaryOperator_PostInc || CXUnaryOperator_PostDec || unary == CXUnaryOperator_PreDec || unary == CXUnaryOperator_PreInc) {
		mutates_left = true;
	}

	clang_disposeString(spelling);

	bool is_left = true;

	cursor_data result {};

	sequence_of_cursors inputs {};

	clang_visitChildren(
		cursor,
		[](CXCursor current_cursor, CXCursor parent, CXClientData client_data) {
			sequence_of_cursors* data = (sequence_of_cursors*) client_data;
			cursor_data new_data {};
			auto res = handle_generic_cursor(current_cursor, &new_data);
			data->data.push_back(new_data);
			return res;
		},
		&inputs
	);

	if (mutates_left) {
		result.side_effects.push_back(inputs.data[0].reference);
		std::cout << "Write to " << inputs.data[0].reference << "\n";
	}

	for (auto& item : inputs.data) {
		for (auto& effect : item.side_effects) {
			result.side_effects.push_back(effect);
			std::cout << "Operation side effects: " << effect << "\n";
		}
		for (auto& input : item.inputs) {
			result.inputs.push_back(input);
			std::cout << "Operation input: " << input << "\n";
		}
	}

	return result;
}

void handle_generic_cursor_children(CXCursor cursor, cursor_data* current_scope) {
	clang_visitChildren(
		cursor,
		[](
			CXCursor current_cursor,
			CXCursor parent,
			CXClientData client_data
		){
			return handle_generic_cursor(current_cursor, (cursor_data*)client_data);
		},
		current_scope
	);
}



CXChildVisitResult handle_generic_cursor(CXCursor cursor, cursor_data* current_scope) {

	CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	CXType cursor_type = clang_getCursorType(cursor);
	CXString cursor_spelling = clang_getCursorSpelling(cursor);
	CXString cursor_kind_spelling = clang_getCursorKindSpelling(cursor_kind);
	CXString kind_spelling = clang_getTypeKindSpelling(cursor_type.kind);
	CXString display = clang_getCursorDisplayName(cursor);
	std::cout
		<< "\t\tCursor "  << clang_getCString(cursor_spelling)
		<< " Display "  << clang_getCString(display)
		<< " CursorKind: " << clang_getCString(cursor_kind_spelling)
		<<" TypeKind: " << cursor_type.kind << " " << clang_getCString(kind_spelling)
		<< "\n";
	clang_disposeString(cursor_spelling);
	clang_disposeString(kind_spelling);
	clang_disposeString(cursor_kind_spelling);
	clang_disposeString(display);

	if (cursor_kind == CXCursor_TranslationUnit) {
		std::cout << "Enter TU\n";
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Continue;
	}
	if (cursor_kind == CXCursor_CompoundStmt) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Continue;
	}

	if (cursor_kind == CXCursor_Namespace) {
		std::cout << "Register namespace\n";
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Continue;
	}
	if (cursor_kind == CXCursor_FunctionDecl) {
		handle_function(cursor);
		return CXChildVisit_Continue;
	}
	if (cursor_kind == CXCursor_BinaryOperator || cursor_kind == CXCursor_UnaryOperator) {
		auto res = handle_binary_operaion(cursor);
		for (auto& side_effect : res.side_effects) {
			current_scope->side_effects.push_back(side_effect);
		}
		for (auto& input : res.inputs) {
			current_scope->inputs.push_back(input);
		}
		return CXChildVisit_Continue;
	}
	if (cursor_kind == CXCursor_VarDecl) {
		CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(cursor));
		std::cout << "Declare " << clang_getCString(pretty) << "\n";
		current_scope->reference = clang_getCString(pretty);
		clang_disposeString(pretty);
		return CXChildVisit_Continue;
	}
	if (cursor_kind == CXCursor_DeclRefExpr) {
		CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(cursor));
		std::cout << "Reference " << clang_getCString(pretty) << "\n";
		if (cursor_type.kind == CXType_FunctionProto) {
			delayed_function_call call {};
			call.function = clang_getCString(pretty);
			current_scope->function_calls.push_back(call);
		} else {
			current_scope->inputs.push_back(clang_getCString(pretty));
		}
		current_scope->reference = clang_getCString(pretty);
		clang_disposeString(pretty);
		return CXChildVisit_Continue;
	}

	if (cursor_kind == CXCursor_CallExpr) {
		auto res = handle_function_call(cursor);
		for (auto& side_effect : res.side_effects) {
			current_scope->side_effects.push_back(side_effect);
		}
		for (auto& input : res.inputs) {
			current_scope->inputs.push_back(input);
		}
		return CXChildVisit_Continue;
	}

	return CXChildVisit_Recurse;
}

void parse_body(CXCursor cursor, cursor_data current_scope) {
	clang_visitChildren(
		cursor,
		[](
			CXCursor current_cursor,
			CXCursor parent,
			CXClientData client_data
		){
			cursor_data* current_scope = (cursor_data*) client_data;

			CXString current_display_name = clang_getCursorDisplayName(current_cursor);
			auto current_cstr = clang_getCString(current_display_name);
			clang_disposeString(current_display_name);

			CXCursorKind cursor_kind = clang_getCursorKind(current_cursor);
			CXType cursor_type = clang_getCursorType(current_cursor);
			CXString cursor_spelling = clang_getCursorSpelling(current_cursor);
			CXString cursor_kind_spelling = clang_getCursorKindSpelling(cursor_kind);
			CXString kind_spelling = clang_getTypeKindSpelling(cursor_type.kind);
			std::cout << "Cursor "  << clang_getCString(cursor_spelling) << " CursorKind: " << clang_getCString(cursor_kind_spelling)  <<" TypeKind: " << cursor_type.kind << " " << clang_getCString(kind_spelling);
			clang_disposeString(cursor_spelling);
			clang_disposeString(kind_spelling);
			clang_disposeString(cursor_kind_spelling);

			CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(current_cursor));
			std::cout << "\n" << "Pretty " << clang_getCString(pretty) << "\n";
			clang_disposeString(pretty);

			CXSourceRange cursor_range = clang_getCursorExtent(current_cursor);
			CXFile file;
			unsigned start_line, start_column, start_offset;
			unsigned end_line, end_column, end_offset;
			clang_getExpansionLocation(clang_getRangeStart(cursor_range), &file, &start_line, &start_column, &start_offset);
			clang_getExpansionLocation(clang_getRangeEnd  (cursor_range), &file, &end_line  , &end_column  , &end_offset);

			std::cout <<" spanning lines " << start_line <<" to " << end_line;


			std::cout <<"\n";

			if (cursor_type.kind == CXType_Invalid) {
				return CXChildVisit_Recurse;
			}

			return CXChildVisit_Continue;
		},
		&current_scope
	);
}

void handle_function (CXCursor cursor) {
	cursor_data function_effects {};

	clang_visitChildren(
		cursor,
		[](CXCursor current_cursor, CXCursor parent, CXClientData client_data) {
			cursor_data* data = (cursor_data*) client_data;

			CXCursorKind cursor_kind = clang_getCursorKind(current_cursor);
			CXString usr = clang_getCursorUSR(current_cursor);

			if (cursor_kind == CXCursor_ParmDecl) {
				data->inputs.push_back(clang_getCString(usr));
			}

			clang_disposeString(usr);

			if (cursor_kind == CXCursor_ParmDecl) {
				return CXChildVisit_Continue;
			} else {
				handle_generic_cursor(current_cursor, data);
				return CXChildVisit_Continue;
			}
		},
		&function_effects
	);

	CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(cursor));
	std::cout << "\n\nRegister function " << clang_getCString(pretty) << "\n";
	for (auto& side_effect : function_effects.side_effects) {
		std::cout << "Influences " << side_effect << "\n";
	}

	CXString current_display_name = clang_getCursorSpelling(cursor);
	std::string display_string = clang_getCString(current_display_name);
	// clang_disposeString(current_display_name);
	std::cout << display_string << "\n";
	for (auto& dep : function_effects.inputs) {
		if (dep.find(display_string) == std::string::npos)
			std::cout << "Depends externally on " << dep << "\n";
		// else
			// std::cout << "Depends internally on " << dep << "\n";
	}
	effects_map[clang_getCString(pretty)] = function_effects;
	clang_disposeString(pretty);
}


int main(){
	CXIndex index = clang_createIndex(0, 0); //Create index
	CXTranslationUnit unit;

	std::vector<const char*> flags {"clang++", "-std=c++23"};

	clang_parseTranslationUnit2FullArgv(
		index,
		"test_function.cpp",
		flags.data(), flags.size(),
		nullptr, 0,
		CXTranslationUnit_None,
		&unit
	);

	if (unit == nullptr){
		std::cerr <<"Unable to parse translation unit. Quitting.\n";
		return 0;
	}

	auto diag_count = clang_getNumDiagnostics(unit);

	for (unsigned int i = 0; i < diag_count; i++) {
		auto  diag = clang_getDiagnostic(unit, i);
		auto text = clang_getDiagnosticSpelling(diag);
		std::cout << clang_getCString(text) <<"\n";
		clang_disposeString(text);
	}

	CXCursor cursor = clang_getTranslationUnitCursor(unit); //Obtain a cursor at the root of the translation unit

	cursor_data cs{};
	handle_generic_cursor(cursor, &cs);

}