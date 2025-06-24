#!/bin/bash -x

if [ $# -lt 1 ]; then
  echo "Pass the tool name as an argument:"
  ls -1 thread*
  exit 1
fi

tool=$1
shift

for num in ${THREADS:-`seq 5 5 100`}; do
perf stat ${PERF_OPTS} -v -- ./$tool -b $num -m $num -d 10 -o 100000 -t 1 -l 100000 "$@";
sleep 5;
done
