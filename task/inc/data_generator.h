#ifndef DATA_GENERATOR_H
#define DATA_GENERATOR_H

#include "common.h"
#include <stdbool.h>

/* ============================================================
 * 全局引擎变量
 * ============================================================ */

/**
 * @brief 当前激活的存储引擎（定义在 main.c 或其他源文件中）
 * @details 用于在数据生成模块中决定数据写入的目标引擎（如 BPlus Tree / Hash Table）
 */
extern StorageEngine g_active_engine;

/* ============================================================
 * 初始化函数
 * ============================================================ */

/**
 * @brief 初始化随机数生成器
 * @details 使用当前时间作为种子，调用 srand() 初始化伪随机数序列，
 *          确保每次运行程序时生成的模拟数据不同。
 *          应在调用任何 Generate* 函数之前调用一次。
 */
void DataGenerator_Init(void);

/* ============================================================
 * 学号相关
 * ============================================================ */

/**
 * @brief 根据索引生成符合规则的学号
 * @param[in]  index   学生索引（从 0 开始），用于确定学号中的序号部分
 * @param[out] stu_id  输出缓冲区，存放生成的学号字符串
 * @param[in]  size    缓冲区大小（字节数），防止溢出
 * @note 学号格式示例: "2022CS0001"，其中前4位为入学年份，
 *       中间2位为学院代码，后4位为序号
 */
void GenerateStudentID(int index, char *stu_id, int size);

/**
 * @brief 根据索引生成学生姓名
 * @param[in]  index  学生索引，用于从预定义的姓氏/名字库中随机选取
 * @param[out] name   输出缓冲区，存放生成的中文姓名（UTF-8 编码）
 * @param[in]  size   缓冲区大小（字节数）
 * @note 姓名由随机选取的姓氏 + 名字组合而成
 */
void GenerateStudentName(int index, char *name, int size);

/**
 * @brief 根据学号解析出对应的学院名称
 * @param[in]  student_id  学号字符串（如 "2022CS0001"）
 * @param[out] college     输出缓冲区，存放学院名称
 * @param[in]  size        缓冲区大小（字节数）
 * @note 从学号的第5~6位提取学院代码，映射为学院全称
 *       例如: "CS" → "计算机学院", "EE" → "电子工程学院"
 */
void GetCollegeFromStudentID(const char *student_id, char *college, int size);

/* ============================================================
 * 课程相关
 * ============================================================ */

/**
 * @brief 根据索引生成课程编号
 * @param[in]  index      课程索引（从 0 开始）
 * @param[out] course_id  输出缓冲区，存放生成的课程编号
 * @param[in]  size       缓冲区大小（字节数）
 * @note 课程编号格式示例: "CS101"，前缀为学院代码，后接课程序号
 */
void GenerateCourseID(int index, char *course_id, int size);

/**
 * @brief 根据课程编号查询对应的课程名称
 * @param[in]  course_id    课程编号字符串
 * @param[out] course_name  输出缓冲区，存放课程名称
 * @param[in]  size         缓冲区大小（字节数）
 * @note 通过内置的课程映射表查找，若未找到则返回 "未知课程"
 */
void GetCourseNameFromID(const char *course_id, char *course_name, int size);

/**
 * @brief 根据课程编号获取该课程的学分
 * @param[in] course_id  课程编号字符串
 * @return 该课程对应的学分值（如 2.0, 3.0, 4.0 等）
 * @note 若课程编号不存在，默认返回 0.0
 */
float GetCreditFromCourseID(const char *course_id);

/* ============================================================
 * 成绩与学期相关
 * ============================================================ */

