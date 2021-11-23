#include <linux/hidraw.h>
#include <getopt.h>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

#define cur_buf(report_buf) (report_buf->buf[report_buf->index])
#define last_buf(report_buf) (report_buf->buf[(report_buf->index+1)%2])
struct report_buf {
    char buf[2][256];
    int index;
    int size;
};

char optstring[] = "p:d:n:f:";
struct option options[] = {
    {"mode", required_argument, 0, 'm'},
    {"hidraw", required_argument, 0, 'p'},
    {"diff", required_argument, 0, 'd'},
    {"interval", required_argument, 0, 'n'},
    {"format", required_argument, 0, 'f'},
    {0, 0, 0, 0}
};

int is_exit = 0;

int sprint_report_bin(char* output_buf, struct report_buf* buf, int diff) {
    int cursor = 0;
    int cur_line = 0;
    for (int i = 0; i < buf->size; ++i) {
        ++cur_line;
        if (cur_buf(buf)[i] - last_buf(buf)[i] > diff) {
            cursor += sprintf(output_buf+cursor, 
                "\e[41;37m"BYTE_TO_BINARY_PATTERN"\e[0m ", BYTE_TO_BINARY(cur_buf(buf)[i]));
        } else if (cur_buf(buf)[i] - last_buf(buf)[i] < -diff) {
            cursor += sprintf(output_buf+cursor, 
                "\e[42;37m"BYTE_TO_BINARY_PATTERN"\e[0m ", BYTE_TO_BINARY(cur_buf(buf)[i]));
        } else {
            cursor += sprintf(output_buf+cursor, 
                BYTE_TO_BINARY_PATTERN" ", BYTE_TO_BINARY(cur_buf(buf)[i]));
        }
        if (cur_line % 16 == 0) {
            cursor += sprintf(output_buf+cursor, "\n");
            cur_line = 0;
        }
    }
    return cursor;
}

int sprint_report_hex(char* output_buf, struct report_buf* buf, int diff) {
    int cursor = 0;
    int cur_line = 0;
    for (int i = 0; i < buf->size; ++i) {
        ++cur_line;
        if (cur_buf(buf)[i] - last_buf(buf)[i] > diff) {
            cursor += sprintf(output_buf+cursor, "\e[41;37m%02hhx\e[0m ", cur_buf(buf)[i]);
        } else if (cur_buf(buf)[i] - last_buf(buf)[i] < -diff) {
            cursor += sprintf(output_buf+cursor, "\e[42;37m%02hhx\e[0m ", cur_buf(buf)[i]);
        } else {
            cursor += sprintf(output_buf+cursor, "%02hhx ", cur_buf(buf)[i]);
        }
        if (cur_line % 16 == 0) {
            cursor += sprintf(output_buf+cursor, "\n");
            cur_line = 0;
        }
    }
    return cursor;
}

int sprint_report_dec(char* output_buf, struct report_buf* buf, int diff) {
    int cursor = 0;
    int cur_line = 0;
    for (int i = 0; i < buf->size; ++i) {
        ++cur_line;
        if (cur_buf(buf)[i] - last_buf(buf)[i] > diff) {
            cursor += sprintf(output_buf+cursor, "\e[41;37m%04hhd\e[0m ", cur_buf(buf)[i]);
        } else if (cur_buf(buf)[i] - last_buf(buf)[i] < -diff) {
            cursor += sprintf(output_buf+cursor, "\e[42;37m%04hhd\e[0m ", cur_buf(buf)[i]);
        } else {
            cursor += sprintf(output_buf+cursor, "%04hhd ", cur_buf(buf)[i]);
        }
        if (cur_line % 16 == 0) {
            cursor += sprintf(output_buf+cursor, "\n");
            cur_line = 0;
        }
    }
    return cursor;
}

int sprint_report_u16(char* output_buf, struct report_buf* buf, int diff) {
    int cursor = 0;
    int cur_line = 0;
    for (int i = 0; i < buf->size; i+=2) {
        ++cur_line;
        if (cur_buf(buf)[i] - last_buf(buf)[i] > diff) {
            cursor += sprintf(output_buf+cursor, "\e[41;37m%05hu\e[0m ", *(__u16*)&cur_buf(buf)[i]);
        } else if (cur_buf(buf)[i] - last_buf(buf)[i] < -diff) {
            cursor += sprintf(output_buf+cursor, "\e[42;37m%05hu\e[0m ", *(__u16*)&cur_buf(buf)[i]);
        } else {
            cursor += sprintf(output_buf+cursor, "%05hu ", *(__u16*)&cur_buf(buf)[i]);
        }
        if (cur_line % 8 == 0) {
            cursor += sprintf(output_buf+cursor, "\n");
            cur_line = 0;
        }
    }
    return cursor;
}
int sprint_report_s16(char* output_buf, struct report_buf* buf, int diff) {
    int cursor = 0;
    int cur_line = 0;
    for (int i = 0; i < buf->size; i+=2) {
        ++cur_line;
        if (cur_buf(buf)[i] - last_buf(buf)[i] > diff) {
            cursor += sprintf(output_buf+cursor, "\e[41;37m%6hd\e[0m ", *(__u16*)&cur_buf(buf)[i]);
        } else if (cur_buf(buf)[i] - last_buf(buf)[i] < -diff) {
            cursor += sprintf(output_buf+cursor, "\e[42;37m%6hd\e[0m ", *(__u16*)&cur_buf(buf)[i]);
        } else {
            cursor += sprintf(output_buf+cursor, "%6hd ", *(__u16*)&cur_buf(buf)[i]);
        }
        if (cur_line % 8 == 0) {
            cursor += sprintf(output_buf+cursor, "\n");
            cur_line = 0;
        }
    }
    return cursor;
}

