/* asynIp330.h

********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************

    Original Author: Jim Kowalkowski
    Date: 2/15/95
    Current Authors: Mark Rivers, Joe Sullivan, and Marty Kraimer
    Converted to MPF: 12/9/98
    27-Oct-1999  MLR  Converted to ip330 (base class) from ip330Scan: 
    31-Mar-2003  MLR  Added MAX_IP330_CHANNELS definition
    11-Jul-2004  MLR  Converted from MPF to asyn and from C++ to C
*/

#ifndef asynIp330H
#define asynIp330H

typedef enum {ip330Data, 
              ip330Gain, 
              ip330ScanPeriod, 
              ip330CalibratePeriod,
              ip330ScanMode
} ip330Command;

#define MAX_IP330_COMMANDS 5

/* Implements the following asyn interfaces:
    Interface:          asynInt32   
    Method:             read   
    asynUser->drvUser:  0 or &ip330Data
    asynDrvUser->create "DATA"
    Description:        read the current value of a channel

    Interface:          asynInt32   
    Method:             write 
    asynUser->drvUser:  0 or &ip330Data 
    asynDrvUser->create "DATA"
    Description:        Error, undefined

    Interface:          asynUInt32Digital 
    Method:             read   
    asynUser->drvUser:  0 or &ip330Gain
    asynDrvUser->create "GAIN"
    Description:        read the gain for a channel

    Interface:          asynUInt32Digital
    Method:             write 
    asynUser->drvUser:  0 or &ip330Gain 
    asynDrvUser->create "GAIN"
    Description:        write the gain for a channel

    Interface:          asynInt32Callback 
    Method:             registerCallback
    asynUser->drvUser:  0 or &ip330Data
    asynDrvUser->create "DATA"
    Description:        register callback with the current value of a channel

    Interface:          asynFloat64 
    Method:             read
    asynUser->drvUser:  0 or &ip330Data 
    asynDrvUser->create "DATA"
    Description:        read the current value of a channel

    Interface:          asynFloat64
    Method:             write 
    asynUser->drvUser:  0 or &ip330Data
    asynDrvUser->create "DATA"
    Description:        Error, undefined

    Interface:          asynFloat64
    Method:             read
    asynUser->drvUser:  &ip330ScanPeriod 
    asynDrvUser->create "SCAN_PERIOD"
    Description:        Read the scan period

    Interface:          asynFloat64
    Method:             write
    asynUser->drvUser:  &ip330ScanPeriod
    asynDrvUser->create "SCAN_PERIOD"
    Description:        Write the scan period

    Interface:          asynFloat64
    Method:             read
    asynUser->drvUser:  &ip330CalibratePeriod
    asynDrvUser->create "CALIBRATE_PERIOD"
    Description:        Read the calibration period

    Interface:          asynFloat64
    Method:             write
    asynUser->drvUser:  &ip330CalibratePeriod
    asynDrvUser->create "CALIBRATE_PERIOD"
    Description:        Write the calibration period

    Interface:          asynFloat64Callback
    Method:             registerCallback
    asynUser->drvUser:  0 or &ip330Data
    asynDrvUser->create "DATA"
    Description:        Register callback with the current value of a channel

    Interface:          asynFloat64Callback
    Method:             registerCallback
    asynUser->drvUser:  &ip330ScanPeriod
    asynDrvUser->create "SCAN_PERIOD"
    Description:        Register callback with the new scan period
*/

#endif /* ip330H */
