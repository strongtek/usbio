# Introduction

usbio is a USB to SPI/I2C bridge utility using FT4222.
In general, it's a Tcl wrapper of FTDI's LibFT4222 library.

# Feature

Features of FT4222 include:

* USB 2.0 high speed.

* Support single, dual and quad lines for SPI master.

* SCLK can support up to 30MHz for SPI master.

* Up to 53.8Mbps data rate for quad lines SPI master.

* Support I2C master interface, conforming to I2C v2.1 and v3.0.

* Support I2C speed modes: SM(100Kbps), FM(400kbps), FM+(1Mbps) and HS(3.4Mbps).

* +5V USB VBUS detection engine.

* Integrated 5V-3.3V-1.8V regulators.

* Configurable IO pin output drive strength.

Features of usbio include:

* Support SPI master and I2C master mode of FT4222.

* Programable with Tcl script.

* Tcl command for LibFT4222 APIs.

* Include work around of FT4222 errata.

* Compatible with PC. OS support Windows, Linux and MacOS

* Compatible with Raspberry Pi.

# Build

## Windows

1. Download and install FTDI driver from https://ftdichip.com/drivers/d2xx-drivers

1. Download and install MSYS2 from https://www.msys2.org

1. Install necessary packages in MINGW64.

    ```
    pacman -S git make mingw-w64-x86_64-toolchain mingw-w64-x86_64-tcl mingw-w64-x86_64-tcllib
    ```

1. Go to project/mingw, and make.

## Linux(include Raspberry Pi OS)

1. Go to imports/linux/libft4222-linux-1.4.4.188, run:
   
    ```
    sudo ./install4222.sh
    ```

    This will install the FT4222 library from FTDI.

1. Install necessary packages.
   
   CentOS/RHEL:

    ```
    sudo yum install tcl-devel
    ```

   Ubuntu/Raspberry Pi OS:

    ```
    sudo apt install git make gcc tcl-dev tcllib
    ```

    There might be some other packages missing on a brand new system, install them if you get errors.

1. Go to project/linux, and make.

## MacOS

1. Install homebrew, as instructed in https://brew.sh/

    ```
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    ```

1. Install necessary packages. 

1. Go to imports/mac, run:
   
    ```
    sudo mkdir -p /usr/local/lib /usr/local/include
    sudo ./install4222_mac.sh
    ```

    This will install the FT4222 library from FTDI.

1. Go to project/mac, and make.

You can also goto the Youtube channel to see the build procedure:

https://www.youtube.com/channel/UCNeU4PBaWM9OUf0JDZU0TXA

# Usage

## Pre requirements

After build the project, you will get usbio or usbio.exe executables in project/\<os> directory.

Note: This utility is free and open source, for your software supply chain security consideration, no binary executables are provided. You are encouraged to build this utility from source.
But if you get the binary executables from other where, you still need to do driver installation. eg:

For Linux:
```
cd imports/linux/libft4222-linux-1.4.4.188
sudo ./install4222.sh
```

For Windows: download driver from FTDI website as instructed in the Build section. Windows executable use dynamic link, better to put LibFT4222-64.dll in the same directory as usbio.exe.

For MacOS:

```
cd imports/mac
sudo mkdir -p /usr/local/lib /usr/local/include
sudo ./install4222_mac.sh
```

## Command line option

usbio accepts the following command line options:

```
./usbio -?
usage: ./usbio [options] ...
options:
  -l, --list    list all connected adapters.
  -f, --file    command file. (string [=usbio.tcl])
  -?, --help    print this message
```

-l/--list is used to list available adapters directly.

-f/--file <command.tcl> is used to specify the Tcl script file.

-?/--help will print a short help message.

usbio use the open source cmdline.h for command line option parsing.

## Tcl command

usbio add the following Tcl command.
Most of them are wrapper to LibFT4222 APIs.
Refer to AN_329 https://ftdichip.com/wp-content/uploads/2024/03/AN_329_User_Guide_for_LibFT4222-v1.8.pdf for more information.

* adapter_list

