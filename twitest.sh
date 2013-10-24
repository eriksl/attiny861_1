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
send "w c0 p r 00 p"

while true
do
#	clear
#	send "w c0 p r 00 p w 01 p r 00 p"
#	send "w c1 p r 00 p w 01 p r 00 p"
#	send "w c2 p r 00 p w 01 p r 00 p"
#	send "w 10 p r 00 p"
#	send "w 11 p r 00 p"
#	send "w 12 p r 00 p"
#	send "w 13 p r 00 p"
#	send "w 30 p r 00 p"
#	send "w 31 p r 00 p"
#	send "w 32 p r 00 p"
#	send "w 33 p r 00 p"
#	send "s 04 c0 p s 05 00 p"

#	a=($(send "w 02 p"))
#	a=($(send "r 00 p"))
#	printf "%d\n" "0x${a[3]}${a[4]}"

#	a=($(send "w 01 p r 00 p"))
#	b=$(printf "%d\n" "0x${a[3]}${a[4]}")
#	c=$(echo "(($b / 64) - 273) / 0.110" | bc -l)
#	d=$[((($b - (273 * 64)) * 1000) / 110) / 64]
#	e=$(printf "%d\n" "0x${a[5]}${a[6]}")
#	echo $a - $b - $c - $d - $e

a=($(send "w 01 p r 00 p"))
b=$(printf "%d\n" "0x${a[3]}${a[4]}")
c=$(printf "%d\n" "0x${a[5]}${a[6]}${a[7]}${a[8]}")

if [ $c != 0 ]
then
	d=$[c / b]
if [ $d != 1023 ]
	then
		echo $a - $b - $c - $d
	fi
else
	echo $a - $b - $c
fi

#	usleep 100000
done
