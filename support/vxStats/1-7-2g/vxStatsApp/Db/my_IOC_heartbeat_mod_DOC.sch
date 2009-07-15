[schematic2]
uniq 190
[tools]
[detail]
w 274 1179 100 0 n#189 ecalc3.HBt.FLNK 224 1168 384 1168 384 1104 480 1104 ecalc3.HBtMod.SLNK
w 50 1259 100 0 n#187 ecalc3.HBt.VAL 224 1136 288 1136 288 1248 -128 1248 -128 1200 -64 1200 ecalc3.HBt.INPA
w 354 1211 100 0 n#187 junction 288 1200 480 1200 ecalc3.HBtMod.INPA
w -126 1179 100 0 n#186 hwin.hwin#185.in -128 1168 -64 1168 ecalc3.HBt.INPB
s 1664 16 100 0 my_IOC_heartbeat_mod.sch
s 1480 40 100 0 linac/cryo/cryoTopApp/Db/
s 1600 -120 60 0 Sept. 18, 2002
s 1768 -120 60 0 Author: P. Gurd
s 1488 -16 100 0 Define my own IOC heartbeat
s 1496 -48 100 0 (plus mod 9 for display)
[cell use]
use ecalc3 16 1000 100 0 HBt
xform 0 80 1120
p 12 1034 100 0 -1 CALC:A>=B?0:A+1
p -352 1038 100 0 0 EGU:ticks
p -352 1006 100 0 0 PREC:0
p -72 1000 100 0 -1 PV:VAR(P):
p -32 1224 100 0 1 SCAN:VAR(SCAN) second
use ecalc3 560 1000 100 0 HBtMod
xform 0 624 1120
p 556 1034 100 0 -1 CALC:A%9
p 192 1038 100 0 0 EGU:ticks
p 192 1006 100 0 0 PREC:0
p 472 1000 100 0 -1 PV:VAR(P):
p 192 1262 100 0 0 SCAN:1 second
use hwin -320 1127 100 0 hwin#185
xform 0 -224 1168
p -317 1160 100 0 -1 val(in):VAR(MAX)
use bb200tr -512 -248 -100 0 frame
xform 0 768 576
[comments]
