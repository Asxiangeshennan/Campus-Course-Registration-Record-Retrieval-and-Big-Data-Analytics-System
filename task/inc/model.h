/**
 * @file model.h
 * @brief 数据模型定义头文件
 *
 * 本文件定义了学生选课与成绩管理系统的核心数据结构，
 * 包括核心业务实体（CourseRecord）和查询筛选条件（FilterCriteria），
 * 以及相关字符串字段的长度限制宏定义。
 */
#ifndef MODEL_H
#define MODEL_H

#include <stdbool.h> // 引入布尔类型支持 (bool, true, false)
#include <stddef.h>  // 引入标准定义 (如 size_t, NULL 等)

// ================= 字符串长度限制 =================
// 用于统一管理系统中各字符串字段的最大长度，防止缓冲区溢出
#define MAX_STU_ID_LEN 20    // 学号 (Student ID) 最大字符长度
#define MAX_NAME_LEN 50      // 姓名 (Name) 最大字符长度
#define MAX_COU_ID_LEN 20    // 课程号 (Course ID) 最大字符长度
#define MAX_COU_NAME_LEN 100 // 课程名称 (Course Name) 最大字符长度
#define MAX_COLLEGE_LEN 50   // 学院名称 (College) 最大字符长度
#define MAX_SEMESTER_LEN 20  // 学期 (Semester, 如 "2023-2024-1") 最大字符长度
#define MAX_DATE_LEN 20      // 日期 (Date, 如 "2023-09-01") 最大字符长度

// ================= 核心业务实体 =================
/**
 * @struct CourseRecord
 * @brief 选课与成绩记录实体
 *
 * 用于存储单条学生选课及成绩的完整信息，通常对应数据库中的一条记录或内存中的一条数据。
 */
typedef struct
{
    char student_id[MAX_STU_ID_LEN];    // 学号：唯一标识学生
    char name[MAX_NAME_LEN];            // 姓名：学生姓名
    char college[MAX_COLLEGE_LEN];      // 学院：学生所属学院
    char course_id[MAX_COU_ID_LEN];     // 课程号：唯一标识课程
    char course_name[MAX_COU_NAME_LEN]; // 课程名称：课程的中文或英文名称
    float credit;                       // 学分：该课程的学分数值
    char semester[MAX_SEMESTER_LEN];    // 学期：开课或选课学期（如 "2025-2026-2"）
    char enroll_date[MAX_DATE_LEN];     // 选课日期：学生选择该课程的日期（如 "2026-02-20"）
    int score;                          // 成绩：学生的最终考核分数（若未出成绩可设为 -1 等特定值）
    int is_elective;                    // 是否选修课：标识课程性质（通常 1: 选修课, 0: 必修课）
} CourseRecord;

// ================= 筛选条件模型 =================
/**
 * @struct FilterCriteria
 * @brief 数据查询与筛选条件模型
 *
 * 用于封装在查询、搜索或过滤 CourseRecord 集合时所使用的各种条件。
 * 注意：未使用的筛选条件可通过空字符串（对于字符串字段）或特定默认值来忽略。
 */
typedef struct
{
    char student_id[MAX_STU_ID_LEN];    // 筛选条件：按学号精确匹配
    char course_id[MAX_COU_ID_LEN];     // 筛选条件：按课程号精确匹配
    char course_name[MAX_COU_NAME_LEN]; // 筛选条件：按课程名称匹配（结合 is_fuzzy_course 决定精确或模糊）
    char semester[MAX_SEMESTER_LEN];    // 筛选条件：按学期精确匹配
    char college[MAX_COLLEGE_LEN];      // 筛选条件：按学院精确匹配
    int score_min;                      // 筛选条件：成绩下限（包含），用于范围查询（如 >= 60）
    int score_max;                      // 筛选条件：成绩上限（包含），用于范围查询（如 <= 100）
    bool is_fuzzy_course;               // 模糊匹配开关：true 表示 course_name 使用模糊匹配（包含），false 表示精确匹配
} FilterCriteria;

#endif // MODEL_H