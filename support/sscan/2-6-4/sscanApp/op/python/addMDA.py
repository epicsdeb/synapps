#!/usr/bin/env python
import sys
from mda import *

def main():
	root = Tkinter.Tk()
	if len(sys.argv) < 3 or sys.argv[1] == '?' or sys.argv[1] == "help" or sys.argv[1][:2] == "-h":
		print "usage: %s [file1] [file2] [sumFile]" % sys.argv[0]
		return()
	else:
		file1 = sys.argv[1]
		file2 = sys.argv[2]
		sumFile = sys.argv[3]
		print file1, " + ", file2, " --> ", sumFile

	d1 = readMDA(file1)
	d2 = readMDA(file2)
	s = opMDA('+',d1,d2)
	writeMDA(s, sumFile)

if __name__ == "__main__":
        main()
