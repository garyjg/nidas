#!/bin/sh

# Test script for nidas::util::UTime

dir=`dirname $0`

PATH=$dir:$PATH

# If the first runstring argument is "installed", then don't fiddle with PATH or
# LD_LIBRARY_PATH, and run the nidas programs from wherever they are found in PATH.
# Otherwise if build_x86/build_apps is not found in PATH, prepend it, and if LD_LIBRARY_PATH 
# doesn't contain the string build_x86, prepend ../src/build_x86/build_{util,core,dynld}.

installed=false
[ $# -gt 0 -a "$1" == "-i" ] && installed=true

if ! $installed; then

    # echo $PATH | fgrep -q build_x86/build_apps || PATH=../../src/build_x86/build_apps:$PATH

    llp=../../build_x86/build_util:../../build_x86/build_core:../../build_x86/build_dynld
    echo $LD_LIBRARY_PATH | fgrep -q build_x86 || \
        export LD_LIBRARY_PATH=$llp${LD_LIBRARY_PATH:+":$LD_LIBRARY_PATH"}

    echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
    echo PATH=$PATH

    if ! ldd `which ck_utime` | awk '/libnidas/{if (index($0,"build_x86") == 0) exit 1}'; then
        echo "using nidas libraries from somewhere other than a build_x86 directory"
        exit 1
    fi
fi

# echo PATH=$PATH
# echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH

echo "ck_utime executable: `which ck_utime`"
echo "nidas libaries:"
ldd `which ck_utime` | fgrep libnidas

valgrind_errors() {
    egrep -q "^==[0-9]*== ERROR SUMMARY:" $1 && \
        sed -n 's/^==[0-9]*== ERROR SUMMARY: \([0-9]*\).*/\1/p' $1 || echo 1
}

[ -d tmp ] || mkdir tmp

valgrind --suppressions=suppressions.txt --gen-suppressions=all ck_utime 2>&1 | tee tmp/utime.log
stat=$?

errs=`valgrind_errors tmp/utime.log`
echo "$errs errors reported by valgrind in tmp/utime.log"

if [ $stat -eq 0 -a $errs -eq 0 ]; then
    echo "UTime test OK"
    exit 0
else
    echo "UTime test failed"
    exit 1
fi
