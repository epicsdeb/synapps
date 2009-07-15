#!/usr/bin/perl

sub Usage
{
	my ($txt) = @_;

	print "Usage:\n";
	print "\tcat.pl infile1 [ infile2 infile3 ...] outfile\n";
	print "\nError: $txt\n" if $txt;

	exit 2;
}

# need at least two args: ARGV[0] and ARGV[1]
Usage("\"cat.pl @ARGV\": No input files specified") if $#ARGV < 1;

$target=$ARGV[$#ARGV];
@sources=@ARGV[0..$#ARGV-1];

open(OUT, "> $target") or die "Cannot create $target\n";;
foreach $file ( @sources )
{
	open(IN, "$file") or die "Cannot open $file\n";;
	my @lines = <IN>;
	print OUT @lines;
	close IN
}

close OUT;

#   EOF cat.pl

