#!/bin/zsh

timeout=0.1
flushcount=2

input=1

#multiplier=0.0909
#offset=-4.8

multiplier=0.1030
offset=-49.8

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

function send_noflush ()
{
	#echo >&2 "> $*"
	echo ":$*" >&6
}

function send ()
{
	send_noflush "$*"
	flush
}

function hex ()
{
	echo "$1" | perl -ne '
		chomp();
		$h=$_;
		printf("%02x %02x\n", ($h >> 8) & 0xff, ($h >> 0) & 0xff);
	'
}

ms=$(echo "$multiplier * 1000" | bc -l | sed -e 's/\..*$//')
os=$(echo "$offset     *   10" | bc -l | sed -e 's/\..*$//')

echo "ms: $ms"
echo "os: $os"

m=$(hex $ms)
o=$(hex $os)

printf "multiplier = %s / %s\n" $m $ms
printf "offset     = %s / %s\n" $o $os

send "s 04 p"
send "w 00 p"
send "r 00 p"

send_noflush "w e${input} $m $o p"
usleep 100000
flush
send "r 00 p"

send "w f${input} p"
reply=($(send "r 00 p"))
mr=$[0x${reply[3]}${reply[4]}]
or=$[0x${reply[5]}${reply[6]}]

if [ $mr -gt 32768 ]
then
	mr=$[0 - (65536 - mr)]
fi
mr=$(echo "scale=4; $mr / 1000" | bc -l)

if [ $or -gt 32768 ]
then
	or=$[0 - (65536 - or)]
fi
or=$(echo "scale=4; $or / 10" | bc -l)

echo "return: ${reply[*]}"
echo "return multiplier: $mr"
echo "return offset: $or"

while true
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
		value=$total
		value=$[total * 10]
		value=$[value * ms]
		value=$[value / samples]
		value=$[value / 1000]
		value=$[value + os]

		echo "$rawline - returned:$returned - samples:$samples - total:$total - raw:$raw - cooked:$value"
	else
		echo "\"$rawline\""
	fi
done
