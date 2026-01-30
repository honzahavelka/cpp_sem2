#ifndef SEM_2_MPTERM_H
#define SEM_2_MPTERM_H

#include <array>
#include <iostream>
#include <regex>

#include "mpint.h"

/*
 * Třída implementující terminálové rozhraní (REPL - Read-Eval-Print Loop).
 * Šablonový parametr TERM_PRECISION určuje přesnost čísel v tomto terminálu.
 */
template<size_t TERM_PRECISION>
class MPTerm {
public:
    MPTerm() = default;
    ~MPTerm() = default;

    /*
     * Hlavní smyčka aplikace.
     * Zajišťuje načítání vstupu, tokenizaci a spuštění příkazů.
     */
    void run() {
        std::string line;
        while (true) {
            std::cout << "mp>";
            /* Načtení celého řádku od uživatele */
            if (!std::getline(std::cin, line)) break;
            /* if is empty, continue */
            if (line.empty()) continue;

            /* Lexikální analýza: Převedení textu na tokeny */
            std::vector<std::string> tokens;
            try {
                tokens = getTokensFromLine(line);
            } catch (const std::exception& e) {
                std::cout << "Chyba pri cteni vstupu: " << e.what() << std::endl;
                continue;
            }

            /* debug print */
            /*
            for (const auto& t : tokens) {
                std::cout << "[" << t << "] ";
            }
            if (!tokens.empty()) std::cout << std::endl;
            */

            /* Validace prázdného vstupu po parsování */
            if (tokens.empty()) {
                std::cout << "Neplatne zadani (zadne zname tokeny)." << std::endl;
                continue;
            }

            /* Ukončení programu příkazem "exit" */
            if (tokens[0] == "exit") break;

            /* Syntaktická analýza a výpočet */
            if (!processTokens(tokens))
                std::cout << "Neplatne zadani." << std::endl;
        }
        std::cout << "Koncim." << std::endl;
    }


private:
    /*
     * Historie výsledků (Banka).
     * Používáme std::array s std::unique_ptr pro automatickou správu paměti.
     * Index 0 je $1 (nejnovější), Index 4 je $5 (nejstarší).
     */
    std::array<std::unique_ptr<MPInt<TERM_PRECISION>>, 5> history;

    /*
     * Tokenizer (Lexer).
     * Rozdělí vstupní řetězec na pole stringů (čísla, operátory, příkazy).
     */
    std::vector<std::string> getTokensFromLine(const std::string& line) const {
        std::vector<std::string> raw_tokens;

        // Hrubé rozdělení pomocí Regexu
        // Hledáme: klíčová slova, odkazy na historii ($N), čísla nebo operátory.
        std::regex re(R"((exit|bank|\$\d+|\d+|[-+*/%!]))");

        auto begin = std::sregex_iterator(line.begin(), line.end(), re);
        auto end = std::sregex_iterator();

        for (std::sregex_iterator i = begin; i != end; ++i) {
            raw_tokens.push_back(i->str());
        }

        // Kontextová analýza unárního mínusu.
        // Řeší rozdíl mezi "5 - 3" (operátor) a "5 * -3" (záporné číslo).
        std::vector<std::string> final_tokens;
        bool expect_operand = true; // Na začátku nebo po operátoru čekáme číslo

        for (size_t i = 0; i < raw_tokens.size(); ++i) {
            const std::string& t = raw_tokens[i];

            // Pokud vidíme "-" v kontextu, kde čekáme číslo, jde o unární mínus.
            if (t == "-" && expect_operand && (i + 1 < raw_tokens.size())) {
                const std::string& next = raw_tokens[i+1];
                if (std::isdigit(next[0])) {
                    final_tokens.push_back("-" + next); // Sloučení do jednoho tokenu
                    i++; // Přeskočení čísla
                    expect_operand = false;
                    continue;
                }
            }

            final_tokens.push_back(t);

            // Aktualizace stavového automatu pro příští iteraci
            if (t == "+" || t == "-" || t == "*" || t == "/" || t == "%") {
                expect_operand = true;
            } else if (t == "!") {
                expect_operand = true;
            } else {
                expect_operand = false;
            }
        }

        return final_tokens;
    }

