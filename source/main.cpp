#ifdef _WIN32
#include <windows.h>
#include "LibFT4222.h"
#else
#include "libft4222.h"
#endif

#include <stdio.h>
#include <string>
#include <vector>
#include <cstdint>
#include "cmdline.h"
#include "ftd2xx.h"

#include "tcl.h"

#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

struct XferConfig
{
    int frequency;
    int real_freq;
    unsigned char tx_buffer[8192];
    unsigned char rx_buffer[8192];
    FT4222_ClockRate sys_clk;
    FT4222_SPIClock clk_div;
};

std::vector <FT_DEVICE_LIST_INFO_NODE> AdapterList;
FT_HANDLE ftHandle;
FT_STATUS ftStatus;
struct XferConfig Config;

inline std::string DeviceFlagToString(DWORD flags)
{
    std::string msg;
    msg += (flags & 0x1)? "Opened" : "Closed";
    msg += ", ";
    msg += (flags & 0x2)? "High-speed USB" : "Full-speed USB";
    return msg;
}

void DetectAdapters(void)
{
    FT_DEVICE_LIST_INFO_NODE devInfo;
    FT_STATUS ftStatus = 0;
    DWORD numOfDevices = 0;

    ftStatus = FT_CreateDeviceInfoList(&numOfDevices);
    for(DWORD i=0; i<numOfDevices; ++i)
    {
        memset(&devInfo, 0, sizeof(devInfo));
        ftStatus = FT_GetDeviceInfoDetail(
            i,
            &devInfo.Flags,
            &devInfo.Type,
            &devInfo.ID,
            &devInfo.LocId,
            devInfo.SerialNumber,
            devInfo.Description,
            &devInfo.ftHandle
        );

        const std::string description = devInfo.Description;
        if ( (ftStatus==FT_OK) && (description.find("FT4222")!= std::string::npos) )
        {
            AdapterList.push_back(devInfo);
        }
    }
}

void PrintAdapters(void)
{
    int number_of_adapters;
    int i;

    number_of_adapters = AdapterList.size();
    printf("Info: %d adapter(s) detected:\n", number_of_adapters);
    for(i=0; i<number_of_adapters; i++)
    {
            printf("  adapter %d\n", i);
            printf("    Flags        : 0x%x (%s)\n", AdapterList[i].Flags, DeviceFlagToString(AdapterList[i].Flags).c_str());
            printf("    Type         : 0x%x\n",      AdapterList[i].Type);
            printf("    ID           : 0x%x\n",      AdapterList[i].ID);
            printf("    LocID        : 0x%x\n",      AdapterList[i].LocId);
            printf("    SerialNumber : %s\n",        AdapterList[i].SerialNumber);
            printf("    Description  : %s\n",        AdapterList[i].Description);
            #ifdef _WIN32
            printf("    ftHandle     : 0x%x\n",      reinterpret_cast<uintptr_t>(AdapterList[i].ftHandle));
            #endif
            #ifdef LINUX
            printf("    ftHandle     : 0x%x\n",      (unsigned int)(uintptr_t)AdapterList[i].ftHandle);
            #endif
    }
    printf("Use adapter(s) with decription \"FT4222 A\" for SPI/I2C, and \"FT4222 B\" for GPIO.\n");
}

//
// tcl command 
//

int do_adapter_list(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 1)
    {
        printf("Error: adapter_list accepts no parameter.\n");
        return TCL_ERROR;
    }

    // detect
    DetectAdapters();
    if(AdapterList.size()==0)
    {
        printf("Error: no adapter detect.\n");
        return TCL_ERROR;
    }

    PrintAdapters();

    return TCL_OK;
}

