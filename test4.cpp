#include"clang-c/Index.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <map>

#include <windows.h>

void pretty_print(CXCursor cursor, std::string indent) {
	CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	CXType cursor_type = clang_getCursorType(cursor);
	CXString cursor_spelling = clang_getCursorSpelling(cursor);
	CXString cursor_kind_spelling = clang_getCursorKindSpelling(cursor_kind);
	CXString kind_spelling = clang_getTypeKindSpelling(cursor_type.kind);
	CXString display = clang_getCursorDisplayName(cursor);
	CXString display_ref = clang_getCursorDisplayName(clang_getCursorReferenced(cursor));
	std::cout
		<< indent
		<< "Cursor "  << clang_getCString(cursor_spelling)
		<< " Display "  << clang_getCString(display)
		<< " Display Ref "  << clang_getCString(display_ref)
		<< " CursorKind: " << clang_getCString(cursor_kind_spelling)
		<< " TypeKind: " << cursor_type.kind << " " << clang_getCString(kind_spelling)
		<< "\n";
	{
		CXString pretty = clang_getCursorUSR (cursor);
		std::cout << indent << "USR " << clang_getCString(pretty) << "\n";
		clang_disposeString(pretty);
	}
	clang_disposeString(cursor_spelling);
	clang_disposeString(kind_spelling);
	clang_disposeString(cursor_kind_spelling);
	clang_disposeString(display);
	clang_disposeString(display_ref);
}

struct point {
	unsigned int hash;
	std::string name;
	std::string display_name;
	bool is_param = false;
	bool is_reference = false;
	bool is_pointer = false;
	int reference_to = -1;
	bool is_function_call = false;
	bool is_projection = false;
	bool is_this = false;
	bool mutated = false;
	// bool is_pointer;
	// unsigned int line;

	// -1 if doesn't point to flow
	std::string associated_flow_name = "";
	// unsigned int associated_flow_hash = -1;

	std::vector<int> computation {};
	std::vector<int> dependency {};

	// -1 means unknown
	int defined_in_flow = -1;

	// std::vector<int> projections;
};

struct flow {
	unsigned int hash;
	std::string name;
	unsigned int line;

	bool is_lambda = false;

	std::vector<int> local_parameters {};
	int parameters_count = 0;
	int return_value = -1;
	// bool freeze = false;
};

struct code {
	std::vector<point> points {};
	std::vector<flow> flows {};
	std::vector<unsigned int> flow_stack;
	std::vector<point> call_stack;
};

std::string
mild(
	std::string name
) {
	std::string result = "";
	for (int i = 0; i < name.length(); i++) {
		if (name[i] == '>') {
			result += "gt";
		} else if (name[i] == '<') {
			result += "lt";
		} else if (name[i] == '%') {
			result += "pt";
		} else {
			result += name[i];
		}
	}
	return result;
}

std::string
ninja_name(
	std::string name
) {
	std::string result = "";
	for (int i = 0; i < name.length(); i++) {
		if (
			name[i] == ':' || name[i] == '$'
			|| name[i] == ' ' || name[i] == '|'
			|| name[i] == '#' || name[i] == '@'
			|| name[i] == '('
			|| name[i] == ')'
			|| name[i] == ' '
		) {
			result += "_";
		}
		else if (name[i] == '.') {
			result += "dot";
		} else if (name[i] == '*') {
			result += "star";
		} else if (name[i] == '!') {
			result += "excl";
		} else if (name[i] == '+') {
			result += "plus";
		} else if (name[i] == '-') {
			result += "minus";
		} else if (name[i] == '^') {
			result += "caret";
		} else if (name[i] == '>') {
			result += "gt";
		} else if (name[i] == '<') {
			result += "lt";
		} else if (name[i] == '=') {
			result += "eq";
		} else if (name[i] == '&') {
			result += "amp";
		} else if (name[i] == '/') {
			result += "div";
		} else if (name[i] == '~') {
			result += "tilde";
		} else  {
			result += name[i];
		}
	}
	return result;
}

// we want
// c:@S@stupid_struct>#f@FI@data
// c:@ST>1#T@stupid_struct@FI@data
// to be the same:
// c:@stupid_struct@FI@data
std::string sanitize_usr(std::string input_usr) {
	bool at_found = false;
	bool second_at_found = false;
	bool templates_count_parsed = false;
	bool gt_found = false;
	bool skip_until_at_or_sharp = false;
	std::string result;
	for (auto i = 0; i < input_usr.size(); i++) {
		if (input_usr[i] == '@') {
			if (!at_found) {
				at_found = true;
				continue;
			} else if (!second_at_found) {
				second_at_found = true;
				continue;
			}
		}
		if (input_usr[i] == '@' || input_usr[i] == '#') {
			skip_until_at_or_sharp = false;
			continue;
		}
		if (at_found && !second_at_found) {
			continue;
		}
		if (input_usr[i] == '>') {
			skip_until_at_or_sharp = true;
		}
		if (skip_until_at_or_sharp) {
			continue;
		}
		result += input_usr[i];
	}

	if (input_usr != "") assert(result != "");

	return result;
}

