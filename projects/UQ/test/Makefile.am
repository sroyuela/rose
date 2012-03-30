include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

CXX_TESTCODES_REQUIRED_TO_PASS = \
				testcase.C

TESTCODE_CURRENTLY_FAILING =

ALL_TESTCODES = \
$(CXX_TESTCODES_REQUIRED_TO_PASS) \
$(TESTCODE_CURRENTLY_FAILING)


EXEC=$(top_builddir)/projects/UQ/src/uq
TEST_PATH = $(top_srcdir)/projects/UQ/test
EXPECTED_PATH = $(top_srcdir)/projects/UQ/test/expected
ROSE_COMPILE_FLAGS=

$(EXEC):
	$(MAKE) -C $(top_builddir)/projects/UQ/src

CXX_TEST_Objects = ${CXX_TESTCODES_REQUIRED_TO_PASS:.C=.exe}
$(CXX_TEST_Objects): $(EXEC) $(CXX_TESTCODES_REQUIRED_TO_PASS)
			$(EXEC) -I$(UQTK_INCLUDE_PATH) $(ROSE_COMPILE_FLAGS) -c $(TEST_PATH)/$(@:.exe=.C) -rose:output rose_$(@:.exe=.C)
			if [[ $$(diff -r rose_$(@:.exe=.C) $(TEST_PATH)/expected/$(@:.exe=.out)) != "" ]]; then echo "Files differ; test falied"; exit 1; fi 

check-local:
	@echo "Test for UQ transformation"
	@rm -rf *.out
	@$(MAKE) $(CXX_TEST_Objects)
	@echo "*******************************************************************************************"
	@echo "****** ROSE/projects/UQ/test: make check rule complete (terminated normally) **************"
	@echo "*******************************************************************************************"

EXTRA_DIST = $(ALL_TESTCODES)

clean-local:
	rm -f *.o rose_*.[cC]
