#include "../Test.hpp"

namespace riedel::fabricsperf
{
    class MXLHost2Host;

    class MXLHost2HostFactory : public TestFactory
    {
    public:
        std::unique_ptr<Test> operator()() const final;
        std::string name() const final;
    };

    class MXLHost2Host : public Test
    {
    public:
        using Factory = MXLHost2HostFactory;

        virtual void setup(mxlFlowReader* reader, mxlFlowWriter* writer);
        virtual void teardown();
        virtual void run(TestContext& ctx);
        virtual void onRemoteEndpointAvailable(std::string info);
    };
}
