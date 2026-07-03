/**
 * @file common.h
 * @brief 公共头文件，用于集中引入核心子模块并定义全局通用类型
 */
#ifndef COMMON_H
#define COMMON_H

// ==========================================
// 1. 引入拆分后的子模块
// ==========================================
#include "status.h"     // 状态码与错误处理模块
#include "model.h"      // 数据模型与结构体定义模块
#include "statistics.h" // 统计信息收集与管理模块
#include "utils.h"      // 通用工具函数模块

// ==========================================
// 2. 全局枚举定义
// ==========================================

/**
 * @brief 存储引擎类型枚举
 * @note 保留原有的 StorageEngine 枚举，建议后续将其移入 data_manager.h 中以降低模块耦合度
 */
typedef enum
{
    ENGINE_LINKED_LIST = 1, // 链表存储引擎 (适合顺序访问和频繁插入/删除)
    ENGINE_AVL_TREE = 2,    // AVL平衡二叉树存储引擎 (适合快速查找和范围查询)
    ENGINE_HASH_TABLE = 3,  // 哈希表存储引擎 (适合极速的键值对查找)
    ENGINE_ALL_SYNC = 4     // 全量同步模式 (同时使用上述多种引擎并保持数据同步)
} StorageEngine;

#endif // COMMON_H