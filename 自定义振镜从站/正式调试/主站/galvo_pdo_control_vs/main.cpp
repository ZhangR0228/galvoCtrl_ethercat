#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <mmsystem.h>

extern "C" {
#include "soem/soem.h"
}

namespace {

constexpr const char *kDefaultIfname = "\\Device\\NPF_{628E019F-362D-4B55-AFCE-CC522F65376F}";
constexpr const char *kDefaultCsv = "position_velocity_joint_mpc_split_samples.csv";
constexpr double kDefaultCycleMs = 1.0;
constexpr double kDefaultFieldMm = 11.0;
constexpr int kDefaultTimeoutUs = 10000;
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
    std::filesystem::path csv_path = kDefaultCsv;
    double cycle_ms = kDefaultCycleMs;
    double field_mm = kDefaultFieldMm;
    double x_offset_mm = 0.0;
    double y_offset_mm = 0.0;
    int timeout_us = kDefaultTimeoutUs;
    int max_lost_frames = 20;
    bool repeat = false;
    bool dry_run = false;
    bool list_adapters = false;
};

uint8 IOmap[4096];
ecx_contextt ctx;
int expected_wkc = 0;

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

int find_column(const std::vector<std::string> &header, const char *name)
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

std::vector<std::filesystem::path> csv_candidates(const std::filesystem::path &path)
{
    if (path.has_parent_path()) {
        return {path};
    }

    return {
        path,
        std::filesystem::path(L"from_station") / path,
        std::filesystem::path(L"\x4ECE\x7AD9") / path,
        std::filesystem::path(L"..") / L"\x4ECE\x7AD9" / path,
        std::filesystem::path(L"..") / L".." / L"\x4ECE\x7AD9" / path,
        std::filesystem::path(L"..") / L".." / L".." / L"\x4ECE\x7AD9" / path,
        std::filesystem::path(L"..") / L".." / L".." / L".." / L"\x4ECE\x7AD9" / path,
    };
}

std::vector<Sample> load_samples(const std::filesystem::path &path, std::filesystem::path *opened_path)
{
    std::ifstream file;
    for (const auto &candidate : csv_candidates(path)) {
        file.open(candidate);
        if (file) {
            if (opened_path != nullptr) {
                *opened_path = candidate;
            }
            break;
        }
        file.clear();
    }

    if (!file) {
        throw std::runtime_error("failed to open CSV");
    }

    std::string line;
    if (!std::getline(file, line)) {
        throw std::runtime_error("empty CSV");
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
        << "  --list-adapters         print Npcap adapter names and exit\n"
        << "  --ifname <adapter>      Npcap adapter name\n"
        << "  --csv <path>            trajectory CSV, default " << kDefaultCsv << "\n"
        << "  --cycle-ms <ms>         EtherCAT PDO cycle time, default 1.0\n"
        << "  --timeout-us <us>       receive timeout, default 10000\n"
        << "  --max-lost <count>      stop after consecutive lost frames, default 20\n"
        << "  --field-mm <mm>         +/- full scale galvo field, default 11.0\n"
        << "  --x-offset-mm <mm>      galvo_x offset before scaling\n"
        << "  --y-offset-mm <mm>      galvo_y offset before scaling\n"
        << "  --repeat                loop the CSV forever\n"
        << "  --dry-run               parse CSV and print first samples without EtherCAT\n";
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
            opt.cycle_ms = std::strtod(need_value("--cycle-ms"), nullptr);
        } else if (arg == "--timeout-us") {
            opt.timeout_us = std::atoi(need_value("--timeout-us"));
        } else if (arg == "--max-lost") {
            opt.max_lost_frames = std::atoi(need_value("--max-lost"));
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
        } else if (arg == "--list-adapters") {
            opt.list_adapters = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg.rfind("--", 0) == 0) {
            throw std::runtime_error("unknown option: " + arg);
        } else {
            opt.ifname = arg;
        }
    }

    if (opt.cycle_ms <= 0.0 || opt.field_mm <= 0.0 || opt.timeout_us <= 0 || opt.max_lost_frames <= 0) {
        throw std::runtime_error("cycle-ms, field-mm, timeout-us and max-lost must be positive");
    }
    return opt;
}

void list_adapters()
{
    ec_adaptert *adapter = ec_find_adapters();
    if (adapter == nullptr) {
        std::puts("no adapters found");
        return;
    }

    for (ec_adaptert *it = adapter; it != nullptr; it = it->next) {
        std::printf("%s\n  %s\n", it->name, it->desc);
    }
    ec_free_adapters(adapter);
}

