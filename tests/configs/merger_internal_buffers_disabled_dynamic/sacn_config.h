#ifdef __cplusplus
extern "C" {
#endif

bool SacnTestingAssertHandler(const char* expression, const char* file, const char* func, unsigned int line);

#ifdef __cplusplus
}
#endif

#define SACN_ASSERT_VERIFY(expr) ((expr) ? true : SacnTestingAssertHandler(#expr, __FILE__, __func__, __LINE__))
#define SACN_LOGGING_ENABLED 0
#define SACN_DYNAMIC_MEM 1

#define SACN_DMX_MERGER_DISABLE_INTERNAL_PAP_BUFFER 1
#define SACN_DMX_MERGER_DISABLE_INTERNAL_OWNER_BUFFER 1
