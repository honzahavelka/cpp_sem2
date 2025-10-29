#ifndef SEM_2_MPTERM_H
#define SEM_2_MPTERM_H

#include <array>
#include <iostream>

#include "../mpint/mpint.h"

template<size_t TERM_PRECISION>
class MPTerm {
public:
    MPTerm() = default;
    ~MPTerm() = default;

    void run() {
        std::string line;
        while (true) {
            std::cout << "mp>";
            if (!std::getline(std::cin, line)) break;
            trim(line);

            if (line.empty()) continue;
            std::vector<std::string> tokens = getTokensFromLine(line);

            if (tokens[0] == "exit") break;

            if (!proccesTokens(tokens)) std::cout << "Neplatne zadani.";
        }
        std::cout << "ending" << std::endl;
    }


private:
    std::array<std::unique_ptr<MPInt<TERM_PRECISION>>, 5> history;

    void trim(std::string& line) {
        line.erase(std::remove_if(line.begin(), line.end(),
        [](unsigned char c){ return std::isspace(c); }),
           line.end());
    }

    std::vector<std::string> getTokensFromLine(const std::string& line) {
        std::vector<std::string> tokens;
        std::string token;

        if (line == "exit") {
            tokens.emplace_back("exit");
            return tokens;
        }
        if (line == "bank") {
            tokens.emplace_back("bank");
            return tokens;
        }

        size_t flag = 0;
        for (size_t i = 0; i < line.size(); ++i) {

            if (line[i] == '$') {
                tokens.emplace_back("$");
                continue;
            }
            if (line[i] >= '0' && line[i] <= '9') {
                flag = 1;
                token += line[i];
                continue;
            }

            /* must be operator */
            if (flag) {
                tokens.push_back(token);
            }
            token = line[i];
            tokens.push_back(token);
            token = "";
            flag = 0;
        }

        if (flag) {
            tokens.push_back(token);
        }

        return tokens;
    }

    bool proccesTokens(const std::vector<std::string>& tokens) {
        if (tokens.empty()) return true;

        if (tokens.size() == 1) {
            if (tokens[0] == "bank") {
                for (size_t i = 0; i < history.size(); ++i) {
                    std::cout << "$" << i + 1 << " = " << history[i] << std::endl;
                }
                return true;
            }
        }

        return true;
    }
};


#endif