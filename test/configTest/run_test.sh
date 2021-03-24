#!/bin/bash

_build_dataHub/localhost/app/dataHub/staging/read-only/bin/hubd &

_build_configTest/localhost/app/configTest/staging/read-only/bin/configTestD $@&
CONFIGTESTPID=$!
echo "Started ConfigTest app with PID $CONFIGTESTPID."

log level DEBUG hubd/dataHub

_build_appInfoStub/localhost/app/appInfoStub/staging/read-only/bin/appInfoD --configTest=$CONFIGTESTPID &

sleep 0.25
