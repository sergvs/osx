#!/bin/sh
# The next line is executed by /bin/sh, but not tcl \
exec tclsh "$0" ${1+"$@"}

package require tls

proc creadable {s} {
    puts "LINE=[gets $s]"
    after 2000
    exit
}

proc myserv {s args} {
    fileevent $s readable [list creadable $s]
}

tls::misc req 1024 $keyfile $certfile [list C CCC ST STTT L LLLL O OOOO OU OUUUU CN CNNNN Email some@email.com days 730 serial 12]

tls::socket -keyfile $keyfile -certfile $certfile -server myserv 12300

puts "Now run keytest2.tcl"
vwait forever

