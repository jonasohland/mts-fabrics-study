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

        virtual void setup(TestContext&);
        virtual void teardown(TestContext&);
        virtual void run(TestContext&);
        virtual void onRemoteEndpointAvailable(TestContext&, std::string);

        mxlFabricsInstance _instance;
        mxlFabricsInitiator _in;
        mxlFabricsTarget _tg;
    };
}