CXChildVisitResult handle_generic_cursor(CXCursor cursor, code* current_scope) ;

void handle_generic_cursor_children(CXCursor cursor, code* current_scope) {
	clang_visitChildren(
		cursor,
		[](
			CXCursor current_cursor,
			CXCursor parent,
			CXClientData client_data
		){
			handle_generic_cursor(current_cursor, (code*)client_data);
			return CXChildVisit_Continue;
		},
		current_scope
	);
}


int last_mention(code* current_scope, std::string name_to_find) {
	auto stack_layer =  (int)current_scope->flow_stack.size() - 1;
	bool found = false;
	while (
		!found
		&& stack_layer >= 0
	) {
		auto& layer = current_scope->flows[current_scope->flow_stack[stack_layer]];
		auto end = (int)layer.local_parameters.size() - 1;
		while (
			!found
			&& end >= 0
		) {
			auto existing_point_index = layer.local_parameters[end];
			auto existing_point = current_scope->points[existing_point_index];
			if (existing_point.name == name_to_find) {
				return  existing_point_index;
			}
			end--;
		}
		stack_layer--;
	}
	return -1;
}


CXChildVisitResult handle_generic_cursor(CXCursor cursor, code* current_scope) {

	CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	CXType cursor_type = clang_getCursorType(cursor);

	// pretty_print(cursor, "\t\t");

	CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(cursor));
	// std::cout << "References: " << clang_getCString(pretty) << "\n";
	std::string usr {sanitize_usr(clang_getCString(pretty))};
	// std::cout << "References(sanitized): " << usr << "\n";
	clang_disposeString(pretty);

	CXString display = clang_getCursorDisplayName(cursor);
	std::string display_name {clang_getCString(display)};
	display_name = mild(display_name);
	clang_disposeString(display);

	auto hash = clang_hashCursor(cursor);

	if (cursor_kind == CXCursor_StaticAssert) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UnaryExpr) {
		return CXChildVisit_Break;
	}

	if  (cursor_kind == CXCursor_ParmDecl) {
		assert(current_scope->flow_stack.size() > 0);
		auto& function = current_scope->flows[current_scope->flow_stack.back()];

		point parameter {};
		parameter.name = usr;
		parameter.display_name = display_name;
		parameter.hash = hash;

		if (cursor_type.kind == CXType_LValueReference) {
			parameter.is_reference = true;
		}

		function.local_parameters.push_back(current_scope->points.size());
		function.parameters_count++;
		parameter.is_param = true;
		current_scope->points.push_back(parameter);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_LambdaExpr) {
		// we start making a new flow chart

		flow lambda_flow {};
		lambda_flow.hash = clang_hashCursor(cursor);
		lambda_flow.name = "Lambda" + std::to_string(lambda_flow.hash);
		lambda_flow.is_lambda = true;

		current_scope->flow_stack.push_back(current_scope->flows.size());
		current_scope->flows.push_back(lambda_flow);

		handle_generic_cursor_children(cursor, current_scope);

		current_scope->flow_stack.pop_back();

		point associated_point {};
		associated_point.name = "LambdaReference";
		associated_point.associated_flow_name = lambda_flow.name;
		associated_point.hash = lambda_flow.hash;
		associated_point.display_name = "Lambda";
		current_scope->points.push_back(associated_point);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UsingDeclaration) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UsingDirective) {
		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_FunctionDecl
		|| cursor_kind == CXCursor_FunctionTemplate
	) {
		// we start making a new flow chart
		flow lambda_flow {};
		lambda_flow.hash = clang_hashCursor(cursor);
		lambda_flow.name = usr;
		current_scope->flow_stack.push_back(current_scope->flows.size());
		current_scope->flows.push_back(lambda_flow);
		handle_generic_cursor_children(cursor, current_scope);
		current_scope->flow_stack.pop_back();
		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_FieldDecl
	) {
		// we treat fields as "projection" functions
		flow lambda_flow {};
		lambda_flow.hash = clang_hashCursor(cursor);
		lambda_flow.name = usr;

		point parameter_this {};
		parameter_this.name = "@THIS";
		parameter_this.is_reference = true;
		parameter_this.hash = hash;
		parameter_this.is_this = true;
		parameter_this.display_name = "this";

		lambda_flow.local_parameters.push_back(current_scope->points.size());
		lambda_flow.parameters_count++;
		parameter_this.is_param = true;
		current_scope->points.push_back(parameter_this);

		current_scope->flows.push_back(lambda_flow);

		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_CXXMethod
	) {
		flow lambda_flow {};
		lambda_flow.hash = clang_hashCursor(cursor);
		lambda_flow.name = usr;

		point parameter_this {};
		parameter_this.name = "@THIS";
		parameter_this.is_reference = true;
		parameter_this.hash = hash;
		parameter_this.is_this = true;
		parameter_this.display_name = "this";

		lambda_flow.local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(parameter_this);

		current_scope->flow_stack.push_back(current_scope->flows.size());
		current_scope->flows.push_back(lambda_flow);

		handle_generic_cursor_children(cursor, current_scope);

		current_scope->flow_stack.pop_back();

		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_TypeRef
	) {
		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_OverloadedDeclRef
		|| cursor_kind == CXCursor_ConceptSpecializationExpr
	) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_DeclRefExpr) {
		// handle_generic_cursor_children(cursor, current_scope);
		auto original_template = clang_getSpecializedCursorTemplate(clang_getCursorReferenced(cursor));
		if (!clang_Cursor_isNull(original_template)) {
			CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(original_template));
			usr = sanitize_usr(clang_getCString(pretty));
			clang_disposeString(pretty);
		}

		point core {};

		if (usr == "") {
			core.name = "Nameless";
		} else {
			core.name = usr;
		}

		if (display_name == "") {
			core.display_name = "None";
		} else {
			core.display_name = display_name;
		}

		core.hash = clang_hashCursor(cursor);

		if (
			cursor_type.kind == CXType_Pointer
		) {
			core.is_pointer = true;
		} if (
			cursor_type.kind == CXType_LValueReference
		) {
			core.is_reference = true;
		}


		int found_index = last_mention(current_scope, core.name);
		bool resolved = true;
		if (found_index != -1) {
			auto origin = current_scope->points[found_index];
			if (origin.is_reference) {
				found_index = origin.reference_to;
				resolved = false;
			}
		}

		if (!resolved && found_index != -1) {
			core.dependency.push_back(found_index);
			auto referenced = current_scope->points[found_index];
			found_index = last_mention(current_scope, referenced.name);
			core.name = referenced.name;
			core.defined_in_flow = referenced.defined_in_flow;
			core.display_name = referenced.display_name;
			auto scope = current_scope->flow_stack.back();
			current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
			current_scope->points.push_back(core);
			return CXChildVisit_Break;
		} else  {

			if (found_index != -1) {
				auto referenced = current_scope->points[found_index];
				core.dependency.push_back(found_index);
				core.defined_in_flow = referenced.defined_in_flow;
			}
			auto scope = current_scope->flow_stack.back();
			current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
			current_scope->points.push_back(core);
			return CXChildVisit_Break;
		}
	}

	if (cursor_kind == CXCursor_NamespaceRef || cursor_kind == CXCursor_DeclStmt) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ConceptDecl) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UnexposedAttr) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TypedefDecl) {
		return CXChildVisit_Break;
	}

	if (cursor_kind ==  CXCursor_CXXBaseSpecifier || cursor_kind == CXCursor_CXXAccessSpecifier) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CStyleCastExpr) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXConstCastExpr) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXStaticCastExpr) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_BuiltinBitCastExpr) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind ==CXCursor_CXXDynamicCastExpr) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXReinterpretCastExpr) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXFunctionalCastExpr) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_RequiresExpr) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_BreakStmt) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_FriendDecl) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXThrowExpr) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_PackExpansionExpr) {
		// todo
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ParenExpr) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_LinkageSpec) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_NullStmt) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_VarDecl) {
		// handle the right side
		handle_generic_cursor_children(cursor, current_scope);

		// now the product in the right side is ready at the last position
		// prepare the thing
		point declared {};
		declared.name = usr;
		declared.display_name = display_name;
		declared.hash = clang_hashCursor(cursor);
		declared.dependency.push_back(current_scope->points.size() - 1);

		declared.defined_in_flow = current_scope->flow_stack.back();

		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(declared);

		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_UnexposedExpr
		|| cursor_kind == CXCursor_CompoundStmt
		|| cursor_kind == CXCursor_Namespace
		|| cursor_kind == CXCursor_TranslationUnit
		|| cursor_kind == CXCursor_DefaultStmt
		|| cursor_kind == CXCursor_CaseStmt
		|| cursor_kind == CXCursor_ClassTemplate
		|| cursor_kind == CXCursor_ClassDecl
		|| cursor_kind == CXCursor_StructDecl
		|| cursor_kind == CXCursor_InitListExpr
		|| cursor_kind == CXCursor_DoStmt
		|| cursor_kind == CXCursor_WhileStmt
		|| cursor_kind == CXCursor_NamespaceRef
		|| cursor_kind == CXCursor_NamespaceAlias
		|| cursor_kind == CXCursor_CXXTryStmt
		|| cursor_kind == CXCursor_CXXCatchStmt
	) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ConversionFunction) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UnexposedStmt) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXDeleteExpr) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TemplateRef) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TemplateTemplateParameter) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXThisExpr) {
		point local_this {};
		local_this.name = "@THIS";
		local_this.is_reference = true;
		local_this.hash = hash;
		local_this.is_this = true;
		local_this.display_name = "this";

		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(local_this);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXNewExpr) {
		point new_value {};
		new_value.name = "NEW";
		new_value.hash = hash;
		new_value.display_name = "new";
		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(new_value);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXNullPtrLiteralExpr) {
		point new_value {};
		new_value.name = "NULL";
		new_value.hash = hash;
		new_value.display_name = "nullptr";
		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(new_value);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_SizeOfPackExpr) {
		point new_value {};
		new_value.name = "SIZE_OF";
		new_value.display_name = "size of something";
		new_value.hash = hash;
		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(new_value);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_VariableRef) {
		return CXChildVisit_Break;
	}


	if (cursor_kind == CXCursor_ContinueStmt) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CXXTypeidExpr) {
		return CXChildVisit_Break;
	}

	if(cursor_kind == CXCursor_CXXBoolLiteralExpr) {
		auto data = clang_Cursor_Evaluate(cursor);
		auto data_int = clang_EvalResult_getAsInt(data);

		point new_value {};
		if (data_int) {
			new_value.name = "LITERALLY_TRUE";
			new_value.display_name = "true";
		} else {
			new_value.name = "LITERALLY_FALSE";
			new_value.display_name = "false";
		}
		new_value.hash = hash;
		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(new_value);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_IntegerLiteral) {
		auto data = clang_Cursor_Evaluate(cursor);
		auto data_int = clang_EvalResult_getAsInt(data);

		point new_value {};
		new_value.name = "LITERALLY_" + std::to_string(data_int);
		new_value.display_name = std::to_string(data_int);
		new_value.hash = hash;
		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(new_value);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_FloatingLiteral) {
		auto data = clang_Cursor_Evaluate(cursor);
		auto data_int = clang_EvalResult_getAsDouble(data);

		point new_value {};
		new_value.name = "LITERALLY_" + std::to_string(data_int);
		new_value.display_name = std::to_string(data_int);
		new_value.hash = hash;
		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(new_value);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_StringLiteral) {
		point new_value {};
		new_value.name = "LITERALLY_STRING";
		new_value.hash = hash;
		new_value.display_name = "string";
		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(new_value);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CharacterLiteral) {
		point new_value {};
		new_value.name = "LITERALLY_CHARACTER";
		new_value.hash = hash;
		new_value.display_name = "character";
		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(new_value);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TemplateTypeParameter) {
		return CXChildVisit_Break;
	}



	if (cursor_kind == CXCursor_ClassTemplatePartialSpecialization) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_NonTypeTemplateParameter) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TypeAliasDecl) {
		return CXChildVisit_Break;
	}
	if (cursor_kind == CXCursor_TypeAliasTemplateDecl) {
		return CXChildVisit_Break;
	}

	if (
		clang_isAttribute(cursor_kind)
	) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_Constructor || cursor_kind == CXCursor_Destructor) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_EnumDecl || cursor_kind == CXCursor_EnumConstantDecl) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_MemberRef) {
		return CXChildVisit_Break;
	}

	if(
		cursor_kind == CXCursor_CallExpr
		&& cursor_type.kind == CXType_Unexposed
	) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CallExpr) {
		auto original_template = clang_getSpecializedCursorTemplate(clang_getCursorReferenced(cursor));
		if (!clang_Cursor_isNull(original_template)) {
			CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(original_template));
			usr = sanitize_usr(clang_getCString(pretty));
			clang_disposeString(pretty);
		}

		// function call has projections to function and params
		point function_call {};
		function_call.hash = hash;
		function_call.name = usr;
		function_call.display_name = display_name;
		function_call.is_function_call = true;
		current_scope->call_stack.push_back(function_call);

		// {
		// 	auto scope = current_scope->flow_stack.back();
		// 	current_scope->flows[scope].freeze = true;
		// }

		// add all children as projections
		// manually
		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto scope = (code*) client_data;
				handle_generic_cursor(current_cursor, (code*)client_data);
				auto& call = scope->call_stack.back();
				call.computation.push_back(scope->points.size() - 1);
				return CXChildVisit_Continue;
			},
			current_scope
		);

		auto& updated_call = current_scope->call_stack.back();
		updated_call.associated_flow_name = usr;

		// pop the stack to points
		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(updated_call);
		current_scope->call_stack.pop_back();

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ArraySubscriptExpr) {
		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto scope = (code*) client_data;
				CXType cursor_type = clang_getCursorType(current_cursor);
				if (cursor_type.kind == CXType_Pointer) {
					handle_generic_cursor(current_cursor, (code*)client_data);
				}
				return CXChildVisit_Continue;
			},
			current_scope
		);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_MemberRefExpr) {
		auto original_template = clang_getSpecializedCursorTemplate(clang_getCursorReferenced(cursor));
		if (!clang_Cursor_isNull(original_template)) {
			CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(original_template));
			usr = sanitize_usr(clang_getCString(pretty));
			clang_disposeString(pretty);
		}

		// function call has projections to function and params
		point member_projection {};
		member_projection.hash = hash;
		member_projection.name = usr;
		member_projection.display_name = display_name;
		member_projection.is_projection = true;
		current_scope->call_stack.push_back(member_projection);

		// add all children as projections
		// manually

		// {
		// 	auto scope = current_scope->flow_stack.back();
		// 	current_scope->flows[scope].freeze = true;
		// }

		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto scope = (code*) client_data;
				handle_generic_cursor(current_cursor, (code*)client_data);

				auto& call = scope->call_stack.back();
				call.computation.push_back(scope->points.size() - 1);

				return CXChildVisit_Continue;
			},
			current_scope
		);

		// pop the stack to points

		// {
		// 	auto scope = current_scope->flow_stack.back();
		// 	current_scope->flows[scope].freeze = false;
		// }
		auto& updated_call = current_scope->call_stack.back();

		if (updated_call.computation.size() == 0) {
			point parameter_this {};
			parameter_this.name = "@THIS";
			parameter_this.is_reference = true;
			parameter_this.hash = hash;
			parameter_this.is_this = true;
			parameter_this.display_name = "this";
			updated_call.computation.push_back(current_scope->points.size());
			current_scope->points.push_back(parameter_this);
		}

		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(updated_call);
		current_scope->call_stack.pop_back();

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ReturnStmt) {
		assert(current_scope->flow_stack.size() > 0);
		auto& function_index = current_scope->flow_stack.back();

		point return_point {};
		return_point.hash = hash;
		return_point.name = "RETURN";
		return_point.display_name = "return";

		handle_generic_cursor_children(cursor, current_scope);

		return_point.dependency.push_back(current_scope->points.size() - 1);
		auto scope = current_scope->flow_stack.back();
		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(return_point);
		current_scope->flows[scope].return_value = current_scope->points.size() - 1;
		return CXChildVisit_Break;
	}

	/*
	if (
		cursor_kind == CXCursor_UnaryOperator
		&& clang_getCursorUnaryOperatorKind(cursor) == CXUnaryOperator_Deref
	) {
		handle_generic_cursor_children(cursor, current_scope);
		// auto latest_assignment = last_mention(current_scope, std::string name_to_find)
	}
	*/

	if (
		cursor_kind == CXCursor_UnaryOperator
	) {
		bool mutates = false;
		auto binary = clang_getCursorUnaryOperatorKind(cursor);
		if (
			binary == CXUnaryOperator_PostDec
			|| binary == CXUnaryOperator_PostInc
		) {
			mutates = true;
		}

		handle_generic_cursor_children(cursor, current_scope);

		point unary_node {};
		unary_node.hash = hash;
		unary_node.name = "UNARY";
		unary_node.dependency.push_back(current_scope->points.size() - 1);
		auto str = clang_getUnaryOperatorKindSpelling(binary);
		unary_node.display_name = clang_getCString(str);
		unary_node.display_name = mild(unary_node.display_name);
		clang_disposeString(str);
		auto scope = current_scope->flow_stack.back();

		if (mutates) {
			handle_generic_cursor_children(cursor, current_scope);
			current_scope->points.back().dependency.push_back(current_scope->points.size());
			current_scope->points.back().mutated = true;
		}

		current_scope->flows[scope].local_parameters.push_back(current_scope->points.size());
		current_scope->points.push_back(unary_node);

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UnionDecl) {
		return CXChildVisit_Break;
	}

	if(cursor_kind == CXCursor_UnexposedDecl) {
		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_BinaryOperator
		|| cursor_kind == CXCursor_CompoundAssignOperator
		|| cursor_kind == CXCursor_IfStmt
		|| cursor_kind == CXCursor_ForStmt
		|| cursor_kind == CXCursor_CXXForRangeStmt
		|| cursor_kind == CXCursor_SwitchStmt
		|| cursor_kind == CXCursor_ConditionalOperator
	) {
		bool mutates_left = false;
		bool left_is_input = true;
		auto binary = clang_getCursorBinaryOperatorKind(cursor);
		if (
			binary == CXBinaryOperator_Assign
			|| binary == CXBinaryOperator_AddAssign
			|| binary == CXBinaryOperator_MulAssign
			|| binary == CXBinaryOperator_SubAssign
			|| binary == CXBinaryOperator_DivAssign
			|| binary == CXBinaryOperator_AndAssign
			|| binary == CXBinaryOperator_OrAssign
			|| cursor_kind == CXCursor_CompoundAssignOperator
			|| cursor_kind == CXCursor_VarDecl
		) {
			mutates_left = true;
		}

		if (binary == CXBinaryOperator_Assign || cursor_kind == CXCursor_VarDecl ) {
			left_is_input = false;
		}

		point binary_op {};
		binary_op.name = "BINARY";
		if (cursor_kind == CXCursor_BinaryOperator) {
			auto str = clang_getBinaryOperatorKindSpelling(binary);
			binary_op.display_name = clang_getCString(str);
			binary_op.display_name = mild(binary_op.display_name);
			clang_disposeString(str);
		} else {
			binary_op.display_name = "operation";
		}
		binary_op.hash = hash;
		current_scope->call_stack.push_back(binary_op);

		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto scope = (code*) client_data;
				handle_generic_cursor(current_cursor, (code*)client_data);

				auto& call = scope->call_stack.back();
				call.computation.push_back(scope->points.size() - 1);

				return CXChildVisit_Continue;
			},
			current_scope
		);

		auto& back = current_scope->call_stack.back();
		auto left = back.computation[0];

		if (!left_is_input) {
			back.computation.erase(back.computation.begin());
		}


		auto flow_index = current_scope->flow_stack.back();
		auto& flow = current_scope->flows[flow_index];

		if (mutates_left) {
			// create a new left node:
			auto copied_left = current_scope->points[left];
			copied_left.mutated = true;
			copied_left.dependency.clear();
			flow.local_parameters.push_back(current_scope->points.size());
			current_scope->points.push_back(copied_left);
			current_scope->points.back().dependency.push_back(current_scope->points.size());
		}

		back.dependency = back.computation;
		flow.local_parameters.push_back(current_scope->points.size());

		current_scope->points.push_back(back);
		current_scope->call_stack.pop_back();

		return CXChildVisit_Break;
	}

	// std::cout << "end: " << usr << "\n";
	pretty_print(cursor, "\t\t");
	std::cout << "UNRESOLVED CASE\n";

	handle_generic_cursor_children(cursor, current_scope);

	return CXChildVisit_Recurse;
}

