/* XMRig
 * Copyright 2018-2019 MoneroOcean <https://github.com/MoneroOcean>, <support@moneroocean.stream>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "core/Benchmark.h"
#include "core/Controller.h"
#include "core/config/Config.h"
#include "core/Miner.h"
#include "base/io/log/Log.h"
#include "base/net/stratum/Job.h"
#include "net/JobResult.h"
#include "net/JobResults.h"
#include "net/Network.h"
#include "rapidjson/document.h"
#include <chrono>

namespace xmrig {                          

Benchmark::Benchmark() : m_controller(nullptr), m_isNewBenchRun(true) {
  for (BenchAlgo bench_algo = static_cast<BenchAlgo>(0); bench_algo != BenchAlgo::MAX; bench_algo = static_cast<BenchAlgo>(bench_algo + 1)) {
    m_bench_job[bench_algo] = new Job(false, Algorithm(ba2a[bench_algo]), "benchmark");
  }
}

Benchmark::~Benchmark() {
  for (BenchAlgo bench_algo = static_cast<BenchAlgo>(0); bench_algo != BenchAlgo::MAX; bench_algo = static_cast<BenchAlgo>(bench_algo + 1)) {
    delete m_bench_job[bench_algo];
  }
}

// start performance measurements from the first bench_algo
void Benchmark::start() {
    JobResults::setListener(this); // register benchmark as job result listener to compute hashrates there
    // write text before first benchmark round
    LOG_ALERT(">>>>> STARTING ALGO PERFORMANCE CALIBRATION (with %i seconds round)", m_controller->config()->benchAlgoTime());
    // start benchmarking from first PerfAlgo in the list
    start(xmrig::Benchmark::MIN);
}

// end of benchmarks, switch to jobs from the pool (network), fill algo_perf
void Benchmark::finish() {
    for (Algorithm::Id algo = static_cast<Algorithm::Id>(0); algo != Algorithm::MAX; algo = static_cast<Algorithm::Id>(algo + 1)) {
        algo_perf[algo] = get_algo_perf(algo);
    }
    m_bench_algo = BenchAlgo::INVALID;
    m_controller->miner()->pause(); // do not compute anything before job from the pool
    JobResults::setListener(m_controller->network());
    m_controller->start();
}

rapidjson::Value Benchmark::toJSON(rapidjson::Document &doc) const
{
    using namespace rapidjson;
    auto &allocator = doc.GetAllocator();

    Value obj(kObjectType);

    for (const auto &a : m_controller->miner()->algorithms()) {
        obj.AddMember(StringRef(a.shortName()), algo_perf[a.id()], allocator);
    }

    return obj;
}

void Benchmark::read(const rapidjson::Value &value)
{
    for (Algorithm::Id algo = static_cast<Algorithm::Id>(0); algo != Algorithm::MAX; algo = static_cast<Algorithm::Id>(algo + 1)) {
        algo_perf[algo] = 0.0f;
    }
    if (value.IsObject()) {
        for (auto &member : value.GetObject()) {
            const Algorithm algo(member.name.GetString());
            if (!algo.isValid()) {
                LOG_ALERT("Ignoring wrong algo-perf name %s", member.name.GetString());
                continue;
            }
            if (member.value.IsFloat()) {
                algo_perf[algo.id()] = member.value.GetFloat();
                m_isNewBenchRun = false;
                continue;
            }
            if (member.value.IsInt()) {
                algo_perf[algo.id()] = member.value.GetInt();
                m_isNewBenchRun = false;
                continue;
            }
            LOG_ALERT("Ignoring wrong value for %s algo-perf", member.name.GetString());
        }
    }
}

float Benchmark::get_algo_perf(Algorithm::Id algo) const {
    switch (algo) {
        case Algorithm::CN_0:          return m_bench_algo_perf[BenchAlgo::CN_R];
        case Algorithm::CN_1:          return m_bench_algo_perf[BenchAlgo::CN_R];
        case Algorithm::CN_2:          return m_bench_algo_perf[BenchAlgo::CN_R];
        case Algorithm::CN_R:          return m_bench_algo_perf[BenchAlgo::CN_R];
        case Algorithm::CN_WOW:        return m_bench_algo_perf[BenchAlgo::CN_R];
        case Algorithm::CN_FAST:       return m_bench_algo_perf[BenchAlgo::CN_R] * 2;
        case Algorithm::CN_HALF:       return m_bench_algo_perf[BenchAlgo::CN_R] * 2;
        case Algorithm::CN_XAO:        return m_bench_algo_perf[BenchAlgo::CN_R];
        case Algorithm::CN_RTO:        return m_bench_algo_perf[BenchAlgo::CN_R];
        case Algorithm::CN_RWZ:        return m_bench_algo_perf[BenchAlgo::CN_R] / 3 * 4;
        case Algorithm::CN_ZLS:        return m_bench_algo_perf[BenchAlgo::CN_R] / 3 * 4;
        case Algorithm::CN_DOUBLE:     return m_bench_algo_perf[BenchAlgo::CN_R] / 2;
        case Algorithm::CN_GPU:        return m_bench_algo_perf[BenchAlgo::CN_GPU];
        case Algorithm::CN_LITE_0:     return m_bench_algo_perf[BenchAlgo::CN_LITE_1];
        case Algorithm::CN_LITE_1:     return m_bench_algo_perf[BenchAlgo::CN_LITE_1];
        case Algorithm::CN_HEAVY_0:    return m_bench_algo_perf[BenchAlgo::CN_HEAVY_TUBE];
        case Algorithm::CN_HEAVY_TUBE: return m_bench_algo_perf[BenchAlgo::CN_HEAVY_TUBE];
        case Algorithm::CN_HEAVY_XHV:  return m_bench_algo_perf[BenchAlgo::CN_HEAVY_TUBE];
        case Algorithm::CN_PICO_0:     return m_bench_algo_perf[BenchAlgo::CN_PICO_0];
        case Algorithm::RX_LOKI:       return m_bench_algo_perf[BenchAlgo::RX_LOKI];
        case Algorithm::RX_WOW:        return m_bench_algo_perf[BenchAlgo::RX_WOW];
        case Algorithm::DEFYX:         return m_bench_algo_perf[BenchAlgo::DEFYX];
        case Algorithm::AR2_CHUKWA:    return m_bench_algo_perf[BenchAlgo::AR2_CHUKWA];
        case Algorithm::AR2_WRKZ:      return m_bench_algo_perf[BenchAlgo::AR2_WRKZ];
        default: return 0.0f;
    }
}

// start performance measurements for specified perf bench_algo
void Benchmark::start(const BenchAlgo bench_algo) {
    // prepare test job for benchmark runs ("benchmark" client id is to make sure we can detect benchmark jobs)
    Job& job = *m_bench_job[bench_algo];
    job.setId(Algorithm(ba2a[bench_algo]).shortName()); // need to set different id so that workers will see job change
    // 99 here to trigger all future bench_algo versions for auto veriant detection based on block version
    job.setBlob("9905A0DBD6BF05CF16E503F3A66F78007CBF34144332ECBFC22ED95C8700383B309ACE1923A0964B00000008BA939A62724C0D7581FCE5761E9D8A0E6A1C3F924FDD8493D1115649C05EB601");
    job.setTarget("FFFFFFFFFFFFFF20"); // set difficulty to 8 cause onJobResult after every 8-th computed hash
    job.setSeedHash("0000000000000000000000000000000000000000000000000000000000000001");
    m_bench_algo = bench_algo;    // current perf bench_algo
    m_hash_count = 0; // number of hashes calculated for current perf bench_algo
    m_time_start = 0; // init time of measurements start (in ms) during the first onJobResult
    m_controller->miner()->setJob(job, false); // set job for workers to compute
}

void Benchmark::onJobResult(const JobResult& result) {
    if (result.clientId != String("benchmark")) { // switch to network pool jobs
        JobResults::setListener(m_controller->network());
        static_cast<IJobResultListener*>(m_controller->network())->onJobResult(result);
        return;
    }
    // ignore benchmark results for other perf bench_algo
    if (m_bench_algo == BenchAlgo::INVALID || result.jobId != String(Algorithm(ba2a[m_bench_algo]).shortName())) return;
    ++ m_hash_count;
    const uint64_t now = get_now();
    if (!m_time_start) m_time_start = now; // time of measurements start (in ms)
    else if (now - m_time_start > static_cast<unsigned>(m_controller->config()->benchAlgoTime()*1000)) { // end of benchmark round for m_bench_algo
        const float hashrate = static_cast<float>(m_hash_count) * result.diff / (now - m_time_start) * 1000.0f;
        m_bench_algo_perf[m_bench_algo] = hashrate; // store hashrate result
        LOG_ALERT(" ===> %s hasrate: %f", Algorithm(ba2a[m_bench_algo]).shortName(), hashrate);
        const BenchAlgo next_bench_algo = static_cast<BenchAlgo>(m_bench_algo + 1); // compute next perf bench_algo to benchmark
        if (next_bench_algo != BenchAlgo::MAX) {
            start(next_bench_algo);
        } else {
            finish();
        }
    }
}

uint64_t Benchmark::get_now() const { // get current time in ms
    using namespace std::chrono;
    return time_point_cast<milliseconds>(high_resolution_clock::now()).time_since_epoch().count();
}

} // namespace xmrig