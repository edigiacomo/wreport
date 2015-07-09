#ifndef WREPORT_BUFFERS_BUFR_H
#define WREPORT_BUFFERS_BUFR_H

#include <wreport/error.h>
#include <wreport/varinfo.h>
#include <cstdint>
#include <string>

namespace wreport {
struct Var;

namespace bulletin {
struct AssociatedField;
}

namespace buffers {

/**
 * Destination for decoded variables from compressed BUFRs
 */
struct CompressedVarSink
{
    virtual ~CompressedVarSink() {}

    /**
     * Send a variable to a subset
     *
     * @param var
     *   Variable to send
     * @param idx
     *   Index of the subset that will get \a var
     */
    virtual void operator()(const Var& var, unsigned idx) = 0;
};

/**
 * Binary buffer with bit-level read operations
 */
class BufrInput
{
protected:
    /**
     * Scan length of section \a sec_no, filling in the start of the next
     * section in sec[sec_no + 1]
     */
    void scan_section_length(unsigned sec_no);

public:
    /// Input buffer
    const unsigned char* data;

    /// Input buffer size
    size_t data_len;

    /**
     * Input file name (optional).
     *
     * If available, it will be used to generate better error messages.
     *
     * If not available, it is NULL.
     */
    const char* fname;

    /**
     * File offset of the start of the message.
     *
     * If available, it will be used to generate better error messages.
     *
     * If not available, it is 0.
     */
    size_t start_offset;

    /// Offset of the byte we are currently decoding
    unsigned s4_cursor;

    /// Byte we are currently decoding
    unsigned char pbyte;

    /// Bits left in pbyte to decode
    int pbyte_len;

    /// Offsets of the start of BUFR sections
    unsigned sec[6];


    /**
     * Wrap a string iinto a BufrInput
     *
     * @param in
     *   String with the data to read
     */
    BufrInput(const std::string& in);

    /// Start decoding a different buffer
    void reset(const std::string& in);

    /**
     * Scan the message filling in the sec[] array of start offsets of sections
     * 0 and 1.
     *
     * We cannot scan past section 1 until we decode it, because we need to
     * know if section 2 is present or not
     */
    void scan_lead_sections();

    /**
     * Scan the message filling in the sec[] array of section start offsets of
     * all sections from 2 on.
     *
     * It also initialises points s4_cursor to the begin of the data in section
     * 4.
     *
     * @param has_optional
     *   True if the optional section is present, false if it should be
     *   skipped.
     */
    void scan_other_sections(bool has_optional);

    /// Return the current decoding byte offset
    int offset() const;

    /// Return the number of bits left in the message to be decoded
    int bits_left() const;

    /**
     * Read a byte value at offset \a pos
     */
    inline unsigned read_byte(unsigned pos) const
    {
        return (unsigned)data[pos];
    }

    /**
     * Read a byte value at offset \a pos inside section \a section
     */
    inline unsigned read_byte(unsigned section, unsigned pos) const
    {
        return (unsigned)data[sec[section] + pos];
    }

    /**
     * Read a big endian integer value \a byte_len bytes long, at offset \a pos
     */
    unsigned read_number(unsigned pos, unsigned byte_len) const;

    /**
     * Read a big endian integer value \a byte_len bytes long, at offset \a pos
     * inside section \a section
     */
    inline unsigned read_number(unsigned section, unsigned pos, unsigned byte_len) const
    {
        return read_number(sec[section] + pos, byte_len);
    }

    /**
     * Get the integer value of the next 'n' bits from the decode input
     * n must be <= 32.
     */
    uint32_t get_bits(unsigned n);

    /// Dump to stderr 'count' bits of 'buf', starting at the 'ofs-th' bit
    void debug_dump_next_bits(const char* desc, int count) const;

    /// Throw an error_parse at the current decoding location
    void parse_error(const char* fmt, ...) const WREPORT_THROWF_ATTRS(2, 3);

    /// Throw an error_parse at the given decoding location
    void parse_error(unsigned pos, const char* fmt, ...) const WREPORT_THROWF_ATTRS(3, 4);

    /// Throw an error_parse at the given decoding location inside the given section
    void parse_error(unsigned section, unsigned pos, const char* fmt, ...) const WREPORT_THROWF_ATTRS(4, 5);

    /**
     * Check that the input buffer contains at least \a datalen characters
     * after offset \a pos; throw error_parse otherwise.
     *
     * @param pos
     *   Starting offset of the required data
     * @param datalen
     *   Required amount of data expected starting from \a pos
     * @param expected
     *    name of what we are about to decode, used for generating nice error
     *    messages
     */
    void check_available_data(unsigned pos, size_t datalen, const char* expected);

    /**
     * Check that the input buffer contains at least \a datalen characters
     * after offset \a pos in section \a section; throw error_parse otherwise.
     *
     * @param section
     *   Number of the section to check
     * @param pos
     *   Starting offset inside the section of the required data
     * @param datalen
     *   Required amount of data expected starting from \a pos
     * @param expected
     *   Name of what we are about to decode, used for generating nice error
     *   messages
     */
    void check_available_data(unsigned section, unsigned pos, size_t datalen, const char* expected);

