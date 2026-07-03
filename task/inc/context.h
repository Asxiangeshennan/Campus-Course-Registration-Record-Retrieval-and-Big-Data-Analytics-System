#ifndef CONTEXT_H
#define CONTEXT_H
/**
 * @file Context.h
 * @brief 上下文管理模块头文件
 *
 * 提供当前日期等运行时上下文信息的获取与设置功能。
 * 使用前需先调用 Context_Init() 进行初始化。
 */

/**
 * @brief 初始化上下文模块
 *
 * 在使用其他 Context 函数之前，必须先调用此函数完成初始化。
 * 通常会将当前日期设置为系统默认值。
 */
void Context_Init();
/**
 * @brief 获取当前日期字符串
 *
 * @return 指向当前日期字符串的常量指针，
 *         格式如 "YYYY-MM-DD"。调用者不应修改或释放该指针。
 */
const char *Context_GetCurrentDate();
/**
 * @brief 设置当前日期
 *
 * @param date 要设置的日期字符串（如 "2026-06-30"）。
 *             函数内部会自行保存副本，调用者可安全释放传入的字符串。
 */
void Context_SetCurrentDate(const char *date);
#endif