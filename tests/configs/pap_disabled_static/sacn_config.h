#ifdef __cplusplus
extern "C" {
#endif

bool SacnTestingAssertHandler(const char* expression, const char* file, const char* func, unsigned int line);

#ifdef __cplusplus
}
#endif

#define SACN_ASSERT_VERIFY(expr) ((expr) ? true : SacnTestingAssertHandler(#expr, __FILE__, __func__, __LINE__))
#define SACN_LOGGING_ENABLED 0

#define SACN_DYNAMIC_MEM 0

#define SACN_RECEIVER_MAX_UNIVERSES 30
#define SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE 8
#define SACN_RECEIVER_MAX_SUBS_PER_SOCKET 5
#define SACN_MAX_NETINTS 6

#define SACN_SOURCE_MAX_SOURCES 10
#define SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE 2048

#define SACN_ETC_PRIORITY_EXTENSION 0
