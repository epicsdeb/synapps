# The is the ASYN example for communication to 4 simulated motors
# "#!" marks lines that can be uncommented.

dbLoadTemplate("motorSim.substitutions")

# Create simulated motors: ( start card , start axis , low limit, high limit, home posn, # cards, # axes to setup)
motorSimCreate( 0, 0, -32000, 32000, 0, 1, 8 )
# Setup the Asyn layer (portname, low-level driver drvet name, card, number of axes on card)
drvAsynMotorConfigure("motorSim1", "motorSim", 0, 8)