    /*
     * Interpret příkazů.
     * Rozhoduje o akci na základě počtu tokenů (Postfix/Infix logika).
     */
    bool processTokens(const std::vector<std::string>& tokens) {
        if (tokens.empty()) return true;

        try {
            // Jeden token (Příkaz "bank" nebo prosté uložení čísla)
            if (tokens.size() == 1) {
                if (tokens[0] == "bank") {
                    for (size_t i = 0; i < history.size(); ++i) {
                        std::cout << "$" << (i + 1) << " = ";
                        if (history[i]) std::cout << *history[i] << std::endl;
                        else std::cout << "(empty)" << std::endl;
                    }
                    return true;
                }
                // Uživatel zadal jen číslo -> uložit do $1
                MPInt<TERM_PRECISION> val = resolveValue(tokens[0]);
                saveResult(val);
                return true;
            }

            // Dva tokeny (Unární operace, např. Faktoriál "5 !")
            if (tokens.size() == 2) {
                if (tokens[1] == "!") {
                    MPInt<TERM_PRECISION> val = resolveValue(tokens[0]);
                    saveResult(computeFactorial(val));
                    return true;
                }
                return false;
            }

            // Tři tokeny (Binární operace, např. "1 + 2")
            if (tokens.size() == 3) {
                MPInt<TERM_PRECISION> left = resolveValue(tokens[0]);
                std::string op = tokens[1];
                MPInt<TERM_PRECISION> right = resolveValue(tokens[2]);

                saveResult(computeOperator(left, op, right));
                return true;
            }

            // Pokud je tokenů víc nebo jiný počet, je to chyba
            return false;

        }
        // Exception Handling: Zachycení přetečení z MPInt
        catch (const typename MPInt<TERM_PRECISION>::OverflowException& e) {
            std::cout << ">>> Chyba: Doslo k preteceni! \n Preteceny vysledek ulozen." << std::endl;
            // Uložíme i oříznutý výsledek, aby byla videt funkcnostu
            saveResult(e.getResult());
            return true;
        }
        catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
            return false;
        }
    }

    /*
     * Pomocná metoda pro získání hodnoty.
     * Rozlišuje mezi literálem ("100") a odkazem na historii ("$1").
     */
    MPInt<TERM_PRECISION> resolveValue(const std::string& token) {
        if (token[0] == '$') {
            try {
                int index = std::stoi(token.substr(1)) - 1; // Převod $1 -> index 0

                if (!checkIndex(index)) {
                    throw std::invalid_argument("Neplatny index historie: " + token);
                }
                return *history[index];
            }
            catch (const std::invalid_argument&) { throw; }
            catch (...) {
                throw std::invalid_argument("Chybny format historie: " + token);
            }
        }

        return parseToken(token);
    }

    MPInt<TERM_PRECISION> parseToken(const std::string& token) {
        MPInt<TERM_PRECISION> value;
        try {
            value = token; // Využití operator=(string) ve třídě MPInt
        }
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

    MPInt<TERM_PRECISION> computeFactorial(const MPInt<TERM_PRECISION>& value) {
        return value.factorial();
    }

    /*
     * Uložení výsledku do historie.
     * Posune staré výsledky a nový vloží na začátek ($1).
     */
    void saveResult(MPInt<TERM_PRECISION> value) {
        moveHistory();
        history[0] = std::make_unique<MPInt<TERM_PRECISION>>(value);
        std::cout << "$1 = " << *history[0] << std::endl;
    }

    /*
     * Posun historie (Cyclic Buffer logic).
     * $4 -> $5, $3 -> $4, atd.
     */
    void moveHistory() {
        for (int i = static_cast<int>(history.size()) - 1; i > 0; --i) {
            history[i] = std::move(history[i - 1]);
        }
    }

    bool checkIndex(const int& index) {
        if (index < 0 || index >= history.size() || !history[index]) {
            std::cout << "Neplatný nebo prázdný index." << std::endl;
            return false;
        }
        return true;
    }
};

#endif