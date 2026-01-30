#include "mpterm.h"

#include <charconv>
#include <cstring>

void printModeHelp() {
    std::cout << "mode <1> pro neomezenou presnost." << std::endl;
    std::cout << "mode <2> pro presnost 32 bajtu." << std::endl;
    std::cout << "mode <3> pro ukazku knihovny." << std::endl;
}

void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << " TEST: " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void printResult(bool success, const std::string& message) {
    if (success) std::cout << "[ OK ] " << message << "\n";
    else         std::cout << "[FAIL] " << message << "\n";
}

void runTestSuite() {
    try {
        std::cout << "Spoustim komplexni testy pro tridu MPInt...\n";
        std::cout << std::boolalpha; // Aby se bool vypisoval jako true/false

        // =============================================================
        // 1. ZÁKLADNÍ KONSTRUKCE A VÝPIS
        // =============================================================
        printHeader("1. Konstrukce a vypis (String conversion)");
        {
            MPInt<1> small("123");
            MPInt<1> neg("-50");
            MPInt<0> unlim("12345678901234567890");

            std::cout << "Small (1B): " << small << "\n";
            std::cout << "Neg (1B):   " << neg << "\n";
            std::cout << "Unlimited:  " << unlim << "\n";

            printResult(small.toString() == "123", "Kladne cislo OK");
            printResult(neg.toString() == "-50", "Zaporne cislo OK");
            printResult(unlim.toString() == "12345678901234567890", "UNLIMITED cislo OK");
        }

        // =============================================================
        // 2. POROVNÁVÁNÍ (COMPARE)
        // =============================================================
        printHeader("2. Porovnavani (<, >, ==, !=)");
        {
            MPInt<1> a("10");
            MPInt<1> b("20");
            MPInt<1> c("-10");
            MPInt<1> d("-20");

            printResult(a < b,  "10 < 20");
            printResult(b > a,  "20 > 10");
            printResult(a != b, "10 != 20");
            printResult(c > d,  "-10 > -20 (Zaporna logika)");
            printResult(d < c,  "-20 < -10");
            printResult(a.toString() == "10", "Rovnost se stringem");
        }

        // =============================================================
        // 3. ARITMETIKA BEZ PŘETEČENÍ (HAPPY PATH)
        // =============================================================
        printHeader("3. Zakladni aritmetika (+, -, *, /, %)");
        {
            MPInt<4> a("100");
            MPInt<4> b("3");

            // Sčítání
            MPInt<4> sum = a + b;
            printResult(sum.toString() == "103", "100 + 3 = 103");

            // Odčítání
            MPInt<4> sub = a - b;
            printResult(sub.toString() == "97", "100 - 3 = 97");

            // Násobení
            MPInt<4> mul = a * b;
            printResult(mul.toString() == "300", "100 * 3 = 300");

            // Dělení
            MPInt<4> div = a / b;
            printResult(div.toString() == "33", "100 / 3 = 33");

            // Modulo
            MPInt<4> mod = a % b;
            printResult(mod.toString() == "1", "100 % 3 = 1");
        }

        // =============================================================
        // 4. STRONG EXCEPTION GUARANTEE: Sčítání (+=)
        // =============================================================
        printHeader("4. CRITICAL: Operator += Overflow");
        {
            // Max hodnota pro 1 bajt je 255.
            MPInt<1> a("250");
            MPInt<1> b("10");

            std::cout << "Pocatecni stav: a = " << a << "\n";
            std::cout << "Akce: a += 10 (Ocekavany vysledek 260 -> Preteceni)\n";
            std::cout << "260 je hex 0x104. Oriznuti na 1 bajt -> 0x04 (4).\n";

            bool exceptionCaught = false;
            try {
                a += b;
            } catch (const MPInt<1>::OverflowException& e) {
                exceptionCaught = true;
                std::cout << ">>> VYJIMKA ZACHYCENA: " << e.what() << "\n";
                std::cout << ">>> Data ve vyjimce: " << e.getResult() << "\n";
                printResult(e.getResult().toString() == "4", "Spravne oriznuta hodnota ve vyjimce (4)");
            }

            if (!exceptionCaught) printResult(false, "Chyba: Nevyhozena vyjimka!");

            // DŮKAZ INTEGRITY DAT
            printResult(a.toString() == "250", "PUVODNI PROMENNA NEZMENENA");
        }

        // =============================================================
        // 5. STRONG EXCEPTION GUARANTEE: Odčítání (-=)
        // =============================================================
        printHeader("5. CRITICAL: Operator -= (Zmena znamenka + Overflow)");
        {
            // 10 - 1000 = -990.
            // -990 hex (ve 2B doplňku) je komplikovanější, ale absolutně je to 990 (0x3DE).
            // Do 1 bajtu se uloží 0xDE = 222.
            // Výsledek by měl být -222.
            MPInt<1> small("10");
            MPInt<2> big("1000");

            std::cout << "Pocatecni stav: small = " << small << "\n";
            try {
                small -= big;
                printResult(false, "Mela byt vyhozena vyjimka");
            } catch (const MPInt<1>::OverflowException& e) {
                std::cout << ">>> VYJIMKA: " << e.what() << ", Data: " << e.getResult() << "\n";
                printResult(e.getResult().toString() == "-222", "Spravne oriznuti na -222");
            }
            printResult(small.toString() == "10", "Puvodni data nezmenena");
        }

        // =============================================================
        // 6. NÁSOBENÍ A UNLIMITED PRECISION
        // =============================================================
        printHeader("6. Unlimited Precision (Nekonecna pamet)");
        {
            MPInt<0> u1("1000000000000000000000"); // 10^21
            MPInt<0> u2("1000000000000000000000"); // 10^21

            // Očekáváme 10^42
            std::cout << "Pocitam 10^21 * 10^21...\n";
            u1 *= u2;

            std::cout << "Vysledek: " << u1 << "\n";
            printResult(u1.toString().size() > 40, "Vysledek ma pres 40 cifer (spravna delka)");
            printResult(u1.toString().substr(0,1) == "1", "Zacina jednickou");
        }

        // =============================================================
        // 7. DĚLENÍ NULOU
        // =============================================================
        printHeader("7. Deleni nulou (Exceptions)");
        {
            MPInt<4> a("100");
            MPInt<4> zero("0");

            try {
                a / zero;
                printResult(false, "Mela nastat chyba deleni nulou");
            } catch (const std::invalid_argument& e) {
                printResult(true, std::string("Zachyceno: ") + e.what());
            }

            try {
                a % zero;
                printResult(false, "Mela nastat chyba modulo nula");
            } catch (const std::invalid_argument& e) {
                printResult(true, std::string("Zachyceno: ") + e.what());
            }
        }

        // =============================================================
        // 8. MÍCHÁNÍ PŘESNOSTÍ (TEMPLATES MIX)
        // =============================================================
        printHeader("8. Michani ruznych presnosti (Limited + Unlimited)");
        {
            MPInt<1> limit("100");
            MPInt<0> unlim("5000"); // Větší než se vejde do 1B

            // Limited += Unlimited (Přeteče)
            try {
                limit += unlim;
                printResult(false, "Melo pretect (100+5000 do 1B)");
            } catch (const MPInt<1>::OverflowException&) {
                printResult(true, "Zachyceno preteceni Limited += Unlimited");
            }

            // Unlimited += Limited (Vejde se vždy)
            MPInt<0> unlim2("5000");
            unlim2 += limit; // 5000 + 100
            printResult(unlim2.toString() == "5100", "Unlimited += Limited (5000 + 100 = 5100)");
        }

        std::cout << "\n========================================\n";
        std::cout << " VSECHNY TESTY DOKONCENY\n";
        std::cout << "========================================\n";

    } catch (const std::exception& e) {
        std::cout << "\n[CRITICAL FAILURE] Neocekavana vyjimka v main: " << e.what() << "\n";
    }
}
int main(const int argc, const char **argv) {
    if (argc != 2) {
        std::cout << "pouziti: my_program.exe <mode>\n";
        printModeHelp();
        return 1;
    }

    int mode;
    auto result = std::from_chars(argv[1], argv[1] + std::strlen(argv[1]), mode);
    if (result.ec != std::errc() || mode < 1 || mode > 3) {
        std::cerr << "mode musi byt 1, 2 nebo 3.\n";
        printModeHelp();
        return 1;
    }

    if (mode == 1) {
        std::cout << "MPCalc - rezim s neomezenou presnosti" << std::endl
        << "Zadejte jednoduchy matematicky vyraz s nejvyse jednou operaci +, -, *, / nebo !" << std::endl;
        MPTerm<0> term;
        term.run();
    }
    else if (mode == 2) {
        std::cout << "MPCalc - rezim s omezenou přesností na 32 bytů" << std::endl
        << "Zadejte jednoduchy matematicky vyraz s nejvyse jednou operaci +, -, *, / nebo !" << std::endl;
        MPTerm<32> term;
        term.run();
    }
    else {
        runTestSuite();
    }

    return 0;
}