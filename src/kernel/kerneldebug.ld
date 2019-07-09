OUTPUT_FORMAT("binary","binary","binary");
OUTPUT_ARCH(i386:x86-64);
ENTRY(kstart64)

SECTIONS
{
  . = 0xc0010000;
  .text : { *(.text) }
  .data : { *(.data) }
  .bss : { *(.bss) }
}
