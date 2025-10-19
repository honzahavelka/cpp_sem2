#ifndef SEM_2_MPINT_H
#define SEM_2_MPINT_H

#include <cstddef>
#include <vector>

template<size_t PRECISION>
class MPInt {
public:
    /* biggest size_t available */
    static constexpr size_t Unlimited = static_cast<size_t>(-1);

    /* default constructor */
    MPInt() {
        if constexpr (PRECISION != Unlimited) {
            data.resize(PRECISION);
        }
        else {
            unlimited = true;
        }
    }

    /* getters */
    size_t size() const {
        return data.size();
    }
    const uint8_t getDataOnPos(size_t index) const {
        return data[index];
    }
    std::vector<uint8_t> getData() const {
        return data;
    }
    bool getUnlimited() const {
        return unlimited;
    }
    bool getNegative() const {
        return negative;
    }

    template<size_t OTHER_PRECISION>
    MPInt& operator=(const MPInt<OTHER_PRECISION>& other) {
        setData(other.getData(), other.getNegative());
        return *this;
    }

    MPInt& operator=(std::string str) {
        // odstranění mezer
        str.erase(std::remove(str.begin(), str.end(), ' '), str.end());

        if (str.empty()) {
            throw std::invalid_argument("MPInt argument is empty");
        }

        // kontrola znaménka
        negative = false;
        if (str[0] == '-') {
            negative = true;
            str = str.substr(1);
        }

        // inicializace prázdného data vektoru
        data.clear();

        // binární převod stringu na vektor uint8_t (little endian)
        std::vector<uint8_t> tmp;
        for (char c : str) {
            if (c < '0' || c > '9') throw std::invalid_argument("Invalid character in MPInt string");
            int digit = c - '0';

            // násobíme existující číslo 10 a přidáme novou cifru
            int carry = digit;
            for (size_t i = 0; i < tmp.size(); ++i) {
                int val = tmp[i] * 10 + carry;
                tmp[i] = val & 0xFF;     // uložíme dolních 8 bitů
                carry = val >> 8;        // zbytek do carry
            }
            if (carry > 0) tmp.push_back(carry);
        }

        // uložíme do dat
        data = std::move(tmp);

        return *this;
    }

    template<size_t OTHER_PRECISION>
    MPInt& operator+=(const MPInt<OTHER_PRECISION>& other) {
        /* if same negative, just add, addAbs controls overflow */
        if (negative == other.getNegative()) {
            addAbs(other);
        }
        else {
            /* no risk of overflow here */
            if (absGreaterOrEqual(other)) {
                subAbs(other);
            }
            /* risk of overflow */
            else {
                /* create tmp with bigger precision */
                MPInt<(PRECISION >= OTHER_PRECISION ? PRECISION : OTHER_PRECISION)> tmp;
                /* fill with bigger value */
                tmp = other;
                /* sub smaller value -> negative cannot change */
                tmp.subAbs(*this);
                /* try to "push" bigger value into maybe smaller precision, if cant, overflow error */
                try {
                    *this = tmp;
                    negative = tmp.getNegative();
                } catch (const std::overflow_error&) {
                    throw std::overflow_error("MPInt overflow in operator +=");
                }
            }
        }
        /* return new this */
        return *this;
    }

    template<size_t OTHER_PRECISION>
    MPInt& operator-=(const MPInt<OTHER_PRECISION>& other) {
        /* if same negative */
        if (negative == other.getNegative()) {
            /* no risk of overflow here */
            if (absGreaterOrEqual(other)) {
                subAbs(other);
            }
            /* risk of overflow */
            else {
                /* create tmp with bigger precision */
                MPInt<(PRECISION >= OTHER_PRECISION ? PRECISION : OTHER_PRECISION)> tmp;
                /* fill with bigger value */
                tmp = other;
                /* sub smaller value -> negative cannot change */
                tmp.subAbs(*this);
                /* try to "push" bigger value into maybe smaller precision, if cant, overflow error */
                try {
                    *this = tmp;
                    negative = !negative;
                    // ReSharper disable once CppDFAUnreachableCode
                } catch (const std::overflow_error&) {
                    throw std::overflow_error("MPInt overflow in operator -=");
                }
            }
        }
        else {
            addAbs(other);
        }

        return *this;
    }

