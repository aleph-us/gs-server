#
# Makefile for SDI GS service
#

SDI_APP_NAME=GS
objects = $(SDI_APP_NAME)App GSHTTPTask GSWorkerTask GSNotification
include $(PROJECT_BASE)/alephone/apps/custom/sdi-svcs/SDI_SVC.make