int do_adapter_open(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    DWORD locID;
    int adapter_index;

    if (objc != 2)
    {
        printf("Error: adapter_open <adapter_index>.\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &adapter_index) != TCL_OK)
    {
        printf("Error: <adapter_index> should be a int number.\n");;
        return TCL_ERROR;
    }

    if( adapter_index>AdapterList.size()-1 )
    {
        printf("Error: adapter number %d, is beyond available range %ld, use --list or list_adapter to show available adapters.\n", adapter_index, AdapterList.size()-1);
        return TCL_ERROR;
    }

    locID = AdapterList[adapter_index].LocId;
    ftStatus = FT_OpenEx((PVOID)(uintptr_t)locID, FT_OPEN_BY_LOCATION, &ftHandle);
    if(ftStatus!=FT_OK)
    {
        printf("Error: FT_OpenEX returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    debug("Info: adapter_open %d, done.\n", adapter_index);

    return TCL_OK;
}

int do_adapter_close(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 1)
    {
        printf("Error: adapter_close accepts no parameter.\n");
        return TCL_ERROR;
    }

    ftStatus = FT_Close(ftHandle);
    if(ftStatus!=FT_OK)
    {
        printf("Error: FT_Close returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }
    else
    {
        debug("Info: adapter_close, done.\n");
    }

    return TCL_OK;
}

int do_adapter_uninitialize(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 1)
    {
        printf("Error: adapter_uninitialize accepts no parameter.\n");
        return TCL_ERROR;
    }

    ftStatus = FT4222_UnInitialize(ftHandle);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_UnInitialize returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_UnInitialize returns(%d), unknown error.\n", ftStatus);
    }
    else
    {
        debug("Info: adapter_uninitialize, done.\n");
    }

    return TCL_OK;
}


int do_adapter_frequency(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    DWORD locID;
    FT4222_ClockRate sys_clk;
    FT4222_SPIClock clk_div;
    int freq;
    int real_freq;

    if (objc != 2)
    {
        printf("Error: adapter_frequency <kHz>.\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &freq) != TCL_OK)
    {
        printf("Error: <kHz> should be a int number.\n");;
        return TCL_ERROR;
    }

    freq = freq*1000;

    // sck lookup table
    //
    //  sck         sys_clk     clk_div
    //   46.875K    24MHz       div_512
    //   93.75K     48MHz       div_512
    //   117.18K    60MHz       div_512
    //   156.25K    80MHz       div_512
    //   187.5K     48MHz       div_256
    //   234.375K   60MHz       div_256
    //   312.5K     80MHz       div_256
    //   375K       48MHz       div_128
    //   468.75K    60MHz       div_128
    //   625K       80MHz       div_128
    //   750K       48MHz       div_64
    //   937.5K     60MHz       div_64
    //   1.25M      80MHz       div_64
    //   1.5M       48MHz       div_32
    //   1.875M     60MHz       div_32
    //   2.5M       80MHz       div_32
    //   3M         48MHz       div_16
    //   3.75M      60MHz       div_16
    //   5M         80MHz       div_16
    //   6M         48MHz       div_8
    //   7.5M       60MHz       div_8
    //   10M        80MHz       div_8
    //   12M        48MHz       div_4
    //   15M        60MHz       div_4
    //   20M        80MHz       div_16(errata suggest)
    //   24M        48MHz       div_2
    //   30M        60MHz       div_2
    //   40M        80MHz       div_2
    if     (freq<   93750) {sys_clk=SYS_CLK_24; clk_div=CLK_DIV_512; real_freq=   46875;}
    else if(freq<  117180) {sys_clk=SYS_CLK_48; clk_div=CLK_DIV_512; real_freq=   93750;}
    else if(freq<  156250) {sys_clk=SYS_CLK_60; clk_div=CLK_DIV_512; real_freq=  117180;}
    else if(freq<  187500) {sys_clk=SYS_CLK_80; clk_div=CLK_DIV_512; real_freq=  156250;}
    else if(freq<  234375) {sys_clk=SYS_CLK_48; clk_div=CLK_DIV_256; real_freq=  187500;}
    else if(freq<  312500) {sys_clk=SYS_CLK_60; clk_div=CLK_DIV_256; real_freq=  234375;}
    else if(freq<  375000) {sys_clk=SYS_CLK_80; clk_div=CLK_DIV_256; real_freq=  312500;}
    else if(freq<  468750) {sys_clk=SYS_CLK_48; clk_div=CLK_DIV_128; real_freq=  375000;}
    else if(freq<  625000) {sys_clk=SYS_CLK_60; clk_div=CLK_DIV_128; real_freq=  468750;}
    else if(freq<  750000) {sys_clk=SYS_CLK_80; clk_div=CLK_DIV_128; real_freq=  625000;}
    else if(freq<  937500) {sys_clk=SYS_CLK_48; clk_div=CLK_DIV_64 ; real_freq=  750000;}
    else if(freq< 1250000) {sys_clk=SYS_CLK_60; clk_div=CLK_DIV_64 ; real_freq=  937500;}
    else if(freq< 1500000) {sys_clk=SYS_CLK_80; clk_div=CLK_DIV_64 ; real_freq= 1250000;}
    else if(freq< 1875000) {sys_clk=SYS_CLK_48; clk_div=CLK_DIV_32 ; real_freq= 1500000;}
    else if(freq< 2500000) {sys_clk=SYS_CLK_60; clk_div=CLK_DIV_32 ; real_freq= 1875000;}
    else if(freq< 3000000) {sys_clk=SYS_CLK_80; clk_div=CLK_DIV_32 ; real_freq= 2500000;}
    else if(freq< 3750000) {sys_clk=SYS_CLK_48; clk_div=CLK_DIV_16 ; real_freq= 3000000;}
    else if(freq< 5000000) {sys_clk=SYS_CLK_60; clk_div=CLK_DIV_16 ; real_freq= 3750000;}
    else if(freq< 6000000) {sys_clk=SYS_CLK_80; clk_div=CLK_DIV_16 ; real_freq= 5000000;}
    else if(freq< 7500000) {sys_clk=SYS_CLK_48; clk_div=CLK_DIV_8  ; real_freq= 6000000;}
    else if(freq<10000000) {sys_clk=SYS_CLK_60; clk_div=CLK_DIV_8  ; real_freq= 7500000;}
    else if(freq<12000000) {sys_clk=SYS_CLK_80; clk_div=CLK_DIV_8  ; real_freq=10000000;}
    else if(freq<15000000) {sys_clk=SYS_CLK_48; clk_div=CLK_DIV_4  ; real_freq=12000000;}
    else if(freq<20000000) {sys_clk=SYS_CLK_60; clk_div=CLK_DIV_4  ; real_freq=15000000;}
    else if(freq<24000000) {sys_clk=SYS_CLK_80; clk_div=CLK_DIV_16 ; real_freq=20000000;}
    else if(freq<30000000) {sys_clk=SYS_CLK_48; clk_div=CLK_DIV_2  ; real_freq=24000000;}
    else if(freq<40000000) {sys_clk=SYS_CLK_60; clk_div=CLK_DIV_2  ; real_freq=30000000;}
    else                   {sys_clk=SYS_CLK_80; clk_div=CLK_DIV_2  ; real_freq=40000000;}

    if(freq>=20000000 && freq<24000000) {sys_clk=SYS_CLK_80; clk_div=CLK_DIV_16 ; real_freq=5000000;}

    Config.sys_clk = sys_clk;
    Config.clk_div = clk_div;
    Config.frequency = freq;
    Config.real_freq = real_freq;

    printf("Info: target frequency %.3fkHz, rounded to %.3fkHz.\n", (float)freq/1000, (float)real_freq/1000);

    return TCL_OK;
}


int do_adapter_get_version(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    FT4222_Version ver;

    if (objc != 1)
    {
        printf("Error: adapter_get_version accepts no parameter.\n");
        return TCL_ERROR;
    }

    ftStatus = FT4222_GetVersion(ftHandle, &ver);
    if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_GetVersion returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    printf("Info: chip version 0x%08x, DLL version 0x%08x.\n", ver.chipVersion, ver.dllVersion);

    return TCL_OK;
}

int do_adapter_chip_reset(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 1)
    {
        printf("Error: adapter_chip_reset accepts no parameter.\n");
        return TCL_ERROR;
    }

    ftStatus = FT4222_ChipReset(ftHandle);
    if(ftStatus==FT4222_DEVICE_NOT_SUPPORTED)
    {
        printf("Error: FT4222_ChipReset returns(%d), FT4222_DEVICE_NOT_SUPPORTED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_ChipReset returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    debug("Info: adapter_chip_reset, done.\n");
    return TCL_OK;
}

int do_spi_reset_transaction(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 1)
    {
        printf("Error: spi_reset_transaction accepts no parameter.\n");
        return TCL_ERROR;
    }

    ftStatus = FT4222_SPI_ResetTransaction(ftHandle, 0);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_SPI_ResetTransaction returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_PARAMETER)
    {
        printf("Error: FT4222_SPI_ResetTransaction returns(%d), INVALID_PARAMETER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_SPI_ResetTransaction returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    debug("Info: spi_reset_transaction, done.\n");
    return TCL_OK;
}

int do_spi_reset(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 1)
    {
        printf("Error: spi_reset accepts no parameter.\n");
        return TCL_ERROR;
    }

    ftStatus = FT4222_SPI_Reset(ftHandle);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_SPI_Reset returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_SPI_Reset returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    debug("Info: spi_reset, done.\n");
    return TCL_OK;
}

int do_spi_set_drive_strength(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int drive_strength;
    SPI_DrivingStrength ds;

    if (objc != 2)
    {
        printf("Error: spi_set_drive_strength <0~3>.\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &drive_strength) != TCL_OK)
    {
        printf("Error: <drive_strength> should be a int number.\n");;
        return TCL_ERROR;
    }

    if( (drive_strength<0) || (drive_strength>3) )
    {
        printf("Error: drive_strength should be 0~3.\n");
        return TCL_ERROR;
    }

    ds =
        (drive_strength==0) ? DS_4MA  : \
        (drive_strength==1) ? DS_8MA  : \
        (drive_strength==2) ? DS_12MA : \
        (drive_strength==3) ? DS_16MA : \
        DS_16MA;

    ftStatus = FT4222_SPI_SetDrivingStrength(ftHandle, ds, ds, ds);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_SPI_SetDrivingStrength returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_IS_NOT_SPI_MODE)
    {
        printf("Error: FT4222_SPI_SetDrivingStrength returns(%d), FT4222_IS_NOT_SPI_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_SPI_SetDrivingStrength returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    debug("Info: spi_set_drive_strength %d, done.\n", drive_strength);
    return TCL_OK;
}

int do_spi_master_init(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int lines;
    int cpol;
    int cpha;
    FT4222_SPIMode ioLine;
    FT4222_SPICPOL ftCPOL;
    FT4222_SPICPHA ftCPHA;

    if (objc != 4)
    {
        printf("Error: spi_master_init <lines> <cpol> <cpha>.\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &lines) != TCL_OK)
    {
        printf("Error: <lines> should be a int number.\n");;
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[2], &cpol) != TCL_OK)
    {
        printf("Error: <cpol> should be a int number.\n");;
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[3], &cpha) != TCL_OK)
    {
        printf("Error: <cpha> should be a int number.\n");;
        return TCL_ERROR;
    }

    if( (lines!=1) && (lines!=2) && (lines!=4) )
    {
        printf("Error: lines should be 1/2/4.\n");
        return TCL_ERROR;
    }

    if( (cpol!=0) && (cpol!=1) )
    {
        printf("Error: cpol should be 0/1.\n");
        return TCL_ERROR;
    }

    if( (cpha!=0) && (cpha!=1) )
    {
        printf("Error: cpha should be 0/1.\n");
        return TCL_ERROR;
    }

    ioLine = (lines==1) ? SPI_IO_SINGLE :(lines==2) ? SPI_IO_DUAL : (lines==4) ? SPI_IO_QUAD : SPI_IO_SINGLE;
    ftCPOL = (cpol==0) ? CLK_IDLE_LOW : CLK_IDLE_HIGH;
    ftCPHA = (cpha==0) ? CLK_LEADING : CLK_TRAILING;

    ftStatus = FT4222_SetClock(ftHandle, Config.sys_clk);
    if(ftStatus==FT4222_DEVICE_NOT_SUPPORTED)
    {
        printf("Error: FT4222_SetClock fail(%d), FT4222_DEVICE_NOT_SUPPORT.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_SetClock fail(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }
    debug("Info: frequency set to %.3fkHz.\n", (float)Config.real_freq/1000);

    ftStatus = FT4222_SPIMaster_Init(ftHandle, ioLine, Config.clk_div, ftCPOL, ftCPHA, 0x1);
    if(ftStatus==FT4222_DEVICE_NOT_SUPPORTED)
    {
        printf("Error: FT4222_SPIMaster_Init returns(%d), FT4222_DEVICE_NOT_SUPPORTED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_PARAMETER)
    {
        printf("Error: FT4222_SPIMaster_Init returns(%d), INVALID_PARAMETER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_SPIMaster_Init returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }
    debug("Info: spi_master_init %d %d %d, done.\n", lines, cpol, cpha);

    return TCL_OK;
}

int do_spi_master_set_lines(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int lines;
    FT4222_SPIMode ioLine;

    if (objc != 2)
    {
        printf("Error: spi_master_set_lines <lines>.\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &lines) != TCL_OK)
    {
        printf("Error: <lines> should be a int number.\n");;
        return TCL_ERROR;
    }

    if( (lines!=1) && (lines!=2) && (lines!=4) )
    {
        printf("Error: lines should be 1/2/4.\n");
        return TCL_ERROR;
    }

    ioLine = (lines==1) ? SPI_IO_SINGLE :(lines==2) ? SPI_IO_DUAL : (lines==4) ? SPI_IO_QUAD : SPI_IO_SINGLE;

    ftStatus = FT4222_SPIMaster_SetLines(ftHandle, ioLine);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_SPIMaster_SetLines returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_IS_NOT_SPI_MODE)
    {
        printf("Error: FT4222_SPIMaster_SetLines returns(%d), FT4222_IS_NOT_SPI_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_NOT_SUPPORTED)
    {
        printf("Error: FT4222_SPIMaster_SetLines returns(%d), FT4222_NOT_SUPPORTED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_SPIMaster_SetLines returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }
    debug("Info: spi_master_set_lines %d, done.\n", lines);

    return TCL_OK;
}

int do_spi_master_set_mode(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int cpol;
    int cpha;
    FT4222_SPICPOL ftCPOL;
    FT4222_SPICPHA ftCPHA;

    if (objc != 3)
    {
        printf("Error: spi_master_set_mode <cpol> <cpha>.\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &cpol) != TCL_OK)
    {
        printf("Error: <cpol> should be a int number.\n");;
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[2], &cpha) != TCL_OK)
    {
        printf("Error: <cpha> should be a int number.\n");;
        return TCL_ERROR;
    }

    if( (cpol!=0) && (cpol!=1) )
    {
        printf("Error: cpol should be 0/1.\n");
        return TCL_ERROR;
    }

    if( (cpha!=0) && (cpha!=1) )
    {
        printf("Error: cpha should be 0/1.\n");
        return TCL_ERROR;
    }

    ftCPOL = (cpol==0) ? CLK_IDLE_LOW : CLK_IDLE_HIGH;
    ftCPHA = (cpha==0) ? CLK_LEADING : CLK_TRAILING;

    ftStatus = FT4222_SPIMaster_SetMode(ftHandle, ftCPOL, ftCPHA);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_SPIMaster_SetMode returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_IS_NOT_SPI_MODE)
    {
        printf("Error: FT4222_SPIMaster_SetMode returns(%d), FT4222_IS_NOT_SPI_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_NOT_SUPPORTED)
    {
        printf("Error: FT4222_SPIMaster_SetMode returns(%d), FT4222_NOT_SUPPORTED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_SPIMaster_SetMode returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }
    debug("Info: spi_master_set_mode %d %d, done.\n", cpol, cpha);

    return TCL_OK;
}

int do_spi_master_single_write(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int i;
    int length;
    int array_length;
    unsigned char* objv1_array;
    std::string objv3_string;
    bool cs_keep;
    bool isEndTransaction;
    uint16_t sizeTransferred;

    if ( (objc!=3) && (objc!=4))
    {
        printf("Error: spi_master_single_write <write_buffer> <length> [cs_keep].\n");
        return TCL_ERROR;
    }

    objv1_array = Tcl_GetByteArrayFromObj(objv[1], &array_length);

    if (Tcl_GetIntFromObj(interp, objv[2], &length) != TCL_OK)
    {
        printf("Error: <length> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: length = %d\n", length);

    for(i=0; i<8192; i++)
    {
        Config.tx_buffer[i] = 0x0;
    }

    for(i=0; i<length; i++)
    {
        Config.tx_buffer[i] = static_cast<unsigned char>(objv1_array[i]);
    }

    debug("Debug: tx_buffer:\n");
    for(i=0; i<length; i++)
    {
        debug(" %02x", Config.tx_buffer[i]);
    }
    debug("\n");

    if (objc==4)
    {
        objv3_string = Tcl_GetString(objv[3]);
        cs_keep = (objv3_string=="cs_keep") ? true : false; 
    }
    else
        cs_keep = false;
    debug("Debug: cs_keep = %s\n", cs_keep?"true":"false");
    isEndTransaction = cs_keep?false:true;

    ftStatus = FT4222_SPIMaster_SingleWrite(ftHandle, Config.tx_buffer, (uint16_t)length, &sizeTransferred, isEndTransaction);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_SPIMaster_SingleWrite returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_POINTER)
    {
        printf("Error: FT4222_SPIMaster_SingleWrite returns(%d), FT4222_INVALID_POINTER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_IS_NOT_SPI_SINGLE_MODE)
    {
        printf("Error: FT4222_SPIMaster_SingleWrite returns(%d), FT4222_IS_NOT_SPI_SINGLE_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_WRITE_DEVICE)
    {
        printf("Error: FT4222_SPIMaster_SingleWrite returns(%d), FT4222_FAILED_TO_WRITE_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_READ_DEVICE)
    {
        printf("Error: FT4222_SPIMaster_SingleWrite returns(%d), FT4222_FAILED_TO_READ_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_SPIMaster_SingleWrite returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    if((int)sizeTransferred != length)
    {
        printf("Error: FT4222_SPIMaster_SingleWrite is required to transfer %d byte(s), but actually transfer %d byte(s).\n", length, sizeTransferred);
        return TCL_ERROR;
    }

    debug("Info: spi_master_single_write, done.\n");
    return TCL_OK;
}

int do_spi_master_single_read(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int i;
    int length;
    std::string objv3_string;
    bool cs_keep;
    bool isEndTransaction;
    uint16_t sizeTransferred;

    if ( (objc!=2) && (objc!=3))
    {
        printf("Error: spi_master_single_read <length> [cs_keep].\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &length) != TCL_OK)
    {
        printf("Error: <length> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: length = %d\n", length);

    if (objc==3)
    {
        objv3_string = Tcl_GetString(objv[2]);
        cs_keep = (objv3_string=="cs_keep") ? true : false; 
    }
    else
        cs_keep = false;
    debug("Debug: cs_keep = %s\n", cs_keep?"true":"false");
    isEndTransaction = cs_keep?false:true;

    for(i=0; i<8192; i++)
    {
        Config.rx_buffer[i] = 0x0;
    }

    ftStatus = FT4222_SPIMaster_SingleRead(ftHandle, Config.rx_buffer, (uint16_t)length, &sizeTransferred, isEndTransaction);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_SPIMaster_SingleRead returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_POINTER)
    {
        printf("Error: FT4222_SPIMaster_SingleRead returns(%d), FT4222_INVALID_POINTER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_IS_NOT_SPI_SINGLE_MODE)
    {
        printf("Error: FT4222_SPIMaster_SingleRead returns(%d), FT4222_IS_NOT_SPI_SINGLE_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_WRITE_DEVICE)
    {
        printf("Error: FT4222_SPIMaster_SingleRead returns(%d), FT4222_FAILED_TO_WRITE_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_READ_DEVICE)
    {
        printf("Error: FT4222_SPIMaster_SingleRead returns(%d), FT4222_FAILED_TO_READ_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_SPIMaster_SingleRead returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    if(sizeTransferred != length)
    {
        printf("Error: FT4222_SPIMaster_SingleRead is required to transfer %d byte(s), but actually transfer %d byte(s).\n", length, sizeTransferred);
        return TCL_ERROR;
    }

    debug("Debug: rx_buffer:\n");
    for(i=0; i<length; i++)
    {
        debug(" %02x", Config.rx_buffer[i]);
    }
    debug("\n");

    Tcl_Obj *byteArrayObj = Tcl_NewByteArrayObj(Config.rx_buffer, length);
    Tcl_SetObjResult(interp, byteArrayObj);

    debug("Info: spi_master_single_read, done.\n");
    return TCL_OK;
}

int do_spi_master_single_read_write(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int i;
    int length;
    int array_length;
    unsigned char* objv1_array;
    std::string objv3_string;
    bool cs_keep;
    bool isEndTransaction;
    uint16_t sizeTransferred;

    if ( (objc!=3) && (objc!=4))
    {
        printf("Error: spi_master_single_write <write_buffer> <length> [cs_keep].\n");
        return TCL_ERROR;
    }

    objv1_array = Tcl_GetByteArrayFromObj(objv[1], &array_length);

    if (Tcl_GetIntFromObj(interp, objv[2], &length) != TCL_OK)
    {
        printf("Error: <length> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: length = %d\n", length);

    for(i=0; i<8192; i++)
    {
        Config.tx_buffer[i] = 0x0;
    }

    for(i=0; i<length; i++)
    {
        Config.tx_buffer[i] = static_cast<unsigned char>(objv1_array[i]);
    }

    debug("Debug: tx_buffer:\n");
    for(i=0; i<length; i++)
    {
        debug(" %02x", Config.tx_buffer[i]);
    }
    debug("\n");

    if (objc==4)
    {
        objv3_string = Tcl_GetString(objv[3]);
        cs_keep = (objv3_string=="cs_keep") ? true : false; 
    }
    else
        cs_keep = false;
    debug("Debug: cs_keep = %s\n", cs_keep?"true":"false");
    isEndTransaction = cs_keep?false:true;

    ftStatus = FT4222_SPIMaster_SingleReadWrite(ftHandle, Config.rx_buffer, Config.tx_buffer, (uint16_t)length, &sizeTransferred, isEndTransaction);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_SPIMaster_SingleReadWrite returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_POINTER)
    {
        printf("Error: FT4222_SPIMaster_SingleReadWrite returns(%d), FT4222_INVALID_POINTER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_IS_NOT_SPI_SINGLE_MODE)
    {
        printf("Error: FT4222_SPIMaster_SingleReadWrite returns(%d), FT4222_IS_NOT_SPI_SINGLE_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_WRITE_DEVICE)
    {
        printf("Error: FT4222_SPIMaster_SingleReadWrite returns(%d), FT4222_FAILED_TO_WRITE_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_READ_DEVICE)
    {
        printf("Error: FT4222_SPIMaster_SingleReadWrite returns(%d), FT4222_FAILED_TO_READ_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_SPIMaster_SingleReadWrite returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    if((int)sizeTransferred != length)
    {
        printf("Error: FT4222_SPIMaster_SingleReadWrite is required to transfer %d byte(s), but actually transfer %d byte(s).\n", length, sizeTransferred);
        return TCL_ERROR;
    }

    debug("Debug: rx_buffer:\n");
    for(i=0; i<length; i++)
    {
        debug(" %02x", Config.rx_buffer[i]);
    }
    debug("\n");

    Tcl_Obj *byteArrayObj = Tcl_NewByteArrayObj(Config.rx_buffer, length);
    Tcl_SetObjResult(interp, byteArrayObj);

    debug("Info: spi_master_single_read_write, done.\n");
    return TCL_OK;
}

int do_spi_master_multi_read_write(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int i;
    int single_write_length;
    int multi_write_length;
    int multi_read_length;
    int array_length;
    unsigned char* objv1_array;
    uint32_t sizeRead;
    int write_length;

    if (objc!=5)
    {
        printf("Error: spi_master_multi_read_write <write_buffer> <single_write_length> <multi_write_length> <multi_read_length>.\n");
        return TCL_ERROR;
    }

    objv1_array = Tcl_GetByteArrayFromObj(objv[1], &array_length);

    if (Tcl_GetIntFromObj(interp, objv[2], &single_write_length) != TCL_OK)
    {
        printf("Error: <single_write_length> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: single_write_length = %d\n", single_write_length);

    if (Tcl_GetIntFromObj(interp, objv[3], &multi_write_length) != TCL_OK)
    {
        printf("Error: <multi_write_length> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: multi_write_length = %d\n", multi_write_length);

    if (Tcl_GetIntFromObj(interp, objv[4], &multi_read_length) != TCL_OK)
    {
        printf("Error: <multi_read_length> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: multi_read_length = %d\n", multi_read_length);

    write_length = single_write_length+multi_write_length;

    for(i=0; i<8192; i++)
    {
        Config.tx_buffer[i] = 0x0;
    }

    for(i=0; i<write_length; i++)
    {
        Config.tx_buffer[i] = static_cast<unsigned char>(objv1_array[i]);
    }

    debug("Debug: tx_buffer:\n");
    for(i=0; i<write_length; i++)
    {
        debug(" %02x", Config.tx_buffer[i]);
    }
    debug("\n");

    ftStatus = FT4222_SPIMaster_MultiReadWrite
    (
        ftHandle,
        Config.rx_buffer,
        Config.tx_buffer,
        (uint16_t)single_write_length,
        (uint16_t)multi_write_length,
        (uint16_t)multi_read_length,
        &sizeRead
    );
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_SPIMaster_MultiReadWrite returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_POINTER)
    {
        printf("Error: FT4222_SPIMaster_MultiReadWrite returns(%d), FT4222_INVALID_POINTER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_WRITE_DEVICE)
    {
        printf("Error: FT4222_SPIMaster_MultiReadWrite returns(%d), FT4222_FAILED_TO_WRITE_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_READ_DEVICE)
    {
        printf("Error: FT4222_SPIMaster_MultiReadWrite returns(%d), FT4222_FAILED_TO_READ_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_SPIMaster_MultiReadWrite returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    if((int)sizeRead != multi_read_length)
    {
        printf("Error: FT4222_SPIMaster_MultiReadWrite is required to read %d byte(s), but actually read %d byte(s).\n", multi_read_length, sizeRead);
        return TCL_ERROR;
    }

    debug("Debug: rx_buffer:\n");
    for(i=0; i<multi_read_length; i++)
    {
        debug(" %02x", Config.rx_buffer[i]);
    }
    debug("\n");

    Tcl_Obj *byteArrayObj = Tcl_NewByteArrayObj(Config.rx_buffer, multi_read_length);
    Tcl_SetObjResult(interp, byteArrayObj);

    debug("Info: spi_master_multi_read_write, done.\n");
    return TCL_OK;
}

int do_i2c_master_init(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int freq;

    if (objc != 2)
    {
        printf("Error: i2c_master_init <kbps>.\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &freq) != TCL_OK)
    {
        printf("Error: <kbps> should be a int number.\n");;
        return TCL_ERROR;
    }

    ftStatus = FT4222_I2CMaster_Init(ftHandle, (uint32)freq);
    if(ftStatus==FT4222_DEVICE_NOT_SUPPORTED)
    {
        printf("Error: FT4222_I2CMaster_Init returns(%d), FT4222_DEVICE_NOT_SUPPORTED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_I2C_NOT_SUPPORTED_IN_THIS_MODE)
    {
        printf("Error: FT4222_I2CMaster_Init returns(%d), FT4222_I2C_NOT_SUPPORTED_IN_THIS_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_I2CMaster_Init returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    debug("Info: i2c_master_init %d, done.\n", freq);
    return TCL_OK;
}

int do_i2c_master_read(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int i;
    int slave;
    int length;
    uint16_t sizeTransferred;

    if (objc!=3)
    {
        printf("Error: i2c_master_read <slave> <length>.\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &slave) != TCL_OK)
    {
        printf("Error: <slave> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: slave = %d\n", slave);

    if (Tcl_GetIntFromObj(interp, objv[2], &length) != TCL_OK)
    {
        printf("Error: <length> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: length = %d\n", slave);

    for(i=0; i<8192; i++)
    {
        Config.rx_buffer[i] = 0x0;
    }

    ftStatus = FT4222_I2CMaster_Read(ftHandle, (uint16)slave, Config.rx_buffer, (uint16)length, &sizeTransferred);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_I2CMaster_Read returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_IS_NOT_I2C_MODE)
    {
        printf("Error: FT4222_I2CMaster_Read returns(%d), FT4222_IS_NOT_I2C_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_POINTER)
    {
        printf("Error: FT4222_I2CMaster_Read returns(%d), FT4222_INVALID_POINTER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_PARAMETER)
    {
        printf("Error: FT4222_I2CMaster_Read returns(%d), FT4222_INVALID_PARAMETER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_READ_DEVICE)
    {
        printf("Error: FT4222_I2CMaster_Read returns(%d), FT4222_FAILED_TO_READ_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_I2CMaster_Read returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    if(sizeTransferred != length)
    {
        printf("Error: FT4222_I2CMaster_Read is required to transfer %d byte(s), but actually transfer %d byte(s).\n", length, sizeTransferred);
        return TCL_ERROR;
    }

    debug("Debug: rx_buffer:\n");
    for(i=0; i<length; i++)
    {
        debug(" %02x", Config.rx_buffer[i]);
    }
    debug("\n");

    Tcl_Obj *byteArrayObj = Tcl_NewByteArrayObj(Config.rx_buffer, length);
    Tcl_SetObjResult(interp, byteArrayObj);

    debug("Info: i2c_master_read, done.\n");
    return TCL_OK;
}

int do_i2c_master_write(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int i;
    int slave;
    int length;
    uint16_t sizeTransferred;
    unsigned char* objv1_array;
    int array_length;

    if (objc!=4)
    {
        printf("Error: i2c_master_write <slave> <write_buffer> <length>.\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &slave) != TCL_OK)
    {
        printf("Error: <slave> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: slave = %d\n", slave);

    objv1_array = Tcl_GetByteArrayFromObj(objv[2], &array_length);

    if (Tcl_GetIntFromObj(interp, objv[3], &length) != TCL_OK)
    {
        printf("Error: <length> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: length = %d\n", slave);

    for(i=0; i<8192; i++)
    {
        Config.tx_buffer[i] = 0x0;
    }

    for(i=0; i<length; i++)
    {
        Config.tx_buffer[i] = static_cast<unsigned char>(objv1_array[i]);
    }

    debug("Debug: tx_buffer:\n");
    for(i=0; i<length; i++)
    {
        debug(" %02x", Config.tx_buffer[i]);
    }
    debug("\n");

    ftStatus = FT4222_I2CMaster_Write(ftHandle, slave, Config.tx_buffer, (uint16_t)length, &sizeTransferred);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_I2CMaster_Write returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_IS_NOT_I2C_MODE)
    {
        printf("Error: FT4222_I2CMaster_Write returns(%d), FT4222_IS_NOT_I2C_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_POINTER)
    {
        printf("Error: FT4222_I2CMaster_Write returns(%d), FT4222_INVALID_POINTER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_PARAMETER)
    {
        printf("Error: FT4222_I2CMaster_Write returns(%d), FT4222_INVALID_PARAMETER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_WRITE_DEVICE)
    {
        printf("Error: FT4222_I2CMaster_Write returns(%d), FT4222_FAILED_TO_WRITE_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_I2CMaster_Write returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    if((int)sizeTransferred != length)
    {
        printf("Error: FT4222_I2CMaster_Write is required to transfer %d byte(s), but actually transfer %d byte(s).\n", length, sizeTransferred);
        return TCL_ERROR;
    }

    debug("Info: i2c_master_write, done.\n");
    return TCL_OK;
}

int do_i2c_master_read_extension(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int i;
    int slave;
    int length;
    uint16_t sizeTransferred;
    std::string flag_string;
    uint8 flag;

    if (objc!=4)
    {
        printf("Error: i2c_master_read_extension <slave> <length> <flag>.\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &slave) != TCL_OK)
    {
        printf("Error: <slave> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: slave = %d\n", slave);

    if (Tcl_GetIntFromObj(interp, objv[2], &length) != TCL_OK)
    {
        printf("Error: <length> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: length = %d\n", slave);

    flag_string = Tcl_GetString(objv[3]);
    if(flag_string=="START")
        flag = 0x2;
    else if(flag_string=="Repeated_START")
        flag = 0x3;
    else if(flag_string=="STOP")
        flag = 0x4;
    else if(flag_string=="START_AND_STOP")
        flag = 0x6;
    else
    {
        printf("Error: flag should be <START|Repeated_START|STOP|START_AND_STOP>");
        return TCL_ERROR;
    }

    for(i=0; i<8192; i++)
    {
        Config.rx_buffer[i] = 0x0;
    }

    ftStatus = FT4222_I2CMaster_ReadEx(ftHandle, (uint16)slave, flag, Config.rx_buffer, (uint16)length, &sizeTransferred);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_I2CMaster_ReadEx returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_IS_NOT_I2C_MODE)
    {
        printf("Error: FT4222_I2CMaster_ReadEx returns(%d), FT4222_IS_NOT_I2C_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_POINTER)
    {
        printf("Error: FT4222_I2CMaster_ReadEx returns(%d), FT4222_INVALID_POINTER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_PARAMETER)
    {
        printf("Error: FT4222_I2CMaster_ReadEx returns(%d), FT4222_INVALID_PARAMETER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_READ_DEVICE)
    {
        printf("Error: FT4222_I2CMaster_ReadEx returns(%d), FT4222_FAILED_TO_READ_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_I2CMaster_ReadEx returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    if(sizeTransferred != length)
    {
        printf("Error: FT4222_I2CMaster_ReadEx is required to transfer %d byte(s), but actually transfer %d byte(s).\n", length, sizeTransferred);
        return TCL_ERROR;
    }

    debug("Debug: rx_buffer:\n");
    for(i=0; i<length; i++)
    {
        debug(" %02x", Config.rx_buffer[i]);
    }
    debug("\n");

    Tcl_Obj *byteArrayObj = Tcl_NewByteArrayObj(Config.rx_buffer, length);
    Tcl_SetObjResult(interp, byteArrayObj);

    debug("Info: i2c_master_read_extension, done.\n");
    return TCL_OK;
}

int do_i2c_master_write_extension(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int i;
    int slave;
    int length;
    uint16_t sizeTransferred;
    unsigned char* objv1_array;
    int array_length;
    std::string flag_string;
    uint8 flag;

    if (objc!=5)
    {
        printf("Error: i2c_master_write_extension <slave> <write_buffer> <length> <flag>.\n");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &slave) != TCL_OK)
    {
        printf("Error: <slave> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: slave = %d\n", slave);

    objv1_array = Tcl_GetByteArrayFromObj(objv[2], &array_length);

    if (Tcl_GetIntFromObj(interp, objv[3], &length) != TCL_OK)
    {
        printf("Error: <length> should be a int number.\n");
        return TCL_ERROR;
    }
    debug("Debug: length = %d\n", slave);

    flag_string = Tcl_GetString(objv[4]);
    if(flag_string=="START")
        flag = 0x2;
    else if(flag_string=="Repeated_START")
        flag = 0x3;
    else if(flag_string=="STOP")
        flag = 0x4;
    else if(flag_string=="START_AND_STOP")
        flag = 0x6;
    else
    {
        printf("Error: flag should be <START|Repeated_START|STOP|START_AND_STOP>");
        return TCL_ERROR;
    }

    for(i=0; i<8192; i++)
    {
        Config.tx_buffer[i] = 0x0;
    }

    for(i=0; i<length; i++)
    {
        Config.tx_buffer[i] = static_cast<unsigned char>(objv1_array[i]);
    }

    debug("Debug: tx_buffer:\n");
    for(i=0; i<length; i++)
    {
        debug(" %02x", Config.tx_buffer[i]);
    }
    debug("\n");

    ftStatus = FT4222_I2CMaster_WriteEx(ftHandle, slave, flag, Config.tx_buffer, (uint16_t)length, &sizeTransferred);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_I2CMaster_WriteEx returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_IS_NOT_I2C_MODE)
    {
        printf("Error: FT4222_I2CMaster_WriteEx returns(%d), FT4222_IS_NOT_I2C_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_POINTER)
    {
        printf("Error: FT4222_I2CMaster_WriteEx returns(%d), FT4222_INVALID_POINTER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_INVALID_PARAMETER)
    {
        printf("Error: FT4222_I2CMaster_WriteEx returns(%d), FT4222_INVALID_PARAMETER.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_FAILED_TO_WRITE_DEVICE)
    {
        printf("Error: FT4222_I2CMaster_WriteEx returns(%d), FT4222_FAILED_TO_WRITE_DEVICE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_I2CMaster_WriteEx returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    if((int)sizeTransferred != length)
    {
        printf("Error: FT4222_I2CMaster_WriteEx is required to transfer %d byte(s), but actually transfer %d byte(s).\n", length, sizeTransferred);
        return TCL_ERROR;
    }

    debug("Info: i2c_master_write_extension, done.\n");
    return TCL_OK;
}

int do_i2c_master_get_status(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    uint8 controller_status;

    if (objc != 1)
    {
        printf("Error: i2c_master_get_status accepts no parameter.\n");
        return TCL_ERROR;
    }

    ftStatus = FT4222_I2CMaster_GetStatus(ftHandle, &controller_status);
    if(ftStatus!=FT4222_OK)
    {
        printf("Error: FT4222_I2CMaster_GetStatus returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    Tcl_Obj *intObj = Tcl_NewIntObj((int)controller_status);
    Tcl_SetObjResult(interp, intObj);

    debug("Info: i2c_master_get_status, done.\n");
    return TCL_OK;
}

int do_i2c_master_reset(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 1)
    {
        printf("Error: i2c_master_reset accepts no parameter.\n");
        return TCL_ERROR;
    }

    ftStatus = FT4222_I2CMaster_Reset(ftHandle);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_I2CMaster_Reset returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus==FT4222_IS_NOT_I2C_MODE)
    {
        printf("Error: FT4222_I2CMaster_Reset returns(%d), FT4222_IS_NOT_I2C_MODE.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_I2CMaster_Reset returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    debug("Info: i2c_master_reset, done.\n");
    return TCL_OK;
}

int do_i2c_master_reset_bus(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 1)
    {
        printf("Error: i2c_master_reset_bus accepts no parameter.\n");
        return TCL_ERROR;
    }

    ftStatus = FT4222_I2CMaster_ResetBus(ftHandle);
    if(ftStatus==FT4222_DEVICE_NOT_OPENED)
    {
        printf("Error: FT4222_I2CMaster_ResetBus returns(%d), FT4222_DEVICE_NOT_OPENED.\n", ftStatus);
        return TCL_ERROR;
    }
    else if(ftStatus!=FT_OK)
    {
        printf("Error: FT4222_I2CMaster_ResetBus returns(%d), unknown error.\n", ftStatus);
        return TCL_ERROR;
    }

    debug("Info: i2c_master_reset_bus, done.\n");
    return TCL_OK;
}

//
// main
//
int main(int argc, char *argv[])
{
    cmdline::parser a;
    std::string command_file;
    int tcl_code;
    Tcl_Interp *interp;

    Tcl_FindExecutable(argv[0]);

    a.add             ("list", 'l',  "list all connected adapters." );
    a.add<std::string>("file", 'f',  "command file.", false, "usbio.tcl");
    a.parse_check(argc, argv);

    // --list
    if( a.exist("list") == true )
    {
        DetectAdapters();
        if(AdapterList.size()==0)
        {
            printf("Error: no adapter detect.\n");
            exit(1);
        }

        PrintAdapters();
        exit(0);
    }

    interp = Tcl_CreateInterp();
    if( interp == NULL )
    {
        printf("Error: Tcl_CreateInterp error.\n");
        exit(1);
    }
    Tcl_Init(interp);

    Tcl_CreateObjCommand(interp, "adapter_list", do_adapter_list, NULL, NULL);
    Tcl_CreateObjCommand(interp, "adapter_open", do_adapter_open, NULL, NULL);
    Tcl_CreateObjCommand(interp, "adapter_close", do_adapter_close, NULL, NULL);
    Tcl_CreateObjCommand(interp, "adapter_uninitialize", do_adapter_uninitialize, NULL, NULL);
    Tcl_CreateObjCommand(interp, "adapter_frequency", do_adapter_frequency, NULL, NULL);
    Tcl_CreateObjCommand(interp, "adapter_get_version", do_adapter_get_version, NULL, NULL);
    Tcl_CreateObjCommand(interp, "adapter_chip_reset", do_adapter_chip_reset, NULL, NULL);
    Tcl_CreateObjCommand(interp, "spi_reset_transaction", do_spi_reset_transaction, NULL, NULL);
    Tcl_CreateObjCommand(interp, "spi_reset", do_spi_reset, NULL, NULL);
    Tcl_CreateObjCommand(interp, "spi_set_drive_strength", do_spi_set_drive_strength, NULL, NULL);
    Tcl_CreateObjCommand(interp, "spi_master_init", do_spi_master_init, NULL, NULL);
    Tcl_CreateObjCommand(interp, "spi_master_set_lines", do_spi_master_set_lines, NULL, NULL);
    Tcl_CreateObjCommand(interp, "spi_master_set_mode", do_spi_master_set_mode, NULL, NULL);
    Tcl_CreateObjCommand(interp, "spi_master_single_write", do_spi_master_single_write, NULL, NULL);
    Tcl_CreateObjCommand(interp, "spi_master_single_read", do_spi_master_single_read, NULL, NULL);
    Tcl_CreateObjCommand(interp, "spi_master_single_read_write", do_spi_master_single_read_write, NULL, NULL);
    Tcl_CreateObjCommand(interp, "spi_master_multi_read_write", do_spi_master_multi_read_write, NULL, NULL);
    Tcl_CreateObjCommand(interp, "i2c_master_init", do_i2c_master_init, NULL, NULL);
    Tcl_CreateObjCommand(interp, "i2c_master_read", do_i2c_master_read, NULL, NULL);
    Tcl_CreateObjCommand(interp, "i2c_master_write", do_i2c_master_write, NULL, NULL);
    Tcl_CreateObjCommand(interp, "i2c_master_read_extension", do_i2c_master_read_extension, NULL, NULL);
    Tcl_CreateObjCommand(interp, "i2c_master_write_extension", do_i2c_master_write_extension, NULL, NULL);
    Tcl_CreateObjCommand(interp, "i2c_master_get_status", do_i2c_master_get_status, NULL, NULL);
    Tcl_CreateObjCommand(interp, "i2c_master_reset", do_i2c_master_reset, NULL, NULL);
    Tcl_CreateObjCommand(interp, "i2c_master_reset_bus", do_i2c_master_reset_bus, NULL, NULL);

    // --file
    if( a.exist("file") == false )
    {
        printf("Use -f <file> to specify a Tcl script file.\n");
        exit(1);
    }
    command_file = a.get<std::string>("file");

    tcl_code = Tcl_EvalFile(interp, command_file.c_str());
    if( tcl_code != TCL_OK )
    {
        printf("Error: Tcl_EvalFile returns error %d.\n%s\n", \
            tcl_code,
            Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY)
        );
    }
    Tcl_DeleteInterp(interp);
    Tcl_Finalize();

    return 0;
}
