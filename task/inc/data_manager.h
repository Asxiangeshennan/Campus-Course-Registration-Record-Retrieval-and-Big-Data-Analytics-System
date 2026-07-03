#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H
#include "common.h"
/**
 * @brief 初始化数据管理模块
 * @details 在程序启动或首次使用数据管理功能前调用，分配必要的内存和初始化存储引擎。
 */
void DM_Init(void);
/**
 * @brief 销毁数据管理模块
 * @details 释放所有分配的内存资源，关闭文件句柄等，通常在程序退出前调用。
 */
void DM_Destroy(void);
/**
 * @brief 设置当前活跃的存储引擎
 * @param engine 要切换到的目标存储引擎类型 (如数组、链表、哈希表、B+树等)
 */
void DM_SetActiveEngine(StorageEngine engine);
/**
 * @brief 插入一条课程记录
 * @param rec 指向要插入的课程记录结构体的指针
 * @return 成功返回 0，失败返回相应的错误码
 */
int DM_InsertRecord(const CourseRecord *rec);
/**
 * @brief 删除指定的课程记录
 * @param stu_id 学生学号
 * @param cou_id 课程编号
 * @return 成功返回 0，未找到记录或失败返回错误码
 */
int DM_DeleteRecord(const char *stu_id, const char *cou_id);
/**
 * @brief 更新指定记录的成绩
 * @param stu_id 学生学号
 * @param cou_id 课程编号
 * @param new_score 新的成绩分数
 * @return 成功返回 0，未找到记录或失败返回错误码
 */
int DM_UpdateScore(const char *stu_id, const char *cou_id, int new_score);
/**
 * @brief 查找指定的课程记录
 * @param stu_id 学生学号
 * @param cou_id 课程编号
 * @param rec 用于接收查找结果的记录结构体指针 (调用前需分配内存)
 * @return 成功找到返回 0，未找到或失败返回错误码
 */
int DM_SearchRecord(const char *stu_id, const char *cou_id, CourseRecord *rec);
/**
 * @brief 插入单条课程记录
 * @details 通常用于单条数据写入场景，与批量插入区分。
 * @param rec 指向要插入的课程记录结构体的指针
 * @return 成功返回 0，失败返回错误码
 */
int DM_InsertSingle(const CourseRecord *rec);
/**
 * @brief 删除单条课程记录
 * @param stu_id 学生学号
 * @param cou_id 课程编号
 * @return 成功返回 0，失败返回错误码
 */
int DM_DeleteSingle(const char *stu_id, const char *cou_id);
/**
 * @brief 根据过滤条件筛选课程记录
 * @param criteria 过滤条件结构体指针
 * @param count 输出参数，用于返回筛选出的记录数量
 * @return 返回包含筛选结果的动态记录数组指针，若无结果或失败返回 NULL (调用者需负责释放内存)
 */
CourseRecord *DM_FilterRecords(const FilterCriteria *criteria, int *count);
/**
 * @brief 将当前内存中的数据持久化保存到文件
 * @param filename 目标文件路径
 * @return 成功返回 0，文件操作失败返回错误码
 */
int DM_SaveToFile(const char *filename);
/**
 * @brief 从指定文件加载数据到内存
 * @param filename 源数据文件路径
 * @return 成功返回 0，文件不存在或解析失败返回错误码
 */
int DM_LoadFromFile(const char *filename);

/**
 * @brief 清理过期的课程记录
 * @details 根据业务逻辑(如毕业年份、课程有效期等)清除无效数据。
 * @return 成功清理的记录数，或表示操作状态的状态码
 */
int DM_CleanExpiredRecords(void);
/**
 * @brief 获取当前数据记录总数
 * @return 当前存储引擎中的有效记录条数
 */
int DM_GetDataSize(void);
/**
 * @brief 获取各课程的选课人数统计信息
 * @param count 输出参数，返回统计结果数组的长度
 * @return 返回课程选课统计数组指针，失败返回 NULL
 */
CourseEnrollmentStat *DM_GetCourseEnrollmentStats(int *count);
/**
 * @brief 获取学生的学分统计信息
 * @param count 输出参数，返回统计结果数组的长度
 * @return 返回学生学分统计数组指针，失败返回 NULL
 */
StudentCreditStat *DM_GetStudentCreditStats(int *count);
/**
 * @brief 获取各学院的学生/课程分布统计信息
 * @param count 输出参数，返回统计结果数组的长度
 * @return 返回学院分布统计数组指针，失败返回 NULL
 */
CollegeDistributionStat *DM_GetCollegeDistributionStats(int *count);
/**
 * @brief 获取各学期的选课/成绩趋势统计信息
 * @param count 输出参数，返回统计结果数组的长度
 * @return 返回学期趋势统计数组指针，失败返回 NULL
 */
SemesterTrendStat *DM_GetSemesterTrendStats(int *count);
/**
 * @brief 获取成绩分布统计信息 (如各分数段的人数分布)
 * @param stat 用于接收成绩分布统计结果的结构体指针 (调用前需分配内存)
 */
void DM_GetScoreDistributionStats(ScoreDistributionStat *stat);

/**
 * @brief 获取当前引擎的内存占用估算值
 * @return 内存占用大小，单位为字节 (Bytes)
 */
long long DM_GetMemoryUsage(void);
// 在头文件中声明
/**
 * @brief 纯插入操作
 * @details 仅执行基础的数据插入，不触发额外的索引更新、级联操作或统计信息维护，适合批量导入时提升性能。
 * @param rec 指向要插入的课程记录结构体的指针
 * @return 成功返回 0，失败返回错误码
 */
int DM_InsertPure(const CourseRecord *rec);
/**
 * @brief 自适应插入操作
 * @details 根据当前的数据规模、引擎状态或记录特征，自动选择最优的插入策略 (如批量缓冲、直接插入或重建索引)。
 * @param rec 指向要插入的课程记录结构体的指针
 * @return 成功返回 0，失败返回错误码
 */
int DM_AdaptiveInsert(const CourseRecord *rec);
// 获取最后一次成功加载的CSV文件路径（供退出时默认保存使用）
const char *DM_GetLastLoadedFile(void);
#endif