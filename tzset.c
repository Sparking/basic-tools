/*
 * Copyright(C) 2020 Ruijie Network. All rights reserved.
 */
/*
 * tzset.c
 *
 * Original Author: liujiaying@ruijie.com.cn, 2020/09/21
 *
 * History
 *
 */

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define TZNAME_SIZE             (17 + 1)                    /* BUG 662591, 时区长度不允许超过17个字符 */
#define WEEKDAY2MONTHDAY(w)     (((w) - 1) * 7 + 1)

/* 时区字段设置标记 */
#define F_STDZONE_SET           (1 << 0)
#define F_DSTZONE_SET           (1 << 1)
#define F_DST_START_SET         (1 << 2)
#define F_DST_END_SET           (1 << 3)

typedef struct {
    signed short   sign;    /* +1, -1 */
    unsigned short h;
    unsigned short m;
    unsigned short s;
} tz_offset_t;

typedef struct {
    /*
     * J0: n (0 <= n <= 365, Feb 29 is count in leap year)
     * J1: Jn (1 <= n <= 365)
     * M: Mm.w.d[/offset] (m: month, w: week, d: day)
     */
    enum {J0, J1, M } type;
    unsigned short m;       /* Month (only M -> m) */
    unsigned short n;       /* Week (only M -> w)*/
    unsigned short d;       /* Day (M -> d, J0 -> n, J1 -> n) */
    tz_offset_t offset;     /* time offset, only for M */
} tz_rule_t;

typedef struct {
    char name[TZNAME_SIZE];
    tz_offset_t offset;
} tz_zone_t;

typedef struct {
    tz_zone_t std;
    tz_zone_t dst;
    tz_rule_t start;
    tz_rule_t end;
    unsigned short mask;
} tz_info_t;

enum {
    TZ_ERROR_NONE                  = 0,
    TZ_ERROR_INTERNAL_ERROR        = -1,
    TZ_ERROR_INVALID_STD_NAME      = -2,
    TZ_ERROR_INVALID_STD_OFFSET    = -3,
    TZ_ERROR_INVALID_DST_NAME      = -4,
    TZ_ERROR_INVALID_DST_OFFSET    = -5,
    TZ_ERRPR_DST_TIME_OUT_OF_RANGE = -6,
    TZ_ERRPR_DST_TIME_CONFLICT     = -7,
    TZ_ERRPR_INVALID_FUNC_PARAM    = -8,
    TZ_ERROR_WRONG_SYNTAX          = -9,
    TZ_ERROR_EMPTY_DST_INFO        = -10,
};

static const struct {
    const int id;
    const char *const reason;
} errcode[] = {
    {TZ_ERROR_NONE,                  "\0"},
    {TZ_ERROR_INTERNAL_ERROR,        "internal error"},
    {TZ_ERROR_INVALID_STD_NAME,      "invalid standard zone name"},
    {TZ_ERROR_INVALID_STD_OFFSET,    "invalid standard zone offset(may out of -14 to +12)"},
    {TZ_ERROR_INVALID_DST_NAME,      "invalid daylight saving time name"},
    {TZ_ERROR_INVALID_DST_OFFSET,    "invalid daylight saving time offset(may out of -14 to +12)"},
    {TZ_ERRPR_DST_TIME_OUT_OF_RANGE, "daylight saving time setting out of range"},
    {TZ_ERRPR_DST_TIME_CONFLICT,     "daylight saving time setting conflict"},
    {TZ_ERRPR_INVALID_FUNC_PARAM,    "invalid parameters"},
    {TZ_ERROR_WRONG_SYNTAX,          "syntax error"},
    {TZ_ERROR_EMPTY_DST_INFO,        "no daylight saving time name"}
};

const char *parse_posix_tzstring_err2str(const int err)
{
    int index;

    if (err > 0 || err <= -(sizeof(errcode) / sizeof(*errcode))) {
        return "unknown cause";
    } else {
        index = -err;
    }

    return errcode[index].reason;
}

static inline char sign_char(const signed short sign)
{
    return (sign < 0) ? '-' : '+';
}

/* Leap days are not counted */
static const unsigned short julian_days[13] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
};

