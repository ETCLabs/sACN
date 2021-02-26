#ifdef __cplusplus
extern "C" {
#endif

void SacnTestingAssertHandler(const char* expression, const char* file, unsigned int line);

#ifdef __cplusplus
}
#endif

#define SACN_ASSERT(expr) (void)((!!(expr)) || (SacnTestingAssertHandler(#expr, __FILE__, __LINE__), 0))
#define SACN_LOGGING_ENABLED 0
#define SACN_DYNAMIC_MEM 1

#define SACN_SOURCE_MAX_SOURCES 0
#define SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE 0
