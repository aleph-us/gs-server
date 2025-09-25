#!/bin/bash

# ==================================================================================================================================
# Script: release.sh (Invoker)
# Description: Lightweight wrapper for invoking the main release script with predefined arguments.
#
# PURPOSE: 
# This script simplifies execution by wrapping the main release script (`sdi-svcs/release.sh`)
# and passing default parameters to build, run, or dry-run a specific service.
#
# USAGE:
# ./release.sh [MODE]
# [MODE] is optional. If omitted, it defaults to:
#   -h               : Show this help message
#
# Available options:
#   -b, --build      : Build-only mode (shared lib + service, no install)
#   -r, --run        : Full deployment to production
#   -d, --dryrun     : Dry run (echo commands only, no execution)
#   -t, --test       : Test deployment to development environment
#   -h, --help       : Show this help message (default)
#
# NOTES:
# - Always targets the `MSM/Delay` service.
# - Useful for quick testing, or local development.
# - For more advanced options or to deploy multiple services, use the full `sdi-svcs/release.sh` script directly.
# 
# EXAMPLE:
# ./release.sh -r    : Will release MSM/Delay in production mode
# ==================================================================================================================================

ARG=${1:--h}  # if argument is ommited, default is '-h'

CALL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REL_PATH="${CALL_DIR##*/}"
PARENT="$(basename "$(dirname "$CALL_DIR")")"
SERVICE="$PARENT/$REL_PATH"

. "$PROJECT_BASE/alephone/apps/custom/sdi-svcs/release.sh" $ARG $SERVICE
