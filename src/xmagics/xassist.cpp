/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/
#include "xassist.hpp"

#define CURL_STATICLIB
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/stat.h>
#include <unordered_set>

using json = nlohmann::json;

// TODO: Implement xplugin to separate the magics from the main code.
namespace xcpp
{
    class api_key_manager
    {
    public:

        static void save_api_key(const std::string& model, const std::string& api_key)
        {
            std::string api_key_file_path = model + "_api_key.txt";
            std::ofstream out(api_key_file_path);
            if (out)
            {
                out << api_key;
                out.close();
                std::cout << "API key saved for model " << model << std::endl;
            }
            else
            {
                std::cerr << "Failed to open file for writing API key for model " << model << std::endl;
            }
        }

        // Method to load the API key for a specific model
        static std::string load_api_key(const std::string& model)
        {
            std::string api_key_file_path = model + "_api_key.txt";
            std::ifstream in(api_key_file_path);
            std::string api_key;
            if (in)
            {
                std::getline(in, api_key);
                in.close();
                return api_key;
            }

            std::cerr << "Failed to open file for reading API key for model " << model << std::endl;
            return "";
        }
    };

    class model_manager
    {
    public:

        static void save_model(const std::string& model, const std::string& model_name)
        {
            std::string model_file_path = model + "_model.txt";
            std::ofstream out(model_file_path);
            if (out)
            {
                out << model_name;
                out.close();
                std::cout << "Model saved for model " << model << std::endl;
            }
            else
            {
                std::cerr << "Failed to open file for writing model for model " << model << std::endl;
            }
        }

        static std::string load_model(const std::string& model)
        {
            std::string model_file_path = model + "_model.txt";
            std::ifstream in(model_file_path);
            std::string model_name;
            if (in)
            {
                std::getline(in, model_name);
                in.close();
                return model_name;
            }

            std::cerr << "Failed to open file for reading model for model " << model << std::endl;
            return "";
        }
    };

    class url_manager
    {
    public:

        static void save_url(const std::string& model, const std::string& url)
        {
            std::string url_file_path = model + "_url.txt";
            std::ofstream out(url_file_path);
            if (out)
            {
                out << url;
                out.close();
                std::cout << "URL saved for model " << model << std::endl;
            }
            else
            {
                std::cerr << "Failed to open file for writing URL for model " << model << std::endl;
            }
        }

        static std::string load_url(const std::string& model)
        {
            std::string url_file_path = model + "_url.txt";
            std::ifstream in(url_file_path);
            std::string url;
            if (in)
            {
                std::getline(in, url);
                in.close();
                return url;
            }

            std::cerr << "Failed to open file for reading URL for model " << model << std::endl;
            return "";
        }
    };

    class chat_history
    {
    public:

        static std::string chat(const std::string& model, const std::string& user, const std::string& cell)
        {
            return append_and_read_back(model, user, "\"" + cell + "\"");
        }

        static std::string chat(const std::string& model, const std::string& user, const nlohmann::json& cell)
        {
            return append_and_read_back(model, user, cell.dump());
        }

        static void refresh(const std::string& model)
        {
            std::string chat_history_file_path = model + "_chat_history.txt";
            std::ofstream out(chat_history_file_path, std::ios::out);
        }

    private:

        static std::string
        append_and_read_back(const std::string& model, const std::string& user, const std::string& serialized_cell)
        {
            std::string chat_history_file_path = model + "_chat_history.txt";
            std::ofstream out;
            bool is_empty = is_file_empty(chat_history_file_path);

            out.open(chat_history_file_path, std::ios::app);
            if (!out)
            {
                std::cerr << "Failed to open file for writing chat history for model " << model << std::endl;
                return "";
            }

            if (!is_empty)
            {
                out << ", ";
            }

            if (model == "gemini")
            {
                out << R"({ "role": ")" << user << R"(", "parts": [ { "text": )" << serialized_cell << "}]}\n";
            }
            else
            {
                out << R"({ "role": ")" << user << R"(", "content": )" << serialized_cell << "}\n";
            }

            out.close();

            return read_file_content(chat_history_file_path);
        }

