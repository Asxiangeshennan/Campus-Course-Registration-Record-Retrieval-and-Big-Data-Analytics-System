/**
 * @file task_demo.c
 * @brief 任务书专项演示与高级控制台主程序
 *
 * 本文件实现了基于控制台的学生选课管理系统的核心交互逻辑。
 * 包含自定义的TUI（文本用户界面）输入引擎、多任务演示（增删改查、排序、统计、性能测试等）。
 * 支持UTF-8编码、ANSI转义序列（光标控制）、Tab键实时预览等高级控制台特性。
 */
#define _CRT_SECURE_NO_WARNINGS // 禁用MSVC编译器的安全函数警告(如strcpy, sprintf等)
#define _POSIX_C_SOURCE 199309L // 在POSIX系统(如Linux)中启用 clock_gettime 等高精度时间函数

#include "task_demo.h"
#include "data_manager.h"
#include "data_generator.h"
#include "external_sort.h"
#include "common.h"
#include "course_catalog.h"
#include "validator.h"
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <conio.h> // 用于 _getch() 无回显获取键盘输入
#include <stdbool.h>

/* =========================================
核心防冲突机制：宏定义重映射
将通用名称映射为带 TD_ 前缀的局部名称，防止与其他模块的同名函数冲突
========================================= */
#define EnableANSI TD_EnableANSI
#define ReadStringWithDefault TD_ReadStringWithDefault
#define ReadStringWithTabPreview TD_ReadStringWithTabPreview
#define WaitEnter TD_WaitEnter
#define ShowMatchingStudents TD_ShowMatchingStudents
#define ShowMatchingCourses TD_ShowMatchingCourses
#define COURSES_COUNT 20 // 模拟系统中的课程总数，用于生成测试数据

/* =========================================
导航状态码：用于统一处理输入引擎的返回结果
========================================= */
#define RET_OK 1          // 正常回车确认
#define RET_BACK -1       // 返回上一步 (输入 'b' 或按 ESC)
#define RET_QUIT -2       // 退出程序 (输入 'q')
#define RET_HOME -3       // 回到主菜单 (输入 'h')
#define PREVIEW_NONE 0    // 无预览
#define PREVIEW_STUDENT 1 // 学生信息预览
#define PREVIEW_COURSE 2  // 课程信息预览

/* =========================================
Windows API 手动声明
避免引入庞大的 <windows.h> 头文件，仅声明需要的控制台API以支持ANSI转义序列
========================================= */
#ifdef _WIN32
typedef void *HANDLE;
typedef unsigned long DWORD;
#define STD_OUTPUT_HANDLE ((DWORD) - 11)
// 启用虚拟终端处理标志，允许使用ANSI转义序列控制颜色和光标
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD nStdHandle);
__declspec(dllimport) int __stdcall GetConsoleMode(HANDLE hConsoleHandle, DWORD *lpMode);
__declspec(dllimport) int __stdcall SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode);
#endif

/* =========================================
辅助函数：UTF-8 字符长度与显示宽度计算
在终端中，英文字符占1个宽度，中文字符占2个宽度，但UTF-8编码下中文占3个字节。
此模块用于解决控制台光标移动时的对齐问题。
========================================= */

// 根据UTF-8首字节判断该字符占用的总字节数
static int utf8_char_len(unsigned char c)
{
    if (c >= 0xF0)
        return 4; // 4字节字符 (如部分Emoji)
    if (c >= 0xE0)
        return 3; // 3字节字符 (如常用中文)
    if (c >= 0xC0)
        return 2; // 2字节字符 (如带音标的拉丁字母)
    return 1;     // 1字节字符 (ASCII)
}
// 计算字符串在终端的实际显示宽度（中文算2，英文算1）
static int display_width(const char *str)
{
    int width = 0;
    while (*str)
    {
        unsigned char c = (unsigned char)*str;
        int len = utf8_char_len(c);
        if (len >= 3)
            width += 2; // 3字节及以上通常为中文字符，占2个显示宽度

        else
            width += 1;
        str += len;
    }
    return width;
}
// 辅助函数：按终端显示宽度打印字符串并补齐空格（完美解决中文对齐问题）
static void PrintPaddedString(const char *str, int target_width)
{
    if (!str)
        str = "";
    int current_width = display_width(str); // 计算实际显示宽度
    printf("%s", str);
    if (current_width < target_width)
    {
        for (int i = 0; i < target_width - current_width; i++)
            printf(" "); // 补齐剩余宽度的空格
    }
}
/* =========================================
核心输入引擎 (静态内部函数)
实现类似图形界面的单行输入体验：支持方向键移动光标、退格删除、实时刷新
========================================= */

// 开启Windows控制台的ANSI转义序列支持
static void EnableANSI()
{
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode))
    {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // 追加ANSI支持标志
        SetConsoleMode(hOut, dwMode);
    }
#endif
}
/**
 * @brief 核心单行输入函数（带默认值）
 * @param prompt 提示语
 * @param buffer 输入缓冲区（可包含默认值）
 * @param size 缓冲区大小
 * @return 导航状态码 (RET_OK, RET_BACK, RET_QUIT等)
 */
