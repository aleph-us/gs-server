#
# Makefile for SDI GS service
#

SDI_APP_NAME=GSServer
objects = $(SDI_APP_NAME)App GSHTTPTask GSWorkerTask GSSenderTask 
# GSNotification
include $(PROJECT_BASE)/alephone/apps/custom/sdi-svcs/SDI_SVC.make

# Ghostscript is AGPL, so only this service links it explicitly.
SYSLIBS += -lgs
