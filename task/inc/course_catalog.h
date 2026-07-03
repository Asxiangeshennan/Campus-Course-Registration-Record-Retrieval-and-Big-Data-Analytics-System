/**
 * @file course_catalog.h
 * @brief 课程目录与自动排课系统头文件
 * @details 定义了课程信息结构体以及排课系统相关的核心接口函数
 */
#ifndef COURSE_CATALOG_H
#define COURSE_CATALOG_H

#include <stdbool.h> // 引入布尔类型支持 (bool, true, false)
/**
 * @brief 课程信息结构体
 */
typedef struct
{
    char id[20];    // 课程编号 (最多19个字符 + '\0')
    char name[100]; // 课程名称 (最多99个字符 + '\0')
    float credit;   // 课程学分
} CourseInfo;

/**
 * @brief 根据当前日期计算“即将到来的目标学期” (用于排课)
 *
 * @param current_date 当前日期字符串 (例如 "YYYY-MM-DD")
 * @param semester     用于存储计算出的目标学期字符串的缓冲区
 * @param size         缓冲区 semester 的最大容量，防止溢出
 */
void GetTargetSemester(const char *current_date, char *semester, int size);
/**
 * @brief 根据学号获取学生所在的学院名称
 *
 * @param student_id 学生学号
 * @param college    用于存储学院名称的缓冲区
 * @param size       缓冲区 college 的最大容量
 */
void GetCollegeByStudentID(const char *student_id, char *college, int size);
/**
 * @brief 根据课程编号获取课程的详细信息
 *
 * @param course_id 课程编号
 * @param info      指向 CourseInfo 结构体的指针，用于接收查询到的课程信息
 * @return true     成功找到并获取到课程信息
 * @return false    未找到对应的课程信息
 */
bool GetCourseInfo(const char *course_id, CourseInfo *info);
/**
 * @brief 获取共享课程（如公共课/通识课）的总数量
 *
 * @return int 共享课程的总数
 */
int GetSharedCourseCount();
/**
 * @brief 根据索引获取共享课程的编号
 *
 * @param index 共享课程的索引 (从 0 开始，必须小于 GetSharedCourseCount() 的返回值)
 * @return const char* 对应索引的课程编号字符串
 */
const char *GetSharedCourseID(int index);

/**
 * @brief 为单个学生进行自动排课
 * @note 参数 target_semester 强调这是排课的目标学期，而非当前学期
 *
 * @param student_id      学生学号
 * @param name            学生姓名
 * @param college         学生所在学院
 * @param target_semester 目标排课学期
 * @param current_date    当前日期 (用于辅助判断排课规则等)
 * @return int            排课结果 (例如成功排课的课程数量，或特定的状态码)
 */
int AutoScheduleForStudent(const char *student_id, const char *name, const char *college,
                           const char *target_semester, const char *current_date);
/**
 * @brief 从 CSV 文件中读取学生数据并批量执行自动排课
 *
 * @param csv_file_path CSV 文件的路径
 */
void AutoScheduleFromCSV(const char *csv_file_path);
/**
 * @brief 根据指定的日期计算当前所处的学期
 *
 * @param date     指定的日期字符串
 * @param semester 用于存储学期字符串的缓冲区
 *                 (返回格式: "YYYY-01" 表示第一学期/上学期, "YYYY-02" 表示第二学期/下学期)
 * @param size     缓冲区 semester 的最大容量
 */
void GetCurrentSemesterFromDate(const char *date, char *semester, int size);
#endif