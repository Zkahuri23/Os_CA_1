#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int is_digit(char c) {
    return c >= '0' && c <= '9';
}

void reverse(char *str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

int itoa(long long num, char *str) {
    int i = 0;
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    
    while (num != 0) {
        int rem = num % 10;
        str[i++] = rem + '0';
        num = num / 10;
    }
    
    reverse(str, i);
    str[i] = '\0';
    return i;
}


int main(int argc, char *argv[]) {
    if (argc <= 1) {
        printf(2, "Usage: find_sum <string1> [string2] ...\n");
        exit();
    }

    long long total_sum = 0;
    for (int j = 1; j < argc; j++) {
        char *input_str = argv[j];
        int i = 0;
        while (input_str[i] != '\0') {
            if (is_digit(input_str[i])) {
                int current_num = 0;
                while (is_digit(input_str[i])) {
                    current_num = current_num * 10 + (input_str[i] - '0');
                    i++;
                }
                total_sum += current_num;
            } else {
                i++;
            }
        }
    }

 
    char result_buf[50];
    int len = itoa(total_sum, result_buf);
    result_buf[len] = '\n'; 
    len++;

    int fd = open("result.txt", O_CREATE | O_WRONLY);
    if (fd < 0) {
        printf(2, "find_sum: cannot open result.txt\n");
        exit();
    }
    if(write(fd, result_buf, len) != len){
        printf(2, "find_sum: error writing to result.txt\n");
    }
    
    close(fd);

    exit();
}