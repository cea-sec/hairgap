#!/bin/bash
source "$TEST_BASE"
keepalive_test1() {
    init_test 10
    echo -n "keepalive test 1, options: $*"
    $HAIRGAPR 127.0.0.1 > $TO & rpid=$! && usleep 10000!
    (cat $FROM && sleep 2 && cat $FROM) | $HAIRGAPS $* 127.0.0.1
    wait "$rpid"
    RET=$?
    # Check no timeout
    check_ret_ok $RET || return
    wait
    check_md5 $(cat $FROM $FROM | md5sum)
}
keepalive_test1 $*
