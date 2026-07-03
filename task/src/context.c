/**
 * @file context.c
 * @brief 上下文(Context)模块实现，主要用于管理全局状态（如当前日期）。
 */
#include "context.h"
#include "common.h"
#include <string.h>
/**
 * @brief 全局当前日期变量
 * @details 存储格式通常为 "YYYY-MM-DD"。
 *          使用 static 修饰，限制其作用域仅在当前文件内，保证数据的封装性，防止外部直接修改。
 *          分配 20 字节的空间足以安全容纳标准日期字符串及字符串结束符 '\0'。
 */
static char g_current_date[20] = "2026-06-01";
/**
 * @brief 初始化上下文模块
 * @details 将全局当前日期重置为默认的初始值 "2026-06-01"。
 *          通常在程序启动或模块加载时调用。
 */
void Context_Init()
{ // 使用安全的字符串拷贝函数，防止潜在的缓冲区溢出风险
    SafeStrCopy(g_current_date, sizeof(g_current_date), "2026-06-01");
}
/**
 * @brief 获取当前日期
 * @return const char* 返回当前日期的字符串指针（外部不可修改）
 */
const char *Context_GetCurrentDate() { return g_current_date; }
/**
 * @brief 设置/更新当前日期
 * @param date 新的日期字符串指针，建议传入格式为 "YYYY-MM-DD"
 */
void Context_SetCurrentDate(const char *date)
{
    // 使用安全的字符串拷贝函数，确保不会超出 g_current_date 的缓冲区大小 (20 bytes)
    SafeStrCopy(g_current_date, sizeof(g_current_date), date);
}