#!/usr/bin/env python

import sys
from mda import *

def main():
	showInfo = 1
	showData = 0
	showEnv = 0
	verbose = 0

	if len(sys.argv) < 2 or sys.argv[1] == '?' or sys.argv[1] == "help" or sys.argv[1][:2] == "-h":
		print "usage: %s [-idev] [filename]" % sys.argv[0]
		print "example: mdaAscii.py -i xxx_0001.mda"
		print "   flags:"
		print "      i -- show information"
		print "      d -- show data (only 1D data is shown)"
		print "      e -- show scan-environment PV's"
		print "      a -- show all (information, data, and scan-environment PV's)"
		print "      v -- tell file reader to be verbose"
		print "\nIf no filename is specified, a file requester window will open."
		return()
	elif sys.argv[1][0] == '-':
		showInfo = 0
		showData = 0
		showEnv = 0
		if ('i' in sys.argv[1]): showInfo = 1
		if ('d' in sys.argv[1]): showData = 1
		if ('e' in sys.argv[1]): showEnv = 1
		if ('v' in sys.argv[1]): verbose = 4
		if ('a' in sys.argv[1]): showInfo = showData = showEnv = 1
		if len(sys.argv) < 3:
			fname = None
		else: 
			fname = sys.argv[2]
	else:
		fname = sys.argv[1]

	d = readMDA(fname, 4, verbose, 0)
	rank = d[0]['rank']
	# number of positioners, detectors
	np = d[rank].np
	nd = d[rank].nd

	min_column_width = 15
	# make sure there's room for the names, etc.
	phead_format = []
	dhead_format = []
	pdata_format = []
	ddata_format = []
	columns = 1
	for i in range(d[rank].np):
		cw = max(min_column_width, len(d[rank].p[i].name)+1)
		cw = max(cw, len(d[rank].p[i].desc)+1)
		cw = max(cw, len(d[rank].p[i].fieldName)+1)
		phead_format.append("%%-%2ds" % (cw-1))
		pdata_format.append("%%- %2d.8f" % (cw-1))
		columns = columns + cw
	for i in range(d[rank].nd):
		cw = max(min_column_width, len(d[rank].d[i].name)+1)
		cw = max(cw, len(d[rank].d[i].desc)+1)
		cw = max(cw, len(d[rank].d[i].fieldName)+1)
		dhead_format.append("%%-%2ds" % (cw-1))
		ddata_format.append("%%- %2d.8f" % (cw-1))
		columns = columns + cw

	# header
	if (showInfo):
		print "### ", d[0]['filename'], " is a ", d[0]['rank'], "dimensional file."
		print "### Number of data points      = [",
		for i in range(d[0]['rank'],1,-1): print "%-d," % d[i].curr_pt,
		print d[1].curr_pt, "]"

		print "### Number of detector signals = [",
		for i in range(d[0]['rank'],1,-1): print "%-d," % d[i].nd,
		print d[1].nd, "]"

	if (showEnv):	
		# scan-environment PV values
		print "#\n# Scan-environment PV values:"
		ourKeys = d[0]['ourKeys']
		maxKeyLen = 0
		for i in d[0].keys():
			if (i not in ourKeys):
				if len(i) > maxKeyLen: maxKeyLen = len(i)
		for i in d[0].keys():
			if (i not in ourKeys):
				print "#", i, (maxKeyLen-len(i))*' ', d[0][i]

	if showInfo:
		print "#\n# ", str(d[1])
		print "#  scan date, time: ", d[1].time
		sep = "#"*columns
		print sep

		# 1D data
		# print table head
		print "#",
		for j in range(d[1].np):
			print phead_format[j] % (d[1].p[j].fieldName),
		for j in range(d[1].nd):
			print dhead_format[j] % (d[1].d[j].fieldName),
		print

		print "#",
		for j in range(d[1].np):
			print phead_format[j] % (d[1].p[j].name),
		for j in range(d[1].nd):
			print dhead_format[j] % (d[1].d[j].name),
		print

		print "#",
		for j in range(d[1].np):
			print phead_format[j] % (d[1].p[j].desc),
		for j in range(d[1].nd):
			print dhead_format[j] % (d[1].d[j].desc),
		print

		print sep

	if (showData):
		# 1D data
		for i in range(d[1].curr_pt):
			print "",
			for j in range(d[1].np):
				print pdata_format[j] % (d[1].p[j].data[i]),
			for j in range(d[1].nd):
				print ddata_format[j] % (d[1].d[j].data[i]),
			print

		# 2D data

		print ""
	
if __name__ == "__main__":
        main()
