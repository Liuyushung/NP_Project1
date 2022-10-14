#!/bin/bash
ps=$(ps -ux | grep npshell | awk '{print $2}')
echo $ps
ps -ux | grep npshell | awk '{print $2}' | xargs kill -9
