#!/bin/bash
ACCOUNT=rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh
HOOKDATA=`(cd ../../build; ./rippled account_objects rECE33X6yXqM7MpjXCqG8nsdSWtSFzeGrS 2> /dev/null | grep '"HookStateData"' | awk '{print $3}' | sed -E 's/[^A-F0-9]//g')`
FLOAT1=`echo $HOOKDATA | cut -c1-16`
FLOAT2=`echo $HOOKDATA | cut -c17-32`
echo $FLOAT1
echo $FLOAT2
echo "Hook Internal Accounting"
echo "  [Sent]                      [Received]"
echo "> USD: `node xls17.js 0x$FLOAT1`   XRP: `node xls17.js 0x$FLOAT2`"
DROPS=`(cd ../../build; ./rippled account_info $ACCOUNT 2> /dev/null | grep Balance | awk '{print $3}'| sed -E 's/[^A-F0-9]//g')`
XRP=`echo "scale=6; $DROPS/1000000" | bc`
USD=`(cd ../../build; ./rippled account_lines $ACCOUNT 2> /dev/null | grep balance | awk '{print $3}' | sed -E 's/[^0-9\.]//g')`
echo "$ACCOUNT"
echo "> USD: $USD   XRP: $XRP"
