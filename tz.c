#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#define TZNAME_SIZE     32

enum {
    TZ_NO_ERROR             = 0,
    TZ_ERROR_INVAL_PARAM    = -1,
    TZ_ERROR_NO_STDZONE     = -2,
    TZ_ERROR_NO_DSTZONE     = -3,
    TZ_ERROR_NO_DST_END_SET = -4,
    TZ_ERROR_NO_BUFF        = -5,
    TZ_ERROR_PARSE_STDINFO  = -6,
    TZ_ERROR_PARSE_DSTINFO  = -7,
    TZ_ERROR_PARSE_RULE     = -8,
    TZ_ERROR_OPEN_FILE      = -9,
    TZ_ERROR_SAVE_FILE      = -10,
    TZ_ERROR_MAX            = -11
};

static const char *tz_error_to_string(const int err)
{
    int index;
    static const char *reason[12] = {
        "\0",
        "invalid parameter",
        "not found valid standard zone",
        "not found valid daylight saving time",
        "not found the valid end rule of daylight saving time",
        "no enough buffer",
        "fail to parse standard zone info",
        "fail to parse daylight saving time info",
        "fail to parse daylight saving time rules",
        "fail to open file",
        "fail to save file"
    };

    if (err > 0 || err <= TZ_ERROR_MAX) {
        return "unknown cause";
    } else {
        index = -err;
    }

    return reason[index];
}

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

#define F_STDZONE_SET           (1 << 0)
#define F_DSTZONE_SET           (1 << 1)
#define F_DST_START_SET         (1 << 2)
#define F_DST_END_SET           (1 << 3)

typedef struct {
    tz_zone_t std;
    tz_zone_t dst;
    tz_rule_t start;
    tz_rule_t end;
    unsigned short mask;
} tz_info_t;

static void offset_to_hhmmss(tz_offset_t *t, int offset)
{
    t->sign = offset > 0 ? 1 : -1;
    offset = abs(offset);
    t->h = offset / (60 * 60);
    t->m = (offset / 60) % 60;
    t->s = offset % 60;
}

static int hhmmss_to_offset(tz_offset_t *t)
{
    int h, m, s;

    if (t->h > 24) {
        t->h = 24;
    }
    h = t->h;

    if (t->m > 59) {
        t->m = 59;
    }
    m = t->m;

    if (t->s > 59) {
        t->s = 59;
    }
    s = t->s;

    return t->sign * (s + (m + h * 60) * 60);
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
        (void)hhmmss_to_offset(offset);
    } else if (whichrule == 0) {
        *tzp = tz + consumed;
    } else {
        memcpy(offset, &info->std.offset, sizeof(*offset));
        offset_to_hhmmss(offset, hhmmss_to_offset(&info->std.offset) + 60 * 60);
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
        return TZ_ERROR_INVAL_PARAM;
    }

    memset(tz, 0, sizeof(tz_info_t));
    if (!parse_tzname(tz, &str, 0)) {
        return TZ_ERROR_PARSE_STDINFO;
    }

    tz->mask |= F_STDZONE_SET;
    if (*str == '\0') {        
        tz->std.offset.sign = 1;
        tz->std.offset.h = 0;
        tz->std.offset.m = 0;
        tz->std.offset.s = 0;
        return TZ_NO_ERROR;
    } else if (!parse_offset(tz, &str, 0)) {
        return TZ_ERROR_PARSE_STDINFO;
    }

    if (*str == '\0') {
        return TZ_NO_ERROR;
    }

    if (parse_tzname(tz, &str, 1)) {
        parse_offset(tz, &str, 1);
        tz->mask |= F_DSTZONE_SET;
    }

    if (*str == '\0' || (str[0] == ',' && str[1] == '\0')) {
        return TZ_NO_ERROR;
    } else if (!(tz->mask & F_DSTZONE_SET)) {
        return TZ_ERROR_PARSE_DSTINFO;
    }

    if (!parse_rule(tz, &str, 0)) {
        return TZ_ERROR_PARSE_RULE;
    }

    tz->mask |= F_DST_START_SET;
    if (!parse_rule(tz, &str, 1)) {
        return TZ_ERROR_PARSE_RULE;
    }
    tz->mask |= F_DST_END_SET;

    return TZ_NO_ERROR;
}

