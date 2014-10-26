#!/usr/bin/env python

import epics
from math import *
from string import *
import time
import copy
import os
import sys
#os.environ['EPICS_CA_ADDR_LIST'] = "164.54.53.99"

aCalcRecord = "xxxL:userArrayCalc10"
calc = aCalcRecord + ".CALC"
result = aCalcRecord + ".VAL"
aresult = aCalcRecord + ".AVAL"

small = 1.e-9

def nint(a):
	if (a>0):
		return(int(a+.5))
	else:
		return(int(a-.5))

def arraySum(a1, a2):
	minlen = min(len(a1), len(a2))
	result = []
	for i in range(minlen):
		result.append(a1[i]+a2[i])
	print a1[:minlen], " + ", a2[:minlen], " = ", result
	return result

def arrayCum(a):
	result = copy.copy(a)
	for i in range(len(a)-1,-1,-1):
		for j in range(0,i):
			print "i=", i, "j=", j
			result[i] = result[i]+a[j]
	return result

def same(a1, a2, l):
	result = True
	for i in range(l):
		result = result and (a1[i]==a2[i])
	return result

# Initialize an aCalcout record's fields for testing
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
AA = [1,2,3]
BB = [4,5,6]
CC = [7,8,9]
DD = [-1,0,1]
EE = [0,1,2]
FF = [16,17,18]
GG = [19,20,21]
HH = [1,2,3]
II = [1,2,3]
JJ = [1,2,3]
KK = [1,2,3]
LL = [1,2,3]

def init(aCalcRecord):
	calc = aCalcRecord + ".CALC"
	epics.caput(calc, "0")
	for i in range(12):
		#print "connecting to", aCalcRecord + "." + A2L[i]
		epics.caput(aCalcRecord + "." + A2L[i], eval(A2L[i]))
		#print "connecting to", aCalcRecord + "." + A2L[i] + A2L[i]
		epics.caput(aCalcRecord + "." + A2L[i] + A2L[i], eval(A2L[i]+A2L[i]) )

# Dictionary of possible expression elements
all_elements = {"@":0,
"@@":0,
"!":0,
"-":0,
".":0,
"0":0,
"1":0,
"2":0,
"3":0,
"4":0,
"5":0,
"6":0,
"7":0,
"8":0,
"9":0,
"A":0,
"AA":0,
"ABS":0,
"ACOS":0,
"ARR":0,
"ARNDM":0,
"ASIN":0,
"ATAN":0,
"ATAN2":0,
"AVAL":0,
"AVG":0,
"B":0,
"BB":0,
"C":0,
"CC":0,
"CEIL":0,
"COS":0,
"COSH":0,
"CUM":0,
"D":0,
"DD":0,
"DBL":0,
"DERIV":0,
"D2R":0,
"E":0,
"EE":0,
"EXP":0,
"F":0,
"FF":0,
"FINITE":0,
"FITPOLY":0,
"FITMPOLY":0,
"FLOOR":0,
"FWHM":0,
"G":0,
"GG":0,
"H":0,
"HH":0,
"I":0,
"II":0,
"INT":0,
"ISINF":0,
"ISNAN":0,
"IX":0,
"IXMAX":0,
"IXMIN":0,
"IXZ":0,
"IXNZ":0,
"J":0,
"JJ":0,
"K":0,
"KK":0,
"L":0,
"LL":0,
"LN":0,
"LOG":0,
"LOGE":0,
"M":0,
"AMAX":0,
"AMIN":0,
"MAX":0,
"MIN":0,
"N":0,
"NINT":0,
"NDERIV":0,
"NOT":0,
"NRNDM":0,
"NSMOO":0,
"O":0,
"P":0,
"PI":0,
"R2D":0,
"R2S":0,
"RNDM":0,
"SIN":0,
"SINH":0,
"SMOO":0,
"SQR":0,
"SQRT":0,
"STD":0,
"SUM":0,
"S2R":0,
"TAN":0,
"TANH":0,
"VAL":0,
"LEN":0,
"UNTIL":0,
"~":0,
"!=":0,
"#":0,
"%":0,
"&":0,
"&&":0,
")":0,
"[":0,
"":0,
"]":0,
"}":0,
"*":0,
"**":0,
"+":0,
",":0,
"-":0,
"/":0,
":":0,
":=":0,
";":0,
"<":0,
"<<":0,
"<=":0,
"=":0,
"==":0,
">":0,
">=":0,
">>":0,
"?":0,
"AND":0,
"OR":0,
"XOR":0,
"^":0,
"|":0,
"||":0,
">?":0,
"<?":0}

