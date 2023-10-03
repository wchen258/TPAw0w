#ifndef TIMED_MILESTONE_GRAPH_H
#define TIMED_MILESTONE_GRAPH_H

typedef struct tmg_node tmg_node;

typedef struct {
	tmg_node* node;
	uint32_t nominal_time;
} __attribute__((packed)) successor;

struct tmg_node {
	uint32_t address;
	uint32_t tail_time;
	successor successors[4];
} __attribute__((packed));

#endif // TIMED_MILESTONE_GRAPH_H
