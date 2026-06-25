#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include "soem/soem.h"
}

#ifdef inline
#undef inline
#endif

namespace {

constexpr const char *kDefaultIfname = "\\Device\\NPF_{628E019F-362D-4B55-AFCE-CC522F65376F}";
constexpr const char *kDefaultCsv = "position_velocity_joint_mpc_split_samples.csv";
constexpr double kDefaultTs = 0.001;
constexpr double kDefaultFieldMm = 11.0;
constexpr uint16_t kControlEnable = 0x0001;

#pragma pack(push, 1)
struct GalvoRxPdo {
    int16_t x_code;
    int16_t y_code;
    uint16_t sequence;
    uint16_t control_flags;
};

struct GalvoTxPdo {
    uint16_t status;
    uint16_t last_sequence;
    uint16_t frame_counter;
};
#pragma pack(pop)

struct Sample {
    double t;
    double galvo_x;
    double galvo_y;
    bool laser_enable;
};

struct Options {
    std::string ifname = kDefaultIfname;
    std::string csv_path = kDefaultCsv;
    double cycle_s = kDefaultTs;
    double field_mm = kDefaultFieldMm;
    double x_offset_mm = 0.0;
    double y_offset_mm = 0.0;
    bool repeat = false;
    bool dry_run = false;
};

uint8 IOmap[4096];
ecx_contextt ctx;

std::vector<std::string> split_csv_line(const std::string &line)
{
    std::vector<std::string> out;
    std::string cell;
    std::stringstream ss(line);
    while (std::getline(ss, cell, ',')) {
        out.push_back(cell);
    }
    return out;
}

int find_column(const std::vector<std::string> &header, const std::string &name)
{
    for (size_t i = 0; i < header.size(); ++i) {
        if (header[i] == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

double parse_double_cell(const std::vector<std::string> &row, int idx)
{
    if (idx < 0 || static_cast<size_t>(idx) >= row.size()) {
        return 0.0;
    }
    return std::strtod(row[static_cast<size_t>(idx)].c_str(), nullptr);
}

std::vector<Sample> load_samples(const std::string &path)
{
    std::vector<std::string> candidates = {
        path,
        "..\\" + path,
        "..\\..\\" + path,
        "..\\..\\..\\" + path,
        "..\\..\\..\\..\\" + path,
        "..\\..\\..\\..\\..\\" + path,
    };

    std::ifstream file;
    std::string opened_path;
    for (const auto &candidate : candidates) {
        file.open(candidate);
        if (file) {
            opened_path = candidate;
            break;
        }
        file.clear();
    }

    if (!file) {
        throw std::runtime_error("failed to open CSV: " + path);
    }

    std::string line;
    if (!std::getline(file, line)) {
        throw std::runtime_error("empty CSV: " + opened_path);
    }

    const auto header = split_csv_line(line);
    const int t_col = find_column(header, "t");
    const int x_col = find_column(header, "galvo_x");
    const int y_col = find_column(header, "galvo_y");
    const int laser_col = find_column(header, "laser_enable");
    if (t_col < 0 || x_col < 0 || y_col < 0) {
        throw std::runtime_error("CSV must contain t, galvo_x and galvo_y columns");
    }

    std::vector<Sample> samples;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        const auto row = split_csv_line(line);
        samples.push_back({
            parse_double_cell(row, t_col),
            parse_double_cell(row, x_col),
            parse_double_cell(row, y_col),
            laser_col >= 0 && parse_double_cell(row, laser_col) > 0.5,
        });
    }

    if (samples.empty()) {
        throw std::runtime_error("CSV contains no samples");
    }
    std::printf("CSV path resolved to %s\n", opened_path.c_str());
    return samples;
}

int16_t mm_to_xy2_code(double mm, double field_mm)
{
    const double clamped = std::clamp(mm / field_mm, -1.0, 1.0);
    const double scaled = std::round(clamped * 32767.0);
    return static_cast<int16_t>(std::clamp(scaled, -32768.0, 32767.0));
}

void print_usage(const char *exe)
{
    std::cout
        << "Usage: " << exe << " [options]\n"
        << "  --ifname <adapter>       Npcap adapter name\n"
        << "  --csv <path>             trajectory CSV, default " << kDefaultCsv << "\n"
        << "  --cycle-ms <ms>          EtherCAT cycle time, default 1.0\n"
        << "  --field-mm <mm>          +/- full scale galvo field, default 11.0\n"
        << "  --x-offset-mm <mm>       galvo_x offset before scaling\n"
        << "  --y-offset-mm <mm>       galvo_y offset before scaling\n"
        << "  --repeat                 loop the CSV forever\n"
        << "  --dry-run                parse and print samples without EtherCAT\n";
}

Options parse_args(int argc, char *argv[])
{
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need_value = [&](const char *name) -> const char * {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++i];
        };

        if (arg == "--ifname") {
            opt.ifname = need_value("--ifname");
        } else if (arg == "--csv") {
            opt.csv_path = need_value("--csv");
        } else if (arg == "--cycle-ms") {
            opt.cycle_s = std::strtod(need_value("--cycle-ms"), nullptr) / 1000.0;
        } else if (arg == "--field-mm") {
            opt.field_mm = std::strtod(need_value("--field-mm"), nullptr);
        } else if (arg == "--x-offset-mm") {
            opt.x_offset_mm = std::strtod(need_value("--x-offset-mm"), nullptr);
        } else if (arg == "--y-offset-mm") {
            opt.y_offset_mm = std::strtod(need_value("--y-offset-mm"), nullptr);
        } else if (arg == "--repeat") {
            opt.repeat = true;
        } else if (arg == "--dry-run") {
            opt.dry_run = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg.rfind("--", 0) == 0) {
            throw std::runtime_error("unknown option: " + arg);
        } else {
            opt.ifname = arg;
        }
    }

    if (opt.cycle_s <= 0.0 || opt.field_mm <= 0.0) {
        throw std::runtime_error("cycle and field must be positive");
    }
    return opt;
}

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

