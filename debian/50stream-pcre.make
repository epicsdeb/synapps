# As of epics-synnapps-dev 20111025-7, libstream depends on libpcre
# To avoid changing a lot of user makefiles when linking static executables
# we try to inject this dependency automatically

# We add the dependency for targets which include 'stream'
# in their *_LIBS list.

ifneq ($(SKIP_STREAM_AUTO_LIBS),YES)

# Places to check.
#  This is a list of prefixes which will have '_LIBS' appended when expanded below
_STREAM_CHECK_VARS := PROD TESTPROD $(PROD) $(TESTPROD) $(STREAM_EXTRA_CHECKS)

# $(1) will expand to one of the names in _STREAM_CHECK_VARS
define _STREAM_ADD_PCRE
$(1)_SYS_LIBS += $$(if $$(findstring stream,$$($(1)_LIBS)),pcre)
endef

$(foreach loc,$(_STREAM_CHECK_VARS),$(eval $(call _STREAM_ADD_PCRE,$(loc))))

# to test (when 'PROD=myApp') add a rule like this to see what extra definition
# would be emitted.
#testrule:
#	echo '$(call _STREAM_ADD_PCRE,myApp)'

endif
