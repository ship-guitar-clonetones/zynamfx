// RtWorkerPool tests
//
// The pool's contract (see RtWorkerPool.h) is scheduling-only: forkJoin runs
// every job exactly once and returns only when all are complete, no matter
// how the jobs land across workers, how deeply forks nest (depth 1 in
// production: a lane job forking NAM phases), whether the slot registry
// saturates, or whether any workers exist at all. These tests pin that
// contract; the bit-exactness of the *audio* under the pool's schedules is
// pinned by multicore_tests.cpp on the real processor.
#include "RtWorkerPool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr int kBlock = 512;

struct CountJob {
  std::atomic<int>* counter;
};

void bumpJob(void* ctx) {
  static_cast<CountJob*>(ctx)->counter->fetch_add(1, std::memory_order_relaxed);
}

// Fork every count in [1, kMaxJobs] many times over: every job runs exactly
// once per fork, and forkJoin never returns before all are done (a lost or
// double-run job shows up as a wrong counter).
TEST(RtWorkerPoolTest, RunsEveryJobExactlyOnce) {
  RtWorkerPool pool;
  pool.start(kRate, kBlock);

  for (int count = 1; count <= RtWorkerPool::kMaxJobs; ++count) {
    constexpr int kIterations = 200;
    std::vector<std::atomic<int>> counters(static_cast<size_t>(count));
    for (auto& c : counters)
      c.store(0);

    for (int iter = 0; iter < kIterations; ++iter) {
      CountJob jobs[RtWorkerPool::kMaxJobs];
      void* ctxs[RtWorkerPool::kMaxJobs];
      for (int i = 0; i < count; ++i) {
        jobs[i] = {&counters[static_cast<size_t>(i)]};
        ctxs[i] = &jobs[i];
      }
      pool.forkJoin(bumpJob, ctxs, count);
    }

    for (int i = 0; i < count; ++i)
      EXPECT_EQ(counters[static_cast<size_t>(i)].load(), kIterations)
          << "job " << i << " of " << count;
  }
}

// A pool that was never started (and one started with zero workers) must
// degrade to plain serial execution on the calling thread, the production
// fallback when prepareToPlay hasn't run or every core is spoken for.
TEST(RtWorkerPoolTest, RunsInlineWithoutWorkers) {
  const auto callerId = std::this_thread::get_id();

  auto verifyInline = [&](RtWorkerPool& pool) {
    std::atomic<int> ran{0};
    std::atomic<bool> offThread{false};
    struct Ctx {
      std::atomic<int>* ran;
      std::atomic<bool>* offThread;
      std::thread::id caller;
    } ctx{&ran, &offThread, callerId};

    void* ctxs[4] = {&ctx, &ctx, &ctx, &ctx};
    pool.forkJoin(
        [](void* c) {
          auto& j = *static_cast<Ctx*>(c);
          j.ran->fetch_add(1);
          if (std::this_thread::get_id() != j.caller)
            j.offThread->store(true);
        },
        ctxs, 4);

    EXPECT_EQ(ran.load(), 4);
    EXPECT_FALSE(offThread.load()) << "job escaped to a worker that shouldn't exist";
  };

  RtWorkerPool neverStarted;
  EXPECT_FALSE(neverStarted.isRunning());
  verifyInline(neverStarted);

  RtWorkerPool zeroWorkers;
  zeroWorkers.start(kRate, kBlock, 0);
  EXPECT_EQ(zeroWorkers.numWorkers(), 0);
  verifyInline(zeroWorkers);
}

// With workers running, forked jobs should actually land on other threads
// (otherwise the pool is a very elaborate serial loop). Scheduling is not
// deterministic per fork, so this asserts over many forks of slow-ish jobs.
TEST(RtWorkerPoolTest, JobsRunOnWorkerThreads) {
  RtWorkerPool pool;
  pool.start(kRate, kBlock);
  if (pool.numWorkers() == 0)
    GTEST_SKIP() << "single-core machine: pool has no workers";

  std::atomic<bool> sawWorker{false};
  struct Ctx {
    std::atomic<bool>* sawWorker;
    std::thread::id caller;
  } ctx{&sawWorker, std::this_thread::get_id()};

  for (int iter = 0; iter < 100 && !sawWorker.load(); ++iter) {
    void* ctxs[8];
    for (auto& c : ctxs)
      c = &ctx;
    pool.forkJoin(
        [](void* c) {
          auto& j = *static_cast<Ctx*>(c);
          // Enough work that a signalled worker can claim a sibling job
          // before the caller inlines everything.
          volatile double sink = 1.0;
          for (int i = 0; i < 20000; ++i)
            sink = sink * 1.0000001 + 1e-9;
          if (std::this_thread::get_id() != j.caller)
            j.sawWorker->store(true);
        },
        ctxs, 8);
  }

  EXPECT_TRUE(sawWorker.load()) << "no job ever ran on a worker across 100 forks";
}

