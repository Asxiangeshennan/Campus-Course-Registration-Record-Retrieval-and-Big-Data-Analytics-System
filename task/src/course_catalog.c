// ================================================================
// course_catalog.c
// 课程目录管理模块 —— 负责课程数据存储、学期计算、智能排课
// ================================================================

#include "course_catalog.h"
#include "data_manager.h"
#include "context.h"
#include "common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ================================================================
// 全局课程数据表
// 与「课程.txt」保持 100% 一致，共 20 门课程
// 课程ID编码规则: XX Y Z W NN
//   XX - 学院/类别代码 (GE=公共, CS=计算机, EE=电信, ME=机械, CE=土木, MA=数学, EM=经管, FL=外语)
//   Y  - 学科大类 (0=公共, 1=工科, 2=理科, 3=文科)
//   Z  - 课程性质 (0=必修, 1=选修)
//   W  - 建议年级 (0=任意, 1=大一, 2=大二, 3=大三, 4=大四)
//   NN - 序号
// ================================================================
static CourseInfo ALL_COURSES[] = {
    // ---- 公共必修课（4门）----
    {"GE001012", "思想政治理论", 2.0f}, // 公共课, 必修, 大一, 春季
    {"GE000011", "体育", 1.0f},         // 公共课, 必修, 任意年级, 春季
    {"GE001011", "心理健康", 1.0f},     // 公共课, 必修, 大一, 春季
    {"MA201017", "高等数学A", 4.0f},    // 理科,   必修, 大一, 春季

    // ---- 公共选修课（3门）----
    {"GE011022", "创新创业基础", 1.5f},   // 公共课, 选修, 任意年级, 秋季
    {"GE011013", "大学生职业规划", 1.0f}, // 公共课, 选修, 任意年级, 春季
    {"GE011024", "艺术鉴赏", 1.5f},       // 公共课, 选修, 任意年级, 秋季

    // ---- 计算机学院（3门：1必修 + 2选修）----
    {"CS102026", "数据结构与算法", 3.5f}, // 工科, 必修, 大二, 秋季
    {"CS114014", "人工智能导论", 2.0f},   // 工科, 选修, 任意年级, 春季
    {"CS113025", "Web开发技术", 2.5f},    // 工科, 选修, 任意年级, 秋季

    // ---- 电信学院（2门：1必修 + 1选修）----
    {"EE102025", "电路分析", 3.0f},   // 工科, 必修, 大二, 秋季
    {"EE113024", "物联网技术", 2.5f}, // 工科, 选修, 任意年级, 春季

    // ---- 机械学院（2门：1必修 + 1选修）----
    {"ME102025", "工程力学", 3.0f},   // 工科, 必修, 大二, 秋季
    {"ME113024", "机器人技术", 2.5f}, // 工科, 选修, 任意年级, 春季

    // ---- 土木学院（1门必修）----
    {"CE102026", "结构力学", 3.5f}, // 工科, 必修, 大二, 秋季

    // ---- 数学学院（2门：1必修 + 1选修）----
    {"MA201025", "线性代数", 3.0f}, // 理科, 必修, 大一, 秋季
    {"MA212026", "数学建模", 2.5f}, // 理科, 选修, 任意年级, 秋季

    // ---- 经管学院（2门：1必修 + 1选修）----
    {"EM301014", "微观经济学", 2.5f}, // 文科, 必修, 大一, 春季
    {"EM312023", "电子商务", 2.0f},   // 文科, 选修, 任意年级, 秋季

    // ---- 外语学院（1门必修）----
    {"FL301015", "综合英语", 3.0f}, // 文科, 必修, 大一, 春季
};
// 宏：计算课程总数（编译期常量，避免运行时除法）
#define ALL_COURSES_COUNT ((int)(sizeof(ALL_COURSES) / sizeof(ALL_COURSES[0])))
// ================================================================
// 函数: GetCurrentSemesterFromDate
// 功能: 根据日期字符串推算当前所属学期
// 参数:
//   date     - 日期字符串，格式 "YYYY-MM-DD"
//   semester - 输出缓冲区，写入学期编号，格式 "YYYY-0S"
//   size     - 缓冲区大小（至少 8 字节）
// 学期划分规则:
//   春季学期 (01): 2月 ~ 8月
//   秋季学期 (02): 9月 ~ 次年1月
// ================================================================
void GetCurrentSemesterFromDate(const char *date, char *semester, int size)
{
    // 防御性编程：检查空指针和缓冲区大小
    if (!date || !semester || size < 8)
        return;
    // 解析年份和月份（直接从字符串固定偏移位置提取）
    int year = atoi(date);      // 前4位: "YYYY"
    int month = atoi(date + 5); // 第6-7位: "MM"

    // 根据月份判断学期类型
    // 2~8月 -> 春季(1), 其余(9~1月) -> 秋季(2)
    int sem_type = (month >= 2 && month <= 8) ? 1 : 2;
    // 特殊情况：1月属于上一学年的秋季学期
    // 例如 "2027-01-15" -> 属于 "2026-02"（2026年秋季）
    int sem_year = (sem_type == 2 && month == 1) ? year - 1 : year;
    // 格式化输出学期编号
    snprintf(semester, size, "%d-%02d", sem_year, sem_type);
}
// ================================================================
// 函数: GetTargetSemester
// 功能: 计算"即将到来的目标学期"（用于排课）
//       排课是为"下学期"提前准备的，所以需要根据当前月份推算
// 参数:
//   current_date - 当前日期字符串 "YYYY-MM-DD"
//   semester     - 输出目标学期编号
//   size         - 缓冲区大小
// 排课窗口期:
//   6~8月   -> 为9月开学的秋季学期排课
//   11~12月 -> 为次年2月开学的春季学期排课
//   1月     -> 为当年2月开学的春季学期排课（补录窗口）
// ================================================================
void GetTargetSemester(const char *current_date, char *semester, int size)
{
    if (!current_date || !semester || size < 8)
        return;

    int year = atoi(current_date);
    int month = atoi(current_date + 5);

    int target_year = year;
    int target_type = 0;

    // ---- 排课窗口期判断 ----
    if (month >= 6 && month <= 8)
    {
        // 6~8月：暑假期间，为即将到来的9月秋季学期排课
        // 例: 2026年7月 -> 目标 "2026-02"（2026秋季）
        target_type = 2;    // 秋季
        target_year = year; // 同一年
    }
    else if (month >= 11)
    {
        // 11~12月：为次年2月春季学期排课
        // 例: 2026年11月 -> 目标 "2027-01"（2027春季）
        target_type = 1;        // 春季
        target_year = year + 1; // 下一年
    }
    else if (month == 1)
    {
        // 1月：寒假补录窗口，为当年2月春季学期排课
        // 例: 2027年1月 -> 目标 "2027-01"（2027春季）
        target_type = 1;
        target_year = year;
    }
    else
    {
        // ---- Fallback: 非排课窗口期的兜底处理 ----
        // 理论上不应在此时调用排课，但为防止程序崩溃，给出合理默认值
        if (month >= 2 && month <= 8)
        {
            target_type = 1; // 当前处于春季，默认指向本年春季
        }
        else
        {
            target_type = 2;
            target_year = year - 1; // 当前处于秋季，默认指向上一学年的秋季
        }
    }

    snprintf(semester, size, "%d-%02d", target_year, target_type);
}

