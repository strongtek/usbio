proc sf_get_status {} {
    set tx_data 05
    set tx_data_binary [binary format H* $tx_data]
    set tx_data_length [string length $tx_data_binary]
    spi_master_single_write $tx_data_binary $tx_data_length cs_keep
    set rx_data_binary [spi_master_single_read 1]
    scan $rx_data_binary %c byte_value
    set hex_value [format "0x%02x" $byte_value]

    return $hex_value
}

proc sf_wait_wip {} {
    set sf_status [sf_get_status]
    while { [expr {$sf_status & [expr {1 << 0}]}] == 0x01 } {
        set sf_status [sf_get_status]
    }
}

proc sf_id {} {
    set tx_data 9f
    set tx_data_binary [binary format H* $tx_data]
    set tx_data_length [string length $tx_data_binary]
    spi_master_single_write $tx_data_binary $tx_data_length cs_keep
    set rx_data_binary [spi_master_single_read 3]
    binary scan $rx_data_binary H* rx_scan_data
    puts "chip id is: $rx_scan_data"
}

proc sf_read {address file_name length} {
    puts "reading flash at address $address, with file $file_name."

    set start_time [clock seconds]

    set fp [open $file_name wb]

    for {set i 0} {$i < $length} {incr i 8192} {
        set remaining [expr {$length - $i}]
        set round_size [expr {$remaining < 8192 ? $remaining : 8192}]

        set tx_data 03
        set tx_data_binary [binary format H* $tx_data]
        set tx_data_length [string length $tx_data_binary]
        spi_master_single_write $tx_data_binary $tx_data_length cs_keep
    
        set round_address [expr $address+$i]
        set round_address_hex [format "%06x" $round_address]
        set tx_data $round_address_hex
        set tx_data_binary [binary format H* $tx_data]
        set tx_data_length [string length $tx_data_binary]
        spi_master_single_write $tx_data_binary $tx_data_length cs_keep

        set rx_data_binary [spi_master_single_read $round_size]

        puts -nonewline $fp $rx_data_binary
    }

    close $fp

    set end_time [clock seconds]
    set elapsed_time [expr {$end_time - $start_time}]
    puts "read in $elapsed_time second(s)."
}

proc sf_chip_erase {} {
    puts "chip erase."

    set start_time [clock seconds]

    set tx_data 06
    set tx_data_binary [binary format H* $tx_data]
    set tx_data_length [string length $tx_data_binary]
    spi_master_single_write $tx_data_binary $tx_data_length

    set tx_data 60
    set tx_data_binary [binary format H* $tx_data]
    set tx_data_length [string length $tx_data_binary]
    spi_master_single_write $tx_data_binary $tx_data_length

    sf_wait_wip

    set end_time [clock seconds]
    set elapsed_time [expr {$end_time - $start_time}]
    puts "chip erased in $elapsed_time second(s)."
}

proc sf_sector_erase {address} {
    set tx_data 06
    set tx_data_binary [binary format H* $tx_data]
    set tx_data_length [string length $tx_data_binary]
    spi_master_single_write $tx_data_binary $tx_data_length

    set tx_data 20
    set tx_data_binary [binary format H* $tx_data]
    set tx_data_length [string length $tx_data_binary]
    spi_master_single_write $tx_data_binary $tx_data_length cs_keep

    set address_hex [format "%06x" $address]
    set tx_data $address_hex
    set tx_data_binary [binary format H* $tx_data]
    set tx_data_length [string length $tx_data_binary]
    spi_master_single_write $tx_data_binary $tx_data_length

    sf_wait_wip
}

proc sf_prog {address file_name length} {
    puts "programming flash at address $address, with file $file_name."
    set start_time [clock seconds]

    set file_length [file size $file_name]
    if { $length == 0 } { set length $file_length}
    if { $length > $file_length } { set length $file_length }

    set number_of_sector [expr int([expr {ceil(double($length) / double(4096))}])]

    for {set i 0} {$i < $number_of_sector} {incr i 1} {
        set sector_address [expr $address+4096*$i]
        sf_sector_erase $sector_address
    }

    set fp [open $file_name rb]

    for {set i 0} {$i < $length} {incr i 256} {
        set tx_data 06
        set tx_data_binary [binary format H* $tx_data]
        set tx_data_length [string length $tx_data_binary]
        spi_master_single_write $tx_data_binary $tx_data_length

        set tx_data 02
        set tx_data_binary [binary format H* $tx_data]
        set tx_data_length [string length $tx_data_binary]
        spi_master_single_write $tx_data_binary $tx_data_length cs_keep

        set round_address [expr $address+$i]
        set round_address_hex [format "%06x" $round_address]
        set tx_data $round_address_hex
        set tx_data_binary [binary format H* $tx_data]
        set tx_data_length [string length $tx_data_binary]
        spi_master_single_write $tx_data_binary $tx_data_length cs_keep

        set remaining [expr {$length - $i}]
        set round_size [expr {$remaining < 256 ? $remaining : 256}]
        set tx_data_binary [read $fp $round_size]
        set tx_data_length [string length $tx_data_binary]
        spi_master_single_write $tx_data_binary $tx_data_length

        sf_wait_wip
    }
    close $fp

    set end_time [clock seconds]
    set elapsed_time [expr {$end_time - $start_time}]
    puts "programmed in $elapsed_time seconds(s)."
}

proc at24c32_prog {slave address file_name length} {
    puts "programming at24c32 at address $address, with file $file_name."
    set start_time [clock seconds]

    set file_length [file size $file_name]
    if { $length == 0 } { set length $file_length}
    if { $length > $file_length } { set length $file_length }

    set fp [open $file_name rb]

    for {set i 0} {$i < $length} {incr i 32} {
        set tx_data [expr $address+$i]
        set tx_data_hex [format "%04x" $tx_data]
        set tx_data_bin [binary format H* $tx_data_hex]

        append tx_data_bin [read $fp 32]

        i2c_master_write $slave $tx_data_bin 34

        while { 1 } {
            i2c_master_read $slave 1
            set i2c_status [i2c_master_get_status]
            if { $i2c_status eq 32 } {
                break
            }
        }
        after 1
    }
    close $fp

    set end_time [clock seconds]
    set elapsed_time [expr {$end_time - $start_time}]
    puts "programmed in $elapsed_time seconds(s)."
}


proc at24c32_read {slave address file_name length} {
    puts "reading at24c32 at address $address, with file $file_name."

    set start_time [clock seconds]

    set fp [open $file_name wb]

    set tx_data [expr $address]
    set tx_data_hex [format "%04x" $tx_data]
    set tx_data_bin [binary format H* $tx_data_hex]
    i2c_master_write $slave $tx_data_bin 2

    set rx_data_binary [i2c_master_read $slave $length]
    puts -nonewline $fp $rx_data_binary

    flush $fp
    close $fp

    set end_time [clock seconds]
    set elapsed_time [expr {$end_time - $start_time}]
    puts "read in $elapsed_time second(s)."
}
puts "usbio start."
puts [info nameofexecutable]
puts [adapter_list]
adapter_open 0
adapter_get_version
adapter_frequency 24000

# For SPI
#spi_master_init 1 0 0
#spi_master_set_lines 1
#spi_master_set_mode 0 0
#spi_set_drive_strength 3
#spi_reset_transaction

# For I2C
#i2c_master_init 400


#adapter_uninitialize
adapter_close
