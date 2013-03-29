package make_test_lib;

use strict;
use Test::More;

sub do_test {
  my ($test) = @_;
  $_ = `make -B -s TESTPROD=$test 2>&1`;
  # uncomment this comment to find out what went wrong:
  #diag("$test result=$?, response=$_");
}

sub check_success {
  ok($? != -1 and $? == 0 and not /error/);
}

sub check_failure {
  my $ne = 0;
  ok($? != -1 and $? != 0);
}

sub do_tests {
  my %tests = @_;

  my @alltests = (
    [\&check_success, $tests{success}],
    [\&check_failure, $tests{failure}],
  );

  plan tests => @{$tests{success}} + @{$tests{failure}};

  foreach my $group (@alltests) {
    my ($check,$tests) = @$group;
    foreach my $test (@$tests) {
      do_test($test);
      &$check();
    }
  }

}

1;
