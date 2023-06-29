# Generated by devtools/yamaker.

LIBRARY()

LICENSE(
    Apache-2.0 WITH LLVM-exception AND
    MIT
)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/clang14
    contrib/libs/clang14/include
    contrib/libs/clang14/lib/AST
    contrib/libs/clang14/lib/ASTMatchers
    contrib/libs/clang14/lib/Analysis
    contrib/libs/clang14/lib/Basic
    contrib/libs/clang14/lib/Format
    contrib/libs/clang14/lib/Frontend
    contrib/libs/clang14/lib/Lex
    contrib/libs/clang14/lib/Rewrite
    contrib/libs/clang14/lib/Sema
    contrib/libs/clang14/lib/Serialization
    contrib/libs/clang14/lib/StaticAnalyzer/Core
    contrib/libs/clang14/lib/StaticAnalyzer/Frontend
    contrib/libs/clang14/lib/Tooling
    contrib/libs/clang14/lib/Tooling/Core
    contrib/libs/llvm14
    contrib/libs/llvm14/lib/Frontend/OpenMP
    contrib/libs/llvm14/lib/Support
)

ADDINCL(
    contrib/libs/clang14/tools/extra/clang-tidy
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCDIR(contrib/libs/clang14/tools/extra/clang-tidy)

SRCS(
    ClangTidy.cpp
    ClangTidyCheck.cpp
    ClangTidyDiagnosticConsumer.cpp
    ClangTidyModule.cpp
    ClangTidyOptions.cpp
    ClangTidyProfiling.cpp
    ExpandModularHeadersPPCallbacks.cpp
    GlobList.cpp
    NoLintDirectiveHandler.cpp
)

END()
