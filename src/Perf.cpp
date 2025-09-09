#include "Perf.hpp"
#include <chrono>
#include <cstdint>
#include <string>
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <fmt/format.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <system_error>
#include "internal/Logging.hpp"

namespace riedel::fabricsperf
{
    ::perf_event_attr nullPerfEventAttr()
    {
        ::perf_event_attr attr{};
        ::memset(&attr, 0, sizeof(::perf_event_attr));
        attr.size = sizeof(::perf_event_attr);

        return attr;
    }

    int perfEventOpen(::perf_event_attr* attr, ::pid_t pid, int cpu, int group_fd,
        unsigned long flags)
    {
        int fd = ::syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
        if (fd == -1)
        {
            throw std::system_error(errno, std::generic_category(), "open perf event");
        }

        if (::ioctl(fd, PERF_EVENT_IOC_RESET) == -1)
        {
            throw std::system_error(errno, std::generic_category(), "reset perf event");
        }

        return fd;
    }

    int perfEventGroup()
    {
        auto attr = nullPerfEventAttr();
        attr.type = PERF_TYPE_SOFTWARE;
        attr.config = PERF_COUNT_SW_DUMMY;
        attr.disabled = 1;
        return perfEventOpen(&attr, 0, -1, -1, 0);
    }

    PerfRecorder::Event::Event(std::string name, int fd)
        : _name(std::move(name))
        , _fd(fd)
    {}

    PerfRecorder::Event::~Event()
    {
        if (::close(_fd) == -1)
        {
            MXL_ERROR("failed to close perf event [{}]: {}", _name, ::strerror(errno));
        }
    }

    uint64_t PerfRecorder::Event::get() const
    {
        uint64_t out;

        if (auto size = ::read(_fd, &out, sizeof(uint64_t)); size != sizeof(uint64_t))

        {
            throw std::system_error(errno, std::generic_category(), "read perf event");
        }

        return out;
    }

    std::string PerfRecorder::Event::name() const noexcept
    {
        return _name;
    }

    PerfRecorder::PerfRecorder()
        : _groupFd(perfEventGroup())
    {}

    void PerfRecorder::start()
    {
        _startTime = std::chrono::steady_clock::now();
        if (auto status = ::ioctl(_groupFd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
            status == -1)
        {
            throw std::system_error(errno, std::generic_category(), "enable perf event group");
        }
        if (auto status = ::ioctl(_groupFd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
            status == -1)
        {
            throw std::system_error(errno, std::generic_category(), "enable perf event group");
        }
    }

    void PerfRecorder::stop()
    {
        if (auto status = ::ioctl(_groupFd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
            status == -1)
        {
            throw std::system_error(errno, std::generic_category(), "disable perf event group");
        }
        _stopTime = std::chrono::steady_clock::now();
    }

    void PerfRecorder::addEvent(std::string name, int type, int config, Filter filter)
    {
        auto attr = nullPerfEventAttr();
        attr.type = type;
        attr.config = config;
        attr.disabled = 1;
        attr.exclude_hv = 1;

        if (filter != Filter::None)
        {
            attr.exclude_user = 1;
            attr.exclude_kernel = 1;
            attr.exclude_idle = 1;

            if (filter & Filter::User)
            {
                attr.exclude_user = 0;
            }
            if (filter & Filter::Kernel)
            {
                attr.exclude_kernel = 0;
            }
            if (filter & Filter::Idle)
            {
                attr.exclude_idle = 0;
            }
        }

        int fd = perfEventOpen(&attr, 0, -1, _groupFd, 0);
        _events.emplace_back(std::move(name), fd);
    }

    std::vector<std::pair<std::string, std::string>> PerfRecorder::exportCounters()
    {
        std::vector<std::pair<std::string, std::string>> out;
        for (auto const& event : _events)
        {
            out.emplace_back(event.name(), std::to_string(event.get()));
        }

        out.emplace_back("time_elapsed",
            std::to_string(
                std::chrono::duration_cast<std::chrono::nanoseconds>(_stopTime - _startTime)
                    .count()));

        return out;
    }

    PerfRecorder::~PerfRecorder()
    {
        _events.clear();
        ::close(_groupFd);
    }

}
