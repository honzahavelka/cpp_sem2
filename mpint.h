#ifndef SEM_2_MPINT_H
#define SEM_2_MPINT_H

#include <cassert>
#include <vector>

template<size_t PRECISION>
class MPInt {
public:

    // třída pro vlastní vyjímku, která vrací přetečené číslo
    class OverflowException final : public std::overflow_error {
    private:
        MPInt<PRECISION> result; // přetečené číslo
    public:
        explicit OverflowException(const MPInt<PRECISION>& res, const std::string& msg = "MPInt overflow")
            : std::overflow_error(msg), result(res) {}

        // getter
        const MPInt<PRECISION>& getResult() const {
            return result;
        }
    };

    // speciální konstanta pro rozlišení Unlimited režimu.
    static constexpr size_t Unlimited = 0;

    // defaultní konstruktor
    MPInt() {
        if constexpr (PRECISION == Unlimited) {
            data.resize(0);
        }
        else {
            // naplníme array nulama
            std::fill(data.begin(), data.end(), 0);
        }
    }

    // konstrukotr ze stringu, volá přetížený operátor =
    MPInt(const std::string& str) {
        *this = str;
    }

    // kontruktor pro čísla
    MPInt(const long long num) {
        // převedem na string a zavoláme přetíženej operátor
        *this = std::to_string(num);
    }

    ~MPInt() = default;

    // copy konstruktor
    template<size_t OTHER_PRECISION>
    MPInt(const MPInt<OTHER_PRECISION>& other) : MPInt() {
        // když maj stejnou délku není co řešit
        if constexpr (PRECISION == OTHER_PRECISION) {
            data = other.data; // std::vector nebo std::array assignment
            negative = other.negative;
        }
        else {
            // pokud jsou různé, musíme použít setData - safe funkce pro pokud naplnění dat
            setData(other.getData(), other.getNegative());
        }
    }
    // copy assignment
    template<size_t OTHER_PRECISION>
    MPInt& operator=(const MPInt<OTHER_PRECISION>& other) {
        // jestli maj stejnou přesnost, neni co řešit
        if constexpr (PRECISION == OTHER_PRECISION) {
            if (this == &other) return *this;
            data = other.data;
            negative = other.negative;
        }
        else {
            // bezpečnost
            setData(other.getData(), other.getNegative());
        }
        return *this;
    }

    // move constructor
    template<size_t OTHER_PRECISION>
    MPInt(MPInt<OTHER_PRECISION>&& other) : MPInt() {
        // stejná přesnost -> neni co řešit
        if constexpr (PRECISION == OTHER_PRECISION) {
            data = std::move(other.data);
            negative = other.negative;
        }
        else {
            // safe cesta
            setData(other.getData(), other.getNegative());
        }
        // vynulovat data ostatního
        other.clearData();
    }
    // move assignment
    template<size_t OTHER_PRECISION>
    MPInt& operator=(MPInt<OTHER_PRECISION>&& other) {
        if constexpr (PRECISION == OTHER_PRECISION) {
            // ochrana proti move sama sebe
            if (this == &other) return *this;

            data = std::move(other.data);
            negative = other.negative;
        }
        else {
            setData(other.getData(), other.getNegative());
        }
        other.clearData();
        return *this;
    }