        static bool is_file_empty(const std::string& file_path)
        {
            std::ifstream file(file_path, std::ios::ate);  // Open the file at the end
            if (!file)                                     // If the file cannot be opened, it might not exist
            {
                return true;  // Consider non-existent files as empty
            }
            return file.tellg() == 0;
        }

        static std::string read_file_content(const std::string& file_path)
        {
            std::ifstream in(file_path);
            std::stringstream buffer_stream;
            buffer_stream << in.rdbuf();
            return buffer_stream.str();
        }
    };

    class curl_helper
    {
    private:

        CURL* m_curl;
        curl_slist* m_headers;

    public:

        curl_helper()
            : m_curl(curl_easy_init())
            , m_headers(curl_slist_append(nullptr, "Content-Type: application/json"))
        {
        }

        ~curl_helper()
        {
            if (m_curl)
            {
                curl_easy_cleanup(m_curl);
            }
            if (m_headers)
            {
                curl_slist_free_all(m_headers);
            }
        }

        // Delete copy constructor and copy assignment operator
        curl_helper(const curl_helper&) = delete;
        curl_helper& operator=(const curl_helper&) = delete;

        // Delete move constructor and move assignment operator
        curl_helper(curl_helper&&) = delete;
        curl_helper& operator=(curl_helper&&) = delete;

        std::string
        perform_request(const std::string& url, const std::string& post_data, const std::string& auth_header = "")
        {
            if (!auth_header.empty())
            {
                m_headers = curl_slist_append(m_headers, auth_header.c_str());
            }

            curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, post_data.c_str());

            std::string response;
            curl_easy_setopt(
                m_curl,
                CURLOPT_WRITEFUNCTION,
                +[](const char* in, size_t size, size_t num, std::string* out)
                {
                    const size_t total_bytes(size * num);
                    out->append(in, total_bytes);
                    return total_bytes;
                }
            );
            curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);

            CURLcode res = curl_easy_perform(m_curl);
            if (res != CURLE_OK)
            {
                std::cerr << "CURL request failed: " << curl_easy_strerror(res) << std::endl;
                return "";
            }

