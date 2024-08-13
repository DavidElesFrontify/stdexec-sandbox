#include "LibuvFakeServer.hpp"
#include "LibuvThreadPool.hpp"

#include "Context.hpp"

#include <uv.h>

namespace
{
    struct Globals
    {
        static Globals& instance()
        {
            static Globals instance;
            return instance;
        }
        Globals()
            : context(uv_default_loop())
            {}

        Context<LibuvThreadPool> context;
        uv_loop_t* main_loop{nullptr};
    };
    void onIdle(uv_idle_t *handle)
    {
        static int64_t call_count = 0;

        if(call_count < 1)
        {
            std::cout << "Spawn worker " << call_count << std::endl;
            Globals::instance().context.spawn2(Input{"Input -" + std::to_string(call_count)}, Output{}, 
            [](){std::cout << "Processing finished " << call_count << std::endl;});
        }
        else
        {
            uv_stop(Globals::instance().main_loop);
        }
        call_count++;
    }
}

void runLibUvServer()
{
    Globals::instance().main_loop = uv_default_loop();
    auto* main_loop = Globals::instance().main_loop;
    uv_idle_t idler;
    uv_idle_init(main_loop, &idler);
    uv_idle_start(&idler, onIdle);

    uv_run(main_loop, UV_RUN_DEFAULT);
}