#include "soem/soem.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8 IOmap[4096];
static ecx_contextt ctx;

static void print_state(void)
{
    int i;

    ecx_readstate(&ctx);
    for (i = 1; i <= ctx.slavecount; i++) {
        ec_slavet *slave = &ctx.slavelist[i];
        printf("slave %d state=0x%04x AL=0x%04x %s\n",
               i,
               slave->state,
               slave->ALstatuscode,
               ec_ALstatuscode2string(slave->ALstatuscode));
    }
}

int main(int argc, char *argv[])
{
    const char *ifname;
    int expected_wkc;
    int wkc;
    int i;

    if (argc < 2) {
        printf("Usage: %s <pcap-ifname> [cycles]\n", argv[0]);
        return 1;
    }

    ifname = argv[1];
    int cycles = (argc >= 3) ? atoi(argv[2]) : 40;
    if (cycles <= 0) {
        cycles = 40;
    }

    printf("SOEM PDO test on %s\n", ifname);
    memset(&ctx, 0, sizeof(ctx));

    if (!ecx_init(&ctx, ifname)) {
        printf("ec_init failed\n");
        return 2;
    }

    if (ecx_config_init(&ctx) <= 0) {
        printf("no EtherCAT slaves found\n");
        ecx_close(&ctx);
        return 3;
    }

    printf("%d slave(s) found\n", ctx.slavecount);
    for (i = 1; i <= ctx.slavecount; i++) {
        ec_slavet *slave = &ctx.slavelist[i];
        printf("slave %d: %s, Obits=%d Ibits=%d, Obytes=%d Ibytes=%d\n",
               i,
               slave->name,
               slave->Obits,
               slave->Ibits,
               slave->Obytes,
               slave->Ibytes);
    }

    ecx_config_map_group(&ctx, IOmap, 0);
    ecx_configdc(&ctx);

    expected_wkc = (ctx.grouplist[0].outputsWKC * 2) + ctx.grouplist[0].inputsWKC;
    printf("mapped Obytes=%d Ibytes=%d expectedWKC=%d\n",
           ctx.grouplist[0].Obytes,
           ctx.grouplist[0].Ibytes,
           expected_wkc);

    ecx_statecheck(&ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);

    ctx.slavelist[0].state = EC_STATE_OPERATIONAL;
    ecx_send_processdata(&ctx);
    ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
    ecx_writestate(&ctx, 0);

    for (i = 0; i < 50; i++) {
        ecx_send_processdata(&ctx);
        ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
        ecx_statecheck(&ctx, 0, EC_STATE_OPERATIONAL, 50000);
        if (ctx.slavelist[0].state == EC_STATE_OPERATIONAL) {
            break;
        }
    }

    if (ctx.slavelist[0].state != EC_STATE_OPERATIONAL) {
        printf("failed to enter OP\n");
        print_state();
        ctx.slavelist[0].state = EC_STATE_INIT;
        ecx_writestate(&ctx, 0);
        ecx_close(&ctx);
        return 4;
    }

    printf("OP reached, cycling PDO\n");

    for (i = 0; i < cycles; i++) {
        uint8_t out = (uint8_t)(i & 0x03);
        uint8_t in = 0;

        if (ctx.grouplist[0].outputs != NULL && ctx.grouplist[0].Obytes > 0) {
            ctx.grouplist[0].outputs[0] = out;
        }

        ecx_send_processdata(&ctx);
        wkc = ecx_receive_processdata(&ctx, EC_TIMEOUTRET);

        if (ctx.grouplist[0].inputs != NULL && ctx.grouplist[0].Ibytes > 0) {
            in = ctx.grouplist[0].inputs[0];
        }

        printf("cycle=%03d WKC=%d OUT=0x%02x led1=%u led2=%u IN=0x%02x switch1=%u switch2=%u\n",
               i,
               wkc,
               out,
               out & 0x01,
               (out >> 1) & 0x01,
               in,
               in & 0x01,
               (in >> 1) & 0x01);

        osal_usleep(100000);
    }

    printf("request INIT\n");
    ctx.slavelist[0].state = EC_STATE_INIT;
    ecx_writestate(&ctx, 0);
    ecx_close(&ctx);

    return 0;
}