# List of expressions for testing
# expressions = [(aCalc_expression, equivalent_python_expression), ...]
# If equivalent_python_expression == None, then the aCalc_expression can
# be evaluated as-is by python.
expressions = [
	("finite(1)", "r=1."),
	("finite(AA)", "r=1."),
	("isnan(1)", "r=0."),
	("isnan(AA)", "r=0."),
	("isinf(1)", "r=0."),
	("isinf(AA)", "r=0."),
	("PI/S2R", "r=180*3600."),
	("R2S*PI", "r=180*3600."),
	("EXP(2)", "r=exp(2)"),
	("1.234", None),
	("tan(A)", None),
	("sin(B)", None),
	("max(A,B,C)", None),
	("min(D,E,F)", None),
	("max(AA,B,C)", "ll=len(AA);ab=[B]*ll;ac=[C]*ll;r=[max(AA[i],ab[i],ac[i]) for i in range(ll)]"),
	("max(A,B,CC)", "ll=len(CC);aa=[A]*ll;ab=[B]*ll;r=[max(aa[i],ab[i],CC[i]) for i in range(ll)]"),
	("max(A,BB,C)", "ll=len(BB);aa=[A]*ll;ac=[C]*ll;r=[max(aa[i],BB[i],ac[i]) for i in range(ll)]"),
	("A<<2", "r=int(A)<<2"),
	("L>>1", "r=int(L)>>1"),
	("A?B:C", "r=(B,C)[A==0]"),
	("A&&B", "r=(A and B) != 0"),
	("A||B", "r=(A or B) != 0"),
	("LL[0,1]", "r=LL[0:2]"),
	("AA{1,2}", "r=[0,2,3]"),
	("A>B", None),
	("A>B?BB:AA[A,A]", "r=(AA[nint(A):nint(A+1)],BB)[A>B]"),
	("A<B?BB:AA[A,A]", "r=(AA[nint(A):nint(A+1)],BB)[A<B]"),
	("A>=4", None),
	("A=0?1:0", "r=(0,1)[A==0]"),
	("A+B", None),
	("(B=0)?(A+16384):A+B", "r=(A+B,A+16384)[B==0]"),
	("1.e7/A", None),
	("A>9?1:0", "r=(0,1)[A>9]"),
	("A%10+1", None),
	("!A", "r=not A"),
	("C+((A-E)/(D-E))*(B-C)", None),
	("A#B", "r=A!=B"),
	("E+nint(D*((A-C)/(B-C)))", None),
	("(A+1)*1000", None),
	("B?0:!A", "r=(0,not A)[B==0]"),
	("1", None),
	("A?0:B", "r=(0,B)[A==0]"),
	("A&&B&&!I", "r=(((A and B) != 0) and (not I)) != 0"),
	("(A&B&C&D)=1", "r=(nint(A)&nint(B)&nint(C)&nint(D))==1"),
	("(A&B&C&D&E&F&G&H)=1", "r=(nint(A)&nint(B)&nint(C)&nint(D)&nint(E)&nint(F)&nint(G)&nint(H))==1"),
	("(A>15)&&B", "r=((A>15) and B) != 0"),
	("(ABS(A)>1)&&B", "r=((abs(A)>1) and B) != 0"),
	("(A)=1", "r=(A)==1"),
	("A*4095", None),
	("(A||B||C||D||E||F)?1:0", "r=(1,0)[(A or B or C or D or E or F)==0]"),
	("(A<B)?C:D", "r=(C,D)[(A<B)==0]"),
	("A/(10**(B-2))", None),
	("(F<0.01)?0:(((A+C-B-D)/F)*E)", "r=(0,(((A+C-B-D)/F)*E))[(F<0.01)==0]"),
	("((A+B)<0.01)?0:(((A-B)/(A+B))*E)", "r=(0,(((A-B)/(A+B))*E))[((A+B)<0.01)==0]"),
	("((A+B)<0.01)?0:(((A-B)/(A+B))*E)", "r=(0,(((A-B)/(A+B))*E))[((A+B)<0.01)==0]"),
	("(A - ((B*C)/D))", None),
	("A-B*C/D", None),
	("A/(10**(B-2))", None),
	("min(max(C,A/B),D)", None),
	("(A=B)?C:D", "r=(C,D)[(A==B)==0]"),
	("A?A+(B|C|D|E):F", "r=(A+(int(round(B))|int(round(C))|int(round(D))|int(round(E))),F)[A==0]"),
	("D?4:C?3:B?2:A?1:0", "r=(4,((3,(2,(1,0)[A==0])[B==0])[C==0]))[D==0]"),
	("a>0?1:0", "r=(1,0)[(A>0)==0]"),
	("AA + BB", "r=arraySum(AA,BB)"),
	("A=1", "r=A==1"),
	("A&(I||!J)&(K||!L)", "r=nint(A)&nint(I or not J)&nint(K or not L)"),
	("(A||!B)&(C||!D)&(E||!F)&(G||!H)", "r=nint(A or not B)&nint(C or  not D)&nint(E or not F)&nint(G or not H)"),
	("(A-1.0)", None),
	("A&&C?A-1:B", "r=(A-1,B)[(A and C)==0]"),
	("C+(A/D)*(B-C)", None),
	("nint(4095*((A-C)/(B-C)))", None),
	(".005*A/8", None),
	("A=0||a=2", "r=A==0 or A==2"),
	("A?2:1", "r=(2,1)[A==0]"),
	("AA+(BB)+CC", "r=arraySum(arraySum(AA,BB),CC)"),
	("A*1.0", None),
	("DD+AA+EE+BB", "r=arraySum(arraySum(DD,AA),arraySum(EE,BB))"),
	("log(A)", None),
	("-(-2)**2", "r=--2**2"),
	("--2**2", None),
	("A--B",None),
	("A--B*C",None),
	("A+B*C",None),
	("D*((A-B)/C)", None),
	# the following two must be in order, because of the store to c
	("A?0;C:=3.3:B", "r=(0,B)[A==0]"),
	("C;C:=3", "r=3.3"),
	# the following two must be in order, because of the store to c
	("A?(0;C:=3.3):B", "r=(0,B)[A==0]"),
	("C;C:=3", "r=3.3"),
	# The following changes AA to BB and back, but depends on AA=[1,2,3]
	("@@0:=BB;AA;aa:=aa-3[0,2]", "r=BB"),
	("CUM(BB)", "r=arrayCum(BB)"),
	("a:=-7;@@-a:=BB;HH;a:=1;hh:=ii", "r=BB"),
	("cos(pi)", None),	
	("sum(BB)", "r=BB[0]+BB[1]+BB[2]"),	
	("avg(BB[0,2])", "r=(BB[0]+BB[1]+BB[2])/3"),	
	("ceil(1.5)", None),	
	("ceil(-1.5)", None),
	("floor(1.5)", None),	
	("floor(-1.5)", None),
	("1!=2", None),
	("D2R", "r=pi/180"),
	("R2D", "r=180/pi"),
	("atan(pi)", None),
	("1<=2", None),
	("loge(10)", "r=log(10)"),
	("ln(10)", "r=log(10)"),
	("log(10)", "r=log(10,10)"),
	("1==2", None),
	("cosh(pi)", None),
	("sinh(pi)", None),
	("tanh(pi)", None),
	("amax(aa[0,2])", "r=max(AA[0:3])"),
	("amin(aa[0,2])", "r=min(AA[0:3])"),
	("ixmax(aa[0,2])", "r=2"),
	("ixmin(aa[0,2])", "r=0"),
	("atan2(10,5)", "r=atan2(5,10)"),
	("sqr(10)", "r=sqrt(10)"),
	("sqrt(10)", "r=sqrt(10)"),
	("2^3", "r=2**3"),
	("2**3", None),
	("a<?b", "r=min(A,B)"),
	("a>?b", "r=max(A,B)"),
	("5xor3", "r=5^3"),
	("amax(aa)", "r=3"),
	("asin(.3)", None),
	("acos(.3)", None),
	("ix[1,3]", "r=AA"),
	("~1", None),
	("ixz(dd)", "r=1"),
	("ixnz(ee)", "r=1"),
	("cat(aa[0,2],bb[0,2])", "r=[1,2,3,4,5,6]"),
	("cat(aa[0,2],7)", "r=[1,2,3,7]")
]

