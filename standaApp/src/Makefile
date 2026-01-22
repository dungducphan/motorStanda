# Makefile
TOP = ../..
include $(TOP)/configure/CONFIG

# The following are used for debugging messages.
#!USR_CXXFLAGS += -DDEBUG

DBD += StandaSupport.dbd

LIBRARY_IOC = Standa

# Motion Controller 8SMC5 from Standa
SRCS += StandaDriver.cpp

Standa_LIBS += motor asyn
Standa_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES

