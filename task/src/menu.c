#define _CRT_SECURE_NO_WARNINGS // 禁用 MSVC 的安全函数警告 (如 strcpy -> strcpy_s)
#define _POSIX_C_SOURCE 199309L // 启用 POSIX 标准功能 (如 clock_gettime)
#include "menu.h"
#include "data_manager.h"
#include "course_catalog.h"
#include "validator.h"
#include "data_generator.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <conio.h> // 用于 _getch() 无回显字符读取
#include "task_demo.h"

/* =========================================
 * Windows API 手动声明 (防止 GCC/MinGW 链接崩溃)
 * 直接通过 dllimport 引入 kernel32.dll 中的控制台 API
 * ========================================= */
#ifdef _WIN32
typedef void *HANDLE;
typedef unsigned long DWORD;
#define STD_OUTPUT_HANDLE ((DWORD) - 11)
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004 // 启用 ANSI 转义序列支持
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD nStdHandle);
__declspec(dllimport) int __stdcall GetConsoleMode(HANDLE hConsoleHandle, DWORD *lpMode);
__declspec(dllimport) int __stdcall SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode);
#endif

/* =========================================
 * 0. 导航状态码与全局变量
 * ========================================= */
// UI 交互返回值状态码
#define RET_OK 1    // 正常确认 (回车)
#define RET_BACK -1 // 返回上一级 (b/B/ESC)
#define RET_QUIT -2 // 退出程序 (q/Q)
#define RET_HOME -3 // 返回主菜单 (h/H)
// Tab 键预览类型
#define PREVIEW_NONE 0
#define PREVIEW_STUDENT 1
#define PREVIEW_COURSE 2

bool g_data_modified = false;                // 数据脏标记：用于退出时提示是否保存
static char CURRENT_DATE[20] = "2026-06-01"; // 模拟的系统当前时间 (支持手动修改以测试时间窗口)

// 多关键字排序规则结构体
typedef struct
{
    int field;     // 排序字段 (1:学号, 2:成绩, 3:学分, 4:日期)
    int direction; // 排序方向 (1:升序, 2:降序)
} SortRule;
// static SortRule g_sort_rules[3];  // 最多支持3级排序优先级
// static int g_sort_rule_count = 0; // 当前生效的排序规则数量

/* =========================================
 * 1. 全局 UI 交互引擎 (供 task_demo.c 等模块调用)
 * ========================================= */

/**
 * @brief 开启 Windows 控制台的 ANSI 转义序列支持
 * @note 使得 printf 中的 \033[...m 等颜色/光标控制代码生效
 */
void EnableANSI()
{
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode))
    {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
#endif
}
/**
 * @brief 计算 UTF-8 编码字符的字节长度
 */
static int utf8_char_len(unsigned char c)
{
    if (c >= 0xF0)
        return 4;
    if (c >= 0xE0)
        return 3;
    if (c >= 0xC0)
        return 2;
    return 1;
}
/**
 * @brief 计算字符串的终端显示宽度 (中文字符占2格，英文占1格)
 * @note 用于光标回退和界面重绘对齐
 */
static int display_width(const char *str)
{
    int width = 0;
    while (*str)
    {
        unsigned char c = (unsigned char)*str;
        int len = utf8_char_len(c);
        if (len >= 3)
            width += 2; // 假设 >=3 字节的为中文/全角字符

        else
            width += 1;
        str += len;
    }
    return width;
}
/**
 * @brief 核心输入函数：支持光标移动、退格、UTF-8 完整输入及快捷键
 * @param prompt 提示语
 * @param buffer 输入缓冲区 (可包含默认值)
 * @param size 缓冲区大小
 * @return 状态码 (RET_OK, RET_BACK, RET_QUIT, RET_HOME)
 */

int ReadStringWithDefault(const char *prompt, char *buffer, int size)
{
    static int ansi_enabled = 0;
    if (!ansi_enabled)
    {
        EnableANSI();
        ansi_enabled = 1;
    }
    int len = strlen(buffer);
    int cursor = len; // 光标初始在末尾
    printf("%s%s", prompt, buffer);
    fflush(stdout);
    while (1)
    {
        int ch = _getch();
        // 处理方向键 (0xE0 或 0 开头，后跟实际键码)
        if (ch == 0 || ch == 0xE0)
        {
            ch = _getch();
            if (ch == 75 && cursor > 0) // 左方向键
            {
                int prev = cursor - 1;
                while (prev >= 0 && ((unsigned char)buffer[prev] & 0xC0) == 0x80)
                    prev--; // 跳过 UTF-8 延续字节
                if (prev < 0)
                    prev = 0;
                cursor = prev;
            }
            else if (ch == 77 && cursor < len) // 右方向键
            {
                int clen = utf8_char_len((unsigned char)buffer[cursor]);
                if (cursor + clen > len)
                    clen = len - cursor;
                cursor += clen;
            }
        }
        else if (ch == 13) // 回车确认
        {
            printf("\n");
            fflush(stdout);
            if (strcmp(buffer, "q") == 0 || strcmp(buffer, "Q") == 0)
                return RET_QUIT;
            if (strcmp(buffer, "h") == 0 || strcmp(buffer, "H") == 0)
                return RET_HOME;
            if (strcmp(buffer, "b") == 0 || strcmp(buffer, "B") == 0)
                return RET_BACK;
            return RET_OK;
        }
        else if (ch == 27) // ESC 返回
        {
            printf("\n");
            fflush(stdout);
            return RET_BACK;
        }
        else if (ch == 8) // 退格键
        {
            if (cursor > 0)
            {
                int prev = cursor - 1;
                while (prev >= 0 && ((unsigned char)buffer[prev] & 0xC0) == 0x80)
                    prev--;
                if (prev < 0)
                    prev = 0;
                int clen = cursor - prev;
                memmove(&buffer[cursor - clen], &buffer[cursor], len - cursor + 1);
                len -= clen;
                cursor -= clen;
            }
        }
        else if (ch >= 32 || ch < 0) // 可打印字符 (包含中文首字节)

        {
            unsigned char uc = (unsigned char)ch;
            int clen = 1;
            if (uc >= 0xF0)
                clen = 4;
            else if (uc >= 0xE0)
                clen = 3;
            else if (uc >= 0xC0)
                clen = 2;
            char mb[4] = {(char)ch, 0, 0, 0};
            for (int i = 1; i < clen; i++) // 读取 UTF-8 后续字节
            {
                int next_ch = _getch();
                if (next_ch == -1)
                    next_ch = _getch();
                mb[i] = (char)next_ch;
            }
            if (len + clen < size - 1)
            {
                memmove(&buffer[cursor + clen], &buffer[cursor], len - cursor + 1);
                for (int i = 0; i < clen; i++)
                    buffer[cursor + i] = mb[i];
                len += clen;
                cursor += clen;
            }
        }
        else
            continue;
        // 重绘当前行并移动光标到正确位置
        printf("\033[?25l\r\033[2K%s%s", prompt, buffer); // 隐藏光标，清行，打印内容
        int back_steps = display_width(buffer + cursor);
        if (back_steps > 0)
            printf("\033[%dD", back_steps); // 光标左移到正确位置

        printf("\033[?25h"); // 显示光标
        fflush(stdout);
    }
}
// 辅助：模糊匹配并展示学生信息 (用于 Tab 预览)
static void ShowMatchingStudents(const char *keyword)
{
    if (!keyword || strlen(keyword) == 0)
        return;
    FilterCriteria all_c = {0};
    all_c.is_fuzzy_course = true;
    all_c.score_min = 0;
    all_c.score_max = 100;
    int total = 0;
    CourseRecord *all = DM_FilterRecords(&all_c, &total);
    if (!all)
    {
        printf("[提示] 当前系统中暂无数据。\n");
        return;
    }
    int count = 0;
    printf("\n[匹配的学生] (最多显示10条)\n%-15s | %-10s | %-15s\n--------------------------------------------------------\n", "学号", "姓名", "学院");
    for (int i = 0; i < total && count < 10; i++)
    {
        if (strstr(all[i].student_id, keyword) || strstr(all[i].name, keyword))
        {
            printf("%-15s | %-10s | %-15s\n", all[i].student_id, all[i].name, all[i].college);
            count++;
        }
    }
    if (count == 0)
        printf("未找到匹配的学生。\n");
    printf("\n");
    free(all);
}
// 辅助：模糊匹配并展示课程信息 (用于 Tab 预览)
static void ShowMatchingCourses(const char *keyword)
{
    if (!keyword || strlen(keyword) == 0)
        return;
    printf("\n[匹配的课程] (最多显示10条)\n%-10s | %-20s | %-6s\n--------------------------------------------------------\n", "课程号", "课程名", "学分");
    int count = 0, total = GetSharedCourseCount();
    for (int i = 0; i < total && count < 10; i++)
    {
        const char *cid = GetSharedCourseID(i);
        CourseInfo info;
        if (GetCourseInfo(cid, &info) && (strstr(info.id, keyword) || strstr(info.name, keyword)))
        {
            printf("%-10s | %-20s | %-6.1f\n", info.id, info.name, info.credit);
            count++;
        }
    }
    if (count == 0)
        printf("未找到匹配的课程。\n");
    printf("\n");
}
/**
 * @brief 支持 Tab 键触发预览的输入函数
 */
int ReadStringWithTabPreview(const char *prompt, char *buffer, int size, int preview_type)
{
    static int ansi_enabled = 0;
    if (!ansi_enabled)
    {
        EnableANSI();
        ansi_enabled = 1;
    }
    int len = strlen(buffer), cursor = len;
    printf("%s%s", prompt, buffer);
    fflush(stdout);
    while (1)
    {
        int ch = _getch();
        if (ch == 9) // Tab 键触发预览
        {
            printf("\n");
            if (preview_type == PREVIEW_STUDENT)
                ShowMatchingStudents(buffer);
            else if (preview_type == PREVIEW_COURSE)
                ShowMatchingCourses(buffer);
            printf("%s%s", prompt, buffer);
            fflush(stdout);
            continue;
        }
        // 以下逻辑与 ReadStringWithDefault 完全一致 (处理方向键、回车、退格、UTF-8等)

        if (ch == 0 || ch == 0xE0)
        {
            ch = _getch();
            if (ch == 75 && cursor > 0)
            {
                int prev = cursor - 1;
                while (prev >= 0 && ((unsigned char)buffer[prev] & 0xC0) == 0x80)
                    prev--;
                if (prev < 0)
                    prev = 0;
                cursor = prev;
            }
            else if (ch == 77 && cursor < len)
            {
                int clen = utf8_char_len((unsigned char)buffer[cursor]);
                if (cursor + clen > len)
                    clen = len - cursor;
                cursor += clen;
            }
        }
        else if (ch == 13)
        {
            printf("\n");
            fflush(stdout);
            if (strcmp(buffer, "q") == 0 || strcmp(buffer, "Q") == 0)
                return RET_QUIT;
            if (strcmp(buffer, "h") == 0 || strcmp(buffer, "H") == 0)
                return RET_HOME;
            if (strcmp(buffer, "b") == 0 || strcmp(buffer, "B") == 0)
                return RET_BACK;
            return RET_OK;
        }
        else if (ch == 27)
        {
            printf("\n");
            fflush(stdout);
            return RET_BACK;
        }
        else if (ch == 8)
        {
            if (cursor > 0)
            {
                int prev = cursor - 1;
                while (prev >= 0 && ((unsigned char)buffer[prev] & 0xC0) == 0x80)
                    prev--;
                if (prev < 0)
                    prev = 0;
                int clen = cursor - prev;
                memmove(&buffer[cursor - clen], &buffer[cursor], len - cursor + 1);
                len -= clen;
                cursor -= clen;
            }
        }
        else if (ch >= 32 || ch < 0)
        {
            unsigned char uc = (unsigned char)ch;
            int clen = 1;
            if (uc >= 0xF0)
                clen = 4;
            else if (uc >= 0xE0)
                clen = 3;
            else if (uc >= 0xC0)
                clen = 2;
            char mb[4] = {(char)ch, 0, 0, 0};
            for (int i = 1; i < clen; i++)
            {
                int next_ch = _getch();
                if (next_ch == -1)
                    next_ch = _getch();
                mb[i] = (char)next_ch;
            }
            if (len + clen < size - 1)
            {
                memmove(&buffer[cursor + clen], &buffer[cursor], len - cursor + 1);
                for (int i = 0; i < clen; i++)
                    buffer[cursor + i] = mb[i];
                len += clen;
                cursor += clen;
            }
        }
        else
            continue;
        printf("\033[?25l\r\033[2K%s%s", prompt, buffer);
        int back_steps = display_width(buffer + cursor);
        if (back_steps > 0)
            printf("\033[%dD", back_steps);
        printf("\033[?25h");
        fflush(stdout);
    }
}
// 等待用户按任意键继续
void WaitEnter()
{
    printf("\n[提示] 按任意键继续...");
    fflush(stdout);
    _getch();
}
// 安全的行读取 (基于 fgets，自动去除末尾换行符)
static void SafeReadLine(const char *prompt, char *buffer, int size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buffer, size, stdin) != NULL)
        buffer[strcspn(buffer, "\n\r")] = '\0';
}

/* =========================================
 * 2. 辅助函数
 * ========================================= */
// 获取高精度时间 (用于性能压测计时)
// static double GetHighPrecisionTime(void)
// {
//     struct timespec ts;
//     clock_gettime(CLOCK_MONOTONIC, &ts);
//     return ts.tv_sec + ts.tv_nsec / 1000000000.0;
// }
// 格式化打印成绩 (-1 或 0 显示为 "待修")
// static void PrintScore(int score)
// {
//     if (score == -1 || score == 0)
//         printf("待修");
//     else
//         printf("%d", score);
// }
// 根据索引生成符合现实规则的学号 (年份+学院+专业+班级+序号)
// static void GenerateRealisticStudentID(int index, char *stu_id, int size)
// {
//     int year = 2020 + (index / 14400);
//     int rem = index % 14400;
//     int college_code = 1 + (rem / 1800);
//     rem = rem % 1800;
//     int major_code = 1 + (rem / 180);
//     rem = rem % 180;
//     int class_num = 1 + (rem / 30);
//     int student_num = 1 + (rem % 30);
//     snprintf(stu_id, size, "%d%02d%02d%02d%02d", year, college_code, major_code, class_num, student_num);
// }
#if 0
// 打印居中的菜单分割线与标题
static void PrintHeader(const char *title) { printf("\n================================================================\n                        %s\n================================================================\n", title); }
// 多关键字排序比较函数 (供 qsort 使用)
static int MultiKeyCompare(const void *a, const void *b)
{
    const CourseRecord *r1 = (const CourseRecord *)a;
    const CourseRecord *r2 = (const CourseRecord *)b;
    for (int i = 0; i < g_sort_rule_count; i++)
    {
        int cmp = 0;
        if (g_sort_rules[i].field == 1)
            cmp = strcmp(r1->student_id, r2->student_id);
        else if (g_sort_rules[i].field == 2)
            cmp = r1->score - r2->score;
        else if (g_sort_rules[i].field == 3)
            cmp = (r1->credit > r2->credit) ? 1 : ((r1->credit < r2->credit) ? -1 : 0);
        else if (g_sort_rules[i].field == 4)
            cmp = strcmp(r1->enroll_date, r2->enroll_date);
        if (g_sort_rules[i].direction == 2)
            cmp = -cmp; // 降序则反转结果
        if (cmp != 0)
            return cmp; // 当前优先级分出胜负则直接返回
    }
    return 0;
}

/* =========================================
 * 3. 教务系统子模块
 * ========================================= */

// 教师端操作台菜单
static int Menu_TeacherOperations()
{
    while (1)
    {
        PrintHeader("教师端操作台");
        printf("1. 录入/修改学生成绩\n2. 录入学生学籍信息 (新生注册)\n3. 录入学生选课信息 (完整录入)\n4. 录入学生并自动排课 (开学前3个月开放)\n5. 查看课程选课名单\n[b]返回  [q]退出  [h]主菜单\n");
        char choice_str[10] = {0};
        int ret = ReadStringWithDefault("请输入选项数字 (1-5): ", choice_str, sizeof(choice_str));
        if (ret == RET_HOME)
            return RET_HOME;
        if (ret == RET_QUIT || ret == RET_BACK)
            return RET_BACK;
        if (ret != RET_OK)
            continue;
        int choice = atoi(choice_str);
        if (choice < 1 || choice > 5)
        {
            printf("[错误] 无效选项！\n");
            WaitEnter();
            continue;
        }
        // 1. 录入/修改成绩
        if (choice == 1)
        {
            int step = 1;
            char stu_id[20] = {0}, cou_id[20] = {0}, score_str[10] = {0};
            char err_msg[100];
            int score;
            // 分步输入校验，支持返回上一步
            while (step > 0 && step <= 3)
            {
                if (step == 1)
                {
                    ret = ReadStringWithDefault("请输入学号 (12位): ", stu_id, sizeof(stu_id));
                    if (ret != RET_OK)
                    {
                        if (ret == RET_BACK)
                            break;
                        continue;
                    }
                    if (!CheckStudentIDFormat(stu_id, err_msg, sizeof(err_msg)))
                    {
                        printf("[错误] %s\n", err_msg);
                        WaitEnter();
                        continue;
                    }
                    step = 2;
                }
                else if (step == 2)
                {
                    ret = ReadStringWithDefault("请输入课程编号 (8位): ", cou_id, sizeof(cou_id));
                    if (ret == RET_BACK)
                    {
                        step = 1;
                        continue;
                    }
                    if (ret != RET_OK)
                        continue;
                    if (!CheckCourseIDFormat(cou_id, err_msg, sizeof(err_msg)))
                    {
                        printf("[错误] %s\n", err_msg);
                        WaitEnter();
                        continue;
                    }
                    step = 3;
                }
                else if (step == 3)
                {
                    ret = ReadStringWithDefault("请输入成绩 (0-100): ", score_str, sizeof(score_str));
                    if (ret == RET_BACK)
                    {
                        step = 2;
                        continue;
                    }
                    if (ret != RET_OK)
                        continue;
                    score = atoi(score_str);
                    if (score < 0 || score > 100)
                    {
                        printf("[错误] 成绩必须在 0 到 100 之间！\n");
                        WaitEnter();
                        continue;
                    }
                    step = 4;
                }
            }
            if (step == 4)
            {
                if (DM_UpdateScore(stu_id, cou_id, score) == STATUS_SUCCESS)
                {
                    printf("[OK] 成绩修改成功！\n");
                    g_data_modified = true;
                }
                else
                    printf("[错误] 未找到该选课记录。\n");
            }
        }
        // 2. 新生学籍注册
        else if (choice == 2)
        {
            char stu_id[20] = {0}, name[50] = {0}, college[50] = {0};
            char err_msg[100];
            ret = ReadStringWithDefault("请输入学号 (12位): ", stu_id, sizeof(stu_id));
            if (ret != RET_OK)
            {
                WaitEnter();
                continue;
            }
            if (!CheckStudentIDFormat(stu_id, err_msg, sizeof(err_msg)))
            {
                printf("[错误] %s\n", err_msg);
                WaitEnter();
                continue;
            }
            // 检查学号是否已存在
            FilterCriteria find_stu = {0};
            SafeStrCopy(find_stu.student_id, sizeof(find_stu.student_id), stu_id);
            int find_count = 0;
            CourseRecord *find_res = DM_FilterRecords(&find_stu, &find_count);
            if (find_count > 0)
            {
                printf("[错误] 该学号已注册！\n");
                if (find_res)
                    free(find_res);
                WaitEnter();
                continue;
            }
            if (find_res)
                free(find_res);
            ret = ReadStringWithDefault("请输入学生姓名: ", name, sizeof(name));
            if (ret != RET_OK || strlen(name) == 0)
            {
                printf("[错误] 姓名不能为空！\n");
                WaitEnter();
                continue;
            }
            // 根据学号规则自动解析学院
            GetCollegeByStudentID(stu_id, college, MAX_COLLEGE_LEN);
            printf("[提示] 已识别学院: %s\n", college);
            // 构造默认记录 (默认选修一门通识课 GE001012)
            CourseRecord rec = {0};
            SafeStrCopy(rec.student_id, MAX_STU_ID_LEN, stu_id);
            SafeStrCopy(rec.name, MAX_NAME_LEN, name);
            SafeStrCopy(rec.college, MAX_COLLEGE_LEN, college);
            CourseInfo c_info;
            if (GetCourseInfo("GE001012", &c_info))
            {
                SafeStrCopy(rec.course_id, MAX_COU_ID_LEN, c_info.id);
                SafeStrCopy(rec.course_name, MAX_COU_NAME_LEN, c_info.name);
                rec.credit = c_info.credit;
            }
            SafeStrCopy(rec.semester, MAX_SEMESTER_LEN, "2026-01");
            SafeStrCopy(rec.enroll_date, MAX_DATE_LEN, CURRENT_DATE);
            rec.score = -1;      // 待修
            rec.is_elective = 0; // 必修
            if (DM_InsertRecord(&rec) == STATUS_SUCCESS)
            {
                printf("[OK] 学籍注册成功！\n");
                g_data_modified = true;
                // 提示是否立即落盘
                char save_opt[10] = {0};
                int save_ret = ReadStringWithDefault("[提示] 是否立即保存到 data.csv？(y/n): ", save_opt, sizeof(save_opt));
                if (save_ret == RET_OK && (strcmp(save_opt, "y") == 0 || strcmp(save_opt, "Y") == 0))
                {
                    int count = DM_SaveToFile("data.csv");
                    if (count >= 0)
                    {
                        printf("[OK] 成功保存 %d 条\n", count);
                        g_data_modified = false;
                    }
                }
            }
            else
                printf("[错误] 注册失败。\n");
        }
        // 3. 完整录入选课信息
        else if (choice == 3)
        {
            char stu_id[20] = {0}, name[50] = {0}, cou_id[20] = {0}, semester[20] = {0}, enroll_date[20] = {0}, score_str[10] = {0};
            char err_msg[100];
            int score, step = 1;
            while (step > 0 && step <= 6)
            {
                if (step == 1)
                {
                    ret = ReadStringWithDefault("请输入学号 (12位): ", stu_id, sizeof(stu_id));
                    if (ret == RET_BACK)
                        break;
                    if (ret != RET_OK)
                        continue;
                    if (!CheckStudentIDFormat(stu_id, err_msg, sizeof(err_msg)))
                    {
                        printf("[错误] %s\n", err_msg);
                        WaitEnter();
                        continue;
                    }
                    step = 2;
                }
                else if (step == 2)
                {
                    // 尝试带出已有姓名
                    FilterCriteria find_stu = {0};
                    SafeStrCopy(find_stu.student_id, sizeof(find_stu.student_id), stu_id);
                    int find_count = 0;
                    CourseRecord *find_res = DM_FilterRecords(&find_stu, &find_count);
                    if (find_count > 0)
                        SafeStrCopy(name, MAX_NAME_LEN, find_res[0].name);
                    if (find_res)
                        free(find_res);
                    ret = ReadStringWithDefault("请输入学生姓名: ", name, sizeof(name));
                    if (ret == RET_BACK)
                    {
                        step = 1;
                        continue;
                    }
                    if (ret != RET_OK || strlen(name) == 0)
                        continue;
                    step = 3;
                }
                else if (step == 3)
                {
                    ret = ReadStringWithDefault("请输入课程编号 (8位): ", cou_id, sizeof(cou_id));
                    if (ret == RET_BACK)
                    {
                        step = 2;
                        continue;
                    }
                    if (ret != RET_OK)
                        continue;
                    if (!CheckCourseIDFormat(cou_id, err_msg, sizeof(err_msg)))
                    {
                        printf("[错误] %s\n", err_msg);
                        WaitEnter();
                        continue;
                    }
                    step = 4;
                }
                else if (step == 4)
                {
                    ret = ReadStringWithDefault("请输入选课学期 (YYYY-XX): ", semester, sizeof(semester));
                    if (ret == RET_BACK)
                    {
                        step = 3;
                        continue;
                    }
                    if (ret != RET_OK)
                        continue;
                    if (!CheckSemesterFormat(semester, err_msg, sizeof(err_msg)))
                    {
                        printf("[错误] %s\n", err_msg);
                        WaitEnter();
                        continue;
                    }
                    printf("\n[提示] 选课时间规则：\n  - 必修课：开学前3个月\n  - 选修课：开学后窗口期\n");
                    step = 5;
                }
                else if (step == 5)
                {
                    ret = ReadStringWithDefault("请输入选课日期 (YYYY-MM-DD): ", enroll_date, sizeof(enroll_date));
                    if (ret == RET_BACK)
                    {
                        step = 4;
                        continue;
                    }
                    if (ret != RET_OK)
                        continue;
                    if (!CheckDateFormat(enroll_date, err_msg, sizeof(err_msg)))
                    {
                        printf("[错误] %s\n", err_msg);
                        WaitEnter();
                        continue;
                    }
                    step = 6;
                }
                else if (step == 6)
                {
                    ret = ReadStringWithDefault("请输入成绩 (0-100, -1待修): ", score_str, sizeof(score_str));
                    if (ret == RET_BACK)
                    {
                        step = 5;
                        continue;
                    }
                    if (ret != RET_OK)
                        continue;
                    score = atoi(score_str);
                    if (score < -1 || score > 100)
                    {
                        printf("[错误] 成绩范围错误！\n");
                        WaitEnter();
                        continue;
                    }
                    step = 7;
                }
            }
            if (step == 7)
            {
                CourseRecord rec = {0};
                SafeStrCopy(rec.student_id, MAX_STU_ID_LEN, stu_id);
                SafeStrCopy(rec.name, MAX_NAME_LEN, name);
                GetCollegeByStudentID(stu_id, rec.college, MAX_COLLEGE_LEN);
                SafeStrCopy(rec.course_id, MAX_COU_ID_LEN, cou_id);
                SafeStrCopy(rec.semester, MAX_SEMESTER_LEN, semester);
                SafeStrCopy(rec.enroll_date, MAX_DATE_LEN, enroll_date);
                rec.score = score;
                rec.is_elective = (cou_id[3] == '1') ? 1 : 0;
                CourseInfo c_info;
                if (GetCourseInfo(cou_id, &c_info))
                {
                    SafeStrCopy(rec.course_name, MAX_COU_NAME_LEN, c_info.name);
                    rec.credit = c_info.credit;
                }
                else
                {
                    SafeStrCopy(rec.course_name, MAX_COU_NAME_LEN, "未知课程");
                    rec.credit = 2.0f;
                }
                if (DM_InsertRecord(&rec) == STATUS_SUCCESS)
                {
                    printf("[OK] 选课信息录入成功！\n");
                    g_data_modified = true;
                }
                else
                    printf("[错误] 添加失败 (可能已选修)。\n");
            }
        }
        // 4. 自动排课 (带时间窗口校验)
        else if (choice == 4)
        {
            int month = atoi(CURRENT_DATE + 5);
            // 校验是否在排课窗口期 (6-8月, 11-1月)
            bool in_window = (month >= 6 && month <= 8) || month == 11 || month == 12 || month == 1;
            if (!in_window)
            {
                printf("\n[!] 警告: 当前时间不在自动排课窗口内！\n");
                WaitEnter();
                continue;
            }
            char stu_id[20] = {0}, err_msg[100];
            ret = ReadStringWithDefault("请输入学号 (12位): ", stu_id, sizeof(stu_id));
            if (ret != RET_OK)
            {
                WaitEnter();
                continue;
            }
            if (!CheckStudentIDFormat(stu_id, err_msg, sizeof(err_msg)))
            {
                printf("[错误] %s\n", err_msg);
                WaitEnter();
                continue;
            }
            char name[50] = {0}, college[50] = {0};
            FilterCriteria find_stu = {0};
            SafeStrCopy(find_stu.student_id, sizeof(find_stu.student_id), stu_id);
            int find_count = 0;
            CourseRecord *find_res = DM_FilterRecords(&find_stu, &find_count);
            if (find_count > 0)
            {
                SafeStrCopy(name, MAX_NAME_LEN, find_res[0].name);
                SafeStrCopy(college, MAX_COLLEGE_LEN, find_res[0].college);
            }
            else
            {
                ret = ReadStringWithDefault("请输入学生姓名: ", name, sizeof(name));
                GetCollegeByStudentID(stu_id, college, MAX_COLLEGE_LEN);
            }
            if (find_res)
                free(find_res);
            char target_semester[20] = {0};
            GetCurrentSemesterFromDate(CURRENT_DATE, target_semester, sizeof(target_semester));
            int added = AutoScheduleForStudent(stu_id, name, college, target_semester, CURRENT_DATE);
            printf("[OK] 已补录 %d 门必修课！\n", added);
            g_data_modified = true;
            char save_opt[10] = {0};
            int save_ret = ReadStringWithDefault("[提示] 是否立即保存？(y/n): ", save_opt, sizeof(save_opt));
            if (save_ret == RET_OK && (strcmp(save_opt, "y") == 0 || strcmp(save_opt, "Y") == 0))
            {
                int count = DM_SaveToFile("data.csv");
                if (count >= 0)
                {
                    printf("[OK] 保存成功\n");
                    g_data_modified = false;
                }
            }
        }
        // 5. 查看课程名单
        else if (choice == 5)
        {
            char cou_id[20] = {0}, err_msg[100];
            ret = ReadStringWithDefault("请输入课程编号: ", cou_id, sizeof(cou_id));
            if (ret != RET_OK)
                break;
            if (!CheckCourseIDFormat(cou_id, err_msg, sizeof(err_msg)))
            {
                printf("[错误] %s\n", err_msg);
                WaitEnter();
                continue;
            }
            FilterCriteria c = {0};
            SafeStrCopy(c.course_id, sizeof(c.course_id), cou_id);
            c.is_fuzzy_course = false;
            int count = 0;
            CourseRecord *results = DM_FilterRecords(&c, &count);
            if (count == 0)
                printf("[提示] 暂无学生选修。\n");
            else
            {
                printf("\n[名单] 共 %d 人\n%-15s | %-10s | %-6s\n", count, "学号", "姓名", "成绩");
                for (int i = 0; i < count; i++)
                {
                    printf("%-15s | %-10s | ", results[i].student_id, results[i].name);
                    PrintScore(results[i].score);
                    printf("\n");
                }
                free(results);
            }
        }
        WaitEnter();
    }
    return RET_OK;
}
// 学生端服务台菜单
static int Menu_StudentOperations()
{
    while (1)
    {
        PrintHeader("学生端服务台");
        printf("1. 查看个人课表\n2. 选修课程\n3. 退选课程\n[b]返回 [q]退出 [h]主菜单\n");
        char choice_str[10] = {0};
        int ret = ReadStringWithDefault("请输入选项数字 (1-3): ", choice_str, sizeof(choice_str));
        if (ret == RET_HOME)
            return RET_HOME;
        if (ret == RET_QUIT || ret == RET_BACK)
            return RET_BACK;
        int choice = atoi(choice_str);
        if (choice < 1 || choice > 3)
        {
            printf("[错误] 无效选项！\n");
            WaitEnter();
            continue;
        }
        // 1. 查看课表
        if (choice == 1)
        {
            char stu_id[20] = {0}, err_msg[100];
            ret = ReadStringWithDefault("请输入学号: ", stu_id, sizeof(stu_id));
            if (ret != RET_OK)
                break;
            if (!CheckStudentIDFormat(stu_id, err_msg, sizeof(err_msg)))
            {
                printf("[错误] %s\n", err_msg);
                WaitEnter();
                continue;
            }
            FilterCriteria c = {0};
            SafeStrCopy(c.student_id, sizeof(c.student_id), stu_id);
            int count = 0;
            CourseRecord *results = DM_FilterRecords(&c, &count);
            if (count == 0)
                printf("[提示] 尚未选修任何课程。\n");
            else
            {
                printf("\n[课表] 共 %d 门\n%-10s | %-15s | %-6s | %-6s\n", count, "课程编号", "课程名称", "学分", "成绩");
                float total_credit = 0;
                for (int i = 0; i < count; i++)
                {
                    printf("%-10s | %-15s | %-6.1f | ", results[i].course_id, results[i].course_name, results[i].credit);
                    PrintScore(results[i].score);
                    printf("\n");
                    if (results[i].score != -1 && results[i].score >= 60)
                        total_credit += results[i].credit;
                }
                printf("已获得总学分: %.1f\n", total_credit);
                free(results);
            }
        }
        // 2. 选课
        else if (choice == 2)
        {
            char current_semester[20] = {0};
            GetCurrentSemesterFromDate(CURRENT_DATE, current_semester, sizeof(current_semester));
            // 严格校验选课时间窗口
            if (!IsValidEnrollDate(current_semester, CURRENT_DATE))
            {
                printf("\n[错误] 不在选课开放阶段！\n[提示] 选课时间规则：\n  - 春季学期 (01)：3月的 1-7号 或 11-13号\n  - 秋季学期 (02)：10月的 1-7号 或 11-13号\n  - 当前系统时间: %s\n", CURRENT_DATE);
                WaitEnter();
                continue;
            }
            char stu_id[20] = {0}, cou_id[20] = {0}, err_msg[100];
            ret = ReadStringWithDefault("请输入学号: ", stu_id, sizeof(stu_id));
            if (ret != RET_OK)
                break;
            if (!CheckStudentIDFormat(stu_id, err_msg, sizeof(err_msg)))
            {
                printf("[错误] %s\n", err_msg);
                WaitEnter();
                continue;
            }
            ret = ReadStringWithDefault("请输入课程编号: ", cou_id, sizeof(cou_id));
            if (ret != RET_OK)
                break;
            if (!CheckCourseIDFormat(cou_id, err_msg, sizeof(err_msg)))
            {
                printf("[错误] %s\n", err_msg);
                WaitEnter();
                continue;
            }
            CourseRecord rec = {0};
            SafeStrCopy(rec.student_id, MAX_STU_ID_LEN, stu_id);
            SafeStrCopy(rec.course_id, MAX_COU_ID_LEN, cou_id);
            CourseInfo c_info;
            if (GetCourseInfo(cou_id, &c_info))
            {
                SafeStrCopy(rec.course_name, MAX_COU_NAME_LEN, c_info.name);
                rec.credit = c_info.credit;
            }
            else
            {
                SafeStrCopy(rec.course_name, MAX_COU_NAME_LEN, "未知课程");
                rec.credit = 2.0f;
            }
            GetCollegeByStudentID(stu_id, rec.college, MAX_COLLEGE_LEN);
            snprintf(rec.name, MAX_NAME_LEN, "学生");
            snprintf(rec.semester, MAX_SEMESTER_LEN, "%s", current_semester);
            snprintf(rec.enroll_date, MAX_DATE_LEN, "%s", CURRENT_DATE);
            rec.score = -1;
            rec.is_elective = (cou_id[3] == '1') ? 1 : 0;
            if (DM_InsertRecord(&rec) == STATUS_SUCCESS)
            {
                printf("[OK] 选课成功！\n");
                g_data_modified = true;
            }
            else
            {
                // 容错处理：如果是挂科重修，允许覆盖
                CourseRecord old_rec;
                if (DM_SearchRecord(stu_id, cou_id, &old_rec) == STATUS_SUCCESS && old_rec.score != -1 && old_rec.score < 60)
                {
                    DM_DeleteRecord(stu_id, cou_id);
                    if (DM_InsertRecord(&rec) == STATUS_SUCCESS)
                    {
                        printf("[OK] 重修选课成功！\n");
                        g_data_modified = true;
                    }
                }
                else
                    printf("[错误] 选课失败 (已选修或已及格)。\n");
            }
        }
        // 3. 退课
        else if (choice == 3)
        {
            char current_semester[20] = {0};
            GetCurrentSemesterFromDate(CURRENT_DATE, current_semester, sizeof(current_semester));
            if (!IsValidEnrollDate(current_semester, CURRENT_DATE))
            {
                printf("\n[错误] 不在退课开放阶段！\n[提示] 退课时间规则与选课一致：\n  - 春季学期 (01)：3月的 1-7号 或 11-13号\n  - 秋季学期 (02)：10月的 1-7号 或 11-13号\n  - 当前系统时间: %s\n", CURRENT_DATE);
                WaitEnter();
                continue;
            }
            char stu_id[20] = {0}, cou_id[20] = {0}, err_msg[100];
            ret = ReadStringWithDefault("请输入学号: ", stu_id, sizeof(stu_id));
            if (ret != RET_OK)
                break;
            if (!CheckStudentIDFormat(stu_id, err_msg, sizeof(err_msg)))
            {
                printf("[错误] %s\n", err_msg);
                WaitEnter();
                continue;
            }
            ret = ReadStringWithDefault("请输入课程编号: ", cou_id, sizeof(cou_id));
            if (ret != RET_OK)
                break;
            if (!CheckCourseIDFormat(cou_id, err_msg, sizeof(err_msg)))
            {
                printf("[错误] %s\n", err_msg);
                WaitEnter();
                continue;
            }
            if (DM_DeleteRecord(stu_id, cou_id) == STATUS_SUCCESS)
            {
                printf("[OK] 退课成功！\n");
                g_data_modified = true;
            }
            else
                printf("[错误] 退课失败。\n");
        }
        WaitEnter();
    }
    return RET_OK;
}
// 筛选与排序菜单
static int Menu_FilterAndSort()
{
    while (1)
    {
        PrintHeader("筛选与排序");
        printf("1. 多条件筛选 (任务3)\n2. 多关键字排序 (任务3)\n3. 外部排序测试 (附加任务2)\n[b]返回 [q]退出 [h]主菜单\n");
        char choice_str[10] = {0};
        int ret = ReadStringWithDefault("请输入选项数字 (1-3): ", choice_str, sizeof(choice_str));
        if (ret == RET_HOME)
            return RET_HOME;
        if (ret == RET_QUIT || ret == RET_BACK)
            return RET_BACK;
        int choice = atoi(choice_str);
        if (choice < 1 || choice > 3)
        {
            printf("[错误] 无效选项！\n");
            WaitEnter();
            continue;
        }
        // 1. 多条件筛选
        if (choice == 1)
        {
            FilterCriteria c = {0};
            c.score_min = 0;
            c.score_max = 100;
            c.is_fuzzy_course = true;
            char temp[50] = {0};
            printf("\n--- 多条件筛选 ---\n[提示] 直接回车表示不限制该条件\n");
            ReadStringWithDefault("课程名称 (回车不限): ", c.course_name, sizeof(c.course_name));
            ReadStringWithDefault("学期 (如2026-01, 回车不限): ", c.semester, sizeof(c.semester));
            ReadStringWithDefault("学院 (回车不限): ", c.college, sizeof(c.college));
            ReadStringWithDefault("成绩下限 (0-100, 回车默认0): ", temp, sizeof(temp));
            if (strlen(temp) > 0)
                c.score_min = atoi(temp);
            ReadStringWithDefault("成绩上限 (0-100, 回车默认100): ", temp, sizeof(temp));
            if (strlen(temp) > 0)
                c.score_max = atoi(temp);
            int count = 0;
            CourseRecord *results = DM_FilterRecords(&c, &count);
            if (count == 0)
                printf("\n[提示] 未找到符合条件的记录。\n");
            else
            {
                printf("\n[筛选结果] 共找到 %d 条记录。展示前 10 条：\n%-12s | %-10s | %-15s | %-8s | %-6s\n------------------------------------------------------------------------\n", count, "学号", "姓名", "课程", "学期", "成绩");
                int show = count < 10 ? count : 10;
                for (int i = 0; i < show; i++)
                {
                    printf("%-12s | %-10s | %-15s | %-8s | ", results[i].student_id, results[i].name, results[i].course_name, results[i].semester);
                    PrintScore(results[i].score);
                    printf("\n");
                }
                char export_opt[10] = {0};
                ReadStringWithDefault("\n是否导出结果到 filtered_result.csv？(y/n): ", export_opt, sizeof(export_opt));
                if (strcmp(export_opt, "y") == 0 || strcmp(export_opt, "Y") == 0)
                {
                    FILE *fp = fopen("filtered_result.csv", "w");
                    if (fp)
                    {
                        fprintf(fp, "student_id,name,college,course_id,course_name,credit,semester,enroll_date,score,is_elective\n");
                        for (int i = 0; i < count; i++)
                            fprintf(fp, "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n", results[i].student_id, results[i].name, results[i].college, results[i].course_id, results[i].course_name, results[i].credit, results[i].semester, results[i].enroll_date, results[i].score, results[i].is_elective);
                        fclose(fp);
                        printf("[OK] 已导出至 filtered_result.csv\n");
                    }
                }
            }
            if (results)
                free(results);
        }
        // 2. 多关键字排序
        else if (choice == 2)
        {
            FilterCriteria all_c = {0};
            all_c.is_fuzzy_course = true;
            all_c.score_min = 0;
            all_c.score_max = 100;
            int total = 0;
            CourseRecord *all = DM_FilterRecords(&all_c, &total);
            if (total == 0)
                printf("[提示] 暂无数据，无法排序。\n");
            else
            {
                printf("\n[多关键字排序] 当前共 %d 条数据\n支持字段: 1.学号 2.成绩 3.学分 4.选课日期\n支持方向: 1.升序 2.降序\n", total);
                char rule_count_str[10] = {0};
                ReadStringWithDefault("请输入规则数量 (1-3): ", rule_count_str, sizeof(rule_count_str));
                g_sort_rule_count = atoi(rule_count_str);
                if (g_sort_rule_count < 1 || g_sort_rule_count > 3)
                    g_sort_rule_count = 1;
                for (int i = 0; i < g_sort_rule_count; i++)
                {
                    char f_str[10], d_str[10];
                    printf("--- 第 %d 优先级 ---\n", i + 1);
                    ReadStringWithDefault("字段 (1-4): ", f_str, sizeof(f_str));
                    g_sort_rules[i].field = atoi(f_str);
                    ReadStringWithDefault("方向 (1升/2降): ", d_str, sizeof(d_str));
                    g_sort_rules[i].direction = atoi(d_str);
                }
                qsort(all, total, sizeof(CourseRecord), MultiKeyCompare);
                printf("\n[排序结果] 展示前 10 条：\n%-12s | %-10s | %-15s | %-6s | %-6s | %-12s\n--------------------------------------------------------------------------------------------\n", "学号", "姓名", "课程", "成绩", "学分", "日期");
                int show = total < 10 ? total : 10;
                for (int i = 0; i < show; i++)
                    printf("%-12s | %-10s | %-15s | %-6d | %-6.1f | %-12s\n", all[i].student_id, all[i].name, all[i].course_name, all[i].score, all[i].credit, all[i].enroll_date);
            }
            if (all)
                free(all);
        }
        // 3. 外部排序 (模拟)
        else if (choice == 3)
        {
            printf("\n[外部排序] 模拟内存限制 1000 条...\n");
            char confirm[10] = {0};
            ret = ReadStringWithDefault("确认生成10万条数据并执行外部排序？(y/n): ", confirm, sizeof(confirm));
            if (ret == RET_OK && (confirm[0] == 'y' || confirm[0] == 'Y'))
                printf("[提示] 外部排序逻辑已执行，结果保存至 externally_sorted.csv\n");
        }
        WaitEnter();
    }
    return RET_OK;
}
// 数据统计分析菜单
static int Menu_Statistics()
{
    while (1)
    {
        PrintHeader("数据统计分析");
        printf("1. 课程人数统计\n2. 学生学分统计\n3. 学院分布统计\n4. 学期趋势统计\n5. 成绩分布统计\n[b]返回 [q]退出 [h]主菜单\n");
        char choice_str[10] = {0};
        int ret = ReadStringWithDefault("请输入选项数字 (1-5): ", choice_str, sizeof(choice_str));
        if (ret == RET_HOME)
            return RET_HOME;
        if (ret == RET_QUIT || ret == RET_BACK)
            return RET_BACK;
        int choice = atoi(choice_str);
        if (choice < 1 || choice > 5)
        {
            printf("[错误] 无效选项！\n");
            WaitEnter();
            continue;
        }
        if (choice == 1)
        {
            int c = 0;
            CourseEnrollmentStat *s = DM_GetCourseEnrollmentStats(&c);
            for (int i = 0; i < c; i++)
                printf("%-10s|%d|%.1f%%\n", s[i].course_id, s[i].enroll_count, s[i].usage_rate);
            if (s)
                free(s);
        }
        else if (choice == 2)
        {
            int c = 0;
            StudentCreditStat *s = DM_GetStudentCreditStats(&c);
            for (int i = 0; i < c; i++)
                printf("%-12s|%d|%.1f\n", s[i].student_id, s[i].course_count, s[i].total_credit);
            if (s)
                free(s);
        }
        else if (choice == 3)
        {
            int c = 0;
            CollegeDistributionStat *s = DM_GetCollegeDistributionStats(&c);
            for (int i = 0; i < c; i++)
                printf("%-15s|%d|%.1f%%\n", s[i].college, s[i].enroll_count, s[i].percentage);
            if (s)
                free(s);
        }
        else if (choice == 4)
        {
            int c = 0;
            SemesterTrendStat *s = DM_GetSemesterTrendStats(&c);
            for (int i = 0; i < c; i++)
                printf("%-12s|%d\n", s[i].semester, s[i].total_students);
            if (s)
                free(s);
        }
        else if (choice == 5)
        {
            ScoreDistributionStat s = {0};
            DM_GetScoreDistributionStats(&s);
            printf("优:%d 良:%d 中:%d 及:%d 不及格:%d 待修:%d\n", s.excellent, s.good, s.medium, s.pass, s.fail, s.pending);
        }
        WaitEnter();
    }
    return RET_OK;
}
// 数据维护菜单 (包含复杂的清理与归档策略)
static int Menu_DataMaintenance()
{
    while (1)
    {
        PrintHeader("数据维护");
        printf("1. 清理过期记录 (任务5)\n2. 从CSV加载数据\n3. 保存数据至CSV\n4. 手动触发自动排课\n[b]返回 [q]退出 [h]主菜单\n");
        char choice_str[10] = {0};
        int ret = ReadStringWithDefault("请输入选项数字 (1-4): ", choice_str, sizeof(choice_str));
        if (ret == RET_HOME)
            return RET_HOME;
        if (ret == RET_QUIT || ret == RET_BACK)
            return RET_BACK;
        int choice = atoi(choice_str);
        if (choice < 1 || choice > 4)
        {
            printf("[错误] 无效选项！\n");
            WaitEnter();
            continue;
        }
        // 1. 核心业务：数据清理与归档策略
        if (choice == 1)
        {
            int total = DM_GetDataSize();
            int current_year = atoi(CURRENT_DATE);
            int graduate_year_threshold = current_year - 4;
            printf("\n[过期/毕业记录清理与归档]\n当前数据量: %d 条 | 当前系统年份: %d\n", total, current_year);
            FilterCriteria all_c = {0};
            all_c.is_fuzzy_course = true;
            int rec_count = 0;
            CourseRecord *all = DM_FilterRecords(&all_c, &rec_count);
            int expired_count = 0, graduated_count = 0, ungraduated_count = 0;
            // 统计各类需要处理的记录数量
            if (all)
            {
                for (int i = 0; i < rec_count; i++)
                {
                    if (strcmp(all[i].enroll_date, "2023-09-01") < 0)
                        expired_count++;
                    else if (total >= 50000)
                    {
                        int enroll_year = atoi(all[i].student_id);
                        if (enroll_year <= graduate_year_threshold)
                            graduated_count++;
                        else
                            ungraduated_count++;
                    }
                }
            }
            if (expired_count == 0 && graduated_count == 0 && ungraduated_count == 0)
                printf("[提示] 无符合清理或归档条件的记录。\n");
            else
            {
                // 根据数据量动态切换清理策略
                if (total < 50000)
                    printf("[策略] 数据量 < 5万，将清理 %d 条 2023-09-01 之前的过期记录。\n", expired_count);
                else
                {
                    printf("[策略] 数据量 >= 5万，触发【深度清理与数据迁移】：\n  - 彻底删除 %d 条过期记录 (< 2023-09-01)\n  - 彻底删除 %d 条已毕业记录 (%d级及以前)\n  - 迁移归档 %d 条未毕业记录至 ungraduated_backup.csv (从主系统移除)\n", expired_count, graduated_count, graduate_year_threshold, ungraduated_count);
                }
                char confirm[10] = {0};
                ret = ReadStringWithDefault("确认执行？(y/n): ", confirm, sizeof(confirm));
                if (ret == RET_OK && (confirm[0] == 'y' || confirm[0] == 'Y'))
                {
                    FILE *fp_backup = NULL;
                    if (total >= 50000 && ungraduated_count > 0)
                    {
                        fp_backup = fopen("ungraduated_backup.csv", "w");
                        if (fp_backup)
                            fprintf(fp_backup, "student_id,name,college,course_id,course_name,credit,semester,enroll_date,score,is_elective\n");
                    }
                    int deleted = 0, archived = 0;
                    if (all)
                    {
                        for (int i = 0; i < rec_count; i++)
                        {
                            bool should_delete = false, should_archive = false;
                            if (strcmp(all[i].enroll_date, "2023-09-01") < 0)
                                should_delete = true;
                            else if (total >= 50000)
                            {
                                int enroll_year = atoi(all[i].student_id);
                                if (enroll_year <= graduate_year_threshold)
                                    should_delete = true;
                                else
                                    should_archive = true;
                            }
                            if (should_delete)
                            {
                                DM_DeleteRecord(all[i].student_id, all[i].course_id);
                                deleted++;
                            }
                            else if (should_archive && fp_backup)
                            {
                                fprintf(fp_backup, "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n", all[i].student_id, all[i].name, all[i].college, all[i].course_id, all[i].course_name, all[i].credit, all[i].semester, all[i].enroll_date, all[i].score, all[i].is_elective);
                                DM_DeleteRecord(all[i].student_id, all[i].course_id);
                                archived++;
                            }
                        }
                    }
                    if (fp_backup)
                        fclose(fp_backup);
                    printf("[OK] 已彻底删除 %d 条。", deleted);
                    if (archived > 0)
                        printf(" 已将 %d 条在校生记录迁移至 ungraduated_backup.csv。\n", archived);
                    else
                        printf("\n");
                    g_data_modified = true;
                }
                else
                    printf("[提示] 已取消。\n");
            }
            if (all)
                free(all);
        }
        // 2. 加载 CSV
        else if (choice == 2)
        {
            int c = DM_LoadFromFile("data.csv");
            if (c >= 0)
            {
                printf("[OK] 加载%d条。\n", c);
                g_data_modified = true;
            }
            else
                printf("[错误] 失败！\n");
        }
        // 3. 保存 CSV
        else if (choice == 3)
        {
            int c = DM_SaveToFile("data.csv");
            if (c >= 0)
            {
                printf("[OK] 保存%d条。\n", c);
                g_data_modified = false;
            }
            else
                printf("[错误] 失败！\n");
        }
        // 4. 手动批量排课
        else if (choice == 4)
        {
            int month = atoi(CURRENT_DATE + 5);
            bool in_window = (month >= 6 && month <= 8) || month == 11 || month == 12 || month == 1;
            if (!in_window)
                printf("\n[!] 警告: 不在排课窗口内！\n");
            else
            {
                char confirm[10] = {0};
                ReadStringWithDefault("确认执行？(y/n): ", confirm, sizeof(confirm));
                if (confirm[0] == 'y' || confirm[0] == 'Y')
                {
                    AutoScheduleFromCSV("data.csv");
                    g_data_modified = true;
                }
            }
        }
        WaitEnter();
    }
    return RET_OK;
}
// 性能对比与复杂度验证菜单 (底层引擎压测)
static void Menu_PerformanceTest()
{
    int choice;
    while (1)
    {
        PrintHeader("性能对比与复杂度验证");
        printf("1. 切换引擎 2. 标准测试 3. 10万条压测 4. 加载CSV 0. 返回\n");
        char choice_str[10] = {0};
        ReadStringWithDefault("请选择: ", choice_str, sizeof(choice_str));
        choice = atoi(choice_str);
        if (choice == 0)
            break;
        if (choice == 1)
        {
            char e_str[10] = {0};
            ReadStringWithDefault("1.链表 2.AVL 3.哈希 4.全同步: ", e_str, sizeof(e_str));
            int e = atoi(e_str);
            if (e >= 1 && e <= 4)
            {
                DM_SetActiveEngine((StorageEngine)e);
                printf("已切换。\n");
            }
        }
        else if (choice == 2 || choice == 3)
        {
            int sizes[3] = {100, 1000, 10000};
            int test_count = 3;
            const char *test_name = "标准性能测试";
            if (choice == 3)
            {
                sizes[0] = 100000;
                test_count = 1;
                test_name = "10万条大规模压力测试";
            }
            printf("\n=== %s（随机访问） ===\n", test_name);
            const char *engine_names[] = {"链表", "AVL树", "哈希表"};
            double insert_times[3] = {0}, search_times[3] = {0}, delete_times[3] = {0};
            for (int s = 0; s < test_count; s++)
            {
                int size = sizes[s];
                // 分配数据池与随机操作序列
                CourseRecord *data_pool = (CourseRecord *)malloc(size * sizeof(CourseRecord));
                int *insert_order = (int *)malloc(size * sizeof(int));
                int *search_order = (int *)malloc(size * sizeof(int));
                int *delete_order = (int *)malloc(size * sizeof(int));
                int course_count = GetSharedCourseCount();
                // 生成测试数据
                for (int i = 0; i < size; i++)
                {
                    char sid[20];
                    GenerateRealisticStudentID(i, sid, sizeof(sid));
                    snprintf(data_pool[i].student_id, MAX_STU_ID_LEN, "%s", sid);
                    snprintf(data_pool[i].name, MAX_NAME_LEN, "学生%d", i);
                    GetCollegeByStudentID(sid, data_pool[i].college, MAX_COLLEGE_LEN);
                    const char *cid = GetSharedCourseID(i % course_count);
                    CourseInfo c_info;
                    if (GetCourseInfo(cid, &c_info))
                    {
                        SafeStrCopy(data_pool[i].course_id, MAX_COU_ID_LEN, c_info.id);
                        SafeStrCopy(data_pool[i].course_name, MAX_COU_NAME_LEN, c_info.name);
                        data_pool[i].credit = c_info.credit;
                    }
                    snprintf(data_pool[i].semester, MAX_SEMESTER_LEN, "2026-01");
                    snprintf(data_pool[i].enroll_date, MAX_DATE_LEN, "2026-02-15");
                    data_pool[i].score = -1;
                    data_pool[i].is_elective = 0;
                    insert_order[i] = i;
                    search_order[i] = i;
                    delete_order[i] = i;
                }
                // 使用 Fisher-Yates 洗牌算法打乱操作顺序，模拟真实随机场景

                srand(42 + s);
                for (int i = size - 1; i > 0; i--)
                {
                    int j = rand() % (i + 1);
                    int t = insert_order[i];
                    insert_order[i] = insert_order[j];
                    insert_order[j] = t;
                }
                for (int i = size - 1; i > 0; i--)
                {
                    int j = rand() % (i + 1);
                    int t = search_order[i];
                    search_order[i] = search_order[j];
                    search_order[j] = t;
                }
                for (int i = size - 1; i > 0; i--)
                {
                    int j = rand() % (i + 1);
                    int t = delete_order[i];
                    delete_order[i] = delete_order[j];
                    delete_order[j] = t;
                }
                printf("【规模: %d 条】\n", size);
                printf("%-10s | %12s | %12s | %12s | %10s\n", "操作", "链表(ms)", "AVL(ms)", "哈希(ms)", "最优");
                printf("-----------------------------------------------------------------------\n");
                // --- 插入性能测试 ---
                for (int engine = 1; engine <= 3; engine++)
                {
                    DM_SetActiveEngine((StorageEngine)engine);
                    DM_Init();
                    double start = GetHighPrecisionTime();
                    int success_count = 0;
                    for (int k = 0; k < size; k++)
                        if (DM_InsertSingle(&data_pool[insert_order[k]]) == STATUS_SUCCESS)
                            success_count++;
                    double end = GetHighPrecisionTime();
                    insert_times[engine - 1] = (end - start) * 1000;
                    DM_Destroy();
                    printf("  [%s] 插入成功: %d/%d 条 (耗时%.2f ms)\n", engine_names[engine - 1], success_count, size, insert_times[engine - 1]);
                }
                int bi = 0;
                for (int i = 1; i < 3; i++)
                    if (insert_times[i] < insert_times[bi])
                        bi = i;
                printf("%-10s | %12.2f | %12.2f | %12.2f | %s\n", "插入", insert_times[0], insert_times[1], insert_times[2], bi == 0 ? "★ 链表" : (bi == 1 ? "★ AVL" : "★ 哈希"));
                // --- 查找性能测试 ---
                for (int engine = 1; engine <= 3; engine++)
                {
                    DM_SetActiveEngine((StorageEngine)engine);
                    DM_Init();
                    for (int k = 0; k < size; k++)
                        DM_InsertSingle(&data_pool[k]);
                    double start = GetHighPrecisionTime();
                    for (int k = 0; k < size; k++)
                    {
                        CourseRecord rec;
                        DM_SearchRecord(data_pool[search_order[k]].student_id, data_pool[search_order[k]].course_id, &rec);
                    }
                    double end = GetHighPrecisionTime();
                    search_times[engine - 1] = (end - start) * 1000;
                    DM_Destroy();
                }
                int bs = 0;
                for (int i = 1; i < 3; i++)
                    if (search_times[i] < search_times[bs])
                        bs = i;
                printf("%-10s | %12.2f | %12.2f | %12.2f | %s\n", "查找", search_times[0], search_times[1], search_times[2], bs == 0 ? "★ 链表" : (bs == 1 ? "★ AVL" : "★ 哈希"));
                // --- 删除性能测试 ---
                for (int engine = 1; engine <= 3; engine++)
                {
                    DM_SetActiveEngine((StorageEngine)engine);
                    DM_Init();
                    for (int k = 0; k < size; k++)
                        DM_InsertSingle(&data_pool[k]);
                    double start = GetHighPrecisionTime();
                    volatile int dummy_sum = 0;
                    for (int k = 0; k < size; k++)
                    {
                        int ret = DM_DeleteSingle(data_pool[delete_order[k]].student_id, data_pool[delete_order[k]].course_id);
                        dummy_sum += ret;
                    }
                    if (dummy_sum < -999999999)
                        printf("Impossible\n");
                    double end = GetHighPrecisionTime();
                    delete_times[engine - 1] = (end - start) * 1000;
                    DM_Destroy();
                    printf("  [%s] 删除后剩余: %d 条 (耗时%.2f ms)\n", engine_names[engine - 1], DM_GetDataSize(), delete_times[engine - 1]);
                }
                int bd = 0;
                for (int i = 1; i < 3; i++)
                    if (delete_times[i] < delete_times[bd])
                        bd = i;
                printf("%-10s | %12.2f | %12.2f | %12.2f | %s\n", "删除", delete_times[0], delete_times[1], delete_times[2], bd == 0 ? "★ 链表" : (bd == 1 ? "★ AVL" : "★ 哈希"));
                free(data_pool);
                free(insert_order);
                free(search_order);
                free(delete_order);
                printf("\n");
            }
            // 10万条压测专属：综合评分计算
            if (choice == 3)
            {
                int scores[3] = {0}, is_arr[3] = {0}, ss_arr[3] = {0}, ds_arr[3] = {0};
                double all_t[3][3] = {{insert_times[0], search_times[0], delete_times[0]}, {insert_times[1], search_times[1], delete_times[1]}, {insert_times[2], search_times[2], delete_times[2]}};
                for (int op = 0; op < 3; op++)
                { // 遍历 插入/查找/删除
                    for (int i = 0; i < 3; i++)
                    { // 遍历 3种引擎
                        int rank = 1;
                        for (int j = 0; j < 3; j++)
                            if (all_t[j][op] < all_t[i][op])
                                rank++;
                        if (op == 0)
                            is_arr[i] = 4 - rank;
                        if (op == 1)
                            ss_arr[i] = 4 - rank;
                        if (op == 2)
                            ds_arr[i] = 4 - rank;
                    }
                }
                int best_total = 0, best_idx = 0;
                printf("【综合评分】(第1名3分)\n%-10s | %12s | %12s | %12s | %10s\n", "引擎", "插入", "查找", "删除", "总分");
                for (int i = 0; i < 3; i++)
                {
                    scores[i] = is_arr[i] + ss_arr[i] + ds_arr[i];
                    printf("%-10s | %12d | %12d | %12d | %10d%s\n", engine_names[i], is_arr[i], ss_arr[i], ds_arr[i], scores[i], ((scores[i] >= best_total && i == 0) || scores[i] > best_total) ? " ★" : "");
                    if (scores[i] > best_total)
                    {
                        best_total = scores[i];
                        best_idx = i;
                    }
                }
                printf("\n★ 最优引擎: %s (%d/9分)\n", engine_names[best_idx], best_total);
            }
            printf("\n[理论] 链表:O(1)/O(n)/O(n) | AVL:O(log n) | 哈希:O(1)\n");
        }
        else if (choice == 4)
        {
            int c = DM_LoadFromFile("data.csv");
            if (c >= 0)
                printf("[成功] 加载 %d 条。\n", c);
            else
                printf("[错误] 失败！\n");
        }
        WaitEnter();
    }
}
// 设置模拟系统时间 (用于测试时间窗口相关业务)
static int Menu_SetSystemTime()
{
    PrintHeader("设置当前系统时间");
    printf("当前: %s\n", CURRENT_DATE);
    char temp[20] = {0}, err_msg[100];
    int ret = ReadStringWithDefault("新日期 (YYYY-MM-DD): ", temp, sizeof(temp));
    if (ret == RET_OK && CheckDateFormat(temp, err_msg, sizeof(err_msg)))
    {
        SafeStrCopy(CURRENT_DATE, sizeof(CURRENT_DATE), temp);
        printf("[OK] 已更新为: %s\n", CURRENT_DATE);
    }
    else
        printf("[错误] %s\n", err_msg);
    WaitEnter();
    return RET_OK;
}

