OUTPUT_FORMAT("binary","binary","binary");
OUTPUT_ARCH(i386:x86-64);
ENTRY(_entry)
SEARCH_DIR(../../)
STARTUP(lib/crt0.o)
INPUT(lib/arch/x86_64/libc.o lib/arch/x86_64/libcasm.o lib/arch/x86_64/libadvos.o)

SECTIONS
{
  . = 0x80000000;
  .text : { *(.text) }
  .data : { *(.data) }
  .bss : { *(.bss) }
}
