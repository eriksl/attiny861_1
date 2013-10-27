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
				echo "${reply[*]}" | tr -d '\r'
		fi
	done
}

function send ()
{
	echo ":$*" >&6
	flush
}

send "s 04 p"
send "w 00 p" > /dev/null
send "r 00 p" > /dev/null

while true
do
	for input in 0 1
	do
		send "w d${input} p"
		send "r 00 p" > /dev/null
		usleep 200000
		send "w 02 p"
		rawline=($(send "r 00 p"))
		returned=$(printf "%d\n" "0x${rawline[3]}${rawline[4]}")
		samples=$(printf "%d\n" "0x${rawline[5]}${rawline[6]}")
		total=$(printf "%d\n" "0x${rawline[7]}${rawline[8]}${rawline[9]}${rawline[10]}")

		if [ $samples != 0 ]
		then
			raw=$[total / samples]

			echo "$input $rawline - returned:$returned - samples:$samples - total:$total - raw:$raw"
		else
			echo "\"$rawline\""
		fi
	done
done