* adapter_open \<adapter_index>
  
      <adapter_index> is the index showed in adapter_list command.

* adapter_close

* adapter_uninitialize

* adapter_frequency \<freq>

      <freq> is the frequency in kHz of the SPI master. You can specify any value to this parameter, but the tool will round it to the nearest number no larger than you specified.

* adapter_get_version

* adapter_chip_reset

* spi_reset_transaction

* spi_reset

* spi_set_drive_strength <0|1|2|3>

* spi_master_init \<lines> \<cpol> \<cpha>

      <lines> can be 1, 2, 4.

      <cpol> and <cpha> can be 0, 1.

* spi_master_set_lines \<lines>

      <lines> can be 1, 2, 4.

* spi_master_set_mode \<cpol> \<cpha>

      <cpol> and <cpha> can be 0, 1.

* spi_master_single_write \<write_buffer> \<length> (\<cs_keep>)

      <write_buffer> is a byte array contains the data to write.

      <length> is the byte size to write.

      <cs_keep> is optional. If specified, the SPI_CS line will keep assert after this command.

* spi_master_single_read \<length> (\<ce_keep>)

      <length> is the byte size to write.

      <cs_keep> is optional. If specified, the SPI_CS line will keep assert after this command.

* spi_master_single_read_write \<write_buffer> \<length> (\<cs_keep>)

      <write_buffer> is a byte array contains the data to write.

      <length> is the byte size to write.

      <cs_keep> is optional. If specified, the SPI_CS line will keep assert after this command.

* spi_master_multi_read_write  \<write_buffer> \<single_write_length> \<multi_write_length> \<multi_read_length>

      <write_buffer> is a byte array contains the data to write.

      <single_write_length> is the byte size to write in single line mode.

      <multi_write_length> is the byte size to write in multi line mode.

      <multi_read_length> is the byte size to read in multi line mode.

* i2c_master_init \<kbps>

      <kbps> is the I2C speed.

* i2c_master_read \<slave> \<length>

      <slave> is the slave address to read.

      <length> is the byte size to write.

* i2c_master_write \<slave> \<write_buffer> \<length>

      <slave> is the slave address to write.

      <write_buffer> is a byte array contains the data to write.

      <length> is the byte size to write.

* i2c_master_read_extension \<slave> \<length> \<flag>

      <slave> is the slave address to write.

      <length> is the byte size to read.

      <flag> is flag of this trasaction, refer to AN_329.

* i2c_master_write_extension \<slave> <write_buffer> \<length> \<flag>

      <slave> is the slave address to write.

      <write_buffer> is a byte array contains the data to write.

      <length> is the byte size to read.

      <flag> is flag of this trasaction, refer to AN_329.

* i2c_master_get_status

* i2c_master_reset

* i2c_master_reset_bus

## Example

The example/usbio.tcl is a simple example Tcl script.

At the beginning of this example, there are some procs for operating SPI serial flash and AT24C32 EEPROM, which you can refer if you are working with these devices.

# Hardware

usbio should work with most of the FT4222 boards.

# Q&A

## Why Tcl?

Tcl script is easy to read and write, it's widely used in EDA software and embedded system, especially for integrated circuit design. 

With the programming ablity of Tcl, you can make full use of the chip easily.

You can use this utility for ATE test of integrated circuit, serial flash programming and other tests that use SPI/I2C.

## Library integrity

The FTDI library are downloaded from the following URL:

https://ftdichip.com/wp-content/uploads/2024/03/LibFT4222-v1.4.6.zip

https://ftdichip.com/wp-content/uploads/2024/03/libft4222-linux-1.4.4.188.tgz

https://ftdichip.com/wp-content/uploads/2024/03/LibFT4222-mac-v1.4.4.190.zip


You can check them with the library under ./imports for software supply chain security purpose.

Note: the original imports/windows/ftd2xx/ftd2xx.h is UTF16-LE encoded, which would lead to compiler error.
So it is change to UTF-8 encoding. The original file is moved to ftd2xx_utf16_le.h for check, you will find they have same content.

