CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -I.
LDFLAGS = -lm -lpthread

CORE    = ca_core/grid.o ca_core/rules.o ca_core/coupling.o ca_core/engine.o ca_core/bitgrid.o ca_core/schedule.o ca_core/hybrid_engine.o ca_core/patterns.o ca_core/intent.o ca_core/pipeline.o ca_core/genome.o ca_core/local_ga.o ca_core/wave_grid.o ca_core/wave_coupling.o ca_core/wave_intent.o ca_core/wave_rules.o ca_core/wave_engine.o ca_core/wave_local_ga.o ca_core/composable_ruleset.o
PCORE   = ca_core/payload_grid.o ca_core/payload_rules.o ca_core/payload_coupling.o ca_core/payload_engine.o ca_core/payload_intent.o ca_core/payload_local_ga.o ca_core/payload_pipeline.o
DSL     = dsl/parser.o dsl/codegen.o dsl/bind.o
UTILS   = utils/rng.o utils/thread_pool.o
METRICS = metrics/metrics.o metrics/export.o metrics/glossary.o metrics/transfer_entropy.o metrics/history.o

OBJS    = $(CORE) $(PCORE) $(DSL) $(UTILS) $(METRICS)

EXPS    = exp_signal exp_sync exp_memory exp_gates exp_sweep exp_payload exp_hybrid_bus exp_hybrid_glider exp_hybrid_reservoir exp_ga_ic exp_local_ga exp_convergence exp_delay_sweep exp_bench exp_rule_mod exp_binary_logic_ga exp_payload_logic_ga exp_wave_logic_ga exp_hybrid_logic exp_glider_preservation exp_ruleset_profile exp_compositional_logic exp_multimodal_logic_ga exp_multimodal_wave_logic_ga exp_multimodal_polar_logic_ga exp_directional_multimodal exp_hierarchical exp_composable_stage0 exp_composable_stage1 exp_te_evolution
TESTS   = tests/test_grid tests/test_coupling tests/test_dsl tests/test_bitgrid tests/test_bind tests/test_schedule tests/test_glossary tests/test_hybrid tests/test_patterns tests/test_intent tests/test_transfer_entropy tests/test_pipeline tests/test_pipeline_parallel tests/test_genome tests/test_local_ga tests/test_wave tests/test_payload_pipeline tests/test_composable_ruleset

.PHONY: all clean test run-tests bench

all: $(EXPS)

exp_signal: experiments/exp_signal.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_sync: experiments/exp_sync.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_memory: experiments/exp_memory.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_gates: experiments/exp_gates.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_sweep: experiments/exp_sweep.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_payload: experiments/exp_payload.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_hybrid_bus: experiments/exp_hybrid_bus.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_hybrid_glider: experiments/exp_hybrid_glider.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_hybrid_reservoir: experiments/exp_hybrid_reservoir.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_ga_ic: experiments/exp_ga_ic.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_local_ga: experiments/exp_local_ga.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_convergence: experiments/exp_convergence.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_delay_sweep: experiments/exp_delay_sweep.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_bench: experiments/exp_bench.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_rule_mod: experiments/exp_rule_mod.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_binary_logic_ga: experiments/exp_binary_logic_ga.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_payload_logic_ga: experiments/exp_payload_logic_ga.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_wave_logic_ga: experiments/exp_wave_logic_ga.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_hybrid_logic: experiments/exp_hybrid_logic.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_glider_preservation: experiments/exp_glider_preservation.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_ruleset_profile: experiments/exp_ruleset_profile.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_compositional_logic: experiments/exp_compositional_logic.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_rule110_tag: experiments/exp_rule110_tag.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_multimodal_logic_ga: experiments/exp_multimodal_logic_ga.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_multimodal_wave_logic_ga: experiments/exp_multimodal_wave_logic_ga.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_multimodal_polar_logic_ga: experiments/exp_multimodal_polar_logic_ga.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_directional_multimodal: experiments/exp_directional_multimodal.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_hierarchical: experiments/exp_hierarchical.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_composable_stage0: experiments/exp_composable_stage0.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_composable_stage1: experiments/exp_composable_stage1.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

exp_te_evolution: experiments/exp_te_evolution.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_grid: tests/test_grid.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_coupling: tests/test_coupling.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_dsl: tests/test_dsl.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_bitgrid: tests/test_bitgrid.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_bind: tests/test_bind.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_schedule: tests/test_schedule.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_glossary: tests/test_glossary.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_hybrid: tests/test_hybrid.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_patterns: tests/test_patterns.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_intent: tests/test_intent.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_transfer_entropy: tests/test_transfer_entropy.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_pipeline: tests/test_pipeline.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_pipeline_parallel: tests/test_pipeline_parallel.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_genome: tests/test_genome.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_local_ga: tests/test_local_ga.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_wave: tests/test_wave.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_payload_pipeline: tests/test_payload_pipeline.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/test_composable_ruleset: tests/test_composable_ruleset.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

tests/bench_pipeline: tests/bench_pipeline.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Header dependency tracking: regenerate .o whenever any included header changes.
DEPS = $(OBJS:.o=.d)
%.d: %.c
	@$(CC) $(CFLAGS) -MM -MT $(@:.d=.o) -MF $@ $<
-include $(DEPS)

clean:
	rm -f $(EXPS) $(TESTS) $(OBJS) $(DEPS) tests/bench_pipeline

run-tests: all $(TESTS)
	./tests/test_grid
	./tests/test_coupling
	./tests/test_dsl
	./tests/test_bitgrid
	./tests/test_bind
	./tests/test_schedule
	./tests/test_glossary
	./tests/test_hybrid
	./tests/test_patterns
	./tests/test_intent
	./tests/test_transfer_entropy
	./tests/test_pipeline
	./tests/test_pipeline_parallel
	./tests/test_genome
	./tests/test_local_ga
	./tests/test_wave
	./tests/test_payload_pipeline
	./exp_signal --grids 2 --size 64 --steps 100 --seed 42 > /dev/null
	./exp_signal --grids 2 --size 32 --steps 50 --seed 1 --connect 0:bottom->1:top > /dev/null
	./exp_sync --grids 2 --size 64 --steps 200 --seed 42 > /dev/null
	./exp_memory --size 64 --steps 200 --seed 42 > /dev/null
	./exp_sweep --size 16 --steps 50 --trials 2 --seed 42 --rules 0-1 --topos none,uni > /dev/null
	./exp_payload --grids 2 --size 16 --steps 20 --seed 42 --connect 0:bottom->1:top,1.0 > /dev/null
	./exp_hybrid_bus --size 16 --steps 20 --seed 42 --xconnect 'B0:bottom->P0:top' > /dev/null
	./exp_hybrid_glider --size 32 --steps 50 --seed 42 > /dev/null
	./exp_hybrid_reservoir --size 16 --steps 30 --trials 2 --seed 42 > /dev/null
	./exp_ga_ic --pop 10 --gen 3 --size 8 --steps 10 --seed 42 > /dev/null
	./exp_local_ga --size 16 --steps 40 --window 10 --seed 42 > /dev/null
	./exp_rule_mod --size 16 --steps 50 --window 20 --trials 2 --seed 42 > /dev/null
	@echo "All tests passed."

test: run-tests

bench: tests/bench_pipeline
	@echo "--- Pipeline benchmark (regression threshold: 5%) ---"
	./tests/bench_pipeline
