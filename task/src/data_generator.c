/**
 * @file data_generator.c
 * @brief 模拟高校教务系统学生选课与成绩数据生成器
 *
 * 功能说明：
 * 1. 支持生成符合特定编码规则的学号、随机中文姓名。
 * 2. 支持生成包含必修课、选修课、补考、挂科等真实教务逻辑的成绩单数据。
 * 3. 采用内存缓冲区批量写入磁盘，大幅提升大文件（百万级以上）的生成速度。
 * 4. 提供两种生成模式：【真实教务系统模式】（数据量大、逻辑严密）和【高随机性测试模式】（完全随机、用于压力测试）。
 */
#define _CRT_SECURE_NO_WARNINGS // 禁用MSVC的安全函数警告（如strcpy -> strcpy_s）
#define _POSIX_C_SOURCE 199309L // 启用POSIX标准特性（如clock_gettime等，视具体系统而定）
#include "data_generator.h"
#include "data_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include "common.h"
// ================= 跨平台头文件与宏定义 =================
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#ifndef getpid
#define getpid _getpid // Windows下获取进程ID的函数名映射
#endif
#else
#include <unistd.h>
#endif
// ================= 静态数据字典（用于随机生成姓名） =================
static const char *SURNAMES[] = {
    "李", "王", "张", "刘", "陈", "杨", "赵", "黄", "周", "吴",
    "徐", "孙", "马", "朱", "胡", "郭", "何", "高", "林", "罗",
    "郑", "梁", "谢", "宋", "唐", "许", "韩", "冯", "邓", "曹",
    "彭", "曾", "萧", "田", "董", "袁", "潘", "于", "蒋", "蔡",
    "余", "杜", "叶", "程", "苏", "魏", "吕", "丁", "任", "沈",
    "姚", "卢", "姜", "崔", "钟", "谭", "陆", "汪", "范", "金",
    "石", "廖", "贾", "夏", "韦", "付", "方", "白", "邹", "孟",
    "熊", "秦", "邱", "江", "尹", "黎", "易", "常", "武", "乔",
    "贺", "赖", "龚", "文", "庞", "樊", "兰", "殷", "施", "陶",
    "洪", "翟", "安", "颜", "倪", "严", "牛", "温", "芦", "季"};
#define SURNAMES_COUNT 100
// 双字名库
static const char *GIVEN_NAMES_2[] = {
    "伟", "芳", "娜", "敏", "静", "丽", "强", "磊", "军", "洋",
    "勇", "艳", "杰", "娟", "涛", "明", "超", "秀英", "霞", "云",
    "玲", "桂", "英", "华", "平", "燕", "辉", "玲", "桂兰", "建华",
    "建国", "红梅", "志强", "秀兰", "小平", "玉兰", "建军", "海涛",
    "秀珍", "海燕", "丽娟", "玉兰", "小芳", "小红", "小明", "小华",
    "志强", "志华", "志军", "志伟", "志敏", "志平", "志勇", "志刚"};
#define GIVEN_NAMES_2_COUNT 54
// 三字名库（姓 + 辈分/修饰字 + 名）
static const char *GIVEN_NAMES_3_FIRST[] = {
    "志", "建", "晓", "小", "文", "玉", "金", "春", "秋", "冬",
    "海", "江", "河", "山", "天", "明", "德", "永", "国", "学"};
#define GIVEN_NAMES_3_FIRST_COUNT 20

static const char *GIVEN_NAMES_3_SECOND[] = {
    "强", "华", "军", "伟", "明", "辉", "龙", "凤", "霞", "梅",
    "兰", "菊", "松", "柏", "涛", "波", "峰", "林", "森", "磊"};
#define GIVEN_NAMES_3_SECOND_COUNT 20
// ================= 学院与课程信息定义 =================
typedef struct
{
    const char *code;   // 学院代码（如"01"）
    const char *prefix; // 课程前缀（如"CS"）
    const char *name;   // 学院名称
    int subject_type;   // 学科类型（1:理工, 2:理学, 3:文管）
    bool is_popular;    // 是否为热门学院（影响学号生成时的分布）
} CollegeInfo;

static const CollegeInfo COLLEGES[] = {
    {"01", "CS", "计算机学院", 1, true}, {"02", "EE", "电信学院", 1, true}, {"03", "ME", "机械学院", 1, false}, {"04", "CE", "土木学院", 1, false}, {"05", "MA", "数学学院", 2, false}, {"06", "PH", "物理学院", 2, false}, {"07", "EM", "经管学院", 3, true}, {"08", "FL", "外语学院", 3, false}};
#define COLLEGES_COUNT 8

