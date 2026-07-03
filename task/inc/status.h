/**
 * @file status.h
 * @brief 统一定义项目中的状态码与错误码
 */
#ifndef STATUS_H
#define STATUS_H

// 统一的状态码与错误码定义
#define STATUS_SUCCESS 0            // 操作成功
#define STATUS_ERR_NOT_FOUND -1     // 错误：未找到目标（如查询的记录不存在）
#define STATUS_ERR_DUPLICATE -2     // 错误：数据重复（如添加的记录已存在）
#define STATUS_ERR_INVALID_DATE -3  // 错误：无效的日期（如日期格式错误或逻辑不合理）
#define STATUS_ERR_INVALID_SCORE -4 // 错误：无效的成绩（如分数超出合理范围或格式错误）
#define STATUS_ERR_MEMORY -5        // 错误：内存操作失败（如动态内存分配失败

#endif // STATUS_H