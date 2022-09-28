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

#define SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER 0