static int ReadStringWithDefault(const char *prompt, char *buffer, int size)
{
    static int ansi_enabled = 0;
    if (!ansi_enabled)
    {
        EnableANSI();
        ansi_enabled = 1;
    } // 首次调用时开启ANSI

    int len = strlen(buffer);
    int cursor = len; // 光标初始在末尾
    printf("%s%s", prompt, buffer);
    fflush(stdout);
    while (1)
    {
        int ch = _getch(); // 获取按键（无回显）
                           // 1. 处理方向键 (Windows下方向键会返回两个字节: 0或0xE0,  followed by 实际键码)

        if (ch == 0 || ch == 0xE0)
        {
            ch = _getch();
            if (ch == 75 && cursor > 0) // 左方向键
            {
                // 向左移动光标，需跳过UTF-8的延续字节(10xxxxxx)

                int prev = cursor - 1;
                while (prev >= 0 && ((unsigned char)buffer[prev] & 0xC0) == 0x80)
                    prev--;
                if (prev < 0)
                    prev = 0;
                cursor = prev;
            }
            else if (ch == 77 && cursor < len) // 右方向键
            {
                // 向右移动光标，需跳过一个完整的UTF-8字符
                int clen = utf8_char_len((unsigned char)buffer[cursor]);
                if (cursor + clen > len)
                    clen = len - cursor;
                cursor += clen;
            }
        }
        // 2. 处理回车键 (Enter)
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
        // 3. 处理 ESC 键 (取消/返回)

        else if (ch == 27)
        {
            printf("\n");
            fflush(stdout);
            return RET_BACK;
        }
        // 4. 处理退格键 (Backspace)
        else if (ch == 8)
        {
            if (cursor > 0)
            {
                // 找到前一个UTF-8字符的起始位置
                int prev = cursor - 1;
                while (prev >= 0 && ((unsigned char)buffer[prev] & 0xC0) == 0x80)
                    prev--;
                if (prev < 0)
                    prev = 0;
                int clen = cursor - prev; // 计算要删除的字节数
                // 内存移动：覆盖掉被删除的字符
                memmove(&buffer[cursor - clen], &buffer[cursor], len - cursor + 1);
                len -= clen;
                cursor -= clen;
            }
        }
        // 5. 处理普通可打印字符或UTF-8首字节
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
            // 读取完整的多字节字符
            char mb[4] = {(char)ch, 0, 0, 0};
            for (int i = 1; i < clen; i++)
            {
                int next_ch = _getch();
                if (next_ch == -1)
                    next_ch = _getch(); // 防止缓冲区空转

                mb[i] = (char)next_ch;
            }
            // 插入字符到buffer，并移动后续内存
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
        // === 核心：使用ANSI转义序列重绘当前行 ===
        // \033[?25l : 隐藏光标
        // \r : 回车到行首
        // \033[2K : 清除整行
        // \033[%dD : 光标向左移动N格，使其停留在正确的输入位置
        // \033[?25h : 显示光标
        printf("\033[?25l\r\033[2K%s%s", prompt, buffer);
        int back_steps = display_width(buffer + cursor);
        if (back_steps > 0)
            printf("\033[%dD", back_steps);
        printf("\033[?25h");
        fflush(stdout);
    }
}
// 预览辅助函数：根据关键词模糊匹配并打印学生信息
static void ShowMatchingStudents(const char *keyword)
{
    if (!keyword || strlen(keyword) == 0)
        return;
    FilterCriteria all_c = {0};
    all_c.is_fuzzy_course = true;
    all_c.score_min = -1;
    all_c.score_max = 100;
    int total = 0;
    CourseRecord *all = DM_FilterRecords(&all_c, &total);
    if (!all)
    {
        printf("[提示] 当前系统中暂无数据。\n");
        return;
    }
    int count = 0;
    printf("\n[匹配的学生] (最多显示10条)\n");
    printf("%-15s | %-10s | %-15s\n", "学号", "姓名", "学院");
    printf("--------------------------------------------------------\n");
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
// 预览辅助函数：根据关键词模糊匹配并打印课程信息
static void ShowMatchingCourses(const char *keyword)
{
    if (!keyword || strlen(keyword) == 0)
        return;
    printf("\n[匹配的课程] (最多显示10条)\n");
    printf("%-10s | %-20s | %-6s\n", "课程号", "课程名", "学分");
    printf("--------------------------------------------------------\n");
    int count = 0;
    int total = GetSharedCourseCount();
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
 * @brief 带Tab键实时预览功能的输入引擎
 * @param preview_type 预览类型 (PREVIEW_STUDENT 或 PREVIEW_COURSE)
 */
static int ReadStringWithTabPreview(const char *prompt, char *buffer, int size, int preview_type)
{
    static int ansi_enabled = 0;
    if (!ansi_enabled)
    {
        EnableANSI();
        ansi_enabled = 1;
    }
    int len = strlen(buffer);
    int cursor = len;
    printf("%s%s", prompt, buffer);
    fflush(stdout);
    while (1)
    {
        int ch = _getch();
        // 拦截 Tab 键 (ASCII 9)，触发预览逻辑
        if (ch == 9)
        {
            printf("\n");
            if (preview_type == PREVIEW_STUDENT)
                ShowMatchingStudents(buffer);
            else if (preview_type == PREVIEW_COURSE)
                ShowMatchingCourses(buffer);
            printf("%s%s", prompt, buffer); // 预览后重新打印输入行

            fflush(stdout);
            continue;
        }
        // 以下逻辑与 ReadStringWithDefault 完全一致，处理方向键、回车、退格、字符输入

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
        // ANSI重绘当前行
        printf("\033[?25l\r\033[2K%s%s", prompt, buffer);
        int back_steps = display_width(buffer + cursor);
        if (back_steps > 0)
            printf("\033[%dD", back_steps);
        printf("\033[?25h");
        fflush(stdout);
    }
}
// 暂停等待用户按键
static void WaitEnter()
{
    printf("\n[提示] 按任意键继续...");
    fflush(stdout);
    _getch();
}
// 获取高精度时间戳（秒），用于性能测试
static double GetHighPrecisionTime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

/* =========================================
辅助函数：打印测试用例与规则指南
========================================= */
static void PrintTestCasesGuide()
{
    printf("\n============================================================\n");
    printf("           [ 🧪 快速测试用例与规则参考 ]\n");
    printf("------------------------------------------------------------\n");
    printf(" 📌 学号规则 (12位): 年份(4)+院校(2)+专业(2)+班级(2)+序号(2)\n");
    printf("    院校代码: 01计算机 02电信 03机械 04土木 05数学 06经管 07外语 08公共\n");
    printf("    ▶ 示例1: 202501050112 (25年-计算机-05专业-01班-12号)\n");
    printf("    ▶ 示例2: 202406020328 (24年-经管-02专业-03班-28号)\n");
    printf("    ▶ 示例3: 202602010205 (26年-电信-01专业-02班-05号)\n");
    printf("------------------------------------------------------------\n");
    printf(" 📌 其他格式要求:\n");
    printf("    学期: 2025-1 (或 2025-2, 2026-1 等)\n");
    printf("    日期: 2025-09-01 (YYYY-MM-DD)\n");
    printf("    成绩: 85 (0-100) 或 -1 (待修)\n");
    printf("------------------------------------------------------------\n");
    printf(" 📌 智能拦截与重修规则 (核心考点):\n");
    printf("    1. 若课程已及格(>=60)或待修(-1)，系统将【拦截重复插入】。\n");
    printf("    2. 若课程不及格(<60)，允许【重修】，但新学期必须晚于原学期！\n");
    printf("============================================================\n");
}

/* =========================================
辅助函数：列出学生可修读/重修的课程
用于在插入记录时，给用户推荐当前可操作的课程
========================================= */
static void ShowAvailableCourses(const char *student_id)
{
    FilterCriteria all_c = {0};
    all_c.is_fuzzy_course = true;
    all_c.score_min = -1;
    all_c.score_max = 100;

    int total = 0;
    CourseRecord *recs = DM_FilterRecords(&all_c, &total);

    printf("\n[ 📚 该学生当前可修读/重修的课程推荐 ]\n");
    printf("------------------------------------------------------------\n");
    int count = 0;
    int course_total = GetSharedCourseCount();

    for (int i = 0; i < course_total; i++)
    {
        const char *cid = GetSharedCourseID(i);
        CourseInfo info;
        if (!GetCourseInfo(cid, &info))
            continue;

        bool can_take = true;
        const char *old_sem = NULL;
        int old_score = -2;
        // 遍历该学生已有的记录，判断课程状态
        if (recs)
        {
            for (int j = 0; j < total; j++)
            {
                if (strcmp(recs[j].student_id, student_id) == 0 && strcmp(recs[j].course_id, cid) == 0)
                {
                    if (recs[j].score >= 60 || recs[j].score == -1)
                        can_take = false;
                    else
                    {
                        old_sem = recs[j].semester;
                        old_score = recs[j].score;
                    }
                    break;
                }
            }
        }

        if (can_take)
        {
            if (old_score < 60 && old_score != -1)
                printf(" [重修] %-10s | %-20s | 原学期:%s (成绩:%d)\n", info.id, info.name, old_sem ? old_sem : "未知", old_score);
            else
                printf(" [可选] %-10s | %-20s | 学分:%.1f\n", info.id, info.name, info.credit);

            count++;
            if (count >= 8)
            {
                printf(" ... (更多课程请直接输入课程编号测试)\n");
                break;
            }
        }
    }
    if (count == 0)
        printf(" 🎉 暂无可选课程（已全部及格或正在修读）。\n");
    printf("------------------------------------------------------------\n");

    if (recs)
        free(recs);
}
/* =========================================
多关键字排序的全局比较规则配置
========================================= */
typedef struct
{
    int field;
    int direction;
} SortRule;
static SortRule g_sort_rules_temp[3];
static int g_sort_rule_count_temp = 0;
// qsort 使用的比较函数，根据全局配置的规则进行多关键字比较
static int TempMultiKeyCompare(const void *a, const void *b)
{
    const CourseRecord *r1 = (const CourseRecord *)a;
    const CourseRecord *r2 = (const CourseRecord *)b;
    for (int i = 0; i < g_sort_rule_count_temp; i++)
    {
        int cmp = 0;
        if (g_sort_rules_temp[i].field == 1)
            cmp = strcmp(r1->student_id, r2->student_id);
        else if (g_sort_rules_temp[i].field == 2)
            cmp = r1->score - r2->score;
        else if (g_sort_rules_temp[i].field == 3)
            cmp = (r1->credit > r2->credit) ? 1 : ((r1->credit < r2->credit) ? -1 : 0);
        else if (g_sort_rules_temp[i].field == 4)
            cmp = strcmp(r1->enroll_date, r2->enroll_date);
        if (g_sort_rules_temp[i].direction == 2)
            cmp = -cmp; // 降序则反转结果
        if (cmp != 0)
            return cmp; // 若当前关键字分出大小，直接返回；否则进入下一优先级
    }
    return 0;
}

/* =========================================
[任务1] 基础操作演示 (增删改查)
采用状态机(step)模式实现多步表单输入
========================================= */
static void Demo_BasicOperations()
{
    CourseRecord rec = {0};
    int choice;
    while (1)
    {
        printf("\n============================================================\n");
        printf("           [任务1] 基础操作演示 - 增删改查\n");
        printf("============================================================\n");
        printf(" 1. 插入选课记录 (智能识别已存在学号)\n");
        printf(" 2. 删除选课记录 (支持Tab预览)\n");
        printf(" 3. 修改成绩 (支持Tab预览)\n");
        printf(" 4. 查找记录 (支持Tab预览)\n");
        printf(" 5. 修改学生姓名 (批量同步历史记录)\n");
        printf(" 0. 返回上级菜单\n");
        printf("============================================================\n");
        printf(" [快捷键提示] b:返回上一步  h:回到主菜单  q:退出程序  ESC:取消输入\n");
        char choice_str[10] = {0};
        int ret = ReadStringWithDefault("请选择操作 (0-5):  ", choice_str, sizeof(choice_str));
        if (ret == RET_BACK || ret == RET_HOME)
            break;
        if (ret == RET_QUIT)
        {
            printf("\n[提示] 已返回主菜单，请在主菜单选择 [0] 退出以触发保存。\n");
            break; // ✅ 返回主菜单，让主菜单统一处理退出
        }

        choice = atoi(choice_str);
        if (choice == 0 && strcmp(choice_str, "0") != 0)
        {
            printf("\n[错误] 无效输入，请重新选择！\n");
            WaitEnter();
            continue;
        }
        if (choice == 0)
            break;
        // ================= 1. 插入记录 (核心状态机) =================

        if (choice == 1)
        {
            int step = 1;
            char score_str[10] = {0};
            char err_msg[100] = {0};
            bool is_retake = false;
            char old_semester[MAX_SEMESTER_LEN] = {0};
            bool skip_name_input = false;

            printf("\n============================================================\n");
            printf("           [ 插入选课记录 - 操作指南 ]\n");
            printf("============================================================\n");
            PrintTestCasesGuide();
            printf("按回车键开始录入...\n");
            WaitEnter();
            // 状态机循环：step 1~6 分别对应6个输入字段
            while (step > 0 && step <= 6)
            {
                if (step == 1) // 步骤1：学号
                {
                    printf("\n[步骤 1/6] 请输入学生学号 (12位数字)\n");
                    ret = ReadStringWithTabPreview("1. 学号 (Tab预览):  ", rec.student_id, MAX_STU_ID_LEN, PREVIEW_STUDENT);
                    if (ret == RET_HOME || ret == RET_QUIT)
                    {
                        step = 0;
                        break;
                    }
                    if (ret == RET_BACK)
                    {
                        step = 0;
                        break;
                    }
                    if (ret != RET_OK)
                        continue;

                    if (!CheckStudentIDFormat(rec.student_id, err_msg, sizeof(err_msg)))
                    {
                        printf("[错误] %s\n", err_msg);
                        continue;
                    }
                    // 智能识别：查询该学号是否已有记录
                    FilterCriteria exist_c = {0};
                    SafeStrCopy(exist_c.student_id, MAX_STU_ID_LEN, rec.student_id);
                    exist_c.is_fuzzy_course = true;
                    exist_c.score_min = -1;
                    exist_c.score_max = 100;
                    int exist_count = 0;
                    CourseRecord *exist_recs = DM_FilterRecords(&exist_c, &exist_count);
                    // 若存在，自动填充姓名和学院，并跳过步骤2

                    if (exist_count > 0)
                    {
                        SafeStrCopy(rec.name, MAX_NAME_LEN, exist_recs[0].name);
                        SafeStrCopy(rec.college, MAX_COLLEGE_LEN, exist_recs[0].college);
                        printf("\n[✓ 智能识别] 该学号已存在，自动匹配姓名: %s，学院: %s\n", rec.name, rec.college);
                        if (exist_recs)
                            free(exist_recs);
                        ShowAvailableCourses(rec.student_id);
                        skip_name_input = true;
                        step = 3; // 直接跳到步骤3
                    }
                    else
                    {
                        if (exist_recs)
                            free(exist_recs);
                        GetCollegeByStudentID(rec.student_id, rec.college, MAX_COLLEGE_LEN);
                        printf("[✓ 提示] 已根据学号自动识别学院: %s\n", rec.college);
                        ShowAvailableCourses(rec.student_id);
                        skip_name_input = false;
                        step = 2;
                    }
                }
                else if (step == 2) // 步骤2：姓名 (新生录入)
                {
                    printf("\n[步骤 2/6] 请输入学生姓名\n");
                    ret = ReadStringWithTabPreview("2. 姓名 (Tab预览):  ", rec.name, MAX_NAME_LEN, PREVIEW_STUDENT);
                    if (ret == RET_HOME || ret == RET_QUIT)
                    {
                        step = 0;
                        break;
                    }
                    if (ret == RET_BACK)
                    {
                        step = 1;
                        continue;
                    }
                    if (ret != RET_OK)
                        continue;
                    if (strlen(rec.name) == 0)
                    {
                        printf("[错误] 姓名不能为空！\n");
                        continue;
                    }
                    step = 3;
                }
                else if (step == 3) // 步骤3：课程编号
                {
                    printf("\n[步骤 3/6] 请输入课程编号 (8位)\n");
                    ret = ReadStringWithTabPreview("3. 课程编号 (Tab预览):  ", rec.course_id, MAX_COU_ID_LEN, PREVIEW_COURSE);
                    if (ret == RET_HOME || ret == RET_QUIT)
                    {
                        step = 0;
                        break;
                    }
                    if (ret == RET_BACK)
                    {
                        step = skip_name_input ? 1 : 2;
                        continue;
                    }
                    if (ret != RET_OK)
                        continue;

                    if (!CheckCourseIDFormat(rec.course_id, err_msg, sizeof(err_msg)))
                    {
                        printf("[错误] %s\n", err_msg);
                        continue;
                    }
                    // 重修拦截逻辑校验
                    CourseRecord old_rec;
                    int search_ret = DM_SearchRecord(rec.student_id, rec.course_id, &old_rec);
                    if (search_ret == STATUS_SUCCESS)
                    {
                        if (old_rec.score >= 60)
                        {
                            printf("\n[✗ 拦截] 该课程已及格 (成绩:%d)，无需重修！请重新输入。\n", old_rec.score);
                            continue;
                        }
                        else if (old_rec.score == -1)
                        {
                            printf("\n[✗ 拦截] 该课程正在修读中 (待修)，请勿重复选课！\n");
                            continue;
                        }
                        else
                        {
                            printf("\n[⚠ 重修] 检测到不及格记录 (成绩:%d, 学期:%s)，允许重修！\n", old_rec.score, old_rec.semester);
                            is_retake = true;
                            SafeStrCopy(old_semester, MAX_SEMESTER_LEN, old_rec.semester);
                        }
                    }
                    // 自动获取课程信息
                    CourseInfo c_info;
                    if (GetCourseInfo(rec.course_id, &c_info))
                    {
                        SafeStrCopy(rec.course_name, MAX_COU_NAME_LEN, c_info.name);
                        rec.credit = c_info.credit;
                        printf("[✓ 提示] 已自动识别课程: %s (学分: %.1f)\n", rec.course_name, rec.credit);
                    }
                    else
                    {
                        snprintf(rec.course_name, MAX_COU_NAME_LEN, "未知课程");
                        rec.credit = 2.0f;
                    }
                    rec.is_elective = (rec.course_id[3] == '1') ? 1 : 0;
                    step = 4;
                }
                else if (step == 4) // 步骤4：学期
                {
                    printf("\n[步骤 4/6] 请输入选课学期 (YYYY-XX)\n");
                    ret = ReadStringWithDefault("4. 选课学期:  ", rec.semester, MAX_SEMESTER_LEN);
                    if (ret == RET_HOME || ret == RET_QUIT)
                    {
                        step = 0;
                        break;
                    }
                    if (ret == RET_BACK)
                    {
                        step = 3;
                        continue;
                    }
                    if (ret != RET_OK)
                        continue;

                    if (!CheckSemesterFormat(rec.semester, err_msg, sizeof(err_msg)))
                    {
                        printf("[错误] %s\n", err_msg);
                        continue;
                    }
                    // 重修学期校验：新学期必须严格晚于旧学期

                    if (is_retake)
                    {
                        int y1, s1, y2, s2;
                        sscanf(old_semester, "%d-%d", &y1, &s1);
                        sscanf(rec.semester, "%d-%d", &y2, &s2);
                        if (y2 < y1 || (y2 == y1 && s2 <= s1))
                        {
                            printf("\n[✗ 错误] 重修学期必须晚于原学期 (%s)！请重新输入。\n", old_semester);
                            continue;
                        }
                    }
                    step = 5;
                }
                else if (step == 5) // 步骤5：日期
                {
                    printf("\n[步骤 5/6] 请输入选课日期 (YYYY-MM-DD)\n");
                    ret = ReadStringWithDefault("5. 选课日期:  ", rec.enroll_date, MAX_DATE_LEN);
                    if (ret == RET_HOME || ret == RET_QUIT)
                    {
                        step = 0;
                        break;
                    }
                    if (ret == RET_BACK)
                    {
                        step = 4;
                        continue;
                    }
                    if (ret != RET_OK)
                        continue;
                    if (!CheckDateFormat(rec.enroll_date, err_msg, sizeof(err_msg)))
                    {
                        printf("[错误] %s\n", err_msg);
                        continue;
                    }
                    step = 6;
                }
                else if (step == 6) // 步骤6：成绩
                {
                    printf("\n[步骤 6/6] 请输入课程成绩 (-1待修, 0-100)\n");
                    snprintf(score_str, sizeof(score_str), "%d", rec.score);
                    ret = ReadStringWithDefault("6. 成绩:  ", score_str, sizeof(score_str));
                    if (ret == RET_HOME || ret == RET_QUIT)
                    {
                        step = 0;
                        break;
                    }
                    if (ret == RET_BACK)
                    {
                        step = 5;
                        continue;
                    }
                    if (ret != RET_OK)
                        continue;
                    int temp_score = atoi(score_str);
                    if (temp_score != -1 && (temp_score < 0 || temp_score > 100))
                    {
                        printf("[错误] 成绩范围错误！\n");
                        continue;
                    }
                    rec.score = temp_score;
                    step = 7; // 标记完成
                }
            }
            // 提交数据
            if (step == 7)
            {
                printf("\n[正在提交] 正在将数据写入系统...\n");
                int insert_ret = DM_AdaptiveInsert(&rec);
                if (insert_ret == STATUS_SUCCESS)
                {
                    printf("\n[✓ 成功] 记录插入成功！\n");
                    printf("------------------------------------------------------------\n");
                    printf(" 学号: %s | 姓名: %s | 学院: %s\n", rec.student_id, rec.name, rec.college);
                    printf(" 课程: %s (%s) | 学分: %.1f | 性质: %s\n", rec.course_id, rec.course_name, rec.credit, rec.is_elective ? "选修" : "必修");
                    printf(" 学期: %s | 选课日期: %s | 成绩: %d\n", rec.semester, rec.enroll_date, rec.score);
                    printf("------------------------------------------------------------\n");
                    int current_size = DM_GetDataSize();
                    if (current_size < 5000)
                        printf(" 💡 [系统调度] 当前数据量 %d (<5000)，采用【三引擎全同步】模式。\n", current_size);
                    else
                        printf(" ⚡ [系统调度] 当前数据量 %d (>=5000)，已自动切换为【哈希表】单引擎模式。\n", current_size);
                    memset(&rec, 0, sizeof(rec));
                    g_data_modified = true; // <--- 添加这一行
                }
                else
                    printf("\n[✗ 失败] 记录已存在或插入失败！\n");
            }
            else
            {
                printf("\n[提示] 已取消插入操作。\n");
                memset(&rec, 0, sizeof(rec));
            }
        }
        // ================= 2. 删除记录 =================
        else if (choice == 2)
        {
            if (DM_GetDataSize() == 0)
                printf("\n[提示] 系统中暂无数据！\n");
            else
            {
                int step = 1;
                char stu_id[20] = {0};
                char cou_id[20] = {0};
                char confirm[20] = {0};
                char err_msg[100] = {0};
                printf("\n============================================================\n");
                printf("           [ 删除选课记录 - 操作指南 ]\n");
                printf("============================================================\n");
                PrintTestCasesGuide();
                WaitEnter();
                while (step > 0 && step <= 2)
                {
                    if (step == 1)
                    {
                        printf("\n[步骤 1/2] 请输入要删除的学生学号\n ");
                        ret = ReadStringWithTabPreview("1. 学号 (Tab预览):   ", stu_id, sizeof(stu_id), PREVIEW_STUDENT);
                        if (ret == RET_HOME || ret == RET_QUIT)
                        {
                            step = 0;
                            break;
                        }
                        if (ret == RET_BACK)
                        {
                            step = 0;
                            break;
                        }
                        if (ret != RET_OK)
                            continue;

                        if (!CheckStudentIDFormat(stu_id, err_msg, sizeof(err_msg)))
                        {
                            printf("[错误] %s\n ", err_msg);
                            continue;
                        }

                        // ================= 新增：展示该学生已选课程 =================
                        FilterCriteria stu_c = {0};
                        SafeStrCopy(stu_c.student_id, MAX_STU_ID_LEN, stu_id);
                        stu_c.is_fuzzy_course = true;
                        stu_c.score_min = -1;
                        stu_c.score_max = 100;

                        int stu_count = 0;
                        CourseRecord *stu_recs = DM_FilterRecords(&stu_c, &stu_count);

                        if (stu_count > 0)
                        {
                            printf("\n[提示] 该学生已选 %d 门课程，请从下方选择要删除的课程：\n", stu_count);
                            printf("------------------------------------------------------------\n");
                            printf(" %-10s | %-20s | %-6s | %-6s\n", "课程号", "课程名", "学分", "成绩");
                            printf("------------------------------------------------------------\n");
                            for (int i = 0; i < stu_count; i++)
                            {
                                char score_str[10];
                                if (stu_recs[i].score == -1)
                                    snprintf(score_str, sizeof(score_str), "待修");
                                else
                                    snprintf(score_str, sizeof(score_str), "%d", stu_recs[i].score);
                                printf(" %-10s | %-20s | %-6.1f | %-6s\n", stu_recs[i].course_id, stu_recs[i].course_name, stu_recs[i].credit, score_str);
                            }
                            printf("------------------------------------------------------------\n");
                        }
                        else
                        {
                            printf("\n[提示] 该学生暂无选课记录，无法删除。\n");
                            if (stu_recs)
                                free(stu_recs);
                            continue; // 无记录则重新输入学号或取消
                        }
                        if (stu_recs)
                            free(stu_recs);
                        // ==========================================================

                        step = 2;
                    }
                    else if (step == 2)
                    {
                        printf("\n[步骤 2/2] 请输入要删除的课程编号\n");
                        ret = ReadStringWithTabPreview("2. 课程编号 (Tab预览):  ", cou_id, sizeof(cou_id), PREVIEW_COURSE);
                        if (ret == RET_HOME || ret == RET_QUIT)
                        {
                            step = 0;
                            break;
                        }
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
                            continue;
                        }
                        step = 3;
                    }
                }
                if (step == 3)
                {
                    CourseRecord temp_rec;
                    if (DM_SearchRecord(stu_id, cou_id, &temp_rec) != STATUS_SUCCESS)
                        printf("\n[✗ 失败] 未找到该记录！\n");
                    else
                    {
                        printf("\n[即将删除] 学号:%s | 课程:%s\n", temp_rec.student_id, temp_rec.course_name);
                        ret = ReadStringWithDefault("确认删除？(输入 yes 确认):  ", confirm, sizeof(confirm));
                        if (ret == RET_OK && strcmp(confirm, "yes") == 0)
                        {
                            if (DM_DeleteSingle(stu_id, cou_id) == STATUS_SUCCESS)
                            {
                                printf("\n[✓ 成功] 删除成功！\n");
                                g_data_modified = true; // <--- 添加这一行
                            }
                            else
                                printf("\n[✗ 失败] 删除失败！\n");
                        }
                        else
                            printf("\n[提示] 已取消删除。\n");
                    }
                }
                else
                    printf("\n[提示] 已取消删除操作。\n");
            }
        }
        // ================= 3. 修改成绩 =================
        else if (choice == 3)
        {
            if (DM_GetDataSize() == 0)
                printf("\n[提示] 系统中暂无数据！\n");
            else
            {
                int step = 1;
                char stu_id[20] = {0};
                char cou_id[20] = {0};
                char score_str[10] = {0};
                char err_msg[100] = {0};
                printf("\n============================================================\n");
                printf("           [ 修改学生成绩 - 操作指南 ]\n");
                printf("============================================================\n");
                PrintTestCasesGuide();
                WaitEnter();
                while (step > 0 && step <= 3)
                {
                    if (step == 1)
                    {
                        printf("\n[步骤 1/3] 请输入学生学号\n ");
                        ret = ReadStringWithTabPreview("1. 学号 (Tab预览):   ", stu_id, sizeof(stu_id), PREVIEW_STUDENT);
                        if (ret == RET_HOME || ret == RET_QUIT)
                        {
                            step = 0;
                            break;
                        }
                        if (ret == RET_BACK)
                        {
                            step = 0;
                            break;
                        }
                        if (ret != RET_OK)
                            continue;

                        if (!CheckStudentIDFormat(stu_id, err_msg, sizeof(err_msg)))
                        {
                            printf("[错误] %s\n ", err_msg);
                            continue;
                        }

                        // ================= 新增：展示该学生已选课程 =================
                        FilterCriteria stu_c = {0};
                        SafeStrCopy(stu_c.student_id, MAX_STU_ID_LEN, stu_id);
                        stu_c.is_fuzzy_course = true;
                        stu_c.score_min = -1;
                        stu_c.score_max = 100;

                        int stu_count = 0;
                        CourseRecord *stu_recs = DM_FilterRecords(&stu_c, &stu_count);

                        if (stu_count > 0)
                        {
                            printf("\n[提示] 该学生已选 %d 门课程，请从下方选择要修改成绩的课程：\n", stu_count);
                            printf("------------------------------------------------------------\n");
                            printf(" %-10s | %-20s | %-6s | %-6s\n", "课程号", "课程名", "学分", "成绩");
                            printf("------------------------------------------------------------\n");
                            for (int i = 0; i < stu_count; i++)
                            {
                                char score_str[10];
                                if (stu_recs[i].score == -1)
                                    snprintf(score_str, sizeof(score_str), "待修");
                                else
                                    snprintf(score_str, sizeof(score_str), "%d", stu_recs[i].score);
                                printf(" %-10s | %-20s | %-6.1f | %-6s\n", stu_recs[i].course_id, stu_recs[i].course_name, stu_recs[i].credit, score_str);
                            }
                            printf("------------------------------------------------------------\n");
                        }
                        else
                        {
                            printf("\n[提示] 该学生暂无选课记录，无法修改成绩。\n");
                            if (stu_recs)
                                free(stu_recs);
                            continue;
                        }
                        if (stu_recs)
                            free(stu_recs);
                        // ==========================================================

                        step = 2;
                    }
                    else if (step == 2)
                    {
                        printf("\n[步骤 2/3] 请输入课程编号\n");
                        ret = ReadStringWithTabPreview("2. 课程编号 (Tab预览):  ", cou_id, sizeof(cou_id), PREVIEW_COURSE);
                        if (ret == RET_HOME || ret == RET_QUIT)
                        {
                            step = 0;
                            break;
                        }
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
                            continue;
                        }
                        step = 3;
                    }
                    else if (step == 3)
                    {
                        printf("\n[步骤 3/3] 请输入新成绩 (-1待修, 0-100)\n");
                        ret = ReadStringWithDefault("3. 新成绩:  ", score_str, sizeof(score_str));
                        if (ret == RET_HOME || ret == RET_QUIT)
                        {
                            step = 0;
                            break;
                        }
                        if (ret == RET_BACK)
                        {
                            step = 2;
                            continue;
                        }
                        if (ret != RET_OK)
                            continue;
                        int temp_score = atoi(score_str);
                        if (temp_score != -1 && (temp_score < 0 || temp_score > 100))
                        {
                            printf("[错误] 成绩范围错误！\n");
                            continue;
                        }
                        step = 4;
                    }
                }
                if (step == 4)
                {
                    if (DM_UpdateScore(stu_id, cou_id, atoi(score_str)) == STATUS_SUCCESS)
                    {
                        printf("\n[✓ 成功] 成绩修改成功！\n");
                        g_data_modified = true; // <--- 添加这一行
                    }
                    else
                        printf("\n[✗ 失败] 未找到该记录！\n");
                }
                else
                    printf("\n[提示] 已取消修改操作。\n");
            }
        }
        // ================= 4. 查找记录 =================
        else if (choice == 4)
        {
            if (DM_GetDataSize() == 0)
                printf("\n[提示] 系统中暂无数据！\n");
            else
            {
                char keyword[50] = {0};
                printf("\n============================================================\n");
                printf("           [ 查找选课记录 - 操作指南 ]\n");
                printf("============================================================\n");
                PrintTestCasesGuide();
                ret = ReadStringWithTabPreview("请输入关键词 (学号/姓名/课程名, Tab预览):  ", keyword, sizeof(keyword), PREVIEW_STUDENT);
                if (ret == RET_HOME || ret == RET_QUIT || ret == RET_BACK)
                {
                    printf("\n[提示] 已取消查找。\n");
                }
                else
                {
                    FilterCriteria all_c = {0};
                    all_c.is_fuzzy_course = true;
                    all_c.score_min = -1;
                    all_c.score_max = 100;
                    int total = 0;
                    CourseRecord *all = DM_FilterRecords(&all_c, &total);
                    if (all && total > 0)
                    {
                        int match_count = 0;
                        for (int i = 0; i < total; i++)
                            if (strstr(all[i].student_id, keyword) || strstr(all[i].course_name, keyword) || strstr(all[i].name, keyword))
                                match_count++;
                        if (match_count > 0)
                        {
                            CourseRecord *matched = (CourseRecord *)malloc(match_count * sizeof(CourseRecord));
                            int idx = 0;
                            for (int i = 0; i < total && idx < match_count; i++)
                                if (strstr(all[i].student_id, keyword) || strstr(all[i].course_name, keyword) || strstr(all[i].name, keyword))
                                    matched[idx++] = all[i];
                            printf("\n[查找结果] 找到 %d 条:\n", match_count);
                            printf("------------------------------------------------------------\n");
                            for (int i = 0; i < match_count; i++)
                                printf("%-12s | %-10s | %-15s | %-6d\n", matched[i].student_id, matched[i].name, matched[i].course_name, matched[i].score);
                            printf("------------------------------------------------------------\n");
                            free(matched);
                        }
                        else
                            printf("\n[提示] 未找到记录。\n");
                    }
                    if (all)
                        free(all);
                }
            }
        }
        // ================= 5. 修改姓名 (批量同步) =================

        else if (choice == 5)
        {
            if (DM_GetDataSize() == 0)
                printf("\n[提示] 系统中暂无数据！\n");
            else
            {
                char stu_id[20] = {0};
                char new_name[MAX_NAME_LEN] = {0};
                char err_msg[100] = {0};
                printf("\n============================================================\n");
                printf("           [ 修改学生姓名 - 批量同步 ]\n");
                printf("============================================================\n");
                PrintTestCasesGuide();

                printf("\n[步骤 1/2] 请输入要修改的学生学号\n");
                ret = ReadStringWithTabPreview("1. 学号 (Tab预览):  ", stu_id, sizeof(stu_id), PREVIEW_STUDENT);
                if (ret == RET_HOME || ret == RET_QUIT || ret == RET_BACK)
                {
                    printf("\n[提示] 已取消修改。\n");
                    continue;
                }

                if (!CheckStudentIDFormat(stu_id, err_msg, sizeof(err_msg)))
                {
                    printf("[错误] %s\n", err_msg);
                    continue;
                }

                FilterCriteria c = {0};
                SafeStrCopy(c.student_id, MAX_STU_ID_LEN, stu_id);
                c.is_fuzzy_course = true;
                c.score_min = -1;
                c.score_max = 100;
                int count = 0;
                CourseRecord *recs = DM_FilterRecords(&c, &count);

                if (count == 0)
                {
                    printf("\n[✗ 失败] 未找到该学号的任何选课记录，无法修改！\n");
                    if (recs)
                        free(recs);
                    continue;
                }

                printf("\n[当前信息] 学号: %s | 姓名: %s | 共有 %d 条选课记录\n", stu_id, recs[0].name, count);
                printf("\n[步骤 2/2] 请输入新姓名\n");
                ret = ReadStringWithDefault("2. 新姓名:  ", new_name, sizeof(new_name));
                if (ret == RET_HOME || ret == RET_QUIT || ret == RET_BACK || strlen(new_name) == 0)
                {
                    printf("\n[提示] 已取消修改。\n");
                    if (recs)
                        free(recs);
                    continue;
                }

                printf("\n[正在同步] 正在更新 %d 条记录...\n", count);
                int success_count = 0;
                // 采用“先删后插”的方式更新所有历史记录中的姓名

                for (int i = 0; i < count; i++)
                {
                    SafeStrCopy(recs[i].name, MAX_NAME_LEN, new_name);
                    if (DM_DeleteSingle(recs[i].student_id, recs[i].course_id) == STATUS_SUCCESS)
                    {
                        if (DM_AdaptiveInsert(&recs[i]) == STATUS_SUCCESS)
                            success_count++;
                    }
                }

                if (success_count == count)
                    printf("\n[✓ 成功] 已成功将 %d 条记录的姓名同步更新为: %s\n", count, new_name);
                else
                    printf("\n[⚠ 警告] 部分记录更新失败 (成功 %d/%d)。\n", success_count, count);
                if (recs)
                    free(recs);
            }
        }
        else
            printf("\n[错误] 无效选项！\n");
        WaitEnter();
    }
}
/* =========================================
[快捷] 从CSV导入初始数据 (提取至主菜单)
========================================= */
static void Demo_LoadFromCSV()
{
    char filename[100] = {0};
    printf("\n============================================================\n");
    printf("           [快捷] 从CSV导入初始数据\n");
    printf("============================================================\n");
    printf(" 💡 提示：请将您的数据文件（如 data.csv）放在程序同级目录下。\n");
    printf(" 💡 提示：导入前，系统【不会】自动清空现有内存数据，而是追加。\n");
    printf(" ⚠️  注意：若CSV中存在【重复的(学号+课程号)】记录，系统将自动触发\n");
    printf("    【主键冲突拦截】并去重，因此最终导入数量可能小于文件总行数。\n");
    printf("    (这是为了保证教务系统数据一致性的核心安全机制！)\n");
    printf("------------------------------------------------------------\n");

    int ret = ReadStringWithDefault("请输入文件名 (回车默认 data.csv):  ", filename, sizeof(filename));
    if (ret == RET_OK)
    {
        if (strlen(filename) == 0)
            strcpy(filename, "data.csv");

        // ================= 优化后的提示代码 =================
        printf("\n============================================================\n");
        printf("  [⏳ 正在解析并导入 %s ，请稍候...]\n", filename);
        printf("  (若数据量达10万条，系统会自动启用哈希引擎加速，可能需要10-20秒)\n");
        printf("  [!] 系统正在全力处理中，请耐心等待，请勿关闭程序...\n");
        printf("============================================================\n");
        fflush(stdout); // ★ 核心：强制刷新输出缓冲区，确保提示瞬间显示在屏幕上！
        // ====================================================

        // 记录开始时间，用于计算耗时（使用高精度计时器）
        double start_time = GetHighPrecisionTime();
        // 调用底层导入函数
        int count = DM_LoadFromFile(filename);

        double end_time = GetHighPrecisionTime();
        double time_spent = (end_time - start_time) * 1000.0; // 转换为毫秒

        if (count >= 0)
        {
            printf("\n[✓ 成功] 导入完成！耗时: %.2f ms\n", time_spent);
            printf("------------------------------------------------------------\n");
            printf(" 📊 导入统计与系统状态:\n");
            printf("    - 成功写入系统: %d 条有效记录 (已自动过滤重复/非法数据)\n", count);
            printf("    - 当前系统总量: %d 条\n", DM_GetDataSize());
            g_data_modified = true;
            int current_size = DM_GetDataSize();
            if (current_size >= 5000)
                printf("    - ⚡ 调度状态: 已自动切换至【哈希表单引擎】模式，查询极速！\n");
            else
                printf("    - 💡 调度状态: 当前处于【三引擎全同步】模式，数据强一致！\n");
            printf("------------------------------------------------------------\n");
        }
        else
        {
            printf("\n[✗ 失败] 加载失败，请检查文件是否存在或格式是否正确！\n");
        }
    }
    WaitEnter();
}
/* =========================================
[任务2] 数据持久化演示 (CSV导出)
========================================= */
static void Demo_Persistence()
{
    int choice;
    while (1)
    {
        printf("\n============================================================\n");
        printf("           [任务2] 数据持久化 - CSV导出\n");
        printf("============================================================\n");
        printf(" 1. 保存数据到CSV文件\n");
        printf(" 2. 查看当前数据量\n");
        printf(" 0. 返回上级菜单\n");
        printf("============================================================\n");
        char choice_str[10] = {0};
        int ret = ReadStringWithDefault("请选择操作 (0-2):  ", choice_str, sizeof(choice_str));
        if (ret == RET_BACK || ret == RET_HOME)
            break;
        if (ret == RET_QUIT)
            return;
        choice = atoi(choice_str);
        if (choice == 0 && strcmp(choice_str, "0") != 0)
        {
            printf("\n[错误] 无效输入！\n");
            WaitEnter();
            continue;
        }
        if (choice == 0)
            break;

        if (choice == 1)
        {
            char filename[100] = {0};
            ret = ReadStringWithDefault("请输入文件名 (回车默认 data.csv):  ", filename, sizeof(filename));
            if (ret == RET_OK)
            {
                if (strlen(filename) == 0)
                    strcpy(filename, "data.csv");
                int count = DM_SaveToFile(filename);
                if (count >= 0)
                    printf("\n[✓ 成功] 已保存 %d 条到 %s\n", count, filename);
                else
                    printf("\n[✗ 失败] 保存失败！\n");
            }
        }
        else if (choice == 2)
        {
            printf("\n[信息] 当前数据量: %d 条\n", DM_GetDataSize());
        }
        else
            printf("\n[错误] 无效选项！\n");
        WaitEnter();
    }
}

/* =========================================
[任务3-1] 多条件筛选
========================================= */
static void Demo_Filter()
{
    printf("\n--- 多条件筛选 (ESC取消) ---\n");
    FilterCriteria criteria = {0};
    criteria.is_fuzzy_course = true;
    criteria.score_min = -1;
    criteria.score_max = 100;
    char temp[50] = {0};
    int ret;
    ret = ReadStringWithDefault("1. 课程名称关键词 (回车不限):  ", criteria.course_name, sizeof(criteria.course_name));
    if (ret != RET_OK)
        return;
    ret = ReadStringWithDefault("2. 学院 (回车不限):  ", criteria.college, sizeof(criteria.college));
    if (ret != RET_OK)
        return;
    ret = ReadStringWithDefault("3. 学期 (回车不限):  ", criteria.semester, sizeof(criteria.semester));
    if (ret != RET_OK)
        return;
    ret = ReadStringWithDefault("4. 成绩下限 (回车默认-1):  ", temp, sizeof(temp));
    if (ret != RET_OK)
        return;
    if (strlen(temp) > 0)
        criteria.score_min = atoi(temp);
    ret = ReadStringWithDefault("5. 成绩上限 (回车默认100):  ", temp, sizeof(temp));
    if (ret != RET_OK)
        return;
    if (strlen(temp) > 0)
        criteria.score_max = atoi(temp);

    int count = 0;
    CourseRecord *results = DM_FilterRecords(&criteria, &count);
    if (count == 0)
    {
        printf("\n[提示] 未找到符合条件的记录。\n");
        if (results)
            free(results);
        WaitEnter();
        return;
    }
    printf("\n[筛选结果] 共找到 %d 条符合条件的记录：\n", count);
    printf("------------------------------------------------------------------------\n");
    printf(" %-4s | %-12s | %-10s | %-18s | %-8s | %-4s\n", "序号", "学号", "姓名", "课程名", "学期", "成绩");
    printf("------------------------------------------------------------------------\n");
    for (int i = 0; i < count; i++)
    {
        char score_str[10];
        if (results[i].score == -1)
            snprintf(score_str, sizeof(score_str), "待修");
        else
            snprintf(score_str, sizeof(score_str), "%d", results[i].score);
        printf(" %-4d | %-12s | %-10s | %-18s | %-8s | %-4s\n",
               i + 1,
               results[i].student_id,
               results[i].name,
               results[i].course_name,
               results[i].semester,
               score_str);
    }
    printf("------------------------------------------------------------------------\n");

    char confirm[10] = {0};
    ret = ReadStringWithDefault("\n是否导出到 filtered_result.csv？(y/n):  ", confirm, sizeof(confirm));
    if (ret == RET_OK && (strcmp(confirm, "y") == 0 || strcmp(confirm, "Y") == 0))
    {
        FILE *fp = fopen("filtered_result.csv", "w");
        if (fp)
        {
            fprintf(fp, "student_id,name,college,course_id,course_name,credit,semester,enroll_date,score,is_elective\n");
            for (int i = 0; i < count; i++)
                fprintf(fp, "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n", results[i].student_id, results[i].name, results[i].college, results[i].course_id, results[i].course_name, results[i].credit, results[i].semester, results[i].enroll_date, results[i].score, results[i].is_elective);
            fclose(fp);
            printf("[✓ 成功] 已导出。\n");
        }
    }
    if (results)
        free(results);
    WaitEnter();
}

/* =========================================
[任务3-2] 多关键字排序
========================================= */
static void Demo_Sort()
{
    printf("\n============================================================\n");
    printf("           [任务3-2] 多关键字排序\n");
    printf("============================================================\n");
    printf(" [快捷键提示] b:返回上一步  h:回到主菜单  q:退出程序  ESC:取消输入\n");

    FilterCriteria all_c = {0};
    all_c.is_fuzzy_course = true;
    all_c.score_min = -1;
    all_c.score_max = 100;
    int total = 0;
    CourseRecord *all = DM_FilterRecords(&all_c, &total);
    if (total == 0)
    {
        printf("\n[提示] 暂无数据，请先插入或导入记录。\n");
        WaitEnter();
        return;
    }

    /* -------- 📖 排序指南面板 -------- */
    printf("\n============================================================\n");
    printf("           [ 📖 多关键字排序 - 操作指南 ]\n");
    printf("------------------------------------------------------------\n");
    printf(" 📌 排序字段编号:\n");
    printf("    1 → 学号 (按字典序, 可观察入学年份/学院分组)\n");
    printf("    2 → 成绩 (数值比较, -1待修排在最前)\n");
    printf("    3 → 学分 (数值比较, 如1.0 / 2.5 / 3.5 / 4.0)\n");
    printf("    4 → 选课日期 (按YYYY-MM-DD字典序)\n");
    printf("------------------------------------------------------------\n");
    printf(" 📌 排序方向:\n");
    printf("    1 → 升序 (小→大, A→Z, 旧→新)\n");
    printf("    2 → 降序 (大→小, Z→A, 新→旧)\n");
    printf("------------------------------------------------------------\n");
    printf(" 📌 推荐测试用例 (直接照着输入):\n");
    printf("    ▶ 用例A [成绩排名]:\n");
    printf("       规则数=1 → 字段=2, 方向=2\n");
    printf("       效果: 按成绩从高到低排名\n");
    printf("    ▶ 用例B [学院内成绩排名]:\n");
    printf("       规则数=2 → ①字段=1升序 ②字段=2降序\n");
    printf("       效果: 同学号前缀(同学院)内, 成绩从高到低\n");
    printf("    ▶ 用例C [学分-成绩-日期 三维排序]:\n");
    printf("       规则数=3 → ①字段=3降序 ②字段=2降序 ③字段=4升序\n");
    printf("       效果: 先按学分高低, 同学分按成绩高低, 再按日期早晚\n");
    printf("    ▶ 用例D [最新选课优先]:\n");
    printf("       规则数=1 → 字段=4, 方向=2\n");
    printf("       效果: 最近选的课排在最前面\n");
    printf("------------------------------------------------------------\n");
    printf(" 💡 提示: 多关键字排序时, 高优先级字段相同时,\n");
    printf("    才会使用低优先级字段进行比较。\n");
    printf("============================================================\n");
    printf("当前系统共有 %d 条记录可供排序。\n", total);
    printf("按回车键开始配置排序规则...\n");
    WaitEnter();

    /* -------- 输入规则数量 -------- */
    char rule_count_str[10] = {0};
    int ret = ReadStringWithDefault("请输入排序规则数量 (1-3):  ", rule_count_str, sizeof(rule_count_str));
    if (ret != RET_OK)
    {
        if (all)
            free(all);
        return;
    }
    g_sort_rule_count_temp = atoi(rule_count_str);
    if (g_sort_rule_count_temp < 1 || g_sort_rule_count_temp > 3)
    {
        printf("\n[⚠ 提示] 输入无效，已自动修正为 1 条规则。\n");
        g_sort_rule_count_temp = 1;
    }

    /* -------- 字段名称映射 (用于回显) -------- */
    const char *field_names[] = {"", "学号", "成绩", "学分", "选课日期"};
    const char *dir_names[] = {"", "升序↑", "降序↓"};

    /* -------- 逐条输入规则 -------- */
    for (int i = 0; i < g_sort_rule_count_temp; i++)
    {
        char f_str[10] = {0};
        char d_str[10] = {0};

        printf("\n------------------------------------------------------------\n");
        printf("  📋 第 %d 优先级 (共 %d 条规则)\n", i + 1, g_sort_rule_count_temp);
        printf("  字段选项: 1=学号  2=成绩  3=学分  4=选课日期\n");
        printf("------------------------------------------------------------\n");

        ret = ReadStringWithDefault("请输入排序字段 (1-4):  ", f_str, sizeof(f_str));
        if (ret != RET_OK)
        {
            if (all)
                free(all);
            return;
        }
        int field_val = atoi(f_str);
        if (field_val < 1 || field_val > 4)
        {
            printf("[⚠ 提示] 无效字段，已自动修正为 1(学号)。\n");
            field_val = 1;
        }
        g_sort_rules_temp[i].field = field_val;

        printf("  方向选项: 1=升序(小→大)  2=降序(大→小)\n");
        ret = ReadStringWithDefault("请输入排序方向 (1/2):  ", d_str, sizeof(d_str));
        if (ret != RET_OK)
        {
            if (all)
                free(all);
            return;
        }
        int dir_val = atoi(d_str);
        if (dir_val < 1 || dir_val > 2)
        {
            printf("[⚠ 提示] 无效方向，已自动修正为 1(升序)。\n");
            dir_val = 1;
        }
        g_sort_rules_temp[i].direction = dir_val;

        printf("  [✓ 已设置] 第%d优先级: 按【%s】%s\n",
               i + 1, field_names[field_val], dir_names[dir_val]);
    }

    /* -------- 回显完整排序规则 -------- */
    printf("\n============================================================\n");
    printf("  📊 最终排序规则确认:\n");
    printf("------------------------------------------------------------\n");
    for (int i = 0; i < g_sort_rule_count_temp; i++)
    {
        printf("  优先级%d: %s %s\n",
               i + 1,
               field_names[g_sort_rules_temp[i].field],
               dir_names[g_sort_rules_temp[i].direction]);
    }
    printf("============================================================\n");

    /* -------- 执行排序 -------- */
    printf("\n[正在排序] 对 %d 条记录进行多关键字排序...\n", total);
    qsort(all, total, sizeof(CourseRecord), TempMultiKeyCompare);

    /* -------- 展示结果 -------- */
    int show = total < 15 ? total : 15;
    printf("\n[排序结果] 展示前 %d 条 (共 %d 条):\n", show, total);
    printf("------------------------------------------------------------------------------\n");
    printf(" %-12s | %-10s | %-18s | %-6s | %-5s | %-12s\n",
           "学号", "姓名", "课程名", "成绩", "学分", "选课日期");
    printf("------------------------------------------------------------------------------\n");
    for (int i = 0; i < show; i++)
    {
        char score_display[10];
        if (all[i].score == -1)
            snprintf(score_display, sizeof(score_display), "待修");
        else
            snprintf(score_display, sizeof(score_display), "%d", all[i].score);

        printf(" %-12s | %-10s | %-18s | %-6s | %-5.1f | %-12s\n",
               all[i].student_id,
               all[i].name,
               all[i].course_name,
               score_display,
               all[i].credit,
               all[i].enroll_date);
    }
    printf("------------------------------------------------------------------------------\n");
    if (total > 15)
        printf(" ... (仅显示前15条，共 %d 条记录已排序)\n", total);

    /* -------- 导出提示 -------- */
    char export_choice[10] = {0};
    ret = ReadStringWithDefault("\n是否导出排序结果到 sorted_result.csv？(y/n):  ", export_choice, sizeof(export_choice));
    if (ret == RET_OK && (strcmp(export_choice, "y") == 0 || strcmp(export_choice, "Y") == 0))
    {
        FILE *fp = fopen("sorted_result.csv", "w");
        if (fp)
        {
            fprintf(fp, "student_id,name,college,course_id,course_name,credit,semester,enroll_date,score,is_elective\n");
            for (int i = 0; i < total; i++)
                fprintf(fp, "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n",
                        all[i].student_id, all[i].name, all[i].college,
                        all[i].course_id, all[i].course_name, all[i].credit,
                        all[i].semester, all[i].enroll_date, all[i].score, all[i].is_elective);
            fclose(fp);
            printf("[✓ 成功] 已将 %d 条排序结果导出至 sorted_result.csv\n", total);
        }
        else
            printf("[✗ 失败] 无法创建文件！\n");
    }

    if (all)
        free(all);
    WaitEnter();
}

