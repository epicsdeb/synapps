.. $Id: simulator.rst 14555 2012-03-02 20:21:10Z jemian $
   .
   build the HTML documentation using this command:
   cmd>  rst2html -s -d simulator.{rst,html}

================================================
``fb_epid`` simulator
================================================

=========== ================================================================
**Purpose** describe the simulator in the ``fb_epid`` support
**Author**  Pete R. Jemian
**SVN**     $Id: simulator.rst 14555 2012-03-02 20:21:10Z jemian $
=========== ================================================================

.. sectnum::

.. contents::

.. sidebar:: About ...

   This simulator is part of the ``fb_epid`` support in 
   the synApps 
   `optics <http://www.aps.anl.gov/bcda/synApps/optics/opticsDocs.html>`_ 
   module.  See `here <index.html>`_ for documentation
   of the ``fb_epid`` support.

Example use of the ``fb_epid`` simulator
------------------------------------------

The support database has a simulator to help learn how 
to use the ``fb_epid`` support.  The simulator models the 
temperature of something which is subject to some cooling.
There is support for heating power to be applied, as 
directed by the output of the ``epid`` record.  The cooling 
could be applied either by adjustment of a continuous
variable or by a (simulated) relay-switched application of
heating power.  Smoother operation is obtained with the
continuous variable but not all temperature controllers
provide this.

The simulator is based on the ``swait`` record.  [#swait]_
The fields are assigned as follows:

=====  =================================
field  description
=====  =================================
A      minimum "temperature" allowed
B      cooling rate parameter
C      heater power
D      output of PID loop
E      heater relay closes when D > E
F      current "temperature"
=====  =================================

The ``fb_epid`` support should be configured like this:

=================   ===================
PV                  value
=================   ===================
$(P):in.INAN        ``$(P):sim``
$(P):out.OUTN       ``$(P):sim.D``
$(P):enable.INAN    ``$(P):on.VAL``
$(P).KP 	    ``0.01``
$(P).KI 	    ``0.1``
$(P).I		    ``0.0``
$(P).KD 	    ``0.0``
$(P).DRVL	    ``0.0``
$(P).DRVH	    ``1.0``
$(P).FMOD	    ``PID``
=================   ===================

This configuration is defined in the 
supplied ``fb_epid.substitutions`` file.

.. include:: ../../iocBoot/iocAny/fb_epid.substitutions
   :literal:

Interface Screens
++++++++++++++++++++

start a MEDM session with a command such as::

    medm -x -macro "P=prj:epid1,C=:sim" fb_epid_sim.adl &

This screen provides access to the simulator,
the ``swait`` calculation record, and 
the ``fb_epid`` controls that support it.

.. figure:: fb_epid_sim_adl.png
   :alt: main MEDM control screen for simulator

   Figure: ``fb_epid`` temperature simulator controls

The ``calc`` button brings up this screen:

.. figure:: userCalc_adl.png
   :alt: simulator calculation

   Figure: temperature simulator calculation

The ``controls`` button brings up the standard ``fb_epid`` controls:

.. figure:: fb_epid_adl.png
   :alt: simulator calculation

   Figure: `fb_epid`` main control screen


Operation
++++++++++++++++++++

to be written


.. rubric:: Footnotes

.. References
.. -----------------

.. [#swait] EPICS ``swait`` record:
   http://www.aps.anl.gov/bcda/synApps/calc/swaitRecord.html
