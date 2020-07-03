#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mathlib.h"

unsigned int bits_count(unsigned int i)
{
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    i = (i + (i >> 4)) & 0x0f0f0f0f;
    i = i + (i >> 8);
    i = i + (i >> 16);

    return (i & 0x3f);
}

unsigned int great_common_divisor(const unsigned int a, const unsigned int b)
{
	unsigned int new_divisor, divisor;

    for (divisor = 1, new_divisor = 2; new_divisor <= a && new_divisor <= b;
            ++new_divisor) {
        if (a % new_divisor == 0 && b % new_divisor == 0) {
            divisor = new_divisor;
        }
    }

    return divisor;
}

int gaussian_elimination(float *restrict matrix, float *restrict res, const unsigned int rows)
{
    int ret;
    unsigned int j, i;
    unsigned int next_row, next_offset, current_offset, row_offset;
	unsigned int max_main_element_row, max_main_element_row_offset;
	float current_main_element, temp;
    const unsigned int columns = rows + 1;

    if (matrix == NULL || res == NULL || rows == 0) {
        return -1;
    }

    ret = 0;
    /* 化成三角形 */
    for (j = 0; j < rows; ++j) {
		max_main_element_row = j;
        current_offset = j * columns + j;
        max_main_element_row_offset = current_offset;
        current_main_element = fabsf(matrix[current_offset]);

        /* 1. 将列主元最大的行移到剩余行中的第一行 */
        /* 1.1 找出主元最大的一行 */
        for (next_offset = current_offset + columns, next_row = j + 1;
                next_row < rows; ++next_row, next_offset += columns) {
            if (fabsf(matrix[next_offset]) > current_main_element) {
                max_main_element_row = next_row;
                max_main_element_row_offset = next_offset;
            }
        }

        /* 1.2 交换两行的顺序 */
        if (max_main_element_row != j) {
            for (i = 0; i + j < columns; ++i) {
                /* 前面都是0, 所以不用再交换 */
                temp = matrix[current_offset + i];
                matrix[current_offset + i] = matrix[max_main_element_row_offset + i];
                matrix[max_main_element_row_offset + i] = temp;
            }
        }

        /* 2. 消主元 */
        for (next_offset = current_offset + columns, next_row = j + 1;
                next_row < rows; ++next_row, next_offset += columns) {
            if (matrix[next_offset] == 0.0f) {
                continue;
            }

            temp = matrix[current_offset] / matrix[next_offset];
            matrix[next_offset] = 0.0f;
            for (i = 1; i + j < columns; ++i) {
                matrix[next_offset + i] =
                    matrix[next_offset + i] * temp - matrix[current_offset + i];
            }
        }

    }

    /* 回代求解 */
    for (j = rows; j-- > 0;) {
        row_offset = j * columns;
        if (matrix[row_offset + j] == 0.0f) {   /* 无唯一解 */
            ret = -1;
            break;
        }

        temp = 0.0f;
        for (i = columns - 2; i > j; --i) {
            temp += res[i] * matrix[row_offset + i];
        }

        res[j] = (matrix[row_offset + rows] - temp) / matrix[row_offset + j];
    }

    return ret;
}
