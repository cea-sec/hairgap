#!/bin/bash

if [ -z "$ERR" ]; then
    ERR=/dev/null
fi
exec 2> $ERR

DIR=$(mktemp -d)
FROM=$DIR/from
TO=$DIR/to

if [ -z $HGAP_PATH ]; then
    HGAP_PATH=.
fi

HAIRGAPR=$HGAP_PATH/hairgapr
HAIRGAPS=$HGAP_PATH/hairgaps

cleanup() {
    pkill -TERM -P $$
    rm -r "$DIR" 2> $ERR
}

trap cleanup EXIT

fail() {
    if [ -n "$1" ]; then
        printf " \e[31m$1\e[0m"
    fi
    exit 1
}

init_test() {
    count=$1
    if [ -z "$2" ]; then
        block=1M
    else
        block=$2
    fi
    echo -n "Testcase: $count * $block, "
    echo -n "" > $TO
    dd if=/dev/zero of=$FROM bs=$block count=$count
}

check_md5() {
    if [ -z "$1" ]; then
        FROM_MD5=$(md5sum $FROM |cut -d' ' -f1)
    else
        FROM_MD5=$(echo -n $1 | cut -d' ' -f1)
    fi
    TO_MD5=$(md5sum $TO |cut -d' ' -f1)
    if [ "$FROM_MD5" != "$TO_MD5" ]; then
        fail "from != to"
        return 1
    fi
    return 0
}

check_ret_ok() {
    if [ $1 -ne 0 ]; then
        fail "Bad ret (should be 0)"
        return 1
    fi
    return 0
}

check_ret_nok() {
    if [ $1 -eq 0 ]; then
        fail "Bad ret (should be != 0)."
        return 1
    fi
    return 0
}

do_test() {
    echo -n "options: $*"
    $HAIRGAPR 127.0.0.1 > $TO      & rpid=$! && usleep 1000000
    $HAIRGAPS $* 127.0.0.1 < $FROM & spid=$!
    wait
    RET=$?
    check_md5 &&
    check_ret_ok $RET
}

