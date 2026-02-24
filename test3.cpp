
#include"clang-c/Index.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <map>

std::string sanitize_usr(std::string input_usr);

struct something {
	uint32_t id;
	bool dirt;
};

struct link {
	int64_t dependent;
	int64_t depends_on;
};


struct diagram {
	std::map<std::string, std::string> morhisms;
};

struct parametrised_data_dependency {
	std::vector<std::string> path;
	bool is_latent;
	bool is_delayed_function_call = false;
	bool is_projection = false;
	bool function_is_not_a_param = false;
	bool is_reference = false;
	bool one_which_is_nameless_but_desires_a_name = false;
	bool desires_universal_recognition = false;
	int32_t parameters_count = 0;
	std::vector<parametrised_data_dependency*> parameters_and_latent_parameters;
	std::vector<link> dependency;
};

struct cursor_data {
	parametrised_data_dependency* data_dependency;
	int not_input = 0;
	int mutates = 0;
	std::vector<size_t> objects_to_link;
	bool temp_flag = false;
};

CXChildVisitResult handle_generic_cursor(CXCursor cursor, cursor_data* current_scope) ;

void handle_generic_cursor_children(CXCursor cursor, cursor_data* current_scope) {
	clang_visitChildren(
		cursor,
		[](
			CXCursor current_cursor,
			CXCursor parent,
			CXClientData client_data
		){
			handle_generic_cursor(current_cursor, (cursor_data*)client_data);
			return CXChildVisit_Continue;
		},
		current_scope
	);
}

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
	clang_disposeString(cursor_spelling);
	clang_disposeString(kind_spelling);
	clang_disposeString(cursor_kind_spelling);
	clang_disposeString(display);
	clang_disposeString(display_ref);
}



void pretty_print_all(CXCursor cursor, std::string indent) {
	pretty_print(cursor, indent);
	{
		CXString pretty = clang_getCursorUSR (cursor);
		std::cout << indent << "USR(san) " << sanitize_usr(clang_getCString(pretty)) << "\n";
		clang_disposeString(pretty);
	}
	{
		CXString pretty = clang_getCursorUSR (cursor);
		std::cout << indent << "USR " << clang_getCString(pretty) << "\n";
		clang_disposeString(pretty);
	}
	{
		CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(cursor));
		std::cout << indent  << "USR REF "<< clang_getCString(pretty) << "\n";
		clang_disposeString(pretty);
	}
	{
		auto original_template = clang_getSpecializedCursorTemplate(cursor);
		if (!clang_Cursor_isNull(original_template)) {
			CXString pretty = clang_getCursorUSR (original_template);
			std::cout << indent << "USR TEM " << clang_getCString(pretty) << "\n";
			clang_disposeString(pretty);
		}
	}
	{
		auto original_template = clang_getSpecializedCursorTemplate(cursor);
		if (!clang_Cursor_isNull(original_template)) {
			CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(original_template));
			std::cout << indent << "USR TEM REF " << clang_getCString(pretty) << "\n";
			clang_disposeString(pretty);
		}
	}
	{
		auto original_template = clang_getSpecializedCursorTemplate(clang_getCursorReferenced(cursor));
		if (!clang_Cursor_isNull(original_template)) {
			CXString pretty = clang_getCursorUSR (original_template);
			std::cout << indent << "USR REF TEM" << clang_getCString(pretty) << "\n";
			clang_disposeString(pretty);
		}
	}

	auto new_indent = indent + "\t";
	clang_visitChildren(
		cursor,
		[](
			CXCursor current_cursor,
			CXCursor parent,
			CXClientData client_data
		){
			// pretty_print_all(current_cursor, *(std::string*)client_data);
			return CXChildVisit_Continue;
		},
		&new_indent
	);
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
	bool skip_until_at = false;
	std::string result;
	for (auto i = 0; i < input_usr.size(); i++) {
		if (input_usr[i] == '@') {
			if (!at_found) {
				at_found = true;
				continue;
			} else if (!second_at_found) {
				second_at_found = true;
				continue;
			} else if (skip_until_at) {
				skip_until_at = false;
			}
		}
		if (at_found && !second_at_found) {
			continue;
		}
		if (input_usr[i] == '>') {
			skip_until_at = true;
		}
		if (skip_until_at) {
			continue;
		}
		result += input_usr[i];
	}

	return result;
}

