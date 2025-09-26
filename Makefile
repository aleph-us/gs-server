#
# Makefile for SDI GS service
#

SDI_APP_NAME=gs-server
objects = $(SDI_APP_NAME)App GSHTTPTask GSWorkerTask GSSenderTask 
# GSNotification
include $(PROJECT_BASE)/alephone/apps/custom/sdi-svcs/SDI_SVC.make
