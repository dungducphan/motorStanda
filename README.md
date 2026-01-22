# motorStanda

EPICS motor driver for the following Standa controller:

- **8SMC5-USB**

The driver is intended for use with Standa motorised positioners (e.g. XY stages) and is based on the XIMC library provided by Standa.

---

## Supported Systems

- Tested on **Windows 10 (64-bit)**
- EPICS **static build**
- USB connection to the controller

A non-static EPICS build should also work but has not been tested yet.

---

## Prerequisites

This driver depends on the **XIMC software package** provided by Standa.

- Download the XIMC package from:  
  http://files.xisupport.com/Software.en.html

---

## Preparing the Build

### Required Files

From the XIMC software package, copy the following files:

### 1. Library file

- **libximc.lib**
- Source location:
  ```
  XIMC_Software_package-<version>\ximc-<version>\ximc\win64
  ```
  (or `win32` for 32-bit builds)
- Destination:
  ```
  motor/lib/windows-x64-static
  ```

### 2. Header file

- **ximc.h**
- Source location:
  ```
  XIMC_Software_package-<version>\ximc-<version>\ximc
  ```
- Destination:
  ```
  motorStanda/standaApp/src
  ```

---

## Building the Driver

To build `motorStanda` as a submodule of the EPICS motor module:

1. Add the following line to the Makefile in `motor/modules`:
   ```make
   SUBMODULES += motorStanda
   ```

2. Build the motor module as usual.

---

## Runtime Requirements

The following DLLs must be available at runtime. Copy them into the IOC binary directory, for example:

```
bin/windows-x64-static
```

Required DLLs:
- `bindy.dll`
- `xiwrapper.dll`
- `libximc.dll`

---

## Usage Notes

### Controller Configuration

Before using the EPICS driver:

- Configure the stage using the vendor software **XILab**
- Load the correct stage profile
- Store the configuration in the controller’s flash memory
  (see the Standa / XIMC documentation)

### Motor Resolution (MRES)

The motor resolution must be set manually.

1. Open the profile file for your positioner
   (e.g. `8MTL20XY-LEn1-25-X.cfg`)
2. Find the parameters:
   ```
   Unit_multiplier
   Step_multiplier
   ```
3. Calculate the motor resolution:
   ```
   MRES = Unit_multiplier / Step_multiplier
   ```
   Example:
   ```
   1 / 40000 = 2.5e-5
   ```

4. Set `MRES` for the corresponding axis in:
   ```
   motorStanda/iocs/standaIOC/iocBoot/iocStanda/motor.substitutions.8SMC5
   ```

---

## To Do

- Automatically determine **MRES** and **velocity scaling** from the controller
- Add support for enabling/disabling motor power via the EPICS `closed_loop` interface
- Add support for connecting the controller via the
  **8A-SMC4-2 RS232 adapter** (e.g. via serial-to-TCP/IP server)

---

## Status

This is an **early development version** of the driver.
Basic communication and motion control are working, but the interface and scaling may change.
