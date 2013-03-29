This project is a component of the Experimental Physics and Industrial
Controls System, short `EPICS`_. EPICS is a system of interactive
applications, development tools, and a common run-time environment that
allows users to build and execute real-time control and data
acquisition systems for experimental facilities, such as particle
accelerators and telescopes.

The state notation language *SNL* allows programming sequential
operations that interact with EPICS process variables without the usual
complexity involved with task scheduling, semaphores, event handling,
and I/O programming. The language is a restricted subset of C, enhanced
with extra syntax for descibing state sets, states, state transitions,
and the binding of program variables to named and anonymous process
variables. See the :doc:`Tutorial` for a gentle introduction.

From the SNL source code, the SNL compiler *snc* generates a number of
C code procedures that are called by the run-time sequencer, a C
library contained in this project. This library handles all low-level
management of connecting, monitoring, and changing process variables,
as well as task (thread) creation and communication. The sequencer
interfaces to the underlying control system through a generic PV
(process variable) API that supports, among other message systems, the
`Channel Access`_ facility of `EPICS`_.

.. _EPICS: http://www.aps.anl.gov/epics/
.. _Channel Access: http://www.aps.anl.gov/epics/base/R3-14/11-docs/CAref.html
