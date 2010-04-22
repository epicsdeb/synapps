[schematic2]
uniq 208
[tools]
[detail]
w 1218 891 100 0 n#203 ecalcs.HBtOK_VAR(SIG).FLNK 1152 880 1344 880 1344 848 1408 848 ebos.HBtAlarm_VAR(SIG).SLNK
w 1314 843 100 0 n#195 junction 1312 832 1376 832 1376 880 1408 880 ebos.HBtAlarm_VAR(SIG).DOL
w 882 539 100 0 n#195 ecalcs.HBtOK_VAR(SIG).VAL 1152 848 1312 848 1312 528 512 528 512 960 864 960 ecalcs.HBtOK_VAR(SIG).INPD
w 770 939 100 0 n#200 hwin.hwin#201.in 736 928 864 928 ecalcs.HBtOK_VAR(SIG).INPE
w 1178 1163 100 0 n#198 hwin.hwin#197.in 1072 1152 1344 1152 ecalc3.HBtLast_VAR(SIG).INPA
w 1058 1131 100 0 n#194 junction 832 1056 832 1120 1344 1120 ecalc3.HBtLast_VAR(SIG).INPB
w 770 1067 100 0 n#194 hwin.hwin#193.in 736 1056 864 1056 ecalcs.HBtOK_VAR(SIG).INPA
w 1122 507 100 0 n#199 ecalc3.HBtLast_VAR(SIG).VAL 1632 1088 1824 1088 1824 496 480 496 480 992 864 992 ecalcs.HBtOK_VAR(SIG).INPC
w 690 1035 100 0 n#186 hwin.hwin#185.in 576 1024 864 1024 ecalcs.HBtOK_VAR(SIG).INPB
f -384 896 192 1216 100 3072 Variables
f -384 672 288 864 100 3072 PVs
f -384 448 352 640 100 3072 PVs_other
s 1496 -32 100 0 Test Other IOC heartbeat
s 1768 -120 60 0 Author: P. Gurd
s 1600 -120 60 0 Sept. 18, 2002
s 1480 40 100 0 linac/cryo/cryoTopApp/Db/
s 1632 16 100 0 test_his_IOC_heartbeat.sch
s -336 1128 100 0 P - my own IOC's name (eg: Cryo_ICS:IOC20)
s -336 1096 100 0 P_his - name of other IOC
s -336 1064 100 0 SIG_his - ID for signal part of heartbeat
s -216 1040 100 0 name
s -336 1000 100 0 TICKS - how many ticks of my heartbeat
s -216 976 100 0 before MAJOR alarm
s -336 936 100 0 SCAN - SCAN every this number of seconds
s -144 1176 100 0 Variables
s -368 576 100 0 $(P):HBt - my own IOC's heartbeat (eg: Cryo_ICS:IOC20:HBt)
s -368 512 100 0 $(P_his):HBt - other IOC's heartbeat
s -352 800 100 0 $(P):HBtOK_$(SIG_his) - my opinion of the health
s -280 840 100 0 PVs defined in this file
s -288 544 100 0 (defined in my_IOC_heartbeat.sch)
s -288 480 100 0 (defined in his copy of my_IOC_heartbeat.sch)
s -296 768 100 0 of his IOC, MAJOR alarm if > $(TICKS)
s -352 736 100 0 $(P):HBtLast_$(SIG_his) - my heartbeat value
s -296 704 100 0 the last time I got a monitor from his heartbeat
s -280 616 100 0 PVs defined in other files
s -360 360 100 0 The translation process translates
s -280 296 100 0 $() in the .db file, and
s -280 328 100 0 VAR() from this .sch file to
s -280 264 100 0 doesn't carry $() through.
s -272 400 100 0 A note about VAR()/$()
s -360 232 100 0 I've used the $() notation in the above comments
s -336 904 100 0 MAX - my heartbeat counts up to this value
[cell use]
use ebos 1504 744 100 0 HBtAlarm_VAR(SIG)
xform 0 1536 832
p 1088 782 100 0 0 OMSL:closed_loop
p 1488 856 100 0 1 ONAM:Fault
p 1312 814 100 0 0 OSV:MAJOR
p 1416 744 100 0 -1 PV:VAR(P):
p 1088 718 100 0 0 ZNAM:OK
p 1088 412 100 0 0 typ(OUT):path
use ecalc3 1424 944 100 0 HBtLast_VAR(SIG)
xform 0 1488 1072
p 1420 986 100 0 -1 CALC:B
p 1440 1184 100 0 1 EGU:ticks
p 1336 944 100 0 -1 PV:VAR(P):
use hwin 880 1111 100 0 hwin#197
xform 0 976 1152
p 880 1184 100 0 -1 val(in):VAR(P_his):HBt
use hwin 544 1015 100 0 hwin#193
xform 0 640 1056
p 552 1080 100 0 -1 val(in):VAR(P):HBt
use hwin 384 983 100 0 hwin#185
xform 0 480 1024
p 387 1016 100 0 -1 val(in):VAR(TICKS)
use hwin 544 887 100 0 hwin#201
xform 0 640 928
p 552 920 100 0 -1 val(in):VAR(MAX)
use ecalcs 944 568 100 0 HBtOK_VAR(SIG)
xform 0 1008 832
p 971 1048 100 0 -1 CALC:D?D:(C>A?(E+A):A)>(C+B)
p 1048 968 100 0 1 HIGH:1.0
p 832 718 100 0 0 HSV:MAJOR
p 856 568 100 0 -1 PV:VAR(P):
p 1008 1008 100 0 1 SCAN:VAR(SCAN) second
use bb200tr -512 -248 -100 0 frame
xform 0 768 576
[comments]