static int tz_offset_to_string(char *str, size_t n, const tz_offset_t *off, int r)
{
    int ret;
    char sign;

    if (r) {
        sign = off->sign > 0 ? '-' : '+';
    } else {
        sign = off->sign < 0 ? '-' : '+';
    }

    if (off->s == 0) {
        if (off->m == 0) {
            if (off->h == 0) {
                ret = 0;
                str[0] = '\0';
            } else {
                ret = snprintf(str, n, "%c%hu", sign, off->h);
            }
        } else {
            ret = snprintf(str, n, "%c%02d:%02hu", sign, off->h, off->m);
        }
    } else {
        ret = snprintf(str, n, "%c%02hu:%02hu:%02hu", sign, off->h, off->m, off->s);
    }

    return ret;
}

static int tz_zone_to_string(char *str, size_t n, const tz_zone_t *zone)
{
    int ret;
    char buf[16];

    if ((ret = tz_offset_to_string(buf, sizeof(buf), &zone->offset, 1)) < 0) {
        return -1;
    }

    if ((ret = snprintf(str, n, "%s%s", zone->name, buf)) < 0) {
        return -1;
    }

    return ret;
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
    if ((off = tz_offset_to_string(buf + 1, sizeof(buf) - 1, &rule->offset, 0)) < 0) {
        return -1;
    }

    if (off != 0) {
        if (ret + off + 1 >= n) {
            return -1;
        }

        strncpy(str + ret, buf, off + 1);
        ret += off + 1;
    }

    return ret;
}

int tz_convert_string(char *str, size_t n, const tz_info_t *tz)
{
    int ret;
    int mask;

    if (str == NULL || tz == NULL) {
        return TZ_ERROR_INVAL_PARAM;
    }

    mask = tz->mask;
    if (!(mask & F_STDZONE_SET)) {
        return TZ_ERROR_NO_STDZONE;
    }

    mask &= ~F_STDZONE_SET;
    if ((ret = tz_zone_to_string(str, n, &tz->std)) < 0) {
        return TZ_ERROR_NO_BUFF;
    }

    n -= ret;
    str += ret;
    if (n == 0) {
        return (mask == 0) ? TZ_NO_ERROR : TZ_ERROR_NO_BUFF; 
    }

    if (mask & F_DSTZONE_SET) {
        mask &= ~F_DSTZONE_SET;
        if ((ret = tz_zone_to_string(str, n, &tz->dst)) < 0) {
            return -1;
        }

        n -= ret;
        str += ret;
        if (n == 0) {
            return (mask == 0) ? TZ_NO_ERROR : TZ_ERROR_NO_BUFF; 
        }
    }

    if (!(mask & F_DST_START_SET)) {
        return TZ_NO_ERROR;
    }

    if (!(tz->mask & F_DSTZONE_SET)) {
        return TZ_ERROR_NO_DSTZONE;
    }

    --n;
    *str++ = ',';
    mask &= ~F_DST_START_SET;
    if ((ret = tz_rule_to_string(str, n, &tz->start)) < 0) {
        return TZ_ERROR_NO_BUFF;
    }

    n -= ret;
    str += ret;
    if (n == 0) {
        return (mask == 0) ? TZ_NO_ERROR : TZ_ERROR_NO_BUFF; 
    }

    if (!(mask & F_DST_END_SET)) {
        return TZ_ERROR_NO_DST_END_SET;
    }

    --n;
    *str++ = ',';
    mask &= ~F_DST_END_SET;
    if ((ret = tz_rule_to_string(str, n, &tz->end)) < 0) {
        return TZ_ERROR_NO_BUFF;
    }

    return TZ_NO_ERROR;
}

