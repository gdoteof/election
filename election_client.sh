#!/bin/bash
function usage(){
  echo "Usage:  $0 [json|html|csv] {request} "
  exit
}

if [ "$1" = "json" ]
then
  ACCEPT="Accept: application/json"
elif [ "$1" = "html" ]
then
  ACCEPT="Accept: text/html"
elif [ "$1" = "csv" ]
then
  ACCEPT="Accept: text/csv"
elif [ "$1" = "xml" ]
then
  ACCEPT="Accept: application/xml"
else
  usage
fi


if [ "$1" = "json" ]
then
curl -H "$ACCEPT" -X GET "http://local.cctv.dev/$2" | json_formatter
else
curl -H "$ACCEPT" -X GET "http://local.cctv.dev/$2"
fi

echo -e "\n==========="
echo -e "used:  curl -H \"$ACCEPT\" -X GET \"http://local.cctv.dev/$2\""