/* =========================================
[任务4] 统计分析 (完整实现5项)
========================================= */
static void Demo_Statistics()
{
    int choice;
    while (1)
    {
        printf("\n============================================================\n");
        printf("             [任务4] 数据统计分析\n");
        printf("============================================================\n");
        printf(" 1. 每门课程的选课人数与容量使用率 (4.1)\n");
        printf(" 2. 每位学生的选课门数与总学分 (4.2)\n");
        printf(" 3. 各学院选课人数分布 (4.3)\n");
        printf(" 4. 按学期统计选课总人数与课程数 (4.4)\n");
        printf(" 5. 课程成绩分布统计 (4.5)\n");
        printf(" 0. 返回上级菜单\n");
        printf("============================================================\n");
        char choice_str[10] = {0};
        int ret = ReadStringWithDefault("请选择统计项 (0-5):  ", choice_str, sizeof(choice_str));
        if (ret == RET_BACK || ret == RET_HOME)
            break;
        if (ret == RET_QUIT)
            return;
        choice = atoi(choice_str);
        if (choice == 0 && strcmp(choice_str, "0") != 0)
        {
            printf("\n[错误] 无效输入！\n");
            WaitEnter();
            continue;
        }
        if (choice == 0)
            break;

        if (choice == 1)
        {
            int count = 0;
            CourseEnrollmentStat *stats = DM_GetCourseEnrollmentStats(&count);
            printf("\n--- 4.1 每门课程的选课人数与容量使用率 ---\n");
            printf("%-10s | ", "课程号");
            PrintPaddedString("课程名", 20);
            printf(" | ");
            printf("%-6s | %-8s\n", "人数", "使用率");
            printf("--------------------------------------------------------------\n");
            for (int i = 0; i < count; i++)
            {
                printf("%-10s | ", stats[i].course_id);
                PrintPaddedString(stats[i].course_name, 20);
                printf(" | ");
                printf("%-6d | %-8.1f%%\n", stats[i].enroll_count, stats[i].usage_rate);
            }
            if (stats)
                free(stats);
        }
        else if (choice == 2)
        {
            int count = 0;
            StudentCreditStat *stats = DM_GetStudentCreditStats(&count);
            printf("\n--- 4.2 每位学生的选课门数与总学分 ---\n");
            printf("%-12s | ", "学号");
            PrintPaddedString("姓名", 10);
            printf(" | ");
            printf("%-6s | %-8s\n", "课程数", "总学分");
            printf("----------------------------------------------------\n");
            int show = count < 20 ? count : 20;
            for (int i = 0; i < show; i++)
            {
                printf("%-12s | ", stats[i].student_id);
                PrintPaddedString(stats[i].name, 10);
                printf(" | ");
                printf("%-6d | %-8.1f\n", stats[i].course_count, stats[i].total_credit);
            }
            if (count > 20)
                printf("... (仅显示前20条，共%d条)\n", count);
            if (stats)
                free(stats);
        }
        else if (choice == 3)
        {
            int count = 0;
            CollegeDistributionStat *stats = DM_GetCollegeDistributionStats(&count);
            printf("\n--- 4.3 各学院选课人数分布 ---\n");
            PrintPaddedString("学院", 20);
            printf(" | ");
            printf("%-6s | %-8s\n", "人数", "占比");
            printf("------------------------------------------------\n");
            for (int i = 0; i < count; i++)
            {
                PrintPaddedString(stats[i].college, 20);
                printf(" | ");
                printf("%-6d | %-8.1f%%\n", stats[i].enroll_count, stats[i].percentage);
            }
            if (stats)
                free(stats);
        }
        else if (choice == 4)
        {
            printf("\n--- 4.4 按学期统计选课总人数与课程数分布 ---\n");
            FilterCriteria all_c = {0};
            all_c.is_fuzzy_course = true;
            all_c.score_min = -1;
            all_c.score_max = 100;
            int total = 0;
            CourseRecord *all = DM_FilterRecords(&all_c, &total);
            if (total == 0)
                printf("[提示] 暂无数据。\n");
            else
            {
                typedef struct
                {
                    char semester[20];
                    int students;
                    int courses;
                    char stu_keys[5000];
                    char cou_keys[5000];
                } SemStat;
                SemStat sems[50];
                int sem_count = 0;
                for (int i = 0; i < total; i++)
                {
                    int idx = -1;
                    for (int j = 0; j < sem_count; j++)
                        if (strcmp(sems[j].semester, all[i].semester) == 0)
                        {
                            idx = j;
                            break;
                        }
                    if (idx == -1)
                    {
                        idx = sem_count++;
                        SafeStrCopy(sems[idx].semester, sizeof(sems[idx].semester), all[i].semester);
                        sems[idx].students = 0;
                        sems[idx].courses = 0;
                        sems[idx].stu_keys[0] = '\0';
                        sems[idx].cou_keys[0] = '\0';
                    }
                    char key[50];
                    snprintf(key, sizeof(key), "|%s|", all[i].student_id);
                    if (!strstr(sems[idx].stu_keys, key))
                    {
                        strcat(sems[idx].stu_keys, key);
                        sems[idx].students++;
                    }
                    snprintf(key, sizeof(key), "|%s|", all[i].course_id);
                    if (!strstr(sems[idx].cou_keys, key))
                    {
                        strcat(sems[idx].cou_keys, key);
                        sems[idx].courses++;
                    }
                }
                // 新代码：
                PrintPaddedString("学期", 12);
                printf(" | ");
                PrintPaddedString("学生人数", 12);
                printf(" | ");
                PrintPaddedString("课程数", 12);
                printf("\n");
                printf("------------------------------------------------\n");
                for (int i = 0; i < sem_count; i++)
                    printf("%-12s | %-12d | %-12d\n", sems[i].semester, sems[i].students, sems[i].courses);
            }
            if (all)
                free(all);
        }
        else if (choice == 5)
        {
            ScoreDistributionStat stat = {0};
            DM_GetScoreDistributionStats(&stat);
            printf("\n--- 4.5 课程成绩分布统计 ---\n");
            printf("优秀(90-100): %d (%.1f%%)\n", stat.excellent, stat.total > 0 ? (float)stat.excellent / stat.total * 100 : 0);
            printf("良好(80-89):  %d (%.1f%%)\n", stat.good, stat.total > 0 ? (float)stat.good / stat.total * 100 : 0);
            printf("中等(70-79):  %d (%.1f%%)\n", stat.medium, stat.total > 0 ? (float)stat.medium / stat.total * 100 : 0);
            printf("及格(60-69):  %d (%.1f%%)\n", stat.pass, stat.total > 0 ? (float)stat.pass / stat.total * 100 : 0);
            printf("不及格(<60):  %d (%.1f%%)\n", stat.fail, stat.total > 0 ? (float)stat.fail / stat.total * 100 : 0);
            printf("待修:         %d (%.1f%%)\n", stat.pending, stat.total > 0 ? (float)stat.pending / stat.total * 100 : 0);
            printf("总计:         %d\n", stat.total);
        }
        else
            printf("\n[错误] 无效选项！\n");
        WaitEnter();
    }
}

