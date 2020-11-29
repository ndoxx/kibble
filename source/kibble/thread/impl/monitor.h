#pragma once

#include <map>
#include <vector>

namespace kb
{

class JobSystem;
struct JobMetadata;
class Monitor
{
public:
    Monitor(JobSystem& js);
    void report_job_execution(const JobMetadata&);
    void wrap();

    inline const auto& get_job_size() const { return job_size_; }
    inline const auto& get_load() const { return load_; }
    inline void add_load(size_t idx, int64_t job_size) { load_[idx] += job_size; }

private:
    std::map<uint64_t, int64_t> job_size_;
    std::vector<int64_t> load_;
    JobSystem& js_;
};

} // namespace kb