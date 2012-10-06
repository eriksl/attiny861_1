#!/bin/zsh

timeout=0.1
flushcount=2

exec 6<> /dev/ttyUSB1

function flush ()
{
	for ((flush = 0; flush < flushcount; flush++))
	do
		reply=""

		read -A -t $timeout reply <&6

		if [ -n "${reply[*]}" ]
		then
				echo "${reply[*]}"
		fi
	done
}

function send ()
{
	echo ":$*" >&6
	flush
}

send "s 04 p"

while true
do
	clear
#	send "w c0 p r 00 p w 01 p r 00 p"
#	send "w c1 p r 00 p w 01 p r 00 p"
#	send "w c2 p r 00 p w 01 p r 00 p"
#	send "w 10 p r 00 p"
#	send "w 11 p r 00 p"
#	send "w 12 p r 00 p"
#	send "w 13 p r 00 p"
	send "w 30 p r 00 p"
	send "w 31 p r 00 p"
	send "w 32 p r 00 p"
	send "w 33 p r 00 p"
	usleep 100000
done
