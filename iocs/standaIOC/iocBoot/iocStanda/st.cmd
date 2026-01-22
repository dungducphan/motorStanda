#!../../bin/windows-x64-static/standa

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/standa.dbd"
standa_registerRecordDeviceDriver pdbbase

cd "${TOP}/iocBoot/${IOC}"

## motorUtil (allstop & alldone)
dbLoadRecords("$(MOTOR)/db/motorUtil.db", "P=standa:")

## 
< motor.cmd.8SMC5

iocInit

## motorUtil (allstop & alldone)
motorUtilInit("standa:")

# Boot complete
