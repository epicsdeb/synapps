Defining IOC heartbeats
-----------------------

BEWARE: Note cautionary note about CapFast schematic files below.

The standard 1 second IOC heartbeat has been incorporated into
"stats.template".  Thus it is no longer necessary to use
"my_IOC_heartbeat.db" or "my_IOC_heartbeat_mod.db" unless you
have special requirements.

The IOC heartbeat files afford a way for an IOC to form an opinion
about the health of another IOC.  Similarly, the template can be used to
monitor the health of a PLC.

Each IOC that is important to others must have a heartbeat as defined in
"stats.template".  If the standard parameters are inadequate, you may use
the file "my_IOC_heartbeat.db" or the file "my_IOC_heartbeat_mod.db" to
define another heartbeat.  "my_IOC_heartbeat_mod.db" also defines a mod 9
version for animated display.  The PV name issues have not been worked out.

Then "test_his_IOC_heartbeat.db" can be used to test whether another IOC is
still running and still communicating.  It can also be used to test whether
a PLC or an internal process is still running and still communicating.
(I opted not to rename the file "test_his_IOC_or_PLC_heartbeat.db" after
I realized it could be used this way, though perhaps a rename would be
less confusing.)

PV Names
--------
In the following, <my_ioc> is the official device name of my IOC and <his_sigid>
is the identifier for my PVs about his heartbeat.  These signal names are being
added to the official SNS names.

These signals are defined in "stats.template":
<my_ioc>:HBt - calc - my heartbeat
<my_ioc>:HBtMod - calc - my heartbeat(mod 9) - used for animated display

These signals are defined in "test_his_IOC_heartbeat.template":
<my_ioc>:HBtOK_<his_sigid> - calc - my calculation of whether his heartbeat is
		okay, 0 is okay, 1 is bad.
<my_ioc>:HBtLast_<his_sigid> - calc - the value of my own heartbeat the last
		time I got a monitor from his heartbeat.
<my_ioc>:HBtAlarm_<his_sigid> - bo - binary heartbeat status, 0 is OK, 1 is
		Fault (SEVR: MAJOR)
For example, if the IOC Cryo_ICS:IOC20 needs to test the IOC Cryo_ICS:IOC30, it
uses the substitution file "ioc20.substitutions" (see below)
to define Cryo_ICS:IOC20:HBt and Cryo_ICS:IOC20:HBtMod using
"stats.template" and Cryo_ICS:IOC20:HBtOK_30 and
Cryo_ICS:IOC20:HBtLast_30 using "test_his_IOC_heartbeat.template".

The following names are used in monitoring PLC health.  <plc> is the official
device name of a PLC, and <plc_sigid> is the signal-name identifier of a PLC.
<plc>:HBtCmd - a copy of the IOC heartbeat sent to the PLC.
<plc>:HBt - a heartbeat produced by the PLC.
<my_ioc>:HBtOK_<plc_sigid>
For example, if the IOC Cryo_ICS:IOC20 needs to test the PLC Cryo_ICS:PLC20, it
uses a substitution file to define Cryo_ICS:IOC20:HBtOK_PLC20 and
Cryo_ICS:IOC20:HBtLast_PLC20 using "test_his_IOC_heartbeat.db".  (See below
under ioc20.substitutions.)

Macros are used to define the parameters of the test.
------
$(P) - the device name of my IOC
$(P_his) - the device name of his IOC
$(SIG) - the signal id for my IOC's opinion of his IOC's health
$(MAX) - the maximum value of the heartbeat.  MAX must be a number of the form
	 9*N-1 for the EDM symbol GlobalSym_heartbeat to work smoothly - only
	 relevant if "my_IOC_heartbeat_mod.template" is used.  The standard
	 value used in "stats.template" is 98 - this must match.
$(TICKS) - how many ticks of my heartbeat before I raise a major alarm about
	  his heartbeat.
$(SCAN) - how often I update my heartbeat and check his (in seconds.)  The
	 standard value used in "stats.template" is 1. (1 second)

ioc20.substitutions (entire contents)
-------------------------
("stats" is defined by an earlier nfs mount.  The other macros for
"stats.template" are described in ioc.substitutions in vxStatsApp/Db.)

file /stats/db/stats.template
{
    {P=Cryo_ICS:IOC20,
	HMM=50000000,WMM=500000,AMM=100000,
	HCP=100,WCP=50,ACP=80,
	HCL=200,WCL=100,ACL=175,
	HCX=2000,WCX=1000,ACX=1750,
	HFD=150,WFD=20,AFD=5}
}
file /stats/db/test_his_IOC_heartbeat.template
{
    {P=Cryo_ICS:IOC20,
        P_his=Cryo_ICS:IOC30,SIG=30,
        TICKS=5,SCAN=1,MAX=98}
}

These lines could be added to monitor the PLC:

file /stats/db/test_his_IOC_heartbeat.template
{
    {P=Cryo_ICS:IOC20,
        P_his=Cryo_ICS:PLC20,SIG=PLC20,
        TICKS=5,SCAN=1,MAX=98}
}

The Macro substitutions for "my_IOC_heartbeat.template" are identical to
those for "my_IOC_heartbeat_mod.template".

Note: MAX must be a number of the form 9*N-1 for the EDM symbol
GlobalSym_heartbeat to work smoothly.

------------------------------------------------------------------------------
Heartbeat Displays
------------------
The edm symbol GlobalSym_heartbeat.edl is provided so that you can create your
own annoying heartbeat displays.  This works with the heartbeat defined in
stats.template; for other heartbeats, you need to use
"my_IOC_heartbeat_mod.template" (or something else that provides a similar mod 9
heartbeat value) to define a heartbeat value.

You can use the display files IOC_2_IOC_heartbeat.edl and
IOC_2_PLC_heartbeat.edl to test your heartbeats.

IOC_2_IOC_heartbeat.edl requires macro substitution values as follows:

P1 - the device name of IOC 1
P2 - the device name of IOC 2
SIG1 - the signal id for IOC 2's opinion of IOC 1's health
SIG2 - the signal id for IOC 1's opinion of IOC 2's health

You can use an edm command of this form:
edm -m "P1=<ioc1name>,P2=<ioc2name>,SIG1=<ioc1sigid>,SIG2=<ioc2sigid>"\
 -x IOC_2_IOC_heartbeat.edl &

For example,
edm -m "P1=Cryo_ICS:IOC01,P2=Cryo_ICS:IOC20,SIG1=01,SIG2=20" -x IOC_2_IOC_heartbeat.edl &

IOC_2_PLC_heartbeat.edl requires macro substitution values as follows:

IOC - the device name of your IOC
PLC - the device name of your PLC
SIGPLC - the signal id for your IOC's opinion of your PLC's health

You can use an edm command of this form:
edm -m "IOC=<iocname>,PLC=<plcname>,SIGPLC=<plcsigid>"\
 -x IOC_2_PLC_heartbeat.edl &

For example,
edm -m "IOC=Cryo_ICS:IOC20,PLC=Cryo_ICS:PLC20,SIGPLC=PLC20" -x IOC_2_PLC_heartbeat.edl &

------------------------------------------------------------------------------
The CapFast schematics that were used to generate "my_IOC_heartbeat.template",
"my_IOC_heartbeat_mod.template" and "test_his_IOC_heartbeat.template" are
included but BEWARE: if the templates are regenerated from CapFast, the INPA
link from "his" heartbeat in "test_his_heartbeat.template" has to be edited to
add " CP NMS".  These files have been renamed with "_DOC" in their names to
signify that they are only included for documentation purposes.
------------------------------------------------------------------------------
