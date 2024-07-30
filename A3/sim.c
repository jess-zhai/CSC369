#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/types.h>
#include <signal.h>
#include "malloc369.h"
#include "sim.h"
#include "coremap.h"
#include "swap.h"

static void install_fatal_handlers(); /* To remove swapfile on failure */

// Define global variables declared in sim.h
size_t memsize = 0;
int debug = 0;
unsigned char *physmem = NULL;
struct frame *coremap = NULL;

/* Each eviction algorithm is represented by a structure with its name
 * and three functions.
 */
struct functions {
	const char *name;          // String name of eviction algorithm
	void (*init)(void);        // Initialize any data needed by alg
	void (*cleanup)(void);     // Cleanup any data initialized in init()
	void (*ref)(int, vaddr_t); // Called on each reference
	int (*evict)(void);        // Called to choose victim for eviction
};

/* The algs array gives us a mapping between the name of an eviction
 * algorithm as given in a command line argument, and the function to
 * call to select the victim page.
 *
 * The list of REPLACEMENT_ALGORITHMS is found in coremap.h
 * We use the C preprocessor stringizing and concatenation operations to
 * create a template for the algorithm function structure.
 * See https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html
 * and https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html
 */
static struct functions algs[] = {
#define RA(name) \
	{ #name, name ## _init, name ## _cleanup, name ## _ref, name ## _evict },
REPLACEMENT_ALGORITHMS
#undef RA
};
static int num_algs = sizeof(algs) / sizeof(algs[0]);

static void (*init_func)() = NULL;
static void (*cleanup_func)() = NULL;
void (*ref_func)(int, vaddr_t) = NULL;
int (*evict_func)() = NULL;


/* An actual memory access based on the vaddr from the trace file.
 *
 * The find_physpage() function is called to translate the virtual address
 * to a (simulated) physical address -- that is, a pointer to the right
 * location in physmem array. The find_physpage() function is responsible for
 * everything to do with memory management - including translation using the
 * pagetable, allocating a frame of (simulated) physical memory (if needed),
 * evicting an existing page from the frame (if needed) and reading the page
 * in from swap (if needed).
 *
 * We then check that the memory has the expected content (just a copy of the
 * virtual address) and, in case of a write reference, increment the version
 * counter.
 */
static void
access_mem(char type, vaddr_t vaddr, unsigned char val, size_t linenum)
{
	unsigned char *pgptr; 
	unsigned char *memptr;
	unsigned offset = vaddr % PAGE_SIZE;
	
	pgptr = find_physpage(vaddr, type);
	memptr = pgptr + offset;

	if ((type == 'S') || (type == 'M')) {
		// write access to page, update value in simulated memory
		*memptr = val;
	} else if ((type == 'L' || type == 'I')) {
		if (*memptr != val) {
			printf("ERROR at trace line %zu: vaddr has %hhu but should have %hhu\n",
			       linenum, *memptr, val);
		}
	}
}

static void
replay_trace(FILE *f)
{
	char line[256];
	size_t linenum = 0;
	while (fgets(line, sizeof(line), f)) {
		++linenum;
		if (line[0] == '=') {
			continue;
		}

		vaddr_t vaddr;
		char type;
		unsigned char val;
		if (sscanf(line, "%c %zx %hhu", &type, &vaddr, &val) != 3) {
			fprintf(stderr, "Invalid trace line %zu: %s\n",
				linenum, line);
			exit(1);
		}
		if (type != 'I' && type != 'L' && type != 'S' && type != 'M') {
			fprintf(stderr,"Invalid reftype, line %zu: %s\n",
				linenum, line);
			exit(1);
		}
		if ((vaddr % PAGE_SIZE) > SIMPAGESIZE) {
			fprintf(stderr,"Invalid vaddr, offset must be in range of simulated page frame size, line %zu: %s\n",
				linenum, line);
			exit(1);
		}
		if (debug > 1) {			
			printf("%c %lx %hhu\n", type, vaddr, val);
		}
		
		access_mem(type, vaddr, val, linenum);
	}
}

void
usage(char *prog)
{
	fprintf(stderr,
		"USAGE: %s -f tracefile "
		"-m memorysize -s swapsize -a algorithm [-v num -p]\n", prog);
	fprintf(stderr, "\t-f tracefile  - path to trace file to simulate\n");
	fprintf(stderr, "\t-m memorysize - number of physical memory frames\n");
	fprintf(stderr, "\t-s swapsize   - number of frames in swapfile\n");
	fprintf(stderr, "\t-a algorithm  - replacement algorithm to use, one of:\n");
	for (int i = 0; i < num_algs; ++i) {
		fprintf(stderr, "\t\t%s\n",algs[i].name);
	}
	fprintf(stderr, "\t-d num        - debug level for output\n");
	fprintf(stderr, "\t-p            - print pagetable at end\n"); 
}

int
main(int argc, char *argv[])
{
	double starttime;
	double endtime;
	long start_mallocs;
	long start_bytes;
	long bytes_used;
	size_t swapsize = 0;
	char *tracefile = NULL;
	char *replacement_alg = NULL;
	int opt;
	bool print_pgtbl = false;
	
	while ((opt = getopt(argc, argv, "f:m:a:s:d:ph")) != -1) {
		switch (opt) {
		case 'f':
			tracefile = optarg;
			break;
		case 'm':
			memsize = strtoul(optarg, NULL, 10);
			break;
		case 'a':
			replacement_alg = optarg;
			break;
		case 's':
			swapsize = strtoul(optarg, NULL, 10);
			break;
		case 'd':
			debug = strtol(optarg, NULL, 10);
			break;
		case 'p':
			print_pgtbl = true;
			break;
		case 'h':
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (!tracefile || !memsize || !swapsize || !replacement_alg) {
		usage(argv[0]);
		return 1;
	}
	
	FILE *tfp = fopen(tracefile, "r");
	if (!tfp) {
		perror(tracefile);
		return 1;
	}

	// Initialize main data structures for simulation.
	// This happens before calling the replacement algorithm init function
	// so that the init_func can refer to the coremap if needed.
	init_csc369_malloc(false);
	coremap = malloc369(memsize * sizeof(struct frame));
	memset(coremap, 0, memsize*sizeof(struct frame));
	physmem = malloc369(memsize * SIMPAGESIZE);
	memset(physmem, 0, memsize*SIMPAGESIZE);
	swap_init(swapsize);
	install_fatal_handlers();
	
	// Get initial memory use after initializing main simulation data structures.
	start_mallocs = get_current_num_mallocs();
	start_bytes = get_current_bytes_malloced();

	for (int i = 0; i < num_algs; ++i) {
		if (strcmp(algs[i].name, replacement_alg) == 0) {
			init_func = algs[i].init;
			cleanup_func = algs[i].cleanup;
			ref_func = algs[i].ref;
			evict_func = algs[i].evict;
			break;
		}
	}
	if (!evict_func) {
		fprintf(stderr, "Error: invalid replacement algorithm - %s\n",
				replacement_alg);
		return 1;
	}

	// Timed section of code starts here. This includes:
	//     - initialization of the pagetable
	//     - initialization of the replacement algorithm
	//     - replaying the trace
	starttime = get_time();
	init_pagetable(); /* pagetable initialization */
	init_func();      /* replacement algorithm initialization */
	replay_trace(tfp);
	endtime = get_time();
	// End of timed section of code.

	// Get final memory use.
	bytes_used = get_current_bytes_malloced() - start_bytes;
	
	// Print statistics.
	printf("Hit count: %zu\n", hit_count);
	printf("Miss count: %zu\n", miss_count);
	printf("Clean evictions: %zu\n", evict_clean_count);
	printf("Dirty evictions: %zu\n", evict_dirty_count);
	printf("Total references: %zu\n", ref_count);
	printf("Hit rate: %.4f\n", ((double)hit_count / ref_count) * 100.0);
	printf("Miss rate: %.4f\n", ((double)miss_count / ref_count) * 100.0);

	printf("Time to run simulation: %f\n",endtime - starttime);
	printf("Memory used by simulation: %ld bytes\n", bytes_used);

	if (print_pgtbl) {
		print_pagetable();
	}
	
	cleanup_func();

	// Cleanup data structures and remove temporary swapfile
	fclose(tfp);
	free369(coremap);
	free369(physmem);
	swap_destroy(true);
	free_pagetable();

	// Check for memory leaks
	if (is_leak_free(start_mallocs, start_bytes)) {
		printf("No memory leaks detected.\n");
	} else {
		long bytes_leaked = get_current_bytes_malloced() - start_bytes;
		long unfreed_mallocs = get_current_num_mallocs() - start_mallocs;
		printf("Detected %lu bytes leaked from %lu un-freed mallocs.\n",
		       bytes_leaked, unfreed_mallocs);
	}
	
	return 0;
}

/*********** TRY TO CATCH SIGNALS FOR ERROR REPORTING ***************/

/* report error-causing %rip relative to this addr */
static unsigned long start_addr; 

static void
fatal_signal_handler(int signum, siginfo_t *info, void *context)
{
	char msg[100];
	ucontext_t *uc = (ucontext_t *)context;
	unsigned long pc = uc->uc_mcontext.gregs[REG_RIP];
	
	snprintf(msg, 100,
		 "%s at instruction %lx (addr %p)\n\n",
		 strsignal(signum), pc, info->si_addr);
	fflush(0);
	write(0, msg, strlen(msg+1));
	swap_destroy(false);
	exit(signum);
}

static void
install_fatal_handlers()
{
	struct sigaction sig_action;
	struct sigaction old_action;

	/* Very rough guess at start of code segment in memory */
	start_addr = (unsigned long)main & 0xfffffffffffff000;
	
	memset(&sig_action, 0, sizeof(sig_action));
	sig_action.sa_sigaction = fatal_signal_handler;
	sig_action.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&sig_action.sa_mask);

	sigaction(SIGSEGV, &sig_action, &old_action);
	sigaction(SIGABRT, &sig_action, &old_action);
	sigaction(SIGTRAP, &sig_action, &old_action);
	sigaction(SIGILL, &sig_action, &old_action);
	sigaction(SIGFPE, &sig_action, &old_action);
}
