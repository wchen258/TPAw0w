#include "tpa_scheduler.h"
#include "coresight_lib.h"
#include "dbg_util.h"
#include "log_util.h"
#include "xtime_l.h"

extern uint8_t trc_buf_lock;
static uint8_t halt_mask = 0;

// logging vars
static uint8_t halt_resume_mask_diff = 0;

// vanilla 2lvl
static uint32_t prev_cores_negative_slack [4] = {0, 0, 0, 0};
static XTime self_halt_timeout [4] = {0, 0, 0, 0};
static uint8_t cores_core_mask [4] = {0, 0, 0, 0};
static uint8_t master_halt;
static uint8_t slave_halt;

//adaptive ss
static uint32_t ass_prev_negative_slack [4] = {0, 0, 0, 0};
static XTime    ass_self_halt_timeout [4] = {0, 0, 0, 0};
static uint8_t  ass_prev_halt_decision [4] = {4, 4, 4, 4};


static void enter_dbg_seq(uint8_t id) {
    if (id >= 4)
        return;

	if (halt_mask >> id & 0b1)
		return;

    a53_enter_dbg(id);
    trc_buf_lock |= 0b1 << id;
    halt_mask |= 0b1 << id;

    halt_resume_mask_diff |= 0b1 << id;
}

static void leave_dbg_seq(uint8_t id) {
    if (id >= 4)
        return;

	if (!(halt_mask >> id & 0b1))
		return;

    a53_leave_dbg(id, 1);
    trc_buf_lock &= ~(0b1 << id);
    halt_mask &= ~(0b1 << id);

    halt_resume_mask_diff |= (0b1 << id) << 4;
}

void reset_sched() {
	uint32_t i;
	halt_mask = 0;
	master_halt = 0;
	slave_halt = 0;
	for (i = 0; i < 4; ++i) {
		prev_cores_negative_slack[i] = 0;
        self_halt_timeout[i] = 0;
        cores_core_mask[i] = 0;
    }

    //adaptive ss shit
    for (i = 0; i < 4; ++i) {
        ass_prev_negative_slack[i] = 0;
        ass_self_halt_timeout[i] = 0;
        ass_prev_halt_decision[i] = 4;
    }

    halt_resume_mask_diff = 0;
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
    //uint32_t resumepoint = setpoint * (1.0 - dbg.beta[id]);
    uint32_t resumepoint = setpoint - (dbg.expected_solo_end_time[id] * dbg.beta[id]);

    halt_resume_mask_diff = 0;

    if (id == 0) {
		if (real_time > setpoint) {
			negative_slack = real_time - (acc_nominal_time[id] * dbg.alpha[id]);
			if (prev_cores_negative_slack[id] == 0) {
				enter_dbg_seq(2);
				enter_dbg_seq(3);
				master_halt = 0b1;
				prev_cores_negative_slack[id] = negative_slack;

				//log_ms_record->decision = 1;
				dbg.milestone_timestamps[2][dbg.enter_dbg_ct++] = dbg.ms_ts_pt[id];
			} else if (negative_slack >= prev_cores_negative_slack[id]) {
				enter_dbg_seq(3);
				enter_dbg_seq(2);
				enter_dbg_seq(1);
				master_halt = 0b11;

				// log_ms_record->decision = 1;
				//log_ms_record->decision = 3; // distinguish normal halt and master halt for logging purpose
				dbg.milestone_timestamps[2][dbg.enter_dbg_ct++] = dbg.ms_ts_pt[id];}
		} else if (real_time <= resumepoint) {
            if (self_halt_timeout[1] == 0) {
			    leave_dbg_seq(1);
            }
			if (slave_halt == 0) {
				leave_dbg_seq(2);
				leave_dbg_seq(3);
			}

			master_halt = 0;
			prev_cores_negative_slack[id] = 0;

			//log_ms_record->decision = 2;
			dbg.milestone_timestamps[3][dbg.leave_dbg_ct++] = dbg.ms_ts_pt[id];
		} else if (real_time < setpoint) {
            if (self_halt_timeout[1] == 0) {
                leave_dbg_seq(1);
            }
            master_halt = master_halt & 0b1;
        }
    } else if (id == 1) {
    	if (real_time > setpoint) {
    		enter_dbg_seq(2);
    		enter_dbg_seq(3);
    		slave_halt = 1;

    		//log_ms_record->decision = 1;
    		dbg.milestone_timestamps[2][dbg.enter_dbg_ct++] = dbg.ms_ts_pt[id];
    	} else if (real_time <= resumepoint) {
            if (master_halt == 0) {
                leave_dbg_seq(2);
                leave_dbg_seq(3);
                slave_halt = 0;
            }
            if (real_time < resumepoint && self_halt_timeout[id] == 0) {
                XTime_GetTime(&self_halt_timeout[id]);
                self_halt_timeout[id] = self_halt_timeout[id] + (resumepoint - real_time);

                halt_resume_mask_diff |= 0b10;
            }

    		//log_ms_record->decision = 2;
    		dbg.milestone_timestamps[3][dbg.leave_dbg_ct++] = dbg.ms_ts_pt[id];
    	}
    }

    log_ms_record->decision = halt_resume_mask_diff;
}

