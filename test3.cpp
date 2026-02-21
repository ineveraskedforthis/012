struct structure_fields {

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