// ================================================================
// 函数: GetCollegeByStudentID
// 功能: 根据学号中的学院代码位提取学院名称
// 学号格式: XXXX CC XXXX
//   CC = 学号第5~6位（索引4,5），表示学院编号
// 参数:
//   student_id - 学号字符串
//   college    - 输出学院名称
//   size       - 缓冲区大小
// ================================================================
void GetCollegeByStudentID(const char *student_id, char *college, int size)
{
    // 学号至少6位才能提取学院代码
    if (!student_id || strlen(student_id) < 6)
    {
        SafeStrCopy(college, size, "未知学院");
        return;
    }
    // 提取第5~6位字符，转换为两位整数作为学院编号
    int col_code = (student_id[4] - '0') * 10 + (student_id[5] - '0');
    const char *col_name = "未知学院";
    // 学院编号 -> 学院名称映射表
    switch (col_code)
    {
    case 1:
        col_name = "计算机学院";
        break;
    case 2:
        col_name = "电信学院";
        break;
    case 3:
        col_name = "机械学院";
        break;
    case 4:
        col_name = "土木学院";
        break;
    case 5:
        col_name = "物理学院";
        break;
    case 6:
        col_name = "数学学院";
        break;
    case 7:
        col_name = "建筑学院";
        break;
    case 8:
        col_name = "外语学院";
        break;
    case 9:
        col_name = "经管学院";
        break;
    case 10:
        col_name = "艺术学院";
        break;
    }
    SafeStrCopy(college, size, col_name);
}
// ================================================================
// 函数: GetCourseInfo
// 功能: 根据课程ID查找课程详细信息（名称、学分等）
// 参数:
//   course_id - 课程编号字符串
//   info      - 输出参数，查找到则填充课程信息
// 返回: true=找到, false=未找到
// ================================================================
bool GetCourseInfo(const char *course_id, CourseInfo *info)
{
    // 线性遍历课程表（20门课程，性能可接受）
    for (int i = 0; i < ALL_COURSES_COUNT; i++)
    {
        if (strcmp(ALL_COURSES[i].id, course_id) == 0)
        {
            *info = ALL_COURSES[i]; // 结构体拷贝
            return true;
        }
    }
    return false;
}
// ================================================================
// 函数: GetSharedCourseCount / GetSharedCourseID
// 功能: 获取课程表总数 / 按索引获取课程ID
//       供外部模块遍历所有课程使用
// ================================================================
int GetSharedCourseCount() { return ALL_COURSES_COUNT; }
const char *GetSharedCourseID(int index)
{
    // 边界检查：防止越界访问
    return (index >= 0 && index < ALL_COURSES_COUNT) ? ALL_COURSES[index].id : NULL;
}