    // naplnění dat za stringu
    MPInt& operator=(std::string str) {
        // vymazání mezer
        str.erase(std::ranges::remove(str, ' ').begin(), str.end());

        // ošetření prázdného
        if (str.empty()) {
            throw std::invalid_argument("MPInt argument is empty");
        }

        // vynulovat data, jestli nich předtím bylo něco jiného
        if constexpr (PRECISION == Unlimited)
            data.clear();
        else
            std::fill(data.begin(), data.end(), 0);

        // check - a + na začátku -> přijmeme
        size_t start_index = 0;
        negative = false;
        if (str[0] == '-') {
            negative = true;
            start_index = 1;
        }
        else if (str[0] == '+') {
            start_index = 1;
        }

        // pokud řetězec obsahoval jen - nebo +, je to chyba
        if (start_index >= str.size()) {
            throw std::invalid_argument("MPInt string contains only sign");
        }

        // projedeme celej string
        for (size_t k = start_index; k < str.size(); ++k) {
            const char c = str[k];
            // musí to být číslo, jinak je to špatně
            if (c < '0' || c > '9') throw std::invalid_argument("Invalid character in MPInt string");
            const int digit = c - '0';

            int carry = digit;
            // pokus to narvat do dat
            for (size_t i = 0; i < data.size(); ++i) {
                const int val = data[i] * 10 + carry;
                data[i] = val & 0xFF;
                carry = val >> 8;
            }
            // pokud zbylo carry
            if (carry > 0) {
                // do unlimited prostě přidáme další byte
                if constexpr (PRECISION == Unlimited)
                    data.push_back(carry);
                // jinak to přeteklo
                else {
                    // vynulovat
                    std::fill(data.begin(), data.end(), 0);
                    throw OverflowException(*this, "Overflow in operator = ");
                }
            }
        }

        if (isZero()) {
            // řešení -0
            negative = false;
        }
        return *this;
    }

    template<size_t OTHER_PRECISION>
    MPInt& operator+=(const MPInt<OTHER_PRECISION>& other) {
        // pokud jsou stejný znaménka - zavoláme privátní funkci addAbs, která při přetečení
        // vyhodí overflow -> zachytíme a přidáme msg
        if (negative == other.getNegative()) {
            // vytvoříme kopii
            MPInt<PRECISION> temp = *this;

            try {
                // pokus o přiřazení
                temp.addAbs(other);
                *this = std::move(temp);
            }
            catch (const OverflowException& e) {
                throw OverflowException(e.getResult(), "Overflow in operator +=");
            }
        }
        else {
            // tady neriskujem overflow
            if (compareAbs(other) > -1) {
                subAbs(other);
            }
            // tady ale jo
            else {
                // vytvoříme s větší přesností
                MPInt<(PRECISION >= OTHER_PRECISION ? PRECISION : OTHER_PRECISION)> tmp_big;
                // dáme tam tu větší hodnotu
                tmp_big = other;
                // odečteme menší
                tmp_big.subAbs(*this);

                // pokusíme se to narvat do menší přesnosti, pokud to nepujde -> overflow
                try {
                    MPInt<PRECISION> tmp;
                    tmp.setData(tmp.getData(), !negative);
                    *this = std::move(tmp);
                } catch (const OverflowException& e) {
                    throw OverflowException(e.getResult(), "Overflow in operator +=");;
                }
            }
        }
        return *this;
    }

    template<size_t OTHER_PRECISION>
    MPInt& operator-=(const MPInt<OTHER_PRECISION>& other) {
        // stejný znaménka
        if (negative == other.getNegative()) {
            // tady neriskujeme overflow
            if (compareAbs(other) > -1) {
                subAbs(other);
            }
            // tady jo
            else {
                // stejně jako v += -> vytvořit ten větší a pak se pokusit to narvat do menšího
                MPInt<(PRECISION >= OTHER_PRECISION ? PRECISION : OTHER_PRECISION)> tmp_big;
                tmp_big = other;
                tmp_big.subAbs(*this);

                try {
                    MPInt<PRECISION> tmp;
                    tmp.setData(tmp_big.getData(), !negative);
                    *this = std::move(tmp);
                } catch (const OverflowException& e) {
                    throw OverflowException(e.getResult(), "Overflow in operator -=");
                }
            }
        }
        else {
            // pokud se nevejde, zachytíme vyjímku a přidáme msg
            MPInt<PRECISION> temp = *this;
            try {
                temp.addAbs(other);
                *this = std::move(temp);
            }
            catch (const OverflowException& e) {
                throw OverflowException(e.getResult(), "Overflow in operator -=");
            }
        }
        return *this;
    }