typedef struct
{
    const char *id;   // 课程ID（规则：前缀2位+性质1位+年级1位+学期2位+序号2位，如CS102026）
    const char *name; // 课程名称
    float credit;     // 学分
} CourseInfo;

// 课程库（包含通识课GE、专业课CS/EE等，第4位'0'为必修，'1'为选修）
static const CourseInfo COURSES[] = {
    {"GE001012", "思想政治理论", 2.0f}, {"GE000011", "体育", 1.0f}, {"GE001011", "心理健康", 1.0f}, {"MA201017", "高等数学A", 4.0f}, {"GE011022", "创新创业基础", 1.5f}, {"GE011013", "大学生职业规划", 1.0f}, {"GE011024", "艺术鉴赏", 1.5f}, {"CS102026", "数据结构与算法", 3.5f}, {"CS114014", "人工智能导论", 2.0f}, {"CS113025", "Web开发技术", 2.5f}, {"EE102025", "电路分析", 3.0f}, {"EE113024", "物联网技术", 2.5f}, {"ME102025", "工程力学", 3.0f}, {"ME113024", "机器人技术", 2.5f}, {"CE102026", "结构力学", 3.5f}, {"MA201025", "线性代数", 3.0f}, {"MA212026", "数学建模", 2.5f}, {"EM301014", "微观经济学", 2.5f}, {"EM312023", "电子商务", 2.0f}, {"FL301015", "综合英语", 3.0f}};
#define COURSES_COUNT 20
// ================= 全局IO缓冲区（优化大文件写入性能） =================
#define IO_BUFFER_SIZE (256 * 1024) // 256KB 缓冲区
static char *g_io_buffer = NULL;
static int g_io_len = 0;
static FILE *g_fp = NULL;

/**
 * @brief 将缓冲区内容刷入磁盘文件
 */
static void FlushIOBuffer(void)
{
    if (g_io_len > 0 && g_fp && g_io_buffer)
    {
        fwrite(g_io_buffer, 1, g_io_len, g_fp);
        fflush(g_fp);
        g_io_len = 0;
    }
}
/**
 * @brief 将一行数据追加到内存缓冲区，若空间不足则先刷盘
 */
static void AppendLineToBuffer(const char *line, int len)
{
    if (!g_io_buffer)
        return;
    if (g_io_len + len >= IO_BUFFER_SIZE - 1)
        FlushIOBuffer(); // 缓冲区满，先写入磁盘
    memcpy(g_io_buffer + g_io_len, line, len);
    g_io_len += len;
}
// ================= 基础数据生成与查询函数 =================

/**
 * @brief 初始化随机数种子（结合时间、CPU时钟和进程ID，保证高随机性）
 */
void DataGenerator_Init(void)
{
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)clock() ^ (unsigned int)getpid();
    srand(seed);
}
/**
 * @brief 根据索引生成12位学号
 * 规则：YYYY(4) + CC(2,学院) + MM(2,专业) + Cls(2,班级) + Seq(2,序号)
 * 逻辑：前5400个索引分配给热门学院(01,02,07)，后续分配给冷门学院，模拟真实招生人数差异。
 */
void GenerateStudentID(int index, char *stu_id, int size)
{
    if (index < 0 || index >= 64050)
    {
        snprintf(stu_id, size, "%s", "202001010101");
        return;
    }
    int year = 2020 + (index / 9150);
    int rem = index % 9150;
    int col, major, cls, seq;
    if (rem < 5400) // 热门学院（计算机、电信、经管），每院1800人
    {
        int hot_idx = rem / 1800;
        rem = rem % 1800;
        int col_map[] = {1, 2, 7};
        col = col_map[hot_idx];
        major = rem / 180 + 1; // 每院3个专业，每专业180人
        rem = rem % 180;
        cls = rem / 30 + 1; // 每专业6个班，每班30人
        seq = rem % 30 + 1;
    }
    else
    { // 冷门学院，每院750人
        rem -= 5400;
        int cold_idx = rem / 750;
        rem = rem % 750;
        int col_map[] = {3, 4, 5, 6, 8};
        col = col_map[cold_idx];
        major = rem / 75 + 1; // 每院5个专业，每专业75人
        rem = rem % 75;
        cls = rem / 25 + 1; // 每专业3个班，每班25人
        seq = rem % 25 + 1;
    }
    snprintf(stu_id, size, "%04d%02d%02d%02d%02d", year, col, major, cls, seq);
}
/**
 * @brief 根据索引生成中文姓名（2字或3字）
 */
