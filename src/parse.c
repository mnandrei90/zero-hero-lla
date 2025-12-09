#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "common.h"
#include "parse.h"

int create_db_header(struct dbheader_t **headerOut) {
    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        printf("Failed to allocate memory for file header.\n");
        return STATUS_ERROR;
    }

    header->version = 0x1;
    header->count = 0;
    header->magic = HEADER_MAGIC;
    header->filesize = sizeof(struct dbheader_t);

    *headerOut = header;

    return STATUS_SUCCESS;
}

int validate_db_header(int fd, struct dbheader_t **headerOut) {
    if (fd < 0) {
        printf("Invalid file descriptor\n");
        return STATUS_ERROR;
    }

    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        printf("Failed to allocate memory for file header.\n");
        return STATUS_ERROR;
    }

    if (read(fd, header, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
        perror("read");
        free(header);

        return STATUS_ERROR;
    }

    header->version = ntohs(header->version);
    header->count = ntohs(header->count);
    header->magic = ntohl(header->magic);
    header->filesize = ntohl(header->filesize);

    if (header->version != 1) {
        printf("Improper header version\n");
        free(header);
        return STATUS_ERROR;
    }

    if (header->magic != HEADER_MAGIC) {
        printf("Improper header magic\n");
        free(header);
        return STATUS_ERROR;
    }

    struct stat dbstat = {0};
    fstat(fd, &dbstat);
    if (header->filesize != dbstat.st_size) {
        printf("Corrupted database file\n");
        free(header);
        return STATUS_ERROR;
    }

    *headerOut = header;

    return STATUS_SUCCESS;
}

int read_employees(int fd, struct dbheader_t * header, struct employee_t **employeesOut) {
    if (fd < 0) {
        printf("Invalid file descriptor\n");
        return STATUS_ERROR;
    }

    int count = header->count;

    struct employee_t *employees = calloc(count, sizeof(struct employee_t));
    if (employees == NULL) {
        printf("Failed to allocate memory for employees.\n");
        return STATUS_ERROR;
    }

    // after reading the header, the pointer in the file should be right after the header data location
    read(fd, employees, count * sizeof(struct employee_t));

    int i = 0;
    for (; i < count; i++) {
        employees[i].hours = ntohl(employees[i].hours);
    }

    *employeesOut = employees;

    return STATUS_SUCCESS;
}

int add_employee(struct dbheader_t * header, struct employee_t **employees, char *addstring) {
    if (header == NULL
     || employees == NULL
     || *employees == NULL
     || addstring == NULL) {
        return STATUS_ERROR;
    }

    char *name = strtok(addstring, ",");
    if (name == NULL) return STATUS_ERROR;

    char *addr = strtok(NULL, ",");
    if (addr == NULL) return STATUS_ERROR;

    char *hours = strtok(NULL, ",");
    if (hours == NULL) return STATUS_ERROR;

    struct employee_t *e = *employees;
    e = realloc(e, sizeof(struct employee_t) * header->count+1);
    if (e == NULL) {
        return STATUS_ERROR;
    }

    header->count++;

    strncpy(e[header->count-1].name, name, sizeof(e[header->count-1].name)-1);
    strncpy(e[header->count-1].address, addr, sizeof(e[header->count-1].address)-1);
    e[header->count-1].hours = atoi(hours);

    *employees = e;

    return STATUS_SUCCESS;
}

int output_file(int fd, struct dbheader_t * header, struct employee_t *employees) {
    if (fd < 0) {
        printf("Invalid file descriptor\n");
        return STATUS_ERROR;
    }

    int employeesCount = header->count;
    int newFileSize = sizeof(struct dbheader_t) + sizeof(struct employee_t) * employeesCount;

    header->magic = htonl(header->magic);
    header->filesize = htonl(newFileSize);
    header->count = htons(header->count);
    header->version = htons(header->version);

    lseek(fd, SEEK_SET, 0);

    write(fd, header, sizeof(struct dbheader_t));

    for (int i = 0; i < employeesCount; i++) {
        employees[i].hours = htonl(employees[i].hours);
        write(fd, &employees[i], sizeof(struct employee_t));
    }

    return STATUS_SUCCESS;
}
