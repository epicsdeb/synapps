#!/usr/bin/env python

#from ca_util import *
import epics
from math import *
from string import *
import time
import sys

import os
#os.environ['EPICS_CA_ADDR_LIST'] = "164.54.53.99"

sCalcRecord = "xxx:userStringCalc2"
calc = sCalcRecord + ".CALC"
result = sCalcRecord + ".VAL"
sresult = sCalcRecord + ".SVAL"

small = 1.e-9

# Initialize an sCalcout record's fields for testing
A2L = "ABCDEFGHIJKL"

A=1.
B=2.
C=3.
D=4.
E=5.
F=6.
G=7.
H=8.
I=9.
J=10.
K=11.
L=12.
AA = "string 1"
BB = "string 2"
CC = "string 3"
DD = "string 4"
EE = "string 5"
FF = "string 6"
GG = "string 7"
HH = "string 8"
II = "string 9"
JJ = "string 10"
KK = "string 11"
LL = "xxx:scan1.EXSC"

def init(sCalcRecord):
	calc = sCalcRecord + ".CALC"
	epics.caput(calc, "0")
	for i in range(12):
		epics.caput(sCalcRecord + "." + A2L[i], eval(A2L[i]))
		epics.caput(sCalcRecord + "." + A2L[i] + A2L[i], eval(A2L[i]+A2L[i]) )

# List of expressions for testing
# exp = [(sCalc_expression, equivalent_python_expression), ...]
# If equivalent_python_expression == None, then the sCalc_expression can
# be evaluated as-is by python.
exp = [
	("tan(A)", None),
	("sin(B)", None),
	("max(A,B,C)", None),
	("min(D,E,F)", None),
	("A<<2", "int(A)<<2"),
	("L>>1", "int(L)>>1"),
	("A?B:C", "(B,C)[A==0]"),
	("A&&B", "(A and B) != 0"),
	("A||B", "(A or B) != 0"),
	("LL[0,'.']", "LL[0:LL.find('.')]"),
	("A>B", None),
	("A>B?BB:AA[A,A]", "(AA[nint(A):nint(A+1)],BB)[A>B]"),
	("A>=4", None),
	("A=0?1:0", "(0,1)[A==0]"),
	("A+B", None),
	("(B=0)?(A+16384):A+B", "(A+B,A+16384)[B==0]"),
	("1.e7/A", None),
	("A>9?1:0", "(0,1)[A>9]"),
	("A%10+1", None),
	("!A", "not A"),
	("C+((A-E)/(D-E))*(B-C)", None),
	("A#B", "A!=B"),
	("E+nint(D*((A-C)/(B-C)))", None),
	("(A+1)*1000", None),
	("printf('!PFCU%02d ', a)+aa[0,1]", "('!PFCU%02d ' %A)+AA[0:1+1]"),
	("$P('!PFCU%02d E ', a) + $P('%d',b*100)", "('!PFCU%02d E ' % A) + ('%d'%(B*100))"),
	("B?0:!A", "(0,not A)[B==0]"),
	("1", None),
	("A?0:B", "(0,B)[A==0]"),
	("A&&B&&!I", "(((A and B) != 0) and (not I)) != 0"),
	("(A&B&C&D)=1", "(nint(A)&nint(B)&nint(C)&nint(D))==1"),
	("(A&B&C&D&E&F&G&H)=1", "(nint(A)&nint(B)&nint(C)&nint(D)&nint(E)&nint(F)&nint(G)&nint(H))==1"),
	("(A>15)&&B", "((A>15) and B) != 0"),
	("(ABS(A)>1)&&B", "((abs(A)>1) and B) != 0"),
	("(A)=1", "(A)==1"),
	("A*4095", None),
	("(A||B||C||D||E||F)?1:0", "(1,0)[(A or B or C or D or E or F)==0]"),
	("(A<B)?C:D", "(C,D)[(A<B)==0]"),
	("A/(10**(B-2))", None),
	("(F<0.01)?0:(((A+C-B-D)/F)*E)", "(0,(((A+C-B-D)/F)*E))[(F<0.01)==0]"),
	("((A+B)<0.01)?0:(((A-B)/(A+B))*E)", "(0,(((A-B)/(A+B))*E))[((A+B)<0.01)==0]"),
	("((A+B)<0.01)?0:(((A-B)/(A+B))*E)", "(0,(((A-B)/(A+B))*E))[((A+B)<0.01)==0]"),
	("(A - ((B*C)/D))", None),
	("A-B*C/D", None),
	("A/(10**(B-2))", None),
	("min(max(C,A/B),D)", None),
	("(A=B)?C:D", "(C,D)[(A==B)==0]"),
	("A?A+(B|C|D|E):F", "(A+(int(round(B))|int(round(C))|int(round(D))|int(round(E))),F)[A==0]"),
	("D?4:C?3:B?2:A?1:0", "(4,((3,(2,(1,0)[A==0])[B==0])[C==0]))[D==0]"),
	("a>0?1:0", "(1,0)[(A>0)==0]"),
	("(LL['.',-1]=='EXSC') && A", "(LL[LL.find('.')+1:]=='EXSC') and A"),
	("AA + BB", None),
	("A?'A':'!A'", "('A','!A')[A==0]"),
	("'$(P)$(SM)CalcMove.CALC PP MS'", None),
	("A=1", "A==1"),
	("A&(I||!J)&(K||!L)", "nint(A)&nint(I or not J)&nint(K or not L)"),
	("(A||!B)&(C||!D)&(E||!F)&(G||!H)", "nint(A or not B)&nint(C or  not D)&nint(E or not F)&nint(G or not H)"),
	("(A-1.0)", None),
	("A&&C?A-1:B", "(A-1,B)[(A and C)==0]"),
	("C+(A/D)*(B-C)", None),
	("nint(4095*((A-C)/(B-C)))", None),
	("SSCANF(AA,'%*6c%f')", "1"),
	(".005*A/8", None),
	("A=0||a=2", "A==0 or A==2"),
	("A?2:1", "(2,1)[A==0]"),
	("AA+(BB)+CC", None),
	("A*1.0", None),
	("$P('RSET %d;RSET?',A)", "'RSET %d;RSET?' % A"),
	('INT("1234")', 'atoi("1234")'),
	('DBL("1234")', 'float("1234")'),
	("$P('SETP %5.2f;SETP?',A)", "'SETP %5.2f;SETP?' % A"),
	("$P('RAMP %d;RAMP?',A)", "'RAMP %d;RAMP?' % A"),
	("$P('RAMPR %5.2f;RAMPR?',A)", "'RAMPR %5.2f;RAMPR?' % A"),
	('DBL("12345678"[4,7])', 'float("12345678"[4:7+1])'),
	("DD+AA+EE+BB", None),
	("log(A)", None),
	("-(-2)**2", "--2**2"),
	("--2**2", None),
	("A--B",None),
	("A--B*C",None),
	("A+B*C",None),
	("D*((A-B)/C)", None),
	('SSCANF("-1","%d")', "-1"),
	('SSCANF("-1","%hd")', "-1"),
	('SSCANF("-1","%ld")', "-1"),
	('"abcdef"{"bc","gh"}', '"aghdef"'),
	("'yyy:'+'xxx:abc'-'xxx:'", '"yyy:abc"'),
	("@@0:=BB;AA;aa:='string 1'", "BB"),
	("a:=-1;@@-a:=AA;BB;a:=1;bb:='string 2'", "AA")
]