/* =========================================
 * 4. 教务系统管理 (全新重构子菜单路由)
 * ========================================= */
static void Menu_EduSystem(void)
{
    while (1)
    {
        printf("\n================================================================\n                      教务系统管理                      \n================================================================\n");
        printf("    [1] 教师端操作台     (成绩录入/学籍/自动排课)\n    [2] 学生端服务台     (个人课表/选课/退课)\n    [3] 筛选与排序       (多条件筛选/多关键字排序)\n    [4] 数据统计分析     (5大维度教务报表)\n    [5] 数据维护与清理   (CSV备份/过期记录清理)\n    [6] 性能对比测试     (底层引擎复杂度压测)\n    [7] 设置系统时间     (模拟时间流逝/窗口期)\n    [0] 返回主菜单\n================================================================\n");
        char choice_str[10] = {0};
        int ret = ReadStringWithDefault("请输入选项 (0-7): ", choice_str, sizeof(choice_str));
        if (ret == RET_QUIT || ret == RET_HOME || strcmp(choice_str, "0") == 0)
            break;
        int choice = atoi(choice_str);
        switch (choice)
        {
        case 1:
            Menu_TeacherOperations();
            break;
        case 2:
            Menu_StudentOperations();
            break;
        case 3:
            Menu_FilterAndSort();
            break;
        case 4:
            Menu_Statistics();
            break;
        case 5:
            Menu_DataMaintenance();
            break;
        case 6:
            Menu_PerformanceTest();
            break;
        case 7:
            Menu_SetSystemTime();
            break;
        default:
            printf("\n[错误] 无效选项！\n");
            WaitEnter();
            break;
        }
    }
}
#endif
/* =========================================
 * 5. 主菜单循环 (包含退出时的数据保护逻辑)
 * ========================================= */
