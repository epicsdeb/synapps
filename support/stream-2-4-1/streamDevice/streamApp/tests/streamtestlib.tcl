#
# Usage
# 1) source this file
# 2) define variables records, protocol, starup
# 3) call startioc
# 4) use ioccmd, assure, receive, send,...
# 5) call finish

set testname [file tail $argv0]

proc bgerror msg {
    error $::errorInfo
}

set debug 0
proc debugmsg {string} {
    global debug
    if $debug {puts $string}
}

proc deviceconnect {s addr port} {
    debugmsg "incoming connenction"
    global sock
    set sock $s
    fconfigure $sock -blocking no -buffering none -translation binary 
    fileevent $sock readable "receiveHandler $sock"
}

set inputbuffer {}
proc receiveHandler {sock} {
    global inputbuffer inputlog
    debugmsg "received data"
    set input [read $sock]
    puts -nonewline $inputlog $input
    append inputbuffer $input
    debugmsg "inputbuffer=[escape $inputbuffer]"
    if [eof $sock] {
        close $sock
        debugmsg "connection closed by ioc"
        return
    }
}

proc startioc {} {
    global debug records protocol startup port sock ioc testname env
    set fd [open test.db w]
    puts $fd $records
    close $fd
    set fd [open test.proto w]
    puts $fd $protocol
    close $fd
    set fd [open test.cmd w]
    puts $fd "dbLoadDatabase ../O.Common/streamApp.dbd"
    puts $fd "streamApp_registerRecordDeviceDriver"
    puts $fd "epicsEnvSet STREAM_PROTOCOL_PATH ."
    puts $fd "drvAsynIPPortConfigure device localhost:$port"
    puts $fd "dbLoadRecords test.db"
    puts $fd $startup
    puts $fd "iocInit"
    puts $fd "dbl"
    puts $fd "dbior stream 2"
    puts $fd "var streamDebug 1"
    close $fd
    set ioc [open "|../O.$env(EPICS_HOST_ARCH)/streamApp test.cmd >& $testname.ioclog" w]
    fconfigure $ioc -blocking yes -buffering none
    debugmsg "waiting to connect"
    vwait sock
}

proc ioccmd {command} {
    global ioc
    puts $ioc $command
}

proc send {string} {
    global sock
    puts -nonewline $sock $string
}

set timeout 10000
proc receive {} {
    global inputbuffer timeoutid timeout
    set timeoutid [after $timeout {
        set inputbuffer {}
    }]
    if {$inputbuffer == {}} { vwait inputbuffer }
    after cancel $timeoutid
    if {$inputbuffer == {}} {
        return -code error "Error in receive: timeout"
    }
    set index [string first "\n" $inputbuffer]
    if {$index > -1} {
        set input [string range $inputbuffer 0 $index]
        set inputbuffer [string range $inputbuffer [expr $index+1] end]
    } else {
        set input $inputbuffer
        set inputbuffer {}
    }
    return $input
}

set faults 0
proc assure {args} {
    global faults
    set input {}
    for {set i 0} {$i < [llength $args]} {incr i} {
        if [catch {lappend input [receive]} msg] {
            puts stderr $msg
            break
        }
    }
    set notfound {}
    foreach expected $args {
        set index [lsearch -exact $input $expected]
        if {$index > -1} {
            set input [lreplace $input $index $index]
        } else {
            lappend notfound $expected
        }
    }
    foreach string $notfound {
        puts stderr "Error in assure: missing    \"[escape $string]\""
    }
    foreach string $input {
        puts stderr "Error in assure: unexpected \"[escape $string]\""
    }
    if {[llength $notfound] || [llength $input]} {incr faults}
}

proc escape {string} {
    while {![string is print -failindex index $string]} {
        set char [string index $string $index]
        scan $char "%c" code
        switch $char {
            "\r" { set escaped "\\r" }
            "\n" { set escaped "\\n" }
            "\a" { set escaped "\\a" }
            "\t" { set escaped "\\t" }
            default { set escaped [format "<%02x>" $code] }
        }
        set string [string replace $string $index $index $escaped]
    }
    return $string
}

proc finish {} {
    global ioc timeout testname faults
    close $ioc
    set timeout 1000
    while {![catch {set string [receive]}]} {
        puts stderr "Error in finish: unexpected \"[escape $string]\""
        incr faults
    }
    if $faults {
        puts "Test failed."
        exit 1
    }
    puts "Test passed."
    eval file delete [glob -nocomplain test.*] StreamDebug.log $testname.ioclog
}

set port 40123
socket -server deviceconnect $port
set inputlog [open "test.inputlog" w]
