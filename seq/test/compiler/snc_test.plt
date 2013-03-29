# Do not run this inside the source directory!
# Instead, run snc_test.t inside O.$(EPICS_HOST_ARCH)

use strict;
use Test::More;

my $success = {
  sncExOpt_DuplOpt => 0,
  sync_not_monitored => 0,
  syncq_not_monitored => 0,
  include_windows_h => 0,
  namingConflict => 0,
  nesting_depth => 0,
};

my $warning = {
  sncExOpt_UnrecOpt => 1,
  state_not_reachable => 3,
  syncq_no_size => 1,
};

my $error = {
  efArray => 1,
  efGlobal => 3,
  efPointer => 1,
  foreignGlobal => 3,
  misplacedExit => 2,
  pvNotAssigned => 20,
  sync_not_assigned => 1,
  syncq_not_assigned => 1,
  syncq_size_out_of_range => 1,
};

sub do_test {
  my ($test) = @_;
  $_ = `make -B -s $test.c 2>&1`;
  # uncomment this comment to find out what went wrong:
  #diag("$test result=$?, response=$_");
}

sub check_success {
  ok($? != -1 and $? == 0 and not /error/ and not /warning/);
}

sub check_warning {
  my ($num_warnings) = @_;
  my $nw = 0;
  $nw++ while (/warning/g);
  #diag("num_warnings=$nw(expected:$num_warnings)\n");
  ok($? != -1 and $? == 0 and not /error/ and $nw == $num_warnings);
}

sub check_error {
  my ($num_errors) = @_;
  my $ne = 0;
  $ne++ while (/error/g);
  #diag("num_errors=$ne (expected:$num_errors)\n");
  ok($? != -1 and $? != 0 and $ne == $num_errors);
}

plan tests => keys(%$success) + keys(%$warning) + keys(%$error);

my @alltests = (
  [\&check_success, $success],
  [\&check_warning, $warning],
  [\&check_error, $error],
);

foreach my $group (@alltests) {
  my ($check,$tests) = @$group;
  foreach my $test (sort(keys(%$tests))) {
    do_test($test);
    &$check($tests->{$test});
  }
}
