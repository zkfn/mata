#include <string>
#include "utils.hh"
#include "mata/nfa/types.hh"
#include "mata/parser/inter-aut.hh"

int load_automaton(
        const std::string& filename,
        Nfa& aut,
        Mata::OnTheFlyAlphabet& alphabet,
        const bool skip_mintermization
) {
    std::vector<Mata::IntermediateAut> inter_auts;
    if (load_intermediate_automaton(filename, inter_auts) != EXIT_SUCCESS) {
        std::cerr << "Could not load intermediate autotomaton from \'" << filename << "'\n";
        return EXIT_FAILURE;
    }
    try {
        if (skip_mintermization or inter_auts[0].alphabet_type != Mata::IntermediateAut::AlphabetType::BITVECTOR) {
            aut = Mata::Nfa::Builder::construct(inter_auts[0], &alphabet);
        } else {
            Mata::Mintermization mintermization;
            TIME_BEGIN(mintermization);
            Mata::IntermediateAut mintermized = mintermization.mintermize(inter_auts[0]);
            TIME_END(mintermization);
            aut = Mata::Nfa::Builder::construct(mintermized, &alphabet);
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

int load_intermediate_automaton(
        const std::string& filename,
        std::vector<Mata::IntermediateAut>& out_inter_auts
) {
    std::fstream fs(filename, std::ios::in);
    if (!fs) {
        std::cerr << "Could not open file \'" << filename << "'\n";
        return EXIT_FAILURE;
    }

    Mata::Parser::Parsed parsed;
    try {
        parsed = Mata::Parser::parse_mf(fs, true);
        fs.close();

        if (parsed.size() != 1) {
            throw std::runtime_error(
                    "The number of sections in the input file is not 1\n");
        }
        if (!parsed[0].type.starts_with(TYPE_NFA)) {
            std::cout << (parsed[0].type) << std::endl;
            throw std::runtime_error("The type of input automaton is not NFA\n");
        }

        std::vector<Mata::IntermediateAut> inter_auts = Mata::IntermediateAut::parse_from_mf(parsed);
        out_inter_auts.push_back(inter_auts[0]);
    } catch (const std::exception& ex) {
        fs.close();
        std::cerr << "error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}