static const unsigned short leap_days[13] = {
    0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366
};

static int get_month_in_tables(const unsigned short *tables, const unsigned short d)
{
    int m;

    for (m = 1; m < 13; ++m) {
        if (d <= tables[m]) {
            break;
        }
    }

    return m - 1;
}

static inline int get_month_in_julian_days(const unsigned short d)
{
    return get_month_in_tables(julian_days, d);
}

static inline int get_month_in_leap_days(const unsigned short d)
{
    return get_month_in_tables(leap_days, d);
}

static void offset_to_hhmmss(tz_offset_t *t, int offset)
{
    t->sign = offset > 0 ? 1 : -1;
    offset = abs(offset);
    t->h = offset / (60 * 60);
    t->m = (offset / 60) % 60;
    t->s = offset % 60;
}

static int hhmmss_to_offset(const tz_offset_t *t)
{
    int h, m, s;

    h = t->h;
    m = t->m;
    s = t->s;

    return t->sign * (s + (m + h * 60) * 60);
}

/* 检查时区偏移值是否确 */
static bool check_tzzone_offset_right(const tz_offset_t *off)
{
    int offset;

    offset = hhmmss_to_offset(off);
    if ((offset > (14 * 60 * 60) || offset < -(12 * 60 * 60))
            || ((off->m > 59 || off->s > 59))) {
        return false;
    }

    return true;
}

/* 检查夏时令偏移值是否正确 */
static bool check_tzrule_offset(const tz_rule_t *tzr)
{
    const tz_offset_t *off;

    off = &tzr->offset;

    return off->sign > 0 && off->h <= 23 && off->m <= 59 && off->s <= 59;
}

static void tz_offset_sub(tz_offset_t *res, const tz_offset_t *a, const tz_offset_t *b)
{
    offset_to_hhmmss(res, hhmmss_to_offset(a) - hhmmss_to_offset(b));
}

static inline int day2sec(const int a)
{
    return a * (24 * 60 * 60);
}

static inline int limit365(const int a)
{
    return a < 365 ? a : (a % 365);
}

static void tz_rule_to_julian_range(const tz_rule_t *r, int *start, int *end)
{
    int a, b;

    switch (r->type) {
    case J0:
        a = r->d + 1;
        b = a + (r->d >= 58);
        if (a >= 365) {
            a = 0;
        }

        if (b >= 365) {
            b = 0;
        }
        break;
    case M:
        a = get_month_in_julian_days(r->m) + WEEKDAY2MONTHDAY(r->n);
        b = a + 7;
        break;
    case J1: /* FALL THROUGH */
    default:
        a = r->d;
        b = r->d;
        break;
    }

    *start = limit365(a);
    *end = limit365(b);
}

static int tz_rule_delta(const tz_rule_t *a, const tz_rule_t *b)
{
    int delta[2];
    int offset[2];
    int range[2][2];

    offset[0] = hhmmss_to_offset(&a->offset);
    offset[1] = hhmmss_to_offset(&b->offset);
    tz_rule_to_julian_range(a, range[0], range[0] + 1);
    tz_rule_to_julian_range(b, range[1], range[1] + 1);
    if (a->type == b->type) {
        if (a->type == M) {
            range[0][0] = limit365(range[0][0] + a->d);
            range[1][0] = limit365(range[1][0] + b->d);
        }

        return abs(day2sec(range[0][0]) + offset[0] - day2sec(range[1][0]) - offset[1]);
    }

    delta[0] = abs(range[0][0] - range[1][1]);
    delta[1] = abs(range[0][1] - range[1][0]);
    if (delta[0] < delta[1]) {
        offset[0] += day2sec(range[0][0]);
        offset[1] += day2sec(range[1][1]);
    } else if (delta[0] > delta[1]) {
        offset[0] += day2sec(range[0][1]);
        offset[1] += day2sec(range[1][0]);
    } else {
        offset[0] += day2sec(range[0][0]);
        offset[1] += day2sec(range[1][0]);
    }

    return abs(offset[0] - offset[1]);
}