CXChildVisitResult handle_generic_cursor(CXCursor cursor, cursor_data* current_scope) {

	CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	CXType cursor_type = clang_getCursorType(cursor);

	// pretty_print(cursor, "\t\t");

	CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(cursor));
	// std::cout << "References: " << clang_getCString(pretty) << "\n";
	std::string usr {sanitize_usr(clang_getCString(pretty))};
	// std::cout << "References(sanitized): " << usr << "\n";
	clang_disposeString(pretty);

	if (cursor_kind == CXCursor_StaticAssert) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UnaryExpr) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_LambdaExpr) {
		auto name = "Named_nameless_" + std::to_string(clang_hashCursor(cursor));
		current_scope->data_dependency->path.push_back(name);
		current_scope->data_dependency->desires_universal_recognition = true;
		current_scope->data_dependency->parameters_count = 0;
		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto function = (parametrised_data_dependency*)(client_data);

				CXCursorKind param_kind = clang_getCursorKind(current_cursor);
				CXType param_type = clang_getCursorType(current_cursor);

				// pretty_print(current_cursor, "\t\t");

				CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(current_cursor));
				// std::cout << "References: " << clang_getCString(pretty) << "\n";
				std::string usr {sanitize_usr(clang_getCString(pretty))};
				// std::cout << "References(sanitized): " << usr << "\n";
				clang_disposeString(pretty);

				if (param_kind == CXCursor_TemplateTypeParameter) {

				} else if (param_kind == CXCursor_ParmDecl) {
					// std::cout << "ADD PARAMETER\n";
					function->parameters_count++;
					cursor_data child_data {};
					child_data.data_dependency = new parametrised_data_dependency;
					child_data.data_dependency->path.push_back(usr);
					child_data.data_dependency->is_latent  = false;
					if (param_type.kind == CXType_LValueReference) {
						child_data.data_dependency->is_reference = true;
					}
					handle_generic_cursor_children(current_cursor, &child_data);
					function->parameters_and_latent_parameters.push_back(child_data.data_dependency);
				} else {
					cursor_data child_data {};
					child_data.data_dependency = function;
					handle_generic_cursor(current_cursor, &child_data);
				}
				return CXChildVisit_Continue;
			},
			current_scope->data_dependency
		);
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
		|| cursor_kind == CXCursor_CXXMethod
		|| cursor_kind == CXCursor_FieldDecl
	) {
		// std::cout << "FUNCTION DETECTED:\n";

		parametrised_data_dependency * new_function = new parametrised_data_dependency;
		new_function->path = {usr};
		new_function->is_latent = false;
		new_function->parameters_count = 0;
		new_function->dependency = {};

		if (cursor_kind == CXCursor_CXXMethod || cursor_kind == CXCursor_FieldDecl) {
			// std::cout << "ADD SELF/THIS PARAMETER\n";
			new_function->parameters_count++;
			auto this_param = new parametrised_data_dependency;
			this_param->path.push_back("THIS");
			this_param->is_latent  = false;
			this_param->is_reference = true;
			new_function->parameters_and_latent_parameters.push_back(this_param);
		}


		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto function = (parametrised_data_dependency*)(client_data);

				CXCursorKind param_kind = clang_getCursorKind(current_cursor);
				CXType param_type = clang_getCursorType(current_cursor);

				// pretty_print(current_cursor, "\t\t");

				CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(current_cursor));
				// std::cout << "References: " << clang_getCString(pretty) << "\n";
				std::string usr {sanitize_usr(clang_getCString(pretty))};
				// std::cout << "References(sanitized): " << usr << "\n";
				clang_disposeString(pretty);

				if (param_kind == CXCursor_TemplateTypeParameter || param_kind == CXCursor_TypeRef) {

				} else if (param_kind == CXCursor_ParmDecl) {
					// std::cout << "ADD PARAMETER\n";
					function->parameters_count++;
					assert(function->parameters_count < 512);
					cursor_data child_data {};
					child_data.data_dependency = new parametrised_data_dependency;
					child_data.data_dependency->path.push_back(usr);
					child_data.data_dependency->is_latent  = false;
					if (param_type.kind == CXType_LValueReference) {
						child_data.data_dependency->is_reference = true;
					}
					handle_generic_cursor_children(current_cursor, &child_data);
					function->parameters_and_latent_parameters.push_back(child_data.data_dependency);
				} else {
					cursor_data child_data {};
					child_data.data_dependency = function;
					handle_generic_cursor(current_cursor, &child_data);
				}
				return CXChildVisit_Continue;
			},
			new_function
		);

		current_scope->data_dependency->parameters_and_latent_parameters.push_back(new_function);
		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_TypeRef
	) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_DeclRefExpr) {
		// handle_generic_cursor_children(cursor, current_scope);
		auto original_template = clang_getSpecializedCursorTemplate(clang_getCursorReferenced(cursor));
		if (!clang_Cursor_isNull(original_template)) {
			CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(original_template));
			// std::cout << "Actually References: " << clang_getCString(pretty) << "\n";
			usr = sanitize_usr(clang_getCString(pretty));
			// std::cout << "Actually References(sanitized): " << usr << "\n";
			clang_disposeString(pretty);
		}
		current_scope->data_dependency->path.push_back(usr);
		return CXChildVisit_Break;
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

	if (cursor_kind == CXCursor_FriendDecl) {
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

	if (cursor_kind == CXCursor_CXXThisExpr) {
		current_scope->data_dependency->path.push_back("THIS");
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_NullStmt) {
		current_scope->data_dependency->path.push_back("NULL");
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_VarDecl) {
		cursor_data declared {};
		declared.data_dependency = new parametrised_data_dependency;
		declared.data_dependency->path.push_back(usr);
		declared.data_dependency->is_latent = true;
		current_scope->data_dependency->parameters_and_latent_parameters.push_back(declared.data_dependency);
		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				handle_generic_cursor(current_cursor, (cursor_data*)client_data);
				return CXChildVisit_Continue;
			},
			&declared
		);
		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_ArraySubscriptExpr
		|| cursor_kind == CXCursor_UnexposedExpr
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
	) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ConversionFunction) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TemplateRef) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_TemplateTemplateParameter) {
		return CXChildVisit_Break;
	}

	if(cursor_kind == CXCursor_CXXBoolLiteralExpr) {
		auto data = clang_Cursor_Evaluate(cursor);
		auto data_int = clang_EvalResult_getAsInt(data);
		current_scope->data_dependency->path.push_back("Literal_" + std::to_string(data_int) + "_" + std::to_string(clang_hashCursor(cursor)));
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_IntegerLiteral) {
		auto data = clang_Cursor_Evaluate(cursor);
		auto data_int = clang_EvalResult_getAsInt(data);
		current_scope->data_dependency->path.push_back("Literal_" + std::to_string(data_int) + "_" + std::to_string(clang_hashCursor(cursor)));
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_FloatingLiteral) {
		auto data = clang_Cursor_Evaluate(cursor);
		auto data_int = clang_EvalResult_getAsDouble(data);
		current_scope->data_dependency->path.push_back("Literal_" + std::to_string(data_int) + "_" + std::to_string(clang_hashCursor(cursor)));
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_StringLiteral) {
		current_scope->data_dependency->path.push_back("String Literal_" + std::to_string(clang_hashCursor(cursor)));
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

	if (cursor_kind == CXCursor_CallExpr || cursor_kind == CXCursor_MemberRefExpr) {
		auto original_template = clang_getSpecializedCursorTemplate(clang_getCursorReferenced(cursor));
		if (!clang_Cursor_isNull(original_template)) {
			CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(original_template));
			// std::cout << "Actually References: " << sanitize_usr(clang_getCString(pretty)) << "\n";
			usr = sanitize_usr(clang_getCString(pretty));
			// std::cout << "Actually References(sanitized): " << usr << "\n";
			clang_disposeString(pretty);
		}

		// std::cout << "CALL/MEMBER EXRESSION\n";

		cursor_data function_result {};
		function_result.data_dependency = new parametrised_data_dependency;
		function_result.data_dependency->path.push_back(usr);
		function_result.data_dependency->is_latent = true;
		function_result.data_dependency->is_delayed_function_call = true;
		current_scope->data_dependency->dependency.push_back({-1, (int64_t)current_scope->data_dependency->parameters_and_latent_parameters.size()});
		current_scope->data_dependency->parameters_and_latent_parameters.push_back(function_result.data_dependency);

		if (cursor_kind == CXCursor_MemberRefExpr) {
			function_result.data_dependency->function_is_not_a_param = true;
			function_result.data_dependency->is_projection = true;
		}

		function_result.temp_flag = false;

		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto parent_cursor_data = (cursor_data*) client_data;
				parent_cursor_data->temp_flag = true;
				auto empty_index = parent_cursor_data->data_dependency->parameters_and_latent_parameters.size();
				cursor_data child_data {};
				child_data.data_dependency = new parametrised_data_dependency;
				child_data.data_dependency->path.push_back({});
				child_data.data_dependency->is_latent = true;

				CXCursorKind child_kind = clang_getCursorKind(current_cursor);
				if (child_kind == CXCursor_MemberRefExpr){
					parent_cursor_data->data_dependency->function_is_not_a_param = true;
					clang_visitChildren(
						current_cursor,
						[](
							CXCursor current_cursor,
							CXCursor parent,
							CXClientData client_data
						){
							handle_generic_cursor(current_cursor, (cursor_data*)client_data);
							return CXChildVisit_Continue;
						},
						&child_data
					);
				} else {
					handle_generic_cursor(current_cursor, &child_data);
				}

				parent_cursor_data->data_dependency->parameters_and_latent_parameters.push_back(child_data.data_dependency);
				return CXChildVisit_Continue;
			},
			&function_result
		);

		if (function_result.temp_flag == false && cursor_kind == CXCursor_MemberRefExpr) {
			auto empty_index = function_result.data_dependency->parameters_and_latent_parameters.size();
			cursor_data child_data {};
			child_data.data_dependency = new parametrised_data_dependency;
			child_data.data_dependency->path.push_back({"THIS"});
			child_data.data_dependency->is_latent = true;
			function_result.data_dependency->parameters_and_latent_parameters.push_back(child_data.data_dependency);
		}

		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_ReturnStmt) {
		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto parent_cursor_data = (cursor_data*) client_data;
				auto empty_index = parent_cursor_data->data_dependency->parameters_and_latent_parameters.size();
				cursor_data child_data {};
				child_data.data_dependency = new parametrised_data_dependency;
				child_data.data_dependency->path.push_back({});
				child_data.data_dependency->is_latent = true;
				handle_generic_cursor(current_cursor, &child_data);
				parent_cursor_data->data_dependency->dependency.push_back({
					-1,
					(int64_t)parent_cursor_data->data_dependency->parameters_and_latent_parameters.size()
				});
				parent_cursor_data->data_dependency->parameters_and_latent_parameters.push_back(child_data.data_dependency);
				return CXChildVisit_Continue;
			},
			current_scope
		);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_UnaryOperator) {
		bool mutates_left = false;
		auto binary = clang_getCursorUnaryOperatorKind(cursor);
		if (
			binary == CXUnaryOperator_PostDec
			|| binary == CXUnaryOperator_PostInc
		) {
			mutates_left = true;
		}
		if (mutates_left) {
			current_scope->mutates = 1;
		}
		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto parent_cursor_data = (cursor_data*) client_data;
				auto empty_index = parent_cursor_data->data_dependency->parameters_and_latent_parameters.size();
				cursor_data child_data {};
				child_data.data_dependency = new parametrised_data_dependency;
				child_data.data_dependency->path.push_back({});
				child_data.data_dependency->is_latent = true;
				handle_generic_cursor(current_cursor, &child_data);
				bool link_required = false;
				if (parent_cursor_data->mutates > 0) {
					parent_cursor_data->mutates--;
					parent_cursor_data->objects_to_link.push_back(empty_index);
				}
				for (auto linked : parent_cursor_data->objects_to_link) {
					parent_cursor_data->data_dependency->dependency.push_back(
						{
							(int64_t)linked,
							(int64_t)parent_cursor_data->data_dependency->parameters_and_latent_parameters.size()
						}
					);
				}
				parent_cursor_data->data_dependency->dependency.push_back(
					{
						-1,
						(int64_t)parent_cursor_data->data_dependency->parameters_and_latent_parameters.size()
					}
				);

				parent_cursor_data->data_dependency->parameters_and_latent_parameters.push_back(child_data.data_dependency);
				return CXChildVisit_Continue;
			},
			current_scope
		);

		current_scope->objects_to_link.clear();

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

		if (mutates_left) {
			current_scope->mutates = 1;
		}
		if (!left_is_input) {
			current_scope->not_input = 1;
		}
		clang_visitChildren(
			cursor,
			[](
				CXCursor current_cursor,
				CXCursor parent,
				CXClientData client_data
			){
				auto parent_cursor_data = (cursor_data*) client_data;
				auto empty_index = parent_cursor_data->data_dependency->parameters_and_latent_parameters.size();
				cursor_data child_data {};
				child_data.data_dependency = new parametrised_data_dependency;
				child_data.data_dependency->path.push_back({});
				child_data.data_dependency->is_latent = true;
				handle_generic_cursor(current_cursor, &child_data);
				bool link_required = false;

				bool skip_input = false;
				if (parent_cursor_data->not_input > 0) {
					parent_cursor_data->not_input--;
					skip_input = true;
				} else {
					parent_cursor_data->data_dependency->dependency.push_back(
						{-1, (int64_t)empty_index}
					);
				}

				bool mutates = true;
				if (parent_cursor_data->mutates > 0) {
					mutates = false;
					parent_cursor_data->mutates--;
					parent_cursor_data->objects_to_link.push_back(empty_index);
				}

				// for (auto& item : child_data.data_dependency->parameters_and_latent_parameters) {
				if (!skip_input) {
					for (auto linked : parent_cursor_data->objects_to_link) {
						parent_cursor_data->data_dependency->dependency.push_back(
							{
								(int64_t)linked,
								(int64_t)parent_cursor_data->data_dependency->parameters_and_latent_parameters.size()
							}
						);
					}
				}
				// }

				parent_cursor_data->data_dependency->parameters_and_latent_parameters.push_back(child_data.data_dependency);


				return CXChildVisit_Continue;
			},
			current_scope
		);

		current_scope->objects_to_link.clear();

		return CXChildVisit_Break;
	}

	// std::cout << "end: " << usr << "\n";
	// pretty_print(cursor, "\t\t");
	std::cout << "UNRESOLVED CASE\nInputs:";

	// assert(false);

	for (auto& item : current_scope->data_dependency->parameters_and_latent_parameters) {
		for (auto & step : item->path) {
			std::cout << step << "\n-->";
		}
		std::cout << "END\n";
	}
	std::cout << "\n\n";

	handle_generic_cursor_children(cursor, current_scope);

	return CXChildVisit_Recurse;
}

