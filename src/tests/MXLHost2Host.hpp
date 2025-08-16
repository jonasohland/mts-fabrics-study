#include <optional>
#include <mxl/fabrics.h>
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

        void setup(TestContext&) override;
        void teardown(TestContext&) override;
        void run(TestContext&) override;
        void onRemoteEndpointAvailable(TestContext&, std::string) override;

    private:
        void runner(TestContext& ctx);
        void reflector(TestContext& ctx);

        mxlFabricsInstance _instance;
        mxlFabricsInitiator _in;
        mxlFabricsTarget _tg;
        std::optional<std::string> _remoteEndpointInfo;
    };
}
