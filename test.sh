#!/bin/sh

echo '========================================'
mkdir -p test
echo 'Generating test data...'

echo 'test1.original = a (non-512 divisible) 640 bytes of printable ascii'
( (tr -dc A-Za-z0-9 </dev/urandom) | head -c 640 ) > test/test1.original
echo 'test2.original = empty file'
touch test/test2.original
echo 'test3.original = 1MB of random noise (uses 2 octets to store block num)'
dd if=/dev/urandom of=test/test3.original bs=1M count=1
echo '========================================'
echo 'Testing server...'
echo 'test1'

echo '========================================'
echo 'Testing client...'

echo '========================================'
echo 'Done'