void GenerateStudentName(int index, char *name, int size)
{
    if (index < 0)
        index = -index;
    index = index % 100000;
    const char *surname = SURNAMES[index % SURNAMES_COUNT];
    int name_len = 2 + (index % 2); // 50%概率2字名，50%概率3字名
    if (name_len == 2)
    {
        const char *given_name = GIVEN_NAMES_2[(index / SURNAMES_COUNT) % GIVEN_NAMES_2_COUNT];
        snprintf(name, size, "%s%s", surname, given_name);
    }
    else
    {
        const char *first = GIVEN_NAMES_3_FIRST[(index / SURNAMES_COUNT) % GIVEN_NAMES_3_FIRST_COUNT];
        const char *second = GIVEN_NAMES_3_SECOND[(index / (SURNAMES_COUNT * GIVEN_NAMES_3_FIRST_COUNT)) % GIVEN_NAMES_3_SECOND_COUNT];
        snprintf(name, size, "%s%s%s", surname, first, second);
    }
}
/**
 * @brief 从学号中提取学院代码并映射为学院名称
 */
void GetCollegeFromStudentID(const char *student_id, char *college, int size)
{
    if (!student_id || strlen(student_id) < 6)
    {
        snprintf(college, size, "%s", "未知学院");
        return;
    }
    char college_code[3] = {0};
    memcpy(college_code, student_id + 4, 2); // 学号第5、6位是学院代码
    for (int i = 0; i < COLLEGES_COUNT; i++)
    {
        if (strcmp(COLLEGES[i].code, college_code) == 0)
        {
            snprintf(college, size, "%s", COLLEGES[i].name);
            return;
        }
    }
    snprintf(college, size, "%s", "未知学院");
}
// 课程信息相关查询函数
void GenerateCourseID(int index, char *course_id, int size)
{
    if (index < 0 || index >= COURSES_COUNT)
    {
        snprintf(course_id, size, "%s", "GE001012");
        return;
    }
    snprintf(course_id, size, "%s", COURSES[index].id);
}

void GetCourseNameFromID(const char *course_id, char *course_name, int size)
{
    for (int i = 0; i < COURSES_COUNT; i++)
        if (strcmp(COURSES[i].id, course_id) == 0)
        {
            snprintf(course_name, size, "%s", COURSES[i].name);
            return;
        }
    snprintf(course_name, size, "%s", "未知课程");
}

float GetCreditFromCourseID(const char *course_id)
{
    for (int i = 0; i < COURSES_COUNT; i++)
        if (strcmp(COURSES[i].id, course_id) == 0)
            return COURSES[i].credit;
    return 2.0f;
}
/**
 * @brief 使用 Box-Muller 变换生成符合正态分布的成绩
 * @param mean 期望均值
 * @param stddev 标准差
 * @param min_score 最低分截断
 * @param max_score 最高分截断
 * @param fail_rate 强制不及格（40-59分）的概率
 */
int GenerateScore(double mean, double stddev, int min_score, int max_score, double fail_rate)
{
    // 1. 按照 fail_rate 概率直接生成不及格分数
    if ((double)rand() / RAND_MAX < fail_rate)
        return 40 + (rand() % 20);
    // 2. Box-Muller 算法：利用两个均匀分布(u1, u2)生成标准正态分布 z0
    double u1 = (double)rand() / RAND_MAX;
    if (u1 < 1e-10)
        u1 = 1e-10; // 防止 log(0) 导致数学错误
    double u2 = (double)rand() / RAND_MAX;
    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979 * u2);
    // 3. 线性变换到目标正态分布并截断
    double score = mean + z0 * stddev;
    if (score < min_score)
        score = min_score;
    if (score > max_score)
        score = max_score;
    return (int)round(score);
}
/**
 * @brief 生成合理的选课/开学日期
 * @param is_elective 是否为选修课（选修课通常在开学初几周抢课，必修课在放假期间预排）
 */