/* 对比start，end是否会存在冲突 */
static bool check_tzrule_right(const tz_info_t *tz)
{
    int delta[2];
    tz_offset_t off;

    tz_offset_sub(&off, &tz->dst.offset, &tz->std.offset);
    delta[0] = tz_rule_delta(&tz->start, &tz->end);
    delta[1] = abs(hhmmss_to_offset(&off));
    if (delta[0] <= delta[1]) {
        return false;
    }

    return true;
}

static bool parse_tzname(tz_info_t *info, const char **tzp, int whichrule)
{
    int len;
    const char *p;
    const char *start;
    tz_zone_t *zone;

    p = start = *tzp;
    while (('a' <= *p && *p <= 'z') || ('A' <= *p && *p <= 'Z')) {
        ++p;
    }

    if ((len = p - start) < 3) {
        p = *tzp;
        if (*p++ != '<') {
            return false;
        }

        start = p;
        while (('a' <= *p && *p <= 'z') || ('A' <= *p && *p <= 'Z') || ('0' <= *p && *p <= '9')
                || *p == '+' || *p == '-') {
            ++p;
        }

        len = p - start;
        if (*p++ != '>' || len < 3) {
            return false;
        }
    }
    zone = (whichrule == 0) ? &info->std : &info->dst;
    memset(zone, 0, sizeof(*zone));
    snprintf(zone->name, sizeof(zone->name), "%.*s", len, start);
    *tzp = p;

    return true;
}

static bool parse_offset(tz_info_t *info, const char **tzp, int whichrule)
{
    int consumed;
    tz_offset_t *offset;
    const char *tz = *tzp;

    if (whichrule == 0
            && (*tz == '\0' || (*tz != '+' && *tz != '-' && !isdigit(*tz)))) {
        return false;
    }

    offset = (whichrule == 0) ? &info->std.offset : &info->dst.offset;
    memset(offset, 0, sizeof(*offset));
    offset->sign = -1;
    if (*tz == '-' || *tz == '+') {
        offset->sign = *tz++ == '-' ? 1 : -1;
    }
    *tzp = tz;

    consumed = 0;
    if (sscanf (tz, "%hu%n:%hu%n:%hu%n",
            &offset->h, &consumed, &offset->m, &consumed, &offset->s, &consumed) > 0) {
        if (!check_tzzone_offset_right(offset)) {
            return false;
        }
    } else if (whichrule == 0) {
        *tzp = tz + consumed;
    } else {
        offset_to_hhmmss(offset, hhmmss_to_offset(&info->std.offset) + 60 * 60);
        if (!check_tzzone_offset_right(offset)) {
            memcpy(offset, &info->std.offset, sizeof(*offset));
        }
    }
    *tzp = tz + consumed;

    return true;
}

static bool parse_rule(tz_info_t *info, const char **tzp, int whichrule)
{
    int consumed;
    int negative;
    unsigned long d;
    char *end;
    tz_rule_t *tzr;
    const char *tz = *tzp;

    tzr = (whichrule == 0) ?  &info->start : &info->end;
    tz += *tz == ',';
    if (*tz == 'J' || isdigit (*tz)) {
        tzr->type = *tz == 'J' ? J1 : J0;
        if (tzr->type == J1 && !isdigit(*++tz)) {
            return false;
        }

        d = strtoul (tz, &end, 10);
        if (end == tz || d > 365) {
            return false;
        }

        if (tzr->type == J1 && d == 0) {
            return false;
        }
        tzr->d = d;
        tz = end;
    } else if (*tz == 'M') {
        tzr->type = M;
        if (sscanf (tz, "M%hu.%hu.%hu%n", &tzr->m, &tzr->n, &tzr->d, &consumed) != 3
                || tzr->m < 1 || tzr->m > 12 || tzr->n < 1 || tzr->n > 5 || tzr->d > 6) {
            return false;
        }

        tz += consumed;
    } else if (*tz == '\0') {
        tzr->type = M;
        if (tzr == &info->start) {
            tzr->m = 3;
            tzr->n = 2;
            tzr->d = 0;
        } else {
            tzr->m = 11;
            tzr->n = 1;
            tzr->d = 0;
        }
    } else {
        return false;
    }

    if (*tz != '\0' && *tz != '/' && *tz != ',') {
        return false;
    } else if (*tz == '/') {
        ++tz;
        if (*tz == '\0') {
            return false;
        }

        negative = *tz == '-';
        tz += negative;
        tzr->offset.h = 2;
        tzr->offset.m = 0;
        tzr->offset.s = 0;
        consumed = 0;
        tzr->offset.sign = (negative ? -1 : 1);
        sscanf(tz, "%hu%n:%hu%n:%hu%n", &tzr->offset.h, &consumed, &tzr->offset.m, &consumed,
                &tzr->offset.s, &consumed);
        tz += consumed;
    } else {
        tzr->offset.h = 2;
        tzr->offset.m = 0;
        tzr->offset.s = 0;
        tzr->offset.sign = 1;
    }

    *tzp = tz;
    return true;
}

