#!/usr/bin/perl
#*************************************************************************
# Copyright (c) 2002      The Regents of the University of California, as
#                         Operator of Los Alamos National Laboratory.
# Copyright (c) 2008      UChicago Argonne LLC, as Operator of Argonne
#                         National Laboratory.
# Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
#                         und Energie GmbH, Germany (HZB)
# This file is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# The makeTestfile.pl script generates a file $target.t which is needed
# because some versions of the Perl test harness can only run test scripts
# that are actually written in Perl.  The script we generate execs the
# real test program which must be in the same directory as the .t file.

# Usage: makeTestfile.pl target.t executable
#     target.t is the name of the Perl script to generate
#     executable is the name of the file the script runs

use strict;

my $valgrind = "";

my ($target, $stem, $exe, $ioc, $softioc, $softdbd, $valgrind_path) = @ARGV;

if ($valgrind_path) {
  `$valgrind_path -h`;
  $valgrind = "$valgrind_path -q --log-file=test " if $? == 0;
}

my $db = "../$stem.db";

my $host_arch = $ENV{EPICS_HOST_ARCH};

open(my $OUT, ">", $target) or die "Can't create $target: $!\n";

my $pid = '$pid';
my $err = '$!';

my $killit;
if ($host_arch =~ /win32/) {
  $killit = 'system("taskkill /F /IM softIoc.exe")';
} else {
  $killit = 'kill 9, $pid or die "kill failed: $!"';
}

if ($ioc eq "ioc") {
  print $OUT <<EOF;
my $pid = fork();
die "fork failed: $err" unless defined($pid);
if (!$pid) {
  exec("$softioc -D $softdbd -S -d $db");
  die "exec failed: $err";
}
system("$valgrind./$exe -S");
$killit;
EOF
} elsif (-r "$db") {
  print $OUT <<EOF;
exec "$valgrind./$exe -S -d $db" or die "exec failed: $err";
EOF
} else {
  print $OUT <<EOF;
exec "$valgrind./$exe -S" or die "exec failed: $err";
EOF
}

close $OUT or die "Can't close $target: $!\n";