            return response;
        }
    };

    std::string escape_special_cases(const std::string& input)
    {
        std::string escaped;
        for (char c : input)
        {
            switch (c)
            {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '\"':
                    escaped += "\\\"";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\b':
                    escaped += "\\b";
                    break;
                case '\f':
                    escaped += "\\f";
                    break;
                default:
                    if (c < 0x20 || c > 0x7E)
                    {
                        // Escape non-printable ASCII characters and non-ASCII characters
                        std::array<char, 7> buffer{};
                        std::stringstream ss;
                        ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (c & 0xFFFF);
                        escaped += ss.str();
                    }
                    else
                    {
                        escaped += c;
                    }
                    break;
            }
        }
        return escaped;
    }

    std::string gemini(const std::string& cell, const std::string& key)
    {
        curl_helper curl_helper;
        const std::string chat_message = xcpp::chat_history::chat("gemini", "user", cell);
        const std::string model = xcpp::model_manager::load_model("gemini");

        if (model.empty())
        {
            std::cerr << "Model not found." << std::endl;
            return "";
        }

        const std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" + model
                                + ":generateContent?key=" + key;
        const std::string post_data = R"({"contents": [ )" + chat_message + R"(]})";

        std::string response = curl_helper.perform_request(url, post_data);

        json j = json::parse(response);
        if (j.find("error") != j.end())
        {
            std::cerr << "Error: " << j["error"]["message"] << std::endl;
            return "";
        }

        const std::string chat = xcpp::chat_history::chat(
            "gemini",
            "model",
            j["candidates"][0]["content"]["parts"][0]["text"]
        );

        return j["candidates"][0]["content"]["parts"][0]["text"];
    }

    std::string ollama(const std::string& cell)
    {
        curl_helper curl_helper;
        const std::string url = xcpp::url_manager::load_url("ollama");
        const std::string chat_message = xcpp::chat_history::chat("ollama", "user", cell);
        const std::string model = xcpp::model_manager::load_model("ollama");

        if (model.empty())
        {
            std::cerr << "Model not found." << std::endl;
            return "";
        }

        if (url.empty())
        {
            std::cerr << "URL not found." << std::endl;
            return "";
        }

        const std::string post_data = R"({
                    "model": ")" + model
                                      + R"(",
                    "messages": [)" + chat_message
                                      + R"(],
                    "stream": false
                })";

        std::string response = curl_helper.perform_request(url, post_data);

        json j = json::parse(response);

        if (j.find("error") != j.end())
        {
            std::cerr << "Error: " << j["error"]["message"] << std::endl;
            return "";
        }

        const std::string chat = xcpp::chat_history::chat("ollama", "assistant", j["message"]["content"]);

        return j["message"]["content"];
    }

    std::string openai(const std::string& cell, const std::string& key)
    {
        curl_helper curl_helper;
        const std::string url = "https://api.openai.com/v1/chat/completions";
        const std::string chat_message = xcpp::chat_history::chat("openai", "user", cell);
        const std::string model = xcpp::model_manager::load_model("openai");

        if (model.empty())
        {
            std::cerr << "Model not found." << std::endl;
            return "";
        }

        const std::string post_data = R"({
                    "model": ")" + model
                                      + R"(",
                    "messages": [)" + chat_message
                                      + R"(],
                    "temperature": 0.7
                })";
        std::string auth_header = "Authorization: Bearer " + key;

        std::string response = curl_helper.perform_request(url, post_data, auth_header);

        json j = json::parse(response);

        if (j.find("error") != j.end())
        {
            std::cerr << "Error: " << j["error"]["message"] << std::endl;
            return "";
        }

        const std::string chat = xcpp::chat_history::chat(
            "openai",
            "assistant",
            j["choices"][0]["message"]["content"]
        );

        return j["choices"][0]["message"]["content"];
    }

    void xassist::operator()(const std::string& line, const std::string& cell)
    {
        try
        {
            std::istringstream iss(line);
            std::vector<std::string> tokens(
                std::istream_iterator<std::string>{iss},
                std::istream_iterator<std::string>()
            );

            std::vector<std::string> models = {"gemini", "openai", "ollama"};
            std::string model = tokens[1];

            if (std::find(models.begin(), models.end(), model) == models.end())
            {
                std::cerr << "Model not found." << std::endl;
                return;
            }

            if (tokens.size() > 2)
            {
                if (tokens[2] == "--save-key")
                {
                    xcpp::api_key_manager::save_api_key(model, cell);
                    return;
                }

                if (tokens[2] == "--refresh")
                {
                    xcpp::chat_history::refresh(model);
                    return;
                }

                if (tokens[2] == "--save-model")
                {
                    xcpp::model_manager::save_model(model, cell);
                    return;
                }

                if (tokens[2] == "--set-url" && model == "ollama")
                {
                    xcpp::url_manager::save_url(model, cell);
                    return;
                }
            }

            std::string key;
            if (model != "ollama")
            {
                key = xcpp::api_key_manager::load_api_key(model);
                if (key.empty())
                {
                    std::cerr << "API key for model " << model << " is not available." << std::endl;
                    return;
                }
            }


            const std::string prompt = escape_special_cases(cell);

            std::string response;
            if (model == "gemini")
            {
                response = gemini(prompt, key);
            }
            else if (model == "openai")
            {
                response = openai(prompt, key);
            }
            else if (model == "ollama")
            {
                response = ollama(prompt);
            }

            std::cout << response;
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "Caught an exception: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Caught an unknown exception" << std::endl;
        }
    }
}  // namespace xcpp