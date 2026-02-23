
#include"clang-c/Index.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>

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
	bool is_reference = false;
	size_t parameters_count;
	std::vector<parametrised_data_dependency*> parameters_and_latent_parameters;
	std::vector<link> dependency;
};

struct cursor_data {
	parametrised_data_dependency* data_dependency;
	int not_input = 0;
	int mutates = 0;
	std::vector<size_t> objects_to_link;
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
	std::cout
		<< indent
		<< "Cursor "  << clang_getCString(cursor_spelling)
		<< " Display "  << clang_getCString(display)
		<< " CursorKind: " << clang_getCString(cursor_kind_spelling)
		<<" TypeKind: " << cursor_type.kind << " " << clang_getCString(kind_spelling)
		<< "\n";
	clang_disposeString(cursor_spelling);
	clang_disposeString(kind_spelling);
	clang_disposeString(cursor_kind_spelling);
	clang_disposeString(display);
}

void pretty_print_all(CXCursor cursor, std::string indent) {
	pretty_print(cursor, indent);
	CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(cursor));
	std::cout << indent << clang_getCString(pretty) << "\n";
	clang_disposeString(pretty);

	auto new_indent = indent + "\t";
	clang_visitChildren(
		cursor,
		[](
			CXCursor current_cursor,
			CXCursor parent,
			CXClientData client_data
		){
			pretty_print_all(current_cursor, *(std::string*)client_data);
			return CXChildVisit_Continue;
		},
		&new_indent
	);
}

CXChildVisitResult handle_generic_cursor(CXCursor cursor, cursor_data* current_scope) {

	CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	CXType cursor_type = clang_getCursorType(cursor);

	pretty_print(cursor, "\t\t");

	CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(cursor));
	std::cout << "References: " << clang_getCString(pretty) << "\n";
	std::string usr {clang_getCString(pretty)};
	clang_disposeString(pretty);

	if (cursor_kind == CXCursor_FunctionDecl) {
		std::cout << "FUNCTION DETECTED:\n";

		parametrised_data_dependency * new_function = new parametrised_data_dependency;
		new_function->path = {usr};
		new_function->is_latent = false;
		new_function->parameters_count = 0;
		new_function->dependency = {};

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

				pretty_print(current_cursor, "\t\t");

				CXString pretty = clang_getCursorUSR (clang_getCursorReferenced(current_cursor));
				std::cout << "References: " << clang_getCString(pretty) << "\n";
				std::string usr {clang_getCString(pretty)};
				clang_disposeString(pretty);

				if (param_kind == CXCursor_ParmDecl) {
					std::cout << "ADD PARAMETER\n";
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
			new_function
		);

		current_scope->data_dependency->parameters_and_latent_parameters.push_back(new_function);
		return CXChildVisit_Break;
	}

	if (
		cursor_kind == CXCursor_FieldDecl
		|| cursor_kind == CXCursor_TypeRef
		|| cursor_kind == CXCursor_StructDecl
	) {
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_DeclRefExpr) {
		// handle_generic_cursor_children(cursor, current_scope);
		current_scope->data_dependency->path.push_back(usr);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_MemberRefExpr) {
		handle_generic_cursor_children(cursor, current_scope);
		current_scope->data_dependency->path.push_back(usr);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_NamespaceRef || cursor_kind == CXCursor_DeclStmt) {
		handle_generic_cursor_children(cursor, current_scope);
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

	}

	if (
		cursor_kind == CXCursor_ArraySubscriptExpr
		|| cursor_kind == CXCursor_UnexposedExpr
		|| cursor_kind == CXCursor_CompoundStmt
		|| cursor_kind == CXCursor_Namespace
		|| cursor_kind == CXCursor_TranslationUnit
	) {
		handle_generic_cursor_children(cursor, current_scope);
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_IntegerLiteral) {
		auto data = clang_Cursor_Evaluate(cursor);
		auto data_int = clang_EvalResult_getAsInt(data);
		current_scope->data_dependency->path.push_back(std::to_string(data_int));
		return CXChildVisit_Break;
	}

	if (cursor_kind == CXCursor_CallExpr) {
		std::cout << "FUNCTION CALL\n";

		cursor_data function_result {};
		function_result.data_dependency = new parametrised_data_dependency;
		function_result.data_dependency->path.push_back(usr);
		function_result.data_dependency->is_latent = true;
		function_result.data_dependency->is_delayed_function_call = true;
		current_scope->data_dependency->parameters_and_latent_parameters.push_back(function_result.data_dependency);

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
				parent_cursor_data->data_dependency->parameters_and_latent_parameters.push_back(child_data.data_dependency);
				return CXChildVisit_Continue;
			},
			&function_result
		);
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
				parent_cursor_data->data_dependency->parameters_and_latent_parameters.push_back(child_data.data_dependency);
				return CXChildVisit_Continue;
			},
			current_scope
		);

		current_scope->objects_to_link.clear();

		return CXChildVisit_Break;
	}


	if (cursor_kind == CXCursor_BinaryOperator || cursor_kind == CXCursor_CompoundAssignOperator) {
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

				bool skip_input = true;
				if (parent_cursor_data->not_input > 0) {
					parent_cursor_data->not_input--;
					skip_input = false;
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

	std::cout << "end: " << usr << "\n";
	pretty_print(cursor, "\t\t");
	std::cout << "UNRESOLVED CASE\nInputs:";

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
	cs.data_dependency = new parametrised_data_dependency;
	cs.data_dependency->path.push_back({});
	handle_generic_cursor(cursor, &cs);

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


	std::cout << "DEPENDENCIES:\n";

	pretty_print_data_dependency("", cs.data_dependency);

	pretty_print_all(clang_getTranslationUnitCursor(unit), "");
}