void GenerateEnrollDate(const char *semester, char *date, int size, bool is_elective)
{
    if (!semester || strlen(semester) < 7)
    {
        snprintf(date, size, "2026-06-01");
        return;
    }
    int year = atoi(semester), sem_type = atoi(semester + 5), target_month = 0, day = 0;
    if (is_elective)
    {
        // 选修课抢课时间：春季学期3月，秋季学期10月
        target_month = (sem_type == 1) ? 3 : 10;
        day = (rand() % 2 == 0) ? (1 + rand() % 7) : (11 + rand() % 3);
    }
    else
    {
        // 必修课预排时间：春季学期上年12月-当年2月，秋季学期当年6-8月
        if (sem_type == 1)
        {
            int offset = rand() % 3;
            if (offset == 0)
            {
                target_month = 12;
                year--;
            }
            else if (offset == 1)
                target_month = 1;
            else
                target_month = 2;
        }
        else
            target_month = 6 + rand() % 3;
        day = 1 + rand() % 28;
    }
    snprintf(date, size, "%04d-%02d-%02d", year, target_month, day);
}
// 生成单条完整记录结构体（主要用于非文件直写的场景）
void GenerateMockRecord(int student_index, int course_index, CourseRecord *rec)
{
    char stu_id[20];
    GenerateStudentID(student_index, stu_id, sizeof(stu_id));
    SafeStrCopy(rec->student_id, MAX_STU_ID_LEN, stu_id);
    char name[50];
    GenerateStudentName(student_index, name, sizeof(name));
    SafeStrCopy(rec->name, MAX_NAME_LEN, name);
    GetCollegeFromStudentID(stu_id, rec->college, MAX_COLLEGE_LEN);
    char course_id[20];
    GenerateCourseID(course_index, course_id, sizeof(course_id));
    SafeStrCopy(rec->course_id, MAX_COU_ID_LEN, course_id);
    GetCourseNameFromID(course_id, rec->course_name, MAX_COU_NAME_LEN);
    rec->credit = GetCreditFromCourseID(course_id);
    int enroll_year = atoi(stu_id);
    bool is_elective = (course_id[3] == '1');
    int applicable_grade = is_elective ? 0 : (course_id[4] - '0');
    char sem_code[3] = {course_id[5], course_id[6], '\0'};
    int sem_type = atoi(sem_code);
    int sem_year = (applicable_grade == 0) ? enroll_year + (rand() % 4) : enroll_year + (applicable_grade - 1);
    char semester[20];
    snprintf(semester, sizeof(semester), "%04d-%02d", sem_year, sem_type);
    SafeStrCopy(rec->semester, MAX_SEMESTER_LEN, semester);
    char enroll_date[20];
    GenerateEnrollDate(semester, enroll_date, sizeof(enroll_date), is_elective);
    SafeStrCopy(rec->enroll_date, MAX_DATE_LEN, enroll_date);
    rec->score = GenerateScore(75.0, 12.0, 60, 100, 0.10);
    if (rand() % 100 < 5)
        rec->score = -1; // 5%概率缺考（记为-1）
    rec->is_elective = is_elective ? 1 : 0;
}
/**
 * @brief 根据学生类型生成成绩（学霸、普通、学渣）
 */
static int GenerateScoreByType(int student_type)
{
    if (student_type == 0)
        return GenerateScore(92.0, 4.0, 60, 100, 0.02); // 学霸：均值92，波动小
    if (student_type == 1)
        return GenerateScore(78.0, 6.0, 60, 100, 0.05); // 普通：均值78
    return GenerateScore(55.0, 15.0, 0, 100, 0.45);     // 学渣：均值55，极高挂科率
}
// 判断是否为容易拿高分的“水课”
static bool IsEasyCourse(const char *name)
{
    return (strstr(name, "基础") || strstr(name, "鉴赏") || strstr(name, "规划") || strstr(name, "心理") || strstr(name, "体育"));
}
// ================= 核心数据生成逻辑 =================
#define MAX_YEARS_SPAN 10                                 // 支持的最大年份跨度（2020-2029）
#define BASE_YEAR 2020                                    // 基础起始年份
#define CAPACITY_LIMIT_THRESHOLD 10000                    // 开启课程容量限制的记录数阈值
#define REALISTIC_MODE_THRESHOLD 50000                    // 启用真实教务模式的记录数阈值
#define MAX_COURSE_CAPACITY 180                           // 单门课单学期的最大容量
#define CAPACITY_INDEX(c, y) ((c) * MAX_YEARS_SPAN + (y)) // 计算容量数组的1D索引
/**
 * @brief 尝试写入一条选课记录到缓冲区
 * @return 返回生成的成绩，若因容量或年份问题写入失败则返回 -1
 */