parametrised_data_dependency* deep_copy(parametrised_data_dependency* data, std::map<std::string, std::string>& replace_table) {
	auto result = new parametrised_data_dependency;
	result->dependency = data->dependency;
	result->is_delayed_function_call = data->is_delayed_function_call;
	result->is_latent = data->is_latent;
	result->is_reference = data->is_reference;
	result->path = data->path;
	result->parameters_count = data->parameters_count;
	result->function_is_not_a_param = data->function_is_not_a_param;
	result->is_projection = data->is_projection;
	result->desires_universal_recognition = result->desires_universal_recognition;
	for (int i = 0; i < result->path.size(); i++) {
		auto it_replace = replace_table.find(result->path[i]);
		if (it_replace == replace_table.end()) continue;
		result->path[i] = it_replace->second;
	}
	for (auto item : data->parameters_and_latent_parameters) {
		result->parameters_and_latent_parameters.push_back(deep_copy(item, replace_table));
	}
	return result;
}

void
simp(
	parametrised_data_dependency*
		current,
	parametrised_data_dependency*
		parent
) {
	for (auto item : current->parameters_and_latent_parameters){
		simp(item, current);
	}

	if (parent) {
		if (parent->parameters_and_latent_parameters.size() == 1) {
			for (auto item : current->path) {
				parent->path.push_back(item);
			}
			parent->parameters_and_latent_parameters.clear();
			parent->dependency = current->dependency;
			for (auto item : current->parameters_and_latent_parameters) {
				parent->parameters_and_latent_parameters.push_back(item);
			}
		}
	}
}

