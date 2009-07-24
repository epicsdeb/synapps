
# -*- makefile -*-

EPICS_BASE = /usr/epics/base

CROSS_COMPILER_TARGET_ARCHS += RTEMS-mvme3100

# Modules must be listed in dependency order.
#  A module must be listed after any modules it depends on

# Tier 1

MODS += vxStats
vxStats_VER = 1-7-2g

MODS += seq
seq_NAME = SNCSEQ
seq_VER = 2-0-12

MODS += allenBradley
allenBradley_NAME = ALLEN_BRADLEY
allenBradley_VER = 2-1

MODS += ipac
ipac_VER = 2-10

MODS += sscan
sscan_VER = 2-6-4

MODS += autosave
autosave_VER = 4-5

# Tier 2

MODS += asyn
asyn_VER = 4-10
asyn_DEPS = seq ipac

MODS += calc
calc_VER = 2-7
calc_DEPS = sscan

# Tier 3

MODS += busy
busy_VER = 1-2
busy_DEPS = asyn

MODS += motor
motor_VER = 6-4-3
motor_DEPS = asyn seq ipac

MODS += std
std_VER = 2-7
std_DEPS = asyn

MODS += dac128V
dac128V_VER = 2-4
dac128V_DEPS = asyn ipac

MODS += ip330
ip330_VER = 2-5
ip330_DEPS = asyn ipac

MODS += ipUnidig
ipUnidig_VER = 2-6
ipUnidig_DEPS = asyn ipac

MODS += love
love_VER = 3-2-4
love_DEPS = asyn ipac

MODS += ip
ip_VER = 2-9
ip_DEPS = asyn ipac seq

MODS += ccd
ccd_VER = 1-10
ccd_DEPS = busy asyn seq

MODS += optics
optics_VER = 2-6-1
optics_DEPS = asyn

MODS += stream
stream_VER = 2-4
stream_DEPS = asyn calc sscan

MODS += modbus
modbus_VER = 1-3
modbus_DEPS = asyn

MODS += vac
vac_VER = 1-2
vac_DEPS = asyn ipac

# Tier 4

MODS += delaygen
delaygen_VER = 1-0-3
delaygen_DEPS = asyn std

MODS += camac
camac_VER = 2-5
camac_DEPS = motor std

MODS += mca
mca_VER = 6-11
mca_DEPS = asyn std

MODS += vme
vme_VER = 2-6
vme_DEPS = std

MODS += pilatus
pilatus_VER = 1-6
pilatus_DEPS = asyn seq stream

# Tier 5

MODS += dxp
dxp_VER = 2-9
dxp_DEPS = asyn camac mca busy

#MODS += 
#_VER = 
#_DEPS = 

#######################################

SUPPORT = $(PWD)

ENVIRON = EPICS_BASE SUPPORT

ifneq ($(INSTALLTO),)
INSTALL_LOCATION=$(INSTALLTO)
ENVIRON += INSTALL_LOCATION
endif

ifneq ($(CROSS_COMPILER_TARGET_ARCHS),)
ENVIRON += CROSS_COMPILER_TARGET_ARCHS
endif

# $(1) is ENVIRON name (ie EPICS_BASE)
ENV = $(1)=$$($(1))

E = $(foreach e,$(ENVIRON),$(call ENV,$(e)))

# $(1) is path relative to module dir (ie bin)
# $(2) is the module name (ie asyn)
FORCERM = $(if $(1),rm -rf $(1:%=$(2)/$($(2)_VER)/%),)

all: realall # see below

# Rules for a a single module
#
# $(1) is mod name (ie seq or sscan)
define build-mod

$(1)_NAME ?= $(shell echo -n "$(1)" | tr '[:lower:]' '[:upper:]')

$(1)_ENV ?= $$(foreach ee,$$($(1)_DEPS),$$($$(ee)_NAME)=$$(SUPPORT)/$$(ee)/$$($$(ee)_VER))

build-$(1): $$($(1)_DEPS:%=build-%) single-$(1)

single-$(1):
	$$(MAKE) -C $(1)/$$($(1)_VER) $(E) $$($(1)_ENV)

build-all += build-$(1)

info-$(1):
	@echo "------------------"
	@echo "Info: $(1)"
	@echo "NAME=$$($(1)_NAME)"
	@echo "VER=$$($(1)_VER)"
	@echo "DEPS=$$($(1)_DEPS)"
	@echo "ENV=$$($(1)_ENV)"

rinfo-$(1): $$($(1)_DEPS:%=rinfo-%) info-$(1)

info-all += info-$(1)

realclean-$(1):
	$$(MAKE) -C $(1)/$$($(1)_VER) $(E) $$($(1)_ENV) realclean

$(1)_CLEAN ?= bin db dbd include lib html templates
fullclean-$(1):
	$$(call FORCERM,$$($(1)_CLEAN),$(1))

clean-$(1): realclean-$(1) fullclean-$(1)

clean-all += clean-$(1)

endef

$(eval $(foreach m,$(MODS),$(call build-mod,$(m))))

realall: $(build-all)

info: $(info-all)

clean: $(clean-all)

help:
	@echo "Modules:"
	@echo "$(MODS)" | tr ' ' '\n'
	@echo "Targets:"
	@echo "  all - Build all modules"
	@echo "  build-MOD - build a module and its dependencies"
	@echo "  single-MOD - build a module without updating dependencies"
