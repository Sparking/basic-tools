#ifndef _MATHLIB_H_
#define _MATHLIB_H_

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif
#include <math.h>

#define M_PIf               acosf(-1.0f)

__attribute__((always_inline)) static inline float degrees2rad(const float degrees)
{
    return degrees * (M_PIf / 180.0f);
}

__attribute__((always_inline)) static inline float rad2degrees(const float rad)
{
    return rad * (180.0f / M_PIf);
}

/**
 * @brief find_max_common_divisor 寻找出两个整数的最大公约数
 */
extern unsigned int find_max_common_divisor(const unsigned int a, const unsigned int b);

/**
 * @brief bits_count 计算出一个整数中1的位数
 */
extern unsigned int bits_count(unsigned int i);

/**
 * @brief great_common_divisor 求解最大公约数
 */
extern unsigned int great_common_divisor(const unsigned int a, const unsigned int b);

/**
 * @brief gaussian_elimination 高斯消元法求解
 * @param matrix 矩阵, 增广矩阵[a * x | b], 其中 [a * x]是个rows*rows行的方阵, [b]是个长度位rows的列向量
 * @param res 解的存放位置, 是个长度为rows的数组
 * @param rows 增广矩阵的行数
 * @return 方程有解返回，如果没有解，返回-1
 */
extern int gaussian_elimination(float *restrict matrix, float *restrict res, const unsigned int rows);

#ifdef __cplusplus
}
#endif

#endif /* _MATHLIB_H_ */
