#!/bin/sh
# Install native-tg/build/org.bq.gram.bar on the phone through BB10's installer (root job). From anywhere.
cd "$(dirname "$0")" || exit 1
scp -q build/org.bq.gram.bar "${BB10_HOST:-passport}":/var/nt/work/org.bq.gram.bar || exit 1
ssh "${BB10_HOST:-passport}" 'B=/var/nt/bin; chmod 644 /var/nt/work/org.bq.gram.bar; j=/pps/system/installer/upd/current/job.bqd.gram; rm -f $j; : > $j; chmod 664 $j; print -r -- "action::install
package_location::/var/nt/work/org.bq.gram.bar
extras::" > $j; i=0; while [ $i -lt 90 ]; do $B/grep -q "^result::" $j 2>/dev/null && break; $B/sleep 1; i=$((i+1)); done; $B/grep -E "^result::|^actual_app_version::" $j; rm -f $j /var/nt/work/org.bq.gram.bar'