static int TryWriteRecord(const char *stu_id, const char *name, const char *college,
                          const CourseInfo *course, int stu_year, int sem_type_override,
                          int *capacity, int *success_count, int threshold,
                          int max_count, bool force_elective_date)
{
    if (*success_count >= max_count)
        return -1;

    bool is_elective = (course->id[3] == '1') || force_elective_date;
    int applicable_grade = is_elective ? 0 : (course->id[4] - '0');
    char sem_code[3] = {course->id[5], course->id[6], '\0'};
    int sem_type = sem_type_override > 0 ? sem_type_override : atoi(sem_code);

    // 计算该课程开设的年份
    int course_year;
    if (applicable_grade == 0)
    {
        // 选修课：在入学到当前年份(2026)之间随机
        int range = 2026 - stu_year + 1;
        if (range < 1)
            range = 1; // 防御性保护
        course_year = stu_year + rand() % range;
    }
    else
    {
        // 必修课：按培养方案固定年级开设
        course_year = stu_year + applicable_grade - 1;
    }
    if (course_year > 2026)
        course_year = 2026;
    if (course_year < BASE_YEAR)
        course_year = BASE_YEAR;

    int year_offset = course_year - BASE_YEAR;
    if (year_offset < 0 || year_offset >= MAX_YEARS_SPAN)
        return -1;

    int course_idx = (int)(course - COURSES);
    // 容量检查：仅在数据量小于阈值时限制容量，防止小批量生成时因随机性导致无法生成
    if (*success_count < threshold && capacity[CAPACITY_INDEX(course_idx, year_offset)] >= MAX_COURSE_CAPACITY)
        return -1;

    char semester[20];
    snprintf(semester, sizeof(semester), "%04d-%02d", course_year, sem_type);
    char enroll_date[20];
    GenerateEnrollDate(semester, enroll_date, sizeof(enroll_date), is_elective);
    if (strcmp(enroll_date, "2026-12-31") > 0)
        return -1; // 过滤掉穿越到未来的数据

    int score = GenerateScore(75.0, 12.0, 0, 100, 0.10);
    if (rand() % 100 < 3)
        score = -1; // 3%缺考率
    // 格式化CSV行并写入缓冲区
    char line[512];
    int line_len = snprintf(line, sizeof(line), "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n",
                            stu_id, name, college, course->id, course->name,
                            course->credit, semester, enroll_date, score, is_elective ? 1 : 0);
    if (line_len > 0 && line_len < (int)sizeof(line))
        AppendLineToBuffer(line, line_len);

    if (*success_count < threshold)
        capacity[CAPACITY_INDEX(course_idx, year_offset)]++;
    (*success_count)++;
    return score;
}
/**
 * @brief 主生成函数：将模拟数据写入CSV文件
 */
