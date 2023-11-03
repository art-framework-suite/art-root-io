#!/bin/bash

. cet_test_functions.sh

rm -f copy.root

export LD_LIBRARY_PATH=..${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}
export DYLD_LIBRARY_PATH=..${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}
export CET_PLUGIN_PATH=..${CET_PLUGIN_PATH:+:${CET_PLUGIN_PATH}}

art_exec=$1
$art_exec -c "${2}" -s "${3}" --rethrow-all

check_files "copy.root"
