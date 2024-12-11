ENTRY(_start) /* 指定shell程序的入口地址 _start, 该地址在 applib/crt0.S 中定义 */
SECTIONS
{
    /* 此处以0x81000000虚拟地址开始, 防止与first_task起始的虚拟地址0x80000000混淆 */
	. = 0x81000000;
	.text :
	{
		*(*.text)
	}

	.rodata :
	{
		*(*.rodata)
	}

	.data :
	{
		*(*.data)
	}

	.bss :
	{
		__bss_start__ = .;
		*(*.bss)
    	__bss_end__ = . ;
	}
}
