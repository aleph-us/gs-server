cmake_minimum_required(VERSION 3.16)

# Ghostscript is AGPL — system package only, not bundled with the SDK.
# Both build machine and prod server need the same version installed.
# Use `dpkg -l ghostscript` on prod to check, then pin in CI:
#   apt-get install "ghostscript=<ver>" "libgs-dev=<ver>"
find_library(GS_LIB NAMES gs REQUIRED)

set(SDI_APP_NAME GSServer)
set(SDI_SOURCES GSServerApp GSHTTPTask GSWorkerTask GSSenderTask)
set(SDI_APP_DIR MSM)
set(SDI_EXTRA_LIBS ${GS_LIB})
include(../../cmake/SdiSvc.cmake)
