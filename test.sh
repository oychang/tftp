#!/bin/bash

# General idea is to bounce files around.
# Two machines:
#     * the local machine will act as client
#     * the remote machine will act as server
# Client writes file to remote and then reads the same file.
# Checks the diff and hopes for the best.
echo 'what is the remote ip?'
read remoteaddr
echo 'how about the port?'
read remoteport
mkdir -p test
echo '========================================'
echo 'test1.original = a (non-512 divisible) 640 bytes of printable ascii'
( (tr -dc A-Za-z0-9 </dev/urandom) | head -c 640 ) > test/test1.original
echo 'sending test1...'
cp test/test1.original test/test1.copy
./tftp -vwp $remoteport test/test1.copy $remoteaddr > test/test1-write.log
echo 'requesting test1...'
./tftp -vrp $remoteport test/test1.copy $remoteaddr > test/test1-read.log
diffout=$(diff test/test1.original test/test1.copy)
if [[ -z "$diffout" ]]; then
    echo 'original and copy are identical'
else
    echo 'original and copy are different'
    echo $diffout
fi
echo '========================================'
echo 'test2.original = empty file'
touch test/test2.original
cp test/test2.original test/test2.copy
./tftp -vwp $remoteport test/test2.copy $remoteaddr > test/test2-write.log
echo 'requesting test2...'
./tftp -vrp $remoteport test/test2.copy $remoteaddr > test/test2-read.log

diffout=$(diff test/test2.original test/test2.copy)
if [[ -z "$diffout" ]]; then
    echo 'original and copy are identical'
else
    echo 'original and copy are different'
    echo $diffout
fi
echo '========================================'
# WARN: for > 217 blocks, some bit mask messes up
#echo 'test3.original = 1MB of random noise (uses 2 octets to store block num)'
#dd if=/dev/urandom of=test/test3.original bs=1M count=1
#cp test/test3.original test/test3.copy
#./tftp -vwp $remoteport test/test3.copy $remoteaddr > test/test3-write.log
#echo 'requesting test2...'
#./tftp -vrp $remoteport test/test3.copy $remoteaddr > test/test3-read.log
#diffout=$(diff test/test3.original test/test3.copy)
#if [[ -z "$diffout" ]]; then
#    echo 'original and copy are identical'
#else
#    echo 'original and copy are different'
#    echo $diffout
#fi
#echo '========================================'

echo 'Done'