int sprint_info(char* output_buf, char* name, struct hidraw_devinfo* info) {
    int cursor = 0;
    cursor += sprintf(output_buf+cursor, "NAME: %s\n", name);
    cursor += sprintf(output_buf+cursor, "INFO:\n");
    cursor += sprintf(output_buf+cursor, "\tvender: \t0x%04hx\n", info->vendor);
    cursor += sprintf(output_buf+cursor, "\tproduct: \t0x%04hx\n", info->product);
    cursor += sprintf(output_buf+cursor, "\n");
    return cursor;
}

void set_exit_flag(int sig) {
    is_exit = 1;
}

int main(int argc, char** argv) {
    int fd;
    int diff = 0;
    int cursor=0;
    int res, rep4_size, rep5_size;
    char input_buf[256];
    char output_buf[1024];
    struct report_buf *buf4, *buf5;
    char name[256];
    struct hidraw_devinfo info;
    double interval = 0.1;
    char* device = "/dev/hidraw0";
    
    int (*sprint_report)(char*, struct report_buf*, int) = sprint_report_hex;
    
    while(1) {
        int c = getopt_long(argc, argv, optstring, options, NULL);

        if (c == -1) {
            break;
        }
        switch (c) {
            case 'p':
                device = optarg;
                break;
            case 'd':
                diff = atoi(optarg);
                break;
            case 'n':
                interval = atof(optarg);
                break;
            case 'f':
                if (strcmp("bin", optarg) == 0)
                    sprint_report = sprint_report_bin;
                if (strcmp("dec", optarg) == 0)
                    sprint_report = sprint_report_dec;
                if (strcmp("u16", optarg) == 0)
                    sprint_report = sprint_report_u16;
                if (strcmp("s16", optarg) == 0)
                    sprint_report = sprint_report_s16;

                break;
            default:
                break;
        }
    }
    
    signal(SIGINT, set_exit_flag);

    fd = open(device, O_RDONLY);

    if (fd < 0) {
        perror("Unable to open this device");
        return 1;
    }
    
    buf4 = malloc(sizeof(struct report_buf));
    buf5 = malloc(sizeof(struct report_buf));
    
    memset(input_buf, 0, sizeof(input_buf));
    memset(output_buf, 0, sizeof(output_buf));
    memset(buf4, 0, sizeof(struct report_buf));
    memset(buf5, 0, sizeof(struct report_buf));
    memset(name, 0, sizeof(name));
    memset(&info, 0, sizeof(info));
    
    res = ioctl(fd, HIDIOCGRAWNAME(256), name);
    if (res < 0) {
        perror("HIDIOCGRAWNAME");
    }

    res = ioctl(fd, HIDIOCGRAWINFO, &info);
    if (res < 0) {
        perror("HIDIOCGRAWINFO");
        return 1;
    }
    

    struct timeval last, cur;
    memset(&cur, 0, sizeof(struct timeval));
    memset(&last, 0, sizeof(struct timeval));

    while (!is_exit) {
        res = read(fd, input_buf, sizeof(input_buf));
        if (res < 0) {
            perror("read error");
            return 1;
        }

        gettimeofday(&cur, NULL);
        if ((cur.tv_sec - last.tv_sec + (cur.tv_usec - last.tv_usec)/1000000.0) < interval)
            continue;
        
        if (input_buf[0] == 4) {
            buf4->size = res;
            buf4->index = (buf4->index + 1) % 2;
            memcpy(cur_buf(buf4), input_buf, sizeof(cur_buf(buf4)));
        } else if (input_buf[0] == 5) {
            buf5->size = res;
            buf5->index = (buf5->index + 1) % 2;
            memcpy(cur_buf(buf5), input_buf, sizeof(cur_buf(buf5)));
        }
        
        // if (cur_buf(buf4)[0] == 0 || cur_buf(buf5)[0] == 0) {
        //     continue;
        // }
        
        
        puts("\033[2J\033[1;1H");
        cursor += sprint_info(output_buf + cursor, name, &info);
        cursor += sprintf(output_buf+cursor, "Report ID: 4\n");
        cursor += sprint_report(output_buf + cursor, buf4, diff);
        cursor += sprintf(output_buf+cursor, "\n");
        cursor += sprintf(output_buf+cursor, "Report ID: 5\n");
        cursor += sprint_report(output_buf + cursor, buf5, diff);
        puts(output_buf);
        fflush(stdout);
        
        cur_buf(buf4)[0] = 0;
        cur_buf(buf5)[0] = 0;
        cursor = 0;
        last = cur;
    }
    
    puts("\nexiting");
    free(buf4);
    free(buf5);
    close(fd);
    return 0;
}