/* =========================================
[任务5] 过期记录清理与归档
========================================= */
static void Demo_CleanExpiredRecords()
{
    printf("\n--- 批量清理与归档记录 (ESC取消) ---\n");
    int total = DM_GetDataSize();
    int current_year = atoi(Menu_GetCurrentDate());
    int graduate_year_threshold = current_year - 4;
    FilterCriteria all_c = {0};
    all_c.is_fuzzy_course = true;
    int rec_count = 0;
    CourseRecord *all = DM_FilterRecords(&all_c, &rec_count);
    if (!all || rec_count == 0)
    {
        printf("[提示] 当前无数据。\n");
        WaitEnter();
        return;
    }

    int expired_count = 0, graduated_count = 0, ungraduated_count = 0;
    for (int i = 0; i < rec_count; i++)
    {
        if (strcmp(all[i].enroll_date, "2023-09-01") < 0)
            expired_count++;
        else if (total >= 50000)
        { // 大数据量下触发毕业归档逻辑

            int enroll_year = atoi(all[i].student_id);
            if (enroll_year <= graduate_year_threshold)
                graduated_count++;
            else
                ungraduated_count++;
        }
    }
    if (expired_count == 0 && graduated_count == 0 && ungraduated_count == 0)
    {
        printf("[提示] 无符合清理或归档条件的记录。\n");
        free(all);
        WaitEnter();
        return;
    }
    if (total < 50000)
        printf("[策略] 数据量 < 5万，将清理 %d 条过期记录。\n", expired_count);
    else
    {
        printf("[策略] 数据量 >= 5万，触发【深度清理与数据迁移】：\n");
        printf("  - 彻底删除 %d 条过期记录\n", expired_count);
        printf("  - 彻底删除 %d 条已毕业记录\n", graduated_count);
        printf("  - 迁移归档 %d 条未毕业记录至 ungraduated_backup.csv\n", ungraduated_count);
    }
    char confirm[20] = {0};
    int ret = ReadStringWithDefault("确认执行？(输入 yes):  ", confirm, sizeof(confirm));
    if (ret == RET_OK && strcmp(confirm, "yes") == 0)
    {
        FILE *fp_backup = NULL;
        if (total >= 50000 && ungraduated_count > 0)
        {
            fp_backup = fopen("ungraduated_backup.csv", "w");
            if (fp_backup)
                fprintf(fp_backup, "student_id,name,college,course_id,course_name,credit,semester,enroll_date,score,is_elective\n");
        }
        int deleted = 0, archived = 0;
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
        if (fp_backup)
            fclose(fp_backup);
        printf("\n[✓ 成功] 已彻底删除 %d 条。", deleted);
        g_data_modified = true;
        if (archived > 0)
            printf(" 已将 %d 条在校生记录安全迁移至 ungraduated_backup.csv。\n", archived);
        else
            printf("\n");
    }
    else
        printf("\n[提示] 已取消。\n");
    if (all)
        free(all);
    WaitEnter();
}

/* =========================================
[任务6] 性能对比 (基础: 100~10000)
测试链表、AVL树、哈希表三种数据结构的增删改查性能
========================================= */