    /**
     * Decode a compressed number as described by dest.info(), ad set it as
     * value for \a dest.
     *
     * @param dest
     *   Variable which holds the decoding information and that will hold the
     *   decoded value
     * @param base
     *   The base value for the compressed number
     * @param diffbits
     *   The number of bits used to encode the difference from \a base
     */
    void decode_number(Var& dest, uint32_t base, unsigned diffbits);

    /**
     * Decode a number as described by dest.info(), and set it as value for \a
     * dest.
     *
     * @param dest
     *   Variable which holds the decoding information and that will hold the
     *   decoded value
     */
    void decode_number(Var& dest);

    /**
     * Decode a number as described by \a info from a compressed bufr with
     * \a subsets subsets, and send the resulting variables to \a dest
     */
    void decode_number(Varinfo info, unsigned subsets, const bulletin::AssociatedField& associated_field, CompressedVarSink& dest);

    /**
     * Decode a number as described by dest.info(), and set it as value for \a
     * dest. The number is decoded for \a subsets compressed datasets, and an
     * exception is thrown if the values differ.
     *
     * @param dest
     *   Variable which holds the decoding information and that will hold the
     *   decoded value
     * @param subsets
     *   Number of subsets in the compressed data section
     */
    void decode_number(Var& dest, unsigned subsets);

    /**
     * Read a string from the data section
     *
     * @param bit_len
     *   Number of bits (not bytes) to read. It is normally a multiple of 8,
     *   and when it is not, the last character will contain the partial byte
     *   read.
     * @retval str
     *   Buffer where the string is written. Must be big enough to contain the
     *   longest string described by info, plus 2 bytes
     * @retval len
     *   The string length
     * @return
     *   true if we decoded a real string, false if we decoded a missing string
     *   value
     */
    bool decode_string(unsigned bit_len, char* str, size_t& len);

    /**
     * Decode a string as described by dest.info(), ad set it as value for \a
     * dest.
     *
     * It is assumed that \a dest is not set, therefore in case we decode a
     * missing value, \a dest will not be touched.
     *
     * @param dest
     *   Variable which holds the decoding information and that will hold the
     *   decoded value
     */
    void decode_string(Var& dest);

    /**
     * Decode a string as described by dest.info(), and set it as value for \a
     * dest. The string is decoded for \a subsets compressed datasets, and an
     * exception is thrown if the values differ.
     *
     * @param dest
     *   Variable which holds the decoding information and that will hold the
     *   decoded value
     * @param subsets
     *   Number of subsets in the compressed data section
     */
    void decode_string(Var& dest, unsigned subsets);

    /**
     * Decode a string as described by \a info from a compressed bufr with \a
     * subsets subsets, and send the resulting variables to \a dest
     */
    void decode_string(Varinfo info, unsigned subsets, CompressedVarSink& dest);

    /**
     * Decode a generic binary value as-is, as described by dest.info(), ad set
     * it as value for \a dest.
     *
     * It is assumed that \a dest is not set, therefore in case we decode a
     * missing value, \a dest will not be touched.
     *
     * @param dest
     *   Variable which holds the decoding information and that will hold the
     *   decoded value
     */
    void decode_binary(Var& dest);
};

/**
 * Binary buffer with bit-level append operations
 */
struct BufrOutput
{
    /// Output buffer to which we append encoded data
    std::string& out;

    /// Byte to which we are appending bits to encode
    uint8_t pbyte;

    /// Number of bits already encoded in pbyte
    int pbyte_len;

    /**
     * Wrap a string into a BufrOutput
     *
     * @param out
     *   String to append data to
     */
    BufrOutput(std::string& out);

    /**
     * Append n bits from 'val'.  n must be <= 32.
     */
    void add_bits(uint32_t val, int n);

    /**
     * Append a string \a len bits long to the output buffer as it is,
     * ignoring partially encoded bits
     */
    void raw_append(const char* str, int len)
    {
        out.append(str, len);
    }

    /// Append a 16 bits integer
    void append_short(unsigned short val)
    {
        add_bits(val, 16);
    }

    /// Append an 8 bits integer
    void append_byte(unsigned char val)
    {
        add_bits(val, 8);
    }

    /// Append a missing value \a len_bits long
    void append_missing(unsigned len_bits)
    {
        add_bits(0xffffffff, len_bits);
    }

    /// Append a string variable
    void append_string(const Var& var, unsigned len_bits);

    /// Append a string \a len_bits bits long
    void append_string(const char* val, unsigned len_bits);

    /// Append a binary value \a len_bits bits long
    void append_binary(const unsigned char* val, unsigned len_bits);

    /// Append \a var encoded according to \a info
    void append_var(Varinfo info, const Var& var);

    /// Append a missing value according to \a info
    void append_missing(Varinfo info);

    /**
     * Write all bits left to the buffer, padding the last partial byte with
     * zeros if needed to make it even
     */
    void flush();
};


}
}

#endif
