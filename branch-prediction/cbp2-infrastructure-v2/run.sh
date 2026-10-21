#!/bin/bash
if [ -z "$1" ]; then
	echo "Usage: $0 <trace-file-directory>"
	exit 1
fi
(cd src && make -q) || echo "predict program is not up to date."
if [ ! -e src/predict ] && [ ! -e src/predict.exe ]; then
	echo "predict program is not built."
	exit 1
fi
BIN=src/predict
[ -e src/predict.exe ] && BIN=src/predict.exe
FIND=/usr/bin/find
[ -x "$FIND" ] || FIND=find
sum=0
n=0
for i in $("$FIND" "$1" -name '*.trace.*' | sort); do
	printf "%-40s\t" "$i"
	mpki=$($BIN "$i" | tail -1 | sed -e 's/ MPKI//')
	printf "%0.3f\n" "$mpki"
	sum=$(awk -v a="$sum" -v b="$mpki" 'BEGIN{print a+b}')
	n=$((n + 1))
done
if [ "$n" -eq 0 ]; then
	echo "no *.trace.* files found under $1"
	exit 1
fi
echo -n "average MPKI: "
awk -v s="$sum" -v n="$n" 'BEGIN{printf "%.3f\n", s/n}'