    template<size_t OTHER_PRECISION>
    MPInt& operator*=(const MPInt<OTHER_PRECISION>& other) {
        /* get sizes */
        const size_t this_len = data.size();
        const size_t other_len = other.size();

        /* if all zeros for one number, return 0 */
        bool zeroA = std::all_of(data.begin(), data.end(), [](uint8_t b){ return b == 0; });
        bool zeroB = std::all_of(other.getData().begin(), other.getData().end(), [](uint8_t b){ return b == 0; });
        if (zeroA || zeroB) {
            std::fill(data.begin(), data.end(), 0);
            negative = false;
            return *this;
        }

        /* init result */
        const size_t result_len = this_len + other_len;
        std::vector<uint8_t> result(result_len, 0);

        /* multiply like in school */
        for (size_t i = 0; i < this_len; ++i) {
            uint16_t carry = 0;
            for (size_t j = 0; j < other_len; ++j) {
                const size_t pos = i + j;
                const uint16_t multiply = static_cast<uint16_t>(data[pos])
                                        * static_cast<uint16_t>(other.getDataOnPos(j))
                                        + result[pos] + carry;

                /* load last 8 bits */
                result[pos] = static_cast<uint8_t>(multiply & 0xFF);
                /* first eight into carry */
                carry = multiply >> 8;
            }

            /* for last inside for */
            if (carry > 0) {
                const size_t pos = i + other_len;
                /* in case there is a carry in last for */
                if (pos < result.size())
                    result[pos] += static_cast<uint8_t>(carry);
                else
                    result.push_back(static_cast<uint8_t>(carry));
            }
        }

        /* move result into data */
        if (PRECISION == unlimited)
            data = std::move(result);
        else {
            /* zero */
            std::fill(data.begin(), data.end(), 0);

            /* try to fill data, if cant, overflow error */
            for (size_t i = 0; i < result_len; i++) {
                uint8_t byte = result[i];
                if (byte == 0) {
                    continue;
                }
                /* cant squeeze into length */
                if (i >= data.size()) {
                    throw std::overflow_error("MPInt overflow in operator *=");
                }
                data[i] = byte;
            }
        }

        /* is negative? */
        this->negative = this->negative != other.getNegative();
        return *this;
    }

    template<size_t OTHER_PRECISION>
    MPInt& operator/=(const MPInt<OTHER_PRECISION>& other) {
        negative = negative != other.getNegative();
        absDiv(other);
        return *this;
    }

    template<size_t OTHER_PRECISION>
    MPInt& operator%=(const MPInt<OTHER_PRECISION>& other) {
        *this = absDiv(other);
        return *this;
    }


    std::string toString() const {
        if (std::all_of(data.begin(), data.end(), [](uint8_t b){ return b == 0; }))
            return "0";

        MPInt<MPInt<PRECISION>::Unlimited> temp;
        temp = *this;

        MPInt<1> ten;
        ten = "10";

        std::string digits;

        while (!temp.isZero()) {
            MPInt<MPInt<PRECISION>::Unlimited> remainder = temp.absDiv(ten); // temp /= 10, remainder = temp % 10
            digits.push_back('0' + remainder.getDataOnPos(0)); // převod na znak
        }

        std::reverse(digits.begin(), digits.end()); // cifry byly od nejnižší po nejvyšší

        if (negative)
            digits.insert(digits.begin(), '-');

        return digits;
    }

    friend std::ostream& operator<<(std::ostream& os, const MPInt<PRECISION>& num) {
        os << num.toString();
        return os;
    }


private:
    bool negative = false;
    bool unlimited = false;
    std::vector<uint8_t> data;

    template<size_t OTHER_PRECISION>
    friend class MPInt;

    void setData(const std::vector<uint8_t>& other, bool other_negative) {
        if (unlimited) {
            data.clear();
            data = other;
            negative = other_negative;
        }

        /* clear data */
        std::fill(data.begin(), data.end(), 0);

        /* try to fill data, if cant, overflow error */
        for (size_t i = 0; i < other.size(); i++) {
            uint8_t byte = other[i];
            if (byte == 0) {
                continue;
            }
            /* cant squeeze into length */
            if (i >= data.size()) {
                throw std::overflow_error("MPInt overflow: number exceeds allowed byte size");
            }
            data[i] = byte;
        }
        negative = other_negative;
    }

    template<size_t OTHER_PRECISION>
    void addAbs(const MPInt<OTHER_PRECISION>& other) {
        /* init carry */
        uint16_t carry = 0;
        const size_t this_len = data.size();
        const size_t other_len = other.size();
        /* max_data */
        const size_t max_len = std::max(this_len, other_len);

        /* for max_data_len */
        for (size_t i = 0; i < max_len; ++i) {
            const uint16_t this_byte = (i < this_len) ? data[i] : 0;
            const uint16_t other_byte = (i < other_len) ? other.getDataOnPos(i) : 0;

            /* sum of bytes plus carry */
            uint16_t sum = this_byte + other_byte + carry;

            /* check for overflow, already has initialized data.size in constructor */
            if (!unlimited) {
                if (i >= data.size() && sum != 0) throw std::overflow_error("MPInt overflow in operator +=");
                data[i] = static_cast<uint8_t>(sum & 0xFF);
            }
            /* if unlimited, just push_back */
            else {
                if (i >= data.size()) data.push_back(static_cast<uint8_t>(sum & 0xFF));
                else data[i] = static_cast<uint8_t>(sum & 0xFF);
            }

            /* carry = 8bits that were not written to data */
            carry = sum >> 8;
        }

        /* carry in last byte, if happened in limited -> overflow, else just push_back */
        if (carry != 0) {
            if (PRECISION != unlimited)
                throw std::overflow_error("MPInt overflow in operator+=");
            data.push_back(static_cast<uint8_t>(carry));
        }
    }

