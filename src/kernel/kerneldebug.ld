OUTPUT_ARCH(i386:x86-64);
ENTRY(kentry)

SECTIONS
{
  . = 0xc0010000;
  .text : { *(.text) }
  .data : { *(.data) }
  .bss : { *(.bss) }
}
