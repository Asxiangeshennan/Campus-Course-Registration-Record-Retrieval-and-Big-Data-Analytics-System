#include "common.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
/**
 * @brief 清空标准输入缓冲区
 *
 * @details 通常在使用 scanf 等格式化输入函数后，缓冲区中可能会残留换行符 '\n'。
 *          调用此函数可以消耗掉这些残留字符，防止影响后续的输入操作（如 fgets, getchar 等）。
 */
void ClearInputBuffer(void)
{
    int c;
    // 循环读取字符，直到遇到换行符 '\n' 或文件结束符 EOF 为止
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}
/**
 * @brief 安全的字符串拷贝函数
 *
 * @param dest 目标字符串缓冲区
 * @param size 目标缓冲区的总大小（必须包含存放 '\0' 的空间）
 * @param src  源字符串
 *
 * @details 类似于 strncpy，但强制保证目标字符串以 '\0' 结尾，防止缓冲区溢出。
 */
void SafeStrCopy(char *dest, int size, const char *src)
{
    // 1. 参数合法性检查：防止空指针和无效的缓冲区大小
    if (!dest || !src || size <= 0)
        return;
    // 2. 拷贝字符串，最多拷贝 size - 1 个字符，为 '\0' 留出空间
    strncpy(dest, src, size - 1);
    // 3. 强制在末尾添加字符串结束符 '\0'
    // (注意：strncpy 在 src 长度大于等于 size 时，不会自动添加 '\0'，这是它的一个常见坑)
    dest[size - 1] = '\0';
}
/**
 * @brief 去除字符串首尾的空白字符（原地修改）
 *
 * @param str 待处理的字符串
 *
 * @details 会去除字符串开头和结尾的空格、制表符、换行符和回车符。
 */
void TrimString(char *str)
{
    if (!str)
        return;
    // 1. 跳过开头的空白字符，找到第一个有效字符的位置
    char *start = str;
    while (*start == ' ' || *start == '\t')
        start++;
    // 2. 从末尾开始，跳过尾部的空白字符
    char *end = str + strlen(str) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
    {
        *end = '\0'; // 将尾部空白字符替换为字符串结束符
        end--;
    }
    // 3. 如果开头有被跳过的空白字符，将剩余的有效字符串整体前移
    if (start != str)
        // 使用 memmove 而不是 strcpy，因为源内存和目标内存区域有重叠
        // strlen(start) + 1 是为了把末尾的 '\0' 也一起移动过去
        memmove(str, start, strlen(start) + 1);
}