#define WEEKDAY2MONTHDAY(w,d)   (((w) - 1) * 7 + (d) + 1)

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

    switch (rule->type) {
    case J0:    /* 0 - 365 */
        yeari = (rule->d >= 59) ? 1 : 0;
        day = rule->d + 1;
        month = get_month_in_julian_days(day);
        day = day - julian_days[month];
        if ((ret = fprintf(fp,
                "Rule RUIJIE minimum maximum %s %s\t%d\t%c%02hu:%02hu:%02hu\t%c%02hu:%02hu:%02hu\t-\n",
                year_type[yeari], tip_mon[month], day,
                rule->offset.sign < 0 ? '-' : '+', rule->offset.h, rule->offset.m, rule->offset.s,
                off->sign < 0 ? '-' : '+', off->h, off->m, off->s)) < 0) {
            break;
        }

        if (rule->d < 59) {
            break;
        }

        yeari = 2;
        day = rule->d + 1;
        month = get_month_in_leap_days(day);
        day = day - leap_days[month];
        ret = fprintf(fp,
                "Rule RUIJIE minimum maximum %s %s\t%d\t%c%02hu:%02hu:%02hu\t%c%02hu:%02hu:%02hu\t-\n",
                year_type[yeari], tip_mon[month], day,
                rule->offset.sign < 0 ? '-' : '+', rule->offset.h, rule->offset.m, rule->offset.s,
                off->sign < 0 ? '-' : '+', off->h, off->m, off->s);
        break;
    case J1:    /* 1 - 365 */
        month = get_month_in_julian_days(rule->d);
        day = rule->d - julian_days[month];
        /* Rule NAME FROM TO TYPE IN ON AT SAVE LETTER/S */
        ret = fprintf(fp,
                "Rule RUIJIE minimum maximum - %s\t%d\t%c%02hu:%02hu:%02hu\t%c%02hu:%02hu:%02hu\t-\n",
                tip_mon[month], day,
                rule->offset.sign < 0 ? '-' : '+', rule->offset.h, rule->offset.m, rule->offset.s,
                off->sign < 0 ? '-' : '+', off->h, off->m, off->s);
        break;
    case M:
        /* Rule NAME FROM TO TYPE IN ON AT SAVE LETTER/S */
        ret = fprintf(fp,
                "Rule RUIJIE minimum maximum - %s\t%s>=%d\t%c%02hu:%02hu:%02hu\t%c%02hu:%02hu:%02hu\t-\n",
                tip_mon[rule->m - 1], tip_week[rule->d], WEEKDAY2MONTHDAY(rule->n, rule->d),
                rule->offset.sign < 0 ? '-' : '+', rule->offset.h, rule->offset.m, rule->offset.s,
                off->sign < 0 ? '-' : '+', off->h, off->m, off->s);
        break;
    default:
        ret = -1;
        break;
    }

    return ret;
}

int str2tzfile(const char *path, const char *str, char *format, const size_t n)
{
    int ret;
    FILE *fp;
    tz_info_t tz;

    if (path == NULL || str == NULL || format == NULL) {
        return TZ_ERROR_INVAL_PARAM;
    }

    if ((ret = str2tz(&tz, str)) != TZ_NO_ERROR
            || (ret = tz_convert_string(format, n, &tz)) != TZ_NO_ERROR) {
        return ret;
    }

    unlink(path);
    if ((fp = fopen(path, "w")) == NULL) {
        return TZ_ERROR_OPEN_FILE;
    }

    if (tz.mask & F_DST_START_SET) {
        if (dst_rule_fprintf(fp, &tz.start, &tz.dst.offset) < 0) {
            fclose(fp);
            return TZ_ERROR_SAVE_FILE;
        }

        memset(&tz.dst.offset, 0, sizeof(tz.dst.offset));
        if (dst_rule_fprintf(fp, &tz.end, &tz.dst.offset) < 0) {
            fclose(fp);
            return TZ_ERROR_SAVE_FILE;
        }
    }

    /* Zone NAME UTOFF RULES FORMAT [UNTIL] */
    if (fprintf(fp,
            "Zone localtime %c%02hu:%02hu:%02hu RUIJIE %s/%s\n",
            tz.std.offset.sign < 0 ? '-' : '+', tz.std.offset.h, tz.std.offset.m, tz.std.offset.s,
            tz.std.name, tz.dst.name) < 0) {
        fclose(fp);
        return TZ_ERROR_SAVE_FILE;
    }
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    return TZ_NO_ERROR;
}

int main(int argc, char *argv[])
{
    int ret;
    char buf[256 + 1];

    if (argc < 2) {
        return -1;
    }

    if ((ret = str2tzfile("/Workspace/xxxx.tz", argv[1], buf, sizeof(buf))) != 0) {
        fprintf(stderr, "setting timezone failed, since %s\n", tz_error_to_string(ret));
        return -1;
    }
    puts(buf);

    return 0;
}
