#!/bin/bash
source "$TEST_BASE"
keepalive_test2() {
    init_test 10
    echo -n "keepalive test 2, options: $*"
    $HAIRGAPR 127.0.0.1 > $TO & rpid=$! && usleep 10000!
    (cat $FROM && sleep 2 && cat $FROM) | $HAIRGAPS -k 3000 $* 127.0.0.1
    wait "$rpid"
    RET=$?
    # Check no timeout
    wait
    check_ret_nok $RET
}
keepalive_test2 $*