static void Demo_PerformanceReport_Base()
{
// ★ 核心修复：定义中文对齐宏，精确计算UTF-8终端显示宽度并补齐空格
#define PRINT_PADDED(str, target_w)                  \
    do                                               \
    {                                                \
        int _w = 0;                                  \
        const char *_p = (str);                      \
        while (*_p)                                  \
        {                                            \
            unsigned char _c = (unsigned char)*_p;   \
            if (_c >= 0xE0)                          \
            {                                        \
                _w += 2;                             \
                _p += 3;                             \
            }                                        \
            else if (_c >= 0xC0)                     \
            {                                        \
                _w += 2;                             \
                _p += 2;                             \
            }                                        \
            else                                     \
            {                                        \
                _w += 1;                             \
                _p += 1;                             \
            }                                        \
        }                                            \
        printf("%s", (str));                         \
        for (int _i = 0; _i < (target_w) - _w; _i++) \
            printf(" ");                             \
    } while (0)

    printf("\n============================================================\n");
    printf("    [任务6] 性能对比测试与复杂度验证 (基础)\n");
    printf("============================================================\n");
    printf("测试规模: 100, 1000, 10000 条\n");
    printf("输出文件: performance_report.txt\n");

    int sizes[] = {100, 1000, 10000};
    const char *engine_names[] = {"链表", "AVL树", "哈希表"};

    FILE *fp = fopen("performance_report.txt", "w");
    if (!fp)
    {
        printf("[错误] 无法创建报告文件！\n");
        WaitEnter();
        return;
    }
    fprintf(fp, "=== 数据结构性能对比测试报告 (基础 100~10000) ===\n");

    double insert_times[3][3], search_times[3][3], delete_times[3][3], memory_usages[3][3];
    int actual_counts[3][3];

    for (int s = 0; s < 3; s++)
    {
        int size = sizes[s];
        printf("\n正在准备规模: %d 条的数据池...\n", size);

        CourseRecord *data_pool = (CourseRecord *)malloc(size * sizeof(CourseRecord));
        if (!data_pool)
        {
            printf("[错误] 内存分配失败！\n");
            fclose(fp);
            return;
        }
        for (int i = 0; i < size; i++)
            GenerateMockRecord(i / COURSES_COUNT, i % COURSES_COUNT, &data_pool[i]);

        fprintf(fp, "\n--- 数据规模: %d 条 ---\n", size);

        char **stu_ids = (char **)malloc(size * sizeof(char *));
        char **cou_ids = (char **)malloc(size * sizeof(char *));
        int *delete_order = (int *)malloc(size * sizeof(int));
        int *search_order = (int *)malloc(size * sizeof(int));

        for (int k = 0; k < size; k++)
        {
            stu_ids[k] = (char *)malloc(20);
            cou_ids[k] = (char *)malloc(20);
            delete_order[k] = k;
            search_order[k] = k;
            GenerateStudentID(k / COURSES_COUNT, stu_ids[k], 20);
            GenerateCourseID(k % COURSES_COUNT, cou_ids[k], 20);
        }

        srand(42 + s);
        for (int k = size - 1; k > 0; k--)
        {
            int j = rand() % (k + 1);
            int t = delete_order[k];
            delete_order[k] = delete_order[j];
            delete_order[j] = t;
            j = rand() % (k + 1);
            t = search_order[k];
            search_order[k] = search_order[j];
            search_order[j] = t;
        }

        printf("\n【规模: %d 条】\n", size);
        printf("%-10s | %12s | %12s | %12s | %12s | %10s\n", "操作", "内存(MB)", "链表", "AVL(ms)", "哈希(ms)", "最优");
        printf("-------------------------------------------------------------------------------------------------\n");

        // 1. 测试内存占用
        for (int engine = 1; engine <= 3; engine++)
        {
            DM_SetActiveEngine((StorageEngine)engine);
            DM_Init();
            double start = GetHighPrecisionTime();
            int success_count = 0;
            for (int i = 0; i < size; i++)
                if (DM_InsertPure(&data_pool[i]) == STATUS_SUCCESS)
                    success_count++;
            double end = GetHighPrecisionTime();
            insert_times[s][engine - 1] = (end - start) * 1000.0;
            actual_counts[s][engine - 1] = success_count;
            memory_usages[s][engine - 1] = DM_GetMemoryUsage() / (1024.0 * 1024.0);
            DM_Destroy();
        }
        int best_mem = 0;
        for (int i = 1; i < 3; i++)
            if (memory_usages[s][i] < memory_usages[s][best_mem])
                best_mem = i;
        printf("%-10s | %12.4f | %12.4f | %12.4f | %12.4f | %s\n", "内存占用", memory_usages[s][0], memory_usages[s][1], memory_usages[s][2], memory_usages[s][best_mem], best_mem == 0 ? "[*] 链表" : (best_mem == 1 ? "[*] AVL" : "[*] 哈希"));

        // 2. 测试插入耗时
        int bi = 0;
        for (int i = 1; i < 3; i++)
            if (insert_times[s][i] < insert_times[s][bi])
                bi = i;
        printf("%-10s | %12s | %12.2f | %12.2f | %12.2f | %s\n", "插入", "-", insert_times[s][0], insert_times[s][1], insert_times[s][2], bi == 0 ? "★ 链表" : (bi == 1 ? "★ AVL" : "★ 哈希"));

        // 3. 测试查找耗时
        for (int engine = 1; engine <= 3; engine++)
        {
            DM_SetActiveEngine((StorageEngine)engine);
            DM_Init();
            for (int i = 0; i < size; i++)
                DM_InsertSingle(&data_pool[i]);
            double start = GetHighPrecisionTime();
            volatile int search_dummy = 0;
            for (int k = 0; k < size; k++)
            {
                int idx = search_order[k];
                CourseRecord rec;
                search_dummy += DM_SearchRecord(stu_ids[idx], cou_ids[idx], &rec);
            }
            if (search_dummy < -999999999)
                printf("Impossible\n");
            double end = GetHighPrecisionTime();
            search_times[s][engine - 1] = (end - start) * 1000.0;
            DM_Destroy();
        }
        int bs = 0;
        for (int i = 1; i < 3; i++)
            if (search_times[s][i] < search_times[s][bs])
                bs = i;
        printf("%-10s | %12s | %12.2f | %12.2f | %12.2f | %s\n", "查找", "-", search_times[s][0], search_times[s][1], search_times[s][2], bs == 0 ? "★ 链表" : (bs == 1 ? "★ AVL" : "★ 哈希"));

        // 4. 测试删除耗时
        for (int engine = 1; engine <= 3; engine++)
        {
            DM_SetActiveEngine((StorageEngine)engine);
            DM_Init();
            for (int i = 0; i < size; i++)
                DM_InsertSingle(&data_pool[i]);
            double start = GetHighPrecisionTime();
            volatile int delete_dummy = 0;
            for (int k = 0; k < size; k++)
            {
                int idx = delete_order[k];
                delete_dummy += DM_DeleteSingle(stu_ids[idx], cou_ids[idx]);
            }
            if (delete_dummy < -999999999)
                printf("Impossible\n");
            double end = GetHighPrecisionTime();
            delete_times[s][engine - 1] = (end - start) * 1000.0;
            printf("  [验证] %s 删除后剩余: %d 条\n", engine_names[engine - 1], DM_GetDataSize());
            DM_Destroy();
        }
        int bd = 0;
        for (int i = 1; i < 3; i++)
            if (delete_times[s][i] < delete_times[s][bd])
                bd = i;
        printf("%-10s | %12s | %12.2f | %12.2f | %12.2f | %s\n", "删除", "-", delete_times[s][0], delete_times[s][1], delete_times[s][2], bd == 0 ? "★ 链表" : (bd == 1 ? "★ AVL" : "★ 哈希"));

        // ================= ★ 新增：当前规模性能红黑榜 ★ =================
        int best_insert = 0, best_search = 0, best_delete = 0;
        for (int i = 1; i < 3; i++)
        {
            if (insert_times[s][i] < insert_times[s][best_insert])
                best_insert = i;
            if (search_times[s][i] < search_times[s][best_search])
                best_search = i;
            if (delete_times[s][i] < delete_times[s][best_delete])
                best_delete = i;
        }
        printf("\n  ---------------- 规模 %d 条 - 性能红黑榜 ----------------\n", size);
        printf("  [⚡ 插入最快] ");
        PRINT_PADDED(engine_names[best_insert], 6);
        printf(" (%.2f ms)\n", insert_times[s][best_insert]);
        printf("  [⚡ 查找最快] ");
        PRINT_PADDED(engine_names[best_search], 6);
        printf(" (%.2f ms)\n", search_times[s][best_search]);
        printf("  [⚡ 删除最快] ");
        PRINT_PADDED(engine_names[best_delete], 6);
        printf(" (%.2f ms)\n", delete_times[s][best_delete]);
        printf("  [💾 内存最省] ");
        PRINT_PADDED(engine_names[best_mem], 6);
        printf(" (%.4f MB)\n", memory_usages[s][best_mem]);
        printf("  ----------------------------------------------------\n");

        // 写入文件报告
        fprintf(fp, "%-10s | %12s | %12s | %12s | %12s | %10s\n", "操作", "内存(MB)", "链表", "AVL(ms)", "哈希(ms)", "最优");
        fprintf(fp, "-------------------------------------------------------------------------------------------------\n");
        fprintf(fp, "%-10s | %12.4f | %12.4f | %12.4f | %12.4f | %s\n", "内存占用", memory_usages[s][0], memory_usages[s][1], memory_usages[s][2], memory_usages[s][best_mem], best_mem == 0 ? "[*] 链表" : (best_mem == 1 ? "[*] AVL" : "[*] 哈希"));
        fprintf(fp, "%-10s | %12s | %12.2f | %12.2f | %12.2f | %s (成功%d条)\n", "插入", "-", insert_times[s][0], insert_times[s][1], insert_times[s][2], bi == 0 ? "[*] 链表" : (bi == 1 ? "[*] AVL" : "[*] 哈希"), actual_counts[s][bi]);
        fprintf(fp, "%-10s | %12s | %12.2f | %12.2f | %12.2f | %s\n", "查找", "-", search_times[s][0], search_times[s][1], search_times[s][2], bs == 0 ? "[*] 链表" : (bs == 1 ? "[*] AVL" : "[*] 哈希"));
        fprintf(fp, "%-10s | %12s | %12.2f | %12.2f | %12.2f | %s\n", "删除", "-", delete_times[s][0], delete_times[s][1], delete_times[s][2], bd == 0 ? "[*] 链表" : (bd == 1 ? "[*] AVL" : "[*] 哈希"));

        free(data_pool);
        for (int k = 0; k < size; k++)
        {
            free(stu_ids[k]);
            free(cou_ids[k]);
        }
        free(stu_ids);
        free(cou_ids);
        free(delete_order);
        free(search_order);
    }

    // ================= ★ 新增：全规模综合演进与选型结论 (控制台+文件) ★ =================
    int scores_per_scale[3][3] = {0}; // [规模][引擎]
    int total_scores[3] = {0};

    // 计算各规模下的综合得分
    for (int s = 0; s < 3; s++)
    {
        int i_rank[3] = {0}, s_rank[3] = {0}, d_rank[3] = {0};
        for (int i = 0; i < 3; i++)
        {
            int r_i = 1, r_s = 1, r_d = 1;
            for (int j = 0; j < 3; j++)
            {
                if (insert_times[s][j] < insert_times[s][i])
                    r_i++;
                if (search_times[s][j] < search_times[s][i])
                    r_s++;
                if (delete_times[s][j] < delete_times[s][i])
                    r_d++;
            }
            i_rank[i] = 4 - r_i;
            s_rank[i] = 4 - r_s;
            d_rank[i] = 4 - r_d;
            scores_per_scale[s][i] = i_rank[i] + s_rank[i] + d_rank[i];
            total_scores[i] += scores_per_scale[s][i];
        }
    }

    int best_total = 0;
    for (int i = 1; i < 3; i++)
        if (total_scores[i] > total_scores[best_total])
            best_total = i;

    // 1. 控制台打印演进矩阵
    printf("\n================ 💡 基础测试(100~1万) - 综合演进结论 ==================\n");
    printf("【各规模综合评分】(单项第1名3分, 第2名2分, 第3名1分)\n");
    printf("%-10s | %8s | %8s | %8s | %12s\n", "引擎", "100条", "1000条", "1万条", "总趋势");
    printf("---------------------------------------------------------------\n");
    for (int i = 0; i < 3; i++)
    {
        PRINT_PADDED(engine_names[i], 10);
        printf(" | %8d | %8d | %8d | %12s\n",
               scores_per_scale[0][i], scores_per_scale[1][i], scores_per_scale[2][i],
               (i == best_total) ? "★ 综合最优" : "");
    }
    printf("---------------------------------------------------------------\n");
    printf("🏆 最终选型结论: 在 100~10000 条数据规模下，【%s】综合性能最佳！\n", engine_names[best_total]);
    printf("📈 演进规律: 随着数据量从 100 增长到 10000，【%s】的优势逐渐扩大，\n", engine_names[best_total]);
    printf("   而【链表】在极小数据量(100条)时因常数项小可能占优，但随规模增大性能急剧下降。\n");
    printf("💡 架构印证: 本系统设计了【自适应策略】(阈值5000条)：\n");
    printf("   当数据量 < 5000 时，使用【三引擎全同步】保证强一致与冗余；\n");
    printf("   当数据量 >= 5000 时，自动切换至【%s】追求极致性能！\n", engine_names[best_total]);
    printf("======================================================================\n");

    // 2. 同步写入 TXT 报告文件
    fprintf(fp, "\n================ 💡 基础测试(100~1万) - 综合演进结论 ==================\n");
    fprintf(fp, "【各规模综合评分】(单项第1名3分, 第2名2分, 第3名1分)\n");
    fprintf(fp, "%-10s | %8s | %8s | %8s | %12s\n", "引擎", "100条", "1000条", "1万条", "总趋势");
    fprintf(fp, "---------------------------------------------------------------\n");
    for (int i = 0; i < 3; i++)
    {
        fprintf(fp, "%-10s | %8d | %8d | %8d | %12s\n",
                engine_names[i], scores_per_scale[0][i], scores_per_scale[1][i], scores_per_scale[2][i],
                (i == best_total) ? "★ 综合最优" : "");
    }
    fprintf(fp, "---------------------------------------------------------------\n");
    fprintf(fp, "🏆 最终选型结论: 在 100~10000 条数据规模下，【%s】综合性能最佳！\n", engine_names[best_total]);
    fprintf(fp, "💡 架构印证: 本系统设计了【自适应策略】(阈值5000条)，完美平衡了一致性与性能。\n");

    fclose(fp);
#undef PRINT_PADDED // 释放宏定义

    printf("\n[✓ 成功] 测试完成！报告已保存至 performance_report.txt\n");
    WaitEnter();
}
/* =========================================
[附加1] 10万条大规模压力测试
========================================= */
static void Demo_PerformanceReport_Stress()
{
// ★ 核心修复：定义中文对齐宏，精确计算UTF-8终端显示宽度并补齐空格
#define PRINT_PADDED(str, target_w)                  \
    do                                               \
    {                                                \
        int _w = 0;                                  \
        const char *_p = (str);                      \
        while (*_p)                                  \
        {                                            \
            unsigned char _c = (unsigned char)*_p;   \
            if (_c >= 0xE0)                          \
            {                                        \
                _w += 2;                             \
                _p += 3;                             \
            }                                        \
            else if (_c >= 0xC0)                     \
            {                                        \
                _w += 2;                             \
                _p += 2;                             \
            }                                        \
            else                                     \
            {                                        \
                _w += 1;                             \
                _p += 1;                             \
            }                                        \
        }                                            \
        printf("%s", (str));                         \
        for (int _i = 0; _i < (target_w) - _w; _i++) \
            printf(" ");                             \
    } while (0)

    printf("\n============================================================\n");
    printf("      [附加任务1] 10万条大规模数据压力测试\n");
    printf("============================================================\n");
    char confirm[10] = {0};
    int ret = ReadStringWithDefault("确认执行？(y/n):   ", confirm, sizeof(confirm));
    if (ret != RET_OK || (strcmp(confirm, "y") != 0 && strcmp(confirm, "Y") != 0))
    {
        printf("\n[提示] 已取消测试。\n");
        WaitEnter();
        return;
    }

    int size = 100000;
    const char *engine_names[] = {"链表", "AVL树", "哈希表"};
    printf("正在准备 10万条 数据池 (请稍候)...\n");

    CourseRecord *data_pool = (CourseRecord *)malloc(size * sizeof(CourseRecord));
    if (!data_pool)
    {
        printf("[错误] 内存分配失败！\n");
        return;
    }
    for (int i = 0; i < size; i++)
        GenerateMockRecord(i / COURSES_COUNT, i % COURSES_COUNT, &data_pool[i]);

    char **stu_ids = (char **)malloc(size * sizeof(char *));
    char **cou_ids = (char **)malloc(size * sizeof(char *));
    int *delete_order = (int *)malloc(size * sizeof(int));
    int *search_order = (int *)malloc(size * sizeof(int));
    for (int k = 0; k < size; k++)
    {
        stu_ids[k] = (char *)malloc(20);
        cou_ids[k] = (char *)malloc(20);
        delete_order[k] = k;
        search_order[k] = k;
        GenerateStudentID(k / COURSES_COUNT, stu_ids[k], 20);
        GenerateCourseID(k % COURSES_COUNT, cou_ids[k], 20);
    }

    srand(42);
    for (int k = size - 1; k > 0; k--)
    {
        int j = rand() % (k + 1);
        int t = delete_order[k];
        delete_order[k] = delete_order[j];
        delete_order[j] = t;
        j = rand() % (k + 1);
        t = search_order[k];
        search_order[k] = search_order[j];
        search_order[j] = t;
    }

    double memory_usages[3] = {0};
    double insert_times[3] = {0};
    double search_times[3] = {0};
    double delete_times[3] = {0};

    for (int engine = 1; engine <= 3; engine++)
    {
        printf("\n================ 测试引擎: %s ================\n", engine_names[engine - 1]);
        DM_SetActiveEngine((StorageEngine)engine);
        DM_Init();

        printf("[1/3] 插入测试 (100,000 条)...\n");
        double start = GetHighPrecisionTime();
        int success_count = 0;
        for (int i = 0; i < size; i++)
            if (DM_InsertPure(&data_pool[i]) == STATUS_SUCCESS)
                success_count++;
        double end = GetHighPrecisionTime();
        insert_times[engine - 1] = (end - start) * 1000.0;
        printf("  [插入] 成功 %d 条, 耗时: %.2f ms\n", success_count, insert_times[engine - 1]);
        memory_usages[engine - 1] = DM_GetMemoryUsage() / (1024.0 * 1024.0);

        printf("[2/3] 查找测试 (100,000 次)...\n");
        start = GetHighPrecisionTime();
        volatile int search_dummy = 0;
        for (int k = 0; k < size; k++)
        {
            int idx = search_order[k];
            CourseRecord rec;
            search_dummy += DM_SearchRecord(stu_ids[idx], cou_ids[idx], &rec);
        }
        if (search_dummy < -999999999)
            printf("Impossible\n");
        end = GetHighPrecisionTime();
        search_times[engine - 1] = (end - start) * 1000.0;
        printf("  [查找] 100,000 次耗时: %.2f ms\n", search_times[engine - 1]);

        printf("[3/3] 删除测试 (100,000 次)...\n");
        start = GetHighPrecisionTime();
        volatile int delete_dummy = 0;
        for (int k = 0; k < size; k++)
        {
            int idx = delete_order[k];
            delete_dummy += DM_DeleteSingle(stu_ids[idx], cou_ids[idx]);
        }
        if (delete_dummy < -999999999)
            printf("Impossible\n");
        end = GetHighPrecisionTime();
        delete_times[engine - 1] = (end - start) * 1000.0;
        printf("  [删除] 100,000 次耗时: %.2f ms\n", delete_times[engine - 1]);
        printf("  [验证] 删除后剩余: %d 条\n", DM_GetDataSize());
        DM_Destroy();
    }

    // ================= ★ 性能红黑榜 (中文对齐修复) ★ =================
    int best_insert = 0, best_search = 0, best_delete = 0, best_mem = 0;
    for (int i = 1; i < 3; i++)
    {
        if (insert_times[i] < insert_times[best_insert])
            best_insert = i;
        if (search_times[i] < search_times[best_search])
            best_search = i;
        if (delete_times[i] < delete_times[best_delete])
            best_delete = i;
        if (memory_usages[i] < memory_usages[best_mem])
            best_mem = i;
    }

    printf("\n================ 10万条压测 - 性能红黑榜 ================\n");
    printf("  [⚡ 插入最快] ");
    PRINT_PADDED(engine_names[best_insert], 6);
    printf(" (%.2f ms)\n", insert_times[best_insert]);
    printf("  [⚡ 查找最快] ");
    PRINT_PADDED(engine_names[best_search], 6);
    printf(" (%.2f ms)\n", search_times[best_search]);
    printf("  [⚡ 删除最快] ");
    PRINT_PADDED(engine_names[best_delete], 6);
    printf(" (%.2f ms)\n", delete_times[best_delete]);
    printf("  [💾 内存最省] ");
    PRINT_PADDED(engine_names[best_mem], 6);
    printf(" (%.4f MB)\n", memory_usages[best_mem]);
    printf("===========================================================\n");

    // ================= ★ 综合评分矩阵 (中文对齐修复) ★ =================
    int i_rank[3] = {0}, s_rank[3] = {0}, d_rank[3] = {0};
    for (int i = 0; i < 3; i++)
    {
        int r_i = 1, r_s = 1, r_d = 1;
        for (int j = 0; j < 3; j++)
        {
            if (insert_times[j] < insert_times[i])
                r_i++;
            if (search_times[j] < search_times[i])
                r_s++;
            if (delete_times[j] < delete_times[i])
                r_d++;
        }
        i_rank[i] = 4 - r_i;
        s_rank[i] = 4 - r_s;
        d_rank[i] = 4 - r_d;
    }

    int total_scores[3] = {0};
    int best_total = 0;
    for (int i = 0; i < 3; i++)
    {
        total_scores[i] = i_rank[i] + s_rank[i] + d_rank[i];
        if (total_scores[i] > total_scores[best_total])
            best_total = i;
    }

    printf("\n================ 💡 10万级数据选型结论 ==================\n");
    printf("【综合评分】(单项第1名3分, 第2名2分, 第3名1分)\n");
    // 表头手动硬编码空格，确保与下方 %8d 的显示宽度完美对齐
    printf("引擎     |     插入 |     查找 |     删除 |     总分 | 评价\n");
    printf("---------+----------+----------+----------+----------+----------\n");
    for (int i = 0; i < 3; i++)
    {
        PRINT_PADDED(engine_names[i], 8); // 引擎列宽8格
        printf(" | %8d | %8d | %8d | %8d | %s\n",
               i_rank[i], s_rank[i], d_rank[i], total_scores[i],
               (i == best_total) ? "★ 综合最优" : "");
    }
    printf("---------------------------------------------------------\n");
    printf("🏆 最终选型结论: 在10万条数据规模下，【%s】综合性能最佳！\n", engine_names[best_total]);
    printf("💡 架构建议: 本系统已采用【自适应策略】(阈值5000条)，\n");
    printf("   小数据量使用【三引擎全同步】保证强一致，\n");
    printf("   大数据量自动切换至【%s】追求极致性能！\n", engine_names[best_total]);
    printf("===========================================================\n");

    // ================= 内存占用汇总 (中文对齐修复) =================
    printf("\n================ 内存占用汇总 (10万条数据) ================\n");
    for (int i = 0; i < 3; i++)
    {
        printf("  [");
        PRINT_PADDED(engine_names[i], 6);
        printf("] 内存占用: %.4f MB\n", memory_usages[i]);
    }
    printf("===========================================================\n");

#undef PRINT_PADDED // 释放宏定义

    free(data_pool);
    for (int k = 0; k < size; k++)
    {
        free(stu_ids[k]);
        free(cou_ids[k]);
    }
    free(stu_ids);
    free(cou_ids);
    free(delete_order);
    free(search_order);

    printf("\n[✓ 成功] 压力测试完成。\n");
    WaitEnter();
}
/* =========================================
[附加2] 外部排序 (处理超出内存限制的大文件)
========================================= */
static void Demo_ExternalSortShow()
{
    printf("\n--- 外部排序演示 (ESC取消) ---\n");
    char confirm[10] = {0};
    int ret = ReadStringWithDefault("确认生成10万条数据并排序？(y/n):  ", confirm, sizeof(confirm));
    if (ret != RET_OK || (strcmp(confirm, "y") != 0 && strcmp(confirm, "Y") != 0))
    {
        WaitEnter();
        return;
    }

    printf("\n[1/4] 生成数据...\n");
    int gen_count = GenerateMockDataToFile("unsorted_100k.csv", 100000);
    if (gen_count <= 0)
    {
        printf("[错误] 生成失败！\n");
        WaitEnter();
        return;
    }

    printf("[2/4] 执行排序...\n");
    // 调用外部排序算法（如多路归并），避免一次性加载10万条数据导致内存溢出

    int sort_ret = ExternalSortByStudentID("unsorted_100k.csv", "sorted_100k.csv");
    if (sort_ret == 0)
    {
        printf("[3/4] 排序完成！\n");
        printf("[4/4] 清理文件...\n");
        remove("unsorted_100k.csv");
        printf("[✓ OK] 演示完成。\n");
    }
    else
        printf("[错误] 排序失败！\n");
    WaitEnter();
}

