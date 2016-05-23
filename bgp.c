#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* AS policy for a give AS and destination */
struct PolicyInput {
    uint32_t as_number;
    size_t num_participants;
    uint64_t *ordering;
    uint64_t *export_policy;
};

#define get_export_policy(policy, next_hop, export_to) \
	policy->export_policy[next_hop * policy->num_participants + export_to]

static void initialize_policy(struct PolicyInput *policy, uint32_t as, size_t num_participants) {
    policy->as_number = as;
    policy->num_participants = num_participants;
    policy->ordering = (uint64_t *)malloc(sizeof(uint64_t) * num_participants);
    policy->export_policy = (uint64_t *)malloc(sizeof(uint64_t) * num_participants * num_participants);
}

static inline uint64_t usecs() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000ul + t.tv_nsec/1000ul;
}

static int rand_int(unsigned long n) {
	unsigned long limit = (1<< 31) - ((1<< 31) % n);
    unsigned long rnd;

    do {
        rnd = lrand48();
    } while (rnd >= limit);
    return rnd % n;
}

static void inline shuffle(uint64_t *array, unsigned long n) {
    int i, j, tmp;

    for (i = n - 1; i > 0; i--) {
        j = rand_int(i + 1);
        tmp = array[j];
        array[j] = array[i];
        array[i] = tmp;
    }
}

static void randomize_policy(struct PolicyInput *policy) {
    for (int i = 0; i < policy->num_participants; i++) {
        policy->ordering[i] = i;
    }
    shuffle(policy->ordering, policy->num_participants);
    for (int i = 0; i < policy->num_participants; i++) {
    	for (int j = 0; j < policy->num_participants; j++) {
    		get_export_policy(policy, i, j) = rand_int(2);
		}
	}
}

static void compute_next_hop(int dest, uint64_t* hops, struct PolicyInput *policies, uint64_t participants) {
	int changed = 0;
	int available[participants];
	int loops = 0;
	memset(hops, 0, sizeof(uint64_t) * participants);
	memset(available, 0, sizeof(int) * participants);
	available[dest] = 1;
	hops[dest] = dest;
	do {
		uint64_t loop_start, loop_end;
		loop_start = usecs();
		changed = 0;
		for (int as = 0; as < participants; as++) {
			if (as == dest) {
				continue;
			}
			uint64_t chosen = 0;
			int success = 0;
			for (int possible = 0; possible < participants; possible++) {
				uint64_t next_hop_check = policies[as].ordering[possible];
				if (available[next_hop_check] && // Has a path
					get_export_policy((policies + next_hop_check), hops[next_hop_check], as)) {
					// Found a path 
					chosen = next_hop_check;
					if (hops[as] != chosen) {
						hops[as] = chosen;
						available[as] = 1;
						changed = 1;
						success = 1;
					}
				}
			}
			if (available[as] != success) {
				available[as] = success;
				changed = 1;
			}
		}
		loop_end = usecs();
		loops ++;
		printf("Loop %d took %lu usecs\n", loops, loop_end - loop_start);
	} while(changed && loops < 500);
	printf("Took %d iters\n", loops);
}

void show_usage() { printf("bgp -s size [-r random_seed] "); }

int main(int argc, char **argv) {
    size_t size = 100;
    char opt;
    long int seed = 42;
    while ((opt = getopt(argc, argv, "s:r:h")) != -1) {
        switch (opt) {
        case 's':
            size = atoi(optarg);
            break;
        case 'r':
            seed = atol(optarg);
            break;
        default:
            show_usage();
        }
    }
    srand48(seed);
    // Consider the single destination case first.
    struct PolicyInput inputs[size];
    uint64_t start = usecs();
    for (size_t i = 0; i < size; i++) {
        initialize_policy(&inputs[i], (uint32_t)i, size);
        randomize_policy(&inputs[i]);
    }
    uint64_t stop = usecs();
    printf("Initialization took %lu usec\n", (stop - start));
    printf("Total size %lu KB\n", sizeof(inputs) / (1024));
    uint64_t hops[size];
    printf("Export policy for %d\n");
    for (int i = 0; i < size; i++) {
    	printf("%d ", get_export_policy((inputs + 12), 12, i));
	}
	printf("\n");
    compute_next_hop(12, hops, inputs, size);
	for (int i = 0; i < size; i++)
		printf("%d %lu\n", i, hops[i]);
}
