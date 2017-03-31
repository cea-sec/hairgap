#!/bin/bash

if [ -z "$ERR" ]; then
    ERR=/dev/null
fi
exec 2> $ERR

print_init() {
    printf "[ ... ] "
}

print_ok() {
    printf "\r[ \e[32mOK\e[0m  ]"
}

print_err() {
    printf "\r[ \e[31mERR\e[0m ]"
}

do_test() {
    total=$(($total+1))
    script=$1
    print_init
    echo -n "$(basename $script) "
    if ! HGAP_PATH=$HGAP_PATH TEST_BASE=$TEST_BASE /bin/bash $script; then
        failed=$(($failed+1))
        print_err
    else
        print_ok
    fi
    echo
}

total=0
failed=0

BASEDIR=$(dirname $(realpath $0))
if [ -z $HGAP_PATH ]; then
    HGAP_PATH=$(realpath $BASEDIR/..)
fi

if [ -z $TEST_BASE ]; then
    TEST_BASE=$(realpath $BASEDIR/test_base.sh)
fi

if [ $# -lt 1 ]; then
    cases=$BASEDIR/cases/*.sh
else
    cases="$*"
fi

for script in $cases; do
    do_test $script
done


passed=$(($total-$failed))
echo "Summary: $passed/$total"

if [ $failed -ne 0 ]; then
    exit 1
fi