int GenerateMockDataToFile(const char *filename, int count)
{
    remove(filename);
    g_fp = fopen(filename, "wb");
    if (!g_fp)
    {
        printf("[错误] 无法创建文件 %s\n", filename);
        return -1;
    }
    if (!g_io_buffer)
    {
        g_io_buffer = (char *)malloc(IO_BUFFER_SIZE);
        if (!g_io_buffer)
        {
            fclose(g_fp);
            g_fp = NULL; // ★修复：置空防止悬空指针
            return -1;
        }
    }
    g_io_len = 0;
    // 写入CSV表头
    const char *header = "student_id,name,college,course_id,course_name,credit,semester,enroll_date,score,is_elective\n";
    AppendLineToBuffer(header, strlen(header));

    int success_count = 0;
    // 记录每门课在每个学期的选课人数，用于容量控制
    int *course_year_capacity = (int *)calloc(COURSES_COUNT * MAX_YEARS_SPAN, sizeof(int));
    if (!course_year_capacity)
    {
        FlushIOBuffer();
        fclose(g_fp);
        g_fp = NULL; // ★修复：置空防止悬空指针
        return -1;
    }
    // ================= 模式一：真实教务系统模式 =================
    if (count >= REALISTIC_MODE_THRESHOLD)
    {
        printf("[模式] 启用【真实教务系统模式】...\n");
        // 计算选修课的初始权重（水课、高学分课权重更高）
        float elective_weights[COURSES_COUNT];
        for (int i = 0; i < COURSES_COUNT; i++)
        {
            if (COURSES[i].id[3] == '1') // 仅选修课参与权重计算
            {
                float w = 1.0f;
                if (IsEasyCourse(COURSES[i].name))
                    w += 5.0f; // 水课受欢迎
                if (COURSES[i].credit >= 2.0f)
                    w += 3.0f; // 学分高的课受欢迎
                elective_weights[i] = w;
            }
            else
            {
                elective_weights[i] = 0.0f;
            }
        }

        int stu_idx = 0;
        while (success_count < count && stu_idx < count * 5)
        {
            stu_idx++;
            // 随机生成学生基本信息
            int stu_year = BASE_YEAR + rand() % (2026 - BASE_YEAR + 1);
            int col_code = 1 + rand() % 8, major_code = 1 + rand() % 5, class_num = 1 + rand() % 10, seq = 1 + rand() % 30;
            char stu_id[20];
            snprintf(stu_id, sizeof(stu_id), "%04d%02d%02d%02d%02d", stu_year, col_code, major_code, class_num, seq);
            char name[50], college[50];
            GenerateStudentName(rand(), name, sizeof(name));
            GetCollegeFromStudentID(stu_id, college, sizeof(college));
            // 划分学生类型：20%学霸，64%普通，16%学渣
            int student_type = (rand() % 100 < 20) ? 0 : (rand() % 100 < 80 ? 1 : 2);
            float target_elective_credits = (student_type == 0) ? 12.0f : (student_type == 1 ? 8.0f : 4.0f);
            // 1. 处理必修课（含挂科补考逻辑）
            for (int c = 0; c < COURSES_COUNT && success_count < count; c++)
            {
                bool is_req = (strncmp(COURSES[c].id, "GE", 2) == 0 && COURSES[c].id[3] == '0') || (strcmp(COURSES[c].id, "MA201017") == 0);
                // 判断是否为本专业必修课
                if (!is_req && COURSES[c].id[3] == '0')
                {
                    if ((col_code == 1 && strncmp(COURSES[c].id, "CS", 2) == 0) || (col_code == 2 && strncmp(COURSES[c].id, "EE", 2) == 0) ||
                        (col_code == 3 && strncmp(COURSES[c].id, "ME", 2) == 0) || (col_code == 4 && strncmp(COURSES[c].id, "CE", 2) == 0) ||
                        (col_code == 5 && strncmp(COURSES[c].id, "MA", 2) == 0) || (col_code == 7 && strncmp(COURSES[c].id, "EM", 2) == 0) ||
                        (col_code == 8 && strncmp(COURSES[c].id, "FL", 2) == 0))
                        is_req = true;
                }

                if (is_req)
                {
                    int score = TryWriteRecord(stu_id, name, college, &COURSES[c], stu_year, 0,
                                               course_year_capacity, &success_count,
                                               CAPACITY_LIMIT_THRESHOLD, count, false);
                    if (success_count >= count)
                        break;
                    // 补考逻辑：如果挂科且不是大四（防止年份越界），则在下一学期生成补考记录
                    if (score != -1 && score < 60 && stu_year < 2026)
                        TryWriteRecord(stu_id, name, college, &COURSES[c], stu_year + 1,
                                       (rand() % 2) + 1, course_year_capacity, &success_count,
                                       CAPACITY_LIMIT_THRESHOLD, count, true);
                }
            }
            if (success_count >= count)
                break;
            // 2. 处理选修课（按权重轮盘赌抽取，直到修满目标学分）
            float current_elective_credits = 0.0f;
            int max_attempts = 100;
            while (current_elective_credits < target_elective_credits && max_attempts-- > 0 && success_count < count)
            {
                // 动态计算当前学生的选课权重
                float dynamic_weights[COURSES_COUNT];
                float dynamic_total = 0.0f;
                for (int i = 0; i < COURSES_COUNT; i++)
                {
                    if (COURSES[i].id[3] == '1')
                    {
                        float w = elective_weights[i];
                        // 学霸偏好：本专业选修课权重极高
                        if (student_type == 0)
                        {
                            bool is_major_elective = false;
                            if ((col_code == 1 && strncmp(COURSES[i].id, "CS", 2) == 0) || (col_code == 2 && strncmp(COURSES[i].id, "EE", 2) == 0) ||
                                (col_code == 3 && strncmp(COURSES[i].id, "ME", 2) == 0) || (col_code == 4 && strncmp(COURSES[i].id, "CE", 2) == 0) ||
                                (col_code == 5 && strncmp(COURSES[i].id, "MA", 2) == 0) || (col_code == 7 && strncmp(COURSES[i].id, "EM", 2) == 0) ||
                                (col_code == 8 && strncmp(COURSES[i].id, "FL", 2) == 0))
                                is_major_elective = true;
                            if (is_major_elective)
                                w += 8.0f + (float)(rand() % 10);
                        }
                        // 学渣偏好：高学分课（可能为了凑学分）
                        if (student_type == 2 && COURSES[i].credit >= 2.5f)
                            w += 5.0f + (float)(rand() % 8);
                        w += (float)(rand() % 5) * 0.2f; // 增加随机扰动
                        dynamic_weights[i] = w;
                        dynamic_total += w;
                    }
                    else
                    {
                        dynamic_weights[i] = 0.0f;
                    }
                }
                if (dynamic_total <= 0.0f)
                    break;
                // 轮盘赌算法选择课程
                float r = ((float)rand() / RAND_MAX) * dynamic_total;
                float cumulative = 0.0f;
                int selected_course_idx = -1;
                for (int i = 0; i < COURSES_COUNT; i++)
                {
                    if (dynamic_weights[i] > 0.0f)
                    {
                        cumulative += dynamic_weights[i];
                        if (r <= cumulative)
                        {
                            selected_course_idx = i;
                            break;
                        }
                    }
                }
                if (selected_course_idx == -1)
                    continue;
                // 写入选修课记录（逻辑同TryWriteRecord，此处内联以支持动态权重和学分统计）
                int course_idx = selected_course_idx;
                char sc[3] = {COURSES[course_idx].id[5], COURSES[course_idx].id[6], '\0'};
                int sem_type = atoi(sc);
                int applicable_grade = (COURSES[course_idx].id[3] == '1') ? 0 : (COURSES[course_idx].id[4] - '0');
                int course_year = (applicable_grade == 0) ? (stu_year + rand() % (2026 - stu_year + 1)) : (stu_year + applicable_grade - 1);
                if (course_year > 2026)
                    course_year = 2026;
                if (course_year < BASE_YEAR)
                    course_year = BASE_YEAR;
                int year_offset = course_year - BASE_YEAR;
                if (year_offset < 0 || year_offset >= MAX_YEARS_SPAN)
                    continue;
                if (success_count < CAPACITY_LIMIT_THRESHOLD && course_year_capacity[CAPACITY_INDEX(course_idx, year_offset)] >= MAX_COURSE_CAPACITY)
                    continue;
                char semester[20];
                snprintf(semester, sizeof(semester), "%04d-%02d", course_year, sem_type);
                char enroll_date[20];
                GenerateEnrollDate(semester, enroll_date, sizeof(enroll_date), true);
                if (strcmp(enroll_date, "2026-12-31") > 0)
                    continue;
                int score = GenerateScoreByType(student_type); // 根据学生类型生成符合人设的成绩
                if (rand() % 100 < 3)
                    score = -1;
                char line[512];
                int line_len = snprintf(line, sizeof(line), "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n", stu_id, name, college, COURSES[course_idx].id, COURSES[course_idx].name, COURSES[course_idx].credit, semester, enroll_date, score, 1);
                if (line_len > 0 && line_len < (int)sizeof(line))
                    AppendLineToBuffer(line, line_len);
                if (success_count < CAPACITY_LIMIT_THRESHOLD)
                    course_year_capacity[CAPACITY_INDEX(course_idx, year_offset)]++;
                success_count++;
                if (success_count >= count)
                    break;
                current_elective_credits += COURSES[course_idx].credit;
            }
            // 进度打印
            if (stu_idx % 2000 == 0)
            {
                FlushIOBuffer();
                printf("[进度] 学生 %d, 记录 %d\n", stu_idx, success_count);
            }
        }
    }
    // ================= 模式二：高随机性测试模式 =================
    else
    {
        printf("[模式] 启用【高随机性测试模式】...\n");
        int fail_attempts = 0, max_fail_attempts = count * 200 + 2000000;
        int consecutive_fails = 0, capacity_auto_released = 0;
        while (success_count < count && fail_attempts < max_fail_attempts)
        {
            int stu_year = BASE_YEAR + rand() % (2026 - BASE_YEAR + 1);
            int col_code = 1 + rand() % 8, major_code = 1 + rand() % 5, class_num = 1 + rand() % 10, seq = 1 + rand() % 30;
            char stu_id[20];
            snprintf(stu_id, sizeof(stu_id), "%04d%02d%02d%02d%02d", stu_year, col_code, major_code, class_num, seq);
            char name[50], college[50];
            GenerateStudentName(rand(), name, sizeof(name));
            GetCollegeFromStudentID(stu_id, college, sizeof(college));
            int course_idx = rand() % COURSES_COUNT;
            bool is_elective = (COURSES[course_idx].id[3] == '1');
            int applicable_grade = is_elective ? 0 : (COURSES[course_idx].id[4] - '0');
            int course_year = (applicable_grade == 0) ? (stu_year + rand() % (2026 - stu_year + 1)) : (stu_year + applicable_grade - 1);
            if (course_year > 2026)
                course_year = 2026;
            if (course_year < BASE_YEAR)
                course_year = BASE_YEAR;
            int year_offset = course_year - BASE_YEAR;
            // 年份越界保护
            if (year_offset < 0 || year_offset >= MAX_YEARS_SPAN)
            {
                fail_attempts++;
                consecutive_fails++;

                year_offset = (year_offset < 0) ? 0 : (MAX_YEARS_SPAN - 1);
                course_year = BASE_YEAR + year_offset;
            }
            // 容量限制与自动释放机制（防止死循环）
            if (!capacity_auto_released && success_count < CAPACITY_LIMIT_THRESHOLD && course_year_capacity[CAPACITY_INDEX(course_idx, year_offset)] >= MAX_COURSE_CAPACITY)
            {
                fail_attempts++;
                consecutive_fails++;
                if (consecutive_fails > 50000 && success_count > 500)
                {
                    capacity_auto_released = 1;
                }
                continue;
            }
            char sc[3] = {COURSES[course_idx].id[5], COURSES[course_idx].id[6], '\0'};
            int sem_type = atoi(sc);
            char semester[20];
            snprintf(semester, sizeof(semester), "%04d-%02d", course_year, sem_type);
            char enroll_date[20];
            GenerateEnrollDate(semester, enroll_date, sizeof(enroll_date), is_elective);
            if (strcmp(enroll_date, "2026-12-31") > 0)
            {
                fail_attempts++;
                consecutive_fails++;
                continue;
            }
            int score = GenerateScore(75.0, 12.0, 0, 100, 0.10);
            if (rand() % 100 < 5)
                score = -1;
            char line[512];
            int line_len = snprintf(line, sizeof(line), "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n", stu_id, name, college, COURSES[course_idx].id, COURSES[course_idx].name, COURSES[course_idx].credit, semester, enroll_date, score, is_elective ? 1 : 0);
            if (line_len > 0 && line_len < (int)sizeof(line))
                AppendLineToBuffer(line, line_len);
            if (success_count < CAPACITY_LIMIT_THRESHOLD && !capacity_auto_released)
                course_year_capacity[CAPACITY_INDEX(course_idx, year_offset)]++;
            success_count++;
            // ✅ 增加进度提示
            if (success_count % 2000 == 0)
            {
                printf("[进度] 已成功生成 %d / %d 条...\n", success_count, count);
            }
            if (success_count >= count)
                break;
            fail_attempts = 0;
            consecutive_fails = 0;
            if (success_count % 5000 == 0)
                FlushIOBuffer();
        }
    }
    // 收尾工作：刷盘、关闭文件、释放内存
    FlushIOBuffer();
    fclose(g_fp);
    g_fp = NULL;
    free(course_year_capacity);
    // 增加最终结果警告
    if (success_count < count)
    {
        printf("[警告] 生成提前结束！目标: %d 条, 实际仅生成: %d 条。\n", count, success_count);
        printf("[提示] 可能原因：课程容量限制或年份越界导致触发安全阀。\n");
    }
    return success_count;
}
/**
 * @brief 验证生成的CSV文件格式是否正确
 */
