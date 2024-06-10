#!/bin/sh
set -eu

workers=${CTP_WORKERS:-2}
anon_script=${CTP_SCRIPT:-anon.script}
command="/opt/java/bin/java -jar DAT.jar -da $anon_script -in /input -out /output -n $workers $*"

echo Running "$command"
echo Environment:
echo "---------------------------"
env
echo "---------------------------"

cd /app
exec $command
