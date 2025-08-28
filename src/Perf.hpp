#pragma once

#include <cstdint>
#include <list>
#include <string>
#include <vector>
#include <linux/perf_event.h>

namespace riedel::fabricsperf
{
    class PerfRecorder
    {
    public:
        class Event
        {
        public:
            Event(std::string name, int fd);
            ~Event();

            Event(Event const&) = delete;
            Event& operator=(Event const&) = delete;
            Event(Event&&) = delete;
            Event& operator=(Event&&) = delete;

            uint64_t get() const;
            std::string name() const noexcept;

            std::string _name;
            int _fd;
        };

        enum Filter : int
        {
            None = 0,
            User = 1 << 0,
            Kernel = 1 << 1,
            Idle = 1 << 2,
        };

        PerfRecorder();
        ~PerfRecorder();

        void start();
        void stop();

        void addEvent(std::string name, int type, int config, Filter filer);

        std::vector<std::pair<std::string, std::string>> exportCounters();

    private:
        std::list<Event> _events;
        int _groupFd;
    };
}
