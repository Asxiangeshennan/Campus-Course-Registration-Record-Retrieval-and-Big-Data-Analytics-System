#ifndef VALIDATOR_H
#define VALIDATOR_H
/**
 * @file validator.h
 * @brief 数据格式与业务规则校验模块
 * @details 提供学号、课程号、学期、日期等基础格式校验，
 *          以及选课时间窗口等核心业务规则的强校验功能。
 */
#include <stdbool.h>

/**
 * @brief 校验学号格式 (严格12位规则)
 * @details 规则说明：学号必须严格为12个字符长度，通常要求为纯数字。
 * @param id 待校验的学号字符串 (不能为NULL)
 * @param err_msg 校验失败时的错误信息输出缓冲区。若校验失败，将具体错误原因写入此缓冲区。
 * @param msg_size err_msg 缓冲区的最大容量（需包含字符串结束符 '\0' 的空间）
 * @return 校验通过返回 true，失败返回 false
 */
bool CheckStudentIDFormat(const char *id, char *err_msg, int msg_size);

/**
 * @brief 校验课程编号格式 (严格8位规则)
 * @details 规则说明：课程编号必须严格为8个字符长度，通常由纯数字或字母数字组合组成。
 * @param id 待校验的课程编号字符串 (不能为NULL)
 * @param err_msg 校验失败时的错误信息输出缓冲区
 * @param msg_size err_msg 缓冲区的最大容量
 * @return 校验通过返回 true，失败返回 false
 */
bool CheckCourseIDFormat(const char *id, char *err_msg, int msg_size);

/**
 * @brief 校验学期格式 (如 2026-01)
 * @details 规则说明：格式必须严格为 "YYYY-SS"。
 *          其中 YYYY 为4位年份，SS 为2位学期编号（01代表春季学期，02代表秋季学期）。
 * @param sem 待校验的学期字符串 (不能为NULL)
 * @param err_msg 校验失败时的错误信息输出缓冲区
 * @param msg_size err_msg 缓冲区的最大容量
 * @return 校验通过返回 true，失败返回 false
 */
bool CheckSemesterFormat(const char *sem, char *err_msg, int msg_size);

/**
 * @brief 校验日期格式 (如 2026-06-01)
 * @details 规则说明：格式必须严格为 "YYYY-MM-DD"。
 *          除了格式校验外，还会校验日期的实际合法性（如月份在1-12之间，日期符合对应月份的天数及闰年规则）。
 * @param date 待校验的日期字符串 (不能为NULL)
 * @param err_msg 校验失败时的错误信息输出缓冲区
 * @param msg_size err_msg 缓冲区的最大容量
 * @return 校验通过返回 true，失败返回 false
 */
bool CheckDateFormat(const char *date, char *err_msg, int msg_size);

/**
 * @brief 选课时间窗口强校验 (核心业务规则)
 * @details 业务规则说明：
 *          1. 月份匹配：春季学期(01)的选课月份必须为2月；秋季学期(02)的选课月份必须为9月。
 *          2. 日期匹配：选课日期必须在当月的 15-21号 或 25-27号 这两个时间窗口内。
 * @param semester 学期字符串，格式需符合 "YYYY-SS" (如 "2026-01")
 * @param enroll_date 选课日期字符串，格式需符合 "YYYY-MM-DD" (如 "2026-02-16")
 * @return 符合选课时间窗口规则返回 true，不符合返回 false
 * @note 此函数通常假设传入的 semester 和 enroll_date 已经通过了基础格式校验
 *       (即已调用过 CheckSemesterFormat 和 CheckDateFormat)。
 */
bool IsValidEnrollDate(const char *semester, const char *enroll_date);
#endif // VALIDATOR_H