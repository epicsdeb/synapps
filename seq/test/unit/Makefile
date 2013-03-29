TOP=../..

include $(TOP)/configure/CONFIG

PROD_LIBS += Com seq pv

USR_INCLUDES += -I$(TOP)/src/seq

TESTPROD_HOST += queueTest
queueTest_SRCS += queueTest.c
testHarness_SRCS += queueTest.c
TESTS += queueTest

# The testHarness runs all the test programs in a known working order.
testHarness_SRCS += epicsTests.c

# PROD_vxWorks = vxTestHarness
# vxTestHarness_SRCS += $(testHarness_SRCS)
# vxTestHarness_OBJS += $(INSTALL_BIN)/vxComLibrary
# TESTSPEC_vxWorks = vxTestHarness.munch; epicsRunLibComTests
# 
# PROD_RTEMS += rtemsTestHarness
# rtemsTestHarness_SRCS += rtemsTestHarness.c
# rtemsTestHarness_SRCS += $(testHarness_SRCS)
# TESTSPEC_RTEMS = rtemsTestHarness.boot; epicsRunLibComTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES
