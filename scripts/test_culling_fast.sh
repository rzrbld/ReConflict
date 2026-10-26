#!/usr/bin/env bash
# Current culling regression entry; historical reject-only tests stay archived.
set -euo pipefail
exec "$(dirname "$0")/test_culling_prefix.sh" "$@"
