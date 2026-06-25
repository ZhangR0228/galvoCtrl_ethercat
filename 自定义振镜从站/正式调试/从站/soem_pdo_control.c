#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "soem/soem.h"

#define EC_TIMEOUTMON 500

static char IOmap[4096];

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

int main(int argc, char *argv[])
{
    const char *ifname;
    int x;
    int y;
    uint16_t flags;
    int cycles;
    int i;
    int expected_wkc;
    int wkc;

    if (argc < 2) {
        printf("Usage: %s ifname [x] [y] [flags] [cycles]\n", argv[0]);
        printf("Example: %s \"\\\\Device\\\\NPF_{...}\" 100 -100 1 100\n", argv[0]);
        return 1;
    }

    ifname = argv[1];
    x = (argc > 2) ? atoi(argv[2]) : 0;
    y = (argc > 3) ? atoi(argv[3]) : 0;
    flags = (uint16_t)((argc > 4) ? strtoul(argv[4], NULL, 0) : 1);
    cycles = (argc > 5) ? atoi(argv[5]) : 100;
    if (cycles <= 0) {
        cycles = 1;
    }

    printf("SOEM PDO control\n");
    printf("ifname=%s x=%d y=%d flags=0x%04X cycles=%d\n", ifname, x, y, flags, cycles);

    if (!ec_init(ifname)) {
        printf("ec_init failed on %s\n", ifname);
        return 1;
    }

    if (ec_config_init(FALSE) <= 0) {
        printf("No slaves found\n");
        ec_close();
        return 1;
    }

    printf("%d slave(s) found\n", ec_slavecount);
    ec_config_map(&IOmap);
    ec_configdc();

    expected_wkc = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
    printf("expected WKC=%d outputs=%d bits inputs=%d bits\n",
           expected_wkc, ec_slave[1].Obits, ec_slave[1].Ibits);

    ec_slave[0].state = EC_STATE_SAFE_OP;
    ec_writestate(0);
    ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);

    ec_slave[0].state = EC_STATE_OPERATIONAL;
    ec_send_processdata();
    ec_receive_processdata(EC_TIMEOUTRET);
    ec_writestate(0);

    for (i = 0; i < 40; i++) {
        ec_send_processdata();
        ec_receive_processdata(EC_TIMEOUTRET);
        ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
        if (ec_slave[0].state == EC_STATE_OPERATIONAL) {
            break;
        }
    }

    if (ec_slave[0].state != EC_STATE_OPERATIONAL) {
        printf("Failed to enter OP, state=0x%02X\n", ec_slave[0].state);
        ec_readstate();
        for (i = 1; i <= ec_slavecount; i++) {
            printf("slave %d state=0x%02X ALstatus=0x%04X %s\n",
                   i, ec_slave[i].state, ec_slave[i].ALstatuscode,
                   ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
        }
        ec_close();
        return 1;
    }

    printf("Operational\n");

    for (i = 0; i < cycles; i++) {
        uint8_t *out = ec_slave[1].outputs;
        const uint8_t *in = ec_slave[1].inputs;
        uint16_t seq = (uint16_t)(i + 1);

        if (out != NULL) {
            put_u16(out + 0, (uint16_t)(int16_t)x);
            put_u16(out + 2, (uint16_t)(int16_t)y);
            put_u16(out + 4, seq);
            put_u16(out + 6, flags);
        }

        ec_send_processdata();
        wkc = ec_receive_processdata(EC_TIMEOUTRET);

        if ((i == 0) || (i == cycles - 1) || ((i + 1) % 20 == 0)) {
            uint16_t status = 0;
            uint16_t last_seq = 0;
            uint16_t frame_counter = 0;
            if (in != NULL) {
                status = get_u16(in + 0);
                last_seq = get_u16(in + 2);
                frame_counter = get_u16(in + 4);
            }
            printf("cycle=%d wkc=%d out{x=%d y=%d seq=%u flags=0x%04X} "
                   "in{status=0x%04X last_seq=%u frame=%u}\n",
                   i + 1, wkc, x, y, seq, flags, status, last_seq, frame_counter);
        }

        osal_usleep(1000);
    }

    ec_slave[0].state = EC_STATE_SAFE_OP;
    ec_writestate(0);
    ec_close();
    return 0;
}
