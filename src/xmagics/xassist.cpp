/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
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

namespace xcpp
{
    class APIKeyManager
    {
    public:

        static void saveApiKey(const std::string& model, const std::string& apiKey)
        {
            std::string apiKeyFilePath = model + "_api_key.txt";
            std::ofstream out(apiKeyFilePath);
            if (out)
            {
                out << apiKey;
                out.close();
                std::cout << "API key saved for model " << model << std::endl;
            }
            else
            {
                std::cerr << "Failed to open file for writing API key for model " << model << std::endl;
            }
        }

        // Method to load the API key for a specific model
        static std::string loadApiKey(const std::string& model)
        {
            std::string apiKeyFilePath = model + "_api_key.txt";
            std::ifstream in(apiKeyFilePath);
            std::string apiKey;
            if (in)
            {
                std::getline(in, apiKey);
                in.close();
                return apiKey;
            }

            std::cerr << "Failed to open file for reading API key for model " << model << std::endl;
            return "";
        }
    };

    class ChatHistory
    {
    public:

        static std::string chat(const std::string& model, const std::string& user, const std::string& cell)
        {
            return appendAndReadBack(model, user, "\"" + cell + "\"");
        }

        static std::string chat(const std::string& model, const std::string& user, const nlohmann::json& cell)
        {
            return appendAndReadBack(model, user, cell.dump());
        }

        static void refresh(const std::string& model)
        {
            std::string chatHistoryFilePath = model + "_chat_history.txt";
            std::ofstream out(chatHistoryFilePath, std::ios::out);
        }

    private:

        static std::string
        appendAndReadBack(const std::string& model, const std::string& user, const std::string& serializedCell)
        {
            std::string chatHistoryFilePath = model + "_chat_history.txt";
            std::ofstream out;
            bool isEmpty = isFileEmpty(chatHistoryFilePath);

            out.open(chatHistoryFilePath, std::ios::app);
            if (!out)
            {
                std::cerr << "Failed to open file for writing chat history for model " << model << std::endl;
                return "";
            }

            if (!isEmpty)
            {
                out << ", ";
            }

            if (model == "gemini")
            {
                out << "{ \"role\": \"" << user << R"(", "parts": [ { "text": )" << serializedCell << "}]}\n";
            }
            else
            {
                out << "{ \"role\": \"" << user << R"(", "content": )" << serializedCell << "}\n";
            }

            out.close();

            return readFileContent(chatHistoryFilePath);
        }

        static bool isFileEmpty(const std::string& filePath)
        {
            std::ifstream file(filePath, std::ios::ate);  // Open the file at the end
            if (!file)                                    // If the file cannot be opened, it might not exist
            {
                return true;  // Consider non-existent files as empty
            }
            return file.tellg() == 0;
        }

        static std::string readFileContent(const std::string& filePath)
        {
            std::ifstream in(filePath);
            std::stringstream bufferStream;
            bufferStream << in.rdbuf();
            return bufferStream.str();
        }
    };

    class CurlHelper
    {
    private:

        CURL* m_curl;
        curl_slist* m_headers;

    public:

        CurlHelper()
            : m_curl(curl_easy_init())
            , m_headers(curl_slist_append(nullptr, "Content-Type: application/json"))
        {
        }

        ~CurlHelper()
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
        CurlHelper(const CurlHelper&) = delete;
        CurlHelper& operator=(const CurlHelper&) = delete;

        // Delete move constructor and move assignment operator
        CurlHelper(CurlHelper&&) = delete;
        CurlHelper& operator=(CurlHelper&&) = delete;

        std::string
        performRequest(const std::string& url, const std::string& postData, const std::string& authHeader = "")
        {
            if (!authHeader.empty())
            {
                m_headers = curl_slist_append(m_headers, authHeader.c_str());
            }

            curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, postData.c_str());

            std::string response;
            curl_easy_setopt(
                m_curl,
                CURLOPT_WRITEFUNCTION,
                +[](const char* in, size_t size, size_t num, std::string* out)
                {
                    const size_t totalBytes(size * num);
                    out->append(in, totalBytes);
                    return totalBytes;
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
        CurlHelper curlHelper;
        const std::string chatMessage = xcpp::ChatHistory::chat("gemini", "user", cell);
        const std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key="
                                + key;
        const std::string postData = R"({"contents": [ )" + chatMessage + R"(]})";

        std::string response = curlHelper.performRequest(url, postData);

        json j = json::parse(response);
        if (j.find("error") != j.end())
        {
            std::cerr << "Error: " << j["error"]["message"] << std::endl;
            return "";
        }

        const std::string chat = xcpp::ChatHistory::chat(
            "gemini",
            "model",
            j["candidates"][0]["content"]["parts"][0]["text"]
        );

        return j["candidates"][0]["content"]["parts"][0]["text"];
    }

    std::string openai(const std::string& cell, const std::string& key)
    {
        CurlHelper curlHelper;
        const std::string url = "https://api.openai.com/v1/chat/completions";
        const std::string chatMessage = xcpp::ChatHistory::chat("openai", "user", cell);
        const std::string postData = R"({
                    "model": "gpt-3.5-turbo-16k",
                    "messages": [)" + chatMessage
                                     + R"(],
                    "temperature": 0.7
                })";
        std::string authHeader = "Authorization: Bearer " + key;

        std::string response = curlHelper.performRequest(url, postData, authHeader);

        json j = json::parse(response);

        if (j.find("error") != j.end())
        {
            std::cerr << "Error: " << j["error"]["message"] << std::endl;
            return "";
        }

        const std::string chat = xcpp::ChatHistory::chat(
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

            if (tokens[2] == "--save-key")
            {
                xcpp::APIKeyManager::saveApiKey(model, cell);
                return;
            }

            if (tokens[2] == "--refresh")
            {
                xcpp::ChatHistory::refresh(model);
                return;
            }

            std::string key = xcpp::APIKeyManager::loadApiKey(model);
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