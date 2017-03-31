#!/bin/bash
source "$TEST_BASE"
edge_test1() {
    init_test 100
    echo -n "edge case 1, options: $*"
    $HAIRGAPS $* 127.0.0.1 < $FROM & spid=$! && usleep 10000
    $HAIRGAPR -t 1 127.0.0.1 > $TO & rpid=$!
    wait "$spid"
    RET=$?
    if [ "$(cat $TO)" ]; then
        fail "dest file should be empty"
        return 1
    fi
    check_ret_nok || return 1
    $HAIRGAPS $* 127.0.0.1 < $FROM
    kill "$rpid"
    check_ret_ok $RET
}
edge_test1
