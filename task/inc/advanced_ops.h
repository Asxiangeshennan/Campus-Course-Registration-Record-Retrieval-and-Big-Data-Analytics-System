#ifndef ADVANCED_OPS_H
#define ADVANCED_OPS_H

#include "model.h"
/**
 * @file advanced_ops.h
 * @brief 高级操作模块接口声明
 *
 * 包含多关键字排序、过期记录管理、数据导出及外部排序等高级功能。
 */
// 使用强类型枚举替代宏定义
/**
 * @brief 排序字段枚举
 * 定义支持排序的数据字段类型
 */
typedef enum
{
    SORT_FIELD_STU_ID = 1, /**< 按学生学号排序 */
    SORT_FIELD_SCORE = 2,  /**< 按成绩排序 */
    SORT_FIELD_CREDIT = 3, /**< 按学分排序 */
    SORT_FIELD_DATE = 4    /**< 按日期排序 */
} SortField;
/**
 * @brief 排序方向枚举
 * 定义排序的升降序规则
 */
typedef enum
{
    SORT_ASC = 1, // 升序
    SORT_DESC = 2 // 降序
} SortDirection;
/**
 * @brief 排序规则结构体
 * 用于组合排序字段和排序方向，支持多关键字排序配置
 */
typedef struct
{
    SortField field;         /**< 排序依据的字段 */
    SortDirection direction; /**< 排序方向 (升序/降序) */
} SortRule;

// ================= 高级操作接口 =================
/**
 * @brief 多关键字排序
 *
 * 根据传入的多个排序规则，对课程记录数组进行排序。
 * 规则优先级按照 rules 数组中的顺序依次降低。
 *
 * @param records    待排序的课程记录数组指针
 * @param count      数组中的记录总数
 * @param rules      排序规则数组指针 (包含字段和方向)
 * @param rule_count 排序规则的数量
 */
void Advanced_MultiKeySort(CourseRecord *records, int count, SortRule *rules, int rule_count);
/**
 * @brief 统计过期记录数量
 *
 * 扫描数据源，统计指定截止日期之前的过期记录总数（仅统计，不修改原数据）。
 *
 * @param cutoff_date 截止日期字符串 (建议格式: "YYYY-MM-DD")
 * @return int 返回过期记录的总数量
 */
int Advanced_CountExpiredRecords(const char *cutoff_date);
/**
 * @brief 执行清理过期记录
 *
 * 从数据源中物理删除或移除指定截止日期之前的过期记录。
 *
 * @param cutoff_date 截止日期字符串 (建议格式: "YYYY-MM-DD")
 * @return int 返回成功清理的记录数量，若失败可能返回负数或特定错误码
 */
int Advanced_ExecuteCleanExpired(const char *cutoff_date);
/**
 * @brief 导出过滤后的记录
 *
 * 将内存中指定的记录数组导出/保存到本地文件中。
 *
 * @param records  待导出的课程记录数组指针
 * @param count    数组中的记录总数
 * @param filename 导出目标文件的路径及文件名
 * @return int 返回成功导出的记录数量，或表示操作成功/失败的标志
 */
int Advanced_ExportFiltered(CourseRecord *records, int count, const char *filename);
/**
 * @brief 外部排序 (针对超大文件)
 *
 * 当文件过大无法一次性加载到内存时，使用“分块读取 -> 内存排序 -> 归并写回”的方式对文件进行排序。
 *
 * @param input_file   待排序的输入文件路径
 * @param output_file  排序完成后的输出文件路径
 * @param memory_limit 排序时允许使用的最大内存限制 (注: 需在 .c 实现中明确单位，如 MB 或 KB)
 */
void Advanced_ExternalSort(const char *input_file, const char *output_file, int memory_limit);

#endif // ADVANCED_OPS_H