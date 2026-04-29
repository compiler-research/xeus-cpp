/************************************************************************************
 * Copyright (c) 2026, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include "xinput_validator.hpp"
#include <cctype>
#include <set>

namespace xcpp
{
    namespace
    {
        bool is_alpha(char c)
        {
            return std::isalpha(static_cast<unsigned char>(c));
        }

        bool is_digit(char c)
        {
            return std::isdigit(static_cast<unsigned char>(c));
        }

        bool is_punctuator(char c)
        {
            static const std::set<char> punctuators =
                {'[', ']', '(', ')', '{', '}', '\\', ',', '.', '!', '?', '<', '>', '&', '#', '@', ';'};

            return punctuators.find(c) != punctuators.cend();
        }

        bool is_quoted_string(char c)
        {
            return c == '"' || c == '\'';
        }

        bool is_whitespace(char c)
        {
            return c == ' ' || c == '\t';
        }
    }  // namespace

    int as_int(token_kind k)
    {
        return static_cast<int>(k);
    }

    token::token(token_kind kind, const char* buffer, std::size_t length)
        : m_kind(kind)
        , p_buffer(buffer)
        , m_length(length)
    {
    }

    token_kind token::kind() const
    {
        return m_kind;
    }

    const char* token::buffer() const
    {
        return p_buffer;
    }

    std::size_t token::length() const
    {
        return m_length;
    }

    std::string_view token::identifier() const
    {
        return {p_buffer, m_length};
    }

    bool token::closes_brace(token_kind k) const
    {
        bool is_closing_brace = m_kind == token_kind::r_square || m_kind == token_kind::r_paren
                                || m_kind == token_kind::r_brace;
        return is_closing_brace && (as_int(m_kind) == as_int(k) + 1);
    }

    xlexer::xlexer(const std::string& code, bool skip_whitespace)
        : p_start(code.data())
        , p_curr_pos(code.data())
    {
        if (skip_whitespace)
        {
            this->skip_whitespace();
        }
    }

    token xlexer::lex()
    {
        char c = *p_curr_pos;

        if (is_quoted_string(c))
        {
            return lex_quoted_string();
        }

        if (is_punctuator(c))
        {
            return lex_punctuator();
        }

        if (c == '/')
        {
            if (*(p_curr_pos + 1) == '/' || *(p_curr_pos + 1) == '*')
            {
                return lex_left_comment();
            }
            return lex_punctuator();
        }

        if (c == '*')
        {
            if (*(p_curr_pos + 1) == '/')
            {
                return lex_right_comment();
            }
            return lex_punctuator();
        }

        if (is_digit(c))
        {
            return lex_constant();
        }

        if (is_alpha(c) || c == '_')
        {
            return lex_identifier();
        }

        if (is_whitespace(c))
        {
            return lex_whitespace();
        }

        const char* pos = p_curr_pos++;
        if (c == '\0')
        {
            return token(token_kind::eof, pos, 1U);
        }

        return token(token_kind::unknown, pos, 0U);
    }

    token xlexer::lex_any_string()
    {
        const char* start = p_curr_pos;
        if (*p_curr_pos == '\0')
        {
            return token(token_kind::eof, start, 0U);
        }

        while (*p_curr_pos != ' ' && *p_curr_pos != '\t' && *p_curr_pos != '\0')
        {
            ++p_curr_pos;
        }
        return token(token_kind::raw_ident, start, std::size_t(p_curr_pos - start));
    }

    token xlexer::read_to_end_of_line()
    {
        const char* start = p_curr_pos;
        while (*p_curr_pos != '\r' && *p_curr_pos != '\n' && *p_curr_pos != '\0')
        {
            ++p_curr_pos;
        }
        return token(token_kind::unknown, start, std::size_t(p_curr_pos - start));
    }

    std::size_t xlexer::skip_whitespace()
    {
        char c = *p_curr_pos;
        std::size_t res = 0U;
        while (is_whitespace(c) && c != '\0')
        {
            ++res;
            c = *(++p_curr_pos);
        }
        return res;
    }

    token xlexer::lex_quoted_string()
    {
        auto kind = (*p_curr_pos == '"') ? token_kind::stringlit : token_kind::charlit;
        const char* start = p_curr_pos++;
        while (true)
        {
            if (*p_curr_pos == '\\')
            {
                p_curr_pos += 2;
                continue;
            }
            if (*p_curr_pos++ == *start)
            {
                return token(kind, start, std::size_t(p_curr_pos - start));
            }
        }
    }

    token xlexer::lex_punctuator()
    {
        const char* pos = p_curr_pos++;
        switch (*pos)
        {
            case '[':
                return token(token_kind::l_square, pos, 1U);
            case ']':
                return token(token_kind::r_square, pos, 1U);
            case '(':
                return token(token_kind::l_paren, pos, 1U);
            case ')':
                return token(token_kind::r_paren, pos, 1U);
            case '{':
                return token(token_kind::l_brace, pos, 1U);
            case '}':
                return token(token_kind::r_brace, pos, 1U);
            case ',':
                return token(token_kind::comma, pos, 1U);
            case '.':
                return token(token_kind::dot, pos, 1U);
            case '!':
                return token(token_kind::excl_mark, pos, 1U);
            case '?':
                return token(token_kind::quest_mark, pos, 1U);
            case '/':
                return token(token_kind::slash, pos, 1U);
            case '\\':
                return token(token_kind::backslash, pos, 1U);
            case '<':
                return token(token_kind::less, pos, 1U);
            case '>':
                return token(token_kind::greater, pos, 1U);
            case '@':
                return token(token_kind::at, pos, 1U);
            case '&':
                return token(token_kind::ampersand, pos, 1U);
            case '#':
                return token(token_kind::hash, pos, 1U);
            case '*':
                return token(token_kind::asterik, pos, 1U);
            case ';':
                return token(token_kind::semicolon, pos, 1U);
            default:
                return token(token_kind::unknown, pos, 1U);
        }
    }

    token xlexer::lex_left_comment()
    {
        const char* start = p_curr_pos;
        auto kind = (*(++p_curr_pos) == '/') ? token_kind::comment : token_kind::l_comment;
        ++p_curr_pos;
        return token(kind, start, 2U);
    }

    token xlexer::lex_right_comment()
    {
        const char* start = p_curr_pos;
        ++p_curr_pos;
        return token(token_kind::r_comment, start, 2U);
    }

    token xlexer::lex_constant()
    {
        const char* start = p_curr_pos;
        char c = *p_curr_pos++;
        while (is_digit(c))
        {
            c = *p_curr_pos++;
        }
        --p_curr_pos;
        return token(token_kind::constant, start, std::size_t(p_curr_pos - start));
    }

    token xlexer::lex_identifier()
    {
        const char* start = p_curr_pos;
        char c = *p_curr_pos++;
        while (is_alpha(c) || is_digit(c) || c == '_')
        {
            c = *p_curr_pos++;
        }
        --p_curr_pos;
        return token(token_kind::ident, start, std::size_t(p_curr_pos - start));
    }

    token xlexer::lex_whitespace()
    {
        const char* start = p_curr_pos;
        skip_whitespace();
        return token(token_kind::space, start, std::size_t(p_curr_pos - start));
    }

    std::string to_string(validation_result res)
    {
        if (res == validation_result::incomplete)
        {
            return "incomplete";
        }
        else if (res == validation_result::complete)
        {
            return "complete";
        }
        else
        {
            return "mismatch";
        }
    }

    validation_result xinput_validator::validate(const std::string& input, std::size_t nb_char_indent)
    {
        auto res = validation_result::complete;

        // Only check for 'template' if we're not already indented
        if (m_parent_stack.empty())
        {
            xlexer luthor(input, true);
            auto tok = luthor.lex();
            if (tok.kind() == token_kind::ident && tok.identifier() == "template")
            {
                m_parent_stack.push_back(as_int(token_kind::less));
            }
        }

        xlexer luthor(input, false);
        std::size_t nb_init_spaces = luthor.skip_whitespace();
        token tok, last_not_space_tok;

        do
        {
            if (tok.kind() != token_kind::space)
            {
                last_not_space_tok = tok;
            }
            tok = luthor.lex();

            if (in_block_comment() && tok.kind() != token_kind::r_comment)
            {
                continue;
            }

            switch (tok.kind())
            {
                case token_kind::comment:
                    tok = luthor.read_to_end_of_line();
                    break;
                case token_kind::r_comment:
                    if (in_block_comment())
                    {
                        m_parent_stack.pop_back();
                    }
                    else
                    {
                        res = validation_result::mismatch;
                    }
                    break;
                case token_kind::l_square:
                case token_kind::l_paren:
                case token_kind::l_brace:
                case token_kind::l_comment:
                    m_parent_stack.push_back(as_int(tok.kind()));
                    break;
                case token_kind::r_square:
                case token_kind::r_paren:
                case token_kind::r_brace:
                {
                    auto tos = m_parent_stack.empty() ? token_kind::unknown
                                                      : static_cast<token_kind>(m_parent_stack.back());
                    if (!tok.closes_brace(tos))
                    {
                        res = validation_result::mismatch;
                        break;
                    }
                    m_parent_stack.pop_back();
                    // '}' will also pop a template '<' if their is one
                    if (tok.kind() == token_kind::r_brace && in_template())
                    {
                        m_parent_stack.pop_back();
                    }
                    break;
                }
                case token_kind::semicolon:
                    // Template forward declatation, i.e. 'template' '<' ... '>' ... ';'
                    if (in_template())
                    {
                        m_parent_stack.pop_back();
                    }
                    break;
                case token_kind::hash:
                    luthor.skip_whitespace();
                    tok = luthor.lex_any_string();
                    if (tok.kind() != token_kind::eof)
                    {
                        auto pptk = tok.identifier();
                        if (pptk.substr(0U, 2U) == "if")
                        {
                            m_parent_stack.push_back(as_int(token_kind::hash));
                        }
                        else if (pptk.substr(0U, 5U) == "endif"
                                 && (pptk.size() == 5U || pptk[5U] == '/' || std::isspace(pptk[5U])))
                        {
                            if (m_parent_stack.empty() || m_parent_stack.back() != as_int(token_kind::hash))
                            {
                                res = validation_result::mismatch;
                            }
                            else
                            {
                                m_parent_stack.pop_back();
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
        } while (tok.kind() != token_kind::eof && res != validation_result::mismatch);

        const auto last_kind = last_not_space_tok.kind();
        const bool cont = (last_kind == token_kind::backslash || last_kind == token_kind::comma);

        if (cont || (!m_parent_stack.empty() && res != validation_result::mismatch))
        {
            res = validation_result::incomplete;
        }

        std::size_t indent_size = nb_init_spaces + m_parent_stack.size() * nb_char_indent;
        if (indent_size != 0U)
        {
            m_indent = std::string(indent_size, ' ');
        }
        return res;
    }

    std::string xinput_validator::get_expected_indent() const
    {
        return m_indent;
    }

    bool xinput_validator::in_block_comment() const
    {
        return !m_parent_stack.empty() && m_parent_stack.back() == as_int(token_kind::l_comment);
    }

    bool xinput_validator::in_template() const
    {
        return m_parent_stack.size() == 1U && m_parent_stack.back() == as_int(token_kind::less);
    }
}  // namespace xcpp