bool ValidateGeneratedData(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
        return false;
    char line[1024];
    int line_count = 0, error_count = 0;
    if (fgets(line, sizeof(line), fp) == NULL)
    {
        fclose(fp);
        return false;
    } // 跳过表头
    while (fgets(line, sizeof(line), fp))
    {
        line_count++;
        line[strcspn(line, "\r\n")] = '\0'; // 去除换行符
        if (strlen(line) == 0)
            continue;
        // 简单的逗号分割校验
        char *fields[10];
        int field_count = 0;
        char *ptr = line;
        for (int i = 0; i < 10; i++)
        {
            fields[i] = ptr;
            field_count++;
            char *comma = strchr(ptr, ',');
            if (comma != NULL)
            {
                *comma = '\0';
                ptr = comma + 1;
            }
            else
                break;
        }
        // 校验字段数和学号长度
        if (field_count < 10 || strlen(fields[0]) != 12)
            error_count++;
    }
    fclose(fp);
    printf("\n[验证结果] 总记录数: %d, 格式错误数: %d\n", line_count, error_count);
    return error_count == 0;
}
// ================= 独立测试入口 =================
#ifdef STANDALONE
int main()
{
    int count;
    char filename[256] = "generated_data.csv";
    char input_buf[256];

    printf("========================================\n");
    printf("       数据生成器独立测试模式\n");
    printf("========================================\n");

    printf("请输入要生成的数据条数: ");
    if (fgets(input_buf, sizeof(input_buf), stdin) == NULL || sscanf(input_buf, "%d", &count) != 1 || count <= 0)
    {
        printf("[错误] 无效的输入！\n");
        system("pause");
        return 1;
    }

    printf("\n请输入文件名 (直接回车使用默认 %s): ", filename);
    if (fgets(input_buf, sizeof(input_buf), stdin) != NULL)
    {
        input_buf[strcspn(input_buf, "\r\n")] = '\0';
        if (strlen(input_buf) > 0)
        {
            strncpy(filename, input_buf, sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';
        }
    }

    DataGenerator_Init();
    printf("\n正在生成 %d 条数据到 %s ...\n", count, filename);

    int result = GenerateMockDataToFile(filename, count);

    if (result > 0)
    {
        printf("\n[✓ 成功] 精确生成 %d 条记录到 %s\n", result, filename);
    }
    else
    {
        printf("\n[✗ 错误] 数据生成失败！\n");
    }

    printf("\n========================================\n");
    system("pause");
    return 0;
}
#endif