std::string get_final_name (point& item) {
	return ninja_name(item.name)
		+ "_"
		+ std::to_string(item.hash)
		+(item.mutated ? "_MUTATED" : "");
};

static std::map<std::string, int> simplifier;
static std::map<std::string, std::string> last_node;


static int available_int = 0;
static int unique_prefix = 0;

std::string unique_form (code& code_db, std::map<std::string, std::string>& result, point& item, bool left_part, std::string prefix) {
	std::string final_string = get_final_name(item);
	std::string result_string;
	result_string = final_string;
	if (item.computation.size() > 0) {
		result_string += "_COMPUTED_WITH_";
		for (auto part_index : item.computation) {
			auto part = code_db.points[part_index];
			result_string += ninja_name(part.name) +  "_" + std::to_string(part.hash);
		}
	}
	return prefix + "i" + result_string;
}

std::string print_point (code& code_db, std::map<std::string, std::string>& result, point& item, bool left_part, std::string prefix) {
	std::string final_string = get_final_name(item);
	std::string result_string;
	result_string = final_string;
	if (item.computation.size() > 0) {
		result_string += "_COMPUTED_WITH_";
		for (auto part_index : item.computation) {
			auto part = code_db.points[part_index];
			result_string += ninja_name(part.name) +  "_" + std::to_string(part.hash);
		}
	}
	if (left_part)
		result[item.name] = prefix + "i" + result_string;

	std::cout << prefix << "i" << result_string;

	/*
	auto it = simplifier.find(result_string);
	if (it == simplifier.end()) {
		simplifier[result_string] = available_int;
		available_int++;
		std::cout << prefix << "i" << available_int;
	} else {
		std::cout << prefix << "i"  << it->second;
	}
	*/
	// std::cout << result_string;
	return prefix + "i" + result_string;
};