void
rw(
	parametrised_data_dependency*
		current,
	std::map<std::string, parametrised_data_dependency*>*
		lookup_table,
	bool& rewrite_exists,
	int& position
) {
	position++;

	bool created_lookup = false;
	if (lookup_table == nullptr) {
		created_lookup = true;
		lookup_table = new std::map<std::string, parametrised_data_dependency*>;
		for (auto item : current->parameters_and_latent_parameters) {
			lookup_table->insert_or_assign(item->path.back(), item);
		}
	} else {
		if (current->one_which_is_nameless_but_desires_a_name && (current->path.empty() || (current->path.size() == 1 && current->path[0] == ""))) {
			current->path.push_back("Named_nameless_" + std::to_string(position));
			lookup_table->insert_or_assign(current->path.back(), current);
		} else if (current->desires_universal_recognition) {
			lookup_table->insert_or_assign(current->path.back(), current);
		}
	}

	for (auto item : current->parameters_and_latent_parameters){
		rw(item, lookup_table, rewrite_exists, position);
	}

	if (current->is_delayed_function_call && !current->is_projection && current->path.size() > 0 && !current->path.back().empty()) {
		auto function = lookup_table->find(current->path.back());
		if (function != lookup_table->end()) {
			// remove function reference
			if (!current->function_is_not_a_param) {
				current->parameters_and_latent_parameters.erase(
					current->parameters_and_latent_parameters.begin(),
					current->parameters_and_latent_parameters.begin() + 1
				);
			}
			current->path.clear();
			rewrite_exists = true;
			std::map<std::string, std::string> replace_table;
			for (int param = 0; param < function->second->parameters_count; param++) {
				replace_table.insert_or_assign(
					function->second->parameters_and_latent_parameters[param]->path.back(),
					current->parameters_and_latent_parameters[param]->path.back()
				);
				std::cout << "REPLACE " << function->second->parameters_and_latent_parameters[param]->path.back() << " WITH " << current->parameters_and_latent_parameters[param]->path.back() << "\n";
			}
			for (
				int next = function->second->parameters_count;
				next < function->second->parameters_and_latent_parameters.size();
				next++
			) {
				current->parameters_and_latent_parameters.push_back(deep_copy(
					function->second->parameters_and_latent_parameters[next], replace_table
				));
			}
			current->dependency = function->second->dependency;
			current->is_delayed_function_call = false;
		}
	}

	if (created_lookup) {
		delete lookup_table;
	}
}

