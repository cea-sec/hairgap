#!/bin/bash
source "$TEST_BASE"
init_test 50;   do_test -N 64000 $*
