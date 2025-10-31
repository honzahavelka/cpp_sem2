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
            /* get line */
            if (!std::getline(std::cin, line)) break;
            /* get rid of spaces */
            trim(line);

            /* if is empty, continue */
            if (line.empty()) continue;
            /* get tokens form line */
            std::vector<std::string> tokens = getTokensFromLine(line);

            /* print tokens for debug */
            for (size_t i = 0; i < tokens.size(); ++i) {
                std::cout << tokens[i] << std::endl;
            }

            /* if is exit code, exit */
            if (tokens[0] == "exit") break;

            /* process tokens */
            if (!processTokens(tokens))
                std::cout << "Neplatne zadani." << std::endl;
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

    bool processTokens(const std::vector<std::string>& tokens) {
        if (tokens.empty()) return true;

        try {
            /* bank, or save normal number to history */
            if (tokens.size() == 1) {
                /* bank */
                if (tokens[0] == "bank") {
                    for (size_t i = 0; i < history.size(); ++i) {
                        if (history[i]) std::cout << "$" << (i+1) << " = " << *history[i] << std::endl;
                        else std::cout << "$" << (i+1) << " = (empty)" << std::endl;
                    }
                    return true;
                }
                /* save positive number */
                saveResult(parseToken(tokens[0]));
                return true;
            }

            /* factorial or save negative numer */
            if (tokens.size() == 2) {
                /* factorial */
                if (tokens[1] == "!") {
                    MPInt<TERM_PRECISION> value = parseToken(tokens[0]);
                    saveResult(computeFactorial(value));
                    return true;
                }
                /* save negative number */
                if (tokens[0] == "-") {
                    MPInt<TERM_PRECISION> value = parseToken(tokens[0] + tokens[1]);
                    saveResult(value);
                    return true;
                }
                return false;
            }

            /* factorial from history $1! or normal mathematical expression 1+1 */
            if (tokens.size() == 3) {
                /* factorial from history */
                if (tokens[0] == "$" && tokens[2] == "!") {
                    int index = std::stoi(tokens[1]) - 1;
                    if (index < 0 || index >= history.size() || !history[index]) {
                        std::cout << "Neplatný nebo prázdný index." << std::endl;
                        return false;
                    }
                    saveResult(computeFactorial(*history[index]));
                    return true;
                }

                /* normal expression */
                MPInt<TERM_PRECISION> a = parseToken(tokens[0]);
                MPInt<TERM_PRECISION> b = parseToken(tokens[2]);
                saveResult(computeOperator(a, tokens[1], b));
                return true;
            }
        }
        catch (const std::exception& e) {
            std::cout << e.what() << std::endl;
            return false;
        }
        return true;
    }

    MPInt<TERM_PRECISION> parseToken(const std::string& token) {
        MPInt<TERM_PRECISION> value;
        try {
            value = token;
        }
        // ReSharper disable once CppDFAUnreachableCode
        catch (const std::exception& e) {
            throw std::runtime_error("Chyba při převodu čísla: " + std::string(e.what()));
        }
        return value;
    }

    MPInt<TERM_PRECISION> computeOperator(const MPInt<TERM_PRECISION>& a, const std::string& op, const MPInt<TERM_PRECISION>& b) {
        if (op == "+") return a + b;
        if (op == "-") return a - b;
        if (op == "*") return a * b;
        if (op == "/") return a / b;
        throw std::invalid_argument("Invalid operator: " + op);
    }

    MPInt<TERM_PRECISION> computeFactorial(MPInt<TERM_PRECISION>& value) {
        value = value.factorial();
        return value;
    }

    void saveResult(MPInt<TERM_PRECISION> value) {
        moveHistory();
        history[0] = std::make_unique<MPInt<TERM_PRECISION>>(value);
        std::cout << "$1 = " << *history[0] << std::endl;
    }

    void moveHistory() {
        for (int i = static_cast<int>(history.size()) - 1; i > 0; --i) {
            history[i] = std::move(history[i - 1]);
        }
    }
};

#endif