# vita-physmem-tool
## Tool for Vita Physical Memory dumps
<p></p>
This is a simple utility for printing the MMU physical to virtual address table from a physical memory dump.
It also can generate a contiguous virtual address space file that is useful for cleanly analyzing what the
memory really looks like to the processor.


### Parameters
```
Vita Physical Memory Tool 3.0 - by Proxima (Build Mar  6 2023 17:00:45)
-----------------------------------------------------------------------

  Usage: vita-physmem-tool.exe [options]

options:
  -i filename   [REQUIRED] specify the input physical dump file
  -0 hex        [REQUIRED] physical file offset for TTBR0
  -1 hex        [REQUIRED] physical file offset for TTBR1
  -c dec        [REQUIRED] decimal value for TTBCR
  -b hex        [REQUIRED] base address of physical device memory. (0x40300000)
  -p            print dump of MMU tables
  -d hex        start virtual address of memory you want to dump
  -e hex        end virtual address of memory you want to dump
  -f filename   output filename prefix for saving the memory range
  ```

Usage by example:

`release\vita-physmem-tool.exe -i tz-physical-0930.bin -0 0x40044000 -1 0x40048000 -b 0x40000000 -c 1 -p`

In the above example, the dump was map of the physical memory space from 0x40000000 to 0x40300000. You will
need to find the location of the TTBR0 and TTBR1 locations. Additionally you will need to know the TTBCR value
as well. The values are as if they were in actual memory and not the file offsets. Memory starts at 0x40000000
for TZ. In this case we found the TTBR0 table at files offset +0x44000. This gives the TTBR0 value of 0x40044000.
TTBR1 is usually 0x4000 bytes later at memory 0x40048000 or +0x48000 in the file. Since the TTBR0 location was
0x4000 aligned and not 0x8000, the TTBCR value was 1 rather than the normal 2 that nearly ever example has.

`release\vita-physmem-tool.exe -i tz-physical-1500.bin -0 0x40108000 -1 0x4010c000 -b 0x40000000 -c 2 -p`

This is a normal TZ example for nearly all modern firmwares. The base again is 0x40000000 and the TTBR0 location
is at +0x108000 in the physical dump of memory. This gives a value of 0x40108000. TTBR1 is again 0x4000 bytes
later. This example has the standard TTBCR value of 2.

`release\vita-physmem-tool.exe -i kernel-physical-1691.bin -0 0x40308000 -1 0x4030c000 -b 0x40300000 -c 2 -p`

This is the normal kernel memory physical example for the majority of available firmwares. In this case the start
of kernel memory is at 0x40300000. Typically in the kernel, the TTBR0 is at +0x8000 from the base and TTBR1 is 0x4000
bytes later at 0xC000. TTBCR is generally 2 to support the 0x8000 alignment of TTBR0. Be cautious with newer firmware
physical dumps, the base address of kernel changed to 0x40200000 at one point.

***

This tool can also generate a contiguous virtual address mapped view of the physical memory. This is very useful for analyzing a real kernel or TZ since it shows all the pages in their proper location as the running CPU sees them with MMU turned on. The `-d` and `-e` parameters are used to indicate the VA address start and end range for the output file (`-f`).

`release\vita-physmem-tool.exe -i tz-physical-1500.bin -0 0x40108000 -1 0x4010c000 -b 0x40000000 -c 2 -p -d 0x0 -e 0x800000 -f tz-va-1500`

This example will generate a file called tz-va-1500-0x0-0x800000.bin that has the actual virtual memory from 0x0 thru 0x800000.



  