    template<size_t OTHER_PRECISION>
    /* function assumes that absolute value of this MPInt is greater than other */
    void subAbs(const MPInt<OTHER_PRECISION>& other) {
        /* declare borrow */
        int16_t borrow = 0;
        /* this len is greater or equal to other len */
        const size_t this_len = data.size();
        const size_t other_len = other.size();

        for (size_t i = 0; i < this_len; ++i) {
            const int16_t this_byte = data[i];
            const int16_t other_byte = (i < other_len) ? other.getDataOnPos(i) : 0;

            int16_t diff = this_byte - other_byte - borrow;

            /* if diff overflowed to next byte */
            if (diff < 0) {
                /* shift diff */
                diff += 256;
                /* set borrow to next */
                borrow = 1;
            }
            else {
                borrow = 0;
            }

            data[i] = static_cast<uint8_t>(diff & 0xFF);
        }
    }

    template<size_t OTHER_PRECISION>
    MPInt absDiv(const MPInt<OTHER_PRECISION>& other) {
        /* throw runtime error */
        if (other.isZero()) {
            throw std::runtime_error("MPInt division by zero");
        }

        /* other is larger than this, return reminder this and set all zero */
        if (!absGreaterOrEqual(other)) {
            MPInt<PRECISION> remainder;
            remainder = *this;
            std::fill(data.begin(), data.end(), 0);
            return remainder;
        }

        std::vector<uint8_t> result_data(data.size(), 0);
        std::vector<uint8_t> remainder_data;

        bool leading_zeros_flag = true;
        for (int i = static_cast<int>(data.size()) - 1; i >= 0; --i) {
            if (data[i] == 0 && leading_zeros_flag) {
                continue;
            }
            leading_zeros_flag = false;
            remainder_data.insert(remainder_data.begin(), data[i]);
            while (remainder_data.size() > 1 && remainder_data.back() == 0)
                remainder_data.pop_back();

            uint8_t q = 0;
            MPInt<PRECISION> rem;
            rem.setData(remainder_data, false);

            while (rem.absGreaterOrEqual(other)) {
                rem.subAbs(other);
                q++;
            }

            result_data[i] = q;
            remainder_data = rem.getData();
        }
        MPInt<PRECISION> remainder;
        remainder.setData(remainder_data, negative);
        this->setData(result_data, negative);
        return remainder;
    }

    template<size_t OTHER_PRECISION>
    bool absGreaterOrEqual(const MPInt<OTHER_PRECISION>& other) const {
        const size_t this_len = data.size();
        const size_t other_len = other.size();
        const size_t max_len = std::max(this_len, other_len);

        for (int i = max_len - 1; i >= 0; --i) {
            const uint8_t this_byte = (i < this_len) ? data[i] : 0;
            const uint8_t other_byte = (i < other_len) ? other.getDataOnPos(i) : 0;
            if (this_byte > other_byte)
                return true;
            if (this_byte < other_byte)
                return false;
        }

        return true;
    }

    bool isZero() const {
        return std::all_of(data.begin(), data.end(), [](uint8_t b) {
            return b == 0;
        });
    }
};

template <size_t PREC_A, size_t PREC_B>
auto operator+(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result;
        result = a;
        result += b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result;
        result = a;
        result += b;
        return result;
    }
}

template <size_t PREC_A, size_t PREC_B>
auto operator-(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result;
        result = a;
        result -= b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result;
        result = a;
        result -= b;
        return result;
    }
}

template <size_t PREC_A, size_t PREC_B>
auto operator*(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result;
        result = a;
        result *= b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result;
        result = a;
        result *= b;
        return result;
    }
}

template <size_t PREC_A, size_t PREC_B>
auto operator/(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result;
        result = a;
        result /= b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result;
        result = a;
        result /= b;
        return result;
    }
}

template <size_t PREC_A, size_t PREC_B>
auto operator%(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result;
        result = a;
        result %= b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result;
        result = a;
        result %= b;
        return result;
    }
}


#endif