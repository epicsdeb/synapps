#!/usr/bin/env python

import sys

if len(sys.argv)>=2:
	out=open(sys.argv,'w')
else:
	out=sys.stdout

out.write("""\
#This file was generated using makecontrol.py

Source: synapps
Section: devel
Priority: extra
Maintainer: Michael Davidsaver <mdavidsaver@bnl.gov>
Build-Depends: debhelper (>= 7), epics-dev
Standards-Version: 3.7.3
Homepage: http://www.aps.anl.gov/bcda/synApps/index.php

Package: epics-synapps-doc
Architecture: all
Description: Documentation for synapps
 It contains beamline-control and data-acquisition components for
 an EPICS based control system. synApps is distributed under the EPICS Open license.
""")

mods=['seq','ipac','sscan','autosave','asyn','calc','motor','stream']

deps={}

deps['asyn']=['seq','ipac']
deps['calc']=['sscan']
deps['motor']=['asyn','seq','ipac']
deps['stream']=['asyn','calc','sscan']

sdesc={}
sdesc['seq']='EPICS State Notation Compiler'
sdesc['ipac']='Support for Industry Pack bus carriers (bridges)'
sdesc['sscan']='Pattern scanning record'
sdesc['autosave']='Periodic save and automatic restore on start'
sdesc['asyn']='The Asynchronous Record'
sdesc['calc']='Advanced calculator record'
sdesc['motor']='Support for various motor controllers'
sdesc['stream']='Support and Db generator for command/responce message passing devices'

ldesc={}

fulldeps=reduce(lambda a,b:a+', '+b,map(lambda a:'epics-%s-dev'%a,mods),'')

out.write("""
Package: epics-synapps-dev
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends}, epics-dev%(deps)s
Description: A collection of tools to create beamline control systems.
 synApps is a collection of software tools that help to create a control system for beamlines.
 It contains beamline-control and data-acquisition components for an EPICS based control system.
 synApps is distributed under the EPICS Open license. 
 .
 This is a meta package which will pull in synapps development packages.
""" % {'deps':fulldeps} )

for m in mods:
	devdep=['epics-dev']
	libdep=['epics-libs']

	for md in deps.get(m,[]):
		if md in mods:
			devdep.append('epics-%s-dev'%md)
			libdep.append('epics-%s-libs'%md)
		else:
			devdep.append(md)
			libdep.append(md)
	devdep.append('epics-%s-libs'%m)

	params={'name':m}
	params['sdesc']=sdesc.get(m,'Short Message')
	params['ldesc']=ldesc.get(m,params['sdesc'])
	params['devdep']=reduce(lambda a,b: a+', '+b,devdep,'')
	params['libdep']=reduce(lambda a,b: a+', '+b,libdep,'')

	out.write("""
Package: epics-%(name)s-libs
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends}%(libdep)s
Description: %(sdesc)s
 %(ldesc)s
 .
 This package contains shared libraries.

Package: epics-%(name)s-dev
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends}%(devdep)s
Description: %(sdesc)s
 %(ldesc)s
 .
 This package contains development headers, libraries, and utilities
"""%params)
