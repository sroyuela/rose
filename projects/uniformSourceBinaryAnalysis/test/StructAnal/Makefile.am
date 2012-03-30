include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

INCLUDES = $(ROSE_INCLUDES) -I$(top_srcdir)/projects/uniformSourceBinaryAnalysis/include

TEST_PATH = $(top_srcdir)/projects/uniformSourceBinaryAnalysis/test/StructAnal
BUILD_PATH = $(top_builddir)/projects/uniformSourceBinaryAnalysis/test/StructAnal

bin_PROGRAMS = test_StructAnalysisGeneral.exe test_StructAnalysisBinary.exe

test_StructAnalysisGeneral_exe_SOURCES = $(TEST_PATH)/test_StructAnalysisGeneral.C
test_StructAnalysisBinary_exe_SOURCES = $(TEST_PATH)/test_StructAnalysisBinary.C

LDADD = $(ROSE_LIBS) 

BINARY_TESTCODES_REQUIRED_TO_PASS = \
				$(TEST_PATH)/test_binary/testImproper.C \
				$(TEST_PATH)/test_binary/testImproper2.C \
				$(TEST_PATH)/test_binary/testImproper3.C \
				$(TEST_PATH)/test_binary/testGOTO.C \
				$(TEST_PATH)/test_binary/testIF_GOTO.C \
				$(TEST_PATH)/test_binary/testLOOPs.C \
				$(TEST_PATH)/test_binary/testWHILE_IF.C \
				$(TEST_PATH)/test_binary/testWHILE_IF2.C \
				$(TEST_PATH)/test_binary/testWHILE_IF3.C

BINARY_TESTCODE_CURRENTLY_FAILING =

ALL_BINARY_TESTCODES = \
$(BINARY_TESTCODES_REQUIRED_TO_PASS) \
$(BINARY_TESTCODE_CURRENTLY_FAILING)

BIN_CHECK = $(BINARY_TESTCODES_REQUIRED_TO_PASS:.C=)
$(BIN_CHECK): test_stree.dot
		$(BUILD_PATH)/test_StructAnalysisBinary $@ -o rost_temp
		if ! [[ -f $@.dot ]] then; echo " Expected output file missing, test failed" ; exit 1; fi
		if [[ $$(diff -r test_stree.dot $@.dot) != "" ]]; then :; else echo "Test of '$@' failed"; exit 1; fi

check-local:
	@echo "Test for UA Structural analysis"
	$(BUILD_PATH)/test_StructAnalysisGeneral
	if ! [[ -f $(TEST_PATH)/test_general/test_streeA.dot && \
		-f $(TEST_PATH)/test_general/test_streeA.dot && \
		-f $(TEST_PATH)/test_general/test_streeA.dot ]]; then :; echo " Expected output file missing, test failed" ; exit 1; fi
	if [[ $$(diff -r test_streeA.dot $(TEST_PATH)/test_general/test_streeA.dot) != "" ]]; then :; echo " General/test_streeA test failed" ; exit 1; fi 
	if [[ $$(diff -r test_streeB.dot $(TEST_PATH)/test_general/test_streeB.dot) != "" ]]; then :; echo " General/test_streeB test failed" ; exit 1; fi 
	if [[ $$(diff -r test_streeC.dot $(TEST_PATH)/test_general/test_streeC.dot) != "" ]]; then :; echo " General/test_streeC test failed" ; exit 1; fi
	touch test_stree.dot
#@$(MAKE) $(BIN_CHECK)
	@echo "*********************************************************************************************************************"
	@echo "***** ROSE/projects/uniformSourceBinaryAnalysis/test/StructAnal: make check rule complete (terminated normally) *****"
	@echo "*********************************************************************************************************************"

EXTRA_DIST = \
	$(CXX_TESTCODES_REQUIRED_TO_PASS) $(BINARY_TESTCODES_REQUIRED_TO_PASS:.C=.dot) $(BINARY_TESTCODES_REQUIRED_TO_PASS:.C=) \
	$(TEST_PATH)/test_general/test_streeA.dot \
	$(TEST_PATH)/test_general/test_streeB.dot \
	$(TEST_PATH)/test_general/test_streeC.dot 

clean-local:
	rm -f *.o rose_*.[cC] *.dot rose_*.cc

	