static std::map<std::string, bool> registered_nodes;
void register_node(code& code_db, std::map<std::string, std::string>& result, point& item, std::string id, std::string prefix) {
	auto it = registered_nodes.find(id);
	if (it == registered_nodes.end()) {
		registered_nodes[id] = true;
		std::cout << "\"";
		print_point(code_db, result, item, false, prefix);
		std::cout << "\"";
		std::cout << "[ shape = \"record\" label = \"" << item.display_name << "\" ]";
		std::cout << "\n";
	}
}

/*

subgraph cluster_0 {
	style=filled;
	color= ;
	node [style=filled,color=white];
	a0 -> a1 -> a2 -> a3;
	label = "process #1";
}
*/

static std::vector<std::pair<std::string, std::string>> delayed_edges;

void print_flow (
	code& code_db,
	std::map<std::string, std::string>& result,
	flow& flow,
	std::vector<int> flow_chain,
	std::vector<std::string> prefix_chain,
	std::map<std::string, int>& substitution,
	int this_replacement
){

	std::cout << "\nsubgraph cluster_" << prefix_chain.back() << "{\n";
	std::cout << "style=filled;\ncolor=\"#4040404d\";\nnode [style=filled,color=white];\n";
	std::cout << "label = " << "\"Flow " << ninja_name(flow.name) << "\"\n";

	int param_index = 0;

	for (auto raw_index : flow.local_parameters) {
		int replaced_index = raw_index;
		if (code_db.points[raw_index].is_this && this_replacement != -1) {
			replaced_index = this_replacement;
		}
		auto& item = code_db.points[replaced_index];

		// auto& item_content = item;
		// // if (substitution.find(item_index) != substitution.end()) {
		// 	item_content = code_db.points[substitution[item_index]];
		// }

		int actual_layer = flow_chain.size() - 1;
		int origin_layer = flow_chain.size() - 1;

		bool side_effect = false;

		if (item.defined_in_flow != -1) {
			side_effect = flow_chain[actual_layer] != item.defined_in_flow;
			while (origin_layer >= 0) {
				if (flow_chain[origin_layer] == item.defined_in_flow) {
					break;
				}
				origin_layer--;
			}
		}

		std::string final_string = prefix_chain[actual_layer] + get_final_name(item);

		auto already_used = result.find(item.name);
		if (side_effect && already_used != result.end()) {
			// delayed_edges.push_back({already_used->second, unique_form(code_db, result, item, true, prefix_chain[actual_layer])});
			// std::cout << already_used->second << " -> ";
			// print_point(code_db, result, item, true, prefix_chain[actual_layer]);
			// std::cout << "\n";
		}


		// std::cout << "build ";
		// print_point(code_db, result, item, true, prefix);
		// std::cout << " : compute ";
		for (auto dep_index : item.dependency) {
			auto& dep = code_db.points[dep_index];
			auto s1 = print_point(code_db, result, dep, false, prefix_chain[actual_layer]);
			std::cout << " -> ";
			auto s2 = print_point(code_db, result, item, true, prefix_chain[actual_layer]);
			std::cout << "\n";
			register_node(code_db, result, dep, s1, prefix_chain[actual_layer]);
			register_node(code_db, result, item, s2, prefix_chain[actual_layer]);
		}

		if (item.is_param) {
			std::cout << prefix_chain[actual_layer] << "PC_" << param_index << " -> ";
			auto s = print_point(code_db, result, item, true, prefix_chain[actual_layer]);
			std::cout << "\n";
			register_node(code_db, result, item, s, prefix_chain[actual_layer]);
			param_index++;
		}

		if (item.is_function_call) {
			// std::cout << "CALL " << item.associated_flow_name;

			std::cout << "\n";

			int found = -1;
			bool there_is_a_way = true;
			int current = replaced_index;

			while(there_is_a_way) {
				auto current_item = code_db.points[current];
				if (current_item.associated_flow_name != "") {
					for (int i = 0; i < code_db.flows.size(); i++) {
						if (current_item.associated_flow_name == code_db.flows[i].name) {
							found = i;
							break;
						}
					}
					if (found != -1) {
						break;
					}
				}

				if (current_item.is_function_call  && current_item.computation.size() > 0) {
					there_is_a_way = true;
					current = current_item.computation[0];
				} else if (substitution.find(current_item.name) != substitution.end()) {
					there_is_a_way = true;
					current = substitution[current_item.name];
				} else if (current_item.dependency.size() == 1) {
					there_is_a_way = true;
					current = current_item.dependency[0];
				} else {
					there_is_a_way = false;
				}
			}

			if (found == -1) {
				std::cout << "\n";
				continue;
			}

			auto& next_flow = code_db.flows[found];

			bool recursion = false;

			// sorry, no recursion..
			if (next_flow.hash == flow.hash) {
				continue;
			}
			for (auto f : flow_chain) {
				if (code_db.flows[f].hash == next_flow.hash) {
					recursion = true;
					break;
				}
			}
			if (recursion) {
				continue;
			}

			auto returned_index = next_flow.return_value;

			std::string next_prefix = "q" + std::to_string(unique_prefix);
			unique_prefix++;

			// std::string next_prefix = prefix_chain.back();
			// if (next_flow.is_lambda) {
				// next_prefix = prefix_chain[actual_layer];
			// }
			for (auto compute_index : item.computation) {
				// next_prefix += "P_" + std::to_string(compute_index);
			}
			// next_prefix += "__";
			if (returned_index >= 0) {
				auto returned = code_db.points[returned_index];
				std::cout << next_prefix << "iRETURN_" << returned.hash;
				std::cout << " -> ";
				auto s = print_point(code_db, result, item, true, prefix_chain[actual_layer]);
				std::cout << "\n";
				register_node(code_db, result, item, s, prefix_chain[actual_layer]);
			}


			// next_prefix = ninja_name(next_prefix);

			std::cout << "\n";

			std::map<std::string, int> next_substitution;



			for (int p = 0; p < next_flow.parameters_count && (p + 1) < item.computation.size(); p++) {
				auto computed_from = item.computation[p + 1];
				next_substitution[code_db.points[next_flow.local_parameters[p]].name] = item.computation[p + 1];
				auto s = print_point(
					code_db,
					result,
					code_db.points[item.computation[p + 1]],
					false,
					prefix_chain[actual_layer]
				);
				std::cout << " -> " << next_prefix  << "PC_" << p;
				std::cout << "\n";

				register_node(code_db, result, code_db.points[item.computation[p + 1]], s, prefix_chain[actual_layer]);
			}

			// next_prefix += std::to_string(next_flow.hash);


			prefix_chain.push_back(next_prefix);
			flow_chain.push_back(found);

			if (item.computation.size() > 0) {
				int replacement = this_replacement;
				int called_index = item.computation[0];
				auto& called = code_db.points[called_index];
				if (called.is_projection) {
					if (called.computation.size() >= 0) {
						replacement = called.computation[0];
					}

					print_flow(code_db, result, next_flow, flow_chain, prefix_chain, next_substitution, replacement);
				} else {
					print_flow(code_db, result, next_flow, flow_chain, prefix_chain, next_substitution, -1);
				}
			} else {
				print_flow(code_db, result, next_flow, flow_chain, prefix_chain, next_substitution, -1);
			}


			prefix_chain.pop_back();
			flow_chain.pop_back();

			std::cout << "\n";

		} else if (item.is_projection) {


			std::cout << "\"";
			auto node_id = print_point(code_db, result, item, true, prefix_chain[actual_layer]);
			std::cout << "\"";

			// auto current = item_index;
			std::cout << "[ shape = \"record\" label = \"";

			auto current = &item;
			std::string fat_label = current->name;
			std::string pretty_label = current->display_name;
			int depth = 0;
			while(current->is_projection && current->computation.size() > 0) {
				depth++;
				assert(depth < 10);
				auto index = -1;
				if (code_db.points[current->computation[0]].is_this && this_replacement >= 0) {
					index = this_replacement;
					current = &code_db.points[index];
					pretty_label += " | " + current->display_name;
					fat_label += "_" + current->name;
					break;
				}

				index = current->computation[0];
				current = &code_db.points[index];
				if (substitution.find(current->name) != substitution.end()) {
					index = substitution[current->name];
					current = &code_db.points[index];
				}
				pretty_label += " | " + current->display_name;
				fat_label += "_" + current->name;
			}
			std::cout << pretty_label <<"\" ];\n";

			auto it = last_node.find(fat_label);
			if  (it != last_node.end()) {
				if (item.dependency.size() == 0 && it->second != node_id) {
					delayed_edges.push_back({it->second, node_id});
				}
			}
			last_node[fat_label] = node_id;
		} else {
			std::cout << "\n";
		}
	}

	std::cout << "}\n";
};

