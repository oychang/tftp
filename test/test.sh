# server = '../tftp -l'
# client = '../tftp'

echo 'Running test case #1: 2K of random printable characters'
dira='test-a'
echo "Making directory $dira"
mkdir -p $dira
echo "Generating random test file as $dira/out.original"
((tr -dc A-Za-z0-9 < /dev/urandom) | head -c 2048) > $dira/out.original
echo 'Making copy'
cp $dira/out.original $dira/out.test
