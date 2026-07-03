#ifndef EXTERNAL_SORT_H
#define EXTERNAL_SORT_H

/**
 * @brief 执行外部排序 (附加任务2)
 * @param input_file 输入的CSV文件路径 (需包含10万条数据)
 * @param output_file 排序后输出的CSV文件路径
 * @return 成功返回0，失败返回-1
 */
int ExternalSortByStudentID(const char *input_file, const char *output_file);

#endif