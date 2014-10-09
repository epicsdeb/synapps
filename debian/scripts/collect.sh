#!/bin/bash
set -x -e

die() {
  echo "$1" >&2
  exit 1
}

warn() {
  echo "$1" >&2
}

usage() {
  die "Usage: `basename "$0"` <VERSION> <output path>"
}

PKG=synapps
VER="$1"
shift
OUT="$1"
shift

[ -n "$VER" ] || usage
[ -n "$OUT" ] && [ -d "$OUT" ] || usage

# $1 - part name (ie asyn)
# $2 - URL
# $3 - file (if omitted then autodetect)
getpart() {
  if [ -z "$3" ]; then
    name="$(echo "$2" | egrep -o '[^/]*$')"
    [ -n "$name" ] || die "Failed to extract name from: $2"
  else
    name="$3"
  fi
  ext="$(echo "$name" | egrep -o '(gz|bz2|lzma)$')"
  [ -n "$ext" ] || die "Failed to extract extension from: $name"

  echo "Get $name"

  if [ -f "$name" ]; then
    echo "  Using existing"

  else
    wget "$2" || die "Failed to get: $2"
    [ -f "$name" ] || die "Extracted wrong name $name from $2"

  fi
  
  oname="$OUT/${PKG}_${VER}.orig-$1.tar.$ext"
  echo "  Put $oname"
  
  cp -lf "$name" "$oname" || die "Can't copy $oname to $OUT"
}

getpart stream http://epics.web.psi.ch/software/streamdevice/StreamDevice-2-6.tgz

getpart seq http://www-csr.bessy.de/control/SoftDist/sequencer/releases/seq-2.1.17.tar.gz

getpart ipac https://svn.aps.anl.gov/trac/epics/ipac/raw-attachment/wiki/V2.12/ipac-2.12.tar.gz

getpart asyn http://www.aps.anl.gov/epics/download/modules/asyn4-23.tar.gz

getpart vme http://www.aps.anl.gov/bcda/synApps/tar/vme_R2-8-1.tar.gz

getpart vac http://www.aps.anl.gov/bcda/synApps/tar/vac_R1-4-1.tar.gz

getpart std http://www.aps.anl.gov/bcda/synApps/tar/std_R3-2.tar.gz

getpart softglue http://www.aps.anl.gov/bcda/synApps/tar/softGlue_R2-4-1.tar.gz

getpart sscan http://www.aps.anl.gov/bcda/synApps/tar/sscan_R2-9.tar.gz

getpart mdautils http://www.aps.anl.gov/bcda/mdautils/packages/mdautils-1.3.1.tar.gz

getpart optics http://www.aps.anl.gov/bcda/synApps/tar/optics_R2-9-2.tar.gz

getpart motor http://www.aps.anl.gov/bcda/synApps/motor/tar/motorR6-8-1.tar.gz

getpart modbus http://cars.uchicago.edu/software/pub/modbusR2-7.tgz

getpart mca http://cars.uchicago.edu/software/pub/mcaR7-5.tgz

getpart love http://www.aps.anl.gov/bcda/synApps/tar/love_R3-2-5.tar.gz

getpart ipunidig http://cars.uchicago.edu/software/pub/ipUnidigR2-9.tgz

getpart ip330 http://cars.uchicago.edu/software/pub/ip330R2-7.tgz

getpart ip http://www.aps.anl.gov/bcda/synApps/tar/ip_R2-15.tar.gz

getpart delaygen http://www.aps.anl.gov/bcda/synApps/delaygen/tar/delaygen_R1-1-0.tar.gz

getpart dac128v http://cars.uchicago.edu/software/pub/dac128VR2-7.tgz

getpart calc http://www.aps.anl.gov/bcda/synApps/tar/calc_R3-4.tar.gz

getpart busy http://www.aps.anl.gov/bcda/synApps/tar/busy_R1-6.tar.gz

getpart autosave http://www.aps.anl.gov/bcda/synApps/tar/autosave_R5-4-2.tar.gz