def nint(x):
	return int(floor(x+.5))

def test(sCalcRecord):
	numErrors = 0
	init(sCalcRecord)
	calc = sCalcRecord + ".CALC"
	result = sCalcRecord + ".VAL"
	sresult = sCalcRecord + ".SVAL"

	for e in exp:
		epics.caput(calc,e[0], wait=True)
		time.sleep(.1)
		rtry = epics.caget(result)
		stry = epics.caget(sresult)
		if (e[1]):
			r = eval(e[1])
			print "\n", e[0], "-->", e[1]
		else:
			r = eval(e[0])
			print "\n", e[0]
		if ((type(r) == type(1.0)) or (type(r) == type(1))):
			if (abs(r - rtry) < small):
				print "OK\t", "rtry=",rtry, ", r=",r
			else:
				print "ERROR\t", "rtry=",rtry, ", stry=",stry, ", r=",r
				numErrors = numErrors+1
		elif (type(r) == type(True)):
			if ((abs(rtry) < small) == (r == False)):
				print "OK\t", "rtry=",rtry, ", r=",r
			else:
				print "ERROR\t", "rtry=",rtry, ", stry=",stry, ", r=",r
				numErrors = numErrors+1
		elif (r == stry):
			print "OK\t", "stry=",stry, ", r=",r
		else:
			print "ERROR\t", "rtry=",rtry, ", stry=",stry, ", r=",r
			numErrors = numErrors+1

	print "\n------------------------"
	print "  ", numErrors, " errors."
	print "------------------------"

if __name__ == '__main__':
	if (len(sys.argv) > 1) :
		test(sys.argv[1])
	else:
		test("xxx:userStringCalc10")
