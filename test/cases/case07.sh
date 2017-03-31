#!/bin/bash
source "$TEST_BASE"
edge_test2() {
    init_test 100
    echo -n "edge case 2, options: $*"
    $HAIRGAPR -t 1 127.0.0.1 > $TO & rpid=$! && usleep 10000;
    $HAIRGAPS $* 127.0.0.1 < $FROM & spid=$!
    kill "$spid"
    wait "$rpid"
    RET=$?
    check_ret_nok $RET
}
edge_test2 $*
