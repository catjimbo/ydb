# Generated by devtools/yamaker.

LIBRARY()

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/clang14
    contrib/libs/clang14/include
    contrib/libs/clang14/lib/AST
    contrib/libs/clang14/lib/ASTMatchers
    contrib/libs/clang14/lib/Analysis
    contrib/libs/clang14/lib/Basic
    contrib/libs/clang14/lib/Lex
    contrib/libs/clang14/lib/StaticAnalyzer/Core
    contrib/libs/llvm14
    contrib/libs/llvm14/lib/Frontend/OpenMP
    contrib/libs/llvm14/lib/Support
)

ADDINCL(
    contrib/libs/clang14/lib/StaticAnalyzer/Checkers
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    AnalysisOrderChecker.cpp
    AnalyzerStatsChecker.cpp
    ArrayBoundChecker.cpp
    ArrayBoundCheckerV2.cpp
    BasicObjCFoundationChecks.cpp
    BlockInCriticalSectionChecker.cpp
    BoolAssignmentChecker.cpp
    BuiltinFunctionChecker.cpp
    CStringChecker.cpp
    CStringSyntaxChecker.cpp
    CXXSelfAssignmentChecker.cpp
    CallAndMessageChecker.cpp
    CastSizeChecker.cpp
    CastToStructChecker.cpp
    CastValueChecker.cpp
    CheckObjCDealloc.cpp
    CheckObjCInstMethSignature.cpp
    CheckPlacementNew.cpp
    CheckSecuritySyntaxOnly.cpp
    CheckSizeofPointer.cpp
    CheckerDocumentation.cpp
    ChrootChecker.cpp
    CloneChecker.cpp
    ContainerModeling.cpp
    ConversionChecker.cpp
    DeadStoresChecker.cpp
    DebugCheckers.cpp
    DebugContainerModeling.cpp
    DebugIteratorModeling.cpp
    DeleteWithNonVirtualDtorChecker.cpp
    DereferenceChecker.cpp
    DirectIvarAssignment.cpp
    DivZeroChecker.cpp
    DynamicTypeChecker.cpp
    DynamicTypePropagation.cpp
    EnumCastOutOfRangeChecker.cpp
    ExprInspectionChecker.cpp
    FixedAddressChecker.cpp
    FuchsiaHandleChecker.cpp
    GCDAntipatternChecker.cpp
    GTestChecker.cpp
    GenericTaintChecker.cpp
    IdenticalExprChecker.cpp
    InnerPointerChecker.cpp
    InvalidatedIteratorChecker.cpp
    Iterator.cpp
    IteratorModeling.cpp
    IteratorRangeChecker.cpp
    IvarInvalidationChecker.cpp
    LLVMConventionsChecker.cpp
    LocalizationChecker.cpp
    MIGChecker.cpp
    MPI-Checker/MPIBugReporter.cpp
    MPI-Checker/MPIChecker.cpp
    MPI-Checker/MPIFunctionClassifier.cpp
    MacOSKeychainAPIChecker.cpp
    MacOSXAPIChecker.cpp
    MallocChecker.cpp
    MallocOverflowSecurityChecker.cpp
    MallocSizeofChecker.cpp
    MismatchedIteratorChecker.cpp
    MmapWriteExecChecker.cpp
    MoveChecker.cpp
    NSAutoreleasePoolChecker.cpp
    NSErrorChecker.cpp
    NoReturnFunctionChecker.cpp
    NonNullParamChecker.cpp
    NonnullGlobalConstantsChecker.cpp
    NullabilityChecker.cpp
    NumberObjectConversionChecker.cpp
    OSObjectCStyleCast.cpp
    ObjCAtSyncChecker.cpp
    ObjCAutoreleaseWriteChecker.cpp
    ObjCContainersASTChecker.cpp
    ObjCContainersChecker.cpp
    ObjCMissingSuperCallChecker.cpp
    ObjCPropertyChecker.cpp
    ObjCSelfInitChecker.cpp
    ObjCSuperDeallocChecker.cpp
    ObjCUnusedIVarsChecker.cpp
    PaddingChecker.cpp
    PointerArithChecker.cpp
    PointerIterationChecker.cpp
    PointerSortingChecker.cpp
    PointerSubChecker.cpp
    PthreadLockChecker.cpp
    RetainCountChecker/RetainCountChecker.cpp
    RetainCountChecker/RetainCountDiagnostics.cpp
    ReturnPointerRangeChecker.cpp
    ReturnUndefChecker.cpp
    ReturnValueChecker.cpp
    RunLoopAutoreleaseLeakChecker.cpp
    STLAlgorithmModeling.cpp
    SimpleStreamChecker.cpp
    SmartPtrChecker.cpp
    SmartPtrModeling.cpp
    StackAddrEscapeChecker.cpp
    StdLibraryFunctionsChecker.cpp
    StreamChecker.cpp
    StringChecker.cpp
    Taint.cpp
    TaintTesterChecker.cpp
    TestAfterDivZeroChecker.cpp
    TraversalChecker.cpp
    TrustNonnullChecker.cpp
    UndefBranchChecker.cpp
    UndefCapturedBlockVarChecker.cpp
    UndefResultChecker.cpp
    UndefinedArraySubscriptChecker.cpp
    UndefinedAssignmentChecker.cpp
    UninitializedObject/UninitializedObjectChecker.cpp
    UninitializedObject/UninitializedPointee.cpp
    UnixAPIChecker.cpp
    UnreachableCodeChecker.cpp
    VLASizeChecker.cpp
    ValistChecker.cpp
    VforkChecker.cpp
    VirtualCallChecker.cpp
    WebKit/ASTUtils.cpp
    WebKit/NoUncountedMembersChecker.cpp
    WebKit/PtrTypesSemantics.cpp
    WebKit/RefCntblBaseVirtualDtorChecker.cpp
    WebKit/UncountedCallArgsChecker.cpp
    WebKit/UncountedLambdaCapturesChecker.cpp
    WebKit/UncountedLocalVarsChecker.cpp
    cert/InvalidPtrChecker.cpp
    cert/PutenvWithAutoChecker.cpp
)

END()