static int str2tz(tz_info_t *tz, const char *str)
{
    if (tz == NULL || str == NULL) {
        return TZ_ERRPR_INVALID_FUNC_PARAM;
    }

    memset(tz, 0, sizeof(tz_info_t));
    if (!parse_tzname(tz, &str, 0)) {
        return TZ_ERROR_INVALID_STD_NAME;
    }

    tz->mask |= F_STDZONE_SET;
    if (*str == '\0') {
        tz->std.offset.sign = 1;
        tz->std.offset.h = 0;
        tz->std.offset.m = 0;
        tz->std.offset.s = 0;
        return TZ_ERROR_NONE;
    } else if (!parse_offset(tz, &str, 0)) {
        return TZ_ERROR_INVALID_STD_OFFSET;
    }

    if (*str != ',') {
        if (*str == '\0') {
            return TZ_ERROR_NONE;
        }

        if (parse_tzname(tz, &str, 1)) {
            if (!parse_offset(tz, &str, 1)) {
                return TZ_ERROR_INVALID_DST_OFFSET;
            }

            tz->mask |= F_DSTZONE_SET;
        } else {
            return TZ_ERROR_INVALID_DST_NAME;
        }
    }

    if (!parse_rule(tz, &str, 0)) {
        return TZ_ERROR_WRONG_SYNTAX;
    }

    if (!check_tzrule_offset(&tz->start)) {
        return TZ_ERRPR_DST_TIME_OUT_OF_RANGE;
    }

    tz->mask |= F_DST_START_SET;
    if (!parse_rule(tz, &str, 1)) {
        return TZ_ERROR_WRONG_SYNTAX;
    }

    if (!check_tzrule_offset(&tz->end)) {
        return TZ_ERRPR_DST_TIME_OUT_OF_RANGE;
    } else if (!check_tzrule_right(tz)) {
        return TZ_ERRPR_DST_TIME_CONFLICT;
    }
    tz->mask |= F_DST_END_SET;

    return TZ_ERROR_NONE;
}

static int tz_offset_to_string(char *str, size_t n, const tz_offset_t *off, bool r, bool omit_sign)
{
    int ret;
    int idx;
    static const char *sign[3] = {"+", "-", ""};

    if (r) {
        idx = off->sign > 0 ? 1 : 0;
    } else {
        idx = off->sign < 0 ? 1 : 0;
    }

    if (omit_sign && idx == 0) {
        idx = 2;
    }

    if (off->s == 0) {
        if (off->m == 0) {
            if (off->h == 0 && r) {
                str[0] = '\0';
                ret = 0;
            } else {
                ret = snprintf(str, n, "%s%hu", sign[idx], off->h);
            }
        } else {
            ret = snprintf(str, n, "%s%02d:%02hu", sign[idx], off->h, off->m);
        }
    } else {
        ret = snprintf(str, n, "%s%02hu:%02hu:%02hu", sign[idx], off->h, off->m, off->s);
    }

    return ret;
}

static int tz_zone_to_string(char *str, size_t n, const tz_zone_t *zone)
{
    int ret;
    char buf[16];
    const char *name_fmt;
    const char *fmt[2] = {"%s%s", "<%s>%s"};

    if (tz_offset_to_string(buf, sizeof(buf), &zone->offset, true, false) < 0) {
        return -1;
    }

    for (name_fmt = fmt[0], ret = 0; zone->name[ret] != '\0'; ++ret) {
        if (!isalpha(zone->name[ret])) {
            name_fmt = fmt[1];
            break;
        }
    }

    return snprintf(str, n, name_fmt, zone->name, buf);
}