// ================================================================
// 函数: AutoScheduleForStudent
// 功能: 为单个学生执行智能自动排课
//       根据学生学号（入学年份、学院）和目标学期（年级），
//       自动匹配并插入该学期的必修课程记录
// 参数:
//   student_id     - 学号（前4位=入学年份，第5~6位=学院代码）
//   name           - 学生姓名
//   college        - 学院名称
//   target_semester- 目标学期编号，如 "2026-02"
//   current_date   - 当前操作日期
// 返回: 成功新增的必修课程记录数
// ================================================================
int AutoScheduleForStudent(const char *student_id, const char *name, const char *college,
                           const char *target_semester, const char *current_date)
{
    // ---- 步骤1: 从学号提取入学年份 ----
    // 学号前4位为入学年份，如 "2023010001" -> 入学年份 2023
    int enroll_year = atoi(student_id);

    // ---- 步骤2: 解析学院代码，映射到学科大类 ----

    int col_code = (student_id[4] - '0') * 10 + (student_id[5] - '0');
    int major_type = 0; // 学科大类: 0=公共, 1=工科, 2=理科, 3=文/商/艺
    switch (col_code)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 7:
        major_type = 1; // 工科（计算机、电信、机械、土木、建筑）
        break;
    case 5:
    case 6:
        major_type = 2; // 理科（物理、数学）
        break;
    case 8:
    case 9:
    case 10:
        major_type = 3; // 文科/商科/艺术（外语、经管、艺术）
        break;
    }

    // ---- 步骤3: 基于"目标学期"计算当前年级 ----
    // 这是核心逻辑：年级 = 目标学年 - 入学学年 + 偏移量
    int sem_year = atoi(target_semester);     // 目标年份
    int sem_type = atoi(target_semester + 5); // 目标学期类型 (1=春, 2=秋)
    int grade = 0;

    if (sem_type == 2)
    {
        // 秋季学期（9月开学，是学年的起始）
        // 例: 2023级学生，2026秋季 -> 2026 - 2023 + 1 = 4（大四上）
        grade = sem_year - enroll_year + 1;
    }
    else
    {
        // 春季学期（2月开学，是学年的后半段）
        // 例: 2023级学生，2027春季 -> (2027-1) - 2023 + 1 = 4（大四下）
        // 减1是因为春季学期在学年中属于"下一年"，但年级与同年秋季相同
        grade = sem_year - enroll_year;
    }
    // 年级边界保护：限制在 1~4 年级范围内
    if (grade < 1)
        grade = 1;
    if (grade > 4)
        grade = 4;
    // ---- 步骤4: 遍历课程表，匹配该学生应修的必修课 ----
    int added_count = 0; // 记录成功插入的课程数
    for (int i = 0; i < ALL_COURSES_COUNT; i++)
    {
        const char *cid = ALL_COURSES[i].id;
        // 从课程ID中解析课程属性（利用ID编码规则）
        int c_subject = cid[2] - '0'; // 第3位: 学科大类 (0=公共, 1=工, 2=理, 3=文)
        int c_type = cid[3] - '0';    // 第4位: 课程性质 (0=必修, 1=选修)
        int c_grade = cid[4] - '0';   // 第5位: 建议年级 (0=任意, 1~4=具体年级)
        // ---- 必修课匹配条件 ----
        // 1) c_type == 0: 必须是必修课（选修课不自动排）
        // 2) c_subject == 0 || c_subject == major_type:
        //    公共课(0) 所有学生都要修，或者与本专业大类匹配
        // 3) c_grade == grade: 课程建议年级与学生当前年级一致
        bool is_required_match = (c_type == 0) &&
                                 (c_subject == 0 || c_subject == major_type) &&
                                 (c_grade == grade);

        if (is_required_match)
        {
            // 查重：检查该学生是否已有此课程记录（避免重复排课）
            CourseRecord existing;
            bool need_insert = (DM_SearchRecord(student_id, cid, &existing) != STATUS_SUCCESS);

            if (need_insert)
            {
                // 构造新课程记录
                CourseRecord new_rec = {0};
                SafeStrCopy(new_rec.student_id, MAX_STU_ID_LEN, student_id);
                SafeStrCopy(new_rec.name, MAX_NAME_LEN, name);
                SafeStrCopy(new_rec.college, MAX_COLLEGE_LEN, college);
                SafeStrCopy(new_rec.course_id, MAX_COU_ID_LEN, cid);
                SafeStrCopy(new_rec.course_name, MAX_COU_NAME_LEN, ALL_COURSES[i].name);
                new_rec.credit = ALL_COURSES[i].credit;
                // 设置学期为"目标学期"（非当前学期！）
                SafeStrCopy(new_rec.semester, MAX_SEMESTER_LEN, target_semester);
                SafeStrCopy(new_rec.enroll_date, MAX_DATE_LEN, current_date);
                new_rec.score = 0;       // 尚未考试，成绩为0
                new_rec.is_elective = 0; // 必修课标记
                // 写入数据库
                if (DM_InsertRecord(&new_rec) == STATUS_SUCCESS)
                    added_count++;
            }
        }
    }
    return added_count;
}

