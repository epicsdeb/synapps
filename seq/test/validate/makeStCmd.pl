print "testHarness\n";
foreach my $x (@ARGV) {
  print "\n# with lowered priority:\n";
  print "run_seq_test \&${x}Test, -1\n";
  print "\n# with normal priority:\n";
  print "run_seq_test \&${x}Test\n";
  print "\n# with raised priority:\n";
  print "run_seq_test \&${x}Test, 1\n";
}
print "\nepicsExit\n";
