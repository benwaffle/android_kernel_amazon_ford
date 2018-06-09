#ifndef _MT_SPM_SLEEP_
#define _MT_SPM_SLEEP_

#include <linux/kernel.h>
#include <linux/device.h>

typedef enum {
    WR_NONE         = 0,
    WR_UART_BUSY    = 1,
    WR_PCM_ASSERT   = 2,
    WR_PCM_TIMER    = 3,
    WR_PCM_WDT      = 4,
    WR_WAKE_SRC     = 5,
    WR_PCM_ABORT    = 6,
    WR_UNKNOWN      = 7,
} wake_reason_t;

/* Added by haitaoy@amazon.com for AUSTINPLAT-1413. */
struct mt_wake_event {
	char *domain;
	struct mt_wake_event *parent;
	int code;
};

struct mt_wake_event_map {
	char *domain;
	int code;
	int irq;
	wakeup_event_t we;
};

void spm_report_wakeup_event(struct mt_wake_event *we, int code);
struct mt_wake_event *spm_get_wakeup_event(void);
void spm_set_wakeup_event_map(struct mt_wake_event_map *tbl);
wakeup_event_t spm_read_wakeup_event_and_irq(int *pirq);

/*
 * for suspend
 */
extern int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace);
extern wake_reason_t spm_go_to_sleep(bool cpu_pdn, bool infra_pdn, int pwake_time);
extern wake_reason_t spm_go_to_sleep_dpidle(bool cpu_pdn, u16 pwrlevel, int pwake_time);


/*
 * for deep idle
 */
extern void spm_dpidle_before_wfi(void);        /* can be redefined */
extern void spm_dpidle_after_wfi(void);         /* can be redefined */
extern wake_reason_t spm_go_to_dpidle(bool cpu_pdn, u16 pwrlevel);
extern void aee_rr_rec_deepidle_val(u32 val);

extern bool spm_is_md_sleep(void);
extern bool spm_is_conn_sleep(void);

extern void spm_output_sleep_option(void);

#endif
