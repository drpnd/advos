# advos

advos had been developed for my advent calendar 2018 project.
See https://ja.tech.jar.jp/ac/2018/day00.html (in Japanese) for the detailed description.

## Memory Management

### Memory Zones

* ZONE_DMA: 0x00000000 -- 0x01000000 (0--16 MiB)
* ZONE_KERNEL: 0x01000000 -- 0x04000000 (16--64 MiB)
* ZONE_NUMA_AWARE: 0x04000000 -- (64-- MiB)

### Memory Map

| Start      | End        | Description |
| :--------- | :--------- | :---------- |
| `00000500` | `00007bff` | Stack of boot loader |
| `00007c00` | `00007dff` | MBR |
| `00007e00` | `00007fff` | Reserved |
| `00008000` | `00008fff` | Boot information (e.g., drive number, system memory map) |
| `00009000` | `0000cfff` | Boot monitor (16 KiB) |
| `0000d000` | `0000ffff` | Reserved for boot monitor |
| `00010000` | `0002ffff` | Kernel (up to 128 KiB) |
| `00030000` | `0005ffff` | Reserved |
| `00060000` | `00067fff` | Per-core data (128-byte * 256) |
| `00068000` | `00068e00` | Global variables |
|  00068f00  |  00068fff  | Global variables for assembly |
| `00069000` | `0006ffff` | MOVED: Kernel's base page table |
| `00070000` | `00073fff` | trampoline (16 KiB) |
| `00074000` | `00075fff` | GDT (8 KiB) |
| `00076000` | `00077fff` | IDT (8 KiB) |
| `00078000` | `00078fff` | TSS for BSP |
| `00079000` | `0007ffff` | Moved: Page table for long mode (Must be >= 24 KiB = 6 * 4 KiB) |

