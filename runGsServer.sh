#!/bin/bash

basedir="$(cd "$(dirname "${BASH_SOURCE[0]}")" || exit ; pwd -P)"
# shellcheck disable=1091
. "$basedir"/runEnv.bash

"$basedir"/runService.sh GS
exit $?