int main(){
	SetConsoleOutputCP(CP_UTF8);

	CXIndex index = clang_createIndex(0, 0); //Create index
	CXTranslationUnit unit;

	std::vector<const char*> flags2 {"clang++", "-std=c++23", "-mavx2", "-mfma", "-DGLEW_NO_GLU","-DGLEW_STATIC","-DGLM_ENABLE_EXPERIMENTAL","-DINCREMENTAL=1","-DPROJECT_ROOT=C:/_projects/cpp/alice","-DVC_EXTRALEAN","-DWIN32_MEAN_AND_LEAN","-D_CRT_SECURE_NO_WARNINGS","-IC:/_projects/cpp/alice/src","-IC:/_projects/cpp/alice/src/ai","-IC:/_projects/cpp/alice/src/common_types","-IC:/_projects/cpp/alice/src/filesystem","-IC:/_projects/cpp/alice/src/gamestate","-IC:/_projects/cpp/alice/src/gui","-IC:/_projects/cpp/alice/src/gui/province_tiles","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/diplomacy_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/production_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/politics_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/military_subwindows","-IC:/_projects/cpp/alice/src/graphics","-IC:/_projects/cpp/alice/src/parsing","-IC:/_projects/cpp/alice/src/window","-IC:/_projects/cpp/alice/src/text","-IC:/_projects/cpp/alice/src/sound","-IC:/_projects/cpp/alice/src/map","-IC:/_projects/cpp/alice/src/network","-IC:/_projects/cpp/alice/src/nations","-IC:/_projects/cpp/alice/src/gamerule","-IC:/_projects/cpp/alice/src/provinces","-IC:/_projects/cpp/alice/src/economy","-IC:/_projects/cpp/alice/src/culture","-IC:/_projects/cpp/alice/src/military","-IC:/_projects/cpp/alice/src/scripting","-IC:/_projects/cpp/alice/src/scripting/luajit","-IC:/_projects/cpp/alice/src/zstd","-IC:/_projects/cpp/alice/src/lunasvg","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glew-src/include/GL","-IC:/_projects/cpp/alice/ankerl","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glew-src/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/freetype-build/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/freetype-src/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/harfbuzz-src/src","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glm-src","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/datacontainer-src/CommonIncludes","-IC:/_projects/cpp/alice/dependencies/stb/.","-o economy.o"
	};

	std::vector<const char*> flags {"clang++", "-std=c++23"};

	clang_parseTranslationUnit2FullArgv(
		index,
		// "test_function.cpp",
		"C:/_projects/cpp/alice/src/economy/economy.cpp",
		// flags.data(), flags.size(),
		flags2.data(), flags2.size(),
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

	code code_db{};

	flow main_flow{};
	main_flow.name = "ROOT";
	main_flow.hash = 0;

	code_db.flows.push_back(main_flow);
	code_db.flow_stack.push_back(0);

	handle_generic_cursor(cursor, &code_db);

	// std::cout << "rule compute\n  command = touch $out\n";

	std::map<std::string, std::string> result;


	std::cout
		<< "digraph G {"
		<< "fontname=\"Helvetica,Arial,sans-serif\""
		<< "node [fontname=\"Helvetica,Arial,sans-serif\"]"
		<< "edge [fontname=\"Helvetica,Arial,sans-serif\"]"
		<< "graph [ rankdir = \"LR\"]";


	int i = 0;
	for (auto& flow : code_db.flows) {
		// std::cout << "root->" << ninja_name(flow.name);
		// if (flow.is_lambda) continue;
		for (int p = 0; p < flow.parameters_count; p++) {
			// std::cout << "build ";
			// std::cout << ninja_name(flow.name) << "PARAM_CHOOSE_" << p;
			// std::cout << " : compute\n";
		}
		std::map<std::string, int> sub;

		// if (flow.name[2] == 'e' && flow.name[3] == 'c' && flow.name[4] == 'o')
		if (flow.name.find("estimate_reparations_income") != std::string::npos)
			print_flow(code_db, result, flow, {0, i}, {"ROOT", ninja_name(flow.name)}, sub, -1);
		i++;
	}

	for (auto& item : delayed_edges) {
		std::cout << item.first << "->" << item.second << "\n";
	}


	std::cout
		<< "root [shape=Mdiamond];"
		<< "}";
}