static int tz_rule_to_string(char *str, size_t n, const tz_rule_t *rule)
{
    int ret;
    int off;
    char buf[16 + 1];

    switch (rule->type) {
    case J0:
        ret = snprintf(str, n, "%hu", rule->d);
        break;
    case J1:
        ret = snprintf(str, n, "J%hu", rule->d);
        break;
    case M:
        ret = snprintf(str, n, "M%hu.%hu.%hu", rule->m, rule->n, rule->d);
        break;
    default:
        ret = -1;
        break;
    }

    if (ret < 0) {
        return -1;
    }

    buf[0] = '/';
    if ((off = tz_offset_to_string(buf + 1, sizeof(buf) - 1, &rule->offset, false, true)) < 0) {
        return -1;
    }

    if (off != 0) {
        if (ret + off + 1 >= n) {
            return -1;
        }

        memcpy(str + ret, buf, off + 2);
        ret += off + 1;
    }

    return ret;
}

static int tz_convert_string(char *str, size_t n, const tz_info_t *tz)
{
    int ret;
    int mask;

    if (str == NULL || tz == NULL) {
        return TZ_ERRPR_INVALID_FUNC_PARAM;
    }

    mask = tz->mask;
    if (!(mask & F_STDZONE_SET)) {
        return TZ_ERROR_INTERNAL_ERROR;
    }

    mask &= ~F_STDZONE_SET;
    if ((ret = tz_zone_to_string(str, n, &tz->std)) < 0) {
        return TZ_ERROR_INTERNAL_ERROR;
    }

    n -= ret;
    str += ret;
    if (n == 0) {
        return (mask == 0) ? TZ_ERROR_NONE : TZ_ERROR_INTERNAL_ERROR;
    }

    if (mask & F_DSTZONE_SET) {
        if (!(mask & F_DST_START_SET)) {
            return TZ_ERROR_NONE;
        }

        mask &= ~F_DSTZONE_SET;
        if ((ret = tz_zone_to_string(str, n, &tz->dst)) < 0) {
            return -1;
        }

        n -= ret;
        str += ret;
        if (n == 0) {
            return (mask == 0) ? TZ_ERROR_NONE : TZ_ERROR_INTERNAL_ERROR;
        }
    }

    if (!(mask & F_DST_START_SET)) {
        return TZ_ERROR_NONE;
    }

    if (!(tz->mask & F_DSTZONE_SET)) {
        return TZ_ERROR_EMPTY_DST_INFO;
    }

    --n;
    *str++ = ',';
    mask &= ~F_DST_START_SET;
    if ((ret = tz_rule_to_string(str, n, &tz->start)) < 0) {
        return TZ_ERROR_INTERNAL_ERROR;
    }

    n -= ret;
    str += ret;
    if (n == 0) {
        return (mask == 0) ? TZ_ERROR_NONE : TZ_ERROR_INTERNAL_ERROR;
    }

    if (!(mask & F_DST_END_SET)) {
        return TZ_ERROR_INTERNAL_ERROR;
    }

    --n;
    *str++ = ',';
    mask &= ~F_DST_END_SET;
    if ((ret = tz_rule_to_string(str, n, &tz->end)) < 0) {
        return TZ_ERROR_INTERNAL_ERROR;
    }

    return TZ_ERROR_NONE;
}

