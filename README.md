# LPCTools
**LPCTools** is an interface to NXP LPC Microcontrollers ISP (In-System
Programming) serial interface.

It is split in two programs: lpcisp and lpcprog

## lpcisp:
This tool gives access to each of the useful isp commands on LPC
devices. It does not provide wrappers for flashing a device.

## lpcprog:
This tool does not give access to each isp command, instead it
provides wrappers for flashing a device.

Both programs were originally written by Nathael Pajani
<nathael.pajani@nathael.net> because existing programs were published
under non-free licenses, did not allow commercial use, or did not
provide source code.

## lpc_binary_check:
This third tool is an additional helper program created to change the
user code and check that the CRP protection is not enabled in a binary
image so that it can be uploaded to a target with different tools.

These programs are released under the terms of the GNU GPLv3 license
as can be found on the GNU website : <http://www.gnu.org/licenses/>
or in the included LICENSE file.

# Update RF-Sub 1GHz module
The new RF-Sub 1GHz module embeds a LPC1224 micro-controller with a new ID, 
so you need to add a new line in your `lpctools_parts.def` below the previous 
LPC1224 definition. 
The new line will look like this :
```
0x3642C02B, LPC1224FBD48/121,   0x00000000, 0x8000,  8,    0x04,    0x10000000, 0x1000, 0x800, 0x400,   1
```

If you installed lpctools globally in your system, look for the 
definitions file at `/etc/lpctools_parts.def`.