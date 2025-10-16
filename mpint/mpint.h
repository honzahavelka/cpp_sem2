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

    template<size_t OTHER_PRECISION>
    MPInt(const MPInt<OTHER_PRECISION>& other) {
        this->data = other.getData();
        this->unlimited = other.getUnlimited();
        this->negative = other.getNegative();
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

    MPInt& operator=(const MPInt& other) {
        if (unlimited) {
            data.clear();
            data = other.getData();
            negative = other.getNegative();
            return *this;
        }

        /* clear data */
        std::fill(data.begin(), data.end(), 0);

        /* try to fill data, if cant, overflow error */
        for (size_t i = 0; i < other.size(); i++) {
            uint8_t byte = other.getDataOnPos(i);
            if (byte == 0) {
                continue;
            }
            /* cant squeeze into length */
            if (i >= data.size()) {
                throw std::overflow_error("MPInt overflow: number exceeds allowed byte size");
            }
            data[i] = byte;
        }

        negative = other.getNegative();

        return *this;
    }

    MPInt& operator=(std::string str) {
        /* erase all spaces */
        std::erase(str, ' ');

        if (str.empty()) {
            throw std::invalid_argument("MPInt argument is empty");
        }

        /* is negative */
        if (str[0] == '-') {
            negative = true;
            str = str.substr(1, str.size() - 1);
        }

        /* split string into chunks per 18 numbers */
        const size_t len = str.size();
        size_t chunks_size = len / 18;
        if (len % 18 != 0) {
            ++chunks_size;
        }
        std::vector<uint64_t> chunks(chunks_size);
        size_t i = 0;
        for (; i < chunks_size - 1; ++i) {
            try {
                chunks[i] = std::stoull(str.substr(len - (i + 1) * 18, 18));
            } catch (const std::invalid_argument&) {
                throw std::runtime_error("MPInt invalid character in number string");
            } catch (const std::out_of_range&) {
                /* this should not happen */
                throw std::runtime_error("MPInt chunk value out of range for uint64_t");
            }
        }
        try {
            chunks[chunks_size - 1] = std::stoull(str.substr(0, len - (i * 18)));
        } catch (const std::invalid_argument&) {
            throw std::runtime_error("MPInt invalid character in number string");
        } catch (const std::out_of_range&) {
            /* this should not happen */
            throw std::runtime_error("MPInt chunk value out of range for uint64_t");
        }

        /* clear data */
        if (unlimited) {
            data.clear();
        }
        else {
            std::fill(data.begin(), data.end(), 0);
        }

        /* fill data with chunks */
        const size_t data_len = data.size();
        size_t data_counter = 0;

        /* for every chunk */
        for (size_t j = 0; j < chunks_size; ++j) {
            int max_bytes = 8;
            /* get rid of last chunks 0 at the front */
            if (j == chunks_size - 1) {
                for (int b = 7; b >= 0; --b) {
                    if (((chunks[j] >> (8 * b)) & 0xFF) != 0) {
                        max_bytes = b + 1;
                        break;
                    }
                }
            }
            /* for every byte in chunk */
            for (i = 0; i < max_bytes; ++i) {
                /* get byte */
                const uint8_t byte = static_cast<uint8_t>((chunks[j] >> (8 * i)) & 0xFF);

                /* if unlimited, just push_back, or place on position */
                if (unlimited) {
                    data.push_back(byte);
                }
                else {
                    if (data_counter >= data_len) {
                        throw std::overflow_error("MPInt overflow: number exceeds allowed byte size");
                    }
                    data[data_counter++] = byte;
                }
            }
        }

        /* return new MPInt */
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
        if (negative == other.negative) {
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
    MPInt& operator *=(const MPInt<OTHER_PRECISION>& other) {
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
        if (PRECISION == Unlimited)
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
        this->negative = this->negative != other.negative;
        return *this;
    }


    /* TODO: pro neomezenou presnost */
    std::string toString() const {
        // pokud je celé číslo nulové
        bool allZero = std::all_of(data.begin(), data.end(), [](uint8_t b){ return b == 0; });
        if (allZero)
            return "0";

        std::vector<uint8_t> temp = data;  // kopie, budeme v ní dělit
        std::string digits;

        while (true) {
            uint16_t remainder = 0; // zbytek pro aktuální dělení
            bool nonZero = false;   // sleduje, zda výsledek po dělení není nula

            // jdeme odzadu (big-end pořadí)
            for (int i = (int)temp.size() - 1; i >= 0; --i) {
                uint16_t value = (remainder << 8) + temp[i];
                temp[i] = static_cast<uint8_t>(value / 10);
                remainder = value % 10;
                if (temp[i] != 0)
                    nonZero = true;
            }

            digits.push_back('0' + remainder);
            if (!nonZero)
                break; // celé číslo se vynulovalo → hotovo
        }

        // cifry byly získané od nejnižší po nejvyšší → otočíme
        std::reverse(digits.begin(), digits.end());

        // znaménko
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

    MPInt& addAbs(const MPInt& other) {
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
            if (PRECISION != Unlimited) {
                if (i >= PRECISION && sum != 0) throw std::overflow_error("MPInt overflow in operator +=");
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
            if (PRECISION != Unlimited)
                throw std::overflow_error("MPInt overflow in operator+=");
            data.push_back(static_cast<uint8_t>(carry));
        }

        return *this;
    }

    /* function assumes that absolute value of this MPInt is greater than other */
    MPInt& subAbs(const MPInt& other) {
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

        return *this;
    }

    bool absGreaterOrEqual(const MPInt& other) const {
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
};

template <size_t PREC_A, size_t PREC_B>
auto operator+(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result(a);
        result += b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result(a);
        result += b;
        return result;
    }
}

template <size_t PREC_A, size_t PREC_B>
auto operator-(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result(a);
        result -= b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result(a);
        result -= b;
        return result;
    }
}

template <size_t PREC_A, size_t PREC_B>
auto operator*(const MPInt<PREC_A>& a, const MPInt<PREC_B>& b) {
    constexpr bool anyUnlimited = (PREC_A == MPInt<PREC_A>::Unlimited || PREC_B == MPInt<PREC_B>::Unlimited);
    if constexpr (anyUnlimited) {
        MPInt<MPInt<PREC_A>::Unlimited> result(a);
        result *= b;
        return result;
    }
    else {
        constexpr size_t MaxBytes = (PREC_A > PREC_B ? PREC_A : PREC_B);
        MPInt<MaxBytes> result(a);
        result *= b;
        return result;
    }
}


#endif