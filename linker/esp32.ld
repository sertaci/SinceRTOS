ENTRY(call_start_cpu0)

MEMORY
{
  dram_seg (RW) : ORIGIN = 0x3FFAE430, LENGTH = 0x51BD0 
  iram_seg (RX) : ORIGIN = 0x40080400, LENGTH = 0x1FC00 
}

SECTIONS
{
    .vectors : ALIGN(256)
    {
        _vecbase_start = .;
        KEEP(*(.vectors))
        KEEP(*(.vectors.*))
    } > iram_seg

    .text : ALIGN(4)
    {
        *(.literal)
        *(.literal.*)

        *(.text)
        *(.text.*)
    } > iram_seg

    .data : ALIGN(4)
    {
        *(.data)
        *(.data.*)
        
        *(.rodata)
        *(.rodata.*)
    } > dram_seg

    .bss : ALIGN(4)
    {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        _bss_end = .;
    } > dram_seg
}