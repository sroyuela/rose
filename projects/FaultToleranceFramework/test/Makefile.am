include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

TEST_PATH=$(top_srcdir)/projects/FaultToleranceFramework/test
INCLUDES = $(ROSE_INCLUDES) -I$(top_srcdir)/projects/FaultToleranceFramework/include
LDFLAGS = -L$(top_builddir)/projects/FaultToleranceFramework/src
LDADD = $(ROSE_LIBS) -lroseft

bin_PROGRAMS = ft_trans.exe #ft_auto.exe

ft_trans_exe_SOURCES = $(TEST_PATH)/ft_trans.C
#ft_auto_exe_SOURCES = $(TEST_PATH)/ft_auto.C


TRANSFORM_TESTCODES_REQUIRED_TO_PASS = \
				fttest1_C.C \
				fttest2_C.C \
				fttest4_C.C \
				testcaseSmall.C \
				testcase.C

TRANSFORM_TESTCODE_CURRENTLY_FAILING = \
				fttest3_C.C \
				fttest1_Cxx.C \
				fttest1_F.f \
				fttest1_F.f90 \
				fttest2_Cxx.C

TRANSFORM_ALL_TESTCODES = \
	$(TRANSFORM_TESTCODES_REQUIRED_TO_PASS) \
	$(TRANSFORM_TESTCODE_CURRENTLY_FAILING)

TRANSFORM_TRANSLATOR = $(top_builddir)/projects/FaultToleranceFramework/test/ft_trans
TRANSFORM_TEST_PATH = $(TEST_PATH)/Transform
TRANSFORM_OUT_PATH = $(top_builddir)/projects/FaultToleranceFramework/test/Transform

ROSE_COMPILE_FLAGS=

$(TRANSFORM_TRANSLATOR):
	@$(MAKE) -C $(top_builddir)/projects/FaultToleranceFramework/src

TRANSFORM_MAKE = ${TRANSFORM_TESTCODES_REQUIRED_TO_PASS:.C=}
$(TRANSFORM_MAKE): $(TRANSFORM_TRANSLATOR)
		$(TRANSFORM_TRANSLATOR) $(ROSE_COMPILE_FLAGS) -c $(TRANSFORM_TEST_PATH)/$@.C -rose:output $(TRANSFORM_OUT_PATH)/$@.out

check-local:
	@echo "Test for FTTransform"
	@rm -rf Transform/*.out
	@$(MAKE) $(TRANSFORM_MAKE)
	@echo "*****************************************************************************************************************"
	@echo "****** ROSE/projects/FfaultToleranceFramework/test: make check rule complete (terminated normally) **************"
	@echo "*****************************************************************************************************************"

EXTRA_DIST = 

SUBDIRS = Analysis Transform

clean-local:
	rm -f *.o *.output rose_*.[cC]
