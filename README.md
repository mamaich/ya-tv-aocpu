# ya-tv-aocpu
Работа с AOCPU (RISC-V) на Яндекс ТВ Станции (Amlogic T3)  

Код для статьи https://dzen.ru/a/aKoJBqdQgCEgcLKw  

Компиляция:  
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-  

Использование:  
insmod risc_access.ko  
read_riscv_mem 0x10000000 0x0x1000000 dump.bin  
