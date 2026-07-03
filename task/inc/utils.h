#ifndef UTILS_H
#define UTILS_H

/**
 * @file utils.h
 * @brief 字符串与输入流安全处理工具集
 *
 * 本头文件声明了用于安全处理标准输入流缓冲区、防止缓冲区溢出的字符串复制，
 * 以及去除字符串首尾空白字符的底层工具函数。
 */

/**
 * @brief 清空标准输入流缓冲区
 *
 * 用于在读取输入（如 scanf, fgets 等）后，清除缓冲区中残留的字符（通常是换行符），
 * 防止残留字符影响后续的输入操作。
 */
void ClearInputBuffer(void);
/**
 * @brief 安全的字符串复制函数
 *
 * 将源字符串复制到目标缓冲区，并严格限制复制长度，确保不会发生缓冲区溢出。
 * 保证目标字符串始终以 '\0' 结尾。
 *
 * @param dest 目标字符串缓冲区
 * @param size 目标缓冲区的总容量大小（必须包含结尾 '\0' 的空间）
 * @param src  需要复制的源字符串
 *
 * @note 类似于 strncpy 或 strlcpy，但修复了 strncpy 不自动补 '\0' 的缺陷。
 */
void SafeStrCopy(char *dest, int size, const char *src);
/**
 * @brief 去除字符串首尾的空白字符
 *
 * 原地（in-place）修改传入的字符串，移除开头和结尾的空格、制表符、换行符等空白字符。
 *
 * @param str 需要处理的字符串（注意：该字符串的内容会被直接修改，不能是字符串字面量）
 */
void TrimString(char *str);

#endif // UTILS_H