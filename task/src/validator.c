#include "validator.h"
#include "common.h"
#include <stdio.h>  // 引入标准输入输出库，snprintf 函数需要
#include <string.h> // 引入字符串处理库，strlen 函数需要
#include <ctype.h>  // 引入字符分类库（通常用于字符判断）
#include <stdlib.h> // 引入标准工具库，atoi 函数需要
/**
 * @brief 检查学号格式是否合法
 *
 * @param id 待检查的学号字符串
 * @param err_msg 用于存储错误信息的字符数组（可为NULL）
 * @param size err_msg 缓冲区的大小
 * @return true 格式合法
 * @return false 格式不合法
 */
bool CheckStudentIDFormat(const char *id, char *err_msg, int size)
{
    // 检查指针是否为空，以及学号长度是否不等于12位
    if (!id || strlen(id) != 12)
    {
        if (err_msg)
            snprintf(err_msg, size, "学号必须为12位数字");
        return false;
    }
    // 遍历学号的每一个字符，检查是否全为数字 ('0'-'9')
    for (int i = 0; i < 12; i++)
    {
        if (id[i] < '0' || id[i] > '9')
        {
            if (err_msg)
                snprintf(err_msg, size, "学号必须全为数字");
            return false;
        }
    }
    return true;
}
/**
 * @brief 检查课程编号格式是否合法
 *
 * @param id 待检查的课程编号字符串
 * @param err_msg 用于存储错误信息的字符数组（可为NULL）
 * @param size err_msg 缓冲区的大小
 * @return true 格式合法
 * @return false 格式不合法
 */
bool CheckCourseIDFormat(const char *id, char *err_msg, int size)
{
    // 检查指针是否为空，以及课程编号长度是否不等于8位
    if (!id || strlen(id) != 8)
    {
        if (err_msg)
            snprintf(err_msg, size, "课程编号必须为8位");
        return false;
    }
    // 检查前两位是否为大写字母 (A-Z)
    if ((id[0] < 'A' || id[0] > 'Z') || (id[1] < 'A' || id[1] > 'Z'))
    {
        if (err_msg)
            snprintf(err_msg, size, "课程编号前两位必须为大写字母");
        return false;
    }
    // 检查后六位是否全为数字 (0-9)
    for (int i = 2; i < 8; i++)
    {
        if (id[i] < '0' || id[i] > '9')
        {
            if (err_msg)
                snprintf(err_msg, size, "课程编号后六位必须为数字");
            return false;
        }
    }
    return true;
}
/**
 * @brief 检查日期格式及合法性 (YYYY-MM-DD)
 *
 * @param date 待检查的日期字符串
 * @param err_msg 用于存储错误信息的字符数组（可为NULL）
 * @param size err_msg 缓冲区的大小
 * @return true 日期格式及数值合法
 * @return false 日期格式或数值不合法
 */
