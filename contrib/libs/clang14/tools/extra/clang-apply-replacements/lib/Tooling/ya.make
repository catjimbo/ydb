# Generated by devtools/yamaker.

LIBRARY()

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/clang14
    contrib/libs/clang14/include
    contrib/libs/clang14/lib/AST
    contrib/libs/clang14/lib/Basic
    contrib/libs/clang14/lib/Rewrite
    contrib/libs/clang14/lib/Tooling/Core
    contrib/libs/clang14/lib/Tooling/Refactoring
    contrib/libs/llvm14
    contrib/libs/llvm14/lib/Support
)

ADDINCL(
    contrib/libs/clang14/tools/extra/clang-apply-replacements
    contrib/libs/clang14/tools/extra/clang-apply-replacements/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    ApplyReplacements.cpp
)

END()
