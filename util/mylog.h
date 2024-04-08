#pragma once

#define BLOCK_LOG(fmt, arg...) com_writelog("BLOCK", "" fmt "", ##arg)
#define TXS_LOG(fmt, arg...) com_writelog("TXS", "" fmt "", ##arg)

#define FILE_DLOG(fmt, arg...) ul_writelog(UL_LOG_DEBUG, "[%s:%d][%s]" fmt "", __FILE__, __LINE__, __func__, ##arg)
#define FILE_ILOG(fmt, arg...) ul_writelog(UL_LOG_DEBUG, "[%s:%d][%s]" fmt "", __FILE__, __LINE__, __func__, ##arg)
#define FILE_TLOG(fmt, arg...) ul_writelog(UL_LOG_TRACE, "[%s:%d][%s]" fmt "", __FILE__, __LINE__, __func__, ##arg)
#define FILE_ELOG(fmt, arg...) ul_writelog(UL_LOG_FATAL, "[%s:%d][%s]" fmt "", __FILE__, __LINE__, __func__, ##arg)
#define FILE_FLOG(fmt, arg...) ul_writelog(UL_LOG_FATAL, "[%s:%d][%s]" fmt "", __FILE__, __LINE__, __func__, ##arg)
#define FILE_WLOG(fmt, arg...) ul_writelog(UL_LOG_WARNING, "[%s:%d][%s]" fmt "", __FILE__, __LINE__, __func__, ##arg)
#define FILE_NLOG(fmt, arg...) ul_writelog(UL_LOG_NOTICE, "[%s:%d][%s]" fmt "", __FILE__, __LINE__, __func__, ##arg)