// ================================================================
// 函数: AutoScheduleFromCSV
// 功能: 从CSV文件批量读取学生信息，为所有学生执行自动排课
//       仅在排课窗口期（开学前3个月）允许执行
// 参数:
//   csv_file_path - CSV学生名单文件路径
// ================================================================
void AutoScheduleFromCSV(const char *csv_file_path)
{
    // ---- 步骤1: 检查当前是否在排课窗口期 ----
    // 窗口期: 6~8月（秋季排课）、11~1月（春季排课）
    int month = atoi(Context_GetCurrentDate() + 5);
    if (!((month >= 6 && month <= 8) || month == 11 || month == 12 || month == 1))
    {
        printf("\n[提示] 当前时间不在自动排课窗口期（开学前3个月）。\n");
        return; // 非窗口期直接退出，防止误操作
    }

    printf("\n[教务系统] 正在从 %s 提取学生信息并执行预排课...\n", csv_file_path);
    // ---- 步骤2: 从CSV加载学生数据 ----
    int count = DM_LoadFromFile(csv_file_path);
    if (count <= 0)
    {
        printf("[提示] CSV 文件为空或不存在。\n");
        return;
    }

    // ---- 步骤3: 计算目标学期 ----
    // 例: 当前 2026-06-30 -> 目标 "2026-02"（2026秋季学期）
    char target_semester[20] = {0};
    GetTargetSemester(Context_GetCurrentDate(), target_semester, sizeof(target_semester));
    printf("[教务系统] 当前排课目标学期: %s\n", target_semester);
    // ---- 步骤4: 获取所有已加载的学生记录 ----
    FilterCriteria all_criteria = {0};
    all_criteria.is_fuzzy_course = true; // 空过滤条件 = 获取全部记录
    int total_records = 0;
    CourseRecord *all_records = DM_FilterRecords(&all_criteria, &total_records);
    if (!all_records)
        return;
    // ---- 步骤5: 去重处理 + 逐人排课 ----
    // 使用数组记录已处理过的学号，避免同一学生重复排课
    // （CSV中可能存在同一学生的多条记录）
    char processed_ids[10000][20]; // 最多处理1万名学生
    int processed_count = 0;
    int total_added = 0; // 累计新增课程数
    for (int i = 0; i < total_records; i++)
    {
        // 检查该学号是否已处理过
        bool is_new = true;
        for (int j = 0; j < processed_count; j++)
            if (strcmp(processed_ids[j], all_records[i].student_id) == 0)
            {
                is_new = false;
                break;
            }
        // 仅对未处理过的学生执行排课
        if (is_new && processed_count < 10000)
        {
            // 记录已处理的学号
            SafeStrCopy(processed_ids[processed_count++], 20, all_records[i].student_id);
            // 为该学生执行自动排课，累加新增课程数
            total_added += AutoScheduleForStudent(all_records[i].student_id, all_records[i].name,
                                                  all_records[i].college, target_semester, Context_GetCurrentDate());
        }
    }
    // 释放动态分配的记录数组内存
    free(all_records);
    printf("[教务系统] 自动排课完成！共生成 %d 条必修记录。\n", total_added);
}