void
ninja_count(
	parametrised_data_dependency*
		data_dependency,
	int&
		position
) {
	position++;
	for (auto item : data_dependency->parameters_and_latent_parameters) {
		ninja_count(item, position);
	}
}

std::pair<std::string, std::string>
ninja_name(
	parametrised_data_dependency*
		data_dependency,
	int&
		position
) {
	int my_position = position;
	position++;
	for (auto item : data_dependency->parameters_and_latent_parameters) {
		ninja_count(item, position);
	}
	std::string link = "";

	for (auto& step : data_dependency->path) {
		if (step.length() == 0) continue;
		std::string replaced = step;
		for (int i = 0; i < step.length(); i++) {
			if (replaced[i] == ':' || replaced[i] == '$' || replaced[i] == ' ' || replaced[i] == '|') {
				replaced[i] = '@';
			}
		}
		link += replaced + "_";
	}
	return  {link, "stage_" + link + "_" + std::to_string(my_position)};
}

void
ninja_print(
	std::pair<std::string, std::string>* name,
	parametrised_data_dependency*
		data_dependency,
	parametrised_data_dependency*
		parent_data_dependency,
	std::vector<std::pair<std::string, std::string>>*
		sibling_names,
	int&
		position,
	std::map<std::string, std::string>*
		last_position,
	int index_among_children
) {
	int my_position = position;
	position++;
	int throwaway_position = position;
	std::vector<std::pair<std::string, std::string>> children_names;

	for (auto item : data_dependency->parameters_and_latent_parameters) {
		children_names.push_back(ninja_name(item, throwaway_position));
	}

	int i = 0;
	for (auto item : data_dependency->parameters_and_latent_parameters) {
		ninja_print(
			&children_names[i],
			item,
			data_dependency,
			&children_names,
			position,
			last_position,
			i
		);
		i++;
	}

	std::vector<std::string> actual_inputs;

	for (auto& dep : data_dependency->dependency) {
		if (dep.dependent == -1) {
			actual_inputs.push_back(children_names[dep.depends_on].second);
		}
	}

	if (parent_data_dependency) {
		for (auto dep: parent_data_dependency->dependency) {
			if (dep.dependent == index_among_children) {
				actual_inputs.push_back(sibling_names->at(dep.depends_on).second);
			}
		}
	}

	bool depends_on_us = false;

	if (parent_data_dependency) {
		for (auto dep: parent_data_dependency->dependency) {
			if (dep.depends_on == index_among_children) {
				depends_on_us = true;
			}
		}
	}

	if (my_position == 1) {
		return;
	}


	auto last = last_position->find(name->first);
	if (last != last_position->end()) {
		actual_inputs.push_back(last->second);
	}

	if (name->first.length() > 0) {
		// std::cout << "#new mapping: " << name->first  << " >---> " << name->second << "\n";
		last_position->insert_or_assign(name->first, name->second);
	}

	if (!depends_on_us && actual_inputs.size() == 0) {
		return;
	}

	std::cout << "build " << name->second << " : compute ";


	for (auto item : actual_inputs) {
		std::cout << item << " ";
	}

	std::cout << "\n";

	return;
}

