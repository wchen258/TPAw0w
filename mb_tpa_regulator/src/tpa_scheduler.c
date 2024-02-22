#include "tpa_scheduler.h"
#include "coresight_lib.h"
#include "dbg_util.h"
#include "log_util.h"

extern uint8_t trc_buf_lock;
static uint8_t halt_mask = 0;

// vanilla 2lvl
static uint32_t prev_cores_negative_slack [4] = {0, 0, 0, 0};
static uint8_t master_halt;
static uint8_t slave_halt;

static void enter_dbg_seq(uint8_t id) {
	if (halt_mask >> id & 0b1)
		return;

    a53_enter_dbg(id);
    trc_buf_lock |= 0b1 << id;
    halt_mask |= 0b1 << id;
}

static void leave_dbg_seq(uint8_t id) {
	if (!(halt_mask >> id & 0b1))
		return;

    a53_leave_dbg(id, 1);
    trc_buf_lock &= ~(0b1 << id);
    halt_mask &= ~(0b1 << id);
}

void reset_sched() {
	uint32_t i;
	halt_mask = 0;
	master_halt = 0;
	slave_halt = 0;
	for (i = 0; i < 4; ++i)
		prev_cores_negative_slack[i] = 0;
}

/*  sched vanilla
    core0 is mission critical, it can halt/resume all other cores
    core1 is secondary, it can only halt/resume core2,3
*/
void invoke_sched_vanilla(uint8_t id, uint32_t real_time, uint32_t* acc_nominal_time, struct ms_record* log_ms_record) {
    int j = id == 0 ? 1 : 2;
    if (real_time > acc_nominal_time[id] * dbg.alpha[id]) {
        // TPA violation occurs
        for(int i=j; i<4; ++i) {
            if (halt_mask >> i & 0b1) {
                continue;
            }
            enter_dbg_seq(i);
            dbg.milestone_timestamps[2][dbg.enter_dbg_ct++] = dbg.ms_ts_pt[id];
            log_ms_record->decision = 1;
        }
	} else if (real_time < acc_nominal_time[id] * dbg.alpha[id] * (1.0 - dbg.beta[id])) {  // this is consistent with the paper, Daniel's, and the old implementation
        // enough positive slack accumuated
        for(int i=j; i<4; ++i) {
            if (halt_mask >> i & 0b1) {
                leave_dbg_seq(i);
                dbg.milestone_timestamps[3][dbg.leave_dbg_ct++] = dbg.ms_ts_pt[id];
                log_ms_record->decision = 2;
            }
        }
    }
}

void invoke_sched_epilogue_vanilla(uint8_t id, struct ms_record* log_ms_record) {
    // if the mission critical core finished, release all. if core1, release core 2 and 3
    int j = id == 0 ? 1 : 2;
    for(int i=j; i<4; ++i) {
        if (halt_mask >> i & 0b1) {
            leave_dbg_seq(i);
            dbg.milestone_timestamps[3][dbg.leave_dbg_ct++] = dbg.ms_ts_pt[id];
            log_ms_record->decision = 2;
        }
    }
}

/*  sched vanilla 2 level
    core0 is mission critical, it can halt/resume all other cores
    core0 first halts core 2 and 3. if this does not accumulate slack, it halts core 1 as well.
    core1 is secondary, it can only halt/resume core2,3
*/
void invoke_sched_vanilla_2lvl(uint8_t id, uint32_t real_time, uint32_t* acc_nominal_time, struct ms_record* log_ms_record) {
    uint32_t i, negative_slack;
    uint32_t setpoint = acc_nominal_time[id] * dbg.alpha[id];
    uint32_t resumepoint = setpoint * (1.0 - dbg.beta[id]);
    if (id == 0) {
		if (real_time > setpoint) {
			negative_slack = real_time - (acc_nominal_time[id] * dbg.alpha[id]);
			if (prev_cores_negative_slack[id] == 0) {
				enter_dbg_seq(2);
				enter_dbg_seq(3);
				master_halt = 1;
				prev_cores_negative_slack[id] = negative_slack;

				log_ms_record->decision = 1;
				dbg.milestone_timestamps[2][dbg.enter_dbg_ct++] = dbg.ms_ts_pt[id];
			} else if (negative_slack >= prev_cores_negative_slack[id]) {
				enter_dbg_seq(3);
				enter_dbg_seq(2);
				enter_dbg_seq(1);
				master_halt = 2;

				log_ms_record->decision = 1;
				dbg.milestone_timestamps[2][dbg.enter_dbg_ct++] = dbg.ms_ts_pt[id];}
		} else if (real_time <= resumepoint) {
			leave_dbg_seq(1);
			if (slave_halt == 0) {
				leave_dbg_seq(2);
				leave_dbg_seq(3);
			}

			master_halt = 0;
			prev_cores_negative_slack[id] = 0;

			log_ms_record->decision = 2;
			dbg.milestone_timestamps[3][dbg.leave_dbg_ct++] = dbg.ms_ts_pt[id];
		}
    } else if (id == 1) {
    	if (real_time > setpoint) {
    		enter_dbg_seq(2);
    		enter_dbg_seq(3);
    		slave_halt = 1;

    		log_ms_record->decision = 1;
    		dbg.milestone_timestamps[2][dbg.enter_dbg_ct++] = dbg.ms_ts_pt[id];
    	} else if (real_time <= resumepoint && master_halt == 0) {
    		leave_dbg_seq(2);
    		leave_dbg_seq(3);
    		slave_halt = 0;

    		log_ms_record->decision = 2;
    		dbg.milestone_timestamps[3][dbg.leave_dbg_ct++] = dbg.ms_ts_pt[id];
    	}
    }

	/*int j = id == 0 ? 1 : 2;
    if (real_time > acc_nominal_time[id] * dbg.alpha[id]) {
        // TPA violation occurs
        for(int i=j; i<4; ++i) {
            if (halt_mask >> i & 0b1) {
                continue;
            }
            enter_dbg_seq(i);
            dbg.milestone_timestamps[2][dbg.enter_dbg_ct++] = dbg.ms_ts_pt[id];
            log_ms_record->decision = 1;
        }
	} else if (real_time < acc_nominal_time[id] * dbg.alpha[id] * (1.0 - dbg.beta[id])) {  // this is consistent with the paper, Daniel's, and the old implementation
        // enough positive slack accumuated
        for(int i=j; i<4; ++i) {
            if (halt_mask >> i & 0b1) {
                leave_dbg_seq(i);
                dbg.milestone_timestamps[3][dbg.leave_dbg_ct++] = dbg.ms_ts_pt[id];
                log_ms_record->decision = 2;
            }
        }
    }*/
}

void invoke_sched_epilogue_vanilla_2lvl(uint8_t id, struct ms_record* log_ms_record) {
    // if the mission critical core finished, release all. if core1, release core 2 and 3
    uint32_t i;
    for(i = id + 1; i < 4; ++i) {
        if (halt_mask >> i & 0b1) {
            leave_dbg_seq(i);
            dbg.milestone_timestamps[3][dbg.leave_dbg_ct++] = dbg.ms_ts_pt[id];
            log_ms_record->decision = 2;
        }
    }
}
