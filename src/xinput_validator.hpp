/************************************************************************************
 * Copyright (c) 2026, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XEUS_CPP_INPUT_VALIDATOR_HPP
#define XEUS_CPP_INPUT_VALIDATOR_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>

namespace xcpp
{
    enum class token_kind : std::uint8_t
    {
        l_square,    // "["
        r_square,    // "]"
        l_paren,     // "("
        r_paren,     // ")"
        l_brace,     // "{"
        r_brace,     // "}"
        stringlit,   // ""...""
        charlit,     // "'.'"
        comma,       // ","
        dot,         // "."
        excl_mark,   // "!"
        quest_mark,  // "?"
        slash,       // "/"
        backslash,   // "\"
        less,        // "<"
        greater,     // ">"
        ampersand,   // "&"
        hash,        // "#"
        ident,       // (a-zA-Z)[(0-9a-zA-Z)*]
        raw_ident,   // .*^(' '|'\t')
        comment,     // //
        l_comment,   // "/*"
        r_comment,   // "*/"
        space,       // (' ' | '\t')*
        constant,    // {0-9}
        at,          // @
        asterik,     // *
        semicolon,   // ;
        eof,
        unknown
    };

    int as_int(token_kind k);

    class token
    {
    public:

        explicit token(token_kind kind = token_kind::unknown, const char* buffer = nullptr, std::size_t length = 0U);

        [[nodiscard]] token_kind kind() const;
        [[nodiscard]] const char* buffer() const;
        [[nodiscard]] std::size_t length() const;

        [[nodiscard]] std::string_view identifier() const;
        [[nodiscard]] bool closes_brace(token_kind k) const;

    private:

        token_kind m_kind;
        const char* p_buffer;
        std::size_t m_length;
    };

    class xlexer
    {
    public:

        explicit xlexer(const std::string& code, bool skip_whitespace);

        token lex();
        token lex_any_string();
        token read_to_end_of_line();
        std::size_t skip_whitespace();

    private:

        token lex_quoted_string();
        token lex_punctuator();
        token lex_left_comment();
        token lex_right_comment();
        token lex_constant();
        token lex_identifier();
        token lex_whitespace();

        const char* p_start;
        const char* p_curr_pos;
    };

    enum class validation_result : std::uint8_t
    {
        incomplete,
        complete,
        mismatch
    };

    std::string to_string(validation_result res);

    class xinput_validator
    {
    public:

        validation_result validate(const std::string& input, std::size_t nb_char_indent = 4U);
        [[nodiscard]] std::string get_expected_indent() const;

    private:

        [[nodiscard]] bool in_block_comment() const;
        [[nodiscard]] bool in_template() const;

        std::deque<int> m_parent_stack;
        std::string m_indent;
    };
}  // namespace xcpp

#endif