// Production nesting: an outer fork (the stereo lanes) whose jobs each fork
// again (their NAM block's phases). The inner join runs on whichever thread
// runs the outer job, including a worker, and must complete without
// deadlock, every leaf exactly once.
TEST(RtWorkerPoolTest, NestedForkJoinRunsEveryLeaf) {
  RtWorkerPool pool;
  pool.start(kRate, kBlock);

  constexpr int kOuter = 2;
  constexpr int kInner = 8;
  constexpr int kIterations = 500;

  std::vector<std::atomic<int>> leaves(kOuter * kInner);
  for (auto& c : leaves)
    c.store(0);

  struct OuterCtx {
    RtWorkerPool* pool;
    std::atomic<int>* leaves;  // this outer job's kInner counters
  };

  for (int iter = 0; iter < kIterations; ++iter) {
    OuterCtx outer[kOuter];
    void* outerCtxs[kOuter];
    for (int o = 0; o < kOuter; ++o) {
      outer[o] = {&pool, &leaves[static_cast<size_t>(o * kInner)]};
      outerCtxs[o] = &outer[o];
    }

    pool.forkJoin(
        [](void* c) {
          auto& j = *static_cast<OuterCtx*>(c);
          CountJob inner[kInner];
          void* innerCtxs[kInner];
          for (int i = 0; i < kInner; ++i) {
            inner[i] = {&j.leaves[i]};
            innerCtxs[i] = &inner[i];
          }
          j.pool->forkJoin(bumpJob, innerCtxs, kInner);
        },
        outerCtxs, kOuter);
  }

  for (size_t i = 0; i < leaves.size(); ++i)
    EXPECT_EQ(leaves[i].load(), kIterations) << "leaf " << i;
}

// Saturate the slot registry: kMaxJobs outer jobs each forking kMaxJobs
// inner jobs wants far more slots than exist. Overflow must fall back to
// inline execution (never block, never drop a job).
TEST(RtWorkerPoolTest, RegistryOverflowFallsBackInline) {
  RtWorkerPool pool;
  pool.start(kRate, kBlock);

  constexpr int kOuter = RtWorkerPool::kMaxJobs;
  constexpr int kInner = RtWorkerPool::kMaxJobs;

  std::atomic<int> leafCount{0};
  struct OuterCtx {
    RtWorkerPool* pool;
    std::atomic<int>* leafCount;
  } ctx{&pool, &leafCount};

  void* outerCtxs[kOuter];
  for (auto& c : outerCtxs)
    c = &ctx;

  pool.forkJoin(
      [](void* c) {
        auto& j = *static_cast<OuterCtx*>(c);
        CountJob inner[kInner];
        void* innerCtxs[kInner];
        for (int i = 0; i < kInner; ++i) {
          inner[i] = {j.leafCount};
          innerCtxs[i] = &inner[i];
        }
        j.pool->forkJoin(bumpJob, innerCtxs, kInner);
      },
      outerCtxs, kOuter);

  EXPECT_EQ(leafCount.load(), kOuter * kInner);
}

// Two external threads forking concurrently (beyond production's single
// audio-thread entry) hammer slot acquisition from both sides; every job of
// every group must still run exactly once for its own group.
TEST(RtWorkerPoolTest, ConcurrentForksFromTwoThreads) {
  RtWorkerPool pool;
  pool.start(kRate, kBlock);

  constexpr int kIterations = 2000;
  constexpr int kCount = 4;

  auto hammer = [&pool](std::atomic<int>* counters) {
    for (int iter = 0; iter < kIterations; ++iter) {
      CountJob jobs[kCount];
      void* ctxs[kCount];
      for (int i = 0; i < kCount; ++i) {
        jobs[i] = {&counters[i]};
        ctxs[i] = &jobs[i];
      }
      pool.forkJoin(bumpJob, ctxs, kCount);
    }
  };

  std::vector<std::atomic<int>> countersA(kCount), countersB(kCount);
  for (auto& c : countersA)
    c.store(0);
  for (auto& c : countersB)
    c.store(0);

  std::thread threadB([&] { hammer(countersB.data()); });
  hammer(countersA.data());
  threadB.join();

  for (int i = 0; i < kCount; ++i) {
    EXPECT_EQ(countersA[static_cast<size_t>(i)].load(), kIterations) << "thread A job " << i;
    EXPECT_EQ(countersB[static_cast<size_t>(i)].load(), kIterations) << "thread B job " << i;
  }
}

// Host lifecycle: repeated start/stop with forks in between, including forks
// while stopped (which must run inline, not hang on a dead pool): the
// releaseResources → defensive processBlock → prepareToPlay sequence.
TEST(RtWorkerPoolTest, SurvivesStartStopChurn) {
  RtWorkerPool pool;
  std::atomic<int> counter{0};

  auto forkSome = [&] {
    for (int iter = 0; iter < 50; ++iter) {
      CountJob jobs[6];
      void* ctxs[6];
      for (int i = 0; i < 6; ++i) {
        jobs[i] = {&counter};
        ctxs[i] = &jobs[i];
      }
      pool.forkJoin(bumpJob, ctxs, 6);
    }
  };

  int expected = 0;
  for (int cycle = 0; cycle < 5; ++cycle) {
    pool.start(kRate, kBlock);
    forkSome();
    expected += 50 * 6;

    pool.stop();
    EXPECT_FALSE(pool.isRunning());
    forkSome();  // stopped: everything inline
    expected += 50 * 6;

    EXPECT_EQ(counter.load(), expected) << "cycle " << cycle;
  }
}

}  // namespace
