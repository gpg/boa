#! /bin/sh

# this script tests the keepalive functionaly
# it is really only meant to be run by the developers, but it's a start
#  on the regression tests.
# It's likely this script would need extensive customization before
#  it's able to be used.
# If you don't like the code, either fix it or shush.
# NOTE: Needs userland programs: awk, grep, md5sum, stat, socket, wc

PORT=80;
TARGETHOST=localhost
URLBASE="/"
FILEBASE="/var/www/"
echo "Creating file list...";
#FILES="/Parallel-Processing-HOWTO-3.html /index.html /todo.txt /test_file.gz";
FILES="Parallel-Processing-HOWTO-3.html todo.txt";
FILE2=$FILES;
for i in `seq 1 150`;do
  FILES="$FILES $FILE2";
done
echo "Forming requests...";
for j in $FILES;do 
  I=$I"GET ${URLBASE}$j HTTP/1.0\r\nConnection: Keep-Alive\r\n\r\n";
done
echo -n "Checking requests length...";
LEN=`/bin/echo -e -n $I | wc -c`;
echo $LEN;
I=$I"HEAD /todo.txt HTTP/1.0\r\n\r\n"; #terminate nicely
echo "Making testdir...."
if ! mkdir -p testdir;then
  echo "Unable to make testdir!";
  exit;
fi
echo "Obtaining files....";
/bin/echo -e -n $I | socket $TARGETHOST $PORT > testdir/t
count=0;
zero=1;
echo "Testing files...."
count2=1000;
OLD=0;
for i in $FILES;do
  FSIZE=`stat ${FILEBASE}$i  | awk '/Size/{print $2}'`
  HSIZE=`/bin/echo -e -n "HEAD ${URLBASE}$i HTTP/1.0\r\nConnection: close\r\n\r\n" | socket $TARGETHOST $PORT | wc -c`;
  # the diff between a close and a keep-alive is 189 vs 228 
  # when count is 1000 and timeout is 10.
  # therefore, take 228 - 189 = 39
  # 39 - 4 = 35
  # take a "close" header size, add 35, and add the # of chars the max is (4)
  # oh, and add 1 for the first header ???why???
  c=`echo $count2 | wc -c`;
  let c="$c - 1";
  let b="$OLD+$HSIZE+35+$zero+$c"; # how far to skip
  let OLD="$b+$FSIZE";
  let count2="$count2 - 1";
#  echo "Skipping.... $b bytes...."
  tail --bytes=+$b testdir/t | head --bytes=$FSIZE > testdir/f$count;
  if md5sum testdir/f$count | grep -v `md5sum ${FILEBASE}$i | awk '{print $1}'`;
  then
    echo "File $i is different! (testdir/f$count)";
  else
    echo "File $i is OK";rm -f testdir/f$count;
  fi
  let count="$count+1";
  let zero="0";
done
