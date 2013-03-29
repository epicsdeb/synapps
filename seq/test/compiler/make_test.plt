# Do not run this inside the source directory!
# Instead, run make_test.t inside O.$(EPICS_HOST_ARCH)

use strict;
use lib "..";
use make_test_lib;

make_test_lib::do_tests(
  success => [qw(
    namingConflict
    tooLong
  )],
  failure => [qw(
    varinit
    varinitOptr
  )]
);