bool CheckDateFormat(const char *date, char *err_msg, int size)
{
    // 检查指针是否为空，以及日期字符串长度是否不等于10 (YYYY-MM-DD)

    if (!date || strlen(date) != 10)
    {
        if (err_msg)
            snprintf(err_msg, size, "日期格式应为YYYY-MM-DD");
        return false;
    }
    // 检查分隔符 '-' 的位置是否正确 (索引4和7)
    if (date[4] != '-' || date[7] != '-')
    {
        if (err_msg)
            snprintf(err_msg, size, "日期格式应为YYYY-MM-DD");
        return false;
    }

    // 将字符串中的年、月、日部分转换为整数
    int year = atoi(date);
    int month = atoi(date + 5);
    int day = atoi(date + 8);

    // 检查年份是否在允许的范围 (2020-2026) 内
    if (year < 2020 || year > 2026)
    {
        if (err_msg)
            snprintf(err_msg, size, "年份必须在2020-2026之间");
        return false;
    }

    // 检查月份是否在 1-12 之间
    if (month < 1 || month > 12)
    {
        if (err_msg)
            snprintf(err_msg, size, "月份必须在1-12之间");
        return false;
    }

    // 判断是否为闰年
    // 闰年规则：能被4整除且不能被100整除，或者能被400整除
    bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);

    // 定义平年情况下每个月的最大天数，索引0占位，1-12对应月份
    int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // 如果是闰年，将2月的最大天数修改为29天
    if (is_leap)
        days_in_month[2] = 29;

    // 检查输入的日期是否超出了当月的最大天数
    if (day < 1 || day > days_in_month[month])
    {
        if (err_msg)
        {
            // 针对2月份给出更详细的闰年/平年提示
            if (month == 2)
            {
                if (is_leap)
                    snprintf(err_msg, size, "%d年是闰年，2月有29天（您输入了%d日）", year, day);
                else
                    snprintf(err_msg, size, "%d年不是闰年，2月只有28天（您输入了%d日）", year, day);
            }
            else
                snprintf(err_msg, size, "%d月只有%d天（您输入了%d日）", month, days_in_month[month], day);
        }
        return false;
    }

    // --- 精确日期范围校验 (2020-09-01 至 2026-06-01) ---

    // 检查是否早于系统允许的最小日期 2020-09-01
    if (year < 2020 ||
        (year == 2020 && month < 9) ||
        (year == 2020 && month == 9 && day < 1))
    {
        if (err_msg)
            snprintf(err_msg, size, "日期不能早于2020-09-01");
        return false;
    }

    // 检查是否晚于系统允许的最大日期 2026-06-01
    if (year > 2026 ||
        (year == 2026 && month > 6) ||
        (year == 2026 && month == 6 && day > 1))
    {
        if (err_msg)
            snprintf(err_msg, size, "日期不能晚于2026-06-01");
        return false;
    }

    return true;
}
/**
 * @brief 检查学期格式是否合法 (YYYY-XX)
 *
 * @param semester 待检查的学期字符串
 * @param err_msg 用于存储错误信息的字符数组（可为NULL）
 * @param size err_msg 缓冲区的大小
 * @return true 格式合法
 * @return false 格式不合法
 */
bool CheckSemesterFormat(const char *semester, char *err_msg, int size)
{
    // 检查指针是否为空，以及学期字符串长度是否不等于7 (例如: 2023-01)

    if (!semester || strlen(semester) != 7)
    {
        if (err_msg)
            snprintf(err_msg, size, "学期格式应为YYYY-XX");
        return false;
    }
    // 检查第5个字符（索引4）是否为分隔符 '-'
    if (semester[4] != '-')
    {
        if (err_msg)
            snprintf(err_msg, size, "学期格式应为YYYY-XX");
        return false;
    }
    return true;
}
/**
 * @brief 校验选课/注册日期是否在学期规定的开放窗口期内
 *
 * @param semester 学期字符串 (如 "2023-01")
 * @param enroll_date 选课日期字符串 (如 "2023-03-05")
 * @return true 日期在允许的窗口期内
 * @return false 日期不在允许的窗口期内或格式错误
 */
bool IsValidEnrollDate(const char *semester, const char *enroll_date)
{
    // 检查传入的指针是否为空
    if (!semester || !enroll_date)
        return false;

    // 1. 解析学期类型 (YYYY-XX)
    // 约定: 01 代表春季学期(通常2月开学), 02 代表秋季学期(通常9月开学)

    int sem_type = atoi(semester + 5); // 提取 'XX' 部分
    int sem_year = atoi(semester);     // 提取 'YYYY' 部分
    // 2. 计算"开学后一个月"的目标月份，作为选课开放的目标月份

    int target_month = 0;
    if (sem_type == 1)
    {
        target_month = 3; // 春季学期: 2月开学 -> 选课目标月为 3月
    }
    else if (sem_type == 2)
    {
        target_month = 10; // 秋季学期: 9月开学 -> 选课目标月为 10月
    }
    else
    {
        return false; // 未知的学期类型 (非01或02)
    }
    // 3. 解析传入的选课日期的年、月、日
    int date_year = atoi(enroll_date);
    int date_month = atoi(enroll_date + 5);
    int date_day = atoi(enroll_date + 8);

    // 4. 校验年份和月份必须严格匹配目标开学月
    // 只有当选课日期处于"开学后一个月"时才允许进行选课操作
    if (date_year != sem_year || date_month != target_month)
    {
        return false;
    }

    // 5. 校验具体日期是否在规定的开放窗口内
    // 业务规则：开学月的第一个星期(1-7日)开放，随后关闭三天(8-10日)，再开放三天(11-13日)
    if ((date_day >= 1 && date_day <= 7) ||
        (date_day >= 11 && date_day <= 13))
    {
        return true; // 在开放窗口期内
    }
    // 其他日期(如14日及以后，或8-10日的关闭期)均不允许
    return false;
}