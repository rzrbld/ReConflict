#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source packaging/cpu-session.sh
fixture=$(mktemp -d /tmp/ds-cpu-session-XXXXXX)
trap 'rm -rf "$fixture"' EXIT
ds_cpu_root=$fixture
for n in 0 4; do
    mkdir "$fixture/policy$n"
    printf 'ondemand conservative performance powersave\n' > "$fixture/policy$n/scaling_available_governors"
    printf '480000\n' > "$fixture/policy$n/scaling_min_freq"
    printf '1512000\n' > "$fixture/policy$n/scaling_max_freq"
done
printf 'ondemand\n' > "$fixture/policy0/scaling_governor"
printf 'conservative\n' > "$fixture/policy4/scaling_governor"
before=$(sha256sum "$fixture"/policy*/scaling_*_freq)
ds_cpu_begin
[[ ${#ds_cpu_policies[@]} == 2 ]]
[[ $(cat "$fixture/policy0/scaling_governor") == performance ]]
[[ $(cat "$fixture/policy4/scaling_governor") == performance ]]
ds_cpu_begin # Idempotent: do not replace the original saved governor.
[[ ${#ds_cpu_policies[@]} == 2 ]]
ds_cpu_restore
[[ $(cat "$fixture/policy0/scaling_governor") == ondemand ]]
[[ $(cat "$fixture/policy4/scaling_governor") == conservative ]]
[[ "$before" == "$(sha256sum "$fixture"/policy*/scaling_*_freq)" ]]
ds_cpu_restore
DS_CPU_PERFORMANCE=0 ds_cpu_begin
[[ ${#ds_cpu_policies[@]} == 0 ]]
ds_cpu_begin
printf 'powersave\n' > "$fixture/policy0/scaling_governor"
ds_cpu_restore
[[ $(cat "$fixture/policy0/scaling_governor") == powersave ]]
[[ $(cat "$fixture/policy4/scaling_governor") == conservative ]]
ds_cpu_root=$fixture/missing
ds_cpu_begin
ds_cpu_restore
echo 'PASS governor selection, idempotence, restoration, opt-out, external changes, absent sysfs and unchanged frequency limits'