    std::printf("SOEM galvo PDO control on %s\n", ifname);
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

    if (ctx.grouplist[0].Obytes < static_cast<int>(sizeof(GalvoRxPdo))) {
        std::printf("RxPDO is too small: need %zu bytes\n", sizeof(GalvoRxPdo));
        ecx_close(&ctx);
        return false;
    }

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
    if (ctx.grouplist[0].outputs != nullptr && ctx.grouplist[0].Obytes >= static_cast<int>(sizeof(GalvoRxPdo))) {
        GalvoRxPdo stop{};
        std::memcpy(ctx.grouplist[0].outputs, &stop, sizeof(stop));
        ecx_send_processdata(&ctx);
        ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
    }

    if (ctx.slavecount > 0) {
        std::printf("request INIT\n");
        ctx.slavelist[0].state = EC_STATE_INIT;
        ecx_writestate(&ctx, 0);
    }
    ecx_close(&ctx);
}

void exchange_pdo(const GalvoRxPdo &rx, GalvoTxPdo *tx, int *wkc)
{
    std::memcpy(ctx.grouplist[0].outputs, &rx, sizeof(rx));
    ecx_send_processdata(&ctx);
    *wkc = ecx_receive_processdata(&ctx, EC_TIMEOUTRET);

    if (tx != nullptr) {
        std::memset(tx, 0, sizeof(*tx));
        if (ctx.grouplist[0].inputs != nullptr && ctx.grouplist[0].Ibytes >= static_cast<int>(sizeof(*tx))) {
            std::memcpy(tx, ctx.grouplist[0].inputs, sizeof(*tx));
        }
    }
}

void run_dry(const std::vector<Sample> &samples, const Options &opt)
{
    const size_t n = std::min<size_t>(samples.size(), 10);
    for (size_t i = 0; i < n; ++i) {
        const auto &s = samples[i];
        std::printf("%zu t=%.6f galvo=(%.6f, %.6f) code=(%d, %d) laser=%u\n",
                    i,
                    s.t,
                    s.galvo_x,
                    s.galvo_y,
                    mm_to_xy2_code(s.galvo_x + opt.x_offset_mm, opt.field_mm),
                    mm_to_xy2_code(s.galvo_y + opt.y_offset_mm, opt.field_mm),
                    s.laser_enable ? 1u : 0u);
    }
}

void run_trajectory(const std::vector<Sample> &samples, const Options &opt)
{
    using clock = std::chrono::steady_clock;
    const auto cycle = std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(opt.cycle_s));
    uint16_t sequence = 0;

    do {
        const auto start = clock::now();
        for (size_t i = 0; i < samples.size(); ++i) {
            const auto &s = samples[i];
            GalvoRxPdo rx{};
            rx.x_code = mm_to_xy2_code(s.galvo_x + opt.x_offset_mm, opt.field_mm);
            rx.y_code = mm_to_xy2_code(s.galvo_y + opt.y_offset_mm, opt.field_mm);
            rx.sequence = sequence++;
            rx.control_flags = s.laser_enable ? kControlEnable : 0u;

            GalvoTxPdo tx{};
            int wkc = 0;
            exchange_pdo(rx, &tx, &wkc);

            if ((i % 1000u) == 0u) {
                std::printf("i=%zu seq=%u code=(%d,%d) WKC=%d status=0x%04X ack=%u frames=%u\n",
                            i,
                            rx.sequence,
                            rx.x_code,
                            rx.y_code,
                            wkc,
                            tx.status,
                            tx.last_sequence,
                            tx.frame_counter);
            }

            std::this_thread::sleep_until(start + cycle * (i + 1));
        }
    } while (opt.repeat);
}

} // namespace

int main(int argc, char *argv[])
{
    try {
        const Options opt = parse_args(argc, argv);
        const auto samples = load_samples(opt.csv_path);
        std::printf("loaded %zu samples from %s\n", samples.size(), opt.csv_path.c_str());

        if (opt.dry_run) {
            run_dry(samples, opt);
            return 0;
        }

        if (!start_ethercat(opt.ifname.c_str())) {
            return 1;
        }

        run_trajectory(samples, opt);
        stop_ethercat();
        return 0;
    } catch (const std::exception &exc) {
        std::fprintf(stderr, "error: %s\n", exc.what());
        print_usage(argv[0]);
        return 1;
    }
}
