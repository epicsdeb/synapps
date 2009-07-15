[schematic2]
uniq 190
[tools]
[detail]
w 50 1259 100 0 n#187 ecalc3.HBt.VAL 224 1136 288 1136 288 1248 -128 1248 -128 1200 -64 1200 ecalc3.HBt.INPA
w -126 1179 100 0 n#186 hwin.hwin#185.in -128 1168 -64 1168 ecalc3.HBt.INPB
s 1664 16 100 0 my_IOC_heartbeat.sch
s 1480 40 100 0 linac/cryo/cryoTopApp/Db/
s 1600 -120 60 0 Sept. 18, 2002
s 1768 -120 60 0 Author: P. Gurd
s 1504 -32 100 0 Define my own IOC heartbeat
[cell use]
use ecalc3 16 1000 100 0 HBt
xform 0 80 1120
p 12 1034 100 0 -1 CALC:A>=B?0:A+1
p -352 1038 100 0 0 EGU:ticks
p -352 1006 100 0 0 PREC:0
p -72 1000 100 0 -1 PV:VAR(P):
p -32 1224 100 0 1 SCAN:VAR(SCAN) second
use hwin -320 1127 100 0 hwin#185
xform 0 -224 1168
p -317 1160 100 0 -1 val(in):VAR(MAX)
use bb200tr -512 -248 -100 0 frame
xform 0 768 576
[comments]
