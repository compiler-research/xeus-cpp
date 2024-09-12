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
// TODO: Add support for open-source models.
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

    std::string gemini(const std::string& cell, const std::string& key)
    {
        curl_helper curl_helper;
        const std::string chat_message = xcpp::chat_history::chat("gemini", "user", cell);
        const std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key="
                                + key;
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

    std::string openai(const std::string& cell, const std::string& key)
    {
        curl_helper curl_helper;
        const std::string url = "https://api.openai.com/v1/chat/completions";
        const std::string chat_message = xcpp::chat_history::chat("openai", "user", cell);
        const std::string post_data = R"({
                    "model": "gpt-3.5-turbo-16k",
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

            std::vector<std::string> models = {"gemini", "openai"};
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
            }

            std::string key = xcpp::api_key_manager::load_api_key(model);
            if (key.empty())
            {
                std::cerr << "API key for model " << model << " is not available." << std::endl;
                return;
            }

            std::string response;
            if (model == "gemini")
            {
                response = gemini(cell, key);
            }
            else if (model == "openai")
            {
                response = openai(cell, key);
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