    template<size_t OTHER_PRECISION>
    MPInt& operator*=(const MPInt<OTHER_PRECISION>& other) {
        const size_t this_len = data.size();
        const size_t other_len = other.size();

        // pokud je jedno z čísel 0 -> rovnou vrátit 0
        const bool zeroA = std::all_of(data.begin(), data.end(), [](const uint8_t b){ return b == 0; });
        const bool zeroB = std::all_of(other.getData().begin(), other.getData().end(), [](uint8_t b){ return b == 0; });
        if (zeroA || zeroB) {
            if constexpr (PRECISION == Unlimited) data.clear(); // Pro vektor vyčistíme
            else std::fill(data.begin(), data.end(), 0);        // Pro array vynulujeme

            negative = false;
            return *this;
        }

        // maximalní délka je součet délek
        const size_t result_len = this_len + other_len;
        // použití vektoru pro dočasný výsledek
        std::vector<uint8_t> result(result_len, 0);

        // algoritmus školního násobení
        // iterace přes obě čísla byte po bytu
        for (size_t i = 0; i < this_len; ++i) {
            uint16_t carry = 0;
            for (size_t j = 0; j < other_len; ++j) {
                // pozice ve výsledku je součet indexů i + j.
                const size_t pos = i + j;
                // výpočet: (číslice A * číslice B) + (hodnota co už tam je) + přenos
                const uint16_t multiply = static_cast<uint16_t>(data[i])
                                        * static_cast<uint16_t>(other.data[j])
                                        + result[pos] + carry;

                // uložení posledních 8 bitů
                result[pos] = static_cast<uint8_t>(multiply & 0xFF);

                // horních 8 do carry
                carry = multiply >> 8;
            }
            // pokud po dokončení řádku zbyl přenos, přičteme ho k vyššímu řádu
            if (carry > 0) {
                const size_t pos = i + other_len;
                if (pos < result.size())
                    result[pos] += static_cast<uint8_t>(carry);
                else
                    result.push_back(static_cast<uint8_t>(carry));
            }
        }

        // výpočet výsledného znaménka
        bool new_sign = this->negative != other.getNegative();


        if constexpr (PRECISION == Unlimited) {
            // odstranění přebytečných nul na konci (v Little Endian jsou to nuly nejvyššího řádu)
            while (!result.empty() && result.back() == 0) {
                result.pop_back();
            }
            data = std::move(result);
            negative = new_sign;
        }
        else {
            // kontrola přetečení
            MPInt<PRECISION> temp;
            temp.negative = new_sign;

            bool overflow = false;

            for (size_t i = 0; i < result.size(); i++) {
                uint8_t byte = result[i];

                if (i < temp.data.size()) {
                    temp.data[i] = byte;
                }
                else if (byte != 0) {
                    overflow = true;
                }
            }

            if (overflow) {
                // vyhodíme výjimku obsahující 'temp' (oříznutý výsledek).
                // původní objekt *this je stále v původním stavu.
                throw OverflowException(temp, "Overflow in operator *=");
            }
            // commit změn pouze pokud nenastala chyba
            *this = std::move(temp);
        }
        return *this;
    }

    template<size_t OTHER_PRECISION>
    MPInt& operator/=(const MPInt<OTHER_PRECISION>& other) {
        // přeuložíme nové znaménko
        const bool new_sign = negative != other.getNegative();
        // uděláme změny
        absDiv(other);
        // uložíme nové znaménko
        negative = new_sign;
        return *this;
    }

    template<size_t OTHER_PRECISION>
    MPInt& operator%=(const MPInt<OTHER_PRECISION>& other) {
        // uložíme zbytek po dělení
        *this = absDiv(other);
        return *this;
    }

    template<size_t OTHER_PRECISION>
    int compareAbs(const MPInt<OTHER_PRECISION>& other) const {
        const size_t this_len = data.size();
        const size_t other_len = other.size();
        const size_t max_len = std::max(this_len, other_len);

        for (int i = max_len - 1; i >= 0; --i) {
            const uint8_t this_byte = (i < this_len) ? data[i] : 0;
            const uint8_t other_byte = (i < other_len) ? other.data[i] : 0;
            // jakmile je ten byte větší - víme že je to číslo větší
            if (this_byte > other_byte)
                return 1;
            if (this_byte < other_byte)
                return -1;
        }

        // sem projedou stejý čísla
        return 0;
    }

