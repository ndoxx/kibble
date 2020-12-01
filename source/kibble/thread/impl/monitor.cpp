#include "thread/impl/monitor.h"
#include "thread/impl/worker.h"
#include "thread/job.h"

#include <fstream>

namespace kb
{
namespace th
{

Monitor::Monitor(JobSystem& js) : js_(js)
{
    (void)js_;
    wrap();
}

void Monitor::report_job_execution(const JobMetadata& meta)
{
    if(meta.label == 0)
        return;

    // Update execution time associated to this label using a moving average
    auto findit = job_size_.find(meta.label);
    if(findit == job_size_.end())
        job_size_.insert({meta.label, meta.execution_time_us});
    else
        findit->second = (findit->second + meta.execution_time_us) / 2;
}

void Monitor::wrap() { std::fill(load_.begin(), load_.end(), 0); }

void Monitor::update_statistics()
{
    WorkerActivity activity;
    while(pop_thread_activity(activity))
    {
        auto tid = activity.tid;
        stats_[tid].active_time_ms += double(activity.active_time_us) / 1000.0;
        stats_[tid].idle_time_ms += double(activity.idle_time_us) / 1000.0;
        stats_[tid].total_executed += activity.executed;
        stats_[tid].total_stolen += activity.stolen;
        stats_[tid].total_rescheduled += activity.rescheduled;
        ++stats_[tid].cycles;
    }
}

void Monitor::log_statistics(tid_t tid) const
{
    K_ASSERT(tid < js_.get_threads_count(), "Worker TID out of range.");

    const auto& stats = get_statistics(tid);
    double mean_active_ms = stats.active_time_ms / double(stats.cycles);
    double mean_idle_ms = stats.idle_time_ms / double(stats.cycles);
    double mean_activity = 100.0 * mean_active_ms / (mean_idle_ms + mean_active_ms);
    float jobs_per_cycle = float(stats.total_executed) / float(stats.cycles);

    KLOG("thread", 1) << WCC('i') << "Thread #" << tid << std::endl;
    KLOGI << "Sleep cycles:         " << stats.cycles << std::endl;
    KLOGI << "Mean active time:     " << mean_active_ms << "ms" << std::endl;
    KLOGI << "Mean idle time:       " << mean_idle_ms << "ms" << std::endl;
    KLOGI << "Mean activity ratio:  " << mean_activity << '%' << std::endl;
    KLOGI << "Total executed:       " << stats.total_executed << " job" << ((stats.total_executed > 1) ? "s" : "") << std::endl;
    KLOGI << "Total stolen:         " << stats.total_stolen << " job" << ((stats.total_stolen > 1) ? "s" : "") << std::endl;
    KLOGI << "Total rescheduled:    " << stats.total_rescheduled << " job" << ((stats.total_rescheduled > 1) ? "s" : "") << std::endl;
    KLOGI << "Average jobs / cycle: " << jobs_per_cycle << " job" << ((jobs_per_cycle > 1.f) ? "s" : "") << std::endl;
}

// Header for Job Profile Persistence files
struct JPPHeader
{
    uint32_t magic;         // Magic number to check file format validity
    uint16_t version_major; // Version major number
    uint16_t version_minor; // Version minor number
    uint64_t label_count;   // Number of job labels in this file
};

#define JPP_MAGIC 0x4650504a // ASCII(JPPF)
#define JPP_VERSION_MAJOR 1
#define JPP_VERSION_MINOR 0

void Monitor::export_job_profiles(const fs::path& filepath)
{
	KLOGN("thread") << "[Monitor] Exporting persistence file:" << std::endl;
	KLOGI << WCC('p') << filepath << std::endl;

	JPPHeader header;
	header.magic = JPP_MAGIC;
	header.version_major = JPP_VERSION_MAJOR;
	header.version_minor = JPP_VERSION_MINOR;
	header.label_count = job_size_.size();

    auto ofs = std::ofstream(filepath, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(JPPHeader));
    for(auto&& [label, size]: job_size_)
    {
    	ofs.write(reinterpret_cast<const char*>(&label), sizeof(uint64_t));
    	ofs.write(reinterpret_cast<const char*>(&size), sizeof(int64_t));
    }
    ofs.close();
}

void Monitor::load_job_profiles(const fs::path& filepath)
{
	if(!fs::exists(filepath))
	{
		KLOGW("thread") << "[Monitor] File does not exist:" << std::endl;
		KLOGI << WCC('p') << filepath << std::endl;
		KLOGI << "Skipping persistence file loading." << std::endl;
		return;
	}

	KLOGN("thread") << "[Monitor] Loading persistence file:" << std::endl;
	KLOGI << WCC('p') << filepath << std::endl;
    auto ifs = std::ifstream(filepath, std::ios::binary);

    // Read header & sanity check
	JPPHeader header;
    ifs.read(reinterpret_cast<char*>(&header), sizeof(JPPHeader));
    K_ASSERT(header.magic == JPP_MAGIC, "Invalid JPP file: magic number mismatch.");
    K_ASSERT(header.version_major == JPP_VERSION_MAJOR, "Invalid JPP file: version (major) mismatch.");
    K_ASSERT(header.version_minor == JPP_VERSION_MINOR, "Invalid JPP file: version (minor) mismatch.");

    for(size_t ii=0; ii<header.label_count; ++ii)
    {
    	uint64_t label = 0;
    	int64_t size = 0;
    	ifs.read(reinterpret_cast<char*>(&label), sizeof(uint64_t));
    	ifs.read(reinterpret_cast<char*>(&size), sizeof(int64_t));
    	job_size_.insert({label, size});
    }
    ifs.close();
}

} // namespace th
} // namespace kb