static int dst_rule_fprintf(FILE *fp, const tz_rule_t *rule, const tz_offset_t *off)
{
    int ret;
    int day;
    int month;
    int yeari;
    static const char *year_type[] = {"-", "noleap", "leap"};
    static const char *tip_week[] =
            {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *tip_mon[] =
            {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    /* Rule NAME FROM TO TYPE IN ON AT SAVE LETTER/S */
    switch (rule->type) {
    case J0:    /* 0 - 365 */
        if (rule->d >= 59) {
            yeari = 2;
            day = rule->d + 1;
            month = get_month_in_leap_days(day);
            day = day - leap_days[month];
            if ((ret = fprintf(fp,
                    "Rule RUIJIE minimum maximum %s %s\t%d\t%c%02hu:%02hu:%02hu\t%c%02hu:%02hu:%02hu\t-\n",
                    year_type[yeari], tip_mon[month], day,
                    sign_char(rule->offset.sign), rule->offset.h, rule->offset.m, rule->offset.s,
                    sign_char(off->sign), off->h, off->m, off->s)) < 0) {
                break;
            }

            if (rule->d == 365) {
                break;
            }

            yeari = 1;
        } else {
            yeari = 0;
        }

        day = rule->d + 1;
        month = get_month_in_julian_days(day);
        day = day - julian_days[month];
        ret = fprintf(fp,
                "Rule RUIJIE minimum maximum %s %s\t%d\t%c%02hu:%02hu:%02hu\t%c%02hu:%02hu:%02hu\t-\n",
                year_type[yeari], tip_mon[month], day,
                sign_char(rule->offset.sign), rule->offset.h, rule->offset.m, rule->offset.s,
                sign_char(off->sign), off->h, off->m, off->s);
        break;
    case J1:    /* 1 - 365 */
        month = get_month_in_julian_days(rule->d);
        day = rule->d - julian_days[month];
        ret = fprintf(fp,
                "Rule RUIJIE minimum maximum - %s\t%d\t%c%02hu:%02hu:%02hu\t%c%02hu:%02hu:%02hu\t-\n",
                tip_mon[month], day,
                sign_char(rule->offset.sign), rule->offset.h, rule->offset.m, rule->offset.s,
                sign_char(off->sign), off->h, off->m, off->s);
        break;
    case M:
        ret = fprintf(fp,
                "Rule RUIJIE minimum maximum - %s\t%s>=%d\t%c%02hu:%02hu:%02hu\t%c%02hu:%02hu:%02hu\t-\n",
                tip_mon[rule->m - 1], tip_week[rule->d], WEEKDAY2MONTHDAY(rule->n),
                sign_char(rule->offset.sign), rule->offset.h, rule->offset.m, rule->offset.s,
                sign_char(off->sign), off->h, off->m, off->s);
        break;
    default:
        ret = -1;
        break;
    }

    return ret;
}

int parse_posix_tzstring(const char *str, const char *path, char *format, const size_t n)
{
    int ret;
    FILE *fp;
    tz_info_t tz;
    char *rule_name;

    if (path == NULL || str == NULL || format == NULL || n == 0) {
        return TZ_ERRPR_INVALID_FUNC_PARAM;
    }

    if ((ret = str2tz(&tz, str)) != TZ_ERROR_NONE
            || (ret = tz_convert_string(format, n, &tz)) != TZ_ERROR_NONE) {
        return ret;
    }

    unlink(path);
    if ((fp = fopen(path, "w")) == NULL) {
        return TZ_ERROR_INTERNAL_ERROR;
    }

    if (tz.mask & F_DST_START_SET) {
        tz_offset_sub(&tz.dst.offset, &tz.dst.offset, &tz.std.offset);
        if (dst_rule_fprintf(fp, &tz.start, &tz.dst.offset) < 0) {
            fclose(fp);
            return TZ_ERROR_INTERNAL_ERROR;
        }

        memset(&tz.dst.offset, 0, sizeof(tz.dst.offset));
        if (dst_rule_fprintf(fp, &tz.end, &tz.dst.offset) < 0) {
            fclose(fp);
            return TZ_ERROR_INTERNAL_ERROR;
        }

        rule_name = "RUIJIE";
    } else {
        rule_name = "-";
    }

    /* Zone NAME UTOFF RULES FORMAT [UNTIL] */
    if (fprintf(fp,
            "Zone localtime %c%02hu:%02hu:%02hu %s %s/%s\n",
            tz.std.offset.sign < 0 ? '-' : '+', tz.std.offset.h, tz.std.offset.m, tz.std.offset.s,
            rule_name, tz.std.name, tz.dst.name[0] == '\0' ? tz.std.name : tz.dst.name) < 0) {
        fclose(fp);
        return TZ_ERROR_INTERNAL_ERROR;
    }
    fflush(fp);
    //fsync(fileno(fp));
    fclose(fp);

    return TZ_ERROR_NONE;
}