    MPInt<PRECISION> factorial() const {
        // sanity check
        if (negative) {
            throw std::invalid_argument("MPInt factorial of negative number is undefined.");
        }
        if (isZero()) {
            MPInt<PRECISION> result;
            result = "1";
            return result;
        }
        // pomocné mpinty
        MPInt<1> one("1");
        MPInt<PRECISION> result;
        result = "1";
        MPInt<PRECISION> counter;
        counter = "2";

        while (counter <= *this) {
            try {
                // pokus o násobení
                result *= counter;
            } catch (const OverflowException& e) {
                // vyhození vyjímky
                throw OverflowException(e.getResult(), "MPInt overflow in factorial");
            }
            counter += one;
        }

        return result;
    }

    // pro výpis pomocí streamu
    friend std::ostream& operator<<(std::ostream& os, const MPInt<PRECISION>& num) {
        os << num.toString();
        return os;
    }

    std::string toString() const {
        if (data.empty() || std::all_of(data.begin(), data.end(), [](const uint8_t b){ return b == 0; }))
            return "0";

        // pracujeme na kopii, protože algoritmus je destruktivní
        MPInt<PRECISION> temp;
        temp = *this;
        const MPInt<1> ten("10");
        std::string digits;

        // zbytek po dělení 10 (temp % 10) je poslední číslice (jednotky).
        // číslo vydělíme 10 (temp /= 10) a posuneme se o řád níž.
        // ppakujeme, dokud není číslo nula.
        while (!temp.isZero()) {
            MPInt<PRECISION> remainder = temp.absDiv(ten); // temp /= 10, remainder = temp % 10
            digits.push_back('0' + remainder.getDataOnPos(0)); // převod na znak
        }

        std::ranges::reverse(digits); // cifry byly od nejnižší po nejvyšší

        // přidat -
        if (negative)
            digits.insert(digits.begin(), '-');

        return digits;
    }

    // gettery
    size_t size() const {
        return data.size();
    }
    uint8_t getDataOnPos(size_t index) const {
        if (index >= data.size()) {
            return 0;
        }
        return data[index];
    }
    const auto& getData() const {
        return data;
    }
    bool getUnlimited() const {
        return PRECISION == Unlimited;
    }
    bool getNegative() const {
        return negative;
    }


private:
    bool negative = false; // true = záporné číslo

    /*
     * Uložiště dat:
     * - Používáme Little Endian (nejméně významný bajt je na indexu 0).
     * - To zjednodušuje matematické operace (sčítání, násobení), protože se iteruje od 0.
     * - Hybridní model paměti:
     * - Pokud je PRECISION == 0 (Unlimited), používáme std::vector (dynamická paměť na haldě).
     * - Pokud je PRECISION > 0 (Limited), používáme std::array (statická paměť na zásobníku).
     * - std::conditional_t vybírá typ v době kompilace.
     */
    using DataContainer = std::conditional_t<
        PRECISION == Unlimited,
        std::vector<uint8_t>,           // pro Unlimited je to Vector
        std::array<uint8_t, PRECISION>  // pro Limited je to Array
    >;

    DataContainer data;

    // aby instance MPInt s jinou přesnostínmohla přistupovat
    // k privátním členům  této instance
    template<size_t OTHER_PRECISION>
    friend class MPInt;

    /*
     * univerzální metoda pro bezpečné nastavení dat.
     * přijímá jakýkoliv kontejner díky šabloně.
     */
    template <typename ContainerType>
    void setData(const ContainerType& other, const bool other_negative) {
        // pokud je tento objekt Unlimited (std::vector), nemůže dojít k přetečení
        if constexpr (PRECISION == Unlimited) {
            data.clear();
            data.assign(other.begin(), other.end());
            negative = other_negative;
            return;
        }

        // vyčistíme data
        std::fill(data.begin(), data.end(), 0);

        bool overflow = false;
        // pokus o narvání čísla
        for (size_t i = 0; i < other.size(); i++) {
            uint8_t byte = other[i]; // funguje pro vector i array
            if (byte == 0) {
                continue;
            }
            // nemůžem se vejít
            if (i >= data.size()) {
                overflow = true;
            }
            else {
                data[i] = byte;
            }

        }
        negative = other_negative;

        // pokud sme se nevešli, vrátíme přetečení
        if (overflow) {
            throw OverflowException(*this);
        }
    }

