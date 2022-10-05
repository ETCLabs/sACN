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

#define SACN_RECEIVER_MAX_UNIVERSES 0
#define SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE 0
#define SACN_RECEIVER_TOTAL_MAX_SOURCES 0
#define SACN_DMX_MERGER_MAX_MERGERS 0
#define SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER 0
