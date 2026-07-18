#
# Makefile for GS Server
#
# Standard POCO gmake build (requires POCO_BASE). When PROJECT_BASE is set,
# release builds additionally back up and stage the binary, config and run
# script into $(PROJECT_BASE) (production release convention); without it,
# this is a plain build into bin/.
#

include $(POCO_BASE)/build/rules/global

target      = GSServer
objects     = GSServerApp GSHTTPTask GSWorkerTask GSSenderTask

target_libs = PocoUtil PocoXML PocoJSON PocoNet PocoFoundation

# Ghostscript is AGPL — system package only, never bundled.
SYSLIBS += -lgs

# SQL (ODBC) log-channel support — GSSERVER_SQL_LOGGING=0 drops the Poco
# Data/ODBC dependency (remove the sql channel from the logging config then).
GSSERVER_SQL_LOGGING ?= 1
ifeq ($(GSSERVER_SQL_LOGGING),1)
target_libs := PocoDataODBC PocoData $(target_libs)
CXXFLAGS    += -DGSSERVER_ENABLE_SQL_LOGGING
SYSLIBS     += -lodbc
endif

CURR_DIR    := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
BIN_TGT_DIR := $(CURR_DIR)bin/$(OSNAME)/$(OSARCH)

ifdef PROJECT_BASE
BIN_LNK_DIR := $(PROJECT_BASE)/bin
DATE_TIME   := $(shell date +"%Y%m%d%H%M%S")

# Back up the previously released binary and config on release builds.
RELEASE_BUILD := $(filter shared_release static_release,$(MAKECMDGOALS) $(DEFAULT_TARGET))
ifneq ($(RELEASE_BUILD),)
BACKUP_RESULT := $(shell \
	if [ -f $(BIN_TGT_DIR)/$(target) ]; then \
		cp $(BIN_TGT_DIR)/$(target) $(BIN_TGT_DIR)/$(target).$(DATE_TIME); \
	fi; \
	if [ -f $(PROJECT_BASE)/etc/$(target).properties ]; then \
		cp $(PROJECT_BASE)/etc/$(target).properties $(PROJECT_BASE)/etc/$(target).properties.$(DATE_TIME); \
	fi)
endif

ifeq ($(DEFAULT_TARGET),shared_release)
postbuild += mkdir -p $(BIN_LNK_DIR) $(PROJECT_BASE)/etc && \
	ln -sf $(BIN_TGT_DIR)/$(target) $(BIN_LNK_DIR)/$(target) && \
	cp -u $(target).properties $(PROJECT_BASE)/etc && \
	(diff $(target).properties $(PROJECT_BASE)/etc/$(target).properties.$(DATE_TIME) || true) && \
	cp -u run$(target).sh $(PROJECT_BASE)
else ifneq ($(filter shared_debug static_debug,$(MAKECMDGOALS) $(DEFAULT_TARGET)),)
postbuild += mkdir -p $(BIN_LNK_DIR) && \
	ln -sf $(BIN_TGT_DIR)/$(target)d $(BIN_LNK_DIR)/$(target)d
endif
endif

include $(POCO_BASE)/build/rules/exec