void print_state()
{
    ecx_readstate(&ctx);
    for (int i = 1; i <= ctx.slavecount; ++i) {
        ec_slavet *slave = &ctx.slavelist[i];
        std::printf("slave %d state=0x%04X AL=0x%04X %s\n",
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

    ecx_config_map_group(&ctx, IOmap, 0);
    ecx_configdc(&ctx);

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

    expected_wkc = (ctx.grouplist[0].outputsWKC * 2) + ctx.grouplist[0].inputsWKC;
    std::printf("mapped Obytes=%d Ibytes=%d expectedWKC=%d\n",
                ctx.grouplist[0].Obytes,
                ctx.grouplist[0].Ibytes,
                expected_wkc);

    if (ctx.grouplist[0].Obytes < static_cast<int>(sizeof(GalvoRxPdo))) {
        std::printf("RxPDO too small: need %zu bytes, mapped %d bytes\n",
                    sizeof(GalvoRxPdo),
                    ctx.grouplist[0].Obytes);
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
            std::puts("OP reached");
            return true;
        }
    }

    std::puts("failed to enter OP");
    print_state();
    ecx_close(&ctx);
    return false;
}

void stop_ethercat()
{
    if (ctx.grouplist[0].outputs != nullptr &&
        ctx.grouplist[0].Obytes >= static_cast<int>(sizeof(GalvoRxPdo))) {
        GalvoRxPdo stop{};
        std::memcpy(ctx.grouplist[0].outputs, &stop, sizeof(stop));
        ecx_send_processdata(&ctx);
        ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
    }

    if (ctx.slavecount > 0) {
        std::puts("request INIT");
        ctx.slavelist[0].state = EC_STATE_INIT;
        ecx_writestate(&ctx, 0);
    }
    ecx_close(&ctx);
}

void exchange_pdo(const GalvoRxPdo &rx, GalvoTxPdo *tx, int *wkc, int timeout_us)
{
    std::memcpy(ctx.grouplist[0].outputs, &rx, sizeof(rx));
    ecx_send_processdata(&ctx);
    *wkc = ecx_receive_processdata(&ctx, timeout_us);

    if (tx != nullptr) {
        std::memset(tx, 0, sizeof(*tx));
        if (ctx.grouplist[0].inputs != nullptr &&
            ctx.grouplist[0].Ibytes >= static_cast<int>(sizeof(*tx))) {
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
    const auto cycle = std::chrono::duration_cast<clock::duration>(
        std::chrono::duration<double, std::milli>(opt.cycle_ms));
    uint16_t sequence = 0;
    int lost_in_row = 0;
    int total_lost = 0;

    timeBeginPeriod(1);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

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
            exchange_pdo(rx, &tx, &wkc, opt.timeout_us);

            if (wkc <= 0) {
                ++lost_in_row;
                ++total_lost;
                std::printf("lost frame i=%zu seq=%u WKC=%d lost=%d/%d total=%d\n",
                            i,
                            rx.sequence,
                            wkc,
                            lost_in_row,
                            opt.max_lost_frames,
                            total_lost);
                print_state();
                if (lost_in_row >= opt.max_lost_frames) {
                    std::puts("too many consecutive lost frames, stop trajectory");
                    timeEndPeriod(1);
                    return;
                }
            } else {
                lost_in_row = 0;
            }

            if ((i % 1000u) == 0u || (wkc > 0 && wkc < expected_wkc)) {
                std::printf("i=%zu seq=%u code=(%d,%d) WKC=%d/%d status=0x%04X ack=%u frames=%u lost=%d\n",
                            i,
                            rx.sequence,
                            rx.x_code,
                            rx.y_code,
                            wkc,
                            expected_wkc,
                            tx.status,
                            tx.last_sequence,
                            tx.frame_counter,
                            total_lost);
            }

            std::this_thread::sleep_until(start + cycle * (i + 1));
        }
    } while (opt.repeat);

    timeEndPeriod(1);
}

} // namespace

int main(int argc, char *argv[])
{
    try {
        const Options opt = parse_args(argc, argv);

        if (opt.list_adapters) {
            list_adapters();
            return 0;
        }

        std::filesystem::path opened_csv;
        const auto samples = load_samples(opt.csv_path, &opened_csv);
        std::printf("loaded %zu samples\n", samples.size());

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
