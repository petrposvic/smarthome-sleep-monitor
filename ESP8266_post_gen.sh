#!/bin/bash

if [ $# -ne 3 ]; then
  echo "Usage: $0 addr port path"
  echo "Example:"
  echo "    $0 192.168.1.203 3000 /api/measurements"
  echo "or"
  echo "    echo \"{\\\"temperature\\\":\\\"123\\\",\\\"humidity\\\":\\\"100\\\"}\" | $0 192.168.1.203 3000 /api/measurements"
  exit 1
fi

read PIPE
BODY_LEN=`echo -n "$PIPE" | wc -m`
MSG="POST $3 HTTP/1.1\r\nHost:$1\r\nCache-Control:no-cache\r\nContent-Type:application/json\r\nContent-Length:$BODY_LEN\r\n\r\n$PIPE\r\n\r\n"
MSG_LEN=`echo -en $MSG | wc -m`

echo " First time:"
echo "-------------"
echo "AT"
echo "AT+CWJAP=\"ssid\",\"pass\""
echo "AT+CWMODE=1"
echo
echo " Request:"
echo "----------"
echo "AT+CIPSTART=\"TCP\",\"$1\",$2"
echo "AT+CIPSEND=$MSG_LEN"
echo -e $MSG