void pretty_print_data_dependency(std::string indent, parametrised_data_dependency* data_dependency) {
	std::cout << indent << "\n";
	if (data_dependency->is_delayed_function_call) {
		std::cout << indent  << "FUNCTION CALL\n";
	}
	if (data_dependency->is_latent) {
		std::cout << indent <<"LATENT\n";
	} else {
		std::cout <<indent <<"INPUT\n";
	}
	if (data_dependency->is_reference) {
		std::cout << indent <<"REFERENCE\n";
	}
	std::cout << indent;
	for (auto step : data_dependency->path){
		std::cout << step << "\n" << indent << "-->";
	}
	std::cout << "OBJ\n";

	bool has_return = false;
	for (auto dep : data_dependency->dependency) {
		if (dep.dependent == -1) {
			has_return = true;
		}
	}
	if (has_return) {
		std::cout << indent << "RETURN VALUE DEPENDS ON: ";
		for (auto dep : data_dependency->dependency) {
			if (dep.dependent == -1) {
				std::cout << dep.depends_on << " ";
			}
		}
	}
	std::cout << "\n";

	auto i = 0;

	for (auto item : data_dependency->parameters_and_latent_parameters) {
		std::cout << indent << i << ")";
		pretty_print_data_dependency(indent + "\t", item);

		std::cout << indent << "DEPENDS ON: ";
		for (auto dep : data_dependency->dependency) {
			if (dep.dependent == i) {
				std::cout << dep.depends_on << " ";
			}
		}

		std::cout << "\n" << indent << "--------------------------" << "\n";
		i++;
	}
}