def nint(x):
	return int(floor(x+.5))

def test1(i):
	e = expressions[i]
	epics.caput(calc,e[0], wait=True)
	#time.sleep(5)
	rtry = epics.caget(result)
	atry = epics.caget(aresult)
	if (e[1]):
		r = eval(e[1])
		print "\n", e[0], "-->", e[1]
	else:
		r = eval(e[0])
		print "\n", e[0]
	if ((type(r) == type(1.0)) or (type(r) == type(1))):
		if (abs(r - rtry) < small):
			print "OK\t", "rtry=",rtry, ", r=",r
			return(0)
		else:
			print "ERROR\t", "rtry=",rtry, ", atry=",atry, ", r=",r
			return(1)
	elif (type(r) == type(True)):
		if ((abs(rtry) < small) == (r == False)):
			print "OK\t", "rtry=",rtry, ", r=",r
			return(0)
		else:
			print "ERROR\t", "rtry=",rtry, ", atry=",atry, ", r=",r
			return(1)
	else:
		print "ERROR\t", "rtry=",rtry, ", atry=",atry, ", r=",r
		return(1)

def test(aCalcRecord):
	print "using acalcout record '%s'" % aCalcRecord
	init(aCalcRecord)
	calc = aCalcRecord + ".CALC"
	result = aCalcRecord + ".VAL"
	aresult = aCalcRecord + ".AVAL"

	numErrors = 0
	numTested = 0
	for e in expressions:
		epics.caput(calc,e[0], wait=True)
		time.sleep(.1)
		rtry = epics.caget(result)
		atry = epics.caget(aresult)
		if (e[1]):
			exec(e[1])
			print "\n", e[0], "-->", e[1]
		else:
			r = eval(e[0])
			print "\n", e[0]
		numTested = numTested + 1
		print "type(r)=", type(r), "type(rtry)=", type(rtry)
		if ((type(r) == type(1.0)) or (type(r) == type(1))):
			if (abs(r - rtry) < small):
				print "OK(s)\t", "rtry=",rtry, ", r=",r
			else:
				print "ERROR(s)\t", "rtry=",rtry, ", atry=",atry, ", r=",r
				numErrors = numErrors+1
		elif (type(r) == type(True)):
			if ((abs(rtry) < small) == (r == False)):
				print "OK(b)\t", "rtry=",rtry, ", r=",r
			else:
				print "ERROR(b)\t", "rtry=",rtry, ", atry=",atry, ", r=",r
				numErrors = numErrors+1
		elif (type(r) == type([1,2])):
			minlen = min(len(r), len(atry))
			if same(r, atry, minlen):
			#if (r[:minlen] == atry[:minlen]):
				print "OK(a)\t", "atry=",atry[:minlen], ", r=",r
			else:
				print "ERROR(a)\t", "rtry=",rtry, ", atry=",atry[:minlen], ", r=",r[:minlen]
				numErrors = numErrors+1
		else:
			print "ERROR(?)\t", "rtry=",rtry, ", atry=",atry, ", r=",r
			numErrors = numErrors+1

	print "\n------------------------"
	print "   %d errors out of %d tested expressions" % (numErrors, numTested)
	print "------------------------"

def showCoverage():
	for e in expressions:
		e_upper = e[0].upper()
		for key in all_elements.keys():
			if key in e_upper:
				all_elements[key]=1
	print("\nNot tested:")
	for key in all_elements.keys():
		if (all_elements[key] == 0):
			print key,",",




if __name__ == '__main__':
	if (len(sys.argv) > 1) :
		test(sys.argv[1])
	else:
		test("xxx:userStringCalc10")
	showCoverage()
