#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define BUFF_SIZE            	(16)

int main(int argc, char *argv[]) {
    unsigned long phys_addr;
    size_t length;
    char *filename;
    int mem_fd, out_fd;
    char buffer[BUFF_SIZE];
    ssize_t bytes_read, bytes_written;
    size_t total_read = 0;

    // Проверка аргументов командной строки
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <physical_address> <length> <output_file>\n", argv[0]);
        fprintf(stderr, "Example: %s 0x10000000 1024 output.bin\n", argv[0]);
        return 1;
    }

    // Парсинг аргументов
    errno = 0;
    phys_addr = strtoul(argv[1], NULL, 0);
    if (errno) {
        perror("Invalid physical address");
        return 1;
    }

    length = strtoul(argv[2], NULL, 0);
    if (errno) {
        perror("Invalid length");
        return 1;
    }

    filename = argv[3];

    // Открытие устройства /proc/phys_mem
    mem_fd = open("/proc/riscv_mem", O_RDONLY);
    if (mem_fd < 0) {
        perror("Failed to open /proc/riscv_mem");
        return 1;
    }

    // Открытие выходного файла
    out_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        perror("Failed to open output file");
        close(mem_fd);
        return 1;
    }

    // Установка начальной позиции для чтения
    if (lseek(mem_fd, phys_addr, SEEK_SET) == -1) {
        perror("Failed to set offset with lseek");
        close(mem_fd);
        close(out_fd);
        return 1;
    }

    // Чтение данных блоками по 32 байта
    while (total_read < length) {
        size_t to_read = (length - total_read) > BUFF_SIZE ? BUFF_SIZE : (length - total_read);
        memset(buffer,0xff,sizeof(buffer));
        bytes_read = read(mem_fd, buffer, to_read);
        if (bytes_read < 0) {
            perror("Failed to read from /proc/riscv_mem");
            close(mem_fd);
            close(out_fd);
            return 1;
        }
        if (bytes_read == 0) {
            fprintf(stderr, "Reached end of device or no more data to read\n");
            break;
        }

        // Запись в выходной файл
        bytes_written = write(out_fd, buffer, bytes_read);
        fdatasync(out_fd);
        if (bytes_written != bytes_read) {
            perror("Failed to write to output file");
            close(mem_fd);
            close(out_fd);
            return 1;
        }

        total_read += bytes_read;
        phys_addr+=bytes_read;
        printf("%08lx\r",phys_addr);
    }

    printf("\nSuccessfully read %zu bytes from physical address 0x%lx and wrote to %s\n",
           total_read, phys_addr, filename);

    // Закрытие файлов
    close(mem_fd);
    close(out_fd);
    return 0;
}