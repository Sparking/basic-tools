/*
 * Copyright(C) 2020 Ruijie Network. All rights reserved.
 */
/*
 * tzset.h
 *
 * Original Author: liujiaying@ruijie.com.cn, 2020/09/21
 *
 * History
 *
 */

#ifndef __RG_SYSMON_TZSET_H__
#define __RG_SYSMON_TZSET_H__

/**
 * parse_posix_tzstring_err2str 将解析字符串的错误码转为字符串
 */
extern const char *parse_posix_tzstring_err2str(const int err);

/**
 * parse_posix_tzstring 解析posix标准的时区字符串
 *
 * str: posix标准的时区字符串
 * path: 转换成的zic文件
 * format: 重新格式化之后的字符串
 *
 * 成功返回0，失败返回错误码
 */
extern int parse_posix_tzstring(const char *str, const char *path, char *format, const size_t n);

#endif /* End of __RG_SYSMON_TZSET_H__ */