int main(){
	CXIndex index = clang_createIndex(0, 0); //Create index
	CXTranslationUnit unit;

	std::vector<const char*> flags2 {"clang++", "-std=c++23", "-mavx2", "-mfma", "-DGLEW_NO_GLU","-DGLEW_STATIC","-DGLM_ENABLE_EXPERIMENTAL","-DINCREMENTAL=1","-DPROJECT_ROOT=C:/_projects/cpp/alice","-DVC_EXTRALEAN","-DWIN32_MEAN_AND_LEAN","-D_CRT_SECURE_NO_WARNINGS","-IC:/_projects/cpp/alice/src","-IC:/_projects/cpp/alice/src/ai","-IC:/_projects/cpp/alice/src/common_types","-IC:/_projects/cpp/alice/src/filesystem","-IC:/_projects/cpp/alice/src/gamestate","-IC:/_projects/cpp/alice/src/gui","-IC:/_projects/cpp/alice/src/gui/province_tiles","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/diplomacy_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/production_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/politics_subwindows","-IC:/_projects/cpp/alice/src/gui/topbar_subwindows/military_subwindows","-IC:/_projects/cpp/alice/src/graphics","-IC:/_projects/cpp/alice/src/parsing","-IC:/_projects/cpp/alice/src/window","-IC:/_projects/cpp/alice/src/text","-IC:/_projects/cpp/alice/src/sound","-IC:/_projects/cpp/alice/src/map","-IC:/_projects/cpp/alice/src/network","-IC:/_projects/cpp/alice/src/nations","-IC:/_projects/cpp/alice/src/gamerule","-IC:/_projects/cpp/alice/src/provinces","-IC:/_projects/cpp/alice/src/economy","-IC:/_projects/cpp/alice/src/culture","-IC:/_projects/cpp/alice/src/military","-IC:/_projects/cpp/alice/src/scripting","-IC:/_projects/cpp/alice/src/scripting/luajit","-IC:/_projects/cpp/alice/src/zstd","-IC:/_projects/cpp/alice/src/lunasvg","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glew-src/include/GL","-IC:/_projects/cpp/alice/ankerl","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glew-src/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/freetype-build/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/freetype-src/include","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/harfbuzz-src/src","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/glm-src","-IC:/_projects/cpp/alice/out/build/x64-release-windows-avx2/_deps/datacontainer-src/CommonIncludes","-IC:/_projects/cpp/alice/dependencies/stb/.","-o economy.o"
	};

	std::vector<const char*> flags {"clang++", "-std=c++23"};

	clang_parseTranslationUnit2FullArgv(
		index,
		// "test_function.cpp",
		"C:/_projects/cpp/alice/src/economy/economy.cpp",
		flags.data(), flags.size(),
		// flags2.data(), flags2.size(),
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
	cs.data_dependency = new parametrised_data_dependency;
	cs.data_dependency->path.push_back({});
	handle_generic_cursor(cursor, &cs);

	/*
	std::cout << "FILE END\n";

	for (auto dependent = 0; dependent < cs.data_dependency->parameters_and_latent_parameters.size(); dependent++) {
		bool something = false;
		for (auto& item : cs.data_dependency->dependency) {
			if (item.dependent != dependent) {
				continue;
			}
			something = true;
		}
		if (!something) {
			continue;
		}

		std::cout << "________\nDEPENDENT " << dependent << "\n";
		for (auto & step : cs.data_dependency->parameters_and_latent_parameters[dependent]->path) {
			std::cout << step << "\n-->";
		}
		std::cout << "OBJ\n";
		std::cout << "DEPENDS ON\n";
		for (auto& item : cs.data_dependency->dependency) {
			if (item.dependent != dependent) {
				continue;
			}
			std::cout << "ITEM " << item.depends_on << "\n";
			for (auto & step : cs.data_dependency->parameters_and_latent_parameters[item.depends_on]->path) {
				std::cout << step << "\n-->";
			}
			std::cout << "OBJ\n";
		}
		std::cout << "\n";
	}
	std::cout << "\n\n";
	*/

	std::cout << "DEPENDENCIES:\n";

	// pretty_print_data_dependency("", cs.data_dependency);

	pretty_print_all(clang_getTranslationUnitCursor(unit), "");

	bool can_be_rewritten = true;

	for (int i = 0; i < 100 && can_be_rewritten; i++)  {
		std::cout << "REWRITE\n";
		can_be_rewritten = false;
		int pos = 1;
		rw(cs.data_dependency, nullptr, can_be_rewritten, pos);
		// pretty_print_data_dependency("", cs.data_dependency);
	}

	simp(cs.data_dependency, nullptr);
	// pretty_print_data_dependency("", cs.data_dependency);

	std::map<std::string, std::string> temp;
	int pos = 1;
	std::pair<std::string, std::string> name = {"root", "root"};
	std::vector<std::pair<std::string, std::string>> sibling_names = {name};
	ninja_print(&name, cs.data_dependency, nullptr, &sibling_names, pos, &temp, 0);
}