sub pr_tok {
  my ($type,$value) = @_;
  return "\"$value\"" if $type eq OPERATOR;
  return "`$value`" if $type eq LITERAL;
  return "\"$value\"" if $type eq KEYWORD;
  return "\"$value\"" if $type eq TYPEWORD;
  return "`$value`"if $type eq IDENTIFIER;
  return "\"$value\"" if $type eq DELIMITER;
}

open $fhre, $ARGV[0] or die;
open $fhlem, $ARGV[1] or die;

while (<$fhre>) {
  if (m/(OPERATOR|KEYWORD|TYPEWORD|DELIMITER)\((\w+),\s+"([^"]+)"\)/) {
    $tok{$2} = [$1, $3];
  }
  if (m/(LITERAL|IDENTIFIER)\((\w+),\s+(\w+),/) {
    $tok{$2} = [$1, $3];
  }
}

# test:
# for $t (sort(keys(%tok))) {
#   @td = @{$tok{$t}};
#   printf "%-16s%-16s%-16s\n",$t,$td[0],$td[1];
# }

print ".. productionlist::\n";
while (<$fhlem>) {
  if (my ($l,$r) = /(\w+) ::= ?(.*)\./) {
    my @rws = split(/ /,$r);
    print "   $l: ";
    print join(" ",map(/[a-z_]+/ ? "`$_`" : pr_tok(@{$tok{$_}}), @rws));
    print "\n";
  }
}