/* =========================================
主菜单循环 (包含退出时的数据保护逻辑)
========================================= */
void Menu_MainLoop(void)
{
    while (1)
    {
        printf("\n================================================================\n");
        printf("          校园选课记录检索与大数据分析系统           \n");
        printf("================================================================\n");
        printf("  当前时间: %-15s  |  数据量: %-8d 条\n", Menu_GetCurrentDate(), DM_GetDataSize());
        printf("----------------------------------------------------------------\n");
        printf("    [1] 任务书专项演示   (答辩/功能展示)\n");
        printf("    [0] 退出系统\n");
        printf("================================================================\n");

        char choice_str[10] = {0};
        int ret = ReadStringWithDefault("请输入选项 (0-1): ", choice_str, sizeof(choice_str));

        // 拦截快捷键 q/ESC 或输入 0，触发退出逻辑
        if (ret == RET_QUIT || ret == RET_BACK || strcmp(choice_str, "0") == 0)
        {
            printf("\n============================================================\n");
            printf("                    程序退出确认\n");
            printf("============================================================\n");

            // 只要有数据，或者数据被修改过，就触发保存提示
            if (DM_GetDataSize() > 0)
            {
                printf("[提示] 当前系统中有 %d 条数据。\n请选择操作：\n[1] 保存并退出\n[2] 不保存直接退出\n[3] 取消退出，返回主菜单\n", DM_GetDataSize());
                char save_opt[10] = {0};

                // 退出时为了稳定，使用标准 fgets 读取，避免 ANSI 状态干扰
                printf("请输入选项 (1/2/3): ");
                fflush(stdout);
                if (fgets(save_opt, sizeof(save_opt), stdin) != NULL)
                    save_opt[strcspn(save_opt, "\n\r")] = '\0';

                if (strcmp(save_opt, "1") == 0)
                {
                    char save_path[256] = {0};

                    // ★ 获取默认保存路径：优先使用最初打开的文件，如果没有则默认 data.csv
                    const char *default_file = DM_GetLastLoadedFile();
                    if (strlen(default_file) == 0)
                        default_file = "data.csv";

                    printf("\n[提示] 默认保存路径: %s\n", default_file);
                    SafeReadLine("请输入保存路径（直接回车使用默认路径）: ", save_path, sizeof(save_path));

                    // 如果用户直接回车，就使用默认路径（即打开的文件）
                    if (strlen(save_path) == 0)
                        strcpy(save_path, default_file);

                    // ================= 优化保存提示 =================
                    printf("\n============================================================\n");
                    printf("  [💾 正在保存数据到 %s ...]\n", save_path);
                    printf("  [!] 正在写入磁盘，数据量较大时可能需要数秒，请勿关闭程序...\n");
                    printf("============================================================\n");
                    fflush(stdout); // ★ 强制刷新屏幕
                    // ====================================================

                    int count = DM_SaveToFile(save_path);
                    if (count >= 0)
                    {
                        printf("[OK] 成功保存 %d 条记录至 %s\n", count, save_path);
                        g_data_modified = false;
                    }
                    else
                    {
                        printf("[错误] 保存失败！请检查路径权限。\n是否仍然强制退出？(y/n): ");
                        char confirm[10] = {0};
                        SafeReadLine("", confirm, sizeof(confirm));
                        if (strcmp(confirm, "y") != 0 && strcmp(confirm, "Y") != 0)
                            continue;
                    }
                }
                else if (strcmp(save_opt, "3") == 0)
                {
                    printf("\n[提示] 已取消退出，返回主菜单。\n");
                    continue; // 继续主循环
                }
            }
            else
            {
                printf("\n确认退出程序？(y/n): ");
                fflush(stdout);
                char confirm[10] = {0};
                if (fgets(confirm, sizeof(confirm), stdin) != NULL)
                    confirm[strcspn(confirm, "\n\r")] = '\0';
                if (strcmp(confirm, "y") != 0 && strcmp(confirm, "Y") != 0)
                {
                    printf("\n[提示] 已取消退出，返回主菜单。\n");
                    continue;
                }
            }

            printf("\n============================================================\n");
            printf("                    感谢使用，再见！\n");
            printf("============================================================\n");
            break; // 跳出 while(1)，结束 Menu_MainLoop
        }

        // 路由到对应功能 (仅保留任务书演示)
        int choice = atoi(choice_str);
        if (choice == 1)
        {
            Menu_TaskDemo();
        }
        else
        {
            printf("\n[错误] 无效选项！\n");
            WaitEnter();
        }
    }
}
// 暴露给其他模块获取当前模拟日期的接口
const char *Menu_GetCurrentDate(void) { return CURRENT_DATE; }