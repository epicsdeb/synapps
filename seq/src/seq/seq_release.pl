#!/usr/bin/perl
#
# create the seq release header file
#
$release = $ARGV[0];
$now = localtime;
print "#define SEQ_RELEASE \"Sequencer release $release, compiled $now\"\n";
($major,$minor,$patch) = split(/[.]/, $release);
printf "#define MAGIC %d%03d%03d\n", $major,$minor,$patch;
