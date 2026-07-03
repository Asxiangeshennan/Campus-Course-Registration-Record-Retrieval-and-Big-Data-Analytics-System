#ifndef STATISTICS_H
#define STATISTICS_H

#include "model.h" // 依赖基础模型（包含各种长度宏定义及基础数据结构）
/*
 * 课程选课统计结构体
 * 用于统计和展示单门课程的选课情况
 */
typedef struct
{
    char course_id[MAX_COU_ID_LEN];     // 课程ID
    char course_name[MAX_COU_NAME_LEN]; // 课程名称
    int enroll_count;                   // 选课人数
    float usage_rate;                   // 选课率 / 资源使用率（如选课人数/容量）
} CourseEnrollmentStat;
/*
 * 学生学分统计结构体
 * 用于统计单个学生的选课数量及学分获取情况
 */
typedef struct
{
    char student_id[MAX_STU_ID_LEN]; // 学号
    char name[MAX_NAME_LEN];         // 学生姓名
    int course_count;                // 已选课程门数
    float total_credit;              // 获得的总学分
} StudentCreditStat;
/*
 * 学院分布统计结构体
 * 用于统计各学院学生的选课人数及占比分布
 */
typedef struct
{
    char college[MAX_COLLEGE_LEN]; // 学院名称
    int enroll_count;              // 该学院的选课总人数
    float percentage;              // 该学院选课人数占总人数的百分比
} CollegeDistributionStat;
/*
 * 学期趋势统计结构体
 * 用于按学期维度统计选课规模和课程开设情况
 */
typedef struct
{
    char semester[MAX_SEMESTER_LEN]; // 学期标识（如 "2025-2026-1"）
    int total_students;              // 该学期选课总人次（或参与学生总数）
    int unique_courses;              // 该学期开设的不重复课程门数
} SemesterTrendStat;
/*
 * 成绩分布统计结构体
 * 用于统计各个分数段的学生人数，以便进行成绩分析
 */
typedef struct
{
    int excellent; // 优秀人数（通常指 90分及以上）
    int good;      // 良好人数（通常指 80-89分）
    int medium;    // 中等人数（通常指 70-79分）
    int pass;      // 及格人数（通常指 60-69分）
    int fail;      // 不及格人数（通常指 60分以下）
    int pending;   // 待处理/未出分人数（如成绩正在录入或审核中）
    int total;     // 参与统计的总人数
} ScoreDistributionStat;

#endif // STATISTICS_H