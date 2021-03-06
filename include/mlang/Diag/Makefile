##===- projects/mlang/include/mlang/Diag/Makefile ----------*- Makefile -*-===##

#
# Indicates our relative path to the top of the project's root directory.
#
LEVEL = ../../..

#
# Sources that built in make
#
BUILT_SOURCES = \
	DiagnosticAnalysisKinds.inc DiagnosticASTKinds.inc \
	DiagnosticCommonKinds.inc DiagnosticDriverKinds.inc \
	DiagnosticFrontendKinds.inc DiagnosticLexKinds.inc \
	DiagnosticParseKinds.inc DiagnosticSemaKinds.inc \
	DiagnosticIndexName.inc DiagnosticSerializationKinds.inc \
	DiagnosticGroups.inc

# #AttrList.inc arm_neon.inc Version.inc

TABLEGEN_INC_FILES_COMMON = 1

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common

# Set up Mlang's tblgen.
MLANG_TBLGEN := $(ToolDir)/mlang-tblgen$(EXEEXT)
MlangTableGen = $(MLANG_TBLGEN) $(TableGen.Flags)

INPUT_TDS = $(wildcard $(PROJ_SRC_DIR)/Diagnostic*.td)

$(ObjDir)/Diagnostic%Kinds.inc.tmp : Diagnostic.td Diagnostic%Kinds.td $(MLANG_TBLGEN) $(ObjDir)/.dir
	$(Echo) "Building mlang $(patsubst Diagnostic%Kinds.inc.tmp,%,$(@F)) diagnostic tables with tblgen"
	$(Verb) $(MlangTableGen) -gen-mlang-diags-defs -mlang-component=$(patsubst Diagnostic%Kinds.inc.tmp,%,$(@F)) -o $(call SYSPATH, $@) $<

$(ObjDir)/DiagnosticIndexName.inc.tmp : Diagnostic.td $(INPUT_TDS) $(MLANG_TBLGEN) $(ObjDir)/.dir
	$(Echo) "Building mlang diagnostic name index with tblgen"
	$(Verb) $(MlangTableGen) -gen-mlang-diags-index-name -o $(call SYSPATH, $@) $<

$(ObjDir)/DiagnosticGroups.inc.tmp : Diagnostic.td DiagnosticGroups.td $(INPUT_TDS) $(MLANG_TBLGEN) $(ObjDir)/.dir
	$(Echo) "Building mlang diagnostic groups with tblgen"
	$(Verb) $(MlangTableGen) -gen-mlang-diag-groups -o $(call SYSPATH, $@) $<