void invoke_sched_epilogue_vanilla_2lvl(uint8_t id, struct ms_record* log_ms_record) {
    // if the mission critical core finished, release all. if core1, release core 2 and 3
    uint32_t i;
    halt_resume_mask_diff = 0;
    for(i = id + 1; i < 4; ++i) {
        if (halt_mask >> i & 0b1) {
            leave_dbg_seq(i);
            dbg.milestone_timestamps[3][dbg.leave_dbg_ct++] = dbg.ms_ts_pt[id];
            //log_ms_record->decision = 2;
        }
    }
    log_ms_record->decision = halt_resume_mask_diff;
}

void invoke_sched_period_vanilla_2lvl(uint8_t id) {
    XTime current_time;

    if (id == 1 && self_halt_timeout[1] != 0) {
        XTime_GetTime(&current_time);

        if (current_time < self_halt_timeout[id]) {
            enter_dbg_seq(id);
        } else {
            leave_dbg_seq(id);
            self_halt_timeout[id] = 0;
        }
    }
}


/*
    Adaptive SS
*/

void invoke_sched_adaptive_ss(uint8_t id, uint32_t real_time, uint32_t* acc_nominal_time, struct ms_record* log_ms_record) {
    uint8_t j;
    uint32_t i, negative_slack;
    uint32_t setpoint = acc_nominal_time[id] * dbg.alpha[id];
    uint32_t resumepoint = setpoint - (dbg.expected_solo_end_time[id] * dbg.beta[id]);

    halt_resume_mask_diff = 0;

    if (real_time > setpoint) {
        negative_slack = real_time - (acc_nominal_time[id] * dbg.alpha[id]);
    
        if (negative_slack >= ass_prev_negative_slack[id]) {
            ass_prev_halt_decision[id] = (ass_prev_halt_decision[id] == id + 1) ? ass_prev_halt_decision[id] : (ass_prev_halt_decision[id] - 1);

            for (j = 3; j >= ass_prev_halt_decision[id]; j--) {
                enter_dbg_seq(j);
            }

            ass_prev_negative_slack[id] = negative_slack;
        }
    } else if (real_time < setpoint) {
        uint8_t prev_halt_decision = ass_prev_halt_decision[id];
        ass_prev_halt_decision[id] = 4;
        ass_prev_negative_slack[id] = 0;

        for (j = prev_halt_decision; j < 4; ++j) {
            if (ass_self_halt_timeout[j] == 0 && ass_prev_halt_decision[0] > j && ass_prev_halt_decision[1] > j && ass_prev_halt_decision[2] > j && ass_prev_halt_decision[3] > j) {
                leave_dbg_seq(j);
            }
        }

        if (real_time < resumepoint && ass_self_halt_timeout[id] == 0) {
            XTime_GetTime(&ass_self_halt_timeout[id]);
            ass_self_halt_timeout[id] = ass_self_halt_timeout[id] + (resumepoint - real_time);
            halt_resume_mask_diff |= (0b1 << id);
        }
    }

    log_ms_record->decision = halt_resume_mask_diff;
}

void invoke_sched_epilogue_adaptive_ss(uint8_t id, struct ms_record* log_ms_record) {
    uint32_t i;
    halt_resume_mask_diff = 0;

    ass_prev_halt_decision[id] = 4;
    ass_prev_negative_slack[id] = 0;

    for (i = id; i < 4; ++i) {
        if (ass_self_halt_timeout[i] == 0 && ass_prev_halt_decision[0] > i && ass_prev_halt_decision[1] > i && ass_prev_halt_decision[2] > i && ass_prev_halt_decision[3] > i) {
            leave_dbg_seq(i);
        }
    }

    log_ms_record->decision = halt_resume_mask_diff;
}

void invoke_sched_period_adaptive_ss(uint8_t id) {
    XTime current_time;

    if (ass_self_halt_timeout[id] != 0) {
        XTime_GetTime(&current_time);

        if (current_time < ass_self_halt_timeout[id]) {
            enter_dbg_seq(id);
        } else {
            leave_dbg_seq(id);
            ass_self_halt_timeout[id] = 0;
        }
    }
}