/**
 * @brief 生成符合正态分布的模拟成绩
 * @param[in] mean       正态分布的均值（期望分数，如 75.0）
 * @param[in] stddev     正态分布的标准差（控制分数离散程度，如 10.0）
 * @param[in] min_score  成绩下限（如 0），低于此值会被截断
 * @param[in] max_score  成绩上限（如 100），高于此值会被截断
 * @param[in] fail_rate  不及格率（0.0~1.0），用于额外控制低分比例
 * @return 生成的整数成绩（0~100 之间）
 * @note 使用 Box-Muller 变换生成正态分布随机数，
 *       再根据 fail_rate 以一定概率强制生成低于60分的成绩
 */
int GenerateScore(double mean, double stddev, int min_score, int max_score, double fail_rate);

/**
 * @brief 根据入学年份生成随机选课学期
 * @param[in]  enroll_year  学生入学年份（如 2022）
 * @param[out] semester     输出缓冲区，存放学期字符串
 * @param[in]  size         缓冲区大小（字节数）
 * @note 学期格式示例: "2023-2024-1"（表示2023~2024学年第1学期）
 *       学期范围从入学后的第二学期开始，到当前时间为止
 */
void GenerateSemester(int enroll_year, char *semester, int size);

/**
 * @brief 根据学期生成选课/录入日期
 * @param[in]  semester    学期字符串（如 "2023-2024-1"）
 * @param[out] date        输出缓冲区，存放日期字符串
 * @param[in]  size        缓冲区大小（字节数）
 * @param[in]  is_elective 是否为选修课
 *                          - true:  生成选课期间的日期（开学前1~2周）
 *                          - false: 生成学期开始后的日期（开学后1~4周）
 * @note 日期格式: "YYYY-MM-DD"
 */
void GenerateEnrollDate(const char *semester, char *date, int size, bool is_elective);

/* ============================================================
 * 记录生成
 * ============================================================ */

/**
 * @brief 生成一条完整的模拟选课记录
 * @param[in]  student_index  学生索引，用于生成学号、姓名等
 * @param[in]  course_index   课程索引，用于生成课程编号、名称、学分等
 * @param[out] rec            指向 CourseRecord 结构体的指针，存放生成的完整记录
 * @note 该函数内部会依次调用 GenerateStudentID、GenerateCourseID、
 *       GenerateScore、GenerateSemester、GenerateEnrollDate 等函数，
 *       组装成一条完整的选课记录（含学号、姓名、课程、成绩、学期、日期等）
 */
void GenerateMockRecord(int student_index, int course_index, CourseRecord *rec);

/* ============================================================
 * 批量数据生成
 * ============================================================ */

/**
 * @brief 批量生成模拟数据并保存到 CSV 文件
 * @param[in] filename  输出 CSV 文件的路径
 * @param[in] count     要生成的记录条数
 * @return 成功生成的记录数；若文件打开失败则返回 -1
 * @note CSV 文件格式: 学号,姓名,学院,课程编号,课程名称,学分,成绩,学期,选课日期
 *       第一行为表头，后续每行为一条记录
 */
int GenerateMockDataToFile(const char *filename, int count);

/**
 * @brief 批量生成模拟数据并直接插入到当前激活的存储引擎中
 * @param[in] count  要生成的记录条数
 * @return 成功插入的记录数；若引擎未初始化则返回 -1
 * @note 该函数通过 g_active_engine 判断当前使用的存储引擎，
 *       并调用对应引擎的 Insert 接口逐条插入数据
 */
int GenerateMockDataToMemory(int count);

/* ============================================================
 * 数据验证
 * ============================================================ */

/**
 * @brief 验证生成的数据文件是否符合业务规则
 * @param[in] filename  待验证的 CSV 文件路径
 * @return 验证通过返回 true，否则返回 false
 * @note 验证规则包括:
 *       - 学号格式是否正确（长度、学院代码是否合法）
 *       - 成绩是否在 0~100 范围内
 *       - 学分是否与课程编号匹配
 *       - 学期是否在合理范围内
 *       - 日期格式是否正确
 */
bool ValidateGeneratedData(const char *filename);

#endif // DATA_GENERATOR_H