    // vyčištění
    void clearData() {
        negative = false;
        if constexpr (PRECISION == Unlimited) {
            data.clear();
        }
        else {
            std::ranges::fill(data, 0);
        }
    }

    template<size_t OTHER_PRECISION>
    void addAbs(const MPInt<OTHER_PRECISION>& other) {
        uint16_t carry = 0;
        const size_t this_len = data.size();
        const size_t other_len = other.size();

        // zjistíme maximální délku, abychom prošli všechny řády obou čísel
        const size_t max_len = std::max(this_len, other_len);

        bool overflow = false;

        for (size_t i = 0; i < max_len; ++i) {
            // bezpečné načtení bajtů: pokud je index mimo rozsah kratšího čísla, bereme 0
            const uint16_t this_byte = (i < this_len) ? data[i] : 0;
            const uint16_t other_byte = (i < other_len) ? other.data[i] : 0;

            // samotný součet bajtů včetně přenosu z minula
            const uint16_t sum = this_byte + other_byte + carry;

            if constexpr (PRECISION != Unlimited) {
                if (i < data.size()) {
                    // jsme uvnitř pole, můžeme zapisovat
                    data[i] = static_cast<uint8_t>(sum & 0xFF);
                }
                else if (sum != 0) {
                    // jsme mimo pole A zároveň máme nenulovou hodnotu -> chyba
                    overflow = true;
                }
            }
            // unlimited prostě zvětšíme
            else {
                if (i >= data.size()) data.push_back(static_cast<uint8_t>(sum & 0xFF));
                else data[i] = static_cast<uint8_t>(sum & 0xFF);
            }

            // bitový posun pro získání přenosu do další iterace
            carry = sum >> 8;
        }

        // zpracování zbylého přenosu po ukončení cyklu
        if (carry != 0) {
            if constexpr (PRECISION != Unlimited)
                overflow = true;
            else {
                data.push_back(static_cast<uint8_t>(carry));
            }
        }

        if (overflow) {
            throw OverflowException(*this);
        }
    }

    template<size_t OTHER_PRECISION>
    // funkce předpokládá, že |this| >= |other|
    void subAbs(const MPInt<OTHER_PRECISION>& other) {
        int16_t borrow = 0;
        const size_t this_len = data.size();
        const size_t other_len = other.size();

       // algoritmus odčítání pod sebou (Little Endian).
       // procházíme od nejnižšího řádu k nejvyššímu.
        for (size_t i = 0; i < this_len; ++i) {
            const int16_t this_byte = data[i];
            // pokud je druhé číslo kratší, doplníme virtuální nuly
            const int16_t other_byte = (i < other_len) ? other.getDataOnPos(i) : 0;

            // výpočet rozdílu včetně předchozí výpůjčky
            int16_t diff = this_byte - other_byte - borrow;

            // řešení podtečení v rámci bajtu
            if (diff < 0) {
                // půjčíme si 256 z vyššího řádu
                diff += 256;
                // nastavíme dluh pro další iteraci
                borrow = 1;
            }
            else {
                borrow = 0;
            }

            data[i] = static_cast<uint8_t>(diff & 0xFF);
        }

        // Normalizace pro Unlimited: Odstranění nul na začátku čísla
        if constexpr (PRECISION == Unlimited) {
            while (data.size() > 0 && data.back() == 0) {
                data.pop_back();
            }
        }
    }

    template<size_t OTHER_PRECISION>
    MPInt absDiv(const MPInt<OTHER_PRECISION>& other) {
        // sanity check
        if (other.isZero()) {
            throw std::invalid_argument("MPInt division by zero");
        }

        // případ: dělenec < dělitel.
        if (compareAbs(other) == -1) {
            MPInt<PRECISION> remainder;
            remainder = *this;

            clearData();
            return remainder;
        }

        std::vector<uint8_t> result_data(data.size(), 0);   // pole pro podíl
        std::vector<uint8_t> remainder_data;                    // buffer pro aktuální zbyte

        MPInt<PRECISION> current_rem;   // pomocný objekt pro výpočty se zbytkem

        bool leading_zeros_flag = true;
        // algoritmus písemného dělení
        // iterujeme od nejvýznamnějšího bajtu k nejméně významnému.
        for (size_t i = data.size(); i > 0; --i) {
            size_t idx = i - 1;

            // Přeskočení úvodních nul dělence
            if (data[idx] == 0 && leading_zeros_flag) {
                continue;
            }
            leading_zeros_flag = false;
            // sepíšeme další číslici: přidáme bajt na nejnižší pozici aktuálního zbytku
            remainder_data.insert(remainder_data.begin(), data[idx]);

            // úklid bufferu (odstranění nepotřebných nul)
            while (remainder_data.size() > 1 && remainder_data.back() == 0)
                remainder_data.pop_back();

            current_rem.setData(remainder_data, false);

            // hledání cifry podílu
            // kolikrát se dělitel vejde do aktuálního zbytku?
            uint8_t q = 0;
            while (current_rem.compareAbs(other) > -1) {    // dokud je zbytek >= dělitel
                current_rem.subAbs(other);
                q++;
            }

            result_data[idx] = q;

            // aktualizace zbytku pro další iteraci
            const auto& rem_d = current_rem.getData();
            remainder_data.assign(rem_d.begin(), rem_d.end());
        }

        if constexpr (PRECISION == Unlimited) {
            while (!result_data.empty() && result_data.back() == 0) {
                result_data.pop_back();
            }
        }

        // příprava návratové hodnoty - zbytku
        MPInt<PRECISION> remainder;
        remainder.setData(remainder_data, negative);
        this->setData(result_data, negative);
        return remainder;
    }

    // pomocná fce na určení 0
    bool isZero() const {
        return std::all_of(data.begin(), data.end(), [](uint8_t b) {
            return b == 0;
        });
    }
};

/*
 * -----------------------------------------------------------------------------
 * Globální aritmetické operátory (+, -, *, /, %)
 * -----------------------------------------------------------------------------
 * Všechny operátory fungují na stejném principu:
 * 1. Jsou šablonované, aby umožnily operace mezi různými přesnostmi (PREC_A, PREC_B).
 * 2. Návratový typ 'auto' se určí podle pravidel:
 * - Pokud je alespoň jeden operand Unlimited -> výsledek je Unlimited.
 * - Jinak je výsledek Limited s přesností většího z operandů (MaxBytes).
 * 3. Využívají "if constexpr" pro optimalizaci v době kompilace.
 * 4. Implementace využívá již hotové operátory +=, -=, atd.
 */
template <size_t PREC_A, size_t PREC_B>
auto operator+(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result = a;
        result += b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result = a;
        result += b;
        return result;
    }
}

template <size_t PREC_A, size_t PREC_B>
auto operator-(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result = a;
        result -= b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result = a;
        result -= b;
        return result;
    }
}

template <size_t PREC_A, size_t PREC_B>
auto operator*(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result = a;
        result *= b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result = a;
        result *= b;
        return result;
    }
}

template <size_t PREC_A, size_t PREC_B>
auto operator/(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result = a;
        result /= b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result = a;
        result /= b;
        return result;
    }
}

template <size_t PREC_A, size_t PREC_B>
auto operator%(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result = a;
        result %= b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result = a;
        result %= b;
        return result;
    }
}

/*
 * -----------------------------------------------------------------------------
 * Porovnávací operátory (==, !=, <, >, <=, >=)
 * -----------------------------------------------------------------------------
 */
template <size_t PREC_A, size_t PREC_B>
bool operator==(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    if (a.getNegative() != b.getNegative()) return false;
    return a.compareAbs(b) == 0;
}

template <size_t PREC_A, size_t PREC_B>
bool operator!=(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    return !(a == b);
}

template <size_t PREC_A, size_t PREC_B>
bool operator<(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    if (a.getNegative() && !b.getNegative()) return true;
    if (!a.getNegative() && b.getNegative()) return false;
    const int cmp = a.compareAbs(b);
    return a.getNegative() ? (cmp > 0) : (cmp < 0);
}

template <size_t PREC_A, size_t PREC_B>
bool operator>(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    return b < a;
}

template <size_t PREC_A, size_t PREC_B>
bool operator<=(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    return !(a > b);
}

template <size_t PREC_A, size_t PREC_B>
bool operator>=(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    return !(a < b);
}
#endif