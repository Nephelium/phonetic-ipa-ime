#include "engine/ipa/update_manifest.h"
#include <fstream>
#include <iostream>
using namespace PhoneticIpa::Updates;
int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("fixture argument");
        std::ifstream input(argv[1]);
        nlohmann::json vectors;
        input >> vectors;
        for (const auto &test : vectors.at("cases"))
            if (Parse(test.at("feed").dump()).has_value() != test.at("valid").get<bool>())
                throw std::runtime_error(test.at("name"));
        for (const auto &test : vectors.at("versions"))
        {
            auto left = *ParseVersion(test[0]), right = *ParseVersion(test[1]);
            int comparison = left == right ? 0 : left > right ? 1 : -1;
            if (comparison != test[2])
                throw std::runtime_error("numeric version comparison");
        }
        Feed server = *Parse(vectors["cases"][0]["feed"].dump()), github = server;
        github.version = "0.1.10";
        if (Newest({server, github})->version != "0.1.10" || Newest({github, server})->version != "0.1.10" ||
            Newest({server})->version != server.version || Newest({}))
            throw std::runtime_error("source independence");
        if (Parse(std::string(65537, ' ')) || Parse("{"))
            throw std::runtime_error("size and malformed JSON");
        std::cout << "Update manifest, numeric versions, source independence and URL checks passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