/* =========================================
主入口：任务书专项演示菜单路由
========================================= */
void Menu_TaskDemo(void)
{
    while (1)
    {
        printf("\n================================================================\n");
        printf("             任务书专项演示与高级控制台\n");
        printf("================================================================\n");
        printf(" 1. [任务1] 基础操作演示\n");
        printf(" 2. [快捷] 从CSV导入初始数据\n");
        printf(" 3. [任务2] 数据持久化(保存/查看)\n");
        printf(" 4. [任务3-1] 筛选\n");
        printf(" 5. [任务3-2] 排序\n");
        printf(" 6. [任务4] 统计分析\n");
        printf(" 7. [任务5] 清理过期\n");
        printf(" 8. [任务6] 性能对比\n");
        printf(" 9. [附加1] 10万条压测\n");
        printf(" 10.[附加2] 外部排序\n");
        printf(" 0. 退出系统\n");
        printf("================================================================\n");

        char choice[10] = {0};
        int ret = ReadStringWithDefault("请输入选项 (0-10):   ", choice, sizeof(choice));

        if (ret == RET_BACK || ret == RET_HOME)
        {
            if (ret == RET_HOME)
                continue;
            break;
        }
        if (ret == RET_QUIT)
        {
            // 按 q 退出，同样触发保存询问
            goto EXIT_PROMPT;
        }

        int opt = atoi(choice);
        if (opt == 0 && strcmp(choice, "0") != 0)
        {
            printf("\n[错误] 无效选项！\n");
            WaitEnter();
            continue;
        }
        if (opt == 0)
        {
            // ★ 修改：不再 break 返回主菜单，而是直接触发退出保存逻辑
            printf("\n============================================================\n");
            printf("                    退出确认\n");
            printf("============================================================\n");

            if (DM_GetDataSize() > 0 && g_data_modified)
            {
                printf("[提示] 当前系统中有 %d 条数据，且数据已被修改。\n", DM_GetDataSize());
                printf("请选择操作：\n  [1] 保存并退出\n  [2] 不保存直接退出\n  [3] 取消退出\n");
                // ✅ 新代码（支持直接回车）：
                char save_opt[10] = {0};
                printf("请输入选项 (1/2/3): ");
                fflush(stdout);
                if (fgets(save_opt, sizeof(save_opt), stdin) != NULL)
                    save_opt[strcspn(save_opt, "\n\r")] = '\0'; // 去除末尾的换行符

                if (strcmp(save_opt, "1") == 0)
                {
                    // 获取默认路径（你之前加的 DM_GetLastLoadedFile 逻辑）
                    const char *default_file = DM_GetLastLoadedFile();
                    if (strlen(default_file) == 0)
                        default_file = "data.csv";
                    printf("默认保存路径: %s (直接回车确认): ", default_file);

                    // 当选择 1 保存时：
                    char path[256] = {0};
                    printf("默认保存路径: %s (直接回车确认): ", default_file);
                    fflush(stdout);
                    if (fgets(path, sizeof(path), stdin) != NULL)
                        path[strcspn(path, "\n\r")] = '\0'; // 去除末尾的换行符

                    if (strlen(path) == 0)
                        strcpy(path, default_file); // 如果直接回车，使用默认路径

                    int count = DM_SaveToFile(path);
                    if (count >= 0)
                        printf("[OK] 保存成功！\n");
                }
            }
            else
            {
                printf("确认退出？(y/n): ");
                char c[10];
                scanf("%s", c);
                if (strcmp(c, "y") != 0 && strcmp(c, "Y") != 0)
                    return; // 取消退出
            }

            printf("\n感谢使用，再见！\n");
            exit(0); // 直接结束程序
        }

        // 路由到对应的Demo函数
        switch (opt)
        {
        case 1:
            Demo_BasicOperations();
            break;
        case 2:
            Demo_LoadFromCSV();
            break;
        case 3:
            Demo_Persistence();
            break;
        case 4:
            Demo_Filter();
            break;
        case 5:
            Demo_Sort();
            break;
        case 6:
            Demo_Statistics();
            break;
        case 7:
            Demo_CleanExpiredRecords();
            break;
        case 8:
            Demo_PerformanceReport_Base();
            break;
        case 9:
            Demo_PerformanceReport_Stress();
            break;
        case 10:
            Demo_ExternalSortShow();
            break;
        default:
            printf("\n[错误] 无效选项！\n");
            WaitEnter();
            break;
        }
    }
    return;

EXIT_PROMPT:
    // 退出前的保存询问逻辑
    printf("\n============================================================\n");
    printf("                    退出确认\n");
    printf("============================================================\n");

    if (DM_GetDataSize() > 0 && g_data_modified)
    {
        printf("[提示] 当前系统中有 %d 条数据，且数据已被修改。\n", DM_GetDataSize());
        printf("请选择操作：\n");
        printf("  [1] 保存并退出\n");
        printf("  [2] 不保存直接退出\n");
        printf("  [3] 取消退出，返回菜单\n");

        char save_opt[10] = {0};
        int save_ret = ReadStringWithDefault("请输入选项 (1/2/3):   ", save_opt, sizeof(save_opt));

        if (save_ret != RET_OK)
        {
            printf("\n[提示] 已取消退出。\n");
            return; // 返回到菜单循环
        }

        if (strcmp(save_opt, "3") == 0)
        {
            printf("\n[提示] 已取消退出。\n");
            return;
        }

        if (strcmp(save_opt, "1") == 0)
        {
            char save_path[256] = {0};
            printf("\n[提示] 默认保存路径: data.csv\n");
            int path_ret = ReadStringWithDefault("请输入保存路径（直接回车使用默认路径）:   ", save_path, sizeof(save_path));

            if (path_ret != RET_OK || strlen(save_path) == 0)
            {
                strcpy(save_path, "data.csv");
            }

            printf("\n正在保存数据到 %s ...\n", save_path);
            int count = DM_SaveToFile(save_path);
            if (count >= 0)
            {
                printf("[OK] 成功保存 %d 条记录至 %s\n", count, save_path);
                g_data_modified = false;
            }
            else
            {
                printf("[错误] 保存失败！请检查路径权限。\n");
            }
        }
        // 选 2 则不保存直接退出
    }
    else if (DM_GetDataSize() > 0)
    {
        // 有数据但未修改
        printf("[提示] 当前系统中有 %d 条数据（未修改）。\n", DM_GetDataSize());
        printf("  [1] 保存并退出\n");
        printf("  [2] 不保存直接退出\n");
        printf("  [3] 取消退出，返回菜单\n");

        char save_opt[10] = {0};
        int save_ret = ReadStringWithDefault("请输入选项 (1/2/3):   ", save_opt, sizeof(save_opt));

        if (save_ret != RET_OK || strcmp(save_opt, "3") == 0)
        {
            printf("\n[提示] 已取消退出。\n");
            return;
        }

        if (strcmp(save_opt, "1") == 0)
        {
            char save_path[256] = {0};
            printf("\n[提示] 默认保存路径: data.csv\n");
            int path_ret = ReadStringWithDefault("请输入保存路径（直接回车使用默认路径）:   ", save_path, sizeof(save_path));

            if (path_ret != RET_OK || strlen(save_path) == 0)
            {
                strcpy(save_path, "data.csv");
            }

            printf("\n正在保存数据到 %s ...\n", save_path);
            int count = DM_SaveToFile(save_path);
            if (count >= 0)
            {
                printf("[OK] 成功保存 %d 条记录至 %s\n", count, save_path);
            }
            else
            {
                printf("[错误] 保存失败！\n");
            }
        }
    }
    else
    {
        // 无数据，直接确认退出
        printf("[提示] 系统中暂无数据。\n");
        char confirm[10] = {0};
        int c_ret = ReadStringWithDefault("确认退出？(y/n):   ", confirm, sizeof(confirm));
        if (c_ret != RET_OK || (strcmp(confirm, "y") != 0 && strcmp(confirm, "Y") != 0))
        {
            printf("\n[提示] 已取消退出。\n");
            return;
        }
    }

    printf("\n============================================================\n");
    printf("                    感谢使用，再见！\n");
    printf("============================================================\n");
}