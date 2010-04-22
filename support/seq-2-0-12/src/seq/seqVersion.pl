#!/usr/bin/perl
#
# seqVersion - create the seq version module
#

$version = $ARGV[0];
$now = localtime;
print "/* seqVersion.c - version & date */\n";
print "/* Created by seqVersion.pl */\n";
print "char *seqVersion = \"SEQ Version ${version}: ${now}\";\n";
