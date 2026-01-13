#include "instacne.hpp"
#include "lsearch.hpp"
#include <cstring>
#include "generatelp/generatelp.h"
bool readLpFile = false;
bool useNewVersion = true;

// int main(int argc, char** argv) {
//     util::setRandom(DEFAULT_RANDOM_SEED);

//     for (int i = 1; i < argc; i++) {
//         if (strcmp(argv[i], "--lp") == 0) {
//             readLpFile = true;
//         }
//         if (strcmp(argv[i], "--nv") == 0) {
//             useNewVersion = true;
//         }
//         if (strcmp(argv[i], "--ov") == 0) {
//             useNewVersion = false;
//         }
//     }

//     if (readLpFile) {
//         LS_NIA::NIA_Formula formula = LS_NIA::lpReader::readLpFile(argv[argc - 1]);

//         startTime = util::getTimePoint();
//         LS_NIA::LsSolver solver(formula);

//         solver.solve(useNewVersion);
//     } else {
//         LS_NIA::Instance instance;
        
//         instance.readDemandFile(argv[1]);
//         instance.readSampleFile(argv[2]);

//         LS_NIA::NIA_Formula formula = instance.genFormula();

//         startTime = util::getTimePoint();
//         LS_NIA::LsSolver solver(formula);

//         solver.solve(useNewVersion);
//     } 
// }

int main(int argc, char** argv) {
    util::setRandom(DEFAULT_RANDOM_SEED);

    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <demand_file> <supply_file> <heat_file> <output_base>" << std::endl;
        return 1;
    }

    std::string demand_filename = argv[1];
    std::string supply_filename = argv[2];
    std::string heat_file = argv[3];
    std::string output_base = argv[4];
    std::string lp_path = generate_lp(demand_filename,supply_filename, heat_file, output_base);
    std::cout << "lp_path: " << lp_path << std::endl;

    LS_NIA::NIA_Formula formula = LS_NIA::lpReader::readLpFile(lp_path);

    startTime = util::getTimePoint();
    LS_NIA::LsSolver solver(formula);

    solver.solve(useNewVersion);
}
