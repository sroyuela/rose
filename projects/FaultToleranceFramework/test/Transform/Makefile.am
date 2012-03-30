include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

TEST_PATH = $(top_srcdir)/projects/FaultToleranceFramework/test/Transform
EXPECTED_PATH = $(top_srcdir)/projects/FaultToleranceFramework/test/Transform/expected

TEST_CODES = PATH/fttest1_C \
	PATH/fttest1_Cxx.C \
	PATH/fttest1_F.f \
	PATH/fttest1_F.f90 \
	PATH/fttest2_C.C \
	PATH/fttest2_Cxx.C \
	PATH/fttest3_C.C \
	PATH/fttest4_C.C \
	PATH/testcaseSmall.C \
	PATH/testcase.C

EXTRA_DIST = ${TEST_CODES: PATH=$(EXPECTED_PATH)} ${${TEST_CODES:.C=.expected}: PATH=$(TEST_PATH)}
