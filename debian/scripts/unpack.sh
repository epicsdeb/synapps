#!/bin/bash
set -x

die() {
  echo "$1" >&2
  exit 1
}

CLEAN='--exclude *.lib --exclude *.dll --exclude *.so* --exclude *.a --exclude .svn'

for ff in "$@"
do
    echo "Unpack: $ff"

    orig="$(echo "$ff" | egrep -o "orig-[^.]*")"
    comp="$(echo "$orig" | cut -f 2 -d '-')"
    
    [ -z "$comp" ] && die "Can't extract component from $ff"

    echo "  Comp: $comp"

    [ -d "$comp" ] && rm -rf "$comp"

    [ -e "$comp" ] && die "$comp is not a directory"

    install -d "$comp" || die "Failed to create $comp"

    tar -C "$comp" --strip-components=1 $CLEAN -xaf "$ff" || die "Failed to unpack $ff"

done
