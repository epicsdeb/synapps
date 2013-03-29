# Do not run this inside the source directory!
# Instead, run make64_test.t inside O.$(EPICS_HOST_ARCH)

use strict;
use lib "..";
use make_test_lib;

make_test_lib::do_tests(
  success => [qw(
  )],
  failure => [qw(
    namingConflict
    tooLong
    varinit
    varinitOptr
  )]
);
