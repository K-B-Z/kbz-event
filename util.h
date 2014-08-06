
enum {
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR,
};

#define log(fmt, args...) _log(LOG_DEBUG, __func__, __FILE__, __LINE__, fmt, ##args) 
#define info(fmt, args...) _log(LOG_INFO, __func__, __FILE__, __LINE__, fmt, ##args) 
#define warn(fmt, args...) _log(LOG_WARN, __func__, __FILE__, __LINE__, fmt, ##args) 
#define error(fmt, args...) _log(LOG_ERROR, __func__, __FILE__, __LINE__, fmt, ##args) 

void _log(int level, const char *, const char *, int, char *, ...);
void log_ban(const char *, const char *);
void log_set_level(int level);
void log_init();

float now();

