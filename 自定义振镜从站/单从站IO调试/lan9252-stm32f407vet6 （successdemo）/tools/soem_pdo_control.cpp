#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

extern "C" {
#include "soem/soem.h"
}

#ifdef inline
#undef inline
#endif

namespace {

constexpr const char *kDefaultIfname = "\\Device\\NPF_{628E019F-362D-4B55-AFCE-CC522F65376F}";

uint8 IOmap[4096];
ecx_contextt ctx;

void print_state()
{
    ecx_readstate(&ctx);
    for (int i = 1; i <= ctx.slavecount; ++i) {
        ec_slavet *slave = &ctx.slavelist[i];
        std::printf("slave %d state=0x%04x AL=0x%04x %s\n",
                    i,
                    slave->state,
                    slave->ALstatuscode,
                    ec_ALstatuscode2string(slave->ALstatuscode));
    }
}

bool start_ethercat(const char *ifname)
{
    std::memset(&ctx, 0, sizeof(ctx));

    std::printf("SOEM PDO control on %s\n", ifname);
    if (!ecx_init(&ctx, ifname)) {
        std::printf("ecx_init failed, check adapter name and administrator permission\n");
        return false;
    }

    if (ecx_config_init(&ctx) <= 0) {
        std::printf("no EtherCAT slaves found\n");
        ecx_close(&ctx);
        return false;
    }

    std::printf("%d slave(s) found\n", ctx.slavecount);
    for (int i = 1; i <= ctx.slavecount; ++i) {
        ec_slavet *slave = &ctx.slavelist[i];
        std::printf("slave %d: %s, Obits=%d Ibits=%d, Obytes=%d Ibytes=%d\n",
                    i,
                    slave->name,
                    slave->Obits,
                    slave->Ibits,
                    slave->Obytes,
                    slave->Ibytes);
    }

    ecx_config_map_group(&ctx, IOmap, 0);
    ecx_configdc(&ctx);

    const int expected_wkc = (ctx.grouplist[0].outputsWKC * 2) + ctx.grouplist[0].inputsWKC;
    std::printf("mapped Obytes=%d Ibytes=%d expectedWKC=%d\n",
                ctx.grouplist[0].Obytes,
                ctx.grouplist[0].Ibytes,
                expected_wkc);

    ecx_statecheck(&ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);

    ctx.slavelist[0].state = EC_STATE_OPERATIONAL;
    ecx_send_processdata(&ctx);
    ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
    ecx_writestate(&ctx, 0);

    for (int i = 0; i < 50; ++i) {
        ecx_send_processdata(&ctx);
        ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
        ecx_statecheck(&ctx, 0, EC_STATE_OPERATIONAL, 50000);
        if (ctx.slavelist[0].state == EC_STATE_OPERATIONAL) {
            std::printf("OP reached\n");
            return true;
        }
    }

    std::printf("failed to enter OP\n");
    print_state();
    ecx_close(&ctx);
    return false;
}

void stop_ethercat()
{
    if (ctx.slavecount > 0) {
        std::printf("request INIT\n");
        ctx.slavelist[0].state = EC_STATE_INIT;
        ecx_writestate(&ctx, 0);
    }
    ecx_close(&ctx);
}

uint8_t parse_output(const std::string &cmd, uint8_t current)
{
    if (cmd == "0" || cmd == "off") {
        return 0x00;
    }
    if (cmd == "1" || cmd == "led1") {
        return 0x01;
    }
    if (cmd == "2" || cmd == "led2") {
        return 0x02;
    }
    if (cmd == "3" || cmd == "both") {
        return 0x03;
    }
    if (cmd == "t1") {
        return current ^ 0x01;
    }
    if (cmd == "t2") {
        return current ^ 0x02;
    }
    return current;
}

bool exchange_pdo(uint8_t output)
{
    if (ctx.grouplist[0].outputs == nullptr || ctx.grouplist[0].Obytes <= 0) {
        std::printf("no PDO outputs mapped\n");
        return false;
    }

    ctx.grouplist[0].outputs[0] = output;

    ecx_send_processdata(&ctx);
    const int wkc = ecx_receive_processdata(&ctx, EC_TIMEOUTRET);

    uint8_t input = 0;
    if (ctx.grouplist[0].inputs != nullptr && ctx.grouplist[0].Ibytes > 0) {
        input = ctx.grouplist[0].inputs[0];
    }

    std::printf("WKC=%d OUT=0x%02X led1=%u led2=%u  IN=0x%02X switch1=%u switch2=%u\n",
                wkc,
                output,
                output & 0x01,
                (output >> 1) & 0x01,
                input,
                input & 0x01,
                (input >> 1) & 0x01);

    return true;
}

void print_help()
{
    std::cout
        << "Commands:\n"
        << "  0/off   turn off led1 and led2\n"
        << "  1/led1  turn on led1 only\n"
        << "  2/led2  turn on led2 only\n"
        << "  3/both  turn on led1 and led2\n"
        << "  t1      toggle led1\n"
        << "  t2      toggle led2\n"
        << "  r       read current PDO once\n"
        << "  q       quit\n";
}

} // namespace

int main(int argc, char *argv[])
{
    const char *ifname = (argc >= 2) ? argv[1] : kDefaultIfname;
    uint8_t output = 0x00;

    if (!start_ethercat(ifname)) {
        return 1;
    }

    print_help();
    exchange_pdo(output);

    while (true) {
        std::cout << "pdo> ";
        std::string cmd;
        if (!(std::cin >> cmd)) {
            break;
        }

        if (cmd == "q" || cmd == "quit" || cmd == "exit") {
            break;
        }
        if (cmd == "h" || cmd == "help" || cmd == "?") {
            print_help();
            continue;
        }
        if (cmd != "r" && cmd != "read") {
            output = parse_output(cmd, output);
        }

        exchange_pdo(output);
    }

    output = 0x00;
    exchange_pdo(output);
    stop_ethercat();
    return 0;
}
