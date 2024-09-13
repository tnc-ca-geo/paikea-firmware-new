/*
 * Define the state type here for better testibility
 */

typedef struct {
    // adjust type
    time_t real_time = 0;
    uint32_t uptime = 0;
    uint32_t prior_